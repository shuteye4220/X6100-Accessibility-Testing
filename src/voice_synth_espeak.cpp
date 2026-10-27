/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  eSpeak behind voice_synth.h. Written to the speak_lib.h interface that
 *  classic eSpeak (1.48, as in Buildroot 2023.02) and eSpeak NG share, so
 *  either can be built in. Audio goes to PulseAudio, like RHVoice's: the
 *  radio's speaker, or the Bluetooth headphones' sink.
 */

#include "voice_synth.h"

#include "cfg/cfg_api.h"

#include <pulse/error.h>
#include <pulse/simple.h>

#ifdef ESPEAK_CLASSIC /* set by CMake when it finds libespeak */
#include <espeak/speak_lib.h>
#else
#include <espeak-ng/speak_lib.h>
#endif

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

/* Voices: the English accents (the radio's prompts are English). An id
 * may list alternatives, '|'-separated, tried in order: classic eSpeak's
 * name first, then eSpeak NG's. */
static const voice_synth_choice_t voices[] = {
    {"en|en-gb",                    "English (UK)"           },
    {"en-us",                       "English (US)"           },
    {"en-sc|en-gb-scotland",        "English (Scottish)"     },
    {"en-n|en-gb-x-gbclan",         "English (Northern)"     },
    {"en-rp|en-gb-x-rp",            "English (Received)"     },
    {"en-wm|en-gb-x-gbcwmd",        "English (West Midlands)"},
    {"en-wi|en-029",                "English (Caribbean)"    },
};

/* Persons: eSpeak's voice variants, in both versions. */
static const voice_synth_choice_t persons[] = {
    {"",         "Default"      },
    {"m1",       "Male 1"       },
    {"m2",       "Male 2"       },
    {"m3",       "Male 3"       },
    {"m4",       "Male 4"       },
    {"m5",       "Male 5"       },
    {"m6",       "Male 6"       },
    {"m7",       "Male 7"       },
    {"f1",       "Female 1"     },
    {"f2",       "Female 2"     },
    {"f3",       "Female 3"     },
    {"f4",       "Female 4"     },
    {"f5",       "Female 5"     },
    {"klatt",    "Klatt 1"      },
    {"klatt2",   "Klatt 2"      },
    {"klatt3",   "Klatt 3"      },
    {"croak",    "Croak"        },
    {"whisper",  "Whisper"      },
    {"whisperf", "Whisper female"},
};

#define N_VOICES  (sizeof(voices) / sizeof(voices[0]))
#define N_PERSONS (sizeof(persons) / sizeof(persons[0]))

/* eSpeak is not thread safe; voice.cpp speaks one thing at a time, but
 * the lock makes that a rule. */
static std::mutex lock;
static int        sample_rate = 0; /* 0: not initialised, -1: failed */
static std::string current_voice;

/* For the synth callback: eSpeak gives it no user pointer. */
static pa_simple *pa;
static bool (*stop_flag)(void);
static bool       write_failed;

static int synth_cb(short *wav, int numsamples, espeak_EVENT *events) {
    (void)events;

    if (stop_flag && stop_flag()) {
        return 1; /* abort */
    }
    if (wav && numsamples > 0) {
        int err;

        if (pa_simple_write(pa, wav, (size_t)numsamples * sizeof(short), &err) < 0) {
            fprintf(stderr, "voice: pulse write: %s\n", pa_strerror(err));
            write_failed = true;
            return 1;
        }
    }
    return 0;
}

static bool init() {
    if (sample_rate == 0) {
        int options = 0;
#ifdef espeakINITIALIZE_DONT_EXIT
        options |= espeakINITIALIZE_DONT_EXIT;
#endif
        sample_rate = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 100, NULL, options);
        if (sample_rate <= 0) {
            fprintf(stderr, "voice: eSpeak failed to start (voice data missing?)\n");
            sample_rate = -1;
        } else {
            espeak_SetSynthCallback(synth_cb);
        }
    }
    return sample_rate > 0;
}

static int32_t choice_index(ParamInt *p, size_t n) {
    int32_t i = p->get();
    return (i < 0 || (size_t)i >= n) ? 0 : i;
}

/* Sets the voice from the settings, trying each name a voice has; falls
 * back to the plain voice if the person isn't there, then to English. */
static void set_voice() {
    const char *ids    = voices[choice_index(cfg.voice.espeak_voice(), N_VOICES)].id;
    const char *person = persons[choice_index(cfg.voice.espeak_person(), N_PERSONS)].id;
    std::string list(ids);
    std::string want = list + "/" + person;

    if (want == current_voice) {
        return;
    }

    size_t pos = 0;

    while (pos <= list.size()) {
        size_t      bar  = list.find('|', pos);
        std::string name = list.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);

        if (*person && espeak_SetVoiceByName((name + "+" + person).c_str()) == EE_OK) {
            current_voice = want;
            return;
        }
        if (espeak_SetVoiceByName(name.c_str()) == EE_OK) {
            current_voice = want;
            return;
        }
        if (bar == std::string::npos) {
            break;
        }
        pos = bar + 1;
    }

    espeak_SetVoiceByName("en");
    current_voice = want;
}

bool voice_synth_speak(const char *text, const char *device, bool (*should_stop)(void)) {
    std::lock_guard<std::mutex> guard(lock);

    if (!text || !*text || !init()) {
        return false;
    }

    set_voice();

    int pitch = cfg.voice.pitch()->get() - 50; /* 50-150 % -> 0-100 */

    espeak_SetParameter(espeakRATE, cfg.voice.espeak_rate()->get(), 0);
    espeak_SetParameter(espeakPITCH, pitch < 0 ? 0 : pitch > 100 ? 100 : pitch, 0);
    espeak_SetParameter(espeakVOLUME, cfg.voice.volume()->get(), 0);

    pa_sample_spec spec;
    spec.format   = PA_SAMPLE_S16LE;
    spec.rate     = (uint32_t)sample_rate;
    spec.channels = 1;

    /* About 100 ms queued: speech starts and stops promptly. */
    pa_buffer_attr attr;
    attr.maxlength = (uint32_t)-1;
    attr.tlength   = (uint32_t)(sample_rate / 10 * sizeof(short));
    attr.prebuf    = (uint32_t)-1;
    attr.minreq    = (uint32_t)-1;
    attr.fragsize  = (uint32_t)-1;

    int err;
    pa = pa_simple_new(NULL, "X6100 GUI", PA_STREAM_PLAYBACK, device, "Voice", &spec, NULL, &attr, &err);
    if (!pa) {
        fprintf(stderr, "voice: pulse: %s\n", pa_strerror(err));
        return false;
    }

    stop_flag    = should_stop;
    write_failed = false;

    espeak_Synth(text, strlen(text) + 1, 0, POS_CHARACTER, 0, espeakCHARS_AUTO, NULL, NULL);

    bool completed = !should_stop() && !write_failed;

    if (completed) {
        pa_simple_drain(pa, &err);
    } else {
        pa_simple_flush(pa, &err); /* stopped: drop what is queued */
    }
    pa_simple_free(pa);
    pa        = NULL;
    stop_flag = NULL;

    return completed && !should_stop();
}

const char *voice_synth_engine(void) {
    return "eSpeak";
}

size_t voice_synth_voices(const voice_synth_choice_t **list) {
    *list = voices;
    return N_VOICES;
}

size_t voice_synth_persons(const voice_synth_choice_t **list) {
    *list = persons;
    return N_PERSONS;
}

bool voice_synth_rate_wpm(void) {
    return true;
}
