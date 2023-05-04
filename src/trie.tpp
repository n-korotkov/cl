#pragma once

#include <bit>
#include <limits>
#include <numeric>
#include <iostream>
#include <string_view>
#include <string>
#include <vector>

static size_t first_different_bit(std::string_view sv1, std::string_view sv2, size_t lim) {
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
Trie<T>::~Trie() {
    delete root;
}

template <typename T>
T &Trie<T>::operator[](size_t index) {
    return values[index];
}

template <typename T>
std::pair<size_t, size_t> Trie<T>::insert(const std::vector<std::string> &keys, const std::vector<T> &vals) {
    std::vector<size_t> indices;
    for (int i {0}; i < keys.size(); ++i) {
        values.push_back(vals[i]);
        indices.push_back(values.size() - 1);
    }
    add_keys(keys, indices);
    return {values.size() - vals.size(), values.size()};
}

template <typename T>
size_t Trie<T>::insert(std::string_view key, const T &value, size_t min_index) {
    if (values.size() < min_index) {
        values.resize(min_index);
    }
    values.push_back(value);
    add_key(key, values.size() - 1);
    return values.size() - 1;
}

template <typename T>
void Trie<T>::add_keys(const std::vector<std::string> &keys, const std::vector<size_t> &indices) {
    if (keys.empty()) {
        return;
    }

    if (root == nullptr) {
        for (int i {0}; i < keys.size(); ++i) {
            add_key(keys[i], indices[i]);
        }
        return;
    }

    size_t prefix_length {keys.front().size() * 8};
    for (int i {1}; i < keys.size(); ++i) {
        std::string_view sv1 {keys[i - 1]};
        std::string_view sv2 {keys[i]};
        size_t lim {std::min(sv1.size(), sv2.size()) * 8};
        prefix_length = std::min(prefix_length, std::min(first_different_bit(sv1, sv2, lim), lim));
    }

    std::string_view first_key {keys.front()};

    Node *t {root}, *t_prev {nullptr};
    while (true) {
        size_t prefix_skip {keys.front().size() - first_key.size()};
        size_t d {first_different_bit(first_key, t->key, t->skip)};
        if (t->skip >= prefix_length - prefix_skip * 8 || d != SIZE_MAX) {
            int ti {t_prev != nullptr && t == t_prev->child[0] ? 0 : 1};
            for (int i {0}; i < keys.size(); ++i) {
                std::string_view key {keys[i]};
                key.remove_prefix(prefix_skip);
                t = (t_prev != nullptr ? t_prev->child[ti] : root);
                add_key(key, indices[i], t, t_prev);
            }
            break;
        }

        t_prev = t;
        first_key.remove_prefix(t->skip / 8);
        t = t->child[((first_key.empty() ? 0 : first_key[0]) >> (t->skip % 8)) & 1];
    }
}

template <typename T>
void Trie<T>::add_key(std::string_view key, size_t index) {
    add_key(key, index, root, nullptr);
}

template <typename T>
void Trie<T>::clear() {
    delete root;
    root = nullptr;
    values.clear();
    key_count = 0;
}

template <typename T>
bool Trie<T>::contains(std::string_view key) {
    return !find_by_key(key).empty();
}

template <typename T>
std::vector<size_t> Trie<T>::find_by_key(std::string_view key) {
    if (root == nullptr) {
        return {};
    }

    Node *t {root};
    while (true) {
        size_t d {first_different_bit(key, t->key, t->skip)};
        if (d != SIZE_MAX) {
            return {};
        }

        if (t->type == Node::Leaf) {
            return t->value_indices;
        }

        key.remove_prefix(t->skip / 8);
        t = t->child[((key.empty() ? 0 : key[0]) >> (t->skip % 8)) & 1];
    }
}

template <typename T>
size_t Trie<T>::index_cap() {
    return values.size();
}

template <typename T>
size_t Trie<T>::size() {
    return key_count;
}

template <typename T>
template <typename U>
void Trie<T>::read_from(U &stream, void (*read_value)(U &, T &)) {
    delete root;
    root = nullptr;

    size_t node_count;
    stream.read(reinterpret_cast<char *>(&node_count), sizeof(node_count));
    key_count = (node_count + 1) / 2;

    std::vector<Node *> node_stack;
    for (size_t i {0}; i < node_count; ++i) {
        Node *node = new Node;
        if (root == nullptr) {
            root = node;
        } else if (node_stack.back()->child[0] == nullptr) {
            node_stack.back()->child[0] = node;
        } else {
            node_stack.back()->child[1] = node;
        }

        stream.read(reinterpret_cast<char *>(&node->skip), sizeof(node->skip));
        std::getline(stream, node->key, '\0');

        char t;
        stream.get(t);
        if (t == 0) {
            node->type = Node::Inner;
            node_stack.push_back(node);
        } else {
            node->type = Node::Leaf;
            size_t index_count;
            stream.read(reinterpret_cast<char *>(&index_count), sizeof(index_count));
            node->value_indices = std::vector<size_t>(index_count);
            stream.read(reinterpret_cast<char *>(node->value_indices.data()), sizeof(size_t) * index_count);
            while (!node_stack.empty() && node_stack.back()->child[1] != nullptr) {
                node = node_stack.back();
                node_stack.pop_back();
            }
        }
    }

    size_t value_count;
    stream.read(reinterpret_cast<char *>(&value_count), sizeof(value_count));
    values.resize(value_count);
    for (T &value : values) {
        read_value(stream, value);
    }
}

template <typename T>
void Trie<T>::write_to(std::ostream &stream, void (*write_value)(std::ostream &, T &)) {
    size_t node_count {key_count * 2 - 1};
    stream.write(reinterpret_cast<char *>(&node_count), sizeof(node_count));
    if (root == nullptr) {
        return;
    }

    std::vector<Node *> node_stack(1, root);
    while (!node_stack.empty()) {
        Node *node {node_stack.back()};
        stream.write(reinterpret_cast<char *>(&node->skip), sizeof(node->skip));
        stream.write(node->key.data(), node->key.size() + 1);
        if (node->type == Node::Inner) {
            stream.put(0);
            node_stack.push_back(node->child[0]);
        } else {
            stream.put(1);
            size_t index_count {node->value_indices.size()};
            stream.write(reinterpret_cast<char *>(&index_count), sizeof(index_count));
            stream.write(reinterpret_cast<char *>(node->value_indices.data()), sizeof(size_t) * index_count);
            node_stack.pop_back();
            while (!node_stack.empty() && node == node_stack.back()->child[1]) {
                node = node_stack.back();
                node_stack.pop_back();
            }
            if (!node_stack.empty()) {
                node_stack.push_back(node_stack.back()->child[1]);
            }
        }
    }

    size_t value_count {values.size()};
    stream.write(reinterpret_cast<char *>(&value_count), sizeof(value_count));
    for (T &value : values) {
        write_value(stream, value);
    }
}

template <typename T>
Trie<T>::Node *Trie<T>::add_key(std::string_view key, size_t index, Node *t, Node *t_prev) {
    if (root == nullptr) {
        root = new Node(key, key.size() * 8 + 8, index);
        ++key_count;
        return root;
    }

    while (true) {
        size_t d {first_different_bit(key, t->key, t->skip)};
        if (d != SIZE_MAX) {
            std::string_view match {key.substr(0, (d + 7) / 8)};
            key.remove_prefix(d / 8);
            Node *new_node = new Node(match, d);
            Node *new_leaf = new Node(key, (key.size() + 1) * 8, index);
            int b {((key.empty() ? 0 : key[0]) >> (d % 8)) & 1};
            new_node->child[b] = new_leaf;
            new_node->child[b ^ 1] = t;
            t->key = t->key.substr(d / 8);
            t->skip -= d - (d % 8);

            if (t_prev == nullptr) {
                root = new_node;
            } else {
                t_prev->child[t == t_prev->child[0] ? 0 : 1] = new_node;
            }

            ++key_count;
            return new_leaf;
        }

        if (t->type == Node::Leaf) {
            t->value_indices.push_back(index);
            return t;
        }

        t_prev = t;
        key.remove_prefix(t->skip / 8);
        t = t->child[((key.empty() ? 0 : key[0]) >> (t->skip % 8)) & 1];
    }
}
