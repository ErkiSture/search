#pragma once

#include<filesystem>
#include<string>
#include<unordered_set>
#include"index/indexer.h"

namespace fs = std::filesystem;

class Storage {
public:
	explicit Storage(const fs::path& dataDir);

	void savePage(const std::string & url, const std::string & text);
	std::unordered_set<std::string> loadVisitedUrls();
	std::string loadPage(const std::string& url);
	void saveIndex(const Index& index);
	Index loadIndex();

private:
	fs::path g_dataDir_;
	fs::path pages_path_;
	fs::path visited_path_;
	fs::path index_path_;
};
