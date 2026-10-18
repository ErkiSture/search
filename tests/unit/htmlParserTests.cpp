#include <gtest/gtest.h>
#include "processing/htmlParser.h"
#include <string>

TEST(HtmlParserTests, parsesHtml) {
    HtmlParser htmlParser;

    const std::string html = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Learn C++</title>
        </head>
        <body>
            <h1>C++ Basics</h1>
            <p>Learn about pointers.</p>
            <a href="https://example.com/loops">Loops Tutorial</a>
            <a href="/functions">Functions Tutorial</a>
        </body>
        </html>
    )";

    ParsedPage result = htmlParser.parse(html);

    EXPECT_EQ(result.title, "Learn C++");
    EXPECT_NE(result.text.find("C++ Basics"), std::string::npos);
    EXPECT_NE(result.text.find("Learn about pointers."), std::string::npos);

    ASSERT_EQ(result.links.size(), 2);
    EXPECT_EQ(result.links[0], "https://example.com/loops");
    EXPECT_EQ(result.links[1], "/functions");
}

TEST(HtmlParserTests, parsesEmptyHtml) {
    HtmlParser htmlParser;

    ParsedPage result = htmlParser.parse("");

    EXPECT_TRUE(result.title.empty());
    EXPECT_TRUE(result.text.empty());
    EXPECT_TRUE(result.links.empty());
}

TEST(HtmlParserTests, parsesHtmlWithoutTitle) {
    HtmlParser htmlParser;

    const std::string html = R"(
        <html>
        <body>
            <p>Hello world</p>
        </body>
        </html>
    )";

    ParsedPage result = htmlParser.parse(html);

    EXPECT_TRUE(result.title.empty());
    EXPECT_NE(result.text.find("Hello world"), std::string::npos);
    EXPECT_TRUE(result.links.empty());
}

TEST(HtmlParserTests, parsesHtmlWithoutLinks) {
    HtmlParser htmlParser;

    const std::string html = R"(
        <html>
        <head><title>No Links</title></head>
        <body>
            <p>This page contains no links.</p>
        </body>
        </html>
    )";

    ParsedPage result = htmlParser.parse(html);

    EXPECT_EQ(result.title, "No Links");
    EXPECT_TRUE(result.links.empty());
}

TEST(HtmlParserTests, ignoresLinksWithoutHref) {
    HtmlParser htmlParser;

    const std::string html = R"(
        <html>
        <body>
            <a>Missing href</a>
            <a href="/valid">Valid link</a>
        </body>
        </html>
    )";

    ParsedPage result = htmlParser.parse(html);

    ASSERT_EQ(result.links.size(), 1);
    EXPECT_EQ(result.links[0], "/valid");
}

TEST(HtmlParserTests, parsesMalformedHtml) {
    HtmlParser htmlParser;

    const std::string html =
        "<html><body><p>Hello <a href='/test'>Link";

    ParsedPage result = htmlParser.parse(html);

    EXPECT_NE(result.text.find("Hello"), std::string::npos);

    ASSERT_EQ(result.links.size(), 1);
    EXPECT_EQ(result.links[0], "/test");
}

TEST(HtmlParserTests, extractsMultipleLinksInOrder) {
    HtmlParser htmlParser;

    const std::string html = R"(
        <html>
        <body>
            <a href="/first">First</a>
            <a href="/second">Second</a>
            <a href="/third">Third</a>
        </body>
        </html>
    )";

    ParsedPage result = htmlParser.parse(html);

    ASSERT_EQ(result.links.size(), 3);
    EXPECT_EQ(result.links[0], "/first");
    EXPECT_EQ(result.links[1], "/second");
    EXPECT_EQ(result.links[2], "/third");
}