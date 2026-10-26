#include"index/indexer.h"
#include<unordered_set>
#include"storage/storage.h"
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

PageIndex Indexer::indexPage(const std::string text)
{
	PageIndex pageIndex;
	std::stringstream ss(text);
	std::string word;

	while (ss >> word) {
		std::string cleaned;
		cleaned.reserve(word.size());

		bool onlyLetters = true;

		for (unsigned char c : word) {
			if (!std::isalpha(c)) {
				onlyLetters = false;
				break;
			}

			cleaned += static_cast<char>(std::tolower(c));
		}

		if (onlyLetters && !cleaned.empty()) {
			pageIndex[cleaned]++;
		}
	}

	return pageIndex;
}