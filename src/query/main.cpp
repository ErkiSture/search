#include<iostream>
#include<string>

#include "query/querier.h"
#include "storage/storage.h"
#include "index/index.h"

int main(int argc, char** argv) {
	Storage storage("data");
	Querier querier(storage);

	std::string query;

	while (true) {
		std::cout << "Search (or 'exit'): ";
		if ((!std::getline(std::cin, query) || query == "exit")) {
			break;
		}

		auto results = querier.search(query);

		if (results.empty()) {
			std::cout << "No results found" << std::endl;
		}

		for (SearchResult result : results) {
			std::cout << "1: " << result.url << "    score: " << result.score << std::endl;
		}

		std::cout << std::endl << std::endl;
	}

	return 1;
}