#include"index/indexer.h"
#include<unordered_set>
#include"storage/storage.h"
#include"parsing/htmlParser.h"
#include<sstream>
#include<iostream>

Indexer::Indexer(Storage& storage) : storage_(storage) {}

void Indexer::printIndex(const Index& index) {
	std::cout << "INDEX:" << "\n";

	for (const auto& [key, value] : index) {
		std::cout << "word: " << key << "\n";

		for (const auto& [key, value] : value) {
			std::cout << key << ": " << value << "\n";
		}

		std::cout << "\n";
	}
}

Index Indexer::buildIndexFromStorage(){
	std::cout << "Start building index from storage" << std::endl;
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

	std::cout << "Start saving index to storage" << std::endl;
	storage_.saveIndex(index);
	std::cout << "Finish saving index to storage" << std::endl;

	return index;
}