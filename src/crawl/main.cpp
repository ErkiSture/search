#include<string>
#include"storage/storage.h"
#include<iostream>
#include"index/indexer.h"
#include<queue>
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
#include"query/querier.h"
#include"processing/htmlParser.h"
#include <cstdlib>

constexpr int WORKER_COUNT = 5;

int main(int argc, char** argv) {

	if (argc < 3) {
		std::cout << "Usage: " << "crawler <seed_url> <max_pages>";
		return 1;
	}

	std::string seedUrl = argv[1];
	int maxPages = std::stoi(argv[2]);

	const char* dataDir = std::getenv("SEARCH_DATA_DIR");

	if (!dataDir) {
		dataDir = "data";
	}

	Storage storage(dataDir);
	SystemClock clock;
	DomainManager domainManager(clock);
	CurlHttpClient httpClient;
	HtmlParser htmlParser;
	UrlProcessor urlProcessor;
	PageScorer pageScorer;
	Indexer indexer(storage);
	Crawler crawler(storage, domainManager, clock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, maxPages, WORKER_COUNT);
	crawler.run(seedUrl);

	std::cout << "Done. Crawled " << maxPages << " pages max.\n\n";

	return 0;
}