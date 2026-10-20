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
    std::vector<std::string> links = { "../articles/contact" };

    auto result = processor.process(
        links, "https://example.com/articles/page.html"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "https://example.com/articles/contact");
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
    EXPECT_EQ(result[0], "https://example.com/page1");
    EXPECT_EQ(result[1], "https://example.com/page2");
    EXPECT_EQ(result[2], "https://example.com/page3");
}

TEST(UrlProcessorTests, FiltersImageUrls) {
    UrlProcessor processor;
    std::vector<std::string> links = {
        "https://example.com/page",
        "https://example.com/image.jpg",
        "https://example.com/image.PNG",
        "https://example.com/logo.svg"
    };

    auto result = processor.process(
        links, "https://example.com/"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "https://example.com/page");
}

TEST(UrlProcessorTests, FiltersDocumentUrls) {
    UrlProcessor processor;
    std::vector<std::string> links = {
        "https://example.com/article",
        "https://example.com/report.pdf",
        "https://example.com/slides.pptx",
        "https://example.com/data.xlsx"
    };

    auto result = processor.process(
        links, "https://example.com/"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "https://example.com/article");
}

TEST(UrlProcessorTests, FiltersMediaAndArchiveUrls) {
    UrlProcessor processor;
    std::vector<std::string> links = {
        "https://example.com/page",
        "https://example.com/video.mp4",
        "https://example.com/audio.mp3",
        "https://example.com/archive.zip"
    };

    auto result = processor.process(
        links, "https://example.com/"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "https://example.com/page");
}

TEST(UrlProcessorTests, FiltersNonHttpSchemes) {
    UrlProcessor processor;
    std::vector<std::string> links = {
        "https://example.com/page",
        "http://example.com/other",
        "mailto:test@example.com",
        "javascript:void(0)",
        "ftp://example.com/file"
    };

    auto result = processor.process(
        links, "https://example.com/"
    );

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "https://example.com/page");
    EXPECT_EQ(result[1], "http://example.com/other");
}

TEST(UrlProcessorTests, FiltersExtensionsBeforeQueryAndFragment) {
    UrlProcessor processor;
    std::vector<std::string> links = {
        "https://example.com/image.jpg?size=large",
        "https://example.com/report.pdf?download=true#top",
        "https://example.com/article?category=cpp#comments"
    };

    auto result = processor.process(
        links, "https://example.com/"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(
        result[0],
        "https://example.com/article?category=cpp"
    );
}

TEST(UrlProcessorTests, KeepsUrlsWithoutFileExtensions) {
    UrlProcessor processor;
    std::vector<std::string> links = {
        "https://example.com/",
        "https://example.com/about",
        "https://example.com/articles/cpp"
    };

    auto result = processor.process(
        links, "https://example.com/"
    );

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "https://example.com/");
    EXPECT_EQ(result[1], "https://example.com/about");
    EXPECT_EQ(result[2], "https://example.com/articles/cpp");
}