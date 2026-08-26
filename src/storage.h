#include<filesystem>
#include<string>
#include<unordered_set>
#include"indexer.h"

void init_storage(const std::filesystem::path& dataDir);

void savePage(const std::string & url, const std::string & text);
std::unordered_set<std::string> loadVisitedUrls();
std::string loadPage(const std::string& url);
void saveIndex(const Index& index);
Index loadIndex();