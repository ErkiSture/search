#include "urlParser.h"
#include <curl/curl.h>

std::optional<std::string> resolveUrl(
    const std::string& baseUrl,
    const std::string& link
) {
    CURLU* url = curl_url();
    if (!url) {
        return std::nullopt;
    }

    CURLUcode result =
        curl_url_set(url, CURLUPART_URL, baseUrl.c_str(), 0);

    if (result == CURLUE_OK) {
        result = curl_url_set(url, CURLUPART_URL, link.c_str(), 0);
    }

    char* scheme = nullptr;
    if (result == CURLUE_OK) {
        result = curl_url_get(url, CURLUPART_SCHEME, &scheme, 0);
    }

    bool validScheme =
        result == CURLUE_OK && scheme &&
        (std::string(scheme) == "http" ||
            std::string(scheme) == "https");

    curl_free(scheme);

    std::optional<std::string> output;

    if (validScheme) {
        curl_url_set(url, CURLUPART_FRAGMENT, nullptr, 0);

        char* fullUrl = nullptr;

        if (curl_url_get(url, CURLUPART_URL, &fullUrl, 0) 
            == CURLUE_OK && fullUrl) {
            output = std::string(fullUrl);
        }

        curl_free(fullUrl);
    }

    curl_url_cleanup(url);
    return output;
}