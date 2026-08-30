#pragma once

#include<vector>
#include<string>

struct RobotsRules {
    std::vector<std::string> disallowed;
    std::vector<std::string> allowed;
    int crawlDelay;
};

RobotsRules parseRobots(const std::string& content);