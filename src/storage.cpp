#include<string>
#include<unordered_set>
#include"storage.h"
#include<filesystem>
#include<iostream>
#include<fstream>
#include<sstream>

namespace fs = std::filesystem;

namespace {
	fs::path g_dataDir;

	// turns a URL into a safe, unique filename
	std::string url_to_filename(const std::string& url) 
	{
		size_t h = std::hash<std::string>{}(url);
		return std::to_string(h) + ".txt";
	}

	fs::path visited_path;
	fs::path index_path;
}

void init_storage(const fs::path& dataDir)
{
	g_dataDir = dataDir;

	if (!fs::exists(g_dataDir)) {
		fs::create_directories(g_dataDir);
	}

	visited_path = fs::path(g_dataDir) / "manifest.txt";
	index_path = fs::path(g_dataDir) / "index.txt";
}

void savePage(const std::string& url, const std::string& text)
{
	if (g_dataDir.empty()) {
		std::cerr << "init_storage() was never called\n";
		return;
	}

	fs::path filepath = fs::path(g_dataDir) / url_to_filename(url);

	std::ofstream file(filepath);
	if (!file) {
		std::cerr << "failed to open file: " << filepath;
	}

	file << text;
	file.close();

	std::ofstream manifest(visited_path, std::ios::app);

	if (!manifest) {
		std::cerr << "failed to open manifest\n";
		return;
	}

	manifest << url << "\n";
}

std::string loadPage(const std::string& url)
{
	fs::path filepath = g_dataDir / url_to_filename(url);
	std::ifstream file(filepath);
	if (!file) {
		std::cerr << "failed to open file: " << filepath << '\n';
		return "";
	}

	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

void saveIndex(const Index& index)
{
	std::ofstream out(index_path);

	if (!out) {
		std::cerr << "failed to open file: " << index_path << "\n";
		return;
	}

	for (const auto& [word, postings] : index) {
		out << word;
		for (const auto& [url, count] : postings) {
			out << ' ' << url << ':' << count;
		}
		out << '\n';
	}
}

Index loadIndex() {
	Index index;
	std::ifstream in(index_path);

	if (!in) {
		std::cerr << "failed to open " << index_path << " for reading\n";
		return index;   // empty index if nothing's been built/saved yet
	}

	std::string line;
	while (std::getline(in, line)) {
		std::stringstream ss(line);

		std::string word;
		ss >> word;              // first token on the line = the word

		std::string entry;
		while (ss >> entry) {    // every remaining token = "url:count"
			size_t colon = entry.rfind(':');   // last colon, since URLs contain ':' too
			if (colon == std::string::npos)
				continue;         // malformed entry — skip rather than crash

			std::string url = entry.substr(0, colon);
			std::string count_str = entry.substr(colon + 1);

			int count = std::stoi(count_str);
			index[word][url] = count;
		}
	}

	return index;
}

std::unordered_set<std::string> loadVisitedUrls()
{
	std::unordered_set<std::string> urls;
	std::ifstream manifest(visited_path);

	if (!manifest)
	{
		std::cerr << "Failed to open manifest\n";
		return urls;
	}

	std::string line;
	while (std::getline(manifest, line)) {
		if (!line.empty()) {
			urls.insert(line);
		}
	}

	return urls;
}
