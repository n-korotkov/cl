#include <algorithm>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../dictionary.h"
#include "../string_utils.h"

extern Dictionary dict;
extern int total_words;
extern int total_files;
extern std::unordered_map<std::string, std::vector<WordFormId>> tokenized_files;

static std::unordered_map<int, int> lexeme_occurences;
static std::unordered_map<int, int> lexeme_texts;

static bool all_lexeme_filter(const Lexeme &id) { return true; }
static bool lexeme_cmp(LexemeId id1, LexemeId id2) {
    int occ1 {lexeme_occurences[id1]};
    int occ2 {lexeme_occurences[id2]};
    std::string &lemma1 {dict.get_lexeme(id1).lemma};
    std::string &lemma2 {dict.get_lexeme(id2).lemma};
    PoS pos1 {dict.get_lexeme(id1).part_of_speech};
    PoS pos2 {dict.get_lexeme(id2).part_of_speech};
    return occ1 > occ2 || (occ1 == occ2 && (lemma1 < lemma2 || (lemma1 == lemma2 && (pos1 < pos2))));
};

static void write_lexeme_frequencies(std::vector<LexemeId> &lexeme_ids,
                                     std::ostream &out,
                                     std::function<bool (const Lexeme &)> filter = all_lexeme_filter) {
    out << "Lemma                PoS                   ipm    Texts\n\n";
    for (LexemeId id : lexeme_ids) {
        Lexeme &lexeme {dict.get_lexeme(id)};
        if (filter(lexeme)) {
            out << format_str_len(lexeme.lemma, 20) << " "
                << format_str_len(to_string(lexeme.part_of_speech), 8) << " "
                << format_num_len(lexeme_occurences[id] * 1000000.0 / total_words, 16) << " "
                << format_num_len(lexeme_texts[id], 8) << "\n";
        }
    }
}

static void write_lexeme_occurences(std::vector<LexemeId> &lexeme_ids,
                                    std::ostream &out,
                                    std::function<bool (const Lexeme &)> filter = all_lexeme_filter) {
    out << "Form               Occurences\n\n";
    for (LexemeId id : lexeme_ids) {
        Lexeme &lexeme {dict.get_lexeme(id)};
        if (filter(lexeme)) {
            out << format_str_len(lexeme.lemma, 20) << " "
                << format_num_len(lexeme_occurences[id], 8) << "\n";
        }
    }
}

void task(int argc, char **argv) {
    for (auto &entry : tokenized_files) {
        std::unordered_set<LexemeId> file_lexeme_ids;
        for (WordFormId word_form_id : entry.second) {
            LexemeId lexeme_id {dict.disambiguate_lexeme(word_form_id)};
            ++lexeme_occurences[lexeme_id];
            file_lexeme_ids.insert(lexeme_id);
        }
        for (LexemeId lexeme_id : file_lexeme_ids) {
            ++lexeme_texts[lexeme_id];
        }
    }

    std::vector<LexemeId> lexeme_occ;
    for (auto lexeme_entry : lexeme_occurences) {
        lexeme_occ.push_back(lexeme_entry.first);
    }
    std::sort(lexeme_occ.begin(), lexeme_occ.end(), lexeme_cmp);

    std::ofstream freq_fout("freq.txt");
    std::ofstream freq_no_func_fout("freq_no_function_words.txt");
    std::ofstream unknown_fout("freq_unknown.txt");

    write_lexeme_frequencies(lexeme_occ, freq_fout,
        [] (const Lexeme &l) { return l.part_of_speech != PoS::Unknown; });
    write_lexeme_frequencies(lexeme_occ, freq_no_func_fout,
        [] (const Lexeme &l) { return l.part_of_speech != PoS::Unknown && !is_function_word(l.part_of_speech); });
    write_lexeme_occurences(lexeme_occ, unknown_fout,
        [] (const Lexeme &l) { return l.part_of_speech == PoS::Unknown; });

    freq_fout.close();
    freq_no_func_fout.close();
    unknown_fout.close();
}
