#pragma once

#include <bit>
#include <string_view>
#include <string>

size_t first_different_bit(std::string_view sv1, std::string_view sv2, size_t lim) {
    for (int i {0}; i <= lim / 8; ++i) {
        unsigned char d {0};
        if (i < sv1.size()) d ^= sv1[i];
        if (i < sv2.size()) d ^= sv2[i];
        if (i == lim / 8) d &= (1 << lim % 8) - 1;
        if (d != 0) {
            return std::countr_zero(d) + i * 8;
        }
    }
    return SIZE_MAX;
}

template <typename T>
class Trie {
public:
    ~Trie() {
        delete root;
    }

    T &operator[](std::string_view sv) {
        if (root == nullptr) {
            root = new Node(sv, sv.size() * 8 + 8, new T);
            ++leaf_count;
            return *root->value;
        }

        Node *t {root}, *t_prev {nullptr};
        while (true) {
            size_t d {first_different_bit(sv, t->s, t->skip)};
            if (d != SIZE_MAX) {
                std::string_view match {sv.substr(0, (d + 7) / 8)};
                sv.remove_prefix(d / 8);
                Node *new_node = new Node(match, d);
                Node *new_leaf = new Node(sv, (sv.size() + 1) * 8, new T);
                int b {((sv.empty() ? 0 : sv[0]) >> (d % 8)) & 1};
                new_node->child[b] = new_leaf;
                new_node->child[b ^ 1] = t;
                t->s = t->s.substr(d / 8);
                t->skip -= d - (d % 8);

                if (t_prev == nullptr) {
                    root = new_node;
                } else {
                    t_prev->child[t == t_prev->child[0] ? 0 : 1] = new_node;
                }

                ++leaf_count;
                return *new_leaf->value;
            }

            if (t->value != nullptr) {
                return *t->value;
            }

            t_prev = t;
            sv.remove_prefix(t->skip / 8);
            t = t->child[((sv.empty() ? 0 : sv[0]) >> (t->skip % 8)) & 1];
        }
    }

    bool contains(std::string_view sv) {
        if (root == nullptr) {
            return false;
        }

        Node *t {root};
        while (true) {
            size_t d {first_different_bit(sv, t->s, t->skip)};
            if (d != SIZE_MAX) {
                return false;
            }

            if (t->value != nullptr) {
                return true;
            }

            sv.remove_prefix(t->skip / 8);
            t = t->child[((sv.empty() ? 0 : sv[0]) >> (t->skip % 8)) & 1];
        }
    }

    size_t size() {
        return leaf_count;
    }

private:
    struct Node {
        Node *child[2] {nullptr, nullptr};
        T *value {nullptr};
        std::string s;
        size_t skip;

        Node(std::string_view sv, size_t skip, T *val = nullptr) : s {sv}, skip {skip}, value {val} {}

        ~Node() {
            if (value == nullptr) {
                delete child[0];
                delete child[1];
            } else {
                delete value;
            }
        }
    };
    
    Node *root {nullptr};
    size_t leaf_count {0};
};
