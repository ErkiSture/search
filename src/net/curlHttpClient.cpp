
#include <curl/curl.h>
#include "curlHttpClient.h"

#include <iostream>
#include <string>

size_t CurlHttpClient::write_callback(
    char* ptr,
    size_t size,
    size_t nmemb,
    std::string* response
) {
    const size_t bytes = size * nmemb;
    response->append(ptr, bytes);
    return bytes;
}

HttpClient::FetchResult CurlHttpClient::fetchUrl(const std::string& url) {
    CURL* curl = curl_easy_init();

    if (!curl) {
        std::cerr << "[ERROR] Failed to initialize CURL"
            << " | URL: " << url << '\n';
        return { false, "" };
    }

    std::string response;
    char errorBuffer[CURL_ERROR_SIZE] = {};

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "MyLearningCrawler/0.1 (contact: erik.j.d.berg@gmail.com)"
    );

    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);

    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    char* effectiveUrl = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);

    if (res != CURLE_OK) {
        std::cerr
            << "[ERROR] Fetch failed"
            << " | URL: " << url
            << " | Final URL: "
            << (effectiveUrl ? effectiveUrl : "(unknown)")
            << " | CURL code: " << static_cast<int>(res)
            << " | CURL error: "
            << (errorBuffer[0] ? errorBuffer : curl_easy_strerror(res))
            << " | HTTP status: " << httpStatus
            << '\n';

        curl_easy_cleanup(curl);
        return { false, "" };
    }

    std::cout
        << "[OK] Fetch succeeded"
        << " | URL: " << url
        << " | Final URL: "
        << (effectiveUrl ? effectiveUrl : "(unknown)")
        << " | HTTP status: " << httpStatus
        << " | Bytes: " << response.size()
        << '\n';

    curl_easy_cleanup(curl);

    return { true, response };
}
