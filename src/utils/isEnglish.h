
#pragma once

#include <cctype>
#include <string>
#include <sstream>
#include <unordered_set>

inline bool isEnglish(const std::string& text) {
    static const std::unordered_set<std::string> words = {
        "the", "and", "is", "in", "to", "of", "a", "that", "it",
        "for", "on", "with", "this", "was", "are", "be", "as",
        "by", "not", "or", "from", "have", "has", "but", "you",
        "which", "their", "they", "we", "can", "will", "would",
        "there", "been", "were", "one", "all", "about", "more",
        "when", "who", "what", "your", "if", "some", "other",
        "into", "than", "then", "these", "how", "its", "our"
    };

    std::istringstream stream(text);
    std::string word;

    int total = 0;
    int matches = 0;

    while (stream >> word) {
        // Convert to lowercase
        for (char& c : word) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))
                );
        }

        // Remove punctuation from the beginning
        // and end of the word
        while (!word.empty() &&
            std::ispunct(static_cast<unsigned char>(word.front()))) {
            word.erase(word.begin());
        }

        while (!word.empty() &&
            std::ispunct(static_cast<unsigned char>(word.back()))) {
            word.pop_back();
        }

        if (word.size() < 2) {
            continue;
        }

        ++total;

        if (words.count(word)) {
            ++matches;
        }
    }

    return total >= 20 &&
        matches >= 3 &&
        static_cast<double>(matches) / total >= 0.08;
}
