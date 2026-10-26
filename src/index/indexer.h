#pragma once

#include<string>
#include<unordered_map>
#include"index.h"
#include"storage/storage.h"


class Storage;

class Indexer {
public:
	explicit Indexer(Storage& storage);
	
	PageIndex indexPage(const std::string text);

private:
	Storage& storage_;
	void printIndex(const Index& index);
};
