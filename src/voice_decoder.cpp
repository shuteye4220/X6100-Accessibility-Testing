/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Reading decoded text aloud. See voice_decoder.h.
 */

#include "voice_decoder.h"

#include "voice.h"
#include "voice_review.h"
#include "voice_text.h"
#include "cfg/cfg_api.h"

#include "lvgl/lvgl.h"

#include <string>

/* Phrase boundaries */
static const uint32_t idle_ms      = 1500; /* a pause in reception */
static const size_t   phrase_chars = 80;
/* Text not yet read is capped: reading can't keep up with a fast
 * stream, and stale text is worth less than current text. */
static const size_t   pending_max  = 400;
/* "Read decoded text" reads this much of the end. */
static const size_t   read_last_chars = 300;

static std::string  pending;
/* Everything received recently, for "Read decoded" and the keyboard
 * review cursor. */
static voice::ReviewText review;
static uint32_t     last_rx  = 0;
static bool         skipped  = false;
static lv_timer_t  *timer    = NULL;

static bool auto_read(bool cw) {
    switch (cfg.voice.decoded()->get()) {
        case VOICE_DECODED_ALL:        return true;
        case VOICE_DECODED_TEXT_MODES: return !cw;
        default:                       return false;
    }
}

static void keep_tail(std::string &s, size_t max) {
    if (s.size() <= max) {
        return;
    }

    size_t cut = s.size() - max;
    size_t sp  = s.find(' ', cut); /* don't start mid-word */

    s.erase(0, (sp != std::string::npos && sp - cut < 20) ? sp + 1 : cut);
}

/* Speaks the pending text if it makes a phrase and nothing is being said. */
static void try_flush(bool force) {
    if (pending.empty()) {
        return;
    }

    bool line   = pending.find('\n') != std::string::npos || pending.find('\r') != std::string::npos;
    bool idle   = lv_tick_elaps(last_rx) >= idle_ms;
    bool long_  = pending.size() >= phrase_chars;

    if (!force && !line && !idle && !long_) {
        return;
    }
    if (voice_is_busy()) {
        return; /* try again on the next tick */
    }

    /* Spelled out, text grows ("VE3XYZ" is "V E 3 X Y Z"): say as much
     * as fits, ending at a space, and leave the rest for the next phrase,
     * rather than have the newest text cut off. */
    std::string prefix = skipped ? "skipped. " : "";
    size_t      take   = pending.size();
    std::string text   = voice::decoded_text(pending);

    while (prefix.size() + text.size() > VOICE_TEXT_MAX && take > 1) {
        size_t cut = pending.rfind(' ', take * 2 / 3);

        take = (cut != std::string::npos && cut > 0) ? cut : take * 2 / 3;
        text = voice::decoded_text(pending.substr(0, take));
    }
    text = prefix + text;
    pending.erase(0, take);
    skipped = false;

    if (!text.empty()) {
        voice_say_text_fmt("%s", text.c_str());
    }
}

static void timer_cb(lv_timer_t *t) {
    try_flush(false);

    if (pending.empty()) {
        lv_timer_pause(timer);
    }
}

void voice_decoder_add(const char *text, bool cw) {
    if (!text || !*text) {
        return;
    }

    review.append(text);

    if (!auto_read(cw)) {
        pending.clear();
        return;
    }

    pending += text;
    if (pending.size() > pending_max) {
        keep_tail(pending, pending_max);
        skipped = true;
    }
    last_rx = lv_tick_get();

    if (!timer) {
        timer = lv_timer_create(timer_cb, 250, NULL);
    }
    lv_timer_resume(timer);
}

voice::ReviewText &voice_decoder_review() {
    return review;
}

void voice_decoder_read_last() {
    std::string last = review.text();

    if (last.size() > read_last_chars) {
        last.erase(0, last.size() - read_last_chars);
        size_t sp = last.find(' ');
        if (sp != std::string::npos && sp < 20) {
            last.erase(0, sp + 1);
        }
    }

    std::string text = voice::decoded_text(last);

    /* Spelled out it may not fit: keep the newest, from a word start. */
    if (text.size() > VOICE_TEXT_MAX) {
        text.erase(0, text.size() - VOICE_TEXT_MAX);
        size_t sp = text.find(' ');
        if (sp != std::string::npos && sp < 40) {
            text.erase(0, sp + 1);
        }
    }

    voice_say_requested(text.empty() ? "Nothing received yet." : text.c_str());
}

void voice_decoder_clear() {
    review.clear();
    pending.clear();
    skipped = false;
}
