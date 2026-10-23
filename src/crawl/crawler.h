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
#include"net/httpClient.h"
#include"processing/htmlParser.h"
#include"processing/urlProcessor.h"
#include"processing/pageScorer.h"
#include"frontier.h"
#include"crawlPolicy.h"

class Crawler {
public:
	explicit Crawler(
		Storage& storage,
		DomainManager& domainManager,
		SystemClock& clock,
		HttpClient& httpClient,
		HtmlParser& htmlParser,
		UrlProcessor& urlProcessor,
		PageScorer& pageScorer,
		Indexer& indexer,
		int maxPages,
		int workerCount
	);
	void run(const std::string& seedUrl);

private:
	std::optional<std::string> Crawler::getNextUrl(std::unique_lock<std::mutex>& lock);
	void Crawler::processUrl(std::string& url);
	void worker();

	void fetchRobots(const std::string& url);
	void fetchPage(const std::string& url);
	void scheduleRetry(const std::string& url);
	void logFetchTime(const std::string& url, std::chrono::steady_clock::time_point start);

	// Prints a url along with how long the fetch took
	int workerCount_ = 0;
	int activeWorkers = 0;
	int maxPages_;
	int sumFetchTime_ = 0;

	Storage& storage_;
	DomainManager& domainManager_;
	SystemClock& clock_;
	HttpClient& httpClient_;
	HtmlParser& htmlParser_;
	UrlProcessor& urlProcessor_;
	PageScorer& pageScorer_;
	Indexer& indexer_;

	Frontier frontier_;
	CrawlPolicy crawlPolicy_;
	std::mutex mutex_;
	std::condition_variable cv_;
};