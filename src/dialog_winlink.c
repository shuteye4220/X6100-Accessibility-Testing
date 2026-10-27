/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Winlink window. See dialog_winlink.h.
 *
 *  Start runs two programs, stopped again by Stop or closing the window:
 *  - xara (/usr/bin/xara), a VARA HF modem: it hears the receiver on the
 *    capture device the DSP reads, transmits into the codec output, and
 *    keys the radio through winlink_rig.c (rigctld on 127.0.0.1:4532).
 *    Its output is read here: connections, speed and errors are shown
 *    and spoken.
 *  - Pat (/usr/bin/pat), the Winlink client, with its web page on port
 *    8080. Its settings come from the environment (PAT_*), so its own
 *    file - with the Winlink password set on its web page - is never
 *    rewritten here. Its files are in /mnt/winlink on the card.
 *
 *  Network access, off whenever the window opens: off, both listen on the
 *  radio only (127.0.0.1); on, on every network - so a phone or computer
 *  on the Wi-Fi can open Pat's web page or use the modem, and so can
 *  anyone else on that network. The rig control is never on the network.
 *
 *  While open: USB-D, a filter for the bandwidth, power at most 5 W as
 *  for FT8, mode locked; what was there before comes back on closing.
 *  Pat may change the frequency (to call a gateway) and back.
 */

#include "dialog_winlink.h"

#include "buttons.h"
#include "dialog.h"
#include "events.h"
#include "keyboard.h"
#include "lock_manager.h"
#include "main_screen.h"
#include "msg.h"
#include "radio.h"
#include "styles.h"
#include "voice.h"
#include "voice_text.h"
#include "winlink_rig.h"
#include "cfg/cfg_api.h"
#include "lvgl/lvgl.h"

#include <aether_radio/x6100_control/control.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define XARA_BIN    "/usr/bin/xara"
#define PAT_BIN     "/usr/bin/pat"
#define DATA_DIR    "/mnt/winlink"
#define CAPTURE_DEV "alsa_input.platform-sound.stereo-fallback"
#define PLAYBACK_DEV "alsa_output.platform-sound.stereo-fallback"
#define VARA_PORT   8300
#define PAT_PORT    8080
#define MAX_PWR     5.0f

#define LOG_LINES   6
#define LINE_LEN    96

typedef enum {
    LINK_IDLE,
    LINK_CALLING,
    LINK_CONNECTED,
} link_state_t;

static const int bw_hz[] = {500, 2300, 2750};

/* Filter for each bandwidth, around VARA's 1500 Hz centre. */
static const int filter_low[]  = {1000, 200, 50};
static const int filter_high[] = {2000, 2800, 2950};

static void construct_cb(lv_obj_t *parent);
static void destruct_cb();
static void key_cb(lv_event_t *e);

static void start_stop_cb(button_data_t *btn);
static void bw_cb(button_data_t *btn);
static void network_cb(button_data_t *btn);
static void status_cb(button_data_t *btn);
static void drive_cb(button_data_t *btn);
static void registered_cb(button_data_t *btn);

static const char *start_label(void);
static const char *bw_label(void);
static const char *network_label(void);
static const char *drive_label(void);
static const char *registered_label(void);

static buttons_page_t page_1;
static buttons_page_t page_2;

static button_data_t btn_p1 = {.type = BTN_TEXT, .label = "(WL 1:2)", .press = button_next_page_cb, .next = &page_2, .voice = "Winlink page 1"};
static button_data_t btn_start = {.type = BTN_TEXT_FN, .label_fn = start_label, .press = start_stop_cb};
static button_data_t btn_bw = {.type = BTN_TEXT_FN, .label_fn = bw_label, .press = bw_cb};
static button_data_t btn_network = {.type = BTN_TEXT_FN, .label_fn = network_label, .press = network_cb};
static button_data_t btn_status = {.type = BTN_TEXT, .label = "Status", .press = status_cb};

static button_data_t btn_p2 = {.type = BTN_TEXT, .label = "(WL 2:2)", .press = button_next_page_cb, .next = &page_1, .voice = "Winlink page 2"};
static button_data_t btn_drive = {.type = BTN_TEXT_FN, .label_fn = drive_label, .press = drive_cb};
static button_data_t btn_registered = {.type = BTN_TEXT_FN, .label_fn = registered_label, .press = registered_cb};

static buttons_page_t page_1 = {
    {&btn_p1, &btn_start, &btn_bw, &btn_network, &btn_status}
};
static buttons_page_t page_2 = {
    {&btn_p2, &btn_drive, &btn_registered}
};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .btn_page     = &page_1,
    .key_cb       = key_cb,
};

dialog_t *dialog_winlink = &dialog;

static struct {
    bool         running;
    bool         network;          /* off whenever the window opens */
    pid_t        xara;
    pid_t        pat;
    int          xara_out;         /* xara's stdout and stderr, non-blocking */
    char         partial[256];     /* an unfinished line from it */
    size_t       partial_len;
    lv_timer_t  *timer;

    char         mycall[16];
    link_state_t link;
    char         remote[16];
    int          bps;
    bool         busy;
    uint32_t     timeouts_seen;

    char         log[LOG_LINES][LINE_LEN];
    int          log_n;
    int          review;           /* the knob's line, -1: none yet */
} S;

static lv_obj_t *status_label;
static lv_obj_t *log_label;

/***** Labels *****/

static const char *start_label(void) {
    return S.running ? "Stop" : "Start";
}

static const char *bw_label(void) {
    static char buf[24];

    snprintf(buf, sizeof(buf), "BW:\n%d Hz", bw_hz[param_i_get(cfg.winlink.bw())]);
    return buf;
}

static const char *network_label(void) {
    return S.network ? "Network:\nOn" : "Network:\nOff";
}

static const char *drive_label(void) {
    static char buf[24];

    snprintf(buf, sizeof(buf), "TX level:\n%d dB", (int)param_i_get(cfg.winlink.drive()));
    return buf;
}

static const char *registered_label(void) {
    return param_i_get(cfg.winlink.registered()) ? "Speeds:\nRegistered" : "Speeds:\nFree";
}

/***** Text *****/

/* printf onto the end of out (size n, *o used so far), never past it. */
static void append(char *out, size_t n, size_t *o, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
static void append(char *out, size_t n, size_t *o, const char *fmt, ...) {
    va_list ap;

    if (*o >= n - 1) {
        return;
    }
    va_start(ap, fmt);
    int r = vsnprintf(out + *o, n - *o, fmt, ap);
    va_end(ap);
    if (r > 0) {
        *o += (size_t)r;
        if (*o > n - 1) {
            *o = n - 1;
        }
    }
}

/* A callsign as it should be spoken: "VE3XYZ" -> "V E 3 X Y Z". */
static void say_call(const char *call, char *out, size_t n) {
    voice_text_decoded(call, out, n);
}

/* The radio's address on the Wi-Fi (its first IPv4 address other than
 * loopback), "" if none. */
static void local_ip(char *out, size_t n) {
    struct ifaddrs *ifs, *i;

    out[0] = '\0';
    if (getifaddrs(&ifs) != 0) {
        return;
    }
    for (i = ifs; i; i = i->ifa_next) {
        if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && !(i->ifa_flags & IFF_LOOPBACK) &&
            (i->ifa_flags & IFF_UP)) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, out, n);
            break;
        }
    }
    freeifaddrs(ifs);
}

/* "192.168.1.20" -> "192 dot 168 dot 1 dot 20": read as an address. */
static void spoken_ip(const char *ip, char *out, size_t n) {
    size_t o = 0;

    out[0] = '\0';
    for (const char *p = ip; *p; p++) {
        if (*p == '.') {
            append(out, n, &o, " dot ");
        } else {
            append(out, n, &o, "%c", *p);
        }
    }
}

static void status_text(char *out, size_t n, bool spoken) {
    char   ip[INET_ADDRSTRLEN], sip[64], call[64];
    size_t o = 0;

    out[0] = '\0';
    if (!S.running) {
        append(out, n, &o, "Winlink stopped. Bandwidth %d hertz. Network access %s.", bw_hz[param_i_get(cfg.winlink.bw())],
               S.network ? "on" : "off");
        return;
    }

    append(out, n, &o, "Winlink running. ");
    switch (S.link) {
        case LINK_CONNECTED:
            if (spoken) {
                say_call(S.remote, call, sizeof(call));
            } else {
                snprintf(call, sizeof(call), "%s", S.remote);
            }
            append(out, n, &o, "Connected to %s", call);
            if (S.bps > 0) {
                append(out, n, &o, ", %d bits per second", S.bps);
            }
            append(out, n, &o, ". ");
            break;
        case LINK_CALLING:
            append(out, n, &o, "Calling. ");
            break;
        default:
            append(out, n, &o, "Not connected%s. ", S.busy ? ", channel busy" : "");
            break;
    }
    append(out, n, &o, "Bandwidth %d hertz%s. ", bw_hz[param_i_get(cfg.winlink.bw())],
           param_i_get(cfg.winlink.registered()) ? ", registered speeds" : "");

    local_ip(ip, sizeof(ip));
    if (!S.network) {
        append(out, n, &o, "Network access off: Pat's web page is on the radio only.");
    } else if (!ip[0]) {
        append(out, n, &o, "Network access on, but the radio has no network address. Is Wi-Fi on?");
    } else if (spoken) {
        spoken_ip(ip, sip, sizeof(sip));
        append(out, n, &o, "Network access on. Pat's web page is at %s, port %d.", sip, PAT_PORT);
    } else {
        append(out, n, &o, "Network access on. Pat: http://%s:%d", ip, PAT_PORT);
    }
}

static void show(void) {
    char text[384];

    if (!status_label) {
        return;
    }
    status_text(text, sizeof(text), false);
    lv_label_set_text(status_label, text);

    char   all[LOG_LINES * (LINE_LEN + 1)];
    size_t o = 0;

    all[0] = '\0';
    for (int i = 0; i < S.log_n; i++) {
        append(all, sizeof(all), &o, "%s\n", S.log[i]);
    }
    lv_label_set_text(log_label, all);
}

/* Adds a line to the window's list of events (newest last). */
static void log_line(const char *text) {
    if (S.log_n == LOG_LINES) {
        memmove(S.log[0], S.log[1], sizeof(S.log[0]) * (LOG_LINES - 1));
        S.log_n--;
    }
    snprintf(S.log[S.log_n++], LINE_LEN, "%s", text);
    S.review = -1;
}

static void event(const char *shown, const char *spoken) {
    log_line(shown);
    show();
    if (spoken) {
        voice_say_text_fmt("%s", spoken);
    }
}

/***** The modem's output *****/

/* "    14.980 < CONNECTED TEST1 TEST2 2300": what the modem tells its
 * client. "xara: ...": its own messages. The rest (frames, commands) is
 * left alone, but for a call ("> Connect { mycall: "A", remote: "B" }"). */
static void modem_line(char *line) {
    char text[128], call[64];
    char *p = line;

    while (*p == ' ') {
        p++;
    }

    if (!strncmp(p, "xara:", 5)) {
        /* Its own notes: errors, mostly. Shown; spoken by the exit handler
         * if it then stops. */
        log_line(p);
        show();
        return;
    }

    /* Past the time stamp */
    while (*p && (isdigit((unsigned char)*p) || *p == '.')) {
        p++;
    }
    while (*p == ' ') {
        p++;
    }

    if (!strncmp(p, "> Connect", 9)) {
        char *r = strstr(p, "remote: \"");

        if (r) {
            r += 9;
            size_t len = strcspn(r, "\"");

            snprintf(S.remote, sizeof(S.remote), "%.*s", (int)(len < sizeof(S.remote) ? len : sizeof(S.remote) - 1), r);
            S.link = LINK_CALLING;
            say_call(S.remote, call, sizeof(call));
            snprintf(text, sizeof(text), "Calling %s", call);
            event(p + 2, text);
        }
        return;
    }
    if (strncmp(p, "< ", 2)) {
        return;
    }
    p += 2;

    if (!strncmp(p, "CONNECTED ", 10)) {
        char a[16] = "", b[16] = "";

        sscanf(p + 10, "%15s %15s", a, b);
        /* "A B": A called B. The other station is whichever isn't us. */
        bool incoming = strcasecmp(a, S.mycall) != 0;
        snprintf(S.remote, sizeof(S.remote), "%s", incoming ? a : b);
        S.link = LINK_CONNECTED;
        S.bps  = 0;
        say_call(S.remote, call, sizeof(call));
        snprintf(text, sizeof(text), incoming ? "%s connected" : "Connected to %s", call);
        event(p, text);
    } else if (!strncmp(p, "DISCONNECTED", 12)) {
        bool was = S.link == LINK_CONNECTED;

        S.link = LINK_IDLE;
        S.bps  = 0;
        event(p, was ? "Disconnected" : "No connection");
    } else if (!strncmp(p, "BITRATE", 7)) {
        /* "BITRATE (4)  175 bps TX": shown, and said by Status */
        int level, bps;

        if (sscanf(p, "BITRATE (%d) %d", &level, &bps) == 2) {
            S.bps = bps;
            show();
        }
    } else if (!strncmp(p, "BUSY ON", 7)) {
        S.busy = true;
    } else if (!strncmp(p, "BUSY OFF", 8)) {
        S.busy = false;
    }
}

static void read_modem(void) {
    char buf[512];

    for (;;) {
        ssize_t n = read(S.xara_out, buf, sizeof(buf));

        if (n <= 0) {
            return; /* nothing now (EAGAIN), or it closed: the exit is seen by waitpid */
        }
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                S.partial[S.partial_len] = '\0';
                modem_line(S.partial);
                S.partial_len = 0;
            } else if (S.partial_len < sizeof(S.partial) - 1) {
                S.partial[S.partial_len++] = buf[i];
            }
        }
    }
}

/***** Running the programs *****/

static pid_t spawn(const char *path, char *const argv[], char *const envp[], int out_fd) {
    posix_spawn_file_actions_t fa;
    pid_t                      pid;

    posix_spawn_file_actions_init(&fa);
    if (out_fd >= 0) {
        posix_spawn_file_actions_adddup2(&fa, out_fd, STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&fa, out_fd, STDERR_FILENO);
    } else {
        posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    }
    posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
    /* Nothing else of the GUI's open files (its database, sockets...) */
    posix_spawn_file_actions_addclosefrom_np(&fa, 3);
#endif

    int r = posix_spawn(&pid, path, &fa, NULL, argv, envp);

    posix_spawn_file_actions_destroy(&fa);
    return r == 0 ? pid : -1;
}

/* Stops both programs: asked first, together, then made to after 2 s. */
static void end_both(void) {
    pid_t *pids[] = {&S.pat, &S.xara};

    for (int i = 0; i < 2; i++) {
        if (*pids[i] > 0) {
            kill(*pids[i], SIGTERM);
        }
    }
    for (int t = 0; t < 40 && (S.pat > 0 || S.xara > 0); t++) {
        for (int i = 0; i < 2; i++) {
            if (*pids[i] > 0 && waitpid(*pids[i], NULL, WNOHANG) == *pids[i]) {
                *pids[i] = -1;
            }
        }
        if (S.pat > 0 || S.xara > 0) {
            usleep(50000);
        }
    }
    for (int i = 0; i < 2; i++) {
        if (*pids[i] > 0) {
            kill(*pids[i], SIGKILL);
            waitpid(*pids[i], NULL, 0);
            *pids[i] = -1;
        }
    }
}

/* A callsign the programs will take: letters, digits, '/' and '-'. */
static bool valid_call(const char *c) {
    size_t n = strlen(c);

    if (n < 3 || n > 12) {
        return false;
    }
    for (; *c; c++) {
        if (!isalnum((unsigned char)*c) && *c != '/' && *c != '-') {
            return false;
        }
    }
    return true;
}

static bool valid_grid(const char *g) {
    size_t n = strlen(g);

    if (n != 4 && n != 6 && n != 8) {
        return false;
    }
    for (; *g; g++) {
        if (!isalnum((unsigned char)*g)) {
            return false;
        }
    }
    return true;
}

static void stop_modem(void);

static void timer_cb(lv_timer_t *t) {
    (void)t;

    if (!S.running) {
        return;
    }
    read_modem();

    uint32_t to = winlink_rig_timeouts();

    if (to != S.timeouts_seen) {
        S.timeouts_seen = to;
        event("Transmit stopped: stuck for 45 s", "Transmit stopped, it was stuck");
    }

    /* Only our own two: the GUI has other children (pactl...), whose
     * exits are theirs to collect. */
    if (S.xara > 0 && waitpid(S.xara, NULL, WNOHANG) == S.xara) {
        S.xara = -1;
        read_modem(); /* its last words */
        stop_modem();
        event("Modem stopped", "Modem stopped. See the window for why.");
        return;
    }
    if (S.pat > 0 && waitpid(S.pat, NULL, WNOHANG) == S.pat) {
        S.pat = -1;
        stop_modem();
        event("Pat stopped", "Pat stopped. Winlink stopped.");
        return;
    }
}

static bool start_modem(void) {
    const char *call = PARAM_T_GET(cfg.callsign());
    const char *grid = PARAM_T_GET(cfg.qth());
    char        bw[8], drive[8], addr[16], vara_addr[32], http_addr[32];

    if (!valid_call(call)) {
        event("Set your call sign first (APP, Callsign)", "Set your call sign first, in the Callsign app.");
        return false;
    }
    if (access(XARA_BIN, X_OK) != 0 || access(PAT_BIN, X_OK) != 0) {
        event("xara or Pat is missing from this image", "Winlink programs are missing from this firmware.");
        return false;
    }
    if (!winlink_rig_start()) {
        event("Rig control port 4532 is in use", "Can't start: rig control port in use.");
        return false;
    }

    snprintf(S.mycall, sizeof(S.mycall), "%s", call);
    for (char *c = S.mycall; *c; c++) {
        *c = (char)toupper((unsigned char)*c);
    }

    int b = param_i_get(cfg.winlink.bw());

    /* Radio: digital USB, a filter for the bandwidth, FT8's power limit. */
    cparam_i_set(cfg.cur.mode(), x6100_mode_usb_dig);
    cparam_i_set(cfg.filter.low(), filter_low[b]);
    cparam_i_set(cfg.filter.high(), filter_high[b]);
    if (param_f_get(cfg.pwr()) > MAX_PWR) {
        radio_set_pwr(MAX_PWR);
        msg_update_text_fmt("Power was limited to %0.0fW", MAX_PWR);
    }

    mkdir(DATA_DIR, 0700);

    snprintf(bw, sizeof(bw), "%d", bw_hz[b]);
    snprintf(drive, sizeof(drive), "%d", (int)param_i_get(cfg.winlink.drive()));
    snprintf(addr, sizeof(addr), "%s", S.network ? "0.0.0.0" : "127.0.0.1");

    /* The modem */
    int fds[2];

    if (pipe2(fds, O_CLOEXEC) != 0) {
        winlink_rig_stop();
        return false;
    }

    char *xara_argv[] = {
        "xara",          "--call",  S.mycall,      "--hf",         bw,
        "--audio",       "pulse",   "--capture",   CAPTURE_DEV,    "--playback",
        PLAYBACK_DEV,    "--radio", "--ptt",       "rigctld:127.0.0.1:4532",
        "--drive-db",    drive,     "--addr",      addr,           "--port",
        "8300",          "--kiss-port", "off",
        param_i_get(cfg.winlink.registered()) ? "--registered" : NULL,
        NULL,
    };

    S.xara = spawn(XARA_BIN, xara_argv, environ, fds[1]);
    close(fds[1]);
    if (S.xara < 0) {
        close(fds[0]);
        winlink_rig_stop();
        event("The modem didn't start", "The modem didn't start.");
        return false;
    }
    S.xara_out = fds[0];
    fcntl(S.xara_out, F_SETFL, fcntl(S.xara_out, F_GETFL) | O_NONBLOCK);
    S.partial_len = 0;

    /* Pat: settings for this run in its environment */
    char env_call[48], env_grid[48], env_http[64], env_vara[64], env_bw[48];
    char *extra[] = {
        env_call, env_http, env_vara, env_bw,
        "PAT_VARAHF_RIG=x6100",
        "PAT_VARAHF_PTTCONTROL=false",
        "PAT_HAMLIB_RIGS_x6100_ADDRESS=127.0.0.1:4532",
        "PAT_HAMLIB_RIGS_x6100_NETWORK=tcp",
        valid_grid(grid) ? env_grid : NULL,
    };
    size_t n_extra = sizeof(extra) / sizeof(extra[0]);

    snprintf(vara_addr, sizeof(vara_addr), "127.0.0.1:%d", VARA_PORT);
    snprintf(http_addr, sizeof(http_addr), "%s:%d", addr, PAT_PORT);
    snprintf(env_call, sizeof(env_call), "PAT_MYCALL=%s", S.mycall);
    snprintf(env_grid, sizeof(env_grid), "PAT_LOCATOR=%s", grid);
    snprintf(env_http, sizeof(env_http), "PAT_HTTPADDR=%s", http_addr);
    snprintf(env_vara, sizeof(env_vara), "PAT_VARAHF_ADDR=%s", vara_addr);
    snprintf(env_bw, sizeof(env_bw), "PAT_VARAHF_BANDWIDTH=%d", bw_hz[b]);

    size_t n_env = 0;
    while (environ[n_env]) {
        n_env++;
    }

    char **envp = calloc(n_env + n_extra + 1, sizeof(char *));
    size_t k    = 0;

    if (!envp) {
        stop_modem();
        return false;
    }
    for (size_t i = 0; i < n_env; i++) {
        if (strncmp(environ[i], "PAT_", 4) != 0) {
            envp[k++] = environ[i];
        }
    }
    for (size_t i = 0; i < n_extra; i++) {
        if (extra[i]) {
            envp[k++] = extra[i];
        }
    }
    envp[k] = NULL;

    char *pat_argv[] = {
        "pat",
        "--config",    DATA_DIR "/config.json",
        "--mbox",      DATA_DIR "/mailbox",
        "--log",       DATA_DIR "/pat.log",
        "--event-log", DATA_DIR "/eventlog.json",
        "--forms",     DATA_DIR "/forms",
        "--prehooks",  DATA_DIR "/prehooks",
        "--listen",    "varahf",
        "http",
        NULL,
    };

    /* Listening for calls too ("--listen varahf"): peer-to-peer, and a
     * gateway calling back. The web address comes from PAT_HTTPADDR. */
    S.pat = spawn(PAT_BIN, pat_argv, envp, -1);
    free(envp);
    if (S.pat < 0) {
        stop_modem();
        event("Pat didn't start", "Pat didn't start.");
        return false;
    }

    S.running       = true;
    S.link          = LINK_IDLE;
    S.bps           = 0;
    S.busy          = false;
    S.timeouts_seen = 0;
    return true;
}

static void stop_modem(void) {
    end_both();
    if (S.xara_out >= 0) {
        close(S.xara_out);
        S.xara_out = -1;
    }
    winlink_rig_stop(); /* unkeys */
    S.running = false;
    S.link    = LINK_IDLE;
    buttons_refresh(&btn_start);
}

/* Settings that take effect at start: a running modem is restarted, but
 * not in the middle of a connection. */
static bool can_change(void) {
    if (S.running && S.link != LINK_IDLE) {
        voice_say_text_fmt("Not while connected");
        return false;
    }
    return true;
}

static void restart_if_running(void) {
    if (S.running) {
        stop_modem();
        start_modem();
        buttons_refresh(&btn_start);
    }
    show();
}

/***** Keys *****/

static void start_stop_cb(button_data_t *btn) {
    (void)btn;
    char text[384];

    if (S.running) {
        stop_modem();
        log_line("Stopped");
        show();
        voice_say_text_fmt("Winlink stopped");
        return;
    }
    if (start_modem()) {
        log_line("Started");
        buttons_refresh(&btn_start);
        show();
        status_text(text, sizeof(text), true);
        voice_say_text_fmt("%s", text);
    }
}

static void bw_cb(button_data_t *btn) {
    (void)btn;
    if (!can_change()) {
        return;
    }
    int b = (param_i_get(cfg.winlink.bw()) + 1) % 3;

    param_i_set(cfg.winlink.bw(), b);
    buttons_refresh(&btn_bw);
    restart_if_running();
    voice_say_text_fmt("Bandwidth %d hertz", bw_hz[b]);
}

static void network_cb(button_data_t *btn) {
    (void)btn;
    if (!can_change()) {
        return;
    }
    S.network = !S.network;
    buttons_refresh(&btn_network);
    restart_if_running();

    char ip[INET_ADDRSTRLEN], sip[64];

    local_ip(ip, sizeof(ip));
    if (!S.network) {
        voice_say_text_fmt("Network access off. Only the radio can reach Pat and the modem.");
    } else if (ip[0]) {
        spoken_ip(ip, sip, sizeof(sip));
        voice_say_text_fmt("Network access on. Anyone on this network can reach Pat and the modem. Pat's web page "
                           "is at %s, port %d.",
                           sip, PAT_PORT);
    } else {
        voice_say_text_fmt("Network access on, but the radio has no network address. Is Wi-Fi on?");
    }
}

static void drive_cb(button_data_t *btn) {
    (void)btn;
    if (!can_change()) {
        return;
    }
    /* -30 to 0 dB in 5 dB steps, then round again */
    int d = param_i_get(cfg.winlink.drive()) + 5;

    if (d > 0) {
        d = -30;
    }
    param_i_set(cfg.winlink.drive(), d);
    buttons_refresh(&btn_drive);
    restart_if_running();
    voice_say_text_fmt("Transmit level %d decibels", d);
}

static void registered_cb(button_data_t *btn) {
    (void)btn;
    if (!can_change()) {
        return;
    }
    bool on = !param_i_get(cfg.winlink.registered());

    param_i_set(cfg.winlink.registered(), on);
    buttons_refresh(&btn_registered);
    restart_if_running();
    voice_say_text_fmt(on ? "Registered speeds, for VARA licence holders" : "Free speeds");
}

static void status_cb(button_data_t *btn) {
    (void)btn;
    dialog_winlink_say_status();
}

void dialog_winlink_say_status(void) {
    char text[384];

    status_text(text, sizeof(text), true);
    voice_say_requested(text);
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct(&dialog);
            break;

        /* Multi-function knob: read the events, newest first. */
        case LV_KEY_RIGHT:
        case LV_KEY_DOWN:
        case LV_KEY_LEFT:
        case LV_KEY_UP: {
            if (S.log_n == 0) {
                voice_say_text_fmt("No events yet");
                break;
            }
            bool older = (key == LV_KEY_LEFT || key == LV_KEY_UP);

            if (S.review < 0) {
                S.review = S.log_n - 1;
            } else if (older) {
                S.review = S.review > 0 ? S.review - 1 : 0;
            } else {
                S.review = S.review < S.log_n - 1 ? S.review + 1 : S.log_n - 1;
            }

            char spoken[192];
            voice_text_decoded(S.log[S.review], spoken, sizeof(spoken));
            voice_say_text_fmt("%s", spoken);
            break;
        }

        case LV_KEY_ENTER:
            dialog_winlink_say_status();
            break;

        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;

        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
    }
}

/***** Window *****/

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    memset(S.log, 0, sizeof(S.log));
    S.log_n    = 0;
    S.review   = -1;
    S.network  = false;
    S.running  = false;
    S.xara     = -1;
    S.pat      = -1;
    S.xara_out = -1;

    lv_group_add_obj(keyboard_group, dialog.obj);
    lv_obj_add_event_cb(dialog.obj, key_cb, LV_EVENT_KEY, NULL);
    lv_group_focus_obj(dialog.obj);
    lv_group_set_editing(keyboard_group, true);

    status_label = lv_label_create(dialog.obj);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(status_label, 720);
    lv_obj_set_pos(status_label, 30, 24);

    log_label = lv_label_create(dialog.obj);
    lv_label_set_long_mode(log_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(log_label, 720);
    lv_obj_set_pos(log_label, 30, 170);
    lv_obj_set_style_text_color(log_label, lv_color_hex(0xAAAAAA), 0);

    /* What was set before comes back on closing, as for FT8. */
    mem_save(MEM_BACKUP_ID);
    lm_set_mode(true);

    S.timer = lv_timer_create(timer_cb, 200, NULL);

    show();
    voice_say_text_fmt("Winlink window. Stopped. Press Start.");
}

static void destruct_cb() {
    if (S.running) {
        stop_modem();
    }
    if (S.timer) {
        lv_timer_del(S.timer);
        S.timer = NULL;
    }
    status_label = NULL;
    log_label    = NULL;
    lv_group_set_editing(keyboard_group, false);

    mem_load(MEM_BACKUP_ID);
    lm_set_mode(false);
}
