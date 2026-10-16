#pragma once
#include<string>
#include<unordered_map>

using Index = std::unordered_map<std::string, std::unordered_map<std::string, int>>;
using PostingList = std::unordered_map<std::string, int>; // url : count
using PageIndex = std::unordered_map<std::string, int>; // word : count
