#pragma once

#include<string>
#include<chrono>
#include<unordered_map>

enum class RequestStatus {
	Allowed,
	Disallowed,
	FetchRobots,
	Wait
};

class DomainManager{
public:
	RequestStatus check(const std::string& url);

	bool isAllowed(const std::string& url);
	void saveRequest(const std::string& url);
	void saveRobotsResult(const std::string& url, const std::string& text);
	std::string getRobotsUrl(const std::string& url);
	bool sameDomain(const std::string& url1, const std::string& url2);

private:
	std::string getDomain(const std::string& url);
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