#include<iostream>
#include<fstream>
#include<string>
#include<filesystem>
#include<sstream>
#include<unordered_map>
#include<vector>
#include"fetcher.h"
#include"html_parser.h" 

namespace fs = std::filesystem;

using Index = std::unordered_map <std::string, std::unordered_map<std::string, int>>;

int main(int argc, char* argv[]) {
    //std::string query;

    //std::cout << "Search: ";
    //std::cin >> query;

    //auto it = index.find(query);

    //if (it != index.end()) {

    //    std::cout << "Results:\n";

    //    for (const auto& [filename, count] : it->second) {
    //        std::cout << filename << ": " << count << '\n';
    //    }
    //}
    //else {
    //    std::cout << "No results found\n";
    //}
    return 0;
}