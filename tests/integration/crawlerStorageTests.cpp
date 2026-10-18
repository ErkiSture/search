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
    DomainManager domainManager{ clock, std::chrono::milliseconds(50) };
    FakeHttpClient httpClient;

    void SetUp() override {
        std::filesystem::create_directories(testDir);

        storage.clearDatabase();
    }

    void TearDown() override {
    }
};

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

    EXPECT_EQ(storage.loadPage("https://example.com"), "Hello world");
    EXPECT_EQ(storage.getStatus("https://example.com"), "fetched");
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
    EXPECT_EQ(storage.getStatus("https://example.com"), "failed");
}

TEST_F(CrawlerTests, UrlHasCorrectStatusWhileFetching) {
    httpClient.responses["https://example.com/robots.txt"] = {
        true, ""
    };

    httpClient.responses["https://example.com"] = {
        false, ""
    };

    httpClient.onFetch = [&](const std::string& url) {
        if (url == "https://example.com") {
                EXPECT_EQ(storage.getStatus(url), "fetching");
            }
        };

    Crawler crawler(storage, domainManager, clock, httpClient, 1, 1);
    crawler.run("https://example.com");
}