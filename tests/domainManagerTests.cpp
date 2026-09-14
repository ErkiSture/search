#include <gtest/gtest.h>
#include "domainManager.h"

TEST(DomainManagerTest, getRobotsUrl){
    DomainManager manager;

    EXPECT_EQ(
        "https://example.com/robots.txt",
        manager.getRobotsUrl("https://example.com")
    );

    EXPECT_EQ(
        "https://example.com/robots.txt",
        manager.getRobotsUrl("https://example.com/")
    );

    EXPECT_EQ(
        "https://example.com/robots.txt",
        manager.getRobotsUrl("https://example.com/test")
    );

    EXPECT_EQ(
        "https://example.com/robots.txt",
        manager.getRobotsUrl("https://example.com/testing/abcabc")
    );

    EXPECT_EQ(
        "https://www.example.com/robots.txt",
        manager.getRobotsUrl("https://www.example.com")
    );

    EXPECT_EQ(
        "https://www.example.com/robots.txt",
        manager.getRobotsUrl("https://www.example.com/ABCDEF")
    );

    EXPECT_EQ(
        "http://example.com/robots.txt",
        manager.getRobotsUrl("http://example.com/page")
    );

    EXPECT_EQ(
        "https://example.com:8080/robots.txt",
        manager.getRobotsUrl("https://example.com:8080/page")
    );

    EXPECT_EQ(
        "https://example.com/robots.txt",
        manager.getRobotsUrl("https://example.com/page?foo=bar#section")
    );
}

TEST(DomainManagerTest, sameDomain){
    DomainManager manager;

    EXPECT_TRUE(manager.sameDomain(
        "https://example.com/page",
        "https://example.com/other"
    ));

    EXPECT_TRUE(manager.sameDomain(
        "https://example.com/page",
        "https://example.com/page"
    ));

    EXPECT_TRUE(manager.sameDomain(
        "https://google.com/",
        "https://google.com"
    ));

    EXPECT_TRUE(manager.sameDomain(
        "https://example.com/page",
        "https://example.com/robots.txt"
    ));

    EXPECT_TRUE(manager.sameDomain(
        "https://example.com:8080/page",
        "https://example.com:9090/other"
    ));

    EXPECT_FALSE(manager.sameDomain(
        "https://example.com/page",
        "https://google.com/page"
    ));

    EXPECT_FALSE(manager.sameDomain(
        "https://example.com/page",
        "https://www.example.com/page"
    ));

    EXPECT_FALSE(manager.sameDomain(
        "https://api.example.com/page",
        "https://example.com/page"
    ));

    EXPECT_FALSE(manager.sameDomain(
        "https://foo.example.com/page",
        "https://bar.example.com/page"
    ));
}