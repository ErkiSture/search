#include "frontier.h"

void Frontier::push(const std::string& url)
{
    frontier_.push(url);
}

void Frontier::schedule(
    const std::string& domain,
    const std::string& url,
    std::chrono::steady_clock::time_point readyAt)
{
    domainWaiting_[domain].push(url);

    if (domainScheduled_.insert(domain).second) {
        delayedDomains_.push({ readyAt, domain });
    }
}

void Frontier::promoteReadyUrls(
    std::chrono::steady_clock::time_point now)
{
    while (!delayedDomains_.empty() &&
        delayedDomains_.top().readyAt <= now)
    {
        auto delayed = delayedDomains_.top();
        delayedDomains_.pop();

        const std::string& domain = delayed.domain;

        domainScheduled_.erase(domain);

        auto& urls = domainWaiting_[domain];

        if (!urls.empty()) {
            frontier_.push(urls.front());
            urls.pop();
        }

        if (urls.empty()) {
            domainWaiting_.erase(domain);
        }
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
    if (delayedDomains_.empty()) {
        return std::nullopt;
    }

    return delayedDomains_.top().readyAt;
}

void Frontier::scheduleNextForDomain(
    const std::string& domain,
    std::chrono::steady_clock::time_point readyAt)
{
    auto it = domainWaiting_.find(domain);

    if (it == domainWaiting_.end() || it->second.empty()) {
        return;
    }

    if (domainScheduled_.insert(domain).second) {
        delayedDomains_.push({ readyAt, domain });
    }
}

bool Frontier::empty() const
{
    return frontier_.empty() && delayedDomains_.empty()
        && domainWaiting_.empty();
}

std::size_t Frontier::frontierSize() const
{
    return frontier_.size();
}

std::size_t Frontier::delayedSize() const
{
    return delayedDomains_.size();
}

std::size_t Frontier::robotsWaitingSize() const
{
    return waitingForRobots_.size();
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
