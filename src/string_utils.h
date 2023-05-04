#pragma once

#include <bit>
#include <istream>
#include <string_view>
#include <vector>

#include "bufstream.h"

struct utf8_char {
    char code_units[4] {0};
};

std::string_view chop_by_delims(std::string_view &sv, const char *delims);
std::string_view prefix_by_delims(std::string_view sv, const char *delims);
void ignore_chars(std::string_view &sv, const char *chars);
std::vector<std::string_view> split_by_delims(std::string_view sv, const char *delims, bool skip_empty = false);

bool utf8_is_punct(const char *b);
int utf8_char_len(const char *b);
int utf8_str_len(std::string_view sv);
std::string_view utf8_common_prefix(std::string_view sv1, std::string_view sv2);

std::string rus_tolower(std::string_view sv);
std::string deyotize(std::string_view sv);
std::string remove_accent(std::string_view sv);

template <typename T>
utf8_char utf8_get(T &stream) {
    utf8_char c;
    stream.get(c.code_units[0]);
    int l {utf8_char_len(c.code_units)};
    stream.read(c.code_units + 1, l - 1);
    return c;
}

template <typename T>
std::string utf8_get_word(T &stream) {
    std::string word;
    utf8_char ch {utf8_get(stream)};
    while (!stream.eof() && (!utf8_is_punct(ch.code_units) || ch.code_units[0] == '-')) {
        word.append(ch.code_units, utf8_char_len(ch.code_units));
        ch = utf8_get(stream);
    }
    return word;
}

template <typename T>
std::string format_num_len(T x, int len) {
    std::string s {std::to_string(x)};
    if (s.size() > len) {
        return "…" + s.substr(s.size() - len);
    } else {
        return std::string(len - s.size(), ' ') + s;
    }
}

std::string format_str_len(std::string_view sv, int len);

#ifdef STRING_UTILS_IMPLEMENTATION

std::string_view chop_by_delims(std::string_view &sv, const char *delims) {
    size_t p {sv.find_first_of(delims)};
    if (p == sv.npos) {
        p = sv.size();
    }
    std::string_view cv(sv.data(), p);
    sv.remove_prefix(std::min(p + 1, sv.size()));
    return cv;
}

std::string_view prefix_by_delims(std::string_view sv, const char *delims) {
    size_t p {sv.find_first_of(delims)};
    if (p == sv.npos) {
        p = sv.size();
    }
    std::string_view cv(sv.data(), p);
    return cv;
}

void ignore_chars(std::string_view &sv, const char *chars) {
    size_t p {sv.find_first_not_of(chars)};
    if (p == sv.npos) {
        p = sv.size();
    }
    sv.remove_prefix(p);
}

std::vector<std::string_view> split_by_delims(std::string_view sv, const char *delims, bool skip_empty) {
    std::vector<std::string_view> parts;
    size_t pc;
    while ((pc = sv.find_first_of(delims)) != sv.npos) {
        parts.emplace_back(sv.data(), pc);
        sv.remove_prefix(pc + 1);
        if (skip_empty) {
            ignore_chars(sv, delims);
        }
    }
    if (!sv.empty()) {
        parts.push_back(sv);
    }
    return parts;
}

bool utf8_is_punct(const char *b) {
    unsigned char x {static_cast<unsigned char>(b[0])};
    if (x <= 0x2F ||
            (0x3A <= x && x <= 0x40) ||
            (0x5B <= x && x <= 0x60) ||
            (0x7B <= x && x <= 0x7F)) {
        return true;
    }
    if (x & 0xC0 != 0xC0) {
        return false;
    }

    unsigned char y {static_cast<unsigned char>(b[1])};
    if (x == 0xC2 ||
            (x == 0xC3 && y == 0x97) ||
            (x == 0xC3 && y == 0xB7)) {
        return true;
    }
    if (x & 0xE0 != 0xE0) {
        return false;
    }

    unsigned char z {static_cast<unsigned char>(b[2])};
    if ((x == 0xE2 && y == 0x80 && z >= 0x80) ||
            (x == 0xE2 && y == 0x81 && z <= 0xB0)) {
        return true;
    }
    return false;
}

int utf8_char_len(const char *b) {
    int l {std::countl_one(static_cast<unsigned char>(b[0]))};
    if (l == 0) l = 1;
    if (l > 4)  l = 0;
    return l;
}

int utf8_str_len(std::string_view sv) {
    int l {0};
    while (!sv.empty()) {
        sv.remove_prefix(utf8_char_len(sv.data()));
        ++l;
    }
    return l;
}

std::string rus_tolower(std::string_view sv) {
    int l {0};
    std::string s {sv};
    while (!sv.empty()) {
        int char_len {utf8_char_len(sv.data())};
        if (char_len == 1) {
            s[l] = std::tolower(sv[0]);
        }
        if (char_len == 2) {
            unsigned char b1 {static_cast<unsigned char>(sv[0])};
            unsigned char b2 {static_cast<unsigned char>(sv[1])};
            int u16b {(static_cast<int>(b1) << 8) | b2};
            if (u16b == 0xD081) {
                s.replace(l, 2, "ё");
            } else if (0xD090 <= u16b && u16b < 0xD0B0) {
                const char *lowercase_letters[] {
                    "а", "б", "в", "г", "д", "е", "ж", "з",
                    "и", "й", "к", "л", "м", "н", "о", "п",
                    "р", "с", "т", "у", "ф", "х", "ц", "ч",
                    "ш", "щ", "ъ", "ы", "ь", "э", "ю", "я",
                };
                s.replace(l, 2, lowercase_letters[u16b - 0xD090]);
            }
        }
        l += char_len;
        sv.remove_prefix(char_len);
    }
    return s;
}

std::string deyotize(std::string_view sv) {
    int l {0};
    std::string s {sv};
    while (sv.size() > 1) {
        int char_len {utf8_char_len(sv.data())};
        if (sv.starts_with("ё")) {
            s.replace(l, 2, "е");
        }
        if (sv.starts_with("Ё")) {
            s.replace(l, 2, "Е");
        }
        l += char_len;
        sv.remove_prefix(char_len);
    }
    return s;
}

std::string remove_accent(std::string_view sv) {
    size_t p;
    std::string s;
    while ((p = sv.find("\u0301")) != sv.npos) {
        s += sv.substr(0, p);
        sv.remove_prefix(p + 2);
    }
    s += sv;
    return s;
}

std::string_view utf8_common_prefix(std::string_view sv1, std::string_view sv2) {
    int i {0};
    while (i < sv1.size() && i < sv2.size()) {
        int l1 {utf8_char_len(sv1.data() + i)};
        int l2 {utf8_char_len(sv2.data() + i)};
        if (sv1.substr(i, l1) == sv2.substr(i, l2)) {
            i += l1;
        } else {
            break;
        }
    }
    return sv1.substr(0, i);
}

std::string format_str_len(std::string_view sv, int len) {
    int l {0};
    std::string s {sv};
    std::string_view svl;
    while (!sv.empty()) {
        sv.remove_prefix(utf8_char_len(sv.data()));
        ++l;
        if (l == len - 1) {
            svl = sv;
        }
    }
    if (l > len) {
        s.resize(s.size() - svl.size());
        s += "…";
    } else {
        s.append(len - l, ' ');
    }
    return s;
}

#endif // STRING_UTILS_IMPLEMENTATION
