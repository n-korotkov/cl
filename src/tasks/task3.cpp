#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../bufstream.h"
#include "../dictionary.h"
#include "../trie.h"

struct NGram {
    std::vector<LexemeId> lexemes;
    std::unordered_map<std::string, std::unordered_set<size_t>> occurences;
    std::unordered_set<size_t> nested;
    std::unordered_set<size_t> extensions;

    size_t total_occurences() {
        size_t total_occurences {0};
        for (auto &entry : occurences) {
            total_occurences += entry.second.size();
        }
        return total_occurences;
    }
};

template <typename T>
static void read_ngram(T &stream, NGram &ngram) {
    size_t lexeme_count;
    stream.read(reinterpret_cast<char *>(&lexeme_count), sizeof(lexeme_count));
    ngram.lexemes.resize(lexeme_count);
    stream.read(reinterpret_cast<char *>(ngram.lexemes.data()), sizeof(LexemeId) * lexeme_count);

    size_t occurence_count;
    stream.read(reinterpret_cast<char *>(&occurence_count), sizeof(occurence_count));
    ngram.occurences.clear();
    for (size_t i {0}; i < occurence_count; ++i) {
        std::string file_name;
        std::getline(stream, file_name, '\0');
        size_t file_occurence_count;
        stream.read(reinterpret_cast<char *>(&file_occurence_count), sizeof(file_occurence_count));
        for (size_t j {0}; j < file_occurence_count; ++j) {
            size_t k;
            stream.read(reinterpret_cast<char *>(&k), sizeof(k));
            ngram.occurences[file_name].insert(k);
        }
    }

    size_t nested_count;
    stream.read(reinterpret_cast<char *>(&nested_count), sizeof(nested_count));
    ngram.nested.clear();
    for (size_t i {0}; i < nested_count; ++i) {
        size_t j;
        stream.read(reinterpret_cast<char *>(&j), sizeof(j));
        ngram.nested.insert(j);
    }

    size_t extension_count;
    stream.read(reinterpret_cast<char *>(&extension_count), sizeof(extension_count));
    ngram.extensions.clear();
    for (size_t i {0}; i < extension_count; ++i) {
        size_t j;
        stream.read(reinterpret_cast<char *>(&j), sizeof(j));
        ngram.extensions.insert(j);
    }
}

static void write_ngram(std::ostream &stream, NGram &ngram) {
    size_t lexeme_count {ngram.lexemes.size()};
    stream.write(reinterpret_cast<char *>(&lexeme_count), sizeof(lexeme_count));
    stream.write(reinterpret_cast<char *>(ngram.lexemes.data()), sizeof(LexemeId) * lexeme_count);

    size_t occurence_count {ngram.occurences.size()};
    stream.write(reinterpret_cast<char *>(&occurence_count), sizeof(occurence_count));
    for (auto &entry : ngram.occurences) {
        stream.write(entry.first.data(), entry.first.size() + 1);

        size_t file_occurence_count {entry.second.size()};
        stream.write(reinterpret_cast<char *>(&file_occurence_count), sizeof(file_occurence_count));
        for (size_t i : entry.second) {
            stream.write(reinterpret_cast<char *>(&i), sizeof(i));
        }
    }

    size_t nested_count {ngram.nested.size()};
    stream.write(reinterpret_cast<char *>(&nested_count), sizeof(nested_count));
    for (size_t i : ngram.nested) {
        stream.write(reinterpret_cast<char *>(&i), sizeof(i));
    }

    size_t extension_count {ngram.extensions.size()};
    stream.write(reinterpret_cast<char *>(&extension_count), sizeof(extension_count));
    for (size_t i : ngram.extensions) {
        stream.write(reinterpret_cast<char *>(&i), sizeof(i));
    }
}

extern Dictionary dict;
extern std::unordered_map<std::string, std::vector<WordFormId>> tokenized_files;
extern std::vector<WordFormId> tokenize(std::istream &fin);

static Trie<NGram> stable_ngrams;

static std::string lexemes_key(std::vector<LexemeId> &lexemes) {
    std::string key;
    for (LexemeId id : lexemes) {
        key.append(dict.get_lexeme(id).lemma).push_back(' ');
    }
    return key;
};

static void find_stable_ngrams(size_t frequency_threshold, float stability_threshold) {
    Trie<NGram> ngrams;

    for (auto &entry : tokenized_files) {
        std::vector<WordFormId> &tokens {entry.second};
        if (tokens.empty()) {
            continue;
        }
        LexemeId first_lexeme_id {dict.disambiguate_lexeme(tokens[0])};
        for (int i {1}; i < tokens.size(); ++i) {
            NGram bigram {{ first_lexeme_id, dict.disambiguate_lexeme(tokens[i]) }};
            std::string key {lexemes_key(bigram.lexemes)};
            if (!ngrams.contains(key)) {
                ngrams.insert(key, bigram);
            }
            size_t bigram_index {ngrams.find_by_key(key)[0]};
            ngrams[bigram_index].occurences[entry.first].insert(i - 1);

            first_lexeme_id = bigram.lexemes[1];
        }
    }

    for (size_t i {0}; i < ngrams.size(); ++i) {
        if (ngrams[i].total_occurences() < frequency_threshold) {
            continue;
        }
        for (auto &entry : std::unordered_map(ngrams[i].occurences)) {
            std::vector<WordFormId> &tokens {tokenized_files[entry.first]};
            for (size_t pos : entry.second) {
                if (pos > 0) {
                    NGram extension {ngrams[i].lexemes};
                    extension.lexemes.insert(extension.lexemes.begin(), dict.disambiguate_lexeme(tokens[pos - 1]));
                    std::string key {lexemes_key(extension.lexemes)};
                    if (!ngrams.contains(key)) {
                        ngrams.insert(key, extension);
                    }
                    size_t j {ngrams.find_by_key(key)[0]};
                    ngrams[j].occurences[entry.first].insert(pos - 1);
                    ngrams[j].nested.insert(i);
                    ngrams[i].extensions.insert(j);
                }
                if (pos + ngrams[i].lexemes.size() < tokens.size()) {
                    NGram extension {ngrams[i].lexemes};
                    extension.lexemes.push_back(dict.disambiguate_lexeme(tokens[pos + ngrams[i].lexemes.size()]));
                    std::string key {lexemes_key(extension.lexemes)};
                    if (!ngrams.contains(key)) {
                        ngrams.insert(key, extension);
                    }
                    size_t j {ngrams.find_by_key(key)[0]};
                    ngrams[j].occurences[entry.first].insert(pos);
                    ngrams[j].nested.insert(i);
                    ngrams[i].extensions.insert(j);
                }
            }
        }
    }

    std::unordered_map<size_t, size_t> stable_ngram_indices;
    for (size_t i {0}; i < ngrams.size(); ++i) {
        size_t total_occurences {ngrams[i].total_occurences()};
        bool is_stable {true};
        for (size_t j : ngrams[i].extensions) {
            if (ngrams[j].total_occurences() > total_occurences * stability_threshold) {
                is_stable = false;
                break;
            }
        }

        std::unordered_set<size_t> stable_nested;
        for (size_t j : ngrams[i].nested) {
            stable_nested.merge(ngrams[j].nested);
            if (stable_ngram_indices.count(j) == 1) {
                stable_nested.insert(j);
            }
        }
        ngrams[i].nested = std::move(stable_nested);

        if (total_occurences >= frequency_threshold && is_stable) {
            stable_ngram_indices[i] = stable_ngrams.index_cap();
            NGram stable_ngram {ngrams[i].lexemes, ngrams[i].occurences};
            for (size_t j : ngrams[i].nested) {
                stable_ngram.nested.insert(stable_ngram_indices[j]);
                stable_ngrams[stable_ngram_indices[j]].extensions.insert(stable_ngram_indices[i]);
            }
            stable_ngrams.insert(lexemes_key(stable_ngram.lexemes), stable_ngram);
        }
    }
}

void task(int argc, char **argv) {
    std::string phrase(argv[4]);
    std::istringstream in(phrase);
    std::vector<WordFormId> phrase_tokens = tokenize(in);

    std::vector<LexemeId> phrase_lexemes(phrase_tokens.size());
    std::transform(phrase_tokens.begin(), phrase_tokens.end(), phrase_lexemes.begin(),
                   [] (WordFormId &word_form) { return dict.disambiguate_lexeme(word_form); });
    std::string phrase_key {lexemes_key(phrase_lexemes)};

    if (!std::filesystem::exists("n-grams.bin")) {
        size_t frequency_threshold {std::stoull(argv[5])};
        float stability_threshold {std::stof(argv[6])};
        find_stable_ngrams(std::max(frequency_threshold, static_cast<size_t>(2)), stability_threshold);
    } else {
        std::cout << "n-grams.bin found, restoring n-grams...\n";
        std::ifstream din("n-grams.bin", std::ios_base::binary);
        IBufferedStream in(din);
        stable_ngrams.read_from(in, read_ngram);
        din.close();
    }


    std::ofstream fout("n-grams.txt");

#define CONTAINS(xs, x) (std::find((xs).begin(), (xs).end(), (x)) != (xs).end())
#define SORT(xs, cmp) (std::sort((xs).begin(), (xs).end(), (cmp)))

    if (phrase_lexemes.size() < 2) {
        std::vector<NGram> found_ngrams;
        for (size_t i {0}; i < stable_ngrams.size(); ++i) {
            if (phrase_lexemes.empty() || CONTAINS(stable_ngrams[i].lexemes, phrase_lexemes[0])) {
                found_ngrams.push_back(stable_ngrams[i]);
            }
        }
        SORT(found_ngrams, [] (NGram &x, NGram &y) { return x.total_occurences() > y.total_occurences(); });

        for (NGram &ngram : found_ngrams) {
            for (LexemeId id : ngram.lexemes) {
                fout << dict.get_lexeme(id).lemma << " ";
            }
            fout << ngram.total_occurences() << "\n";
        }
    } else {
        if (stable_ngrams.contains(phrase_key)) {
            size_t i {stable_ngrams.find_by_key(phrase_key)[0]};
            for (LexemeId id : stable_ngrams[i].lexemes) {
                fout << dict.get_lexeme(id).lemma << " ";
            }
            fout << stable_ngrams[i].total_occurences() << "\n";
            fout << "\nNested n-grams:\n";
            for (size_t j : stable_ngrams[i].nested) {
                fout << " ";
                for (LexemeId id : stable_ngrams[j].lexemes) {
                    fout << dict.get_lexeme(id).lemma << " ";
                }
                fout << stable_ngrams[j].total_occurences() << "\n";
            }
        } else {
            std::cout << "N-gram not found\n";
        }
    }


    if (!std::filesystem::exists("n-grams.bin")) {
        std::cout << "Saving n-grams to n-grams.bin...\n";
        std::ofstream dout("n-grams.bin", std::ios_base::binary);
        stable_ngrams.write_to(dout, write_ngram);
        dout.close();
    }

    fout.close();
}
