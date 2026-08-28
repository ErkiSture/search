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
#include<vector>
#include"crawler.h"

Crawler::Crawler(Storage& storage, int maxPages, int workerCount) :
    storage_(storage),
    maxPages_(maxPages),
    workerCount_(workerCount)
{
    visited_ = storage.loadVisitedUrls();
}

void Crawler::worker()
{
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_.wait(lock, [&] {
            return !frontier_.empty() ||
                visited_.size() >= maxPages_ ||
                (frontier_.empty() && activeWorkers == 0);
            });

        if (visited_.size() >= maxPages_) {
            break;
        }

        if (frontier_.empty() && activeWorkers == 0) {
            break;
        }

        std::string url = frontier_.front();
        frontier_.pop();

        if (visited_.count(url)) {
            continue;
        }

        visited_.insert(url);

        activeWorkers++;

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
        sumFetchTime_ += duration.count();

        if (html.empty()) {
            lock.unlock();
            activeWorkers--;
            cv_.notify_all();
            continue;
        }

        std::string text = strip_html(html);
        storage_.savePage(url, text);

        for (const std::string& link : extract_links(html, url)) {
            if (!visited_.count(link))
                frontier_.push(link);
        }

        activeWorkers--;
        lock.unlock();

        cv_.notify_all();
    }
}

void Crawler::run(const std::string& seedUrl) {
    frontier_.push(seedUrl);

    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "--------------------" << "\n";
    std::cout << "CRAWL START" << "\n";
    std::cout << "--------------------" << "\n";

    std::vector<std::thread> workers;

    for (int i = 0; i < workerCount_; i++) {
        workers.push_back(std::thread(&Crawler::worker, this));
    }

    for (auto& worker : workers) {
        worker.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start
    );

    std::cout << "total: " << sumFetchTime_ << " ms" << "\n";
    std::cout << "--------------------" << "\n";
    std::cout << "CRAWL END, took " << duration.count() << " ms" << "\n";
    std::cout << "--------------------" << "\n";

}

constexpr int WORKER_COUNT = 3;

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cout << "Usage: " << "crawler <seed_url> <max_pages>";
		return 1;
	}

	std::string seedUrl = argv[1];
	int maxPages = std::stoi(argv[2]);

    Storage storage("data");
    Crawler crawler(storage, maxPages, WORKER_COUNT);
	crawler.run(seedUrl);
    Indexer indexer(storage);

	std::cout << "Done. Crawled " << maxPages << " pages max.\n";

	return 0;
}