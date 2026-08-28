#include"indexer.h"
#include<unordered_set>
#include"storage.h"
#include"html_parser.h"
#include<sstream>
#include<iostream>

void print_index(const Index& index) {
	std::cout << "INDEX:" << "\n";

	for (const auto& [key, value] : index) {
		std::cout << "word: " << key << "\n";

		for (const auto& [key, value] : value) {
			std::cout << key << ": " << value << "\n";
		}

		std::cout << "\n";
	}
}

Index build_index_from_storage(){
	Index index;

	std::unordered_set visisted_urls = loadVisitedUrls();

	for (std::string url : visisted_urls) {
		std::string page = loadPage(url);

		std::stringstream ss(page);
		std::string word;

		while (ss >> word) {
			index[word][url]++;
		}
	}

	saveIndex(index);
	//print_index(index);

	return index;
}
