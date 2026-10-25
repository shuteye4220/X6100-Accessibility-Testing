/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "dialog.h"

extern dialog_t *dialog_gps;

/* Speaks the fix, grid, satellites and time (the Summary key). */
void dialog_gps_say_summary(void);

