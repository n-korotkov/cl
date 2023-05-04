#include <cstring>
#include <istream>
#include <string>

#include "bufstream.h"

IBufferedStream::IBufferedStream(std::istream &in) : in{in} {
    char *c {new char[2 * BufferSize]};
    buf[0] = c;
    buf[1] = c + BufferSize;
};

IBufferedStream::~IBufferedStream() {
    delete[] buf[0];
}

int IBufferedStream::get() {
    if (!ensure()) {
        return std::char_traits<char>::eof();
    }
    if (pos < 0) {
        return buf[(buf_index + 1) % 2][pos++ + BufferSize];
    }
    return buf[buf_index][pos++];
}

void IBufferedStream::get(char &c) {
    int r {get()};
    if (r != std::char_traits<char>::eof()) {
        c = r;
    }
}

int IBufferedStream::peek() {
    if (!ensure()) {
        return std::char_traits<char>::eof();
    }
    if (pos < 0) {
        return buf[(buf_index + 1) % 2][pos + BufferSize];
    }
    return buf[buf_index][pos];
}

void IBufferedStream::seek(int offset) {
    if (offset < 0) {
        pos = std::max(pos + offset, -static_cast<int>(BufferSize));
    }
    while (offset > 0 && ensure()) {
        int add {std::min(offset, lim - pos)};
        pos += add;
        offset -= add;
    }
}

void IBufferedStream::ignore(char delim) {
    if (pos < 0) {
        for (int i {pos}; i < 0; ++i) {
            if (buf[(buf_index + 1) % 2][i + BufferSize] == delim) {
                pos = i + 1;
                return;
            }
        }
        pos = 0;
    }

    while (ensure()) {
        for (int i {pos}; i < lim; ++i) {
            if (buf[buf_index][i] == delim) {
                pos = i + 1;
                return;
            }
        }
        pos = lim;
    }
}

void IBufferedStream::read(char *s, int size) {
    if (pos < 0) {
        int l {std::min(-pos, size)};
        memcpy(s, buf[(buf_index + 1) % 2] + pos + BufferSize, l);
        s += l;
        size -= l;
        pos += l;
    }

    while (size > 0 && ensure()) {
        int l {std::min(lim - pos, size)};
        memcpy(s, buf[buf_index] + pos, l);
        s += l;
        size -= l;
        pos += l;
    }
}

std::string IBufferedStream::getline(char delim) {
    std::string s;
    if (pos < 0) {
        for (int i {pos}; i < 0; ++i) {
            char *c {&buf[(buf_index + 1) % 2][i + BufferSize]};
            if (*c == delim) {
                s = std::string(c, i - pos);
                pos = i + 1;
                return s;
            }
        }
        s = std::string(buf[(buf_index + 1) % 2] + pos + BufferSize, -pos);
        pos = 0;
    }

    while (ensure()) {
        for (int i {pos}; i < lim; ++i) {
            if (buf[buf_index][i] == delim) {
                s.append(buf[buf_index] + pos, i - pos);
                pos = i + 1;
                return s;
            }
        }
        s.append(buf[buf_index] + pos, lim - pos);
        pos = lim;
    }

    return s;
}

bool IBufferedStream::eof() {
    return _eof;
}

bool IBufferedStream::ensure() {
    if (pos == lim && !in.eof()) {
        buf_index = (buf_index + 1) % 2;
        in.read(buf[buf_index], BufferSize);
        pos = 0;
        lim = in.gcount();
    }
    if (pos == lim) {
        _eof = true;
        return false;
    }
    return true;
}
