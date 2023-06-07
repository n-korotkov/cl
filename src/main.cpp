#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#define STRING_UTILS_IMPLEMENTATION

#include "bufstream.h"
#include "dictionary.h"
#include "grammeme.h"
#include "pos.h"
#include "string_utils.h"
#include "trie.h"
#include "xmler.h"

Dictionary dict;
int total_words {0};
int total_files {0};
std::unordered_map<std::string, std::vector<WordFormId>> tokenized_files;

void read_word_forms(std::filesystem::path morph_dict_path) {
    static const bool is_merging_link[] {
        true,  true,  true,  true,
        true,  true,  true,  false,
        true,  true,  true,  true,
        false, true,  true,  false,
        false, false, false, false,
        false, true,  true,  false,
        true,  true,  false, true,
    };

    std::ifstream fin(morph_dict_path, std::ios_base::binary);
    Xmler xmler(fin);
    xmler.step_in(); // dictionary
    xmler.step_in(); // grammemes
    xmler.next(); // restrictions
    xmler.next(); // lemmata
    for (bool b {xmler.step_in()}; b; b = xmler.step_out()) { // lemma
        Lexeme lexeme;
        lexeme.id = {std::stoull(xmler.attr("id"))};
        xmler.step_in(); // l
        lexeme.lemma = xmler.attr("t");
        xmler.step_in(); // g
        lexeme.part_of_speech = to_pos(xmler.attr("v"));
        lexeme.grammemes[static_cast<int>(to_grammeme(xmler.attr("v")))] = 1;
        while (xmler.next()) {
            lexeme.grammemes[static_cast<int>(to_grammeme(xmler.attr("v")))] = 1;
        }

        std::vector<WordForm> word_forms;
        for (bool b {xmler.step_out()}; b; b = xmler.step_out()) { // f
            WordForm word_form {
                xmler.attr("t"),
                lexeme.grammemes,
                lexeme.part_of_speech,
                lexeme.id,
            };
            for (bool b {xmler.step_in()}; b; b = xmler.next()) { // g
                word_form.grammemes[static_cast<int>(to_grammeme(xmler.attr("v")))] = 1;
            }
            word_forms.push_back(word_form);
        }
        dict.add_word_forms(word_forms);
        dict.add_lexeme(lexeme);
    }
    xmler.step_out(); // link_types
    xmler.next(); // links
    for (bool b {xmler.step_in()}; b; b = xmler.next()) { // link
        int type {std::stoi(xmler.attr("type"))};
        LexemeId from {std::stoull(xmler.attr("from"))};
        LexemeId to {std::stoull(xmler.attr("to"))};
        if (is_merging_link[type]) {
            dict.get_lexeme(to).parent_ids.insert(from);
        }
    }
    fin.close();
    dict.merge_linked_lexemes();
}

void read_lexeme_frequencies(std::filesystem::path freq_dict_path) {
    std::ifstream fin(freq_dict_path, std::ios_base::binary);
    IBufferedStream in(fin);
    in.ignore('\n');
    while (!in.eof()) {
        std::string lemma {in.getline('\t')};
        std::string pos_string {in.getline('\t')};
        float freq {std::stof(in.getline('\t'))};
        in.ignore('\n');
        in.peek();

        for (LexemeId lexeme_id : dict.find_lexemes_by_lemma(lemma)) {
            Lexeme &lexeme {dict.get_lexeme(lexeme_id)};
            if (lexeme.part_of_speech == to_pos(pos_string)) {
                dict.set_lexeme_frequency(lexeme_id, freq);
            }
        }
    }
    fin.close();
}

std::vector<WordFormId> tokenize(std::istream &fin) {
    std::vector<WordFormId> parsed_word_forms;
    IBufferedStream in(fin);
    while (!in.eof()) {
        std::string word {utf8_get_word(in)};
        if (!word.empty()) {
            word = remove_accent(rus_tolower(word));
            WordFormId id {dict.disambiguate_word_form(word)};
            if (id == dict.npos) {
                dict.add_unknown_word(word);
                id = dict.find_word_forms_by_form(word)[0];
            }
            parsed_word_forms.push_back(id);
        }
    }
    return parsed_word_forms;
}

void tokenize_corpus(std::filesystem::path corpus_path) {
    for (auto &entry : std::filesystem::directory_iterator {corpus_path}) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::ifstream fin(entry.path(), std::ios_base::binary);
        std::string title {entry.path().filename().string()};
        tokenized_files[title] = tokenize(fin);
        total_words += tokenized_files[title].size();
        ++total_files;
        fin.close();
    }
}

#define TIMER_START(id) auto timer_start_##id {std::chrono::high_resolution_clock::now()}
#define TIMER_READ(id) std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(std::chrono::high_resolution_clock::now() - timer_start_##id).count()

extern void task(int argc, char **argv);

int main(int argc, char **argv) {
    TIMER_START(main);

    if (!std::filesystem::exists("dict.bin")) {
        std::cout << "Reading lexemes and word forms...\n";
        read_word_forms(argv[2]);
        std::cout << "Reading lexeme frequencies...\n";
        read_lexeme_frequencies(argv[3]);
    } else {
        std::cout << "dict.bin found, restoring dictionary...\n";
        std::ifstream din("dict.bin", std::ios_base::binary);
        IBufferedStream in(din);
        dict.read_from(in);
        din.close();
    }

    std::cout << "Reading the corpus...\n";
    tokenize_corpus(argv[1]);
    std::cout << "Done in " << TIMER_READ(main) << "s\n\n";

    std::cout << "Files:  " << total_files << "\n";
    std::cout << "Tokens: " << total_words << "\n\n";

    std::cout << "Running " << argv[0] << "...\n";
    TIMER_START(task);
    task(argc, argv);
    std::cout << "Done in " << TIMER_READ(task) << "s\n";

    if (!std::filesystem::exists("dict.bin")) {
        std::cout << "\nSaving dictionary to dict.bin...\n";
        std::ofstream dout("dict.bin", std::ios_base::binary);
        dict.write_to(dout);
        dout.close();
    }

    return 0;
}
