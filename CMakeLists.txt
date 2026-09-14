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
    src/fetcher.cpp
    src/html_parser.cpp
)

add_executable(crawler
    src/crawler.cpp
    src/indexer.cpp
    src/link_extractor.cpp
    src/storage.cpp
    src/fetcher.cpp
    src/html_parser.cpp
    src/domainManager.cpp
    src/robotsTxtParser.cpp
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

add_executable(domainManagerTests
    tests/domainManagerTests.cpp
    src/domainManager.cpp
    src/robotsTxtParser.cpp
)

target_include_directories(domainManagerTests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(domainManagerTests
    PRIVATE
    GTest::gtest_main
)


include(GoogleTest)
gtest_discover_tests(domainManagerTests)