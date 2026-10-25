#pragma once

#include <string>
#include <mutex>
#include <optional>
#include <chrono>
#include <vector>
#include <condition_variable>
#include <cstddef>

#include "storage/storage.h"
#include "index/indexer.h"
#include "index/index.h"
#include "domainManager.h"
#include "utils/systemClock.h"
#include "net/httpClient.h"
#include "processing/htmlParser.h"
#include "processing/urlProcessor.h"
#include "processing/pageScorer.h"
#include "frontier.h"
#include "crawlPolicy.h"

struct WorkerStats {
    // All timing fields use nanoseconds.
    std::chrono::nanoseconds frontierWait{ 0 };
    std::chrono::nanoseconds httpFetch{ 0 };
    std::chrono::nanoseconds robotsFetch{ 0 };
    std::chrono::nanoseconds processing{ 0 };

    std::chrono::nanoseconds parseTime{ 0 };
    std::chrono::nanoseconds scoringTime{ 0 };
    std::chrono::nanoseconds saveContentTime{ 0 };
    std::chrono::nanoseconds indexTime{ 0 };
    std::chrono::nanoseconds saveIndexTime{ 0 };
    std::chrono::nanoseconds urlProcessingTime{ 0 };
    std::chrono::nanoseconds visitedCheckTime{ 0 };
    std::chrono::nanoseconds frontierPushTime{ 0 };

    std::chrono::nanoseconds mutexWaitTime{ 0 };
    std::chrono::nanoseconds mutexHoldTime{ 0 };

    std::size_t urlsProcessed = 0;
    std::size_t pagesParsed = 0;
    std::size_t pagesIndexed = 0;
    std::size_t linksProcessed = 0;
    std::size_t visitedChecks = 0;
    std::size_t frontierPushes = 0;

    std::size_t allowed = 0;
    std::size_t rateLimited = 0;
    std::size_t waitForRobots = 0;
    std::size_t fetchRobots = 0;
    std::size_t disallowed = 0;
};

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
    std::optional<std::string> getNextUrl(
        std::unique_lock<std::mutex>& lock,
        WorkerStats& stats
    );

    void processUrl(std::string& url, WorkerStats& stats);
    void worker(int workerId);

    void fetchRobots(const std::string& url, WorkerStats& stats);
    void fetchPage(const std::string& url, WorkerStats& stats);
    void scheduleRetry(const std::string& url, WorkerStats& stats);

    int workerCount_ = 0;
    int activeWorkers = 0;
    int maxPages_;
    long long sumFetchTime_ = 0;

    std::vector<WorkerStats> workerStats_;

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