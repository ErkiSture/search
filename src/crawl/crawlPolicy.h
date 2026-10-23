#pragma once

#include <cstddef>
#include "processing/htmlParser.h"

class CrawlPolicy {
public:
    bool shouldExplore(
        double relevanceScore,
        std::size_t frontierSize
    ) const;
    bool shouldIndex(
        const ParsedPage& page, 
        double relevanceScore
    ) const;
};