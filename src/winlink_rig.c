/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  The radio as a hamlib rigctld. See winlink_rig.h.
 */

#include "winlink_rig.h"

#include "radio.h"
#include "voice.h"
#include "cfg/cfg_api.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 4
#define LINE_MAX_LEN 128

typedef struct {
    int    fd;
    size_t len;
    char   line[LINE_MAX_LEN];
} client_t;

static pthread_t      thread;
static bool           running = false;   /* UI thread only */
static atomic_bool    quit;
static int            listen_fd = -1;
static client_t       clients[MAX_CLIENTS];

/* The rig thread changes these; the UI thread reads them. */
static atomic_bool    keyed;
static atomic_uint    timeouts;
static int            key_owner = -1;    /* fd of the program that keyed */
static int64_t        keyed_since;       /* ms */
static int64_t        unkeyed_at = -1;   /* ms, -1: not yet */

static int64_t now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Keying: speech stopped first, the audio path before the carrier, as
 * bt_audio.c does; unkeying the other way round. */
static bool set_ptt(bool on, int fd) {
    if (on == atomic_load(&keyed)) {
        /* Keyed by another program: not this one's to share. */
        return !on || fd == key_owner;
    }
    if (on) {
        /* Not over someone else: the operator's own PTT, a tune, a scan.
         * Right after our own unkey the radio may still say TX for a
         * moment, and the modem's next turn comes quickly: that's ours. */
        radio_state_t st   = radio_get_state();
        bool          ours = st == RADIO_TX && unkeyed_at >= 0 && now_ms() - unkeyed_at < 1500;

        if (st != RADIO_RX && !ours) {
            return false;
        }
        voice_hold(true);
        radio_set_modem(true);
        key_owner   = fd;
        keyed_since = now_ms();
        atomic_store(&keyed, true);
    } else {
        radio_set_modem(false);
        atomic_store(&keyed, false);
        key_owner  = -1;
        unkeyed_at = now_ms();
        voice_hold(false);
    }
    return true;
}

static void reply(int fd, const char *text) {
    size_t len = strlen(text);

    /* Short replies to a local program: a failed or partial write means it
     * has gone, and poll() will report the hang-up. */
    if (send(fd, text, len, MSG_NOSIGNAL) != (ssize_t)len) {
        return;
    }
}

/* "\set_freq 7101500" -> cmd "\set_freq", arg "7101500". Short and long
 * forms both, as rigctld takes them. */
static void command(client_t *c, char *line) {
    char *save = NULL;
    char *cmd  = strtok_r(line, " \t\r", &save);
    char *arg  = strtok_r(NULL, " \t\r", &save);
    char  out[32];

    if (!cmd) {
        return;
    }

    if (!strcmp(cmd, "T") || !strcmp(cmd, "\\set_ptt")) {
        char *end = NULL;
        long  v   = arg ? strtol(arg, &end, 10) : -1;

        if (!arg || *end || v < 0 || v > 3) {
            reply(c->fd, "RPRT -1\n");
        } else {
            reply(c->fd, set_ptt(v != 0, c->fd) ? "RPRT 0\n" : "RPRT -1\n");
        }
    } else if (!strcmp(cmd, "t") || !strcmp(cmd, "\\get_ptt")) {
        reply(c->fd, atomic_load(&keyed) ? "1\n" : "0\n");
    } else if (!strcmp(cmd, "f") || !strcmp(cmd, "\\get_freq")) {
        snprintf(out, sizeof(out), "%d\n", (int)cparam_i_get(cfg.cur.fg_freq()));
        reply(c->fd, out);
    } else if (!strcmp(cmd, "F") || !strcmp(cmd, "\\set_freq")) {
        char  *end = NULL;
        double f   = arg ? strtod(arg, &end) : -1; /* hamlib may send "7101500.000000" */

        /* Never while transmitting, and only where the radio can go. */
        if (!arg || *end || !(f >= 0 && f <= 2e9) /* NaN too */ || atomic_load(&keyed) ||
            !radio_check_freq((int32_t)f)) {
            reply(c->fd, "RPRT -1\n");
        } else {
            cparam_i_set(cfg.cur.fg_freq(), (int32_t)f);
            reply(c->fd, "RPRT 0\n");
        }
    } else if (!strcmp(cmd, "\\chk_vfo")) {
        reply(c->fd, "0\n"); /* not in VFO mode: no VFO argument expected */
    } else if (!strcmp(cmd, "\\dump_caps") || !strcmp(cmd, "dump_caps") || !strcmp(cmd, "1")) {
        reply(c->fd, "RPRT 0\n"); /* Pat's ping: any answer will do */
    } else if (!strcmp(cmd, "q") || !strcmp(cmd, "Q") || !strcmp(cmd, "\\quit")) {
        shutdown(c->fd, SHUT_RDWR);
    } else {
        reply(c->fd, "RPRT -4\n"); /* RIG_ENIMPL */
    }
}

static void drop(client_t *c) {
    if (c->fd == key_owner) {
        set_ptt(false, c->fd);
    }
    close(c->fd);
    c->fd  = -1;
    c->len = 0;
}

static void read_client(client_t *c) {
    char    buf[256];
    ssize_t n = recv(c->fd, buf, sizeof(buf), 0);

    if (n <= 0) {
        drop(c);
        return;
    }
    for (ssize_t i = 0; i < n && c->fd >= 0; i++) {
        if (buf[i] == '\n') {
            c->line[c->len] = '\0';
            c->len          = 0;
            command(c, c->line);
        } else if (c->len < LINE_MAX_LEN - 1) {
            c->line[c->len++] = buf[i];
        } else {
            drop(c); /* not a rigctld client */
        }
    }
}

static void accept_client(void) {
    int fd = accept(listen_fd, NULL, NULL);

    if (fd < 0) {
        return;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd < 0) {
            int one = 1;

            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
            clients[i].fd  = fd;
            clients[i].len = 0;
            return;
        }
    }
    close(fd); /* full */
}

static void *rig_thread(void *arg) {
    (void)arg;

    while (!atomic_load(&quit)) {
        struct pollfd pfd[MAX_CLIENTS + 1];
        int           idx[MAX_CLIENTS + 1];
        int           n = 0;

        pfd[n].fd     = listen_fd;
        pfd[n].events = POLLIN;
        idx[n++]      = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0) {
                pfd[n].fd     = clients[i].fd;
                pfd[n].events = POLLIN;
                idx[n++]      = i;
            }
        }

        /* Wakes twice a second for quit and the backstop. */
        int r = poll(pfd, n, 500);

        if (atomic_load(&keyed) && now_ms() - keyed_since >= WINLINK_RIG_MAX_TX_S * 1000) {
            set_ptt(false, key_owner);
            atomic_fetch_add(&timeouts, 1);
        }
        if (r <= 0) {
            continue;
        }
        for (int k = 0; k < n; k++) {
            if (!(pfd[k].revents & (POLLIN | POLLHUP | POLLERR))) {
                continue;
            }
            if (idx[k] < 0) {
                accept_client();
            } else {
                read_client(&clients[idx[k]]);
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0) {
            drop(&clients[i]);
        }
    }
    set_ptt(false, -1);
    return NULL;
}

bool winlink_rig_start(void) {
    if (running) {
        return true;
    }

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (fd < 0) {
        return false;
    }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family      = AF_INET;
    a.sin_port        = htons(WINLINK_RIG_PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* never the network */

    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(fd, MAX_CLIENTS) < 0) {
        int e = errno;
        close(fd);
        errno = e;
        return false;
    }

    listen_fd = fd;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd  = -1;
        clients[i].len = 0;
    }
    atomic_store(&quit, false);
    atomic_store(&timeouts, 0);
    unkeyed_at = -1;

    if (pthread_create(&thread, NULL, rig_thread, NULL) != 0) {
        close(listen_fd);
        listen_fd = -1;
        return false;
    }
    running = true;
    return true;
}

void winlink_rig_stop(void) {
    if (!running) {
        return;
    }
    atomic_store(&quit, true);
    pthread_join(thread, NULL); /* unkeys on its way out */
    close(listen_fd);
    listen_fd = -1;
    running   = false;
}

bool winlink_rig_keyed(void) {
    return atomic_load(&keyed);
}

uint32_t winlink_rig_timeouts(void) {
    return atomic_load(&timeouts);
}
