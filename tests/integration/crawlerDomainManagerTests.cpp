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