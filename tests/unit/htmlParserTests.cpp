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
            <meta name="description" content="A website about C++ programming.">
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
    EXPECT_EQ(result.description, "A website about C++ programming.");
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

TEST(HtmlParserTests, ignoreJavascript) {
    HtmlParser htmlParser;

    const std::string html = R"(
        <html><body>
            <p>VISIBLE_MALFORMED_TEXT
            <script>HIDDEN_MALFORMED_SCRIPT
            <a href="/test">Test link
        </body>
    )";

    const ParsedPage page = htmlParser.parse(html);
    EXPECT_NE(
        page.text.find("VISIBLE_MALFORMED_TEXT"),
        std::string::npos
    );

    EXPECT_EQ(
        page.text.find("HIDDEN_MALFORMED_SCRIPT"),
        std::string::npos
    );
}


TEST(HtmlParserTests, ignoresCss)
{
    HtmlParser htmlParser;

    const std::string html = R"(
        <html>
        <head>
            <style>HIDDEN_CSS_MARKER { color: red; }</style>
        </head>
        <body>
            <p>Visible article text</p>
        </body>
        </html>
    )";

    const ParsedPage page = htmlParser.parse(html);

    EXPECT_NE(page.text.find("Visible article text"), std::string::npos);
    EXPECT_EQ(page.text.find("HIDDEN_CSS_MARKER"), std::string::npos);
}

TEST(HtmlParserTests, ignoresNoscript)
{
    HtmlParser htmlParser;

    const std::string html = R"(
        <html><body>
            <p>Visible text</p>
            <noscript>HIDDEN_NOSCRIPT_MARKER</noscript>
        </body></html>
    )";

    const ParsedPage page = htmlParser.parse(html);

    EXPECT_NE(page.text.find("Visible text"), std::string::npos);
    EXPECT_EQ(page.text.find("HIDDEN_NOSCRIPT_MARKER"), std::string::npos);
}

TEST(HtmlParserTests, ignoresTemplateContent)
{
    HtmlParser htmlParser;

    const std::string html = R"(
        <html><body>
            <p>Visible text</p>
            <template>HIDDEN_TEMPLATE_MARKER</template>
        </body></html>
    )";

    const ParsedPage page = htmlParser.parse(html);

    EXPECT_NE(page.text.find("Visible text"), std::string::npos);
    EXPECT_EQ(page.text.find("HIDDEN_TEMPLATE_MARKER"), std::string::npos);
}

TEST(HtmlParserTests, ignoresSvgText)
{
    HtmlParser htmlParser;

    const std::string html = R"(
        <html><body>
            <p>Visible text</p>
            <svg><text>HIDDEN_SVG_MARKER</text></svg>
        </body></html>
    )";

    const ParsedPage page = htmlParser.parse(html);

    EXPECT_NE(page.text.find("Visible text"), std::string::npos);
    EXPECT_EQ(page.text.find("HIDDEN_SVG_MARKER"), std::string::npos);
}

TEST(HtmlParserTests, ignoresCanvasFallbackText)
{
    HtmlParser htmlParser;

    const std::string html = R"(
        <html><body>
            <p>Visible text</p>
            <canvas>HIDDEN_CANVAS_MARKER</canvas>
        </body></html>
    )";

    const ParsedPage page = htmlParser.parse(html);

    EXPECT_NE(page.text.find("Visible text"), std::string::npos);
    EXPECT_EQ(page.text.find("HIDDEN_CANVAS_MARKER"), std::string::npos);
}

TEST(HtmlParserTests, keepsLinksInsideScriptFilteredPages)
{
    HtmlParser htmlParser;

    const std::string html = R"(
        <html><body>
            <script>HIDDEN_SCRIPT_MARKER</script>
            <a href="/article">Article link</a>
        </body></html>
    )";

    const ParsedPage page = htmlParser.parse(html);

    EXPECT_EQ(page.text.find("HIDDEN_SCRIPT_MARKER"), std::string::npos);

    ASSERT_EQ(page.links.size(), 1u);
    EXPECT_EQ(page.links[0], "/article");
}

TEST(HtmlParserTests, separatesTextFromAdjacentElements)
{
    HtmlParser htmlParser;

    const std::string html = R"(
        <html><body>
            <div>alpha</div><div>beta</div>
            <p>gamma <strong>delta</strong></p>
        </body></html>
    )";

    const ParsedPage page = htmlParser.parse(html);

    EXPECT_NE(page.text.find("alpha"), std::string::npos);
    EXPECT_NE(page.text.find("beta"), std::string::npos);
    EXPECT_NE(page.text.find("gamma"), std::string::npos);
    EXPECT_NE(page.text.find("delta"), std::string::npos);

    EXPECT_EQ(page.text.find("alphabeta"), std::string::npos);
    EXPECT_EQ(page.text.find("gammadelta"), std::string::npos);
}

TEST(HtmlParserTests, normalizesRepeatedWhitespace)
{
    HtmlParser htmlParser;

    const std::string html = R"(
        <html><body>
            <p>alpha       beta
               gamma</p>
        </body></html>
    )";

    const ParsedPage page = htmlParser.parse(html);

    EXPECT_NE(page.text.find("alpha beta gamma"), std::string::npos);
}
