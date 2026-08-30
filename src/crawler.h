#pragma once

#include<string>
#include<unordered_set>
#include<queue>
#include"storage.h"
#include"indexer.h"
#include"index.h"
#include<mutex>
#include"domainManager.h"

class Crawler {
public:
	explicit Crawler(Storage& storage, DomainManager& domainManager, int maxPages, int workerCount);

	void run(const std::string& seedUrl);
private:

	void worker();

	int workerCount_ = 0;
	int activeWorkers = 0;
	int maxPages_;

	std::unordered_set<std::string> visited_;
	std::queue<std::string> frontier_;

	Storage storage_;
	DomainManager domainManager_;

	int sumFetchTime_ = 0;

	std::mutex mutex_;
	std::condition_variable cv_;
};