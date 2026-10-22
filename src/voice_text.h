/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Turning radio values into text that reads well through the speech
 *  synthesizer. Pure functions, unit tested on the host
 *  (tests/test_voice_text.cpp).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <string>

namespace voice {

/* A frequency as "MHz.|kHz": 14 200 050 Hz -> "14.|200 point 0 5". The '|'
 * marks where the MHz prompt ends. Leading zeros of the kHz part are spoken,
 * so 14 005 kHz is not heard as 14.5 MHz. Below 1 MHz: "810 kilohertz". */
std::string freq_text(int32_t freq_hz);

/* "40m CW" -> "40 meters CW", "70cm" -> "70 centimeters"; a negative band
 * id (a gap between bands) is "out of band". */
std::string band_text(const std::string &name, int32_t band_id);

/* An on-screen label made speakable ("Filter BW:\n2700 Hz" -> "Filter
 * bandwidth, 2700 hertz", "(VOL 1:2)" -> "volume 1 of 2"). */
std::string label_text(const std::string &label);

/* dBm as S units: "S 7", "S 9 plus 20 decibels". */
std::string s_meter_text(float dbm);

/* At most `decimals` digits after the point, trailing zeros dropped. */
std::string number_text(float value, int decimals);

/* A Maidenhead locator in ITU phonetics: "FN25dk" -> "Foxtrot November
 * 2 5 Delta Kilo". */
std::string grid_text(const std::string &grid);

/* Degrees as degrees and tenths of minutes: 45.4215, latitude ->
 * "45 degrees 25 point 3 minutes north". */
std::string coord_text(double deg, bool latitude);

} // namespace voice

extern "C" {
#endif

/* C versions, writing into `out` (always terminated). voice_text_freq
 * returns the length of the MHz prompt ("14. ") within `out`, for
 * voice.cpp's repeat throttle; 0 if there is none. */
size_t voice_text_freq(int32_t freq_hz, char *out, size_t n);
void   voice_text_band(const char *name, int32_t band_id, char *out, size_t n);
void   voice_text_label(const char *label, char *out, size_t n);
void   voice_text_s_meter(float dbm, char *out, size_t n);
void   voice_text_number(float value, int decimals, char *out, size_t n);
void   voice_text_grid(const char *grid, char *out, size_t n);
void   voice_text_coord(double deg, bool latitude, char *out, size_t n);

#ifdef __cplusplus
}
#endif
