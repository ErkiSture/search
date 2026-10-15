#include<iostream>
#include<string>

#include "query/querier.h"
#include "storage/storage.h"
#include "index/index.h"

int main(int argc, char** argv) {
	Storage storage("data");
	Querier querier(storage);

	std::string query;

	while (true) {
		std::cout << "Search (or 'exit'): ";
		if ((!std::getline(std::cin, query) || query == "exit")) {
			break;
		}

		auto results = querier.search(query);

		//print results
	}


	return 1;
}