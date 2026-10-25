/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  The speech synthesizer behind voice.cpp. One engine is compiled in,
 *  chosen at build time (CMake X6100_TTS):
 *
 *    rhvoice  RHVoice (default)                 voice_synth_rhvoice.cpp
 *    espeak   eSpeak 1.48 or eSpeak NG          voice_synth_espeak.cpp
 *
 *  Each engine has its own voices, and eSpeak also "persons" (its
 *  variants: male, female, whisper...) and a rate in words per minute.
 *  The Voice settings page asks this interface what to offer.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
#include <atomic>

/* Speak `text` and wait until it has been played. `device`: a PulseAudio
 * sink (Bluetooth headphones), NULL for the radio's speaker. Stops early,
 * dropping what's left, once `stop` is set. True if spoken to the end. */
bool voice_synth_speak(const char *text, const char *device, const std::atomic<bool> &stop);

extern "C" {
#endif

typedef struct {
    const char *id;    /* the engine's name for it */
    const char *label; /* shown in Settings, and spoken */
} voice_synth_choice_t;

/* "RHVoice" or "eSpeak" */
const char *voice_synth_engine(void);

/* Voices to choose from (Settings > Voice > Voice type). */
size_t voice_synth_voices(const voice_synth_choice_t **list);

/* Persons (voice variants) to choose from; 0 if the engine has none. */
size_t voice_synth_persons(const voice_synth_choice_t **list);

/* The rate setting is in words per minute (eSpeak), not percent. */
bool voice_synth_rate_wpm(void);

#ifdef __cplusplus
}
#endif
