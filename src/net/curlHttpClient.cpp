#include<curl/curl.h>
#include"curlHttpClient.h"
#include<iostream>

size_t CurlHttpClient::write_callback(char* ptr, size_t size, size_t nmemb, std::string* response) {
	response->append(ptr, size * nmemb);
	return size * nmemb;
}

HttpClient::FetchResult CurlHttpClient::fetchUrl(const std::string& url) const {
    CURL* curl = curl_easy_init();

    if (!curl) {
        std::cerr << "Failed to init curl\n";
        return { false, "" };
    }

    std::string response;

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

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "curl error: "
            << curl_easy_strerror(res) << '\n';

        curl_easy_cleanup(curl);
        return { false, "" };
    }

    curl_easy_cleanup(curl);

    return { true, response };
}
