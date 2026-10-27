/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 *
 *  RHVoice behind voice_synth.h - the synthesis that was in voice.cpp's
 *  say_thread(), unchanged.
 */

#include "voice_synth.h"

#include "cfg/cfg_api.h"
#include "settings_types.h"

#include <iterator>
#include <memory>
#include <sstream>

#include <RHVoice/core/engine.hpp>
#include <RHVoice/core/document.hpp>
#include <RHVoice/core/client.hpp>
#include <RHVoice/audio.hpp>

using namespace RHVoice;

static const voice_synth_choice_t voices[VOICES_NUM] = {
    {"lyubov",      "Lyubov (En)" },
    {"slt",         "SLT (En)"    },
    {"alan",        "Alan (En)"   },
    {"evgeniy-eng", "Evgeniy (En)"},
    {"bdl",         "BDL (En)"    },
    {"clb",         "CLB (En)"    },
};

class audio_player : public client {
  public:
    audio_player(const char *device, bool (*should_stop)(void)) : should_stop(should_stop) {
        stream.set_sample_rate(24000);
        stream.set_buffer_size(512);
        if (device) {
            stream.set_device(device);
        }
        stream.open();
    }

    bool play_speech(const short *buf, std::size_t count) override {
        if (should_stop()) {
            return false;
        }

        try {
            stream.write(buf, count);
            return true;
        } catch (...) {
            stream.close();
            failed = true;
            return false;
        }
    }

    void finish() {
        if (stream.is_open())
            stream.drain();
    }

    bool failed = false;

  private:
    bool (*should_stop)(void);
    audio::playback_stream stream;
};

static std::shared_ptr<engine> eng(new engine);

bool voice_synth_speak(const char *text, const char *device, bool (*should_stop)(void)) {
    int32_t lang = cfg.voice.lang()->get();

    if (lang < 0 || lang >= VOICES_NUM) {
        lang = 0;
    }

    voice_profile profile = eng->create_voice_profile(voices[lang].id);
    audio_player  player(device, should_stop);

    std::istringstream             in{text};
    std::istreambuf_iterator<char> start{in};
    std::istreambuf_iterator<char> end;
    std::unique_ptr<document>      doc = document::create_from_plain_text(eng, start, end, content_text, profile);

    doc->speech_settings.relative.rate   = cfg.voice.rate()->get() / 100.0;
    doc->speech_settings.relative.pitch  = cfg.voice.pitch()->get() / 100.0;
    doc->speech_settings.relative.volume = cfg.voice.volume()->get() / 100.0;
    doc->set_owner(player);

    doc->synthesize();
    player.finish();

    return !should_stop() && !player.failed;
}

const char *voice_synth_engine(void) {
    return "RHVoice";
}

size_t voice_synth_voices(const voice_synth_choice_t **list) {
    *list = voices;
    return VOICES_NUM;
}

size_t voice_synth_persons(const voice_synth_choice_t **list) {
    *list = NULL;
    return 0;
}

bool voice_synth_rate_wpm(void) {
    return false;
}
