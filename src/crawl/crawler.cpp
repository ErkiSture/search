#include <string>
#include <iostream>
#include <queue>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <optional>

#include "storage/storage.h"
#include "index/indexer.h"
#include "crawler.h"
#include "domainManager.h"
#include "utils/systemClock.h"
#include "net/httpClient.h"
#include "net/curlHttpClient.h"
#include "utils/isEnglish.h"
#include "Frontier.h"

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
    : storage_(storage),
    domainManager_(domainManager),
    clock_(clock),
    httpClient_(httpClient),
    htmlParser_(htmlParser),
    urlProcessor_(urlProcessor),
    pageScorer_(pageScorer),
    indexer_(indexer),
    maxPages_(maxPages),
    workerCount_(workerCount)
{
}

void Crawler::logFetchTime(
    const std::string& url,
    std::chrono::steady_clock::time_point start
) {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock_.now() - start
    );

    std::cout << "fetched "
        << duration.count() << " ms "
        << url << "\n";
}

void Crawler::scheduleRetry(const std::string& url)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (storage_.checkVisited(url)) {
            return;
        }

        auto readyAt = domainManager_.nextAllowedRequestTime(url);
        frontier_.schedule(url, readyAt);
    }

    cv_.notify_all();
}

std::optional<std::string> Crawler::getNextUrl(
    std::unique_lock<std::mutex>& lock)
{
    while (true) {
        std::cout << "COUNT SAVED: " << storage_.countSaved()
            << " ACTIVE WORKERS: " << activeWorkers
            << std::endl;

        if (storage_.countSaved() >= static_cast<size_t>(maxPages_)) {
            return std::nullopt;
        }

        frontier_.promoteReadyUrls(clock_.now());

        if (auto url = frontier_.tryGetNextUrl()) {
            if (storage_.checkVisited(*url)) {
                continue;
            }

            return url;
        }

        // Wait until the earliest delayed URL becomes ready.
        if (auto readyAt = frontier_.nextReadyAt()) {
            cv_.wait_until(lock, *readyAt);
            continue;
        }

        // No queued URLs. Only terminate if no worker is processing
        // something and no URLs are waiting for robots.txt.
        if (activeWorkers == 0) {
            if (!frontier_.hasRobotsWaiters()) {
                return std::nullopt;
            }

            // Robots fetch is still in progress. Wait for its result.
            cv_.wait(lock);
            continue;
        }

        cv_.wait(lock);
    }
}

void Crawler::fetchRobots(const std::string& url)
{
    std::string robotsUrl;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        robotsUrl = domainManager_.getRobotsUrl(url);
    }

    auto start = clock_.now();
    auto result = httpClient_.fetchUrl(robotsUrl);

    //logFetchTime(robotsUrl, start);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock_.now() - start
    );

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // A failed robots.txt request is treated as empty rules.
        domainManager_.saveRobotsResult(
            robotsUrl,
            result.success ? result.data : ""
        );

        sumFetchTime_ += duration.count();

        // Retry the URL that triggered the robots request.
        if (!storage_.checkVisited(url)) {
            frontier_.push(url);
        }

        // Release URLs waiting for robots.txt for this domain.
        std::string domain = domainManager_.getDomain(url);
        auto waitingUrls = frontier_.releaseRobotsWaiters(domain);

        for (const auto& waitingUrl : waitingUrls) {
            if (!storage_.checkVisited(waitingUrl)) {
                frontier_.push(waitingUrl);
            }
        }
    }

    cv_.notify_all();
}

void Crawler::fetchPage(const std::string& url)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (storage_.checkVisited(url)) {
            return;
        }
        else {
            storage_.markFetching(url);
        }

        domainManager_.saveRequest(url);
    }

    auto start = clock_.now();
    auto result = httpClient_.fetchUrl(url);

    //logFetchTime(url, start);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock_.now() - start
    );

    {
        std::lock_guard<std::mutex> lock(mutex_);

        sumFetchTime_ += duration.count();

        if (!result.success) {
            storage_.markFailed(url);
            cv_.notify_all();
            return;
        }

        ParsedPage parsedPage = htmlParser_.parse(result.data);

        double relevance = pageScorer_.score(parsedPage.title, parsedPage.text);
        std::cout << "RELEVANCE: " << relevance << std::endl;

        storage_.markFetched(url);
        storage_.savePageContent(url, parsedPage.title, parsedPage.description, parsedPage.text);

        if (relevance >= 30.0 && isEnglish(parsedPage.text)) {
            PageIndex pageIndex = indexer_.indexPage(parsedPage.text);
            storage_.savePageIndex(url, pageIndex);
        }

        if (relevance >= 65 || frontier_.frontierSize() < 1000) {
            std::vector<std::string> links = urlProcessor_.process(parsedPage.links, url);
            for (const std::string& link : links) {
                if (!storage_.checkVisited(link)) {
                    frontier_.push(link);
                }
            }
        }
    }

    cv_.notify_all();
}

void Crawler::processUrl(std::string& url)
{
    RequestStatus status;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        status = domainManager_.check(url);

        if (status == RequestStatus::WaitForRobots) {
            std::string domain = domainManager_.getDomain(url);
            frontier_.waitForRobots(domain, url);
        }
    }

    switch (status) {
    case RequestStatus::Disallowed:
        break;

    case RequestStatus::WaitForRobots:
        // fetchRobots() will requeue this URL when robots.txt finishes.
        break;

    case RequestStatus::RateLimited:
        scheduleRetry(url);
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

        if (!url.has_value()) {
            return;
        }

        // This worker now owns a URL and is processing it.
        ++activeWorkers;

        lock.unlock();

        processUrl(*url);

        lock.lock();
        --activeWorkers;
        lock.unlock();

        cv_.notify_all();
    }
}

void Crawler::run(const std::string& seedUrl)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frontier_.push(seedUrl);
    }

    auto start = std::chrono::steady_clock::now();

    std::cout << "--------------------\n";
    std::cout << "CRAWL START\n";
    std::cout << "--------------------\n";

    std::vector<std::thread> workers;

    for (int i = 0; i < workerCount_; ++i) {
        workers.emplace_back(&Crawler::worker, this);
    }

    for (auto& workerThread : workers) {
        workerThread.join();
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    );

    std::cout << "total: " << sumFetchTime_ << " ms\n";
    std::cout << "--------------------\n";
    std::cout << "CRAWL END, took " << duration.count() << " ms\n";
    std::cout << "Frontier size: " << frontier_.frontierSize() << "\n";
    std::cout << "Domain manager size: " << domainManager_.domains_.size() << "\n";
    std::cout << "--------------------\n";
}