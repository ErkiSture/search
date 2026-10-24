#include <gtest/gtest.h>
#include "storage/storage.h"
#include "crawl/frontier.h"
#include <string>

TEST(StorageTests, SavesFrontier) {
    Storage storage("test.db");

    Frontier frontier;
    frontier.push("https://example.com");
    frontier.push("https://google.com");

    storage.saveFrontier(frontier);

    Frontier loaded;
    storage.loadFrontier(loaded);

    EXPECT_EQ(loaded.tryPop(), "https://example.com");
    EXPECT_EQ(loaded.tryPop(), "https://google.com");
}

TEST(StorageTests, LoadsFrontier) {
    Storage storage("test.db");

    Frontier frontier;
    frontier.push("https://example.com");
    frontier.push("https://google.com");

    storage.saveFrontier(frontier);

    Frontier loaded;
    storage.loadFrontier(loaded);

    EXPECT_EQ(loaded.tryPop(), "https://example.com");
    EXPECT_EQ(loaded.tryPop(), "https://google.com");
    EXPECT_FALSE(loaded.tryPop().has_value());
}

TEST(StorageTests, OverwritesExistingFrontier) {
    Storage storage("test.db");

    Frontier first;
    first.push("https://old.com");

    storage.saveFrontier(first);

    Frontier second;
    second.push("https://example.com");
    second.push("https://google.com");

    storage.saveFrontier(second);

    Frontier loaded;
    storage.loadFrontier(loaded);

    EXPECT_EQ(loaded.tryPop(), "https://example.com");
    EXPECT_EQ(loaded.tryPop(), "https://google.com");
    EXPECT_FALSE(loaded.tryPop().has_value());
}

TEST(StorageTests, LoadsEmptyFrontier) {
    Storage storage("test.db");

    Frontier frontier;
    storage.saveFrontier(frontier);

    Frontier loaded;
    storage.loadFrontier(loaded);

    EXPECT_FALSE(loaded.tryPop().has_value());
}