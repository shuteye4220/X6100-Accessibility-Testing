/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "voice.h"

#include "util.h"
#include "display.h"
#include "voice_text.h"
#include "voice_synth.h"
#include "lock_manager.h"

/* C++ headers: before the extern "C" block. */
#include "cfg/cfg_api.h"
#include "cfg/db.h"

extern "C" {
#include <unistd.h>
#include <pthread.h>
#include <aether_radio/x6100_control/control.h>

#include "audio.h"
#include "bt_audio.h"
#include "recorder.h"
#include "msg.h"
#include "meter.h"
#include "tx_info.h"
#include "radio.h"
#include "cw_tune_ui.h"
}


#include <atomic>
#include <cctype>
#include <cmath>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>

// Set by voice_stop_current() to cooperatively abort an in-progress
// announcement - checked by the synthesizer (per audio chunk) and in
// say_thread()'s pre-speech delay wait. This replaces an earlier
// pthread_cancel()-based approach: cancelling mid-synthesize() risked
// skipping the audio_set_play_mode(AUDIO_PLAY_OFF) call in say_thread(),
// which would leave the radio's BASE audio hardware stuck routed to the
// app instead of back to normal mic/RX - a stop flag lets the current
// thread always finish its own cleanup on its own terms, just sooner.
static std::atomic<bool> stop_requested{false};

static char                         buf[512];
static char                         prev[512];
static pthread_t                    thread;
static uint32_t                     delay = 0;
static bool                         run = false;
static uint16_t                     repeated = 0;
static bool                         sure = false;
// Length of the "prompt" prefix in buf, set by the caller. 0 means the
// whole announcement is spoken every time; a non-zero value means the
// prefix is compared against the previous announcement and, if unchanged,
// only the text after it is spoken (e.g. a control's value changing while
// its name stays the same) - see voice_say_prompted_fmt().
static size_t                       prompt_len = 0;

static void * say_thread(void *arg) {
    // Poll rather than a single usleep(delay) so an interrupting
    // announcement (see voice_stop_current()) can cut a still-waiting
    // delayed announcement short instead of blocking the caller for up
    // to the full delay.
    for (uint32_t waited = 0; waited < delay; waited += 10000) {
        if (stop_requested) {
            return NULL;
        }
        usleep(10000);
    }

    run = true;

    char *ptr = buf;

    // voice_repeat_count == 0 means "always speak the full prompt", i.e.
    // never throttle - handled by simply not entering the throttle at all,
    // which leaves ptr at the full text below just like a fresh prompt does.
    if (prompt_len > 0 && cfg.voice.repeat_count()->get() > 0) {
        if (strncmp(buf, prev, prompt_len) == 0) {
            repeated++;

            if (repeated >= cfg.voice.repeat_count()->get()) {
                repeated = 0;
            } else {
                ptr = buf + prompt_len;
            }
        } else {
            repeated = 0;
        }
    } else {
        repeated = 0;
    }
    strcpy(prev, buf);

    /* Bluetooth listening: speak into the headphones, the radio's
     * speaker is silent. */
    char bt_sink[96];
    bool bt = bt_audio_playback_begin(bt_sink, sizeof(bt_sink));

    if (!bt) {
        audio_set_play_mode(AUDIO_PLAY_ON);
    }
    /* The engine chosen at build time (voice_synth.h). */
    voice_synth_speak(ptr, bt ? bt_sink : nullptr, stop_requested);
    if (!bt) {
        audio_set_play_mode(AUDIO_PLAY_OFF);
    }
    if (bt) {
        bt_audio_playback_end();
    }

    run = false;
    sure = false;
    return NULL;
}

void voice_sure() {
    sure = true;
}

// Blocks until the current announcement (if any) has actually finished
// playing through the speaker. Needed anywhere a caller is about to turn on
// real audio capture right after speaking - without this, the capture starts
// while the announcement is still mid-playback and the mic (physically close
// to the speaker on this radio) picks up the announcement itself.
//
// `run` going false only means the app has finished handing samples to
// PulseAudio, not that they've physically finished playing - the audio
// sink's own buffering adds real latency after that point (measured at
// ~380ms steady-state on this hardware during live testing). The extra
// sleep below absorbs that gap so callers don't have to know about it.
bool voice_is_busy() {
    return run;
}

void voice_wait_done() {
    while (run) {
        usleep(10000);
    }
    usleep(400000);
}

void voice_change_mode() {
    voice_sure();

    switch (cfg.voice.mode()->get()) {
        case VOICE_OFF:
            cfg.voice.mode()->set(VOICE_LCD);
            msg_update_text_fmt("Voice mode: LCD");
            voice_say_text("Voice mode", "is LCD");
            break;

        case VOICE_LCD:
            cfg.voice.mode()->set(VOICE_ALWAYS);
            msg_update_text_fmt("Voice mode: Always");
            voice_say_text("Voice mode", "is always");
            break;

        case VOICE_ALWAYS:
            cfg.voice.mode()->set(VOICE_OFF);
            msg_update_text_fmt("Voice mode: Off");
            voice_say_text("Voice mode", "is off");
            break;
    }
}

/* lv_tick_get() of the last announcement asked for, spoken or not. Lets a
 * pop-up shown by the same action stay quiet (voice_say_popup()). */
static std::atomic<uint32_t> last_request_tick{0};
static std::atomic<bool>     have_request{false};

bool voice_enable() {
    last_request_tick = lv_tick_get();
    have_request      = true;

    if (recorder_is_on()) {
        return false;
    }

    bool allowed;

    if (sure) {
        allowed = true;
    } else {
        switch (cfg.voice.mode()->get()) {
            case VOICE_OFF:
                return false;

            case VOICE_ALWAYS:
                allowed = true;
                break;

            case VOICE_LCD:
                allowed = !display_is_on();
                break;

            default:
                return false;
        }
    }

    if (!allowed) {
        return false;
    }

    // Normally a new announcement is dropped while one is already
    // speaking. With voice_interrupt on, it's instead allowed through
    // here and the caller (see voice_stop_current()) cuts the old one
    // off cooperatively so the newest announcement is always heard,
    // screen-reader style.
    if (run && !cfg.voice.interrupt()->get()) {
        return false;
    }

    return true;
}

// Cooperatively stops whatever announcement is currently running (or
// still waiting out its pre-speech delay) and waits for it to finish its
// own cleanup, so audio_set_play_mode(AUDIO_PLAY_OFF) always runs before
// a new announcement takes over. Safe to call even if nothing is running.
static void voice_stop_current() {
    stop_requested = true;

    if (thread) {
        pthread_join(thread, NULL);
    }

    stop_requested = false;
}

// The 1 second pre-speech wait below exists to avoid trying to speak every
// value flown past while rapidly turning a knob - only the one landed on.
// That's only needed because interrupting wasn't safe before voice_interrupt
// existed; with it on, a still-waiting or still-speaking announcement is
// always safely cut off by the next one anyway (see voice_stop_current()).
// But dropping the wait to 0 turned out to have a second, less obvious job:
// it also gave several announcements fired back-to-back during heavier UI
// work (e.g. opening/closing a dialog) a cheap way to cancel each other
// while still just waiting to start, before any of them paid the real cost
// of starting the speech engine. voice_interrupt_delay_ms lets that tradeoff
// be tuned live instead of guessed at - 0 is closest to instant, 1000
// reproduces the original always-1-second wait.
static uint32_t voice_debounce_delay() {
    if (!cfg.voice.interrupt()->get()) {
        return 1000000;
    }
    return (uint32_t)cfg.voice.interrupt_delay_ms()->get() * 1000;
}

// Frequency tuning gets its own delay, separate from voice_debounce_delay()
// above: the main VFO knob is typically spun much faster/more continuously
// than a settings spinbox or on-screen keyboard letter, so the short delay
// that works well elsewhere isn't long enough here - fast tuning re-triggers
// this too often, each attempt only surviving a moment before the next spin
// cancels it, which is audible as chatter rather than one clean announcement
// once you settle on a frequency.
static uint32_t voice_freq_debounce_delay() {
    if (!cfg.voice.interrupt()->get()) {
        return 1000000;
    }
    return (uint32_t)cfg.voice.freq_interrupt_delay_ms()->get() * 1000;
}

void voice_delay_say_text_fmt(const char * fmt, ...) {
    if (!voice_enable()) {
        return;
    }

    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    prompt_len = 0;

    voice_stop_current();

    delay = voice_debounce_delay();
    pthread_create(&thread, NULL, say_thread, NULL);
}

void voice_say_text_fmt(const char * fmt, ...) {
    if (!voice_enable()) {
        return;
    }

    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    prompt_len = 0;

    voice_stop_current();

    delay = 0;
    pthread_create(&thread, NULL, say_thread, NULL);
}

// Speak "prompt" followed by a formatted value, throttling to just the value
// on repeat (see prompt_len / say_thread) instead of relying on a magic '|'
// character in the formatted text - upstream's still-unfixed page-name-drop
// bug (1565f2e) came from exactly that: any string that happened to contain
// a literal '|' engaged the throttle even when it wasn't meant to.
void voice_say_prompted_fmt(const char *prompt, const char *fmt, ...) {
    if (!voice_enable()) {
        return;
    }

    size_t plen = strlen(prompt);
    if (plen >= sizeof(buf)) {
        plen = sizeof(buf) - 1;
    }
    memcpy(buf, prompt, plen);

    va_list args;

    va_start(args, fmt);
    vsnprintf(buf + plen, sizeof(buf) - plen, fmt, args);
    va_end(args);

    prompt_len = plen;

    voice_stop_current();

    delay = voice_debounce_delay();
    pthread_create(&thread, NULL, say_thread, NULL);
}

/* Band last spoken with a frequency; -2: none yet. UI thread only. */
static int32_t last_band_id = -2;

/* "40 meters CW, " when `freq` is in another band than last spoken (or out
 * of band), else "". */
static std::string band_prefix(int32_t freq, bool force) {
    BandInfoLoadResult band = BandsTable::get_by_freq((uint32_t)freq);
    int32_t            id   = band.rc == SUCCESS ? band.value.id : BAND_UNDEFINED;

    if (!force && id == last_band_id) {
        return "";
    }
    last_band_id = id;

    return voice::band_text(band.value.name, id) + ", ";
}

static void voice_say_freq_impl(uint64_t freq, uint32_t say_delay) {
    if (!voice_enable()) {
        return;
    }

    /* Tuning across a band edge says the new band first. Being part of the
     * prompt, it isn't repeated while tuning within the band. */
    std::string band = band_prefix((int32_t)freq, false);
    size_t      pos  = snprintf(buf, sizeof(buf), "%s", band.c_str());

    if (pos >= sizeof(buf)) {
        pos = sizeof(buf) - 1;
    }

    /* "14. 0 0 5": leading zeros of the kHz part are spoken, so 14.005 MHz
     * isn't heard as "14. 5"; Hz are digits after "point" (14.200050 MHz
     * was read as "200.50"). The "14. " prompt is skipped on repeat. */
    size_t plen = voice_text_freq((int32_t)freq, buf + pos, sizeof(buf) - pos);

    prompt_len = plen ? pos + plen : 0;

    voice_stop_current();

    delay = say_delay;
    pthread_create(&thread, NULL, say_thread, NULL);
}

// For continuous VFO-knob tuning, debounced against fast spinning - see
// voice_freq_debounce_delay().
void voice_say_freq(uint64_t freq) {
    voice_say_freq_impl(freq, voice_freq_debounce_delay());
}

// For a discrete, one-shot frequency change (e.g. a button press that jumps
// straight to a new frequency) - nothing to debounce against, so this should
// read back immediately like any other button-press announcement, not wait
// through the knob-tuning delay.
void voice_say_freq_now(uint64_t freq) {
    voice_say_freq_impl(freq, 0);
}

// Combines a spoken prefix with an immediate frequency reading in one
// utterance, e.g. "F T 8, 14.074" when opening a mode's dialog. Deliberately
// one call rather than a prompt announcement followed by voice_say_freq_now()
// - two separate calls each stop whatever is currently speaking, so the
// first would just get cut off by the second before it was ever heard.
void voice_say_prompted_freq_now(const char *prompt, uint64_t freq) {
    if (!voice_enable()) {
        return;
    }

    int    n   = snprintf(buf, sizeof(buf), "%s, ", prompt);
    size_t pos = (n > 0 && (size_t)n < sizeof(buf)) ? (size_t)n : sizeof(buf) - 1;

    voice_text_freq((int32_t)freq, buf + pos, sizeof(buf) - pos);
    prompt_len = 0;

    voice_stop_current();

    delay = 0;
    pthread_create(&thread, NULL, say_thread, NULL);
}

// Always announces in full ("Auto gain is on") rather than throttling to
// just the value like the numeric helpers do - on a plain on/off toggle,
// losing the control name after the first press is more disorienting than
// useful, unlike a knob where the changing number is self-explanatory.
void voice_say_bool(const char *prompt, bool x) {
    voice_delay_say_text_fmt("%s is %s", prompt, x ? "on" : "off");
}

void voice_say_int(const char *prompt, int32_t x) {
    voice_say_prompted_fmt(prompt, "%i", x);
}

void voice_say_float(const char *prompt, float x) {
    voice_say_prompted_fmt(prompt, "%.1f", x);
}

void voice_say_float2(const char *prompt, float x) {
    voice_say_prompted_fmt(prompt, "%.2f", x);
}

void voice_say_text(const char *prompt, const char *x) {
    voice_say_prompted_fmt(prompt, "%s", x);
}

void voice_say_lang() {
    const voice_synth_choice_t *voices;
    size_t                      n = voice_synth_voices(&voices);
    int32_t                     i = voice_synth_rate_wpm() ? cfg.voice.espeak_voice()->get() : cfg.voice.lang()->get();

    if (i < 0 || (size_t)i >= n) {
        i = 0;
    }
    voice_delay_say_text_fmt("Hello. This is %s voice %s", voice_synth_engine(), voices[i].label);
}

/***** On-demand readouts *****/

/* Asked for by a key press: spoken now even with voice mode off, and
 * even if something is being said with "Interrupt speech" off, which
 * would otherwise drop it. Not while recording (the mic would hear it). */
/* Speech uses the radio's audio path, which is transmitted while keyed
 * up: nothing asked for by a key or typed is spoken then. */
static bool on_air() {
    return radio_get_state() != RADIO_RX;
}

static void say_on_demand(const std::string &text) {
    last_request_tick = lv_tick_get();
    have_request      = true;

    if (recorder_is_on() || on_air()) {
        return;
    }

    /* Stop first: the old thread may still be reading buf. */
    voice_stop_current();

    snprintf(buf, sizeof(buf), "%s", text.c_str());
    prompt_len = 0;

    delay = 0;
    pthread_create(&thread, NULL, say_thread, NULL);
}

static const char *mode_text(int32_t mode) {
    switch ((x6100_mode_t)mode) {
        case x6100_mode_lsb:     return "LSB";
        case x6100_mode_lsb_dig: return "LSB digital";
        case x6100_mode_usb:     return "USB";
        case x6100_mode_usb_dig: return "USB digital";
        case x6100_mode_cw:      return "CW";
        case x6100_mode_cwr:     return "CW reverse";
        case x6100_mode_am:      return "A M";
        case x6100_mode_nfm:     return "N F M";
    }
    return "unknown mode";
}

void voice_announce_band() {
    if (!voice_enable()) {
        return;
    }

    int32_t     freq = cfg.cur.fg_freq()->get();
    std::string band = band_prefix(freq, true);
    char        f[64];

    voice_text_freq(freq, f, sizeof(f));
    voice_say_text_fmt("%s%s", band.c_str(), f);
}

void voice_say_requested(const char *text) {
    if (text && *text) {
        say_on_demand(text);
    }
}

void voice_say_typing(const char *text) {
    if (!text || !*text || cfg.voice.mode()->get() == VOICE_OFF) {
        return;
    }
    if (cfg.voice.mode()->get() == VOICE_LCD && display_is_on()) {
        return;
    }
    /* Each key cuts short the last one's echo, as in a screen reader,
     * whether or not "Interrupt speech" is on. */
    say_on_demand(text);
}

void voice_silence() {
    voice_stop_current();
}

void voice_say_status() {
    int32_t freq  = cfg.cur.fg_freq()->get();
    bool    vfo_b = cfg.band.current_vfo()->get() == X6100_VFO_B;

    BandInfoLoadResult band = BandsTable::get_by_freq((uint32_t)freq);
    int32_t            id   = band.rc == SUCCESS ? band.value.id : BAND_UNDEFINED;

    char f[64];
    voice_text_freq(freq, f, sizeof(f));

    std::string text = vfo_b ? "V F O B. " : "V F O A. ";

    text += voice::band_text(band.value.name, id) + ". ";
    text += std::string(f) + ". ";
    text += std::string(mode_text(cfg.cur.mode()->get())) + ". ";
    text += "Filter " + std::to_string(cfg.filter.bw()->get()) + " hertz. ";
    text += "Power " + voice::number_text(cfg.pwr()->get(), 1) + " watts.";

    if (cfg.band.split()->get()) {
        text += " Split on.";
    }
    if (lm_get_freq()) {
        text += " Frequency locked.";
    }

    say_on_demand(text);
}

void voice_say_meter() {
    std::string text = voice::s_meter_text(meter_get_db()) + ". ";
    float       pwr = 0.0f, swr = 0.0f;

    float alc = 0.0f;

    /* CW: how far the received tone is from yours (the tuning aid). */
    x6100_mode_t mode = (x6100_mode_t)cfg.cur.mode()->get();
    float        offset;

    if ((mode == x6100_mode_cw || mode == x6100_mode_cwr) && cw_tune_get_offset(&offset)) {
        int hz = (int)lroundf(offset / 10.0f) * 10;

        if (hz == 0) {
            text += "CW signal on your tone. ";
        } else {
            text += "CW signal " + std::to_string(hz < 0 ? -hz : hz) + " hertz " + (hz > 0 ? "above" : "below") +
                    " your tone. ";
        }
    }

    if (tx_info_get_summary(&pwr, &swr, &alc)) {
        /* The SWR meter stops at 5. */
        std::string swr_text = swr >= 4.95f ? "5 or more" : voice::number_text(swr < 1.0f ? 1.0f : swr, 1);

        text += "Last transmit, power " + voice::number_text(pwr, 1) + " watts, S W R " + swr_text + ", A L C " +
                voice::number_text(alc, 1) + ".";
    } else {
        text += "No transmission yet.";
    }

    say_on_demand(text);
}

void voice_say_time() {
    static const char *days[]   = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *months[] = {"January", "February", "March",     "April",   "May",      "June",
                                   "July",    "August",   "September", "October", "November", "December"};

    /* As the clock on screen shows it (the radio's own clock). */
    time_t    now = time(NULL);
    struct tm t;

    if (!localtime_r(&now, &t)) {
        say_on_demand("Time unknown.");
        return;
    }

    char text[96];
    snprintf(text, sizeof(text), "%02d %02d, %s %d %s %d.", t.tm_hour, t.tm_min, days[t.tm_wday], t.tm_mday,
             months[t.tm_mon], t.tm_year + 1900);
    say_on_demand(text);
}

static std::string read_file(const char *path) {
    std::ifstream in(path);

    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void voice_say_memory() {
    std::string       meminfo = read_file("/proc/meminfo");
    std::string       status  = read_file("/proc/self/status");
    voice::MemoryInfo m;

    m.total_kb      = voice::meminfo_kb(meminfo, "MemTotal");
    m.available_kb  = voice::meminfo_kb(meminfo, "MemAvailable");
    m.self_kb       = voice::meminfo_kb(status, "VmRSS");
    m.swap_total_kb = voice::meminfo_kb(meminfo, "SwapTotal");
    m.swap_free_kb  = voice::meminfo_kb(meminfo, "SwapFree");

    /* zram: data stored (bytes), compressed, memory used - see
     * Documentation/admin-guide/blockdev/zram.rst, mm_stat. */
    long long stored = 0, compr = 0, used = 0;

    if (sscanf(read_file("/sys/block/zram0/mm_stat").c_str(), "%lld %lld %lld", &stored, &compr, &used) != 3) {
        stored = used = 0;
    }
    m.zram_stored_kb = (long)(stored / 1024);
    m.zram_used_kb   = (long)(used / 1024);

    say_on_demand(voice::memory_text(m));
}

/***** Pop-up messages *****/

/* "#FFFFFF NR: On" -> "NR: On": LVGL recolor marks ("#RGB ", "#RRGGBB ",
 * and the closing '#') are not spoken. */
static std::string strip_recolor(const char *text) {
    std::string out;

    for (const char *p = text; *p; p++) {
        if (*p != '#') {
            out += *p;
            continue;
        }

        size_t n = 0;
        while (n < 6 && isxdigit((unsigned char)p[1 + n])) {
            n++;
        }
        if ((n == 3 || n == 6) && p[1 + n] == ' ') {
            p += n + 1; /* the mark and its space */
        }
        /* else: a closing '#', dropped */
    }
    return out;
}

void voice_say_popup(const char *text, uint32_t created_tick) {
    /* An announcement asked for within this window of the pop-up being
     * made belongs to the same action, which then needs no pop-up speech. */
    static const uint32_t paired_ms = 1000;
    /* A pop-up that only differs in numbers from the one just spoken is a
     * countdown or progress update ("Beacon pause: 9 s"): not repeated. */
    static const uint32_t repeat_ms = 10000;

    static std::string last_key;
    static uint32_t    last_tick = 0;

    if (!text || !*text || !cfg.voice.popups()->get()) {
        return;
    }
    if (radio_get_state() != RADIO_RX) {
        return; /* would go out on the air with the headset PTT */
    }

    if (have_request) {
        uint32_t req  = last_request_tick;
        uint32_t diff = req > created_tick ? req - created_tick : created_tick - req;

        if (diff <= paired_ms) {
            return;
        }
    }

    std::string clean = strip_recolor(text);
    std::string key;

    for (char c : clean) {
        if (!isdigit((unsigned char)c)) {
            key += c;
        }
    }
    if (key == last_key && lv_tick_elaps(last_tick) < repeat_ms) {
        return;
    }

    std::string spoken = voice::label_text(clean);

    if (spoken.empty() || !voice_enable()) {
        return;
    }
    last_key  = key;
    last_tick = lv_tick_get();

    voice_say_text_fmt("%s", spoken.c_str());
}
