#include <gtest/gtest.h>
#include "robotsTxtParser.h"

TEST(RobotsTxtParserTest, ParsesDisallow)
{
    RobotsRules result = parseRobots(
        "User-agent:\n"
        "Disallow: /private\n"
    );

    ASSERT_EQ(result.disallowed.size(), 1);
    EXPECT_EQ(result.disallowed[0], "/private");
}

TEST(RobotsTxtParserTest, ParsesAllowAndDisallow)
{
    RobotsRules result = parseRobots(
        "User-agent:\n"
        "Allow: /public\n"
        "Disallow: /private\n"
    );

    ASSERT_EQ(result.allowed.size(), 1);
    ASSERT_EQ(result.disallowed.size(), 1);

    EXPECT_EQ(result.allowed[0], "/public");
    EXPECT_EQ(result.disallowed[0], "/private");
}

TEST(RobotsTxtParserTest, IgnoresOtherUserAgents)
{
    RobotsRules result = parseRobots(
        "User-agent: Googlebot\n"
        "Disallow: /google\n"
        "User-agent: *\n"
        "Disallow: /private\n"
    );

    ASSERT_EQ(result.disallowed.size(), 1);
    EXPECT_EQ(result.disallowed[0], "/private");
}

TEST(RobotsTxtParserTest, RemovesComments)
{
    RobotsRules result = parseRobots(
        "User-agent: *\n"
        "Disallow: /private # secret area\n"
    );

    EXPECT_EQ(result.disallowed.size(), 1);
    EXPECT_EQ(result.disallowed[0], "/private");
}

TEST(RobotsTxtParserTest, TestBlockMultiple)
{
    RobotsRules result = parseRobots(
        "User-agent: *\n"
        "Disallow: /private # secret area\n"
        "Disallow: /admin # admin \n"
        "Disallow: /test\n"
    );

    EXPECT_EQ(result.disallowed.size(), 3);
    EXPECT_EQ(result.disallowed[0], "/private");
    EXPECT_EQ(result.disallowed[1], "/admin");
    EXPECT_EQ(result.disallowed[2], "/test");
}

TEST(RobotsTxtParserTest, DisallowEmptySlash) 
{
    RobotsRules result = parseRobots(
        "User-agent: *\n"
        "Disallow: /\n"
    );

    EXPECT_EQ(result.disallowed.size(), 1);
    EXPECT_EQ(result.disallowed[0], "/");
}

TEST(RobotsTxtParserTest, Whitespace)
{
    RobotsRules result = parseRobots(
        "User-agent: *\n"
        "Disallow:/private        \n"
    );

    EXPECT_EQ(result.disallowed.size(), 1);
    EXPECT_EQ(result.disallowed[0], "/private");
}

TEST(RobotsTxtParserTest, AllowOverrideDisallow)
{
    RobotsRules result = parseRobots(
        "User-agent: *\n"
        "Disallow: /private\n"
        "Allow: /private/admin\n"
    );

    EXPECT_EQ(result.disallowed.size(), 1);    
    EXPECT_EQ(result.allowed.size(), 1);
    EXPECT_EQ(result.allowed[0], "/private/admin");
}