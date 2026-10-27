/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  The radio as a hamlib rigctld, for the Winlink programs on the radio
 *  (dialog_winlink.c): the xara modem keys the transmitter through it,
 *  and Pat sets the frequency of the station it calls.
 *
 *  Only on 127.0.0.1:4532 - programs on the radio itself - and only while
 *  the Winlink window runs the modem. Speaks just enough of rigctld's
 *  protocol for those two: get and set frequency and PTT, chk_vfo,
 *  dump_caps (as a ping), quit.
 *
 *  Keying is the modem's: the codec output goes to the modulator
 *  (radio_set_modem(), as FT8 and the voice messages do), and speech is
 *  held meanwhile (voice_hold()), since it would go out on the air.
 *  It unkeys when the program that keyed hangs up, after WINLINK_RIG_MAX_TX_S
 *  keyed (a stuck modem), and when stopped.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WINLINK_RIG_PORT      4532
/* xara releases PTT after 30 s itself (--ptt-max-s); this is the backstop. */
#define WINLINK_RIG_MAX_TX_S  45

/* Starts listening. False (and why, in errno) if the port can't be had. */
bool winlink_rig_start(void);

/* Unkeys, hangs up and stops. Safe to call when not started. */
void winlink_rig_stop(void);

bool winlink_rig_keyed(void);

/* Times the backstop unkeyed a stuck transmission since started. */
uint32_t winlink_rig_timeouts(void);

#ifdef __cplusplus
}
#endif
