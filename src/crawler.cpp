#include<string>
#include"storage.h"
#include<iostream>
#include"indexer.h"
#include<queue>
#include"fetcher.h"
#include"html_parser.h"
#include"link_extractor.h"
#include<thread>
#include<chrono>

void run_crawler(const std::string& seed_url, int max_pages) {
    std::unordered_set<std::string> visited = loadVisitedUrls();

    std::queue<std::string> frontier;
    frontier.push(seed_url);

    while (!frontier.empty() && (int)visited.size() < max_pages) {
        std::string url = frontier.front();
        frontier.pop();
        std::cout << "visited count: " << visited.size() << "\n";

        if (visited.count(url))
            continue;

        std::cout << "Fetching: " << url << '\n';

        std::string html = fetch_url(url);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Fetched " << html.size() << " bytes\n";
        if (html.empty()) {
            visited.insert(url);
            continue;
        }

        std::string text = strip_html(html);
        savePage(url, text);
        visited.insert(url);

        for (const auto& link : extract_links(html, url)) {
            std::cout << "link: " << link;
            if (!visited.count(link))
                frontier.push(link);
        }
    }
}

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cout << "Usage: " << "crawler <seed_url> <max_pages>";
		return 1;
	}

	std::string seed_url = argv[1];
	int max_pages = std::stoi(argv[2]);

	init_storage("data");
	run_crawler(seed_url, max_pages);
	build_index_from_storage();

	std::cout << "Done. Crawled " << max_pages << " pages max.\n";

	return 0;
}