#include <crow.h>
#include <cstdlib>
#include <iostream>
#include <string>

#include "storage/storage.h"
#include "query/querier.h"

int main()
{
    const char* dataDir = std::getenv("SEARCH_DATA_DIR");

    if (!dataDir) {
        dataDir = "data";
    }

    Storage storage(dataDir);
    Querier querier(storage);

    crow::SimpleApp app;

    CROW_ROUTE(app, "/api/search")
        ([&querier](const crow::request& req) {

        auto query = req.url_params.get("q");

        if (!query || std::string(query).empty()) {
            return crow::response(400, "Missing search query");
        }

        auto results = querier.search(query);

        crow::json::wvalue response;
        response["query"] = query;

        crow::json::wvalue::list resultList;

        for (const auto& result : results) {
            crow::json::wvalue item;

            item["url"] = result.url;
            item["title"] = result.title;
            item["description"] = result.description;
            item["score"] = result.score;

            resultList.push_back(std::move(item));
        }

        response["results"] = std::move(resultList);

        return crow::response(response);
            });

    app.port(8080).multithreaded().run();
}