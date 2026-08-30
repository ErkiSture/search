#include<sstream>
#include<string>
#include<vector>
#include"robotsTxtParser.h"

RobotsRules parseRobots(const std::string& content)
{
    RobotsRules rules;
    std::istringstream stream(content);
    std::string line;

    bool relevantAgent = false;

    while (std::getline(stream, line)) {

        // Remove comments
        size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // TODO: trim whitespace

        if (key == "User-agent") {
            relevantAgent = (value == "*");
        }
        else if (relevantAgent && key == "Disallow") {
            rules.disallowed.push_back(value);
        }
        else if (relevantAgent && key == "Allow") {
            rules.allowed.push_back(value);
        }
        else if (relevantAgent && key == "Crawl-delay") {
            rules.crawlDelay = std::stoi(value);
        }
    }

    return rules;
}