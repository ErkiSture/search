#pragma once

#include<vector>
#include<string>

class UrlProcessor {
public:
    std::vector<std::string> process(
        const std::vector<std::string>& links,
        const std::string& baseUrl
    ) const;
};