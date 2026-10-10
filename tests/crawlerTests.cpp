#include <gtest/gtest.h>
#include "crawl/domainManager.h"
#include "utils/systemClock.h"
#include <chrono>
#include "fakeHttpClient.h"
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

TEST_F(CrawlerTests, SavesFetchedPage)
{
    httpClient.responses["https://example.com/robots.txt"] = {
        true, ""
    };

    httpClient.responses["https://example.com"] = {
        true, "<html><body>Hello world</body></html>"
    };

    Crawler crawler(storage, domainManager, clock, httpClient, 1, 1);
    crawler.run("https://example.com");

    EXPECT_EQ(
        storage.loadPage("https://example.com"),
        "Hello world"
    );
}

TEST_F(CrawlerTests, DoesNotSaveFailedPage)
{
    httpClient.responses["https://example.com/robots.txt"] = {
        true, ""
    };

    httpClient.responses["https://example.com"] = {
        false, ""
    };

    Crawler crawler(storage, domainManager, clock, httpClient, 1, 1);
    crawler.run("https://example.com");

    EXPECT_TRUE(storage.loadPage("https://example.com").empty());
}

// DOMAIN MANAGER INTEGRATION TESTS

TEST_F(CrawlerTests, SkipsDisallowedUrls) {
    httpClient.responses["https://example.com/robots.txt"] = {
        true,
        "User-agent:\n"
        "Disallow: /private\n"
    };

    httpClient.responses["https://example.com"] = {
        true,
        R"(<a href="https://example.com/private">1</a>)"
        R"(<a href="https://example.com/1">1</a>)"
    };

    httpClient.responses["https://example.com/private"] = {
        true,
        "<html>private</html>"
    };

    httpClient.responses["https://example.com/1"] = {
        true,
        "<html>1</html>"
    };

    Crawler crawler(storage, domainManager, clock, httpClient, 100, 1);
    crawler.run("https://example.com");

    EXPECT_EQ(httpClient.requestedUrls.size(), 3);
}