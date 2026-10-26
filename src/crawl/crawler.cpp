
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <optional>
#include <iomanip>

#include "crawler.h"

namespace {

    using SteadyClock = std::chrono::steady_clock;

    std::chrono::nanoseconds elapsedSince(
        SteadyClock::time_point start
    ) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            SteadyClock::now() - start
        );
    }

    double toMilliseconds(std::chrono::nanoseconds duration) {
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    // Measures a database operation that returns void.
    template <typename Function>
    void timeDatabaseOperation(
        std::chrono::nanoseconds& totalTime,
        std::size_t& calls,
        Function&& operation
    ) {
        auto start = SteadyClock::now();

        operation();

        totalTime += elapsedSince(start);
        ++calls;
    }

    // Measures a database operation that returns a value.
    template <typename Function>
    auto timeDatabaseQuery(
        std::chrono::nanoseconds& totalTime,
        std::size_t& calls,
        Function&& operation
    ) {
        auto start = SteadyClock::now();

        auto result = operation();

        totalTime += elapsedSince(start);
        ++calls;

        return result;
    }

    void recordLockWait(
        WorkerStats& stats,
        LockSectionStats& section,
        SteadyClock::time_point start
    ) {
        auto duration = elapsedSince(start);

        stats.mutexWaitTime += duration;
        section.wait += duration;
        ++section.acquisitions;
    }

    void recordLockHold(
        WorkerStats& stats,
        LockSectionStats& section,
        SteadyClock::time_point start
    ) {
        auto duration = elapsedSince(start);

        stats.mutexHoldTime += duration;
        section.hold += duration;
    }

    void printTime(
        const std::string& name,
        std::chrono::nanoseconds duration
    ) {
        std::cout << "  " << std::left << std::setw(25) << name
            << ": " << std::right << std::fixed
            << std::setprecision(2)
            << toMilliseconds(duration) << " ms\n";
    }

    void printLockStats(
        const std::string& name,
        const LockSectionStats& stats
    ) {
        std::cout << "  " << std::left << std::setw(25) << name
            << " Wait: " << std::right << std::fixed
            << std::setprecision(2)
            << toMilliseconds(stats.wait)
            << " ms | Hold: "
            << toMilliseconds(stats.hold)
            << " ms | Calls: "
            << stats.acquisitions << '\n';
    }

    void printDatabaseStats(const DatabaseStats& stats)
    {
        std::cout << "\n  Database operations:\n";

        auto printOperation = [](
            const std::string& name,
            std::chrono::nanoseconds duration,
            std::size_t calls
            ) {
                std::cout << "  " << std::left << std::setw(25) << name
                    << ": " << std::right << std::fixed
                    << std::setprecision(2)
                    << toMilliseconds(duration)
                    << " ms | Calls: " << calls;

                if (calls > 0) {
                    std::cout << " | Avg: "
                        << toMilliseconds(duration) / calls
                        << " ms";
                }

                std::cout << '\n';
            };

        printOperation(
            "checkVisited",
            stats.checkVisitedTime,
            stats.checkVisitedCalls
        );

        printOperation(
            "countSaved",
            stats.countSavedTime,
            stats.countSavedCalls
        );

        printOperation(
            "markFetching",
            stats.markFetchingTime,
            stats.markFetchingCalls
        );

        printOperation(
            "markFetched",
            stats.markFetchedTime,
            stats.markFetchedCalls
        );

        printOperation(
            "markFailed",
            stats.markFailedTime,
            stats.markFailedCalls
        );

        printOperation(
            "savePageContent",
            stats.savePageContentTime,
            stats.savePageContentCalls
        );

        printOperation(
            "savePageIndex",
            stats.savePageIndexTime,
            stats.savePageIndexCalls
        );
    }

} // namespace


Crawler::Crawler(
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
)
    : workerCount_(workerCount),
    maxPages_(maxPages),
    workerStats_(
        workerCount > 0
        ? static_cast<std::size_t>(workerCount)
        : 0
    ),
    storage_(storage),
    domainManager_(domainManager),
    clock_(clock),
    httpClient_(httpClient),
    htmlParser_(htmlParser),
    urlProcessor_(urlProcessor),
    pageScorer_(pageScorer),
    indexer_(indexer)
{
}


void Crawler::scheduleRetry(
    const std::string& url,
    WorkerStats& stats
) {
    auto waitStart = SteadyClock::now();
    std::unique_lock<std::mutex> lock(mutex_);

    recordLockWait(stats, stats.scheduleRetryLock, waitStart);

    auto holdStart = SteadyClock::now();

    auto visitedStart = SteadyClock::now();

    bool visited = timeDatabaseQuery(
        stats.database.checkVisitedTime,
        stats.database.checkVisitedCalls,
        [&] { return storage_.checkVisited(url); }
    );

    stats.visitedCheckTime += elapsedSince(visitedStart);
    ++stats.visitedChecks;

    if (visited) {
        recordLockHold(stats, stats.scheduleRetryLock, holdStart);
        return;
    }

    auto readyAt = domainManager_.nextAllowedRequestTime(url);
    frontier_.schedule(url, readyAt);

    recordLockHold(stats, stats.scheduleRetryLock, holdStart);

    lock.unlock();
    cv_.notify_all();
}


std::optional<std::string> Crawler::getNextUrl(
    std::unique_lock<std::mutex>& lock,
    WorkerStats& stats
) {
    // The caller already owns mutex_.
    auto holdStart = SteadyClock::now();

    auto finishHold = [&]() {
        recordLockHold(stats, stats.getNextUrlLock, holdStart);
        };

    while (true) {
        // Read the saved count only once.
        std::size_t saved = savedCount_.load(
            std::memory_order_relaxed
        );

        // A negative maxPages_ means unlimited.
        if (maxPages_ >= 0 &&
            saved >= static_cast<std::size_t>(maxPages_)) {
            finishHold();
            return std::nullopt;
        }

        frontier_.promoteReadyUrls(clock_.now());

        if (auto url = frontier_.tryGetNextUrl()) {
            auto visitedStart = SteadyClock::now();

            bool visited = timeDatabaseQuery(
                stats.database.checkVisitedTime,
                stats.database.checkVisitedCalls,
                [&] { return storage_.checkVisited(*url); }
            );

            stats.visitedCheckTime += elapsedSince(visitedStart);
            ++stats.visitedChecks;

            if (visited) {
                continue;
            }

            finishHold();
            return url;
        }

        // Wait until the earliest delayed URL becomes ready.
        if (auto readyAt = frontier_.nextReadyAt()) {
            finishHold();

            auto waitStart = SteadyClock::now();
            cv_.wait_until(lock, *readyAt);
            stats.frontierWait += elapsedSince(waitStart);

            holdStart = SteadyClock::now();
            continue;
        }

        // No queued URLs. Only terminate if no worker is processing
        // something and no URLs are waiting for robots.txt.
        if (activeWorkers == 0) {
            if (!frontier_.hasRobotsWaiters()) {
                finishHold();
                return std::nullopt;
            }

            finishHold();

            auto waitStart = SteadyClock::now();
            cv_.wait(lock);
            stats.frontierWait += elapsedSince(waitStart);

            holdStart = SteadyClock::now();
            continue;
        }

        finishHold();

        auto waitStart = SteadyClock::now();
        cv_.wait(lock);
        stats.frontierWait += elapsedSince(waitStart);

        holdStart = SteadyClock::now();
    }
}


void Crawler::fetchRobots(
    const std::string& url,
    WorkerStats& stats
) {
    std::string robotsUrl;

    // First critical section: get robots.txt URL.
    {
        auto waitStart = SteadyClock::now();
        std::unique_lock<std::mutex> lock(mutex_);

        recordLockWait(stats, stats.fetchRobotsLock, waitStart);

        auto holdStart = SteadyClock::now();

        robotsUrl = domainManager_.getRobotsUrl(url);

        recordLockHold(stats, stats.fetchRobotsLock, holdStart);
    }

    // HTTP request happens outside the mutex.
    auto fetchStart = SteadyClock::now();
    auto result = httpClient_.fetchUrl(robotsUrl);
    auto fetchDuration = elapsedSince(fetchStart);

    stats.robotsFetch += fetchDuration;

    auto fetchMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            fetchDuration
        );

    // Second critical section: save robots rules and release waiters.
    {
        auto waitStart = SteadyClock::now();
        std::unique_lock<std::mutex> lock(mutex_);

        recordLockWait(stats, stats.fetchRobotsLock, waitStart);

        auto holdStart = SteadyClock::now();

        // A failed robots.txt request is treated as empty rules.
        domainManager_.saveRobotsResult(
            robotsUrl,
            result.success ? result.data : ""
        );

        sumFetchTime_ += fetchMilliseconds.count();

        // Retry the URL that triggered the robots request.
        auto visitedStart = SteadyClock::now();

        bool visited = timeDatabaseQuery(
            stats.database.checkVisitedTime,
            stats.database.checkVisitedCalls,
            [&] { return storage_.checkVisited(url); }
        );

        stats.visitedCheckTime += elapsedSince(visitedStart);
        ++stats.visitedChecks;

        if (!visited) {
            auto pushStart = SteadyClock::now();
            frontier_.push(url);

            stats.frontierPushTime += elapsedSince(pushStart);
            ++stats.frontierPushes;
        }

        // Release URLs waiting for robots.txt for this domain.
        std::string domain = domainManager_.getDomain(url);
        auto waitingUrls = frontier_.releaseRobotsWaiters(domain);

        for (const auto& waitingUrl : waitingUrls) {
            visitedStart = SteadyClock::now();

            visited = timeDatabaseQuery(
                stats.database.checkVisitedTime,
                stats.database.checkVisitedCalls,
                [&] { return storage_.checkVisited(waitingUrl); }
            );

            stats.visitedCheckTime += elapsedSince(visitedStart);
            ++stats.visitedChecks;

            if (!visited) {
                auto pushStart = SteadyClock::now();
                frontier_.push(waitingUrl);

                stats.frontierPushTime += elapsedSince(pushStart);
                ++stats.frontierPushes;
            }
        }

        recordLockHold(stats, stats.fetchRobotsLock, holdStart);
    }

    cv_.notify_all();
}


void Crawler::flushIndexBatch(WorkerStats& stats)
{
    if (pendingIndexBatch_.empty()) {
        return;
    }

    // One SQLite transaction writes all pages currently in the batch.
    auto saveStart = SteadyClock::now();

    timeDatabaseOperation(
        stats.database.savePageIndexTime,
        stats.database.savePageIndexCalls,
        [&] {
            storage_.savePagesIndex(pendingIndexBatch_);
        }
    );

    stats.saveIndexTime += elapsedSince(saveStart);

    // Clear only after the transaction succeeds. If saving throws, the
    // pending pages remain in memory rather than being silently discarded.
    pendingIndexBatch_.clear();
}


void Crawler::fetchPage(
    const std::string& url,
    WorkerStats& stats
) {
    // First critical section: check and mark the URL.
    {
        auto waitStart = SteadyClock::now();
        std::unique_lock<std::mutex> lock(mutex_);

        recordLockWait(stats, stats.fetchPageStartLock, waitStart);

        auto holdStart = SteadyClock::now();

        // Check whether URL has already been visited.
        auto visitedStart = SteadyClock::now();

        bool visited = timeDatabaseQuery(
            stats.database.checkVisitedTime,
            stats.database.checkVisitedCalls,
            [&] { return storage_.checkVisited(url); }
        );

        stats.visitedCheckTime += elapsedSince(visitedStart);
        ++stats.visitedChecks;

        if (visited) {
            recordLockHold(stats, stats.fetchPageStartLock, holdStart);
            return;
        }

        // Mark URL as fetching.
        timeDatabaseOperation(
            stats.database.markFetchingTime,
            stats.database.markFetchingCalls,
            [&] { storage_.markFetching(url); }
        );

        // Save domain request information.
        domainManager_.saveRequest(url);

        recordLockHold(stats, stats.fetchPageStartLock, holdStart);
    }

    // Perform HTTP request without holding the crawler mutex.
    auto fetchStart = SteadyClock::now();
    auto result = httpClient_.fetchUrl(url);
    auto fetchDuration = elapsedSince(fetchStart);

    stats.httpFetch += fetchDuration;

    auto fetchMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            fetchDuration
        );

    // Handle failed requests.
    if (!result.success) {
        auto waitStart = SteadyClock::now();
        std::unique_lock<std::mutex> lock(mutex_);

        recordLockWait(stats, stats.fetchPageFailureLock, waitStart);

        auto holdStart = SteadyClock::now();

        sumFetchTime_ += fetchMilliseconds.count();

        timeDatabaseOperation(
            stats.database.markFailedTime,
            stats.database.markFailedCalls,
            [&] { storage_.markFailed(url); }
        );

        recordLockHold(stats, stats.fetchPageFailureLock, holdStart);

        lock.unlock();
        cv_.notify_all();
        return;
    }

    // Parse HTML outside the global mutex.
    auto stageStart = SteadyClock::now();
    ParsedPage parsedPage = htmlParser_.parse(result.data);
    stats.parseTime += elapsedSince(stageStart);
    ++stats.pagesParsed;

    // Score page relevance outside the global mutex.
    stageStart = SteadyClock::now();

    double relevance = pageScorer_.score(
        parsedPage.title,
        parsedPage.text
    );

    stats.scoringTime += elapsedSince(stageStart);

    // Decide whether to index the page.
    bool shouldIndex = crawlPolicy_.shouldIndex(
        parsedPage,
        relevance
    );

    // Generate the page index outside the global mutex.
    PageIndex pageIndex;

    if (shouldIndex) {
        stageStart = SteadyClock::now();
        pageIndex = indexer_.indexPage(parsedPage.text);
        stats.indexTime += elapsedSince(stageStart);
    }

    // Get a snapshot of the frontier size.
    std::size_t currentFrontierSize;

    {
        auto waitStart = SteadyClock::now();
        std::unique_lock<std::mutex> lock(mutex_);

        recordLockWait(stats, stats.fetchPageSnapshotLock, waitStart);

        auto holdStart = SteadyClock::now();

        currentFrontierSize = frontier_.frontierSize();

        recordLockHold(stats, stats.fetchPageSnapshotLock, holdStart);
    }

    // Process URLs outside the global mutex.
    std::vector<std::string> links;

    if (crawlPolicy_.shouldExplore(
        relevance,
        currentFrontierSize
    )) {
        stageStart = SteadyClock::now();
        links = urlProcessor_.process(parsedPage.links, url);
        stats.urlProcessingTime += elapsedSince(stageStart);
    }

    // Final critical section: save page and enqueue links.
    {
        auto waitStart = SteadyClock::now();
        std::unique_lock<std::mutex> lock(mutex_);

        recordLockWait(stats, stats.fetchPageSaveLock, waitStart);

        auto holdStart = SteadyClock::now();

        sumFetchTime_ += fetchMilliseconds.count();

        // Mark page as fetched.
        timeDatabaseOperation(
            stats.database.markFetchedTime,
            stats.database.markFetchedCalls,
            [&] { storage_.markFetched(url); }
        );

        savedCount_.fetch_add(
            1,
            std::memory_order_relaxed
        );

        // Save page content.
        stageStart = SteadyClock::now();

        timeDatabaseOperation(
            stats.database.savePageContentTime,
            stats.database.savePageContentCalls,
            [&] {
                storage_.savePageContent(
                    url,
                    parsedPage.title,
                    parsedPage.description,
                    parsedPage.text
                );
            }
        );

        stats.saveContentTime += elapsedSince(stageStart);

        // Queue the generated page index. Every ten indexed pages are
        // written together in one SQLite transaction.
        if (shouldIndex) {
            pendingIndexBatch_.emplace_back(url, std::move(pageIndex));
            ++stats.pagesIndexed;

            if (pendingIndexBatch_.size() >= INDEX_BATCH_SIZE) {
                flushIndexBatch(stats);
            }
        }

        // Check and enqueue extracted links.
        for (const std::string& link : links) {
            ++stats.linksProcessed;

            stageStart = SteadyClock::now();

            bool linkVisited = timeDatabaseQuery(
                stats.database.checkVisitedTime,
                stats.database.checkVisitedCalls,
                [&] { return storage_.checkVisited(link); }
            );

            stats.visitedCheckTime += elapsedSince(stageStart);
            ++stats.visitedChecks;

            if (!linkVisited) {
                stageStart = SteadyClock::now();
                frontier_.push(link);

                stats.frontierPushTime += elapsedSince(stageStart);
                ++stats.frontierPushes;
            }
        }

        recordLockHold(stats, stats.fetchPageSaveLock, holdStart);
    }

    cv_.notify_all();
}


void Crawler::processUrl(
    std::string& url,
    WorkerStats& stats
) {
    RequestStatus status;

    {
        auto waitStart = SteadyClock::now();
        std::unique_lock<std::mutex> lock(mutex_);

        recordLockWait(stats, stats.processUrlLock, waitStart);

        auto holdStart = SteadyClock::now();

        status = domainManager_.check(url);

        if (status == RequestStatus::WaitForRobots) {
            std::string domain = domainManager_.getDomain(url);
            frontier_.waitForRobots(domain, url);
        }

        recordLockHold(stats, stats.processUrlLock, holdStart);
    }

    switch (status) {
    case RequestStatus::Disallowed:
        ++stats.disallowed;
        break;

    case RequestStatus::WaitForRobots:
        ++stats.waitForRobots;
        break;

    case RequestStatus::RateLimited:
        ++stats.rateLimited;
        scheduleRetry(url, stats);
        break;

    case RequestStatus::FetchRobots:
        ++stats.fetchRobots;
        fetchRobots(url, stats);
        break;

    case RequestStatus::Allowed:
        ++stats.allowed;
        fetchPage(url, stats);
        break;
    }
}


void Crawler::worker(int workerId)
{
    WorkerStats& stats =
        workerStats_[static_cast<std::size_t>(workerId)];

    while (true) {
        // The initial lock acquisition is measured as Get next URL.
        auto waitStart = SteadyClock::now();
        std::unique_lock<std::mutex> lock(mutex_);

        recordLockWait(stats, stats.getNextUrlLock, waitStart);

        auto url = getNextUrl(lock, stats);

        if (!url.has_value()) {
            lock.unlock();

            std::cout << "[WORKER " << workerId
                << "] Exiting.\n";
            return;
        }

        // The lock is still held after getNextUrl returns.
        auto holdStart = SteadyClock::now();

        ++activeWorkers;
        ++stats.urlsProcessed;

        recordLockHold(stats, stats.getNextUrlLock, holdStart);

        lock.unlock();

        auto processStart = SteadyClock::now();
        processUrl(*url, stats);
        stats.processing += elapsedSince(processStart);

        // Decrement active worker count.
        waitStart = SteadyClock::now();
        lock.lock();

        recordLockWait(stats, stats.workerBookkeepingLock, waitStart);

        holdStart = SteadyClock::now();

        --activeWorkers;

        recordLockHold(stats, stats.workerBookkeepingLock, holdStart);

        lock.unlock();
        cv_.notify_all();
    }
}


void Crawler::run(const std::string& seedUrl)
{
    storage_.loadFrontier(frontier_);

    savedCount_.store(
        storage_.countSaved(),
        std::memory_order_relaxed
    );

    {
        std::lock_guard<std::mutex> lock(mutex_);
        frontier_.push(seedUrl);
    }

    auto start = SteadyClock::now();

    std::cout << "--------------------\n";
    std::cout << "CRAWL START\n";
    std::cout << "--------------------\n";

    std::vector<std::thread> workers;

    if (workerCount_ > 0) {
        workers.reserve(static_cast<std::size_t>(workerCount_));
    }

    for (int i = 0; i < workerCount_; ++i) {
        workers.emplace_back(&Crawler::worker, this, i);
    }

    for (auto& workerThread : workers) {
        workerThread.join();
    }

    // Workers have stopped, so it is safe to write the final batch, which
    // may contain fewer than INDEX_BATCH_SIZE pages.
    if (!pendingIndexBatch_.empty() && !workerStats_.empty()) {
        // The final flush happens on the main thread; attribute its timing
        // to worker 0 so it is still included in the existing statistics.
        flushIndexBatch(workerStats_.front());
    }

    auto totalDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            SteadyClock::now() - start
        );

    std::cout << "\n====================\n";
    std::cout << "WORKER STATISTICS\n";
    std::cout << "====================\n";

    for (std::size_t i = 0; i < workerStats_.size(); ++i) {
        const WorkerStats& stats = workerStats_[i];

        std::cout << "\nWorker " << i << '\n';

        printTime("Frontier wait", stats.frontierWait);
        printTime("HTTP fetch", stats.httpFetch);
        printTime("Robots fetch", stats.robotsFetch);
        printTime("Processing", stats.processing);

        std::cout << "\n  Stage timings:\n";

        printTime("Parse", stats.parseTime);
        printTime("Scoring", stats.scoringTime);
        printTime("Save content", stats.saveContentTime);
        printTime("Index", stats.indexTime);
        printTime("Save index", stats.saveIndexTime);
        printTime("URL processing", stats.urlProcessingTime);
        printTime("Visited checks", stats.visitedCheckTime);
        printTime("Frontier pushes", stats.frontierPushTime);

        // Individual database operation timings.
        printDatabaseStats(stats.database);

        std::cout << "\n  Aggregate mutex timings:\n";

        printTime("Mutex wait", stats.mutexWaitTime);
        printTime("Mutex hold", stats.mutexHoldTime);

        std::cout << "\n  Mutex breakdown:\n";

        printLockStats("Get next URL", stats.getNextUrlLock);
        printLockStats("Process URL", stats.processUrlLock);
        printLockStats("Fetch page start", stats.fetchPageStartLock);
        printLockStats("Fetch page snapshot", stats.fetchPageSnapshotLock);
        printLockStats("Fetch page save", stats.fetchPageSaveLock);
        printLockStats("Fetch page failure", stats.fetchPageFailureLock);
        printLockStats("Fetch robots", stats.fetchRobotsLock);
        printLockStats("Schedule retry", stats.scheduleRetryLock);
        printLockStats("Worker bookkeeping", stats.workerBookkeepingLock);

        std::cout << "\n  Counters:\n";

        std::cout << "  URLs processed: "
            << stats.urlsProcessed << '\n';
        std::cout << "  Pages parsed: "
            << stats.pagesParsed << '\n';
        std::cout << "  Pages indexed: "
            << stats.pagesIndexed << '\n';
        std::cout << "  Links processed: "
            << stats.linksProcessed << '\n';
        std::cout << "  Visited checks: "
            << stats.visitedChecks << '\n';
        std::cout << "  Frontier pushes: "
            << stats.frontierPushes << '\n';

        std::cout << "\n  Request statuses:\n";

        std::cout << "  Allowed: "
            << stats.allowed << '\n';
        std::cout << "  Rate limited: "
            << stats.rateLimited << '\n';
        std::cout << "  Wait for robots: "
            << stats.waitForRobots << '\n';
        std::cout << "  Fetch robots: "
            << stats.fetchRobots << '\n';
        std::cout << "  Disallowed: "
            << stats.disallowed << '\n';
    }

    std::cout << "\n====================\n";
    std::cout << "CRAWL SUMMARY\n";
    std::cout << "====================\n";

    std::cout << "Saving frontier ("
        << frontier_.frontierSize()
        << " entries)\n";

    storage_.saveFrontier(frontier_);

    std::cout << "Total fetch time: "
        << sumFetchTime_ << " ms\n";

    std::cout << "Crawl duration: "
        << totalDuration.count() << " ms\n";

    std::cout << "Frontier size: "
        << frontier_.frontierSize() << '\n';

    std::cout << "Domain manager size: "
        << domainManager_.domains_.size() << '\n';

    std::cout << "--------------------\n";
    std::cout << "CRAWL END\n";
    std::cout << "--------------------\n";
}
