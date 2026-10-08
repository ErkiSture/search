#include<string>
#include"storage/storage.h"
#include<iostream>
#include"index/indexer.h"
#include<queue>
#include"parsing/htmlParser.h"
#include"parsing/linkExtractor.h"
#include<thread>
#include<chrono>
#include<mutex>
#include<condition_variable>
#include<vector>
#include"crawler.h"
#include"domainManager.h"
#include"utils/systemClock.h"
#include<optional>
#include"net/httpClient.h"
#include"net/curlHttpClient.h"

constexpr int WORKER_COUNT = 4;

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cout << "Usage: " << "crawler <seed_url> <max_pages>";
		return 1;
	}

	std::string seedUrl = argv[1];
	int maxPages = std::stoi(argv[2]);

	Storage storage("data");
	SystemClock clock;
	DomainManager domainManager(clock);
	CurlHttpClient httpClient;
	Crawler crawler(storage, domainManager, clock, httpClient, maxPages, WORKER_COUNT);
	crawler.run(seedUrl);
	Indexer indexer(storage);

	std::cout << "Done. Crawled " << maxPages << " pages max.\n";

	return 0;
}