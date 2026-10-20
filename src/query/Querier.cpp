#include<vector>
#include<sstream>
#include<iostream>

#include"Querier.h"
#include"index/index.h"

Querier::Querier(Storage& storage) : storage_(storage)
{
}

std::vector<SearchResult> Querier::search(std::string query)
{
    std::vector<std::string> queryWords;
    std::stringstream ss(query);
    std::string word;

    while (ss >> word) {
        queryWords.push_back(word);
    }

    std::unordered_map<std::string, int> scores;

    for (const std::string& word : queryWords) {
        auto start = std::chrono::steady_clock::now();

        PostingList postings = storage_.loadIndexOfWord(word);

        auto end = std::chrono::steady_clock::now();

        std::cout << "Loading postings took "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start).count()
            << " ms\n";
        for (const auto& [url, frequency] : postings) {
            scores[url] += frequency;
        }
    }

    std::vector<SearchResult> results;

    for (const auto& [url, score] : scores) {
        results.push_back({ url, score });
    }

    std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.score > b.score;
        });

    return results;
}