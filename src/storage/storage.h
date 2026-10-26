
#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "index/index.h"
#include "crawl/frontier.h"

#include <sqlite3.h>

namespace fs = std::filesystem;

class Storage {
public:
    explicit Storage(const fs::path& dataDir);
    ~Storage();

    // Storage owns SQLite resources and must not be copied.
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    void savePageContent(
        const std::string& url,
        const std::string& title,
        const std::string& description,
        const std::string& text
    );

    std::unordered_set<std::string> loadVisitedUrls();
    std::string loadPage(const std::string& url);

    void savePageIndex(
        const std::string& url,
        const PageIndex& pageIndex
    );

    void Storage::savePagesIndex(
        const std::vector<std::pair<std::string, PageIndex>>& pages);

    PostingList loadIndexOfWord(const std::string& word);

    std::pair<std::string, std::string> loadPageMetadata(
        const std::string& url
    );
    std::unordered_map<std::string, std::pair<std::string, std::string>> 
        loadPageMetadataBulk(const std::vector<std::string>& urls);

    bool checkVisited(const std::string& url);
    void markFetching(const std::string& url);
    int countSaved();
    void markFailed(const std::string& url);
    void markFetched(const std::string& url);
    std::string getStatus(const std::string& url);

    void loadFrontier(Frontier& frontier);
    void saveFrontier(Frontier& frontier);

    void clearDatabase();

private:
    fs::path g_dataDir_;
    fs::path pages_path_;
    fs::path visited_path_;
    fs::path index_path_;

    sqlite3* db_ = nullptr;

    // Reusable prepared statements
    sqlite3_stmt* markFetchingStmt_ = nullptr;
    sqlite3_stmt* checkVisitedStmt_ = nullptr;

    void initializeDatabase();
    void prepareStatements();
    void finalizeStatements();
};
