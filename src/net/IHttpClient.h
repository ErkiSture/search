#pragma once

#include<string>

class IHttpClient {

public:
    virtual fetchUrl(const std::string& url) const = 0;

    struct FetchResult {
        bool success;
        std::string data;
    };
};
