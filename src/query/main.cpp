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

		for (size_t i = 0; i < std::min(results.size(), size_t{ 5 }); ++i) {
			const SearchResult& result = results[i];

			std::cout << "Title: " << result.title << '\n';
			std::cout << "Description: " << result.description << '\n';
			std::cout << "Url: " << result.url << '\n';
			std::cout << "Score: " << result.score << "\n\n";
		}

		std::cout << std::endl << std::endl;
	}

	return 1;
}