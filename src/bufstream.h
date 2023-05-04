#pragma once

#include <cstring>
#include <istream>
#include <string>

class IBufferedStream {
public:
    IBufferedStream(std::istream &in);
    ~IBufferedStream();

    int get();
    void get(char &c);
    int peek();
    std::string getline(char delim = '\n');
    void ignore(char delim);
    void read(char *s, int size);
    void seek(int offset);
    bool eof();

private:
    static constexpr int BufferSize {4096};

    std::istream &in;
    char *buf[2];
    int buf_index {0};
    int pos {0};
    int lim {0};
    bool _eof {false};

    bool ensure();
};

namespace std {
    inline IBufferedStream &getline(IBufferedStream &stream, std::string &s, char delim) {
        s = stream.getline(delim);
        return stream;
    }
}
