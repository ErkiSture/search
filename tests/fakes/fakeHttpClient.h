#include "net/httpClient.h"
#include <map>
#include <vector>
#include <string>

class FakeHttpClient : public HttpClient {
public:
	std::map<std::string, FetchResult> responses;
	std::vector<std::string> requestedUrls;

    FetchResult fetchUrl(const std::string& url) override {
        requestedUrls.push_back(url);

        auto it = responses.find(url);
        if (it == responses.end()) {
            return { false, "" };
        }

        return it->second;
    }
};