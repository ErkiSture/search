#include<string>
#include<unordered_set>
#include"storage.h"
#include<filesystem>
#include<iostream>
#include<fstream>
#include<sstream>

// turns a URL into a safe, unique filename
std::string url_to_filename(const std::string& url)
{
	size_t h = std::hash<std::string>{}(url);
	return std::to_string(h) + ".txt";
}

namespace fs = std::filesystem;

Storage::Storage(const fs::path& dataDir) {
	g_dataDir_ = dataDir;

	if (!fs::exists(g_dataDir_)) {
		fs::create_directories(g_dataDir_);
	}

	pages_path_ = fs::path(g_dataDir_) / "pages";
	visited_path_ = fs::path(g_dataDir_) / "manifest.txt";
	index_path_ = fs::path(g_dataDir_) / "index.txt";

	if (!fs::exists(pages_path_)) {
		fs::create_directories(pages_path_);
	}
}

void Storage::savePage(const std::string& url, const std::string& text)
{
	if (g_dataDir_.empty()) {
		std::cerr << "init_storage() was never called\n";
		return;
	}

	fs::path filepath = fs::path(pages_path_) / url_to_filename(url);

	std::ofstream file(filepath);
	if (!file) {
		std::cerr << "failed to open file: " << filepath;
	}

	file << text;
	file.close();

	std::ofstream manifest(visited_path_, std::ios::app);

	if (!manifest) {
		std::cerr << "failed to open manifest\n";
		return;
	}

	manifest << url << "\n";
}

std::string Storage::loadPage(const std::string& url)
{
	fs::path filepath = pages_path_ / url_to_filename(url);
	std::ifstream file(filepath);
	if (!file) {
		std::cerr << "failed to open file: " << filepath << '\n';
		return "";
	}

	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

void Storage::saveIndex(const Index& index)
{
	std::ofstream out(index_path_);

	if (!out) {
		std::cerr << "failed to open file: " << index_path_ << "\n";
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

Index Storage::loadIndex() {
	Index index;
	std::ifstream in(index_path_);

	if (!in) {
		std::cerr << "failed to open " << index_path_ << " for reading\n";
		return index;   // empty index if nothing's been built/saved yet
	}

	std::string line;
	while (std::getline(in, line)) {
		std::stringstream ss(line);

		std::string word;
		ss >> word;

		std::string entry;
		while (ss >> entry) {
			size_t colon = entry.rfind(':');
			if (colon == std::string::npos)
				continue;

			std::string url = entry.substr(0, colon);
			std::string count_str = entry.substr(colon + 1);

			int count = std::stoi(count_str);
			index[word][url] = count;
		}
	}

	return index;
}

std::unordered_set<std::string> Storage::loadVisitedUrls()
{
	std::unordered_set<std::string> urls;
	std::ifstream manifest(visited_path_);

	if (!manifest) {
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