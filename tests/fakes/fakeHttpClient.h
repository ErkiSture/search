#include "net/httpClient.h"
#include <map>
#include <vector>
#include <string>
#include <chrono>

class FakeHttpClient : public HttpClient {
public:
    std::map<std::string, FetchResult> responses;
    std::vector<std::string> requestedUrls;
    std::vector<std::chrono::steady_clock::time_point> requestTimes;

    std::function<void(const std::string&)> onFetch;

    FetchResult fetchUrl(const std::string& url) override {
        requestedUrls.push_back(url);
        requestTimes.push_back(std::chrono::steady_clock::now());

        if (onFetch) {
            onFetch(url);
        }

        auto it = responses.find(url);
        if (it == responses.end()) {
            return { false, "" };
        }

        return it->second;
    }
};