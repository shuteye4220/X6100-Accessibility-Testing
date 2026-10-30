/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Speech to Bluetooth headphones. See voice_bt.h.
 *
 *  Finding the headphones takes a "pactl list short sinks", too slow to
 *  run before each thing said, so a thread of its own looks every
 *  POLL_S seconds while the setting is on, and speech takes what it
 *  found last. Headphones that go away in between: the stream on them
 *  fails to open and speech falls back to the radio (voice.cpp).
 */

#include "voice_bt.h"

#include "bt_audio.h"
#include "cfg/cfg_api.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define POLL_S 3

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static char            sink[96];
static bool            started = false; /* UI thread only */

static void *watch_thread(void *arg) {
    (void)arg;

    for (;;) {
        char found[sizeof(sink)];

        found[0] = '\0';
        if (param_i_get(cfg.voice.bt_speech())) {
            bt_audio_find_sink(found, sizeof(found));
        }

        pthread_mutex_lock(&lock);
        snprintf(sink, sizeof(sink), "%s", found);
        pthread_mutex_unlock(&lock);

        sleep(POLL_S);
    }
    return NULL;
}

void voice_bt_init(void) {
    pthread_t thread;

    if (started) {
        return;
    }
    if (pthread_create(&thread, NULL, watch_thread, NULL) == 0) {
        pthread_detach(thread);
        started = true;
    }
}

bool voice_bt_sink(char *out, size_t n) {
    if (!out || n == 0) {
        return false;
    }
    out[0] = '\0';
    if (!param_i_get(cfg.voice.bt_speech())) {
        return false;
    }
    pthread_mutex_lock(&lock);
    snprintf(out, n, "%s", sink);
    pthread_mutex_unlock(&lock);
    return out[0] != '\0';
}
