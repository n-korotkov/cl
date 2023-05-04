#include <iostream>
#include <istream>
#include <limits>
#include <string_view>
#include <string>
#include <sstream>
#include <unordered_map>

#include "bufstream.h"
#include "string_utils.h"
#include "xmler.h"

Xmler::Xmler(std::istream &inn) : in(inn) {
    inn.ignore(std::numeric_limits<std::streamsize>::max(), '>');
    inn >> std::ws;
};

bool Xmler::step_in(bool skip_text) {
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

bool Xmler::next(bool skip_text) {
    if (_depth == 0 || _type == ElementEnd) {
        return false;
    }
    int d {_type == Element ? 1 : 0};
    while (d > 0) {
        in.ignore('<');
        if (in.peek() == '/') {
            d -= 2;
        }
        in.ignore('>');
        in.seek(-2);
        if (in.get() != '/') {
            ++d;
        }
        in.seek(1);
    }
    if (read_element(skip_text)) {
        return true;
    } else {
        _type = ElementEnd;
        return false;
    }
}

bool Xmler::step_out(bool skip_text) {
    if (_depth == 0) {
        return false;
    }
    while (next()) {}
    in.ignore('>');
    if (--_depth > 0 && read_element(skip_text)) {
        return true;
    } else {
        _type = ElementEnd;
        return false;
    }
}

bool Xmler::end_reached() {
    return _depth == 0 && _type == ElementEnd;
}

bool Xmler::read_element(bool skip_text) {
    if (end_reached()) {
        return false;
    }

    _text = in.getline('<');
    if (!_text.empty() && !skip_text) {
        in.seek(-1);
        _attrs.clear();
        _tag = "";
        _type = Text;
        return true;
    }

    if (in.peek() == '/') {
        in.seek(-1);
        return false;
    } else {
        _attrs.clear();
        std::string attr_s {in.getline('>')};
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
