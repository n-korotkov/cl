#include <bitset>
#include <string_view>
#include <string>
#include <unordered_set>
#include <vector>
#include <iostream>

#include "bufstream.h"
#include "dictionary.h"
#include "grammeme.h"
#include "pos.h"
#include "string_utils.h"
#include "trie.h"

void Dictionary::merge_linked_lexemes() {
    for (int i {1}; i < lexeme_tree.index_cap(); ++i) {
        Lexeme &lexeme {lexeme_tree[i]};
        if (!lexeme.parent_ids.empty()) {
            bool parents_exist {true};
            while (parents_exist) {
                parents_exist = false;
                std::unordered_set<LexemeId> new_parents;
                for (int parent_index : lexeme.parent_ids) {
                    Lexeme &parent {lexeme_tree[parent_index]};
                    if (parent.parent_ids.empty()) {
                        new_parents.insert(parent_index);
                    } else {
                        parents_exist = true;
                        new_parents.insert(parent.parent_ids.begin(), parent.parent_ids.end());
                    }
                }
                lexeme.parent_ids = new_parents;
            }
        }
    }
}

LexemeId Dictionary::add_lexeme(Lexeme lexeme) {
    LexemeId id {lexeme_tree.insert(lexeme.lemma, lexeme, lexeme.id)};
    if (lexeme.lemma.find("ё") != lexeme.lemma.npos || lexeme.lemma.find("Ё") != lexeme.lemma.npos) {
        lexeme_tree.add_key(deyotize(lexeme.lemma), id);
    }
    return id;
}

WordFormId Dictionary::add_word_form(WordForm word_form) {
    WordFormId id {word_form_tree.insert(word_form.form, word_form)};
    if (word_form.form.find("ё") != word_form.form.npos || word_form.form.find("Ё") != word_form.form.npos) {
        word_form_tree.add_key(deyotize(word_form.form), id);
    }
    return id;
}

std::pair<WordFormId, WordFormId> Dictionary::add_word_forms(const std::vector<WordForm> &word_forms) {
    std::vector<std::string> forms;
    for (const WordForm &word_form : word_forms) {
        forms.push_back(word_form.form);
    }
    auto id_bounds {word_form_tree.insert(forms, word_forms)};

    std::vector<std::string> deyotized_forms;
    std::vector<WordFormId> ids;
    for (WordFormId id {id_bounds.first}; id < id_bounds.second; ++id) {
        std::string_view form {word_forms[id - id_bounds.first].form};
        if (form.find("ё") != form.npos || form.find("Ё") != form.npos) {
            deyotized_forms.push_back(deyotize(form));
            ids.push_back(id);
        }
    }
    word_form_tree.add_keys(deyotized_forms, ids);
    return id_bounds;
}

LexemeId Dictionary::add_unknown_word(std::string form) {
    Lexeme lexeme {form};
    lexeme.grammemes[static_cast<int>(Grammeme::Unknown)] = true;
    lexeme.id = lexeme_tree.insert(lexeme.lemma, lexeme);

    WordForm unknown_word_form {form};
    unknown_word_form.lexeme_id = lexeme.id;
    add_word_form(unknown_word_form);
    return lexeme.id;
}

std::vector<WordFormId> Dictionary::find_word_forms_by_form(std::string_view form) {
    return word_form_tree.find_by_key(form);
}

std::vector<LexemeId> Dictionary::find_lexemes_by_lemma(std::string_view lemma) {
    return lexeme_tree.find_by_key(lemma);
}

WordForm &Dictionary::get_word_form(WordFormId id) {
    return word_form_tree[id];
}

Lexeme &Dictionary::get_lexeme(LexemeId id) {
    return lexeme_tree[id];
}

WordFormId Dictionary::disambiguate_word_form(std::string_view word) {
    return disambiguate(word).first;
}

LexemeId Dictionary::disambiguate_lexeme(std::string_view word) {
    return disambiguate(word).second;
}

LexemeId Dictionary::disambiguate_lexeme(WordFormId word_form_id) {
    LexemeId id {get_word_form(word_form_id).lexeme_id};
    float freq {-1};
    for (LexemeId parent_id : get_lexeme(id).parent_ids) {
        if (freq < lexeme_freq[parent_id]) {
            id = parent_id;
            freq = lexeme_freq[id];
        }
    }
    return id;
}

std::pair<WordFormId, LexemeId> Dictionary::disambiguate(std::string_view word) {
    std::pair<WordFormId, LexemeId> ids {npos, npos};
    float freq {-1};
    for (WordFormId word_form_id : word_form_tree.find_by_key(word)) {
        LexemeId lexeme_id {disambiguate_lexeme(word_form_id)};
        if (freq < lexeme_freq[lexeme_id]) {
            ids = std::make_pair(word_form_id, lexeme_id);
            freq = lexeme_freq[lexeme_id];
        }
    }
    return ids;
}

void Dictionary::set_lexeme_frequency(LexemeId id, float freq) {
    lexeme_freq[id] = freq;
}

static void write_word_form(std::ostream &stream, WordForm &word_form) {
    stream.write(word_form.form.data(), word_form.form.size() + 1);
    stream.write(reinterpret_cast<char *>(&word_form.grammemes), sizeof(word_form.grammemes));
    size_t pos_num {static_cast<size_t>(word_form.part_of_speech)};
    stream.write(reinterpret_cast<char *>(&pos_num), sizeof(pos_num));
    stream.write(reinterpret_cast<char *>(&word_form.lexeme_id), sizeof(word_form.lexeme_id));
}

static void write_lexeme(std::ostream &stream, Lexeme &lexeme) {
    stream.write(lexeme.lemma.data(), lexeme.lemma.size() + 1);
    stream.write(reinterpret_cast<char *>(&lexeme.grammemes), sizeof(lexeme.grammemes));
    size_t pos_num {static_cast<size_t>(lexeme.part_of_speech)};
    stream.write(reinterpret_cast<char *>(&pos_num), sizeof(pos_num));
    stream.write(reinterpret_cast<char *>(&lexeme.id), sizeof(lexeme.id));
    size_t parent_id_count {lexeme.parent_ids.size()};
    stream.write(reinterpret_cast<char *>(&parent_id_count), sizeof(parent_id_count));
    for (LexemeId id : lexeme.parent_ids) {
        stream.write(reinterpret_cast<char *>(&id), sizeof(id));
    }
}

void Dictionary::write_to(std::ostream &stream) {
    word_form_tree.write_to(stream, write_word_form);
    lexeme_tree.write_to(stream, write_lexeme);
    size_t freq_count {lexeme_freq.size()};
    stream.write(reinterpret_cast<char *>(&freq_count), sizeof(freq_count));
    for (auto &entry : lexeme_freq) {
        LexemeId id {entry.first};
        float freq {entry.second};
        stream.write(reinterpret_cast<char *>(&id), sizeof(id));
        stream.write(reinterpret_cast<char *>(&freq), sizeof(freq));
    }
}
