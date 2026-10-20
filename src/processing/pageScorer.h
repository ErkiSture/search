#pragma once

#include<string>

class PageScorer {
public:
    double score(
        const std::string& title,
        const std::string& text
    ) const;
};