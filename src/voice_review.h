/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Screen-reader style reading for keyboard users, without LVGL or the
 *  synthesizer, so it can be unit tested on the host
 *  (tests/test_voice_review.cpp):
 *
 *  - ReviewText: a review cursor over received text (RTTY now; JS8,
 *    PSK31, Olivia... later), read by character, word or line.
 *  - TypingEcho: what to say as keys are typed - characters, words, both
 *    or none.
 *  - char_name(): a character as it should be spoken ("space", "period").
 *
 *  voice_keys.cpp connects them to the keyboard and the voice.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace voice {

/* One character for speech: letters and digits as themselves ("capital
 * A" if `say_capital`), the rest by name: "space", "new line", "period",
 * "slash"... */
std::string char_name(char c, bool say_capital);

class ReviewText {
  public:
    /* Keeps the last `max_chars` received; lines longer than `wrap` are
     * split at a space, as they would be on screen. */
    explicit ReviewText(size_t max_chars = 8000, size_t wrap = 60);

    void append(const std::string &text);
    void clear();

    const std::string &text() const { return text_; }
    size_t             cursor() const { return cursor_; }
    bool               empty() const { return text_.empty(); }

    /* Each returns what to say. Moving past either end stays there and
     * says "top" or "bottom" first. Until the cursor is first moved (or
     * after clear()), it starts on the newest line. */
    std::string current_char();
    std::string current_word();
    std::string current_line();
    std::string next_char();
    std::string prev_char();
    std::string next_word();
    std::string prev_word();
    std::string next_line();
    std::string prev_line();
    std::string first_line();
    std::string last_line();

    /* From the cursor to the end (at most `max_chars`); moves the cursor
     * to the end of what was read. */
    std::string read_to_end(size_t max_chars = 1000);

  private:
    struct Line {
        size_t start;
        size_t end; /* exclusive, without the line break */
    };

    std::vector<Line> lines() const;
    size_t            line_of(size_t pos, const std::vector<Line> &ls) const;
    void              place();
    std::string       word_at(size_t pos, size_t *start) const;
    std::string       say_line(const Line &l) const;
    std::string       say_char(size_t pos) const;

    std::string text_;
    size_t      cursor_ = 0;
    bool        placed_  = false;
    bool        last_cr_ = false;
    size_t      max_chars_;
    size_t      wrap_;
};

enum EchoMode {
    ECHO_NONE = 0,
    ECHO_CHARS,
    ECHO_WORDS,
    ECHO_BOTH,
};

class TypingEcho {
  public:
    /* A typed key: a character, or one of LVGL's LV_KEY_* codes (passed
     * as is). Returns what to say, "" for nothing. */
    std::string key(uint32_t key, EchoMode mode);

    void reset() { word_.clear(); }

  private:
    std::string word_;
};

} // namespace voice
