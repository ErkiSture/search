#pragma once

#include"httpClient.h"

class CurlHttpClient : public HttpClient {
private:
	static size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* response);

public:
	FetchResult fetchUrl(const std::string& url) const override;

};