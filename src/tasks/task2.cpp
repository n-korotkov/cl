#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../dictionary.h"
#include "../string_utils.h"

struct Concordance {
    std::vector<LexemeId> left_context;
    std::vector<LexemeId> keywords;
    std::vector<LexemeId> right_context;
};

bool operator==(Concordance const &c1, Concordance const &c2) {
    return std::equal(c1.left_context.begin(), c1.left_context.end(),
                      c2.left_context.begin(), c2.left_context.end())
        && std::equal(c1.keywords.begin(), c1.keywords.end(),
                      c2.keywords.begin(), c2.keywords.end())
        && std::equal(c1.right_context.begin(), c1.right_context.end(),
                      c2.right_context.begin(), c2.right_context.end());
}

template<>
struct std::hash<Concordance>{
    size_t operator()(Concordance const &c) const noexcept {
        size_t h {0};
        for (LexemeId id : c.left_context) {
            h = 6364136223846793005ull * h + id;
        }
        for (LexemeId id : c.keywords) {
            h = 6364136223846793005ull * h + id;
        }
        for (LexemeId id : c.right_context) {
            h = 6364136223846793005ull * h + id;
        }
        return h;
    }
};

extern Dictionary dict;
extern std::unordered_map<std::string, std::vector<WordFormId>> tokenized_files;
extern std::vector<WordFormId> tokenize(std::istream &fin);

static std::unordered_map<Concordance, int> conc_freq;

static bool all_conc_filter(const Concordance &conc) { return true; }
static bool conc_cmp(Concordance &conc1, Concordance &conc2) {
    return conc_freq[conc1] > conc_freq[conc2];
}

static void write_concordances(std::vector<Concordance> &concordances,
                               std::ostream &out,
                               std::function<bool (const Concordance &)> filter = all_conc_filter) {
    std::vector<int> left_len;
    std::vector<int> right_len;
    for (Concordance &conc : concordances) {
        if (filter(conc)) {
            left_len.push_back(0);
            right_len.push_back(0);
            for (LexemeId id : conc.left_context) {
                left_len.back() += utf8_str_len(dict.get_lexeme(id).lemma) + 1;
            }
            for (LexemeId id : conc.right_context) {
                right_len.back() += utf8_str_len(dict.get_lexeme(id).lemma) + 1;
            }
        }
    }
    int left_len_max {*std::max_element(left_len.begin(), left_len.end())};
    int right_len_max {*std::max_element(right_len.begin(), right_len.end())};

    auto left_len_it {left_len.begin()};
    auto right_len_it {right_len.begin()};
    for (Concordance &conc : concordances) {
        if (filter(conc)) {
            out << std::string(left_len_max - *left_len_it, ' ');
            for (LexemeId id : conc.left_context) {
                out << dict.get_lexeme(id).lemma << " ";
            }
            out << "| ";
            for (LexemeId id : conc.keywords) {
                out << dict.get_lexeme(id).lemma << " ";
            }
            out << "| ";
            for (LexemeId id : conc.right_context) {
                out << dict.get_lexeme(id).lemma << " ";
            }
            out << std::string(right_len_max - *right_len_it, ' ');
            out << ": " << conc_freq[conc] << "\n";
            left_len_it++;
            right_len_it++;
        }
    }
}

void task(int argc, char **argv) {
    std::string phrase(argv[4]);
    std::istringstream in(phrase);
    std::vector<WordFormId> phrase_tokens = tokenize(in);

    size_t context_size {std::stoull(argv[5])};
    size_t freq_threshold {std::stoull(argv[6])};

    std::vector<LexemeId> lexemes(phrase_tokens.size() + 1, 0);
    std::transform(phrase_tokens.begin(), phrase_tokens.end(), lexemes.begin(),
                   [] (WordFormId &word_form) { return dict.disambiguate_lexeme(word_form); });

    std::vector<size_t> prefix_func {0};
    for (size_t i {1}; i < lexemes.size(); ++i) {
        size_t j {prefix_func[i - 1]};
        while (j != 0 && lexemes[i] != lexemes[j]) {
            j = prefix_func[j - 1];
        }
        prefix_func.push_back(lexemes[i] == lexemes[j] ? j + 1 : 0);
    }

    for (auto &entry : tokenized_files) {
        std::vector<size_t> subs;
        prefix_func.resize(phrase_tokens.size() + 1);
        lexemes.resize(phrase_tokens.size() + 1);
        std::transform(entry.second.begin(), entry.second.end(), std::back_inserter(lexemes),
                       [] (WordFormId &word_form) { return dict.disambiguate_lexeme(word_form); });

        for (size_t i {phrase_tokens.size() + 1}; i < lexemes.size(); ++i) {
            size_t j {prefix_func[i - 1]};
            while (j != 0 && lexemes[i] != lexemes[j]) {
                j = prefix_func[j - 1];
            }
            prefix_func.push_back(lexemes[i] == lexemes[j] ? j + 1 : 0);
        }

        for (size_t i {phrase_tokens.size() + 1}; i < lexemes.size(); ++i) {
            if (prefix_func[i] == phrase_tokens.size()) {
                size_t kwi {i - phrase_tokens.size() + 1};
                std::vector<LexemeId> keywords;
                for (size_t j {kwi}; j < i + 1; ++j) {
                    keywords.push_back(lexemes[j]);
                }
                size_t li {std::max(phrase_tokens.size() + 1, kwi - context_size)};
                std::vector<LexemeId> left_context;
                for (size_t j {li}; j < kwi; ++j) {
                    left_context.push_back(lexemes[j]);
                }
                size_t ri {std::min(lexemes.size(), i + context_size + 1)};
                std::vector<LexemeId> right_context;
                for (size_t j {i + 1}; j < ri; ++j) {
                    right_context.push_back(lexemes[j]);
                }
                for (size_t left_bound {0}; left_bound <= left_context.size(); ++left_bound) {
                    for (size_t right_bound {0}; right_bound <= std::min(right_context.size(), context_size - left_bound); ++right_bound) {
                        if (left_bound == 0 && right_bound == 0) {
                            continue;
                        }
                        Concordance conc {
                            std::vector<LexemeId>(left_context.end() - left_bound, left_context.end()),
                            keywords,
                            std::vector<LexemeId>(right_context.begin(), right_context.begin() + right_bound),
                        };
                        ++conc_freq[conc];
                    }
                }
            }
        }
    }

    std::vector<Concordance> conc_occ;
    for (auto &entry : conc_freq) {
        conc_occ.push_back(entry.first);
    }
    std::sort(conc_occ.begin(), conc_occ.end(), conc_cmp);

    std::ofstream conc_l_fout("concordances_l.txt");
    std::ofstream conc_r_fout("concordances_r.txt");
    std::ofstream conc_lr_fout("concordances_lr.txt");

    write_concordances(conc_occ, conc_l_fout,
        [freq_threshold] (const Concordance &conc) { return conc_freq[conc] >= freq_threshold && conc.right_context.empty(); });
    write_concordances(conc_occ, conc_r_fout,
        [freq_threshold] (const Concordance &conc) { return conc_freq[conc] >= freq_threshold && conc.left_context.empty(); });
    write_concordances(conc_occ, conc_lr_fout,
        [freq_threshold] (const Concordance &conc) { return conc_freq[conc] >= freq_threshold && !conc.left_context.empty() && !conc.right_context.empty(); });

    conc_l_fout.close();
    conc_r_fout.close();
    conc_lr_fout.close();

}
