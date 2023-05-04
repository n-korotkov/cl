#pragma once

#include <bitset>
#include <ostream>

#include "grammeme.h"
#include "pos.h"
#include "trie.h"

template <typename T>
static void read_word_form(T &stream, WordForm &word_form) {
    std::getline(stream, word_form.form, '\0');
    stream.read(reinterpret_cast<char *>(&word_form.grammemes), sizeof(word_form.grammemes));
    size_t pos_num;
    stream.read(reinterpret_cast<char *>(&pos_num), sizeof(pos_num));
    word_form.part_of_speech = static_cast<PoS>(pos_num);
    stream.read(reinterpret_cast<char *>(&word_form.lexeme_id), sizeof(word_form.lexeme_id));
}

template <typename T>
static void read_lexeme(T &stream, Lexeme &lexeme) {
    std::getline(stream, lexeme.lemma, '\0');
    stream.read(reinterpret_cast<char *>(&lexeme.grammemes), sizeof(lexeme.grammemes));
    size_t pos_num;
    stream.read(reinterpret_cast<char *>(&pos_num), sizeof(pos_num));
    lexeme.part_of_speech = static_cast<PoS>(pos_num);
    stream.read(reinterpret_cast<char *>(&lexeme.id), sizeof(lexeme.id));
    size_t parent_id_count;
    stream.read(reinterpret_cast<char *>(&parent_id_count), sizeof(parent_id_count));
    for (size_t i {0}; i < parent_id_count; ++i) {
        LexemeId id;
        stream.read(reinterpret_cast<char *>(&id), sizeof(id));
        lexeme.parent_ids.insert(id);
    }
}

template <typename T>
void Dictionary::read_from(T &stream) {
    lexeme_freq.clear();
    word_form_tree.read_from(stream, read_word_form);
    lexeme_tree.read_from(stream, read_lexeme);
    size_t freq_count;
    stream.read(reinterpret_cast<char *>(&freq_count), sizeof(freq_count));
    for (size_t i {0}; i < freq_count; ++i) {
        LexemeId id;
        float freq;
        stream.read(reinterpret_cast<char *>(&id), sizeof(id));
        stream.read(reinterpret_cast<char *>(&freq), sizeof(freq));
        lexeme_freq[id] = freq;
    }
}
