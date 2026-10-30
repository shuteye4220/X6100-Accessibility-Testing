/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Speech to Bluetooth headphones (Settings > Voice): with it on and
 *  headphones connected, speech plays only into them - never into the
 *  radio's own audio, which is what the transmitter takes - so it goes on
 *  while transmitting, and the speaker keeps the receiver. Without
 *  headphones, speech is as before.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Starts watching for headphones (a PulseAudio sink list every few
 * seconds, only while the setting is on). */
void voice_bt_init(void);

/* The headphones to speak into: true and their sink name in `out` with
 * the setting on and headphones connected, as last seen. */
bool voice_bt_sink(char *out, size_t n);

#ifdef __cplusplus
}
#endif
