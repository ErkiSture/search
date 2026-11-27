#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <queue>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct DelayedUrl {
    std::chrono::steady_clock::time_point readyAt;
    std::string url;
};

struct CompareReadyTime {
    bool operator()(const DelayedUrl& a, const DelayedUrl& b) const {
        return a.readyAt > b.readyAt;
    }
};

struct DelayedDomain {
    std::chrono::steady_clock::time_point readyAt;
    std::string domain;
};

struct CompareDomainReadyTime {
    bool operator()(const DelayedDomain& a, const DelayedDomain& b) const {
        return a.readyAt > b.readyAt;
    }
};

class Frontier {
public:
    void push(const std::string& url);
    void schedule(
        const std::string& domain,
        const std::string& url,
        std::chrono::steady_clock::time_point readyAt);

    void promoteReadyUrls(std::chrono::steady_clock::time_point now);
    std::optional<std::string> tryPop();
    std::optional<std::string> tryGetNextUrl();
    std::optional<std::chrono::steady_clock::time_point> nextReadyAt() const;

    void scheduleNextForDomain(
        const std::string& domain,
        std::chrono::steady_clock::time_point readyAt);

    bool empty() const;
    std::size_t frontierSize() const;
    std::size_t delayedSize() const;
    std::size_t robotsWaitingSize() const;

    void waitForRobots(const std::string& domain, const std::string& url);
    std::vector<std::string> releaseRobotsWaiters(
        const std::string& domain
    );
    bool hasRobotsWaiters() const;

    std::queue<std::string> frontier_;

private:
    std::unordered_map<std::string, std::queue<std::string>>
        domainWaiting_;

    std::priority_queue<
        DelayedDomain,
        std::vector<DelayedDomain>,
        CompareDomainReadyTime
    > delayedDomains_;

    std::unordered_set<std::string> domainScheduled_;

    std::unordered_map<std::string, std::vector<std::string>>
        waitingForRobots_;
};