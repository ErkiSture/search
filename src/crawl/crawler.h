#pragma once

#include <string>
#include <mutex>
#include <optional>
#include <chrono>
#include <vector>
#include <condition_variable>
#include <cstddef>
#include <atomic>
#include <utility>

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


struct LockSectionStats {
    std::chrono::nanoseconds wait{};
    std::chrono::nanoseconds hold{};
    std::size_t acquisitions = 0;
};


struct DatabaseStats {
    std::chrono::nanoseconds checkVisitedTime{};
    std::chrono::nanoseconds countSavedTime{};
    std::chrono::nanoseconds markFetchingTime{};
    std::chrono::nanoseconds markFetchedTime{};
    std::chrono::nanoseconds markFailedTime{};
    std::chrono::nanoseconds savePageContentTime{};
    std::chrono::nanoseconds savePageIndexTime{};

    std::size_t checkVisitedCalls = 0;
    std::size_t countSavedCalls = 0;
    std::size_t markFetchingCalls = 0;
    std::size_t markFetchedCalls = 0;
    std::size_t markFailedCalls = 0;
    std::size_t savePageContentCalls = 0;
    std::size_t savePageIndexCalls = 0;
};


struct WorkerStats {
    DatabaseStats database;

    std::chrono::nanoseconds mutexWaitTime{};
    std::chrono::nanoseconds mutexHoldTime{};

    std::chrono::nanoseconds frontierWait{};
    std::chrono::nanoseconds httpFetch{};
    std::chrono::nanoseconds robotsFetch{};
    std::chrono::nanoseconds processing{};

    std::chrono::nanoseconds parseTime{};
    std::chrono::nanoseconds scoringTime{};
    std::chrono::nanoseconds saveContentTime{};
    std::chrono::nanoseconds indexTime{};
    std::chrono::nanoseconds saveIndexTime{};
    std::chrono::nanoseconds urlProcessingTime{};
    std::chrono::nanoseconds visitedCheckTime{};
    std::chrono::nanoseconds frontierPushTime{};

    LockSectionStats getNextUrlLock;
    LockSectionStats processUrlLock;
    LockSectionStats fetchPageStartLock;
    LockSectionStats fetchPageSnapshotLock;
    LockSectionStats fetchPageSaveLock;
    LockSectionStats fetchPageFailureLock;
    LockSectionStats fetchRobotsLock;
    LockSectionStats scheduleRetryLock;
    LockSectionStats workerBookkeepingLock;

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
    static constexpr std::size_t INDEX_BATCH_SIZE = 10;

    std::optional<std::string> getNextUrl(
        std::unique_lock<std::mutex>& lock,
        WorkerStats& stats
    );

    void processUrl(std::string& url, WorkerStats& stats);
    void worker(int workerId);

    void fetchRobots(
        const std::string& url,
        WorkerStats& stats
    );

    void fetchPage(
        const std::string& url,
        WorkerStats& stats
    );

    void flushIndexBatch(WorkerStats& stats);

    void scheduleRetry(
        const std::string& url,
        WorkerStats& stats
    );

    int workerCount_ = 0;
    int activeWorkers = 0;
    int maxPages_;
    long long sumFetchTime_ = 0;

    std::atomic<std::size_t> savedCount_{ 0 };

    std::vector<WorkerStats> workerStats_;

    // Accessed only while mutex_ is held, except the final flush after
    // all worker threads have joined.
    std::vector<std::pair<std::string, PageIndex>> pendingIndexBatch_;

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
