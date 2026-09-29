#include <gtest/gtest.h>
#include "domainManager.h"
#include "systemClock.h"
#include <chrono>

TEST(DomainManagerTest, getRobotsUrl)
{
    SystemClock clock;
    DomainManager manager(clock);

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

TEST(DomainManagerTest, sameDomain)
{
    SystemClock clock;
    DomainManager manager(clock);

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

    //EXPECT_TRUE(manager.sameDomain(
    //    "https://example.com:8080/page",
    //    "https://example.com:9090/other"
    //));

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

    EXPECT_FALSE(manager.sameDomain(
        "https://example.com",
        "https://example.se"
    ));
}

TEST(DomainManagerTest, AllowsUrlsNotMatchingDisallow)
{
    SystemClock clock;
    DomainManager domainManager(clock);

    std::string domain = "https://example.com";
    std::string robotsText =
        "User-agent: *\n"
        "Disallow: /private\n"
        "Disallow: /admin\n"
        "Allow: /private/public\n";

    domainManager.saveRobotsResult(domain, robotsText);

    EXPECT_TRUE(domainManager.isAllowed("https://example.com/"));
    EXPECT_TRUE(domainManager.isAllowed("https://example.com/test"));
    EXPECT_TRUE(domainManager.isAllowed("https://example.com/public"));
}

TEST(DomainManagerTest, DisallowsMatchingUrls) 
{
    SystemClock clock;
    DomainManager domainManager(clock);

    std::string domain = "https://example.com";
    std::string robotsText =
        "User-agent: *\n"
        "Disallow: /private\n"
        "Disallow: /admin\n";

    domainManager.saveRobotsResult(domain, robotsText);

    EXPECT_FALSE(domainManager.isAllowed("https://example.com/private"));
    EXPECT_FALSE(domainManager.isAllowed("https://example.com/private1"));
    EXPECT_FALSE(domainManager.isAllowed("https://example.com/private1/test"));
    EXPECT_FALSE(domainManager.isAllowed("https://example.com/admin"));
}


TEST(DomainManagerTest, AllowOverridesDisallow) 
{
    SystemClock clock;
    DomainManager domainManager(clock);

    std::string domain = "https://example.com";
    std::string robotsText =
        "User-agent: *\n"
        "Disallow: /private\n"
        "Allow: /private/public\n";

    domainManager.saveRobotsResult(domain, robotsText);

    EXPECT_TRUE(domainManager.isAllowed("https://example.com/private/public"));
    EXPECT_TRUE(domainManager.isAllowed("https://example.com/private/public1"));
    EXPECT_TRUE(domainManager.isAllowed("https://example.com/private/public/test"));
    EXPECT_TRUE(domainManager.isAllowed("https://example.com/private/public1/test"));
}

TEST(DomainManagerTest, UnmatchingDomainReturnsFetchRobots)
{
    SystemClock clock;
    DomainManager domainManager(clock);

    EXPECT_EQ(domainManager.check("https://example.com"), RequestStatus::FetchRobots);
    EXPECT_EQ(domainManager.check("https://example.se"), RequestStatus::FetchRobots);
    EXPECT_EQ(domainManager.check("https://foo.example.com"), RequestStatus::FetchRobots);
    EXPECT_EQ(domainManager.check("https://bar.example.com"), RequestStatus::FetchRobots);
    EXPECT_EQ(domainManager.check("https://test.com"), RequestStatus::FetchRobots);
}

TEST(DomainManagerTest, RequestToSameDomainWhileWaitingReturnsWait)
{
    SystemClock clock;
    DomainManager domainManager(clock);

    // Fetching from same domain immediately after first request to domain should signal waiting
    EXPECT_EQ(domainManager.check("https://example.com"), RequestStatus::FetchRobots);
    EXPECT_EQ(domainManager.check("https://example.com"), RequestStatus::Wait);
    EXPECT_EQ(domainManager.check("https://example.com/private"), RequestStatus::Wait);
}



class FakeClock : public Clock {
public:
    FakeClock()
        : time(std::chrono::steady_clock::time_point{})
    {
    }

    std::chrono::steady_clock::time_point now() const override {
        return time;
    }

    void advance(std::chrono::milliseconds amount) {
        time += amount;
    }
private:
    std::chrono::steady_clock::time_point time;
};



TEST(DomainManagerTest, RequestAfterSavedRobotsReturnsPermission)
{
    FakeClock clock;
    DomainManager domainManager(clock);

    domainManager.saveRobotsResult("https://example.com",
        "User-agent: *\n"
        "Disallow: /private\n"
    );

    EXPECT_EQ(domainManager.check("https://example.com"), RequestStatus::Wait);    
    
    clock.advance(std::chrono::milliseconds(499));
    EXPECT_EQ(domainManager.check("https://example.com"), RequestStatus::Wait);

    clock.advance(std::chrono::milliseconds(500));
    EXPECT_EQ(domainManager.check("https://example.com"), RequestStatus::Allowed);
}


TEST(DomainManagerTest, FailedRobotsRequestMarksDomainAsFetched)
{
    SystemClock clock;
    DomainManager domainManager(clock);

    domainManager.saveRobotsResult("https://example.com",
        "User-agent: *\n"
        "Disallow: /private\n"
    );

}