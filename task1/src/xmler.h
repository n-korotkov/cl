#pragma once

#include <istream>
#include <limits>
#include <string_view>
#include <string>
#include <unordered_map>

#include "string_utils.h"

class Xmler {
public:
    enum ContentType {
        Text,
        Element,
        EmptyElement,
        ElementEnd,
    };

    Xmler(std::istream &in) : in(in) {
        in.ignore(std::numeric_limits<std::streamsize>::max(), '>');
        in >> std::ws;
    };

    bool step_in(bool skip_text = true) {
        if (_type == Element) {
            ++_depth;
            if (read_element(skip_text)) {
                return true;
            } else {
                _type = ElementEnd;
                return false;
            }
        } else {
            return false;
        }
    }

    bool next(bool skip_text = true) {
        if (_depth == 0 || _type == ElementEnd) {
            return false;
        }
        int d {_type == Element ? 1 : 0};
        while (d > 0) {
            in.ignore(std::numeric_limits<std::streamsize>::max(), '<');
            if (in.peek() == '/') {
                d -= 2;
            }
            in.ignore(std::numeric_limits<std::streamsize>::max(), '>');
            in.seekg(-2, in.cur);
            if (in.get() != '/') {
                ++d;
            }
            in.ignore(1);
        }
        if (read_element(skip_text)) {
            return true;
        } else {
            _type = ElementEnd;
            return false;
        }
    }

    bool step_out(bool skip_text = true) {
        if (_depth == 0) {
            return false;
        }
        while (next()) {}
        in.ignore(std::numeric_limits<std::streamsize>::max(), '>');
        if (--_depth > 0 && read_element(skip_text)) {
            return true;
        } else {
            _type = ElementEnd;
            return false;
        }
    }

    bool end_reached() {
        return _depth == 0 && _type == ElementEnd;
    }

    std::string attr(std::string key) {
        return _attrs.at(key);
    }

    std::unordered_map<std::string, std::string> attrs() { return _attrs; }
    std::string tag() { return _tag; }
    std::string text() { return _text; }
    ContentType type() { return _type; }
    int depth() { return _depth; }

private:
    std::istream &in;
    std::unordered_map<std::string, std::string> _attrs;
    std::string _tag;
    std::string _text;
    ContentType _type {Element};
    int _depth {-1};

    bool read_element(bool skip_text) {
        if (end_reached()) {
            return false;
        }

        std::getline(in, _text, '<');
        if (!_text.empty() && !skip_text) {
            in.seekg(-1, in.cur);
            _attrs.clear();
            _tag = "";
            _type = Text;
            return true;
        }

        if (in.peek() == '/') {
            in.seekg(-1, in.cur);
            return false;
        } else {
            _attrs.clear();
            std::string attr_s;
            std::getline(in, attr_s, '>');
            std::string_view attr_sv {attr_s};

            _type = Element;
            if (attr_sv.ends_with('/')) {
                attr_sv.remove_suffix(1);
                _type = EmptyElement;
            }

            _tag = chop_by_delims(attr_sv, " \t\n\r");
            ignore_chars(attr_sv, " \t\n\r");
            while (!attr_sv.empty()) {
                std::string key {chop_by_delims(attr_sv, "= \t\n\r")};
                attr_sv.remove_prefix(attr_sv.find('\"') + 1);
                std::string val {chop_by_delims(attr_sv, "\"")};
                ignore_chars(attr_sv, " \t\n\r");
                _attrs[key] = val;
            }

            return true;
        }
    }
};
