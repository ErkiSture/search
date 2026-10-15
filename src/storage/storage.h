#pragma once

#include<filesystem>
#include<string>
#include<unordered_set>
#include"index/indexer.h"
#include<unordered_map>

namespace fs = std::filesystem;
using PostingList = std::unordered_map<std::string, int>; // url : count
using PageIndex = std::unordered_map<std::string, int>; // word : count

class Storage {
public:
	explicit Storage(const fs::path& dataDir);

	void savePage(const std::string & url, const std::string & text);
	std::unordered_set<std::string> loadVisitedUrls();
	std::string loadPage(const std::string& url);
	
	Index loadIndex();
	void saveIndex(const Index& index);

	void savePageIndex(std::string& url, PageIndex& pageIndex);
	PostingList loadIndexOfWord(const std::string& word);

private:
	fs::path g_dataDir_;
	fs::path pages_path_;
	fs::path visited_path_;
	fs::path index_path_;
};
