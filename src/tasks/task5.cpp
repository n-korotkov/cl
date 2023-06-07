#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../dictionary.h"
#include "../pos.h"
#include "../string_utils.h"
#include "../trie.h"
#include "../xmler.h"

static constexpr float K1 {2};
static constexpr float B {0.75f};

extern Dictionary dict;
extern int total_words;
extern int total_files;
extern std::unordered_map<std::string, std::vector<WordFormId>> tokenized_files;
extern std::vector<WordFormId> tokenize(std::istream &fin);

static std::unordered_map<LexemeId, std::vector<LexemeId>> synsets;

static void read_thesaurus(std::ifstream &fin) {
    Xmler xmler(fin);
    xmler.step_in(); // thesaurus
    for (bool b {xmler.step_in()}; b; b = xmler.step_out()) { // synset
        LexemeId synonym_parent {dict.disambiguate_lexeme(xmler.attr("l"))};
        for (bool b {xmler.step_in()}; b; b = xmler.next()) { // synonym
            synsets[synonym_parent].push_back(dict.disambiguate_lexeme(xmler.attr("l")));
        }
    }
}

void task(int argc, char **argv) {
    std::ifstream thesaurus_in(argv[4], std::ios_base::binary);
    read_thesaurus(thesaurus_in);
    thesaurus_in.close();

    std::string phrase(argv[5]);
    std::istringstream in(phrase);
    std::vector<WordFormId> phrase_tokens {tokenize(in)};
    std::unordered_set<LexemeId> query_lexeme_ids;
    for (WordFormId id : phrase_tokens) {
        LexemeId lexeme_id {dict.disambiguate_lexeme(id)};
        if (!is_function_word(dict.get_word_form(id).part_of_speech)) {
            query_lexeme_ids.insert(lexeme_id);
            if (synsets.contains(lexeme_id)) {
                query_lexeme_ids.insert(synsets[lexeme_id].begin(), synsets[lexeme_id].end());
            }
        }
    }

    std::ofstream fout("search_result.txt");

    std::unordered_map<LexemeId, std::unordered_map<std::string, std::vector<size_t>>> lexeme_occurences;
    std::unordered_map<size_t, size_t> lexeme_occurences_total;
    for (auto &entry : tokenized_files) {
        std::vector<WordFormId> &tokens {entry.second};
        for (size_t i {0}; i < tokens.size(); ++i) {
            LexemeId lexeme_id {dict.disambiguate_lexeme(tokens[i])};
            lexeme_occurences[lexeme_id][entry.first].push_back(i);
            ++lexeme_occurences_total[lexeme_id];
        }
    }

    std::vector<std::string> relevant_files;
    std::unordered_map<std::string, float> bm25_scores;
    int filename_len_max {0};
    float avg_file_size {static_cast<float>(total_words) / total_files};
    for (auto &entry : tokenized_files) {
        std::vector<WordFormId> &tokens {entry.second};
        float bm25 {0};
        for (LexemeId id : query_lexeme_ids) {
            float n {static_cast<float>(total_files - lexeme_occurences[id].size()) + 0.5f};
            float d {static_cast<float>(lexeme_occurences[id].size()) + 0.5f};
            float idf {std::log(n / d)};
            size_t f {lexeme_occurences[id].contains(entry.first) ? lexeme_occurences[id][entry.first].size() : 0};
            bm25 += idf * (f * (K1 + 1)) / (f + K1 * (1 - B + B * tokens.size() / avg_file_size));
        }
        if (bm25 > 0) {
            bm25_scores[entry.first] = bm25;
            relevant_files.push_back(entry.first);
            filename_len_max = std::max(filename_len_max, utf8_str_len(entry.first));
        }
    }

    std::sort(
        relevant_files.begin(),
        relevant_files.end(),
        [bm25_scores] (std::string &f1, std::string &f2) {
            return bm25_scores.at(f1) > bm25_scores.at(f2);
        }
    );

    fout << format_str_len("Document", filename_len_max) << "        Score\n\n";
    for (std::string &f : relevant_files) {
        fout << format_str_len(f, filename_len_max) << " " << format_num_len(bm25_scores[f], 12) << "\n";
    }

    fout.close();
}
