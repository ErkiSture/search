#pragma once

#include<string>
#include<unordered_set>
#include<queue>
#include"storage/storage.h"
#include"index/indexer.h"
#include"index/index.h"
#include<mutex>
#include"domainManager.h"
#include<optional>
#include"utils/systemClock.h"

class Crawler {
public:
	explicit Crawler(
		Storage& storage,
		DomainManager& domainManager,
		SystemClock& clock,
		int maxPages,
		int workerCount
	);
	void run(const std::string& seedUrl);

private:
	std::optional<std::string> Crawler::getNextUrl(std::unique_lock<std::mutex>& lock);
	void Crawler::processUrl(std::string& url);
	void worker();

	void reQueueUrl(const std::string& url);
	void fetchRobots(const std::string& url);
	void fetchPage(const std::string& url);

	// Prints a url along with how long the fetch took
	void Crawler::logFetchTime(const std::string& url, std::chrono::steady_clock::time_point start);
	int workerCount_ = 0;
	int activeWorkers = 0;
	int maxPages_;

	std::unordered_set<std::string> visited_;
	std::queue<std::string> frontier_;

	Storage storage_;
	DomainManager domainManager_;
	SystemClock clock_;

	int sumFetchTime_ = 0;

	std::mutex mutex_;
	std::condition_variable cv_;
};