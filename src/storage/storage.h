#pragma once

#include<filesystem>
#include<string>
#include<unordered_set>
#include<unordered_map>
#include"index/index.h"
#include<sqlite3.h>

namespace fs = std::filesystem;

class Storage {
public:
	explicit Storage(const fs::path& dataDir);
	~Storage();

	void savePage(const std::string & url, const std::string & text);
	std::unordered_set<std::string> loadVisitedUrls();
	std::string loadPage(const std::string& url);
	
	Index loadIndex();
	void saveIndex(const Index& index);

	void savePageIndex(std::string& url, PageIndex& pageIndex);
	PostingList loadIndexOfWord(const std::string& word);

	bool checkVisited(const std::string& url);
	void markFetching(const std::string& url);
	int countSaved();
	void markFailed(const std::string& url);

	void clearDatabase();

private:
	fs::path g_dataDir_;
	fs::path pages_path_;
	fs::path visited_path_;
	fs::path index_path_;


	sqlite3* db_ = nullptr;

	void initializeDatabase();
};
