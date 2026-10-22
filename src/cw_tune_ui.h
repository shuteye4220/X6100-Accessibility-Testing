/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2024 Georgy Dyuldin aka R2RFE
 */

#pragma once
#include <stdbool.h>
#include <lvgl/lvgl.h>

void cw_tune_init(lv_obj_t * parent);

// bool cw_tune_toggle(int16_t diff);

void cw_tune_set_freq(float hz);

/* The received CW tone minus your key tone, in Hz, as last measured by
 * the CW tuning aid (CW peak on). False if there was no reading in the
 * last 2 seconds. */
bool cw_tune_get_offset(float *hz);
