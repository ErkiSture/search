#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <queue>
#include <string>
#include <vector>
#include <unordered_map>

struct DelayedUrl {
    std::chrono::steady_clock::time_point readyAt;
    std::string url;
};

struct CompareReadyTime {
    bool operator()(const DelayedUrl& a, const DelayedUrl& b) const {
        return a.readyAt > b.readyAt;
    }
};

class Frontier {
public:
    void push(const std::string& url);
    void schedule(
        const std::string& url,
        std::chrono::steady_clock::time_point readyAt
    );

    void promoteReadyUrls(std::chrono::steady_clock::time_point now);
    std::optional<std::string> tryPop();
    std::optional<std::string> tryGetNextUrl();
    std::optional<std::chrono::steady_clock::time_point> nextReadyAt() const;

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
    std::priority_queue<
        DelayedUrl,
        std::vector<DelayedUrl>,
        CompareReadyTime
    > delayed_;

    std::unordered_map<std::string, std::vector<std::string>>
        waitingForRobots_;
};