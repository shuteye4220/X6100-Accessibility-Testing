/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Screen-reader keys for a USB or Bluetooth keyboard (keyboard.c).
 *
 *  The voice key is Caps Lock or Insert, held, as in NVDA and JAWS. With
 *  it, keys read the received text (RTTY now; JS8, PSK31, Olivia... later)
 *  with a review cursor, or speak readouts; these never reach the radio.
 *  Double-tap Caps Lock for a real caps lock. A tap of Control alone stops
 *  speech. Voice key + H lists the keys.
 *
 *  Typing into a text field (callsign, QTH, CW message, Wi-Fi password,
 *  and the text modes' compose boxes) is echoed as set on the Voice
 *  settings page: characters, words, both or none; moving with the arrows
 *  or deleting says the character concerned.
 *
 *  All of it only while Voice mode is on; with it off the keyboard is as
 *  before. UI thread (LVGL input read).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Before xkb sees the key (evdev scancode, value 0 up, 1 down, 2 repeat).
 * True: swallow it, neither xkb nor LVGL get it. Sets *toggle_caps when
 * caps lock should be toggled (Caps Lock double-tapped). */
bool voice_keys_pre(uint16_t scancode, int value, bool *toggle_caps);

/* After xkb (`key`: LVGL key or character, 0 for none), before LVGL
 * handles it. */
void voice_keys_post(uint16_t scancode, uint32_t key, int value);

#ifdef __cplusplus
}
#endif
