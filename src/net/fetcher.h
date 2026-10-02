#pragma once
#include <string>

struct FetchResult {
    bool success;
    std::string data;
};

FetchResult fetch_url(const std::string& url);