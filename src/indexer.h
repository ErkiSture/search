#pragma once

#include<string>
#include<unordered_map>
#include"index.h"

class Storage;

class Indexer {
public:
	explicit Indexer(Storage& storage);
	Index build_index_from_storage();
private:
	Storage& storage_;
	void print_index(const Index& index);
};
