#include "frontier.h"

void Frontier::push(const std::string& url)
{
    frontier_.push(url);
}

void Frontier::schedule(
    const std::string& url,
    std::chrono::steady_clock::time_point readyAt)
{
    delayed_.push({ readyAt, url });
}

void Frontier::promoteReadyUrls(
    std::chrono::steady_clock::time_point now)
{
    while (!delayed_.empty() && delayed_.top().readyAt <= now) {
        frontier_.push(delayed_.top().url);
        delayed_.pop();
    }
}

std::optional<std::string> Frontier::tryPop()
{
    if (frontier_.empty()) {
        return std::nullopt;
    }

    std::string url = frontier_.front();
    frontier_.pop();

    return url;
}

std::optional<std::string> Frontier::tryGetNextUrl()
{
    return tryPop();
}

std::optional<std::chrono::steady_clock::time_point>
Frontier::nextReadyAt() const
{
    if (delayed_.empty()) {
        return std::nullopt;
    }

    return delayed_.top().readyAt;
}

bool Frontier::empty() const
{
    return frontier_.empty();
}

std::size_t Frontier::frontierSize() const
{
    return frontier_.size();
}

std::size_t Frontier::delayedSize() const
{
    return delayed_.size();
}

void Frontier::waitForRobots(
    const std::string& domain,
    const std::string& url)
{
    waitingForRobots_[domain].push_back(url);
}

std::vector<std::string> Frontier::releaseRobotsWaiters(
    const std::string& domain)
{
    auto it = waitingForRobots_.find(domain);

    if (it == waitingForRobots_.end()) {
        return {};
    }

    auto urls = std::move(it->second);
    waitingForRobots_.erase(it);

    return urls;
}

bool Frontier::hasRobotsWaiters() const
{
    return !waitingForRobots_.empty();
}
