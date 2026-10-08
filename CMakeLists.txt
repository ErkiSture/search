cmake_minimum_required (VERSION 3.10)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("search")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

add_executable(search
    src/main.cpp
    "src/net/fetcher.cpp"
    "src/parsing/htmlParser.cpp"
)

target_include_directories(search PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(crawler
    "src/crawl/main.cpp"
    "src/crawl/crawler.cpp"
    "src/index/indexer.cpp"
    "src/parsing/linkExtractor.cpp"
    "src/storage/storage.cpp"
    "src/net/fetcher.cpp"
    "src/parsing/htmlParser.cpp"
    "src/crawl/domainManager.cpp"
    "src/parsing/robotsTxtParser.cpp"
    "src/net/curlHttpClient.cpp"
)

target_include_directories(crawler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(search PRIVATE CURL::libcurl)
target_link_libraries(crawler PRIVATE CURL::libcurl)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(tests
    tests/domainManagerTests.cpp
    tests/robotsTxtParserTests.cpp
    "src/crawl/domainManager.cpp"
    "src/parsing/robotsTxtParser.cpp"
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tests
    PRIVATE
    GTest::gtest_main
)


include(GoogleTest)
gtest_discover_tests(tests)