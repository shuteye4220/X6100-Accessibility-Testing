/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Speakable text for radio values. See voice_text.h.
 *
 *  Ported from the OpenX6100 voice patches (shuteye4220).
 */

#include "voice_text.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <vector>

namespace voice {

static bool all_digits(const std::string &s, size_t from, size_t to) {
    if (from >= to) {
        return false;
    }
    for (size_t i = from; i < to; i++) {
        if (!isdigit((unsigned char)s[i])) {
            return false;
        }
    }
    return true;
}

static std::vector<std::string> words(const std::string &text) {
    std::vector<std::string> out;
    std::istringstream       in(text);
    std::string              w;

    while (in >> w) {
        out.push_back(w);
    }
    return out;
}

static std::string join(const std::vector<std::string> &w) {
    std::string out;

    for (const std::string &s : w) {
        if (s.empty()) {
            continue;
        }
        if (!out.empty()) {
            out += ' ';
        }
        out += s;
    }
    return out;
}

/* "0 0 5", "0 50", "200": each leading zero is its own word. */
static std::string khz_digits(int khz) {
    if (khz < 10) {
        return "0 0 " + std::to_string(khz);
    }
    if (khz < 100) {
        return "0 " + std::to_string(khz);
    }
    return std::to_string(khz);
}

/* Hz below 1 kHz as decimal digits of the kHz value: 50 -> " point 0 5". */
static std::string hz_digits(int hz) {
    if (hz == 0) {
        return "";
    }

    char buf[4];
    snprintf(buf, sizeof(buf), "%03d", hz);

    std::string digits(buf);
    while (!digits.empty() && digits.back() == '0') {
        digits.pop_back();
    }

    std::string out = " point";
    for (char c : digits) {
        out += ' ';
        out += c;
    }
    return out;
}

std::string freq_text(int32_t freq_hz) {
    if (freq_hz < 0) {
        freq_hz = 0;
    }

    int mhz = freq_hz / 1000000;
    int khz = (freq_hz / 1000) % 1000;
    int hz  = freq_hz % 1000;

    if (mhz == 0) {
        return std::to_string(khz) + hz_digits(hz) + " kilohertz";
    }
    return std::to_string(mhz) + ".|" + khz_digits(khz) + hz_digits(hz);
}

std::string band_text(const std::string &name, int32_t band_id) {
    if (band_id < 0) {
        return "out of band";
    }

    std::vector<std::string> w = words(name);

    for (std::string &s : w) {
        if (s.size() > 2 && s.compare(s.size() - 2, 2, "cm") == 0 && all_digits(s, 0, s.size() - 2)) {
            s = s.substr(0, s.size() - 2) + " centimeters";
        } else if (s.size() > 1 && s.back() == 'm' && all_digits(s, 0, s.size() - 1)) {
            s = s.substr(0, s.size() - 1) + " meters";
        }
    }

    std::string out = join(w);
    return out.empty() ? "unnamed band" : out;
}

struct Expansion {
    const char *from;
    const char *to;
};

/* Whole words only. Matching ignores a trailing comma. */
static const Expansion expansions[] = {
    {"Hz",     "hertz"            },
    {"kHz",    "kilohertz"        },
    {"dB",     "decibels"         },
    {"ms",     "milliseconds"     },
    {"wpm",    "words per minute" },
    {"W",      "watts"            },
    {"BW",     "bandwidth"        },
    {"H-Mic",  "hand mic"         },
    {"I-Mic",  "internal mic"     },
    {"MIC",    "mic"              },
    {"Sel",    "select"           },
    {"Moni",   "monitor"          },
    {"Comp",   "compressor"       },
    {"Dec",    "decoder"          },
    {"a-gain", "anti gain"        },
    {"SNR",    "S N R"            },
    {"VOL",    "volume"           },
    {"MFK",    "multi function"   },
    {"MEM",    "memory"           },
    {"APP",    "application"      },
    {"DFN",    "D F N"            },
    {"ZAP",    "zap"              },
};

std::string label_text(const std::string &label) {
    std::string flat;

    // Line breaks and colons are pauses, except a colon between digits,
    // which is a page number ("1:2" -> "1 of 2"). Parentheses go.
    for (size_t i = 0; i < label.size(); i++) {
        char c = label[i];

        if (c == '\n' || c == '\r' || c == '\t') {
            flat += ' ';
        } else if (c == ':') {
            bool digit_before = i > 0 && isdigit((unsigned char)label[i - 1]);
            bool digit_after  = i + 1 < label.size() && isdigit((unsigned char)label[i + 1]);

            flat += (digit_before && digit_after) ? " of " : ", ";
        } else if (c == '(' || c == ')') {
            flat += ' ';
        } else {
            flat += c;
        }
    }

    std::vector<std::string> w = words(flat);

    for (std::string &s : w) {
        bool        comma = !s.empty() && s.back() == ',';
        std::string core  = comma ? s.substr(0, s.size() - 1) : s;

        for (const Expansion &e : expansions) {
            if (core == e.from) {
                core = e.to;
                break;
            }
        }
        s = comma ? core + "," : core;
    }

    std::string out = join(w);

    // "Volume, " at the end of an empty value reads badly; drop it.
    while (!out.empty() && (out.back() == ',' || out.back() == ' ')) {
        out.pop_back();
    }
    return out;
}

std::string s_meter_text(float dbm) {
    const float s1 = -121.0f;
    const float s9 = -73.0f;

    if (dbm > s9 + 0.5f) {
        int over = (int)std::lround(dbm - s9);
        return "S 9 plus " + std::to_string(over) + " decibels";
    }

    int s = (int)std::lround((dbm - s1) / 6.0f) + 1;

    if (s < 0) {
        s = 0;
    }
    if (s > 9) {
        s = 9;
    }
    return "S " + std::to_string(s);
}

std::string number_text(float value, int decimals) {
    if (decimals < 0) {
        decimals = 0;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, value);

    std::string out(buf);

    if (out.find('.') != std::string::npos) {
        while (!out.empty() && out.back() == '0') {
            out.pop_back();
        }
        if (!out.empty() && out.back() == '.') {
            out.pop_back();
        }
    }
    if (out == "-0") {
        out = "0";
    }
    return out;
}


/* Spelled rather than read as words: Q codes, prosigns and common CW/RTTY
 * abbreviations. Matched on the whole word, ignoring case. */
static const char *const ham_abbrev[] = {
    "CQ",  "DE",  "QRZ", "QSL", "QSO", "QTH", "QRM", "QRN", "QSB", "QRP", "QRO", "QRL", "QRT", "QSY",
    "RST", "TU",  "TNX", "TKS", "PSE", "UR",  "ES",  "FB",  "OM",  "YL",  "XYL", "GM",  "GA",  "GE",
    "GN",  "HW",  "CPY", "BK",  "KN",  "SK",  "AR",  "AS",  "BT",  "CL",  "WX",  "HR",  "NR",  "RPT",
    "AGN", "CUL", "DX",  "OP",  "ANT", "PWR", "RIG", "SRI", "TEST", "NIL",
};

static bool is_ham_abbrev(const std::string &w) {
    for (const char *a : ham_abbrev) {
        if (w.size() != strlen(a)) {
            continue;
        }
        bool same = true;
        for (size_t i = 0; i < w.size(); i++) {
            if (toupper((unsigned char)w[i]) != a[i]) {
                same = false;
                break;
            }
        }
        if (same) {
            return true;
        }
    }
    return false;
}

static std::string spell(const std::string &w) {
    std::string out;

    for (char c : w) {
        if (!isalnum((unsigned char)c) && c != '/') {
            continue;
        }
        if (!out.empty()) {
            out += ' ';
        }
        if (c == '/') {
            out += "stroke";
        } else {
            out += (char)toupper((unsigned char)c);
        }
    }
    return out;
}

std::string decoded_text(const std::string &text) {
    std::string flat;

    for (char c : text) {
        unsigned char u = (unsigned char)c;

        if (c == '\n' || c == '\r' || c == '\t') {
            flat += ' ';
        } else if (u >= 0x20 && u < 0x7f) {
            flat += c;
        }
    }

    std::vector<std::string> w = words(flat);

    for (std::string &s : w) {
        bool letters = false, digits = false;

        for (char c : s) {
            letters |= isalpha((unsigned char)c) != 0;
            digits |= isdigit((unsigned char)c) != 0;
        }
        if ((letters && digits) || is_ham_abbrev(s)) {
            s = spell(s);
        }
    }
    return join(w);
}

static const char *const phonetic[26] = {
    "Alfa",  "Bravo",   "Charlie", "Delta",  "Echo",   "Foxtrot", "Golf",
    "Hotel", "India",   "Juliett", "Kilo",   "Lima",   "Mike",    "November",
    "Oscar", "Papa",    "Quebec",  "Romeo",  "Sierra", "Tango",   "Uniform",
    "Victor", "Whiskey", "X-ray",  "Yankee", "Zulu",
};

std::string grid_text(const std::string &grid) {
    std::vector<std::string> out;

    for (char c : grid) {
        if (isalpha((unsigned char)c)) {
            out.push_back(phonetic[toupper((unsigned char)c) - 'A']);
        } else if (isdigit((unsigned char)c)) {
            out.push_back(std::string(1, c));
        }
    }
    return out.empty() ? "unknown" : join(out);
}

std::string coord_text(double deg, bool latitude) {
    if (!std::isfinite(deg) || std::fabs(deg) > 180.0) {
        return "unknown";
    }

    const char *hemi = latitude ? (deg < 0 ? "south" : "north") : (deg < 0 ? "west" : "east");
    double      a    = std::fabs(deg);
    int         d    = (int)a;
    double      min  = (a - d) * 60.0;

    /* Tenths of a minute (about 185 m): enough to check a position by
     * ear. Round, carrying into the degrees. */
    int tenths = (int)std::lround(min * 10.0);
    if (tenths >= 600) {
        d++;
        tenths -= 600;
    }

    std::string text = std::to_string(d) + (d == 1 ? " degree " : " degrees ");
    text += std::to_string(tenths / 10);
    if (tenths % 10) {
        text += " point " + std::to_string(tenths % 10);
    }
    text += (tenths == 10 ? " minute " : " minutes ");
    return text + hemi;
}

/***** C interface *****/

static size_t copy_out(const std::string &s, char *out, size_t n) {
    if (!out || n == 0) {
        return 0;
    }
    size_t len = s.size() < n - 1 ? s.size() : n - 1;
    memcpy(out, s.data(), len);
    out[len] = '\0';
    return len;
}

long meminfo_kb(const std::string &text, const char *key) {
    std::string k = std::string(key) + ":";
    size_t      pos = 0;

    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) {
            end = text.size();
        }
        if (text.compare(pos, k.size(), k) == 0) {
            const char *p = text.c_str() + pos + k.size();
            char       *q;
            long        v = strtol(p, &q, 10);

            return q == p ? -1 : v;
        }
        pos = end + 1;
    }
    return -1;
}

/* kB to whole megabytes, rounded; anything at all is at least 1. */
static long mb(long kb) {
    if (kb <= 0) {
        return 0;
    }
    long m = (kb + 512) / 1024;
    return m ? m : 1;
}

std::string memory_text(const MemoryInfo &m) {
    char buf[96];
    std::string out;

    if (m.total_kb <= 0) {
        return "Memory unknown.";
    }
    if (m.available_kb >= 0) {
        snprintf(buf, sizeof(buf), "Memory, %ld megabytes free of %ld.", mb(m.available_kb), mb(m.total_kb));
    } else {
        snprintf(buf, sizeof(buf), "Memory, %ld megabytes.", mb(m.total_kb));
    }
    out = buf;

    if (m.self_kb > 0) {
        snprintf(buf, sizeof(buf), " This program uses %ld.", mb(m.self_kb));
        out += buf;
    }

    if (m.swap_total_kb <= 0) {
        return out + " No swap.";
    }

    long used = m.swap_total_kb - (m.swap_free_kb < 0 ? m.swap_total_kb : m.swap_free_kb);

    if (used <= 0) {
        snprintf(buf, sizeof(buf), " Swap, none used of %ld megabytes.", mb(m.swap_total_kb));
        return out + buf;
    }
    snprintf(buf, sizeof(buf), " Swap, %ld megabytes used of %ld", mb(used), mb(m.swap_total_kb));
    out += buf;
    if (m.zram_stored_kb > 0 && m.zram_used_kb > 0) {
        snprintf(buf, sizeof(buf), ", compressed into %ld", mb(m.zram_used_kb));
        out += buf;
    }
    return out + ".";
}

} // namespace voice

extern "C" {

size_t voice_text_freq(int32_t freq_hz, char *out, size_t n) {
    std::string text = voice::freq_text(freq_hz);
    size_t      bar  = text.find('|');
    size_t      plen = 0;

    if (bar != std::string::npos) {
        text.replace(bar, 1, " ");
        plen = bar + 1;
    }
    voice::copy_out(text, out, n);
    return plen < n ? plen : 0;
}

void voice_text_band(const char *name, int32_t band_id, char *out, size_t n) {
    voice::copy_out(voice::band_text(name ? name : "", band_id), out, n);
}

void voice_text_label(const char *label, char *out, size_t n) {
    voice::copy_out(voice::label_text(label ? label : ""), out, n);
}

void voice_text_s_meter(float dbm, char *out, size_t n) {
    voice::copy_out(voice::s_meter_text(dbm), out, n);
}

void voice_text_number(float value, int decimals, char *out, size_t n) {
    voice::copy_out(voice::number_text(value, decimals), out, n);
}

void voice_text_grid(const char *grid, char *out, size_t n) {
    voice::copy_out(voice::grid_text(grid ? grid : ""), out, n);
}

void voice_text_coord(double deg, bool latitude, char *out, size_t n) {
    voice::copy_out(voice::coord_text(deg, latitude), out, n);
}

void voice_text_decoded(const char *text, char *out, size_t n) {
    voice::copy_out(voice::decoded_text(text ? text : ""), out, n);
}

} // extern "C"
