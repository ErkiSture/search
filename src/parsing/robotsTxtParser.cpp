#include<sstream>
#include<string>
#include<vector>
#include"parsing/robotsTxtParser.h"

static std::string trim(const std::string& str) {
    size_t start = 0;
    size_t end = str.size();

    while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }

    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        end--;
    }

    return str.substr(start, end - start);
}

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

        // Remove whitespace around the entire line
        line = trim(line);

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Find ':'
        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, colon));
        std::string value = trim(line.substr(colon + 1));

        if (key == "User-agent") {
            // Empty User-agent is treated as the default group in these tests.
            relevantAgent = value.empty() || value == "*";
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