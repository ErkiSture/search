#include"Querier.h"
#include"index/index.h"

Querier::Querier(Storage& storage) : storage_(storage)
{
}

std::vector<SearchResult> Querier::search(std::string query) 
{
	std::vector<SearchResult> results;

	Index index = storage_.loadIndex();
	return results;
}