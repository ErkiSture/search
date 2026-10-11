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

class CrawlerTests : public testing::Test {
protected:
    std::filesystem::path testDir = "testData";

    Storage storage{ testDir };
    SystemClock clock;
    DomainManager domainManager{ clock };
    FakeHttpClient httpClient;

    void SetUp() override {
        std::filesystem::create_directories(testDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir);
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

    Crawler crawler(storage, domainManager, clock, httpClient, 100, 1);
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

    Crawler crawler(storage, domainManager, clock, httpClient, 100, 1);
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

    Crawler crawler(storage, domainManager, clock, httpClient, 3, 1);
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

    Crawler crawler(storage, domainManager, clock, httpClient, 100, 1);
    EXPECT_NO_THROW(crawler.run("https://example.com"));
    EXPECT_EQ(httpClient.requestedUrls.size(), 5);
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

    Crawler crawler(storage, domainManager, clock, httpClient, 5, 3);

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

//TEST_F(CrawlerTests, RespectsMaxPagesWithMultipleWorkers)
//{
//    httpClient.responses["https://example.com/robots.txt"] = {
//        true, ""
//    };
//
//    httpClient.responses["https://example.com"] = {
//        true,
//        R"(<a href="https://example.com/1">1</a>)"
//        R"(<a href="https://example.com/2">2</a>)"
//        R"(<a href="https://example.com/3">3</a>)"
//        R"(<a href="https://example.com/4">4</a>)"
//    };
//
//    for (int i = 1; i <= 4; ++i) {
//        httpClient.responses[
//            "https://example.com/" + std::to_string(i)
//        ] = { true, "<html>Page</html>" };
//    }
//
//    const int maxPages = 2;
//
//    Crawler crawler(
//        storage, domainManager, clock, httpClient, maxPages, 4
//    );
//
//    crawler.run("https://example.com");
//
//    const auto pageRequests = std::count_if(
//        httpClient.requestedUrls.begin(),
//        httpClient.requestedUrls.end(),
//        [](const std::string& url) {
//            return url != "https://example.com/robots.txt";
//        }
//    );
//
//    EXPECT_LE(pageRequests, maxPages);
//}

//TEST_F(CrawlerTests, DoesNotFetchDuplicateUrlsWithMultipleWorkers)
//{
//    httpClient.responses["https://example.com/robots.txt"] = {
//        true, ""
//    };
//
//    httpClient.responses["https://example.com"] = {
//        true,
//        R"(<a href="https://example.com/shared">A</a>)"
//        R"(<a href="https://example.com/shared">B</a>)"
//        R"(<a href="https://example.com/other">C</a>)"
//    };
//
//    httpClient.responses["https://example.com/shared"] = {
//        true, "<html>Shared</html>"
//    };
//
//    httpClient.responses["https://example.com/other"] = {
//        true, "<html>Other</html>"
//    };
//
//    Crawler crawler(storage, domainManager, clock, httpClient, 3, 4);
//    crawler.run("https://example.com");
//
//    EXPECT_EQ(
//        std::count(
//            httpClient.requestedUrls.begin(),
//            httpClient.requestedUrls.end(),
//            "https://example.com/shared"
//        ),
//        1
//    );
//}