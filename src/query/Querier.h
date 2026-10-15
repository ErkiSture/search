#pragma once

#include<string>
#include"storage/storage.h"
#include<vector>

struct SearchResult {
	std::string url;
};

class Querier {
public:
	explicit Querier(Storage& storage);

	std::vector<SearchResult> search(std::string query);
private:
	Storage storage_;
};