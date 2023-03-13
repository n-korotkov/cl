#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <bitset>

#include <grammeme.h>
#include <pos.h>
#include <string_utils.h>
#include <trie.h>
#include <xmler.h>

struct WordForm {
    std::bitset<static_cast<size_t>(Grammeme::Max)> grammemes;
    PoS part_of_speech {PoS::Unknown};
    int lexeme_index {0};
};

struct Lexeme {
    std::string lemma;
    std::bitset<static_cast<size_t>(Grammeme::Max)> grammemes;
    PoS part_of_speech {PoS::Unknown};
    int occurences {0};
    int texts {0};
};

static std::vector<std::unordered_set<int>> parent_lexemes;
static std::vector<Lexeme> pseudo_lexemes;
static Trie<std::vector<WordForm>> word_form_tree;
static Trie<std::unordered_map<PoS, float>> lexeme_freq_tree;
static std::unordered_map<std::string, int> unknown_word_occurences;

static int lexeme_count {0};
static int total_words {0};
static int ambiguous_words {0};
static int unknown_words {0};
static int total_files {0};

void read_lexeme_frequencies(std::filesystem::path freq_dict_path) {
    std::ifstream fin(freq_dict_path, std::ios_base::binary);
    fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    while (!fin.eof()) {
        std::string lemma;
        std::string pos_string;
        float freq;

        fin >> lemma >> pos_string >> freq;
        fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        fin >> std::ws;

        PoS part_of_speech {to_pos(pos_string)};
        if (freq > lexeme_freq_tree[lemma][part_of_speech]) {
            lexeme_freq_tree[lemma][part_of_speech] = freq;
        }
    }
    fin.close();
}

bool is_merging_link_type(int type) {
    static const bool t[] {
        true,  true,  true,  true,
        true,  true,  true,  false,
        true,  true,  true,  true,
        false, true,  true,  false,
        false, false, false, false,
        false, true,  true,  false,
        true,  true,  false, true,
    };
    return t[type];
}

void read_word_forms(std::filesystem::path morph_dict_path) {
    std::ifstream fin(morph_dict_path, std::ios_base::binary);
    Xmler xmler(fin);
    xmler.step_in(); // dictionary
    xmler.step_in(); // grammemes
    xmler.next(); // restrictions
    xmler.next(); // lemmata
    for (bool b {xmler.step_in()}; b; b = xmler.step_out()) { // lemma
        int lexeme_id {std::stoi(xmler.attr("id"))};
        Lexeme lexeme;
        xmler.step_in(); // l
        lexeme.lemma = xmler.attr("t");
        xmler.step_in(); // g
        lexeme.part_of_speech = to_pos(xmler.attr("v"));
        while (xmler.next()) {
            lexeme.grammemes[static_cast<int>(to_grammeme(xmler.attr("v")))] = 1;
        }

        for (bool b {xmler.step_out()}; b; b = xmler.step_out()) { // f
            WordForm word_form {
                lexeme.grammemes,
                lexeme.part_of_speech,
                lexeme_id,
            };
            std::string form {xmler.attr("t")};
            for (bool b {xmler.step_in()}; b; b = xmler.next()) { // g
                word_form.grammemes[static_cast<int>(to_grammeme(xmler.attr("v")))] = 1;
            }
            word_form_tree[form].push_back(word_form);
            if (form.find("ё") != form.npos || form.find("Ё") != form.npos) {
                word_form_tree[deyotize(form)].push_back(word_form);
            }
        }
        if (pseudo_lexemes.size() < lexeme_id) {
            pseudo_lexemes.resize(lexeme_id);
            parent_lexemes.resize(lexeme_id);
        }
        pseudo_lexemes.push_back(lexeme);
        parent_lexemes.emplace_back();
        ++lexeme_count;
    }
    xmler.step_out(); // link_types
    xmler.next(); // links
    for (bool b {xmler.step_in()}; b; b = xmler.next()) { // link
        int type {std::stoi(xmler.attr("type"))};
        int from {std::stoi(xmler.attr("from"))};
        int to {std::stoi(xmler.attr("to"))};
        if (is_merging_link_type(type)) {
            parent_lexemes[to].insert(from);
        }
    }
    fin.close();
}

void merge_linked_lexemes() {
    for (int i {0}; i < parent_lexemes.size(); ++i) {
        if (!parent_lexemes[i].empty()) {
            --lexeme_count;
            bool parents_exist {true};
            while (parents_exist) {
                parents_exist = false;
                std::unordered_set<int> new_parents;
                for (int parent : parent_lexemes[i]) {
                    if (parent_lexemes[parent].empty()) {
                        new_parents.insert(parent);
                    } else {
                        parents_exist = true;
                        new_parents.insert(
                            parent_lexemes[parent].begin(),
                            parent_lexemes[parent].end());
                    }
                }
                parent_lexemes[i] = new_parents;
            }
        }
    }
}

int find_lexeme_index(std::string_view form) {
    int lexeme_index {0};
    if (word_form_tree.contains(form)) {
        std::unordered_set<int> candidate_lexemes;
        for (WordForm word_form : word_form_tree[form]) {
            if (parent_lexemes[word_form.lexeme_index].empty()) {
                candidate_lexemes.insert(word_form.lexeme_index);
            } else {
                candidate_lexemes.insert(
                    parent_lexemes[word_form.lexeme_index].begin(),
                    parent_lexemes[word_form.lexeme_index].end());
            }
        }

        lexeme_index = *candidate_lexemes.begin();
        if (candidate_lexemes.size() > 1) {
            ++ambiguous_words;
            float freq {0};
            for (int candidate_lexeme : candidate_lexemes) {
                Lexeme lexeme = pseudo_lexemes[candidate_lexeme];
                std::string lemma = deyotize(lexeme.lemma);
                PoS target_part_of_speech {lexeme.part_of_speech};
                if (lexeme_freq_tree.contains(lemma) &&
                        lexeme_freq_tree[lemma].contains(target_part_of_speech) &&
                        lexeme_freq_tree[lemma][target_part_of_speech] > freq) {
                    freq = lexeme_freq_tree[lemma][target_part_of_speech];
                    lexeme_index = candidate_lexeme;
                }
            }
        }
    }
    return lexeme_index;
}

void analyze_corpus(std::filesystem::path corpus_path) {
    std::vector<char> buf;
    for (auto &entry : std::filesystem::directory_iterator {corpus_path}) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::ifstream fin(entry.path(), std::ios_base::binary);
        fin.seekg(0, fin.end);
        buf.resize(1 + fin.tellg());
        fin.seekg(0, fin.beg);
        fin.read(buf.data(), buf.size() - 1);
        fin.close();
        buf.back() = 0;

        char *a {buf.data()}, *b {a};
        std::unordered_set<int> file_lexeme_indices;
        while (true) {
            if (utf8_is_punct(b) && *b != '-') {
                if (a != b) {
                    std::string form {remove_accent(rus_tolower(std::string_view(a, b - a)))};
                    int lexeme_index {find_lexeme_index(form)};
                    if (lexeme_index != 0) {
                        ++pseudo_lexemes[lexeme_index].occurences;
                        file_lexeme_indices.insert(lexeme_index);
                    } else {
                        ++unknown_words;
                        ++unknown_word_occurences[form];
                    }
                    ++total_words;
                }
                if (*b == 0) {
                    break;
                }
                a = b + utf8_char_len(b);
            }
            b += utf8_char_len(b);
        }
        for (int lexeme_index : file_lexeme_indices) {
            ++pseudo_lexemes[lexeme_index].texts;
        }
        ++total_files;
    }
}

void write_lexeme_frequencies(
        std::vector<Lexeme> &lexemes,
        std::ostream &out,
        bool (*filter)(Lexeme) = [] (Lexeme l) { return true; }) {
    out << "Lemma                PoS                   ipm    Texts\n\n";
    for (auto &lexeme : lexemes) {
        if (filter(lexeme)) {
            out << format_str_len(lexeme.lemma, 20) << " "
                << format_str_len(to_string(lexeme.part_of_speech), 8) << " "
                << format_num_len(lexeme.occurences * 1000000.0 / total_words, 16) << " "
                << format_num_len(lexeme.texts, 8) << "\n";
        }
    }
}

void write_unknown_word_occurences(
        std::vector<Lexeme> &lexemes,
        std::ostream &out,
        bool (*filter)(Lexeme) = [] (Lexeme l) { return true; }) {
    out << "Form               Occurences\n\n";
    for (auto &lexeme : lexemes) {
        out << format_str_len(lexeme.lemma, 20) << " " << format_num_len(lexeme.occurences, 8) << "\n";
    }
}

int main(int argc, char **argv) {
    std::cout << "Reading lexemes and word forms...\n";
    read_word_forms(argv[2]);
    std::cout << "Merging linked lexemes...\n";
    merge_linked_lexemes();
    std::cout << "Reading lexeme frequencies...\n";
    read_lexeme_frequencies(argv[3]);
    std::cout << "\n";

    std::cout << "Trie size:   " << word_form_tree.size() << "\n";
    std::cout << "Lexemes:     " << lexeme_count << "\n\n";

    std::cout << "Analyzing the corpus...\n";
    analyze_corpus(argv[1]);
    std::cout << "Done!\n\n";

    std::cout << "Files:       " << total_files << "\n";
    std::cout << "Tokens:      " << total_words << "\n";
    std::cout << "  ambiguous: " << ambiguous_words << " (" << (ambiguous_words * 100 / total_words) << "%)\n";
    std::cout << "  unknown:   " << unknown_words << " (" << (unknown_words * 100 / total_words) << "%)\n";

    auto lexeme_cmp {
        [] (const Lexeme l1, const Lexeme l2) {
            return l1.occurences > l2.occurences;
        }};

    std::vector<Lexeme> lexeme_occ;
    for (Lexeme lexeme : pseudo_lexemes) {
        if (lexeme.occurences > 0) {
            lexeme_occ.push_back(lexeme);
        }
    }

    std::vector<Lexeme> unknown_occ;
    unknown_occ.reserve(unknown_word_occurences.size());
    for (auto unknown_entry : unknown_word_occurences) {
        unknown_occ.emplace_back();
        unknown_occ.back().lemma = unknown_entry.first;
        unknown_occ.back().occurences = unknown_entry.second;
    }

    std::sort(lexeme_occ.begin(), lexeme_occ.end(), lexeme_cmp);
    std::sort(unknown_occ.begin(), unknown_occ.end(), lexeme_cmp);

    std::ofstream freq_fout("freq.txt");
    std::ofstream freq_no_func_fout("freq_no_function_words.txt");
    std::ofstream unknown_fout("freq_unknown.txt");

    write_lexeme_frequencies(lexeme_occ, freq_fout);
    write_lexeme_frequencies(lexeme_occ, freq_no_func_fout,
        [] (Lexeme l) { return !is_function_word(l.part_of_speech); });
    write_unknown_word_occurences(unknown_occ, unknown_fout);

    freq_fout.close();
    freq_no_func_fout.close();
    unknown_fout.close();

    return 0;
}
