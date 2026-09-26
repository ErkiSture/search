#include"domainManager.h"
#include<string>
#include<iostream>
#include"robotsTxtParser.h"

int DEFAULT_DOMAIN_RATE_LIMIT_MS = 1000;

/*
* Looks of the domain status for the given url and returns a status describing 
* if the url should be crawled. Alternatively if robots.txt should be fetched
*/
RequestStatus DomainManager::check(const std::string& url)
{
	std::string domain = getDomain(url);

	auto it = domains_.find(domain);

	if (it == domains_.end()) {

		domains_[domain] = {
			false,
			true,
			{},
			{},
			std::chrono::milliseconds(DEFAULT_DOMAIN_RATE_LIMIT_MS),
			std::chrono::steady_clock::now()
		};

		return RequestStatus::FetchRobots;
	}

	auto& state = it->second;

	if (!state.loadedRobotsTxt) {

		if (state.loadingRobotsTxt) {
			return RequestStatus::Wait;
		}

		state.loadingRobotsTxt = true;
		return RequestStatus::FetchRobots;
	}

	if (!isAllowed(url)) {
		return RequestStatus::Disallowed;
	}

	if (std::chrono::steady_clock::now() - state.lastRequest < state.crawlDelay) {
		return RequestStatus::Wait;
	}

	return RequestStatus::Allowed;
}

/*
* Checks if the given url is allowed to be crawled according to the domains robots.txt rules
*/
bool DomainManager::isAllowed(const std::string& url)
{
	std::string domain = getDomain(url);

	auto it = domains_.find(domain);

	if (it == domains_.end() || !it->second.loadedRobotsTxt) {
		return false;
	}

	const auto& state = it->second;

	size_t pathStart = url.find('/', domain.length());

	std::string path = "/";

	if (pathStart != std::string::npos) {
		path = url.substr(pathStart);
	}

	size_t longestMatch = 0;
	bool allowed = true;

	// Check Disallow rules
	for (const auto& rule : state.disallowed) {
		// Empty Disallow means nothing is disallowed
		if (rule.empty()) continue;

		if (path.compare(0, rule.length(), rule) == 0 &&
			rule.length() > longestMatch) {

			longestMatch = rule.length();
			allowed = false;
		}
	}

	// Check Allow rules
	for (const auto& rule : state.allowed) {
		if (rule.empty()) continue;

		if (path.compare(0, rule.length(), rule) == 0 &&
			rule.length() >= longestMatch) {

			longestMatch = rule.length();
			allowed = true;
		}
	}

	return allowed;
}

/*
* Updates the time for latest request on the given urls domain
*/
void DomainManager::saveRequest(const std::string& url)
{
	std::string domain = getDomain(url);

	auto it = domains_.find(domain);

	if (it == domains_.end()) {
		std::cerr << "request on domain before checking robots.txt\n";
		return;
	}

	auto& state = it->second;

	state.lastRequest = std::chrono::steady_clock::now();	
}

/*
* Saves the result of a request to a robots.txt site
*/
void DomainManager::saveRobotsResult(const std::string& url, const std::string& text)
{
	RobotsRules robotsRules = parseRobots(text);

	DomainState domainState{
		true,
		false,
		robotsRules.allowed,
		robotsRules.disallowed,
		std::chrono::milliseconds(DEFAULT_DOMAIN_RATE_LIMIT_MS),
		std::chrono::steady_clock::now()
	};

	std::string domain = getDomain(url);
	domains_[domain] = domainState;
}

/*
* Takes a url and returns the domain
*/
std::string DomainManager::getDomain(const std::string& url){
	size_t schemeEnd = url.find("://");
	if (schemeEnd == std::string::npos)
		return "";

	size_t hostStart = schemeEnd + 3;
	size_t pathStart = url.find('/', hostStart);

	if (pathStart == std::string::npos)
		return url;

	return url.substr(0, pathStart);
}

/*
* Takes a url and returns the robots.txt url
*/
std::string DomainManager::getRobotsUrl(const std::string& url) {
	size_t pos = url.find("://");

	if (pos == std::string::npos)
		return "";

	pos = url.find('/', pos + 3);

	if (pos == std::string::npos)
		return url + "/robots.txt";

	return url.substr(0, pos) + "/robots.txt";
}

/*
* Checks if two urls share the same domain
*/
bool DomainManager::sameDomain(const std::string& url1, const std::string& url2)
{
	return getDomain(url1) == getDomain(url2);
}
