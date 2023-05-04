#pragma once

#include <bitset>
#include <limits>
#include <ostream>
#include <string_view>
#include <string>
#include <unordered_set>
#include <vector>

#include "grammeme.h"
#include "pos.h"
#include "trie.h"

using WordFormId = size_t;
using LexemeId = size_t;

struct WordForm {
    std::string form;
    std::bitset<static_cast<size_t>(Grammeme::Max)> grammemes;
    PoS part_of_speech {PoS::Unknown};
    LexemeId lexeme_id {0};
};

struct Lexeme {
    std::string lemma;
    std::bitset<static_cast<size_t>(Grammeme::Max)> grammemes;
    PoS part_of_speech {PoS::Unknown};
    LexemeId id {0};
    std::unordered_set<LexemeId> parent_ids;
};

class Dictionary {
public:
    static constexpr size_t npos {std::numeric_limits<size_t>::max()};

    WordFormId add_word_form(WordForm word_form);
    std::pair<WordFormId, WordFormId> add_word_forms(const std::vector<WordForm> &word_forms);
    LexemeId add_lexeme(Lexeme lexeme);
    LexemeId add_unknown_word(std::string form);

    WordForm &get_word_form(WordFormId id);
    Lexeme &get_lexeme(LexemeId id);

    std::vector<WordFormId> find_word_forms_by_form(std::string_view form);
    std::vector<LexemeId> find_lexemes_by_lemma(std::string_view lemma);

    std::pair<WordFormId, LexemeId> disambiguate(std::string_view word);
    WordFormId disambiguate_word_form(std::string_view word);
    LexemeId disambiguate_lexeme(std::string_view word);
    LexemeId disambiguate_lexeme(WordFormId word_form_id);

    void merge_linked_lexemes();

    void set_lexeme_frequency(LexemeId id, float freq);

    template <typename T>
    void read_from(T &stream);
    void write_to(std::ostream &stream);

private:
    Trie<WordForm> word_form_tree;
    Trie<Lexeme> lexeme_tree;
    std::unordered_map<LexemeId, float> lexeme_freq;
};

#include "dictionary.tpp"
