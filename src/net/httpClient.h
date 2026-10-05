#pragma once

#include<string>

class HttpClient {

public:
    struct FetchResult {
        bool success;
        std::string data;
    };

    virtual FetchResult fetchUrl(const std::string& url) const = 0;
};
