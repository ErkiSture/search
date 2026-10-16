#include<vector>
#include<sstream>

#include"Querier.h"
#include"index/index.h"

Querier::Querier(Storage& storage) : storage_(storage)
{
}

std::vector<SearchResult> Querier::search(std::string query) 
{
	// split words in query
	std::vector<std::string> queryWords;
	std::stringstream ss(query);
	std::string word;

	while (ss >> word) {
		queryWords.push_back(word);
	}

	// TODO: normalise words

	Index index = storage_.loadIndex();

	std::unordered_map<std::string, int> scores;

	for (const std::string& word : queryWords) {
		auto it = index.find(word);

		if (it == index.end()) {
			continue;
		}

		for (const auto& [url, frequency] : it->second) {
			scores[url] += frequency;
		}
	}

	std::vector<SearchResult> results;

	// Populate results
	for (const auto& [url, score] : scores) {
		results.push_back({ url, score });
	}

	// Sort URLs by score
	std::sort(results.begin(), results.end(),
		[](const SearchResult& a, const SearchResult& b) {
			return a.score > b.score;
		});

	return results;
}