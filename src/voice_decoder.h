/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Reading decoded text aloud: RTTY and CW decoder output today, and the
 *  keyboard-to-keyboard modes on their way in mainline (JS8, PSK31,
 *  Olivia). Any app that shows received text calls voice_decoder_add()
 *  with it; text sent to the decoder panel (panel_add_text()) already is.
 *
 *  New text is read out in phrases - at a line break, after a pause in
 *  reception, or every ~80 characters - never cutting itself short: while
 *  a phrase is being spoken, newer text waits. The last few hundred
 *  characters are read by "Read decoded" (an assignable action), and the
 *  last 8000 are kept for the keyboard review cursor (voice_keys.h).
 *
 *  Voice settings, "Read decoded text": Off, Text modes (everything but
 *  the CW decoder - you hear CW yourself), or All.
 *
 *  UI thread only.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VOICE_DECODED_OFF = 0,
    VOICE_DECODED_TEXT_MODES,
    VOICE_DECODED_ALL,
} voice_decoded_t;

/* Received text, as it arrives. `cw`: from the CW decoder. */
void voice_decoder_add(const char *text, bool cw);

/* Kept for "Read decoded" and the review cursor, but not read aloud as it
 * comes (JS8's heartbeats; what an app announces in its own way). */
void voice_decoder_add_review(const char *text);

/* Speak the last received text, on request (even with voice mode off). */
void voice_decoder_read_last();

/* Forget the history (e.g. the panel was cleared). */
void voice_decoder_clear();

#ifdef __cplusplus
}

namespace voice {
class ReviewText;
}

/* The received text, for the keyboard review cursor (voice_keys.cpp). */
voice::ReviewText &voice_decoder_review();
#endif
