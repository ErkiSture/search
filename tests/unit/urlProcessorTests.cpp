#include <gtest/gtest.h>
#include "processing/urlProcessor.h"
#include <string>

TEST(UrlProcessorTests, ResolvesRootRelativeUrl) {
    UrlProcessor processor;
    std::vector<std::string> links = { "/about" };

    auto result = processor.process(
        links, "https://example.com/articles/page.html"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "https://example.com/about");
}

TEST(UrlProcessorTests, ResolvesRelativeUrl) {
    UrlProcessor processor;
    std::vector<std::string> links = { "contact" };

    auto result = processor.process(
        links, "https://example.com/articles/page.html"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "https://example.com/articles/contact");
}

TEST(UrlProcessorTests, ResolvesParentDirectoryUrl) {
    UrlProcessor processor;
    std::vector<std::string> links = { "../images/cat.png" };

    auto result = processor.process(
        links, "https://example.com/articles/page.html"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "https://example.com/images/cat.png");
}

TEST(UrlProcessorTests, KeepsAbsoluteUrl) {
    UrlProcessor processor;
    std::vector<std::string> links = { "https://google.com/" };

    auto result = processor.process(
        links, "https://example.com/articles/page.html"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "https://google.com/");
}

TEST(UrlProcessorTests, RemovesFragment) {
    UrlProcessor processor;
    std::vector<std::string> links = { "/search?q=cpp#results" };

    auto result = processor.process(
        links, "https://example.com/articles/page.html"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "https://example.com/search?q=cpp");
}

TEST(UrlProcessorTests, HandlesEmptyInput) {
    UrlProcessor processor;
    std::vector<std::string> links;

    auto result = processor.process(
        links, "https://example.com/articles/page.html"
    );

    EXPECT_TRUE(result.empty());
}

TEST(UrlProcessorTests, DeduplicatesUrls) {
    UrlProcessor processor;

    std::vector<std::string> links = {
        "https://example.com/page1",
        "https://example.com/page2",
        "https://example.com/page1",
        "https://example.com/page3",
        "https://example.com/page2#fragment"
    };

    auto result = processor.process(
        links, "https://example.com/articles/page.html"
    );

    ASSERT_EQ(result.size(), 3);
    ASSERT_EQ(result.size(), 3);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "https://example.com/page1");
    EXPECT_EQ(result[1], "https://example.com/page2");
    EXPECT_EQ(result[2], "https://example.com/page3");
}