/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - tests for the voice assistant's speakable text
 *  (src/voice_text.cpp)
 */

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <string>

#include "voice_text.h"

using namespace voice;

TEST_CASE("frequencies are unambiguous", "[voice][text]") {
    CHECK(freq_text(14200000) == "14.|200");
    CHECK(freq_text(14074000) == "14.|0 74");
    CHECK(freq_text(14005000) == "14.|0 0 5");
    CHECK(freq_text(14000000) == "14.|0 0 0");
    CHECK(freq_text(7030500) == "7.|0 30 point 5");
    // Was read as "14. 200.50", i.e. 14 200 500 Hz.
    CHECK(freq_text(14200050) == "14.|200 point 0 5");
    CHECK(freq_text(14200005) == "14.|200 point 0 0 5");
    CHECK(freq_text(432100000) == "432.|100");
    CHECK(freq_text(810000) == "810 kilohertz");
    CHECK(freq_text(153500) == "153 point 5 kilohertz");
}

TEST_CASE("the C frequency text marks the MHz prompt", "[voice][text]") {
    char   buf[64];
    size_t plen = voice_text_freq(14005000, buf, sizeof(buf));

    CHECK(std::string(buf) == "14. 0 0 5");
    CHECK(plen == 4);
    CHECK(std::string(buf, plen) == "14. ");

    plen = voice_text_freq(810000, buf, sizeof(buf));
    CHECK(std::string(buf) == "810 kilohertz");
    CHECK(plen == 0);

    char tiny[4];
    plen = voice_text_freq(14005000, tiny, sizeof(tiny));
    CHECK(std::strlen(tiny) == 3);
    CHECK(plen == 0);
}

TEST_CASE("band names are spoken in words", "[voice][text]") {
    CHECK(band_text("20m SSB", 7) == "20 meters SSB");
    CHECK(band_text("160m", 1) == "160 meters");
    CHECK(band_text("70cm", 302) == "70 centimeters");
    CHECK(band_text("Broadcast 41m", 103) == "Broadcast 41 meters");
    CHECK(band_text("CB A", 201) == "CB A");
    CHECK(band_text("20m", -1) == "out of band");
    CHECK(band_text("", 5) == "unnamed band");
}

TEST_CASE("function key labels are spelled out", "[voice][text]") {
    CHECK(label_text("Filter BW:\n2700 Hz") == "Filter bandwidth, 2700 hertz");
    CHECK(label_text("TX power:\n5.0 W") == "TX power, 5.0 watts");
    CHECK(label_text("H-Mic gain:\n50") == "hand mic gain, 50");
    CHECK(label_text("Speed:\n20 wpm") == "Speed, 20 words per minute");
    CHECK(label_text("(VOL 1:2)") == "volume 1 of 2");
    CHECK(label_text("SWR\nScan") == "SWR Scan");
    CHECK(label_text("Volume:\n") == "Volume");
}

TEST_CASE("S-meter readings", "[voice][text]") {
    CHECK(s_meter_text(-121) == "S 1");
    CHECK(s_meter_text(-97) == "S 5");
    CHECK(s_meter_text(-73) == "S 9");
    CHECK(s_meter_text(-53) == "S 9 plus 20 decibels");
    CHECK(s_meter_text(-140) == "S 0");
}

TEST_CASE("numbers drop useless decimals", "[voice][text]") {
    CHECK(number_text(5.0f, 1) == "5");
    CHECK(number_text(1.25f, 2) == "1.25");
    CHECK(number_text(-0.01f, 1) == "0");
}

TEST_CASE("grid squares are spoken phonetically", "[voice][text][gps]") {
    CHECK(grid_text("FN25dk") == "Foxtrot November 2 5 Delta Kilo");
    CHECK(grid_text("FN25") == "Foxtrot November 2 5");
    CHECK(grid_text("") == "unknown");
}

TEST_CASE("coordinates as degrees and minutes", "[voice][text][gps]") {
    CHECK(coord_text(45.4215, true) == "45 degrees 25 point 3 minutes north");
    CHECK(coord_text(-75.6972, false) == "75 degrees 41 point 8 minutes west");
    CHECK(coord_text(-33.0, true) == "33 degrees 0 minutes south");
    CHECK(coord_text(1.0 + 1.0 / 60.0, false) == "1 degree 1 minute east");
    // 59.98 minutes rounds up into the next degree.
    CHECK(coord_text(10.0 + 59.98 / 60.0, true) == "11 degrees 0 minutes north");
    CHECK(coord_text(NAN, true) == "unknown");
}

TEST_CASE("decoded text spells callsigns and abbreviations", "[voice][text][decoder]") {
    CHECK(decoded_text("CQ CQ DE VE3XYZ VE3XYZ K") == "C Q C Q D E V E 3 X Y Z V E 3 X Y Z K");
    CHECK(decoded_text("ur rst 599 599") == "U R R S T 599 599");
    CHECK(decoded_text("NAME IS JIM\r\nQTH OTTAWA") == "NAME IS JIM Q T H OTTAWA");
    CHECK(decoded_text("VE3XYZ/P") == "V E 3 X Y Z stroke P");
    CHECK(decoded_text("  \n\x07  ") == "");
    CHECK(decoded_text("TEST de K1ABC") == "T E S T D E K 1 A B C");
}

TEST_CASE("memory use", "[voice][text][memory]") {
    const std::string meminfo = "MemTotal:         505908 kB\nMemFree:           40000 kB\n"
                                "MemAvailable:     319488 kB\nSwapTotal:        252928 kB\nSwapFree:         232448 kB\n";

    CHECK(meminfo_kb(meminfo, "MemTotal") == 505908);
    CHECK(meminfo_kb(meminfo, "MemAvailable") == 319488);
    CHECK(meminfo_kb(meminfo, "SwapFree") == 232448);
    CHECK(meminfo_kb(meminfo, "Mem") == -1);
    CHECK(meminfo_kb(meminfo, "Cached") == -1);
    CHECK(meminfo_kb("VmPeak:\t  90000 kB\nVmRSS:\t   87040 kB\n", "VmRSS") == 87040);

    MemoryInfo m = {505908, 319488, 87040, 252928, 232448, 61440, 7168};
    CHECK(memory_text(m) == "Memory, 312 megabytes free of 494. This program uses 85. "
                            "Swap, 20 megabytes used of 247, compressed into 7.");
    m.swap_free_kb = m.swap_total_kb;
    CHECK(memory_text(m) == "Memory, 312 megabytes free of 494. This program uses 85. "
                            "Swap, none used of 247 megabytes.");
    m.swap_total_kb = 0;
    m.self_kb       = -1;
    CHECK(memory_text(m) == "Memory, 312 megabytes free of 494. No swap.");
    m.available_kb = -1;
    CHECK(memory_text(m) == "Memory, 494 megabytes. No swap.");
    // A swap file, not zram: no "compressed".
    m = {505908, 319488, 87040, 252928, 232448, 0, 0};
    CHECK(memory_text(m) == "Memory, 312 megabytes free of 494. This program uses 85. "
                            "Swap, 20 megabytes used of 247.");
    m.total_kb = 0;
    CHECK(memory_text(m) == "Memory unknown.");
}

TEST_CASE("the C helpers never overrun", "[voice][text]") {
    char buf[8];

    voice_text_grid("FN25dk", buf, sizeof(buf));
    CHECK(std::strlen(buf) == 7);
    voice_text_coord(45.4215, true, buf, sizeof(buf));
    CHECK(std::strlen(buf) == 7);
    voice_text_label(nullptr, buf, sizeof(buf));
    CHECK(std::string(buf) == "");
}
