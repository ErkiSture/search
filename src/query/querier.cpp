#include<vector>
#include<sstream>
#include<iostream>
#include <algorithm>

#include"querier.h"
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
            << " ms\n\n";
        for (const auto& [url, frequency] : postings) {
            scores[url] += frequency;
        }
    }

    // Build search results
    //auto start = std::chrono::steady_clock::now();

    //std::vector<SearchResult> results;
    //for (const auto& [url, score] : scores) {
    //    auto [title, description] = storage_.loadPageMetadata(url);

    //    results.push_back({
    //        url,
    //        score,
    //        title,
    //        description
    //    });
    //}

    //std::sort(results.begin(), results.end(),
    //    [](const SearchResult& a, const SearchResult& b) {
    //        return a.score > b.score;
    //    });
    //auto end = std::chrono::steady_clock::now();
    //std::cout << "Building results and sorting them took: "
    //    << std::chrono::duration_cast<std::chrono::milliseconds>(
    //        end - start).count()
    //    << " ms\n\n";

    auto start = std::chrono::steady_clock::now();

    std::vector<std::string> urls;
    urls.reserve(scores.size());

    for (const auto& [url, score] : scores) {
        urls.push_back(url);
    }

    auto metadata = storage_.loadPageMetadataBulk(urls);

    std::vector<SearchResult> results;
    results.reserve(scores.size());

    for (const auto& [url, score] : scores) {
        auto it = metadata.find(url);

        if (it == metadata.end()) {
            continue;
        }

        results.push_back({
            url,
            score,
            it->second.first,
            it->second.second
            });
    }

    std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.score > b.score;
        });

    auto end = std::chrono::steady_clock::now();

    std::cout << "Building results and sorting them took: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count()
        << " ms\n\n";

    return results;
}