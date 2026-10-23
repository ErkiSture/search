#include "pageScorer.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_set>

namespace {

    std::string toLower(const std::string& input)
    {
        std::string result = input;

        std::transform(
            result.begin(), result.end(), result.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            }
        );

        return result;
    }

    int countWords(const std::string& text)
    {
        std::istringstream stream(text);
        std::string word;
        int count = 0;

        while (stream >> word) {
            ++count;
        }

        return count;
    }

    int countKeywordMatches(
        const std::string& text,
        const std::unordered_set<std::string>& keywords)
    {
        std::istringstream stream(text);
        std::string word;
        int matches = 0;

        while (stream >> word) {
            word.erase(
                std::remove_if(
                    word.begin(), word.end(),
                    [](unsigned char c) {
                        return !std::isalnum(c);
                    }
                ),
                word.end()
            );

            word = toLower(word);

            if (keywords.count(word)) {
                ++matches;
            }
        }

        return matches;
    }

}

double PageScorer::score(
    const std::string& title,
    const std::string& text
) const
{
    static const std::unordered_set<std::string> educationalKeywords = {
        "education", "educational", "university", "college",
        "school", "course", "courses", "lecture", "lectures",
        "tutorial", "tutorials", "learn", "learning",
        "lesson", "lessons", "study", "studies",
        "research", "academic", "science", "scientific",
        "mathematics", "math", "algebra", "calculus",
        "physics", "chemistry", "biology", "history",
        "engineering", "programming", "computer", "theory",
        "exercise", "exercises", "assignment", "textbook"
    };

    const std::string lowerTitle = toLower(title);
    const std::string lowerText = toLower(text);

    const int wordCount = countWords(text);

    // Very little content usually means an empty or low-value page.
    if (wordCount < 20) {
        return 0.0;
    }

    double result = 0.0;

    // Reward useful content length, but cap the contribution.
    result += std::min(wordCount / 20.0, 20.0);

    // Educational keywords in the title are especially useful.
    result += std::min(
        countKeywordMatches(lowerTitle, educationalKeywords) * 8.0,
        40.0
    );

    // Reward educational keywords in the body text.
    result += std::min(
        countKeywordMatches(lowerText, educationalKeywords) * 2.0,
        40.0
    );

    return std::min(result, 100.0);
}