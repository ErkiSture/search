#pragma once

#include<vector>
#include<string>

struct RobotsRules {
    std::vector<std::string> disallowed;
    std::vector<std::string> allowed;
    int crawlDelay = 0;
};

RobotsRules parseRobots(const std::string& content);