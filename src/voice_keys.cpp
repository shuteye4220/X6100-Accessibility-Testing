/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Screen-reader keys for a keyboard. See voice_keys.h.
 */

#include "voice_keys.h"

#include "voice.h"
#include "voice_decoder.h"
#include "voice_review.h"
#include "cfg/cfg_api.h"

#include "buttons.h" /* has its own extern "C" */

extern "C" {
#include "clock.h"
#include "keyboard.h"
#include "lvgl/lvgl.h"
}

#include <cstring>

#include <linux/input.h>

#include <set>
#include <string>

/* A second Caps Lock tap within this, with nothing between: caps lock. */
static const uint32_t double_tap_ms = 400;

static bool voice_key_held = false;
static bool ctrl_held      = false;
static bool shift_held     = false;
static bool caps_on        = false;

/* A command used the voice key since it went down: its release is not a
 * Caps Lock tap. */
static bool     voice_key_used  = false;
static uint32_t last_caps_tap   = 0;
static bool     have_caps_tap   = false;
/* Control pressed with nothing else: a tap stops speech. */
static bool     ctrl_alone      = false;

/* Keys pressed with the voice key: their repeats and releases are
 * swallowed too, even if the voice key was let go first. */
static std::set<uint16_t> swallowed;

static voice::TypingEcho echo;

static const char *help_text =
    "Voice key commands. Hold Caps Lock or Insert and press: "
    "Left or Right, previous or next character. "
    "Control Left or Right, previous or next word. "
    "Up or Down, previous or next line. "
    "Home or End, first or newest line. "
    "C, W or L, the current character, word or line. "
    "Enter, read from here to the end. "
    "R, the last text received. "
    "S, status. M, meters. T, time. B, battery. K, function keys. "
    "E, typing echo. A, reading received text aloud. "
    "H, this help. "
    "Tap Control to stop speech. Double tap Caps Lock for caps lock.";

static bool voice_active() {
    return cfg.voice.mode()->get() != VOICE_OFF;
}

static void say(const std::string &text) {
    voice_say_typing(text.c_str());
}

static void cycle_echo() {
    static const char *names[] = {"Typing echo off", "Typing echo characters", "Typing echo words",
                                  "Typing echo characters and words"};
    int32_t m = (cfg.voice.echo()->get() + 1) % 4;

    cfg.voice.echo()->set(m);
    echo.reset();
    say(names[m]);
}

static void cycle_auto_read() {
    static const char *names[] = {"Read received text off", "Read received text, text modes",
                                  "Read received text, all including CW"};
    int32_t m = (cfg.voice.decoded()->get() + 1) % 3;

    cfg.voice.decoded()->set(m);
    say(names[m]);
}

/* The voice key is held and `code` went down (or repeats). True if it was
 * a command. */
static bool command(uint16_t code, bool repeat) {
    voice::ReviewText &r = voice_decoder_review();

    /* Arrows repeat for skimming; the rest act once. */
    switch (code) {
        case KEY_LEFT:  say(ctrl_held ? r.prev_word() : r.prev_char()); return true;
        case KEY_RIGHT: say(ctrl_held ? r.next_word() : r.next_char()); return true;
        case KEY_UP:    say(r.prev_line()); return true;
        case KEY_DOWN:  say(r.next_line()); return true;
        default:        break;
    }
    if (repeat) {
        return true;
    }

    switch (code) {
        case KEY_HOME:  say(r.first_line()); break;
        case KEY_END:   say(r.last_line()); break;
        case KEY_C:     say(r.current_char()); break;
        case KEY_W:     say(r.current_word()); break;
        case KEY_L:     say(r.current_line()); break;
        case KEY_ENTER:
        case KEY_KPENTER: say(r.read_to_end()); break;
        case KEY_R:     voice_decoder_read_last(); break;
        case KEY_S:     voice_say_status(); break;
        case KEY_M:     voice_say_meter(); break;
        case KEY_T:     voice_say_time(); break;
        case KEY_B:     clock_say_bat_info(); break;
        case KEY_K:     buttons_say_keys(); break;
        case KEY_E:     cycle_echo(); break;
        case KEY_A:     cycle_auto_read(); break;
        case KEY_H:
        case KEY_F1:    say(help_text); break;
        default:        return true; /* swallowed, as any voice-key combination */
    }
    return true;
}

bool voice_keys_pre(uint16_t code, int value, bool *toggle_caps) {
    *toggle_caps = false;

    /* Modifiers are tracked whatever happens; xkb still gets them. */
    switch (code) {
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            ctrl_held = value != 0;
            if (value == 1) {
                ctrl_alone = true;
            } else if (value == 0 && ctrl_alone && voice_active()) {
                voice_silence();
            }
            return false;

        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            shift_held = value != 0;
            return false;

        default:
            break;
    }
    if (value == 1) {
        ctrl_alone = false;
    }

    /* Releases and repeats of keys taken by a command. */
    if (value != 1 && swallowed.count(code)) {
        if (value == 0) {
            swallowed.erase(code);
        } else if (voice_key_held) {
            command(code, true);
        }
        return true;
    }

    if (!voice_active()) {
        voice_key_held = false;
        return false;
    }

    /* The voice key itself */
    if (code == KEY_CAPSLOCK || code == KEY_INSERT) {
        if (value == 1) {
            voice_key_held = true;
            voice_key_used = false;
        } else if (value == 0) {
            voice_key_held = false;

            if (code == KEY_CAPSLOCK && !voice_key_used) {
                if (have_caps_tap && lv_tick_elaps(last_caps_tap) < double_tap_ms) {
                    have_caps_tap = false;
                    caps_on       = !caps_on;
                    *toggle_caps  = true;
                    say(caps_on ? "caps lock on" : "caps lock off");
                } else {
                    have_caps_tap = true;
                    last_caps_tap = lv_tick_get();
                }
            }
        }
        return true;
    }

    if (voice_key_held && value == 1) {
        voice_key_used = true;
        have_caps_tap  = false;
        swallowed.insert(code);
        command(code, false);
        return true;
    }

    return false;
}

/* Typing in a text field: what the arrows and deleting land on. The key
 * hasn't been handled yet, so the cursor is where it was. */
static bool text_field_key(lv_obj_t *ta, uint32_t key) {
    const char *text = lv_textarea_get_text(ta);
    size_t      len  = text ? strlen(text) : 0;
    size_t      pos  = lv_textarea_get_cursor_pos(ta);

    if (pos > len) {
        pos = len;
    }

    switch (key) {
        case LV_KEY_LEFT:
            say(pos == 0 ? "start" : voice::char_name(text[pos - 1], true));
            return true;

        case LV_KEY_RIGHT:
            say(pos + 1 >= len ? "end" : voice::char_name(text[pos + 1], true));
            return true;

        case LV_KEY_HOME:
            say(len == 0 ? "empty" : "start, " + voice::char_name(text[0], true));
            return true;

        case LV_KEY_END:
            say("end");
            return true;

        case LV_KEY_BACKSPACE:
            say(pos == 0 ? "start" : voice::char_name(text[pos - 1], true));
            return true;

        case LV_KEY_DEL:
            say(pos >= len ? "end" : voice::char_name(text[pos], true));
            return true;

        default:
            return false;
    }
}

void voice_keys_post(uint16_t code, uint32_t key, int value) {
    (void)code;

    if (value != 1 && !(value == 2 && (key == LV_KEY_LEFT || key == LV_KEY_RIGHT || key == LV_KEY_BACKSPACE))) {
        return;
    }
    if (!voice_active() || key == 0) {
        return;
    }

    /* Only where text is typed: elsewhere keys are shortcuts, which
     * announce themselves. */
    lv_obj_t *focused = lv_group_get_focused(keyboard_group);

    if (!focused || !lv_obj_check_type(focused, &lv_textarea_class)) {
        echo.reset();
        return;
    }

    voice::EchoMode mode = (voice::EchoMode)cfg.voice.echo()->get();

    if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
        echo.key(key, mode); /* keeps the word being typed right */
    }
    if (text_field_key(focused, key)) {
        if (key != LV_KEY_BACKSPACE && key != LV_KEY_DEL) {
            echo.reset();
        }
        return;
    }

    std::string out = echo.key(key, mode);

    if (!out.empty()) {
        say(out);
    }
}
