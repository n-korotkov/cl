#pragma once

#include <string_view>
#include <string>
#include <vector>

template <typename T>
class Trie {
private:
    struct Node {
        enum { Inner, Leaf } type;

        union {
            std::vector<size_t> value_indices;
            Node *child[2];
        };

        std::string key;
        size_t skip;

        Node() : child { nullptr, nullptr } {}

        Node(std::string_view key, size_t skip)
                : type {Inner}, key {key}, skip {skip}, child { nullptr, nullptr } {}

        Node(std::string_view key, size_t skip, size_t index)
                : type {Leaf}, key {key}, skip {skip}, value_indices(1, index) {}

        ~Node() {
            if (type == Inner) {
                delete child[0];
                delete child[1];
            }
        }
    };

public:
    ~Trie();

    T &operator[](size_t index);

    std::pair<size_t, size_t> insert(const std::vector<std::string> &keys, const std::vector<T> &vals);
    size_t insert(std::string_view key, const T &value, size_t min_index = 0);

    void add_keys(const std::vector<std::string> &keys, const std::vector<size_t> &indices);
    void add_key(std::string_view key, size_t index);

    void clear();

    bool contains(std::string_view key);
    std::vector<size_t> find_by_key(std::string_view key);

    size_t index_cap();
    size_t size();

    template <typename U>
    void read_from(U &stream, void (*read_value)(U &, T &));
    void write_to(std::ostream &stream, void (*write_value)(std::ostream &, T &));

private:
    Node *root {nullptr};
    std::vector<T> values;
    size_t key_count {0};

    Node *add_key(std::string_view key, size_t index, Node *t, Node *t_prev);
};

#include "trie.tpp"
