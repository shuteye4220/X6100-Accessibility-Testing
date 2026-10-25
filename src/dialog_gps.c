/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */
#include "dialog_gps.h"

#include "gps.h"
#include "dialog.h"
#include "styles.h"
#include "events.h"
#include "pubsub_ids.h"
#include "radio.h"
#include "keyboard.h"
#include "qth/qth.h"
#include "cfg/cfg_api.h"
#include "msg.h"
#include "buttons.h"
#include "voice.h"
#include "voice_text.h"
#include "lvgl/lvgl.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define HEIGHT 42
#define LABEL_WIDTH 300

static void construct_cb(lv_obj_t *parent);
static void destruct_cb();
static void key_cb(lv_event_t * e);

static void summary_btn_cb(button_data_t *btn);
static void grid_btn_cb(button_data_t *btn);
static void position_btn_cb(button_data_t *btn);
static void time_btn_cb(button_data_t *btn);
static void sats_btn_cb(button_data_t *btn);

typedef enum {
    deg_dd,
    deg_ddmm,
    deg_ddmmss
} deg_str_type;

static deg_str_type         deg_type = deg_ddmm;

static lv_obj_t             *satellites_cnt;
static lv_obj_t             *fix;
static lv_obj_t             *date;
static lv_obj_t             *lat;
static lv_obj_t             *lon;
static lv_obj_t             *qth;
static lv_obj_t             *status;

static lv_timer_t           *status_update_timer;

static void                 *gps_sub;

/***** Speech *****/

/* The last values seen, for speaking: a GPS report often carries only
 * some of them. Updated in gps_cb() while the window is open. */
static struct {
    bool            have_data;
    int             mode;           /* MODE_NOT_SEEN, MODE_NO_FIX, 2D, 3D */
    bool            have_sats;
    int             sats_visible;
    int             sats_used;
    bool            have_time;
    time_t          time;
    double          lat;
    double          lon;
    char            grid[9];
} seen;

/* Rows read by the multi-function knob, in screen order. */
typedef enum {
    ROW_FIX,
    ROW_SATS,
    ROW_TIME,
    ROW_LAT,
    ROW_LON,
    ROW_GRID,
    ROW_STATUS,
    ROW_COUNT
} gps_row_t;

static int row = -1;

static button_data_t btn_summary  = {.type = BTN_TEXT, .label = "Summary",  .press = summary_btn_cb};
static button_data_t btn_grid     = {.type = BTN_TEXT, .label = "Grid",     .press = grid_btn_cb};
static button_data_t btn_position = {.type = BTN_TEXT, .label = "Position", .press = position_btn_cb};
static button_data_t btn_time     = {.type = BTN_TEXT, .label = "Time",     .press = time_btn_cb};
static button_data_t btn_sats     = {.type = BTN_TEXT, .label = "Sats",     .press = sats_btn_cb};

static buttons_page_t btn_page = {
    {&btn_summary, &btn_grid, &btn_position, &btn_time, &btn_sats}
};

static dialog_t             dialog = {
    .run = false,
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .btn_page = &btn_page,
    .key_cb = key_cb
};

dialog_t                    *dialog_gps = &dialog;

char *deg_to_str2(deg_str_type type, double f, char *buf, unsigned int buf_size, const char *suffix_pos, const char *suffix_neg) {
    int dsec, sec, deg, min;
    double fdsec, fsec, fdeg, fmin;
    const char *suffix = "";

    if (20 > buf_size) {
        strncpy(buf, "Err", buf_size);
        return buf;
    }

    if (!isfinite(f) || 360.0 < fabs(f)) {
        strncpy(buf, "N/A", buf_size);
        return buf;
    }

    /* suffix? */
    if (0.0 > f) {
        f = -f;
        if (NULL != suffix_neg) {
            suffix = suffix_neg;
        }
    } else if (NULL != suffix_pos) {
        suffix = suffix_pos;
    }

    /* add rounding quanta */
    /* IEEE 754 wants round to nearest even.
     * We cheat and just round to nearest.
     * Intel trying to kill off round to nearest even. */
    switch (type) {
        default:
            /* huh? */
            type = deg_dd;
            /* It's not worth battling fallthrough warnings just for two lines */
            f += 0.5 * 1e-8;              /* round up */
            break;

        case deg_dd:
            /* DD.dddddddd */
            f += 0.5 * 1e-8;              /* round up */
            break;

        case deg_ddmm:
            /* DD MM.mmmmmm */
            f += (0.5 * 1e-6) / 60;       /* round up */
            break;

        case deg_ddmmss:
            f += (0.5 * 1e-5) / 3600;     /* round up */
            break;
    }

    fmin = modf(f, &fdeg);
    deg = (int)fdeg;

    if (360 == deg) {
        /* fix round-up roll-over */
        deg = 0;
        fmin = 0.0;
    }

    if (deg_dd == type) {
        /* DD.dddddddd */
        long frac_deg = (long)(fmin * 100000000.0);
        /* cm level accuracy requires the %08ld */
        snprintf(buf, buf_size, "%d.%08ld%s", deg, frac_deg, suffix);
        return buf;
    }

    fsec = modf(fmin * 60, &fmin);
    min = (int)fmin;

    if (deg_ddmm == type) {
        /* DD MM.mmmmmm */
        sec = (int)(fsec * 1000000.0);
        snprintf(buf, buf_size, "%d %s %02d.%06d'", deg, suffix, min, sec);
        return buf;
    }

    /* else DD MM SS.sss */
    fdsec = modf(fsec * 60.0, &fsec);
    sec = (int)fsec;
    dsec = (int)(fdsec * 100000.0);
    snprintf(buf, buf_size, "%d %s %02d' %02d.%05d\"", deg, suffix, min, sec, dsec);

    return buf;
}

static const char *status_text() {
    switch (gps_status()) {
        case GPS_STATUS_WAITING:    return "waiting for a GPS receiver";
        case GPS_STATUS_WORKING:    return "working";
        case GPS_STATUS_RESTARTING: return "restarting";
        case GPS_STATUS_EXITED:     return "stopped";
    }
    return "unknown";
}

static const char *fix_text() {
    if (!seen.have_data) {
        return "No GPS data yet";
    }
    switch (seen.mode) {
        case MODE_3D: return "3D fix";
        case MODE_2D: return "2D fix";
        default:      return "No fix";
    }
}

static bool have_fix() {
    return seen.have_data && seen.mode >= MODE_2D;
}

static void sats_text(char *out, size_t n) {
    if (!seen.have_sats) {
        snprintf(out, n, "Satellites unknown");
    } else {
        snprintf(out, n, "%i satellite%s in use, %i in view", seen.sats_used, seen.sats_used == 1 ? "" : "s",
                 seen.sats_visible);
    }
}

static void time_text(char *out, size_t n) {
    static const char *months[] = {"January", "February", "March",     "April",   "May",      "June",
                                   "July",    "August",   "September", "October", "November", "December"};
    struct tm t;

    if (!seen.have_time || !gmtime_r(&seen.time, &t)) {
        snprintf(out, n, "Time unknown");
        return;
    }
    snprintf(out, n, "%02i %02i UTC, %i %s %i", t.tm_hour, t.tm_min, t.tm_mday, months[t.tm_mon], t.tm_year + 1900);
}

static void grid_text(char *out, size_t n) {
    char g[96];

    if (!have_fix()) {
        snprintf(out, n, "Grid unknown, no fix");
        return;
    }
    voice_text_grid(seen.grid, g, sizeof(g));
    snprintf(out, n, "Grid %s", g);
}

static void lat_text(char *out, size_t n) {
    char c[64];

    if (!have_fix()) {
        snprintf(out, n, "Latitude unknown");
        return;
    }
    voice_text_coord(seen.lat, true, c, sizeof(c));
    snprintf(out, n, "Latitude %s", c);
}

static void lon_text(char *out, size_t n) {
    char c[64];

    if (!have_fix()) {
        snprintf(out, n, "Longitude unknown");
        return;
    }
    voice_text_coord(seen.lon, false, c, sizeof(c));
    snprintf(out, n, "Longitude %s", c);
}

static void summary_text(char *out, size_t n) {
    char sats[64], grid[128], tm[64];

    if (gps_status() != GPS_STATUS_WORKING && !seen.have_data) {
        snprintf(out, n, "GPS %s.", status_text());
        return;
    }

    sats_text(sats, sizeof(sats));
    time_text(tm, sizeof(tm));

    if (have_fix()) {
        grid_text(grid, sizeof(grid));
        snprintf(out, n, "%s. %s. %s. %s.", fix_text(), grid, sats, tm);
    } else {
        snprintf(out, n, "%s. %s. %s.", fix_text(), sats, tm);
    }
}

static void row_text(int r, char *out, size_t n) {
    switch (r) {
        case ROW_FIX:    snprintf(out, n, "%s", fix_text()); break;
        case ROW_SATS:   sats_text(out, n); break;
        case ROW_TIME:   time_text(out, n); break;
        case ROW_LAT:    lat_text(out, n); break;
        case ROW_LON:    lon_text(out, n); break;
        case ROW_GRID:   grid_text(out, n); break;
        case ROW_STATUS: snprintf(out, n, "GPS status, %s", status_text()); break;
        default:         out[0] = '\0'; break;
    }
}

void dialog_gps_say_summary(void) {
    char text[256];

    summary_text(text, sizeof(text));
    voice_say_requested(text);
}

static void summary_btn_cb(button_data_t *btn) {
    dialog_gps_say_summary();
}

static void grid_btn_cb(button_data_t *btn) {
    char text[128];

    grid_text(text, sizeof(text));
    voice_say_requested(text);
}

static void position_btn_cb(button_data_t *btn) {
    char lat[96], lon[96], text[200];

    lat_text(lat, sizeof(lat));
    lon_text(lon, sizeof(lon));
    snprintf(text, sizeof(text), "%s. %s.", lat, lon);
    voice_say_requested(text);
}

static void time_btn_cb(button_data_t *btn) {
    char text[64];

    time_text(text, sizeof(text));
    voice_say_requested(text);
}

static void sats_btn_cb(button_data_t *btn) {
    char text[64];

    sats_text(text, sizeof(text));
    voice_say_requested(text);
}

/* Gaining or losing a fix, and a new grid square, are spoken as they
 * happen; everything else only when asked. */
static void announce_changes(int prev_mode, const char *prev_grid) {
    char text[200], grid[128];
    bool had_fix = prev_mode >= MODE_2D;

    if (have_fix() && !had_fix) {
        grid_text(grid, sizeof(grid));
        snprintf(text, sizeof(text), "GPS %s. %s.", fix_text(), grid);
        voice_say_text_fmt("%s", text);
    } else if (!have_fix() && had_fix) {
        voice_say_text_fmt("GPS fix lost");
    } else if (have_fix() && prev_grid[0] && strcmp(prev_grid, seen.grid) != 0) {
        grid_text(grid, sizeof(grid));
        voice_say_text_fmt("%s", grid);
    }
}

/* Shows the latest GPS data, and speaks a gained or lost fix or a new grid
 * if `announce`. */
static void gps_update(bool announce) {
    struct gps_data_t   data;
    char                str[64];

    gps_get_snapshot(&data);

    int  prev_mode = seen.have_data ? seen.mode : MODE_NO_FIX;
    char prev_grid[sizeof(seen.grid)];

    strcpy(prev_grid, seen.grid);

    seen.have_data = true;
    seen.mode      = data.fix.mode;

    if (data.set & SATELLITE_SET) {
        lv_label_set_text_fmt(satellites_cnt, "%i/%i", data.satellites_visible, data.satellites_used);
        seen.have_sats    = true;
        seen.sats_visible = data.satellites_visible;
        seen.sats_used    = data.satellites_used;
    }

    if (data.set & TIME_SET) {
        seen.have_time = true;
        seen.time      = data.fix.time.tv_sec;
    }

    switch (data.fix.mode) {
        case MODE_3D:
            lv_label_set_text(fix, "3D");
            break;

        case MODE_2D:
            lv_label_set_text(fix, "2D");
            break;

        case MODE_NO_FIX:
            lv_label_set_text(fix, "None");
            break;

        default:
            break;
    }

    if (data.set & TIME_SET) {
        timespec_to_iso8601(data.fix.time, str, sizeof(str));
        lv_label_set_text(date, str);
    } else {
        lv_label_set_text(date, "N/A");
    }

    if (data.fix.mode >= MODE_2D) {
        deg_to_str2(deg_type, data.fix.latitude, str, sizeof(str), "N", "S");
        lv_label_set_text(lat, str);

        deg_to_str2(deg_type, data.fix.longitude, str, sizeof(str), "E", "W");
        lv_label_set_text(lon, str);

        char qth_val[9];
        qth_pos_to_str(data.fix.latitude, data.fix.longitude, qth_val);
        lv_label_set_text(qth, qth_val);

        seen.lat = data.fix.latitude;
        seen.lon = data.fix.longitude;
        snprintf(seen.grid, sizeof(seen.grid), "%s", qth_val);

        const char *saved_qth = PARAM_T_GET(cfg.qth());
        if ((strlen(saved_qth) == 0) || (strncmp(qth_val, saved_qth, strlen(saved_qth)) != 0)) {
            param_t_set(cfg.qth(), qth_val);
            msg_schedule_text_fmt("QTH updated: %s", PARAM_T_GET(cfg.qth()));
        }
    } else {
        lv_label_set_text(lat, "N/A");
        lv_label_set_text(lon, "N/A");
        lv_label_set_text(qth, "N/A");
    }

    if (announce) {
        announce_changes(prev_mode, prev_grid);
    }
}

static void gps_cb(void *s, lv_msg_t *m) {
    gps_update(true);
}

static void gps_status_update_timer(lv_timer_t * timer)
{
    static char*  gps_status_str[] = {
        "waiting",
        "working",
        "restarting",
        "exited",
    };
    lv_label_set_text_fmt(status, "%s", gps_status_str[gps_status()]);
}

static void construct_cb(lv_obj_t *parent) {
    lv_obj_t    *label;
    lv_coord_t  y = 32;

    dialog.obj = dialog_init(parent);

    memset(&seen, 0, sizeof(seen));
    row = -1;

    lv_group_add_obj(keyboard_group, dialog.obj);
    lv_obj_add_event_cb(dialog.obj, key_cb, LV_EVENT_KEY, NULL);

    /* Edit mode: the multi-function knob then sends LEFT/RIGHT (read the
     * rows) and its press ENTER (the summary) to this window, instead of
     * moving focus, as in the Bluetooth window. */
    lv_group_focus_obj(dialog.obj);
    lv_group_set_editing(keyboard_group, true);
    gps_sub = lv_msg_subscribe(MSG_GPS, gps_cb, NULL);

    /* Working, satellites count */

    label = lv_label_create(dialog.obj);

    lv_label_set_text(label, "Sat in view/in use:");
    lv_obj_set_size(label, LABEL_WIDTH, HEIGHT);
    lv_obj_set_pos(label, 30, y);

    satellites_cnt = lv_label_create(dialog.obj);

    lv_label_set_text(satellites_cnt, "N/A");
    lv_obj_set_size(satellites_cnt, 450, HEIGHT);
    lv_obj_set_pos(satellites_cnt, LABEL_WIDTH + 50, y);

    y += HEIGHT;

    /* Fix */

    label = lv_label_create(dialog.obj);

    lv_label_set_text(label, "Fix:");
    lv_obj_set_size(label, LABEL_WIDTH, HEIGHT);
    lv_obj_set_pos(label, 30, y);

    fix = lv_label_create(dialog.obj);

    lv_label_set_text(fix, "N/A");
    lv_obj_set_size(fix, 450, HEIGHT);
    lv_obj_set_pos(fix, LABEL_WIDTH + 50, y);

    y += HEIGHT;

    /* Date, time */

    label = lv_label_create(dialog.obj);

    lv_label_set_text(label, "Date, time:");
    lv_obj_set_size(label, LABEL_WIDTH, HEIGHT);
    lv_obj_set_pos(label, 30, y);

    date = lv_label_create(dialog.obj);

    lv_label_set_text(date, "N/A");
    lv_obj_set_size(date, 450, HEIGHT);
    lv_obj_set_pos(date, LABEL_WIDTH + 50, y);

    y += HEIGHT;

    /* Lat */

    label = lv_label_create(dialog.obj);

    lv_label_set_text(label, "Latitude:");
    lv_obj_set_size(label, LABEL_WIDTH, HEIGHT);
    lv_obj_set_pos(label, 30, y);

    lat = lv_label_create(dialog.obj);

    lv_label_set_text(lat, "N/A");
    lv_obj_set_size(lat, 450, HEIGHT);
    lv_obj_set_pos(lat, LABEL_WIDTH + 50, y);

    y += HEIGHT;

    /* Lon */

    label = lv_label_create(dialog.obj);

    lv_label_set_text(label, "Longitude:");
    lv_obj_set_size(label, LABEL_WIDTH, HEIGHT);
    lv_obj_set_pos(label, 30, y);

    lon = lv_label_create(dialog.obj);

    lv_label_set_text(lon, "N/A");
    lv_obj_set_size(lon, LABEL_WIDTH, HEIGHT);
    lv_obj_set_pos(lon, LABEL_WIDTH + 50, y);

    y += HEIGHT;

    /* QTH Grid */

    label = lv_label_create(dialog.obj);

    lv_label_set_text(label, "QTH Grid:");
    lv_obj_set_size(label, LABEL_WIDTH, HEIGHT);
    lv_obj_set_pos(label, 30, y);

    qth = lv_label_create(dialog.obj);

    lv_label_set_text(qth, "N/A");
    lv_obj_set_size(qth, 450, HEIGHT);
    lv_obj_set_pos(qth, LABEL_WIDTH + 50, y);

    y += HEIGHT;

    /* GPS status */

    label = lv_label_create(dialog.obj);

    lv_label_set_text(label, "GPS status");
    lv_obj_set_size(label, LABEL_WIDTH, HEIGHT);
    lv_obj_set_pos(label, 30, y);

    status = lv_label_create(dialog.obj);

    lv_label_set_text_fmt(status, "%s", "");
    lv_obj_set_size(status, 450, HEIGHT);
    lv_obj_set_pos(status, LABEL_WIDTH + 50, y);

    status_update_timer = lv_timer_create(gps_status_update_timer, 500,  NULL);
    lv_timer_ready(status_update_timer);

    /* The latest data, if the receiver has already reported. The summary
     * below covers it, so nothing is announced here. */
    if (gps_status() == GPS_STATUS_WORKING) {
        gps_update(false);
    }

    char text[256];
    summary_text(text, sizeof(text));
    voice_say_text_fmt("GPS window. %s", text);
}

static void destruct_cb() {
    lv_group_set_editing(keyboard_group, false);

    if (gps_sub) {
        lv_msg_unsubscribe(gps_sub);
        gps_sub = NULL;
    }
    lv_timer_del(status_update_timer);
}

static void key_cb(lv_event_t * e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct(&dialog);
            break;

        /* Multi-function knob: read the rows one at a time. */
        case LV_KEY_RIGHT:
        case LV_KEY_DOWN:
        case LV_KEY_LEFT:
        case LV_KEY_UP: {
            bool fwd = (key == LV_KEY_RIGHT || key == LV_KEY_DOWN);
            char text[128];

            if (row < 0) {
                row = fwd ? 0 : ROW_COUNT - 1;
            } else {
                row = (row + (fwd ? 1 : ROW_COUNT - 1)) % ROW_COUNT;
            }
            row_text(row, text, sizeof(text));
            voice_say_text_fmt("%s", text);
            break;
        }

        case LV_KEY_ENTER:
            summary_btn_cb(NULL);
            break;

        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;

        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
    }
}
