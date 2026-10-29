/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "settings_types.h"

/* The longest text spoken at once; more is cut off. */
#define VOICE_TEXT_MAX 511

#ifdef __cplusplus

#include <cstdarg>

extern "C" {
#include <stdint.h>
#endif

void voice_sure();
void voice_change_mode();

void voice_say_text_fmt(const char * fmt, ...);
void voice_delay_say_text_fmt(const char * fmt, ...);
void voice_say_prompted_fmt(const char *prompt, const char * fmt, ...);
void voice_say_freq(uint64_t freq);
void voice_say_freq_now(uint64_t freq);
void voice_say_prompted_freq_now(const char *prompt, uint64_t freq);

void voice_say_bool(const char *prompt, bool x);
void voice_say_int(const char *prompt, int32_t x);
void voice_say_float(const char *prompt, float x);
void voice_say_float2(const char *prompt, float x);
void voice_say_text(const char *prompt, const char *x);
void voice_say_lang();

void voice_wait_done();

/* True while an announcement is being spoken. */
bool voice_is_busy();

/* On demand, even with voice mode off. Status: VFO, band, frequency, mode,
 * filter width, power, split and lock. Meters: S-meter and the last
 * transmission's power and SWR. */
void voice_say_status();

/* "Radio ready", then the status: said once the radio has started, so a
 * silent start means something is wrong. Follows Voice mode. */
void voice_say_ready();
void voice_say_meter();

/* The radio's clock and date, as on screen. */
void voice_say_time();

/* Free memory, this program's share, and swap (zram) use. */
void voice_say_memory();

/* An on-screen pop-up message (msg.c) as it is shown. Spoken only if no
 * announcement was asked for around the time it was made (the code that
 * made it already speaks), not while transmitting, and not a countdown or
 * progress update of the one just spoken. Setting: Speak pop-up messages. */
void voice_say_popup(const char *text, uint32_t created_tick);

/* After a band key: "40 meters CW, 7. 0 30". */
void voice_announce_band();

/* Typing echo and keyboard review: spoken at once, cutting short what is
 * being said, if Voice mode allows. Never while transmitting. */
void voice_say_typing(const char *text);

/* Stop speaking. */
void voice_silence();

/* While on, nothing is spoken, and speech in progress is cut short: for
 * the modem's transmissions (winlink_rig.c), which speech would get into.
 * Any thread; returns once the speech has stopped (at most 300 ms). */
void voice_hold(bool on);

/* How many announcements have been asked for (said or not): a caller can
 * tell whether something it ran spoke. */
uint32_t voice_request_count();

/* Text asked for by a key press, spoken the same way as the above. */
void voice_say_requested(const char *text);

#ifdef __cplusplus
}
#endif
