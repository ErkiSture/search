#include"indexer.h"
#include<unordered_set>
#include"storage.h"
#include"html_parser.h"
#include<sstream>
#include<iostream>

Indexer::Indexer(Storage& storage) : storage_(storage) {}

void Indexer::print_index(const Index& index) {
	std::cout << "INDEX:" << "\n";

	for (const auto& [key, value] : index) {
		std::cout << "word: " << key << "\n";

		for (const auto& [key, value] : value) {
			std::cout << key << ": " << value << "\n";
		}

		std::cout << "\n";
	}
}

Index Indexer::build_index_from_storage(){
	Index index;

	std::unordered_set<std::string> visisted_urls = storage_.loadVisitedUrls();

	for (std::string url : visisted_urls) {
		std::string page = storage_.loadPage(url);

		std::stringstream ss(page);
		std::string word;

		while (ss >> word) {
			index[word][url]++;
		}
	}

	storage_.saveIndex(index);
	//print_index(index);

	return index;
}
