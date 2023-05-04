#pragma once

#include <istream>
#include <string>
#include <unordered_map>

#include "bufstream.h"

class Xmler {
public:
    enum ContentType {
        Text,
        Element,
        EmptyElement,
        ElementEnd,
    };

    Xmler(std::istream &in);

    bool step_in(bool skip_text = true);
    bool next(bool skip_text = true);
    bool step_out(bool skip_text = true);

    std::string attr(std::string key) { return _attrs.at(key); };
    std::unordered_map<std::string, std::string> attrs() { return _attrs; };
    std::string tag() { return _tag; };
    std::string text() { return _text; };
    ContentType type() { return _type; };

    int depth() { return _depth; };
    bool end_reached();

private:
    IBufferedStream in;
    std::unordered_map<std::string, std::string> _attrs;
    std::string _tag;
    std::string _text;
    ContentType _type {Element};
    int _depth {-1};

    bool read_element(bool skip_text);
};
