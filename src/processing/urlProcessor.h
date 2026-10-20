#pragma once

#include<vector>
#include<string>

class UrlProcessor {
public:
    std::vector<std::string> process(
        const std::vector<std::string>& links,
        const std::string& baseUrl
    ) const;
private:
    std::vector<std::string> UrlProcessor::resolveRelative(
        const std::vector<std::string>& links,
        const std::string& baseUrl
    ) const;
    std::vector<std::string> UrlProcessor::removeFragments(
        const std::vector<std::string>& links
    ) const;
    std::vector<std::string> UrlProcessor::deduplicateLinks(
        const std::vector<std::string>& links
    ) const;
};