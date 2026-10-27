/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Winlink window: runs the xara VARA HF modem and the Pat Winlink client
 *  on the radio. Pat is used from its web page, on a phone or computer
 *  on the same Wi-Fi once Network access is on. See dialog_winlink.c.
 */

#pragma once

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_winlink;

/* Speaks the window's status: running or not, connection, speed,
 * bandwidth, network access and where Pat's web page is. */
void dialog_winlink_say_status(void);

#ifdef __cplusplus
}
#endif
