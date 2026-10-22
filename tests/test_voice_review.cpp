/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - tests for keyboard review and typing echo
 *  (src/voice_review.cpp)
 */

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "voice_review.h"

using namespace voice;

static const uint32_t BS = 8, ENTER = 10, LEFT = 20;

TEST_CASE("characters by name", "[voice][review]") {
    CHECK(char_name(' ', true) == "space");
    CHECK(char_name('.', true) == "period");
    CHECK(char_name('/', true) == "slash");
    CHECK(char_name('A', true) == "capital A");
    CHECK(char_name('A', false) == "A");
    CHECK(char_name('a', true) == "a");
    CHECK(char_name('7', true) == "7");
}

TEST_CASE("received text keeps one break per line", "[voice][review]") {
    ReviewText r;
    r.append("CQ CQ\r\r\nDE K1ABC\r\n\r\n\r\n\r\nK");
    CHECK(r.text() == "CQ CQ\n\nDE K1ABC\n\nK");
}

TEST_CASE("the cursor starts on the newest line", "[voice][review]") {
    ReviewText r;
    r.append("FIRST LINE\nSECOND LINE\nTHIRD LINE");

    CHECK(r.current_line() == "THIRD LINE");
    CHECK(r.prev_line() == "SECOND LINE");
    CHECK(r.prev_line() == "FIRST LINE");
    CHECK(r.prev_line() == "top, FIRST LINE");
    CHECK(r.next_line() == "SECOND LINE");
    CHECK(r.last_line() == "bottom, THIRD LINE");
    CHECK(r.next_line() == "bottom, THIRD LINE");
    CHECK(r.first_line() == "top, FIRST LINE");
}

TEST_CASE("lines are spoken with callsigns spelled", "[voice][review]") {
    ReviewText r;
    r.append("CQ DE VE3XYZ K");
    CHECK(r.current_line() == "C Q D E V E 3 X Y Z K");
}

TEST_CASE("long lines wrap at a space", "[voice][review]") {
    ReviewText r(8000, 10);
    r.append("ONE TWO THREE FOUR FIVE");

    CHECK(r.first_line() == "top, ONE TWO");
    CHECK(r.next_line() == "THREE FOUR");
    CHECK(r.next_line() == "FIVE");
    CHECK(r.next_line() == "bottom, FIVE");
}

TEST_CASE("characters and words", "[voice][review]") {
    ReviewText r;
    r.append("HI OM 73");

    // newest line: the cursor starts at its first character
    CHECK(r.current_char() == "H");
    CHECK(r.next_char() == "I");
    CHECK(r.next_char() == "space");
    CHECK(r.next_char() == "O");
    CHECK(r.current_word() == "O M");
    CHECK(r.next_word() == "73");
    CHECK(r.next_word() == "bottom, 73");
    CHECK(r.prev_word() == "O M");
    CHECK(r.prev_word() == "HI");
    CHECK(r.prev_word() == "top, HI");
    CHECK(r.prev_char() == "top, H");
}

TEST_CASE("words cross line breaks", "[voice][review]") {
    ReviewText r;
    r.append("ONE\nTWO");
    r.first_line();

    CHECK(r.next_word() == "TWO");
    CHECK(r.prev_word() == "ONE");
}

TEST_CASE("read to end moves the cursor", "[voice][review]") {
    ReviewText r;
    r.append("AAA\nBBB\nCCC");
    r.first_line();

    CHECK(r.read_to_end() == "AAA BBB CCC");
    CHECK(r.current_line() == "CCC");
}

TEST_CASE("an empty review says so", "[voice][review]") {
    ReviewText r;
    CHECK(r.current_line() == "No text");
    CHECK(r.next_char() == "No text");
    CHECK(r.prev_word() == "No text");
}

TEST_CASE("old text is dropped, the cursor follows", "[voice][review]") {
    ReviewText r(10, 60);
    r.append("ABCDEFGHIJ");
    r.first_line();
    r.next_char(); // on B
    r.append("KLM");  // A, B, C dropped
    CHECK(r.text() == "DEFGHIJKLM");
    CHECK(r.current_char() == "D");

    r.clear();
    CHECK(r.current_char() == "No text");
}

TEST_CASE("typing echo: characters", "[voice][echo]") {
    TypingEcho e;
    CHECK(e.key('h', ECHO_CHARS) == "h");
    CHECK(e.key('I', ECHO_CHARS) == "capital I");
    CHECK(e.key(' ', ECHO_CHARS) == "space");
    CHECK(e.key('?', ECHO_CHARS) == "question");
    CHECK(e.key(ENTER, ECHO_CHARS) == "");
}

TEST_CASE("typing echo: words", "[voice][echo]") {
    TypingEcho e;
    for (char c : std::string("cq")) {
        CHECK(e.key(c, ECHO_WORDS) == "");
    }
    CHECK(e.key(' ', ECHO_WORDS) == "C Q");

    for (char c : std::string("ve3xyz")) {
        e.key(c, ECHO_WORDS);
    }
    CHECK(e.key(ENTER, ECHO_WORDS) == "V E 3 X Y Z");

    e.key('h', ECHO_WORDS);
    e.key('x', ECHO_WORDS);
    e.key(BS, ECHO_WORDS);
    e.key('i', ECHO_WORDS);
    CHECK(e.key('.', ECHO_WORDS) == "hi");

    e.key('a', ECHO_WORDS);
    e.key(LEFT, ECHO_WORDS); // moving away forgets the word
    CHECK(e.key(' ', ECHO_WORDS) == "");
}

TEST_CASE("typing echo: both, and none", "[voice][echo]") {
    TypingEcho e;
    CHECK(e.key('o', ECHO_BOTH) == "o");
    CHECK(e.key('k', ECHO_BOTH) == "k");
    CHECK(e.key(' ', ECHO_BOTH) == "ok");
    e.key('h', ECHO_BOTH);
    CHECK(e.key(',', ECHO_BOTH) == "h comma");

    CHECK(e.key('a', ECHO_NONE) == "");
    CHECK(e.key(' ', ECHO_NONE) == "");
}
