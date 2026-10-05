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

Crawler::Crawler(
    Storage& storage, 
    DomainManager& domainManager,
    SystemClock& clock,
    HttpClient& httpClient,
    int maxPages, 
    int workerCount) :
        storage_(storage),
        domainManager_(domainManager),
        clock_(clock),
        httpClient_(httpClient),
        maxPages_(maxPages),
        workerCount_(workerCount)
{
    visited_ = storage.loadVisitedUrls();
}

void Crawler::logFetchTime(const std::string& url, std::chrono::steady_clock::time_point start) {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock_.now() - start
    );

    std::cout << "fetched "
        << duration.count() << " ms"
        << " "
        << url
        << "\n";
}

std::optional<std::string> Crawler::getNextUrl(std::unique_lock<std::mutex>& lock) {
    while (true) {
        cv_.wait(lock, [&] {
            return !frontier_.empty() ||
                visited_.size() >= maxPages_ ||
                (frontier_.empty() && activeWorkers == 0);
            });

        if (visited_.size() >= maxPages_) {
            return std::nullopt;
        }

        if (frontier_.empty() && activeWorkers == 0) {
            return std::nullopt;
        }

        // Retrieve next URL
        std::string url = frontier_.front();
        frontier_.pop();

        if (visited_.count(url)) {
            continue;
        }

        return url;
    }
}

void Crawler::reQueueUrl(const std::string& url) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frontier_.push(url);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
}

void Crawler::fetchRobots(const std::string& url) {

    {
        std::unique_lock<std::mutex> lock(mutex_);

        std::string robotsUrl = domainManager_.getRobotsUrl(url);

        // Put original URL back for later
        frontier_.push(url);

        lock.unlock();

        auto start = clock_.now();

        HttpClient::FetchResult result = httpClient_.fetchUrl(robotsUrl);

        logFetchTime(url, start);
        auto duration = clock_.now() - start;

        // Add robots fetch time
        sumFetchTime_ += duration.count();

        if (result.success) {
            domainManager_.saveRobotsResult(robotsUrl, result.data);
        }
        else {
            // Failed/missing robots.txt -> treat as empty
            domainManager_.saveRobotsResult(robotsUrl, "");
        }
    }

    cv_.notify_all();
}

void Crawler::fetchPage(const std::string& url) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        visited_.insert(url);

        // Reserve domain request time BEFORE fetching
        domainManager_.saveRequest(url);
    }

    auto start = clock_.now();

    HttpClient::FetchResult result = httpClient_.fetchUrl(url);

    logFetchTime(url, start);
    auto duration = clock_.now() - start;

    if (!result.success) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            sumFetchTime_ += duration.count();
        }
        
        cv_.notify_all();
        return;
    }


    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Handle fetch result
        sumFetchTime_ += duration.count();

        std::string text = strip_html(result.data);
        storage_.savePage(url, text);

        for (const std::string& link : extract_links(result.data, url)) {
            if (!visited_.count(link))
            {
                frontier_.push(link);
            }
        }
    }

    cv_.notify_all();
}

void Crawler::processUrl(std::string& url) {
    RequestStatus status = domainManager_.check(url);

    switch (status) {

    case RequestStatus::Disallowed:
        break;
    case RequestStatus::Wait:
        reQueueUrl(url);
        break;
    case RequestStatus::FetchRobots:
        fetchRobots(url);
        break;
    case RequestStatus::Allowed:
        fetchPage(url);
        break;
    }
}

void Crawler::worker()
{
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);

        auto url = getNextUrl(lock);

        std::string actualUrl;

        if (!url.has_value())
            return;


        activeWorkers++;

        lock.unlock();

        processUrl(*url);

        lock.lock();
        activeWorkers--;
        lock.unlock();


        cv_.notify_all();
    }
}

void Crawler::run(const std::string& seedUrl) {
    frontier_.push(seedUrl);

    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "--------------------" << "\n";
    std::cout << "CRAWL START" << "\n";
    std::cout << "--------------------" << "\n";

    std::vector<std::thread> workers;

    for (int i = 0; i < workerCount_; i++) {
        workers.push_back(std::thread(&Crawler::worker, this));
    }

    for (auto& worker : workers) {
        worker.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start
    );

    std::cout << "total: " << sumFetchTime_ << " ms" << "\n";
    std::cout << "--------------------" << "\n";
    std::cout << "CRAWL END, took " << duration.count() << " ms" << "\n";
    std::cout << "--------------------" << "\n";

}

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