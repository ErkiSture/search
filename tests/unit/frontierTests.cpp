#include <gtest/gtest.h>
#include "crawl/frontier.h"
#include <chrono>
#include "fakes/fakeClock.h"

class FrontierTests : public testing::Test {
protected:
    Frontier frontier;
    FakeClock clock;
};

TEST_F(FrontierTests, PushAndPop)
{
    frontier.push("url1");
    frontier.push("url2");

    EXPECT_EQ(frontier.tryPop(), "url1");
    EXPECT_EQ(frontier.tryPop(), "url2");
    EXPECT_FALSE(frontier.tryPop().has_value());
}

TEST_F(FrontierTests, PopEmptyFrontierReturnsNullopt)
{
    Frontier frontier;

    auto url = frontier.tryPop();

    EXPECT_FALSE(url.has_value());
}

TEST_F(FrontierTests, DoesNotPromoteBeforeReady)
{
    auto readyAt = clock.now() + std::chrono::seconds(5);

    frontier.schedule("example.com", "url1", readyAt);

    frontier.promoteReadyUrls(clock.now());

    EXPECT_EQ(frontier.frontierSize(), 0);
    EXPECT_EQ(frontier.delayedSize(), 1);
}

TEST_F(FrontierTests, PromotesWhenReady)
{
    auto readyAt = clock.now() + std::chrono::seconds(5);

    frontier.schedule("example.com", "url1", readyAt);

    clock.advance(std::chrono::seconds(5));
    frontier.promoteReadyUrls(clock.now());

    EXPECT_EQ(frontier.frontierSize(), 1);
    EXPECT_EQ(frontier.tryPop(), "url1");
}

TEST_F(FrontierTests, DifferentDomainsAreScheduledIndependently)
{
    auto readyAt = clock.now() + std::chrono::seconds(5);

    frontier.schedule("domain1", "url1", readyAt);
    frontier.schedule("domain2", "url2", readyAt);

    clock.advance(std::chrono::seconds(5));
    frontier.promoteReadyUrls(clock.now());

    EXPECT_EQ(frontier.frontierSize(), 2);
    EXPECT_EQ(frontier.tryPop(), "url1");
    EXPECT_EQ(frontier.tryPop(), "url2");
}

TEST_F(FrontierTests, MultipleUrlsSameDomainArePromotedOneAtATime)
{
    auto readyAt = clock.now() + std::chrono::seconds(5);

    frontier.schedule("example.com", "url1", readyAt);
    frontier.schedule("example.com", "url2", readyAt);

    clock.advance(std::chrono::seconds(5));
    frontier.promoteReadyUrls(clock.now());

    EXPECT_EQ(frontier.frontierSize(), 1);
    EXPECT_EQ(frontier.tryPop(), "url1");

    frontier.scheduleNextForDomain("example.com", clock.now());
    frontier.promoteReadyUrls(clock.now());

    EXPECT_EQ(frontier.frontierSize(), 1);
    EXPECT_EQ(frontier.tryPop(), "url2");

    EXPECT_TRUE(frontier.empty());
}

TEST_F(FrontierTests, PromotesMultipleReadyDomains)
{
    auto readyAt = clock.now() + std::chrono::seconds(5);

    frontier.schedule("domain1", "url1", readyAt);
    frontier.schedule("domain2", "url2", readyAt);
    frontier.schedule("domain3", "url3", readyAt);

    clock.advance(std::chrono::seconds(5));
    frontier.promoteReadyUrls(clock.now());

    EXPECT_EQ(frontier.frontierSize(), 3);
    EXPECT_EQ(frontier.delayedSize(), 0);
}

TEST_F(FrontierTests, NextReadyAtReturnsEarliestTime)
{
    auto later = clock.now() + std::chrono::seconds(10);
    auto earlier = clock.now() + std::chrono::seconds(5);

    frontier.schedule("domain1", "url1", later);
    frontier.schedule("domain2", "url2", earlier);

    EXPECT_EQ(frontier.nextReadyAt(), earlier);
}

TEST_F(FrontierTests, ScheduleNextDoesNothingWhenDomainHasNoWaitingUrls)
{
    frontier.scheduleNextForDomain(
        "example.com",
        clock.now()
    );

    EXPECT_EQ(frontier.delayedSize(), 0);
}

TEST_F(FrontierTests, EmptyReflectsFrontierAndDelayedUrls)
{
    EXPECT_TRUE(frontier.empty());

    frontier.push("url1");
    EXPECT_FALSE(frontier.empty());

    frontier.tryPop();
    EXPECT_TRUE(frontier.empty());

    frontier.schedule(
        "example.com",
        "url2",
        clock.now() + std::chrono::seconds(5)
    );

    EXPECT_FALSE(frontier.empty());
}

TEST_F(FrontierTests, ReleaseRobotsWaitersReturnsAllUrls)
{
    frontier.waitForRobots("example.com", "url1");
    frontier.waitForRobots("example.com", "url2");

    auto urls = frontier.releaseRobotsWaiters("example.com");

    ASSERT_EQ(urls.size(), 2);
    EXPECT_EQ(urls[0], "url1");
    EXPECT_EQ(urls[1], "url2");
}

TEST_F(FrontierTests, ReleaseRobotsWaitersOnlyAffectsRequestedDomain)
{
    frontier.waitForRobots("domain1", "url1");
    frontier.waitForRobots("domain2", "url2");

    auto urls = frontier.releaseRobotsWaiters("domain1");

    ASSERT_EQ(urls.size(), 1);
    EXPECT_EQ(urls[0], "url1");

    EXPECT_TRUE(frontier.hasRobotsWaiters());
    EXPECT_EQ(frontier.robotsWaitingSize(), 1);
}

TEST_F(FrontierTests, ReleaseUnknownRobotsDomainReturnsEmpty)
{
    auto urls = frontier.releaseRobotsWaiters("unknown.com");

    EXPECT_TRUE(urls.empty());
    EXPECT_FALSE(frontier.hasRobotsWaiters());
}

TEST_F(FrontierTests, ReleaseRobotsWaitersRemovesDomainEntry)
{
    frontier.waitForRobots("example.com", "url1");

    frontier.releaseRobotsWaiters("example.com");

    EXPECT_EQ(frontier.robotsWaitingSize(), 0);
    EXPECT_FALSE(frontier.hasRobotsWaiters());

    auto urls = frontier.releaseRobotsWaiters("example.com");
    EXPECT_TRUE(urls.empty());
}