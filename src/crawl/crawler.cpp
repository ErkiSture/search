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
#include "parsing/htmlParser.h"
#include "parsing/linkExtractor.h"
#include "crawler.h"
#include "domainManager.h"
#include "utils/systemClock.h"
#include "net/httpClient.h"
#include "net/curlHttpClient.h"

Crawler::Crawler(
    Storage& storage,
    DomainManager& domainManager,
    SystemClock& clock,
    HttpClient& httpClient,
    int maxPages,
    int workerCount
)
    : storage_(storage),
    domainManager_(domainManager),
    clock_(clock),
    httpClient_(httpClient),
    maxPages_(maxPages),
    workerCount_(workerCount)
{
    visited_ = storage.loadVisitedUrls();
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

// Caller must hold mutex_.
void Crawler::promoteReadyUrls()
{
    auto now = clock_.now();

    while (!delayed_.empty() && delayed_.top().readyAt <= now) {
        frontier_.push(delayed_.top().url);
        delayed_.pop();
    }
}

void Crawler::scheduleRetry(const std::string& url)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (visited_.count(url)) {
            return;
        }

        auto readyAt = domainManager_.nextAllowedRequestTime(url);
        delayed_.push({ readyAt, url });
    }

    cv_.notify_all();
}

std::optional<std::string> Crawler::getNextUrl(
    std::unique_lock<std::mutex>& lock
) {
    while (true) {
        if (visited_.size() >= static_cast<size_t>(maxPages_)) {
            return std::nullopt;
        }

        promoteReadyUrls();

        if (!frontier_.empty()) {
            std::string url = frontier_.front();
            frontier_.pop();

            if (visited_.count(url)) {
                continue;
            }

            return url;
        }

        // Wait until the earliest delayed URL becomes ready.
        if (!delayed_.empty()) {
            cv_.wait_until(lock, delayed_.top().readyAt);
            continue;
        }

        // No queued URLs. Only terminate if no worker is processing
        // something and no URLs are waiting for robots.txt.
        if (activeWorkers == 0) {
            if (waitingForRobots_.empty()) {
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

    logFetchTime(robotsUrl, start);

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
        if (!visited_.count(url)) {
            frontier_.push(url);
        }

        // Release URLs waiting for robots.txt for this domain.
        std::string domain = domainManager_.getDomain(url);
        auto it = waitingForRobots_.find(domain);

        if (it != waitingForRobots_.end()) {
            for (const auto& waitingUrl : it->second) {
                if (!visited_.count(waitingUrl)) {
                    frontier_.push(waitingUrl);
                }
            }

            waitingForRobots_.erase(it);
        }
    }

    cv_.notify_all();
}

void Crawler::fetchPage(const std::string& url)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Avoid duplicate page fetches.
        if (visited_.count(url)) {
            return;
        }

        // Mark visited before releasing the lock so another worker
        // cannot fetch the same URL simultaneously.
        visited_.insert(url);

        // Reserve the domain's request time before fetching.
        domainManager_.saveRequest(url);
    }

    auto start = clock_.now();
    auto result = httpClient_.fetchUrl(url);

    logFetchTime(url, start);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock_.now() - start
    );

    {
        std::lock_guard<std::mutex> lock(mutex_);

        sumFetchTime_ += duration.count();

        if (!result.success) {
            cv_.notify_all();
            return;
        }

        std::string text = strip_html(result.data);
        storage_.savePage(url, text);

        for (const std::string& link : extract_links(result.data, url)) {
            if (!visited_.count(link)) {
                frontier_.push(link);
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
            waitingForRobots_[domain].push_back(url);
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
    std::cout << "--------------------\n";
}