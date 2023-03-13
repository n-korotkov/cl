#pragma once

#include <string_view>
#include <unordered_map>

enum class PoS {
    Unknown,

    Noun,
    AdjectiveFull,
    AdjectiveShort,
    Comparative,
    Verb,
    Infinitive,
    ParticipleFull,
    ParticipleShort,
    Gerund,
    Numeral,
    Adverb,
    Pronoun,
    Predicative,
    Preposition,
    Conjunction,
    Particle,
    Interjection,

    Max,
};

PoS to_pos(std::string_view sv) {
    static const std::unordered_map<std::string_view, PoS> t({
        { "NOUN", PoS::Noun },
        { "ADJF", PoS::AdjectiveFull },
        { "ADJS", PoS::AdjectiveShort },
        { "COMP", PoS::Comparative },
        { "VERB", PoS::Verb },
        { "INFN", PoS::Infinitive },
        { "PRTF", PoS::ParticipleFull },
        { "PRTS", PoS::ParticipleShort },
        { "GRND", PoS::Gerund },
        { "NUMR", PoS::Numeral },
        { "ADVB", PoS::Adverb },
        { "NPRO", PoS::Pronoun },
        { "PRED", PoS::Predicative },
        { "PREP", PoS::Preposition },
        { "CONJ", PoS::Conjunction },
        { "PRCL", PoS::Particle },
        { "INTJ", PoS::Interjection },

        { "s",      PoS::Noun },
        { "s.PROP", PoS::Noun },
        { "a",      PoS::AdjectiveFull },
        { "num",    PoS::Numeral },
        { "anum",   PoS::AdjectiveFull },
        { "v",      PoS::Infinitive },
        { "adv",    PoS::Adverb },
        { "spro",   PoS::Pronoun },
        { "apro",   PoS::AdjectiveFull },
        { "advpro", PoS::Adverb },
        { "pr",     PoS::Preposition },
        { "conj",   PoS::Conjunction },
        { "part",   PoS::Particle },
        { "intj",   PoS::Interjection },
        { "init",   PoS::Noun },
    });
    return t.at(sv);
}

std::string_view to_string(PoS pos) {
    static const char *t[] {
        "    ", "NOUN", "ADJF", "ADJS",
        "COMP", "VERB", "INFN", "PRTF",
        "PRTS", "GRND", "NUMR", "ADVB",
        "NPRO", "PRED", "PREP", "CONJ",
        "PRCL", "INTJ", "====",
    };
    return t[static_cast<int>(pos)];
}

bool is_function_word(PoS pos) {
    return pos == PoS::Pronoun ||
        pos == PoS::Preposition ||
        pos == PoS::Conjunction ||
        pos == PoS::Particle ||
        pos == PoS::Interjection;
}
