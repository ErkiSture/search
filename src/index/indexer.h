#pragma once

#include<string>
#include<unordered_map>
#include"index.h"
#include"storage/storage.h"

class Storage;

class Indexer {
public:
	explicit Indexer(Storage& storage);
	Index build_index_from_storage();
	
	//PageIndex indexPage(std::string text);

private:
	Storage& storage_;
	void print_index(const Index& index);
};
