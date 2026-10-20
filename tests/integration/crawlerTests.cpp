#include <gtest/gtest.h>
#include "crawl/domainManager.h"
#include "utils/systemClock.h"
#include <chrono>
#include "fakes/fakeHttpClient.h"
#include "storage/storage.h"
#include "crawl/crawler.h"
#include "parsing/linkExtractor.h"
#include "net/curlHttpClient.h"
#include <filesystem>
#include "processing/htmlParser.h"
#include "processing/urlProcessor.h"
#include "processing/pageScorer.h"
#include "index/indexer.h"

class CrawlerTests : public testing::Test {
protected:
    std::filesystem::path testDir = "testData";

    Storage storage{ testDir };
    SystemClock clock;
    DomainManager domainManager{ clock, std::chrono::milliseconds(50) };
    FakeHttpClient httpClient;
    HtmlParser htmlParser;
    UrlProcessor urlProcessor;
    PageScorer pageScorer;
    Indexer indexer{ storage };

    void SetUp() override {
        std::filesystem::create_directories(testDir);

        storage.clearDatabase();
    }

    void TearDown() override {
    }
};

// SINGULAR WORKER TESTS

TEST_F(CrawlerTests, FetchesDiscoveredLinks) 
{
    httpClient.responses["https://example.com/robots.txt"] = {
        true,
        ""
    };

    httpClient.responses["https://example.com"] = {
        true,
        R"(<a href="https://example.com/1">1</a>)"
    };

    httpClient.responses["https://example.com/1"] = {
        true,
        R"(<a href="https://example.com/2">2</a>)"
        R"(<a href="https://example.com/3">3</a>)"
        R"(<a href="https://example.com/4">4</a>)"
    };

    httpClient.responses["https://example.com/2"] = {
        true,
        R"(<html>2</html>)"
    };
    httpClient.responses["https://example.com/3"] = {
        true,
        R"(<html>3</html>)"
    };
    httpClient.responses["https://example.com/4"] = {
        true,
        R"(<html>4</html>)"
    };

    Crawler crawler(storage, domainManager, clock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, 100, 1);
    crawler.run("https://example.com");

    EXPECT_EQ(httpClient.requestedUrls.size(), 6);
}

TEST_F(CrawlerTests, DoesNotFetchDuplicateUrls) 
{
    httpClient.responses["https://example.com/robots.txt"] = {
        true,
        ""
    };

    httpClient.responses["https://example.com"] = {
        true,
        R"(<a href="https://example.com/1">1</a>)"
        R"(<a href="https://example.com/1">1</a>)"
    };

    httpClient.responses["https://example.com/1"] = {
        true,
        R"(<a href="https://example.com/1">1</a>)"
    };

    Crawler crawler(storage, domainManager, clock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, 100, 1);
    crawler.run("https://example.com");

    EXPECT_EQ(httpClient.requestedUrls.size(), 3);
}

TEST_F(CrawlerTests, StopsAtMaxPages)
{
    httpClient.responses["https://example.com/robots.txt"] = {
        true,
        ""
    };

    httpClient.responses["https://example.com"] = {
        true,
        R"(<a href="https://example.com/1">1</a>)"
    };

    httpClient.responses["https://example.com/1"] = {
        true,
        R"(<a href="https://example.com/2">2</a>)"
    };

    httpClient.responses["https://example.com/2"] = {
        true,
        R"(<a href="https://example.com/3">3</a>)"
    };

    httpClient.responses["https://example.com/3"] = {
        true,
        "<html>4</html>"
    };

    Crawler crawler(storage, domainManager, clock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, 3, 1);
    crawler.run("https://example.com");
    
    // 3 + 1, accounting for robots.txt
    EXPECT_EQ(httpClient.requestedUrls.size(), 4);
}

TEST_F(CrawlerTests, HandlesFailedFetch)
{
    httpClient.responses["https://example.com/robots.txt"] = {
        true, ""
    };

    httpClient.responses["https://example.com"] = {
        true,         
        R"(<a href="https://example.com/1">1</a>)"
        R"(<a href="https://example.com/2">2</a>)"
        R"(<a href="https://example.com/3">3</a>)"
    };

    httpClient.responses["https://example.com/1"] = {
        false, "<html>1</html>"
    };

    httpClient.responses["https://example.com/2"] = {
        true, "<html>2</html>"
    };

    httpClient.responses["https://example.com/3"] = {
        false, "<html>3</html>"
    };

    Crawler crawler(storage, domainManager, clock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, 100, 1);
    EXPECT_NO_THROW(crawler.run("https://example.com"));
    EXPECT_EQ(httpClient.requestedUrls.size(), 5);
}

TEST_F(CrawlerTests, RespectsDomainCrawlDelay) {
    httpClient.responses["https://example.com/robots.txt"] = {
        true, ""
    };
    httpClient.responses["https://test1.com/robots.txt"] = {
        true, ""
    };
    httpClient.responses["https://test1.com/robots.txt"] = {
        true, ""
    };

    httpClient.responses["https://example.com"] = {
        true,
        R"(<a href="https://test1.com">1</a>)"
        R"(<a href="https://test2.com">1</a>)"
    };

    httpClient.responses["https://test1.com"] = {
        true,
        "<html>test1</html>"
    };

    httpClient.responses["https://test2.com"] = {
        true,
        "<html>test2</html>"
    };

    SystemClock realClock;
    DomainManager testDomainManager(
        realClock, std::chrono::milliseconds(500));

    Crawler crawler(
        storage, testDomainManager, realClock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, 100, 1);

    auto start = std::chrono::steady_clock::now();
    crawler.run("https://example.com");
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_GE(elapsed, std::chrono::milliseconds(1000));
    EXPECT_LE(elapsed, std::chrono::milliseconds(1300));

    EXPECT_GE(httpClient.requestTimes[1] - httpClient.requestTimes[0], std::chrono::milliseconds(500));
    EXPECT_LE(httpClient.requestTimes[2] - httpClient.requestTimes[1], std::chrono::milliseconds(100));
    EXPECT_LE(httpClient.requestTimes[3] - httpClient.requestTimes[2], std::chrono::milliseconds(100));
    EXPECT_GE(httpClient.requestTimes[4] - httpClient.requestTimes[2], std::chrono::milliseconds(500));
    EXPECT_GE(httpClient.requestTimes[5] - httpClient.requestTimes[3], std::chrono::milliseconds(500));
}

// MULTIPLE WORKERS TESTS

TEST_F(CrawlerTests, MultipleWorkersProcessPages)
{
    httpClient.responses["https://example.com/robots.txt"] = {
        true, ""
    };

    httpClient.responses["https://example.com"] = {
        true,
        R"(<a href="https://example.com/1">1</a>)"
        R"(<a href="https://example.com/2">2</a>)"
        R"(<a href="https://example.com/3">3</a>)"
        R"(<a href="https://example.com/4">4</a>)"
    };

    for (int i = 1; i <= 4; ++i) {
        httpClient.responses[
            "https://example.com/" + std::to_string(i)
        ] = { true, "<html>Page</html>" };
    }

    Crawler crawler(storage, domainManager, clock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, 5, 3);

    crawler.run("https://example.com");

    for (int i = 1; i <= 4; ++i) {
        const std::string url = "https://example.com/" + std::to_string(i);

        EXPECT_EQ(
            std::count(
                httpClient.requestedUrls.begin(),
                httpClient.requestedUrls.end(),
                url
            ),
            1
        );
    }
}

TEST_F(CrawlerTests, RespectsMaxPagesWithMultipleWorkers)
{
    httpClient.responses["https://example.com/robots.txt"] = {
        true, ""
    };

    httpClient.responses["https://example.com"] = {
        true,
        R"(<a href="https://example.com/1">1</a>)"
        R"(<a href="https://example.com/2">2</a>)"
        R"(<a href="https://example.com/3">3</a>)"
        R"(<a href="https://example.com/4">4</a>)"
        R"(<a href="https://example.com/5">5</a>)"
        R"(<a href="https://example.com/6">6</a>)"
    };

    for (int i = 1; i <= 6; ++i) {
        httpClient.responses[
            "https://example.com/" + std::to_string(i)
        ] = { true, "<html>Page</html>" };
    }

    int maxPages = 3;

    Crawler crawler(storage, domainManager, clock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, maxPages, 4);

    crawler.run("https://example.com");

    for (std::string x : httpClient.requestedUrls) {
        std::cout << x << std::endl;
    }

    EXPECT_EQ(httpClient.requestedUrls.size(), maxPages + 1);
}

TEST_F(CrawlerTests, DoesNotFetchDuplicateUrlsWithMultipleWorkers)
{
    httpClient.responses["https://example.com/robots.txt"] = {
        true, ""
    };

    httpClient.responses["https://example.com"] = {
        true,
        R"(<a href="https://example.com/shared">A</a>)"
        R"(<a href="https://example.com/shared">B</a>)"
        R"(<a href="https://example.com/other">C</a>)"
    };

    httpClient.responses["https://example.com/shared"] = {
        true, "<html>Shared</html>"
    };

    httpClient.responses["https://example.com/other"] = {
        true, "<html>Other</html>"
    };

    Crawler crawler(storage, domainManager, clock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, 100, 4);
    crawler.run("https://example.com");

    EXPECT_EQ(
        std::count(
            httpClient.requestedUrls.begin(),
            httpClient.requestedUrls.end(),
            "https://example.com/shared"
        ),
        1
    );
}

TEST_F(CrawlerTests, RespectsDomainCrawlDelayWithMultipleWorkers) {
    httpClient.responses["https://example.com/robots.txt"] = {
        true, ""
    };
    httpClient.responses["https://test1.com/robots.txt"] = {
        true, ""
    };
    httpClient.responses["https://test1.com/robots.txt"] = {
        true, ""
    };

    httpClient.responses["https://example.com"] = {
        true,
        R"(<a href="https://test1.com">1</a>)"
        R"(<a href="https://test2.com">1</a>)"
    };

    httpClient.responses["https://test1.com"] = {
        true,
        "<html>test1</html>"
    };

    httpClient.responses["https://test2.com"] = {
        true,
        "<html>test2</html>"
    };

    SystemClock realClock;
    DomainManager testDomainManager(
        realClock, std::chrono::milliseconds(500));

    Crawler crawler(
        storage, testDomainManager, realClock, httpClient, htmlParser, urlProcessor, pageScorer, indexer, 100, 4);

    auto start = std::chrono::steady_clock::now();
    crawler.run("https://example.com");
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_GE(elapsed, std::chrono::milliseconds(1000));
    EXPECT_LE(elapsed, std::chrono::milliseconds(1300));

    EXPECT_GE(httpClient.requestTimes[1] - httpClient.requestTimes[0], std::chrono::milliseconds(500));
    EXPECT_LE(httpClient.requestTimes[2] - httpClient.requestTimes[1], std::chrono::milliseconds(100));
    EXPECT_LE(httpClient.requestTimes[3] - httpClient.requestTimes[2], std::chrono::milliseconds(100));
    EXPECT_GE(httpClient.requestTimes[4] - httpClient.requestTimes[2], std::chrono::milliseconds(500));
    EXPECT_GE(httpClient.requestTimes[5] - httpClient.requestTimes[3], std::chrono::milliseconds(500));
}