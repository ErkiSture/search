#include "crawlPolicy.h"
#include "utils/isEnglish.h"

bool CrawlPolicy::shouldExplore(
    double relevanceScore,
    std::size_t frontierSize
) const {
    return relevanceScore >= 65.0 || frontierSize < 1000;
}

bool CrawlPolicy::shouldIndex(
    const ParsedPage& page,
    double relevanceScore
) const {
    return relevanceScore >= 30.0 && isEnglish(page.text);
}
