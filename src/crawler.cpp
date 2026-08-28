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
#include<mutex>
#include<condition_variable>

std::mutex m;
std::condition_variable cv;

void worker_fetch
(
    std::queue<std::string>& frontier,
    std::unordered_set<std::string>& visited,
    int max_pages,
    int& sum_fetch_time
)
{
    while (true) {
        std::unique_lock<std::mutex> lock(m);

        cv.wait(lock, [&] {
            return !frontier.empty() || visited.size() >= max_pages;
            });

        if (visited.size() >= max_pages) {
            break;
        }

        std::string url = frontier.front();
        frontier.pop();

        if (visited.count(url)) {
            continue;
        }

        visited.insert(url);

        lock.unlock();

        auto start = std::chrono::high_resolution_clock::now();

        std::string html = fetch_url(url);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start
        );

        std::cout << "thread " << std::this_thread::get_id()
            << ": " << duration.count() << " ms\n";

        lock.lock();
        sum_fetch_time += duration.count();

        if (html.empty()) {
            visited.insert(url);
            lock.unlock();
            continue;
        }

        std::string text = strip_html(html);
        savePage(url, text);

        for (const std::string& link : extract_links(html, url)) {
            if (!visited.count(link))
                frontier.push(link);
        }

        lock.unlock();

        cv.notify_all();
    }
}

void run_crawler_2(const std::string& seed_url, int max_pages) {
    std::unordered_set<std::string> visited = loadVisitedUrls();

    std::queue<std::string> frontier;
    frontier.push(seed_url);

    int sum_fetch_time = 0;
    bool finished = false;

    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "--------------------" << "\n";
    std::cout << "CRAWL START" << "\n";
    std::cout << "--------------------" << "\n";

    std::thread t1(worker_fetch, std::ref(frontier), std::ref(visited), max_pages, std::ref(sum_fetch_time));
    std::thread t2(worker_fetch, std::ref(frontier), std::ref(visited), max_pages, std::ref(sum_fetch_time));
    std::thread t3(worker_fetch, std::ref(frontier), std::ref(visited), max_pages, std::ref(sum_fetch_time));
    std::thread t4(worker_fetch, std::ref(frontier), std::ref(visited), max_pages, std::ref(sum_fetch_time));
    std::thread t5(worker_fetch, std::ref(frontier), std::ref(visited), max_pages, std::ref(sum_fetch_time));

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start
    );

    std::cout << "total: " << sum_fetch_time << " ms" << "\n";
    std::cout << "--------------------" << "\n";
    std::cout << "CRAWL END, took " << duration.count() << " ms" << "\n";
    std::cout << "--------------------" << "\n";

}

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cout << "Usage: " << "crawler <seed_url> <max_pages>";
		return 1;
	}

	std::string seed_url = argv[1];
	int max_pages = std::stoi(argv[2]);

	init_storage("data");
	run_crawler_2(seed_url, max_pages);
	build_index_from_storage();

	std::cout << "Done. Crawled " << max_pages << " pages max.\n";

	return 0;
}