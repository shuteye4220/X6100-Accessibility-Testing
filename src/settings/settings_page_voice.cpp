#include "settings_widgets.h"

#include <string>

#include "../voice.h"
#include "../voice_synth.h"

/***** VOICE *****/

static void make_voice(SettingsPage &page) {
    uint8_t   col = 1;
    lv_obj_t *obj;

    page.label("Voice mode");

    obj = page.dropdown_int(page.grid, *cfg.voice.mode(), " Off \n When LCD off \n Always",
                            "Voice mode");

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, page.row, 1);
    lv_obj_center(obj);

    /* * */

    page.row++;
    /* RHVoice: rate in percent. eSpeak: in words per minute, 80 to 450. */
    bool wpm = voice_synth_rate_wpm();

    page.label(wpm ? "Rate (wpm), pitch, volume" : "Voice rate, pitch, volume");

    if (wpm) {
        obj = page.spinbox_int(page.grid, *cfg.voice.espeak_rate(), 80, 450, "Voice rate, words per minute");
    } else {
        obj = page.spinbox_int(page.grid, *cfg.voice.rate(), 50, 150, "Voice rate");
    }

    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, page.row, 1);
    col += 2;

    obj = page.spinbox_int(page.grid, *cfg.voice.pitch(), 50, 150, "Voice pitch");

    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, page.row, 1);
    col += 2;

    obj = page.spinbox_int(page.grid, *cfg.voice.volume(), 50, 150, "Voice volume");

    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, page.row, 1);
    col += 2;

    page.row++;
}

/* A dropdown of the engine's choices (voice_synth.h). */
static void make_choice(SettingsPage &page, const char *label, ParamInt &param, const voice_synth_choice_t *list,
                        size_t n) {
    page.label(label);

    std::string options;
    for (size_t i = 0; i < n; i++) {
        options += (i ? "\n " : " ") + std::string(list[i].label) + " ";
    }

    lv_obj_t *obj = page.dropdown_int(page.grid, param, options.c_str(), label);

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, page.row, 1);
    lv_obj_center(obj);

    page.row++;
}

static void make_voice_lang(SettingsPage &page) {
    const voice_synth_choice_t *list;
    size_t                      n = voice_synth_voices(&list);

    if (voice_synth_rate_wpm()) {
        /* eSpeak: a voice (accent), and a person (male, female...). */
        make_choice(page, "Voice type", *cfg.voice.espeak_voice(), list, n);

        n = voice_synth_persons(&list);
        if (n) {
            make_choice(page, "Person", *cfg.voice.espeak_person(), list, n);
        }
    } else {
        make_choice(page, "Voice type", *cfg.voice.lang(), list, n);
    }
}

// Screen-reader style behavior (see voice.cpp: voice_enable(),
// voice_debounce_delay(), voice_freq_debounce_delay(), say_thread()).
static void make_voice_behavior(SettingsPage &page) {
    lv_obj_t *obj;

    page.label("Interrupt speech");

    obj = page.switch_bool(page.grid, *cfg.voice.interrupt(), "Interrupt speech");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, page.row, 1);

    page.row++;
    page.label("Full announce every (0=always)");

    obj = page.spinbox_int(page.grid, *cfg.voice.repeat_count(), 0, 20, "Full announce every");

    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 2, LV_GRID_ALIGN_CENTER, page.row, 1);

    page.row++;
    page.label("Interrupt delay, ms");

    obj = page.spinbox_int(page.grid, *cfg.voice.interrupt_delay_ms(), 0, 1000, "Interrupt delay");

    lv_spinbox_set_digit_format(obj, 4, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, page.row, 1);

    page.row++;
    page.label("Frequency interrupt delay, ms");

    obj = page.spinbox_int(page.grid, *cfg.voice.freq_interrupt_delay_ms(), 0, 1000, "Frequency interrupt delay");

    lv_spinbox_set_digit_format(obj, 4, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, page.row, 1);

    page.row++;
    page.label("Read function keys");

    obj = page.switch_bool(page.grid, *cfg.voice.keys(), "Read function keys");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, page.row, 1);

    page.row++;
    page.label("Speak pop-up messages");

    obj = page.switch_bool(page.grid, *cfg.voice.popups(), "Speak pop-up messages");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, page.row, 1);

    page.row++;
    page.label("Read decoded text");

    obj = page.dropdown_int(page.grid, *cfg.voice.decoded(), " Off \n Text modes \n All, including CW ",
                            "Read decoded text");

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, page.row, 1);
    lv_obj_center(obj);

    page.row++;
    page.label("Typing echo");

    obj = page.dropdown_int(page.grid, *cfg.voice.echo(), " None \n Characters \n Words \n Characters and words ",
                            "Typing echo");

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, page.row, 1);
    lv_obj_center(obj);

    page.row++;
}

/***** PAGE *****/

void make_voice_page(SettingsPage &page) {
    page.reset();

    make_voice(page);
    make_voice_behavior(page);
    make_voice_lang(page);

    page.finish();
}
