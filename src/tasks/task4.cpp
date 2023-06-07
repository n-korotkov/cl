#include <cmath>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "../bufstream.h"
#include "../dictionary.h"
#include "../trie.h"
#include "../xmler.h"

struct Model {

    struct Token {
        LexemeId lexeme_id {0};
        WordFormId word_form_id {0};
        Grammemes grammemes {0};
    };

    std::string name;
    std::vector<Token> tokens;
};

extern Dictionary dict;
extern int total_words;
extern int total_files;
extern std::unordered_map<std::string, std::vector<WordFormId>> tokenized_files;

static std::unordered_map<LexemeId, LexemeId> synonym_group;
static std::vector<Model> models;

static void read_thesaurus(std::ifstream &fin) {
    Xmler xmler(fin);
    xmler.step_in(); // thesaurus
    for (bool b {xmler.step_in()}; b; b = xmler.step_out()) { // synset
        LexemeId synonym_parent {dict.disambiguate_lexeme(xmler.attr("l"))};
        for (bool b {xmler.step_in()}; b; b = xmler.next()) { // synonym
            synonym_group[dict.disambiguate_lexeme(xmler.attr("l"))] = synonym_parent;
        }
    }
}

static void read_models(std::ifstream &fin) {
    Xmler xmler(fin);
    xmler.step_in(); // models
    for (bool b {xmler.step_in()}; b; b = xmler.step_out()) { // model
        Model &model {models.emplace_back()};
        model.name = xmler.attr("name");
        for (bool b {xmler.step_in()}; b;) { // token
            Model::Token &token {model.tokens.emplace_back()};
            if (xmler.attrs().contains("lemma")) {
                token.lexeme_id = dict.disambiguate_lexeme(xmler.attr("lemma"));
            }
            if (xmler.attrs().contains("wordform")) {
                token.word_form_id = dict.disambiguate_word_form(xmler.attr("wordform"));
            }
            if (xmler.step_in()) {
                for (bool b {true}; b; b = xmler.next()) { // g
                    token.grammemes[static_cast<int>(to_grammeme(xmler.attr("v")))] = 1;
                }
                b = xmler.step_out();
            } else {
                b = xmler.next();
            }
        }
    }
}

void task(int argc, char **argv) {
    std::ifstream thesaurus_in(argv[4], std::ios_base::binary);
    std::ifstream models_in(argv[5], std::ios_base::binary);

    read_thesaurus(thesaurus_in);
    read_models(models_in);

    thesaurus_in.close();
    models_in.close();

    std::ofstream fout("models.txt");

    for (Model &model : models) {
        std::unordered_map<std::string, std::vector<size_t>> model_occurences;
        int model_occurences_total {0};
        for (auto &entry : tokenized_files) {
            std::vector<WordFormId> &tokens {entry.second};
            for (size_t i {0}; i < tokens.size() - model.tokens.size(); ++i) {
                bool match {true};
                for (size_t j {0}; j < model.tokens.size(); ++j) {
                    WordForm &word_form {dict.get_word_form(tokens[i + j])};
                    LexemeId lexeme_id {dict.disambiguate_lexeme(tokens[i + j])};
                    if (synonym_group.contains(lexeme_id)) {
                        lexeme_id = synonym_group[lexeme_id];
                    }
                    if ((model.tokens[j].lexeme_id != 0 && lexeme_id != model.tokens[j].lexeme_id && word_form.lexeme_id != model.tokens[j].lexeme_id) ||
                            (model.tokens[j].word_form_id != 0 && tokens[i + j] != model.tokens[j].word_form_id) ||
                            (word_form.grammemes & model.tokens[j].grammemes) != model.tokens[j].grammemes) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    model_occurences[entry.first].push_back(i);
                    ++model_occurences_total;
                }
            }
        }
        float tf {static_cast<float>(model_occurences_total) / total_words};
        float idf {std::log(static_cast<float>(total_files) / model_occurences.size())};
        fout << model.name << ": " << model_occurences_total << " (idf = " << (tf * idf) << ")\n";
        for (auto &model_entry : model_occurences) {
            for (size_t i : model_entry.second) {
                fout << "  ";
                for (size_t j {0}; j < model.tokens.size(); ++j) {
                    fout << dict.get_word_form(tokenized_files[model_entry.first][i + j]).form << " ";
                }
                fout << "\n";
            }
        }
        fout << "\n";
    }

    fout.close();
}
