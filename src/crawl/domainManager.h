#pragma once

#include<string>
#include<chrono>
#include<unordered_map>
#include"utils/Clock.h"

enum class RequestStatus {
	Allowed,
	Disallowed,
	FetchRobots,
	Wait,
	WaitForRobots,
	RateLimited
};

class DomainManager{
public:
	explicit DomainManager(
		Clock& clock,
		std::chrono::milliseconds crawlDelay =
		std::chrono::milliseconds(500));

	RequestStatus check(const std::string& url);
	bool isAllowed(const std::string& url);
	void saveRequest(const std::string& url);
	void saveRobotsResult(const std::string& url, const std::string& text);
	std::string getRobotsUrl(const std::string& url);
	bool sameDomain(const std::string& url1, const std::string& url2);
	std::string getDomain(const std::string& url) const;

	std::chrono::steady_clock::time_point
		nextAllowedRequestTime(const std::string& url) const;

private:
	Clock& clock_;
	std::chrono::milliseconds crawlDelay_;

	struct DomainState {
		bool loadedRobotsTxt;
		bool loadingRobotsTxt = false;

		std::vector<std::string> allowed;
		std::vector<std::string> disallowed;

		std::chrono::milliseconds crawlDelay;
		std::chrono::steady_clock::time_point lastRequest;
	};

	std::unordered_map<std::string, DomainState> domains_;
};