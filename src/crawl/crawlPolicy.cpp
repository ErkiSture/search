#include "crawlPolicy.h"
#include "utils/isEnglish.h"

bool CrawlPolicy::shouldExplore(
    double relevanceScore,
    std::size_t frontierSize
) const {
    return (relevanceScore >= 65.0 && frontierSize <= 20000)
        || frontierSize < 1000;
}

bool CrawlPolicy::shouldIndex(
    const ParsedPage& page,
    double relevanceScore
) const {
    constexpr size_t MIN_TEXT_LENGTH = 200;
    constexpr size_t MAX_TEXT_LENGTH = 500'000;

    const size_t length = page.text.size();

    return relevanceScore >= 30.0
        && isEnglish(page.text)
        && length >= MIN_TEXT_LENGTH
        && length <= MAX_TEXT_LENGTH;
}