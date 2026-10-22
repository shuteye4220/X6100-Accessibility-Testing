/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Screen-reader style reading for keyboard users. See voice_review.h.
 */

#include "voice_review.h"

#include "voice_text.h"

#include <cctype>

namespace voice {

/* LVGL's key codes (lv_group.h), so this compiles without LVGL. */
enum : uint32_t {
    KEY_UP        = 17,
    KEY_DOWN      = 18,
    KEY_RIGHT     = 19,
    KEY_LEFT      = 20,
    KEY_ESC       = 27,
    KEY_DEL       = 127,
    KEY_BACKSPACE = 8,
    KEY_ENTER     = 10,
    KEY_NEXT      = 9,
    KEY_PREV      = 11,
    KEY_HOME      = 2,
    KEY_END       = 3,
};

std::string char_name(char c, bool say_capital) {
    switch (c) {
        case ' ':  return "space";
        case '\n':
        case '\r': return "new line";
        case '\t': return "tab";
        case '.':  return "period";
        case ',':  return "comma";
        case '?':  return "question";
        case '!':  return "bang";
        case ':':  return "colon";
        case ';':  return "semicolon";
        case '/':  return "slash";
        case '\\': return "backslash";
        case '-':  return "dash";
        case '_':  return "underline";
        case '+':  return "plus";
        case '=':  return "equals";
        case '@':  return "at";
        case '#':  return "number";
        case '$':  return "dollar";
        case '%':  return "percent";
        case '&':  return "and";
        case '*':  return "star";
        case '(':  return "left paren";
        case ')':  return "right paren";
        case '[':  return "left bracket";
        case ']':  return "right bracket";
        case '{':  return "left brace";
        case '}':  return "right brace";
        case '<':  return "less";
        case '>':  return "greater";
        case '\'': return "tick";
        case '"':  return "quote";
        case '`':  return "grave";
        case '^':  return "caret";
        case '~':  return "tilde";
        case '|':  return "bar";
    }

    unsigned char u = (unsigned char)c;

    if (isupper(u)) {
        return say_capital ? std::string("capital ") + c : std::string(1, c);
    }
    if (isalnum(u)) {
        return std::string(1, c);
    }
    return "symbol";
}

/***** ReviewText *****/

ReviewText::ReviewText(size_t max_chars, size_t wrap) : max_chars_(max_chars), wrap_(wrap ? wrap : 60) {}

void ReviewText::append(const std::string &text) {
    for (char c : text) {
        unsigned char u = (unsigned char)c;

        if (c == '\r' || c == '\n') {
            /* "\r\n" (and RTTY's "\r\r\n") is one line break; at most
             * one blank line in a row is kept. */
            bool crlf = c == '\n' && last_cr_;

            last_cr_ = c == '\r';
            if (crlf) {
                continue;
            }
            size_t n = text_.size();
            if (!(n >= 2 && text_[n - 1] == '\n' && text_[n - 2] == '\n')) {
                text_ += '\n';
            }
            continue;
        }

        last_cr_ = false;
        if (u >= 0x20 && u < 0x7f) {
            text_ += c;
        }
    }

    if (text_.size() > max_chars_) {
        size_t cut = text_.size() - max_chars_;

        text_.erase(0, cut);
        cursor_ = cursor_ > cut ? cursor_ - cut : 0;
    }
}

void ReviewText::clear() {
    text_.clear();
    cursor_  = 0;
    placed_  = false;
    last_cr_ = false;
}

std::vector<ReviewText::Line> ReviewText::lines() const {
    std::vector<Line> out;
    size_t            pos = 0;

    while (pos <= text_.size()) {
        size_t nl  = text_.find('\n', pos);
        size_t end = nl == std::string::npos ? text_.size() : nl;

        /* Soft wrap at a space, as on screen. */
        size_t s = pos;
        while (end - s > wrap_) {
            size_t brk = text_.rfind(' ', s + wrap_);

            if (brk == std::string::npos || brk <= s) {
                brk = s + wrap_;
                out.push_back({s, brk});
                s = brk;
            } else {
                out.push_back({s, brk});
                s = brk + 1;
            }
        }
        out.push_back({s, end});

        if (nl == std::string::npos) {
            break;
        }
        pos = nl + 1;
    }

    /* A trailing break leaves an empty last line: not a line of text. */
    if (out.size() > 1 && out.back().start == out.back().end && out.back().start == text_.size()) {
        out.pop_back();
    }
    return out;
}

size_t ReviewText::line_of(size_t pos, const std::vector<Line> &ls) const {
    for (size_t i = 0; i < ls.size(); i++) {
        size_t next = i + 1 < ls.size() ? ls[i + 1].start : SIZE_MAX;

        if (pos < next) {
            return i;
        }
    }
    return ls.empty() ? 0 : ls.size() - 1;
}

/* Before the cursor is first used, it sits on the newest line. */
void ReviewText::place() {
    if (placed_) {
        return;
    }
    placed_ = true;

    std::vector<Line> ls = lines();
    cursor_              = ls.empty() ? 0 : ls.back().start;
}

std::string ReviewText::say_char(size_t pos) const {
    if (pos >= text_.size()) {
        return "blank";
    }
    /* Received text is mostly capitals: no "capital" before each. */
    return char_name(text_[pos], false);
}

std::string ReviewText::say_line(const Line &l) const {
    std::string spoken = decoded_text(text_.substr(l.start, l.end - l.start));
    return spoken.empty() ? "blank" : spoken;
}

static bool is_space(char c) {
    return c == ' ' || c == '\n';
}

std::string ReviewText::word_at(size_t pos, size_t *start) const {
    if (text_.empty()) {
        return "";
    }
    if (pos >= text_.size()) {
        pos = text_.size() - 1;
    }

    size_t s = pos;
    while (s > 0 && !is_space(text_[s - 1])) {
        s--;
    }
    size_t e = pos;
    while (e < text_.size() && !is_space(text_[e])) {
        e++;
    }
    if (start) {
        *start = s;
    }
    return text_.substr(s, e - s);
}

static std::string with_edge(const char *edge, const std::string &what) {
    return std::string(edge) + ", " + what;
}

std::string ReviewText::current_char() {
    if (empty()) {
        return "No text";
    }
    place();
    return say_char(cursor_);
}

std::string ReviewText::next_char() {
    if (empty()) {
        return "No text";
    }
    place();
    if (cursor_ + 1 >= text_.size()) {
        cursor_ = text_.size() - 1;
        return with_edge("bottom", say_char(cursor_));
    }
    cursor_++;
    return say_char(cursor_);
}

std::string ReviewText::prev_char() {
    if (empty()) {
        return "No text";
    }
    place();
    if (cursor_ == 0) {
        return with_edge("top", say_char(0));
    }
    cursor_--;
    return say_char(cursor_);
}

std::string ReviewText::current_word() {
    if (empty()) {
        return "No text";
    }
    place();

    std::string w = word_at(cursor_, nullptr);
    return w.empty() ? "blank" : decoded_text(w);
}

std::string ReviewText::next_word() {
    if (empty()) {
        return "No text";
    }
    place();

    /* Past the end of this word, then past the spaces after it. */
    size_t p = cursor_;
    while (p < text_.size() && !is_space(text_[p])) {
        p++;
    }
    while (p < text_.size() && is_space(text_[p])) {
        p++;
    }
    if (p >= text_.size()) {
        return with_edge("bottom", current_word());
    }
    cursor_ = p;
    return decoded_text(word_at(p, nullptr));
}

std::string ReviewText::prev_word() {
    if (empty()) {
        return "No text";
    }
    place();

    size_t start;
    word_at(cursor_, &start);

    /* Back over the spaces before this word, to the word before it. */
    size_t p = start;
    while (p > 0 && is_space(text_[p - 1])) {
        p--;
    }
    if (p == 0) {
        cursor_ = start;
        return with_edge("top", current_word());
    }
    word_at(p - 1, &start);
    cursor_ = start;
    return decoded_text(word_at(start, nullptr));
}

std::string ReviewText::current_line() {
    if (empty()) {
        return "No text";
    }
    place();

    std::vector<Line> ls = lines();
    return say_line(ls[line_of(cursor_, ls)]);
}

std::string ReviewText::next_line() {
    if (empty()) {
        return "No text";
    }
    place();

    std::vector<Line> ls = lines();
    size_t            i  = line_of(cursor_, ls);

    if (i + 1 >= ls.size()) {
        cursor_ = ls[i].start;
        return with_edge("bottom", say_line(ls[i]));
    }
    cursor_ = ls[i + 1].start;
    return say_line(ls[i + 1]);
}

std::string ReviewText::prev_line() {
    if (empty()) {
        return "No text";
    }
    place();

    std::vector<Line> ls = lines();
    size_t            i  = line_of(cursor_, ls);

    if (i == 0) {
        cursor_ = ls[0].start;
        return with_edge("top", say_line(ls[0]));
    }
    cursor_ = ls[i - 1].start;
    return say_line(ls[i - 1]);
}

std::string ReviewText::first_line() {
    if (empty()) {
        return "No text";
    }
    placed_ = true;

    std::vector<Line> ls = lines();
    cursor_              = ls[0].start;
    return with_edge("top", say_line(ls[0]));
}

std::string ReviewText::last_line() {
    if (empty()) {
        return "No text";
    }
    placed_ = true;

    std::vector<Line> ls = lines();
    cursor_              = ls.back().start;
    return with_edge("bottom", say_line(ls.back()));
}

std::string ReviewText::read_to_end(size_t max_chars) {
    if (empty()) {
        return "No text";
    }
    place();

    size_t      n    = std::min(max_chars, text_.size() - cursor_);
    std::string part = text_.substr(cursor_, n);

    cursor_ = cursor_ + n >= text_.size() ? text_.size() - 1 : cursor_ + n;

    std::string spoken = decoded_text(part);
    return spoken.empty() ? "blank" : spoken;
}

/***** TypingEcho *****/

static bool is_word_char(uint32_t k) {
    return k < 0x80 && (isalnum((int)k) || k == '\'' || k == '/');
}

std::string TypingEcho::key(uint32_t k, EchoMode mode) {
    bool chars = mode == ECHO_CHARS || mode == ECHO_BOTH;
    bool words = mode == ECHO_WORDS || mode == ECHO_BOTH;

    if (k == KEY_BACKSPACE) {
        if (!word_.empty()) {
            word_.pop_back();
        }
        return "";
    }

    /* A printable character */
    if (k >= 0x20 && k < 0x7f) {
        char c = (char)k;

        if (is_word_char(k)) {
            word_ += c;
            return chars ? char_name(c, true) : "";
        }

        /* A space or punctuation ends the word. */
        std::string out;

        if (words && !word_.empty()) {
            out = decoded_text(word_);
        }
        if (chars && !(words && !word_.empty() && c == ' ')) {
            out += (out.empty() ? "" : " ") + char_name(c, true);
        }
        word_.clear();
        return out;
    }

    /* Enter ends the word too; other keys (arrows...) just forget it. */
    std::string out;

    if (k == KEY_ENTER && words && !word_.empty()) {
        out = decoded_text(word_);
    }
    word_.clear();
    return out;
}

} // namespace voice
