#include<string>
#include<unordered_set>
#include"storage/storage.h"
#include<filesystem>
#include<iostream>
#include<sqlite3.h>

namespace fs = std::filesystem;

Storage::Storage(const fs::path& dataDir) {
	fs::create_directories(dataDir);

	fs::path dbPath = dataDir / "search.db";

	if (sqlite3_open(dbPath.string().c_str(), &db_) != SQLITE_OK) {
		std::string error = db_
			? sqlite3_errmsg(db_)
			: "Could not allocate SQLite connection";

		if (db_) {
			sqlite3_close(db_);
			db_ = nullptr;
		}

		throw std::runtime_error("Failed to open database: " + error);
	}

	initializeDatabase();
}

Storage::~Storage() {
	if (db_) {
		sqlite3_close(db_);
	}
}

void Storage::initializeDatabase()
{
	const char* sql = R"(
		CREATE TABLE IF NOT EXISTS crawl_urls (
			url TEXT PRIMARY KEY,
			status TEXT NOT NULL,
			redirect_to TEXT
		);

		CREATE TABLE IF NOT EXISTS pages (
			url TEXT PRIMARY KEY,
			content TEXT NOT NULL
			
		);	

		CREATE TABLE IF NOT EXISTS words (
			id INTEGER PRIMARY KEY,
			word TEXT UNIQUE NOT NULL
			
		);

		CREATE TABLE IF NOT EXISTS postings (
			word_id INTEGER NOT NULL,
			url TEXT NOT NULL,
			frequency INTEGER NOT NULL,
			PRIMARY KEY (word_id, url)
		);

		CREATE TABLE IF NOT EXISTS domains (
			host TEXT PRIMARY KEY,
			robots_txt TEXT
		);
	)";

	char* error = nullptr;

	if (sqlite3_exec(db_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
		std::string message = error ? error : sqlite3_errmsg(db_);
		sqlite3_free(error);
		throw std::runtime_error("Failed to create tables: " + message);
	}
}

void Storage::savePage(const std::string& url, const std::string& text)
{
	const char* crawlSql = R"(
        INSERT INTO crawl_urls (url, status)
        VALUES (?, 'fetched')
        ON CONFLICT(url) DO UPDATE SET status = 'fetched';
    )";

	const char* pageSql = R"(
        INSERT INTO pages (url, content)
        VALUES (?, ?)
        ON CONFLICT(url) DO UPDATE SET content = excluded.content;
    )";

	sqlite3_stmt* stmt = nullptr;

	// Update crawl_urls
	if (sqlite3_prepare_v2(db_, crawlSql, -1, &stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error(sqlite3_errmsg(db_));

	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

	int result = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (result != SQLITE_DONE)
		throw std::runtime_error(sqlite3_errmsg(db_));

	// Save page content
	if (sqlite3_prepare_v2(db_, pageSql, -1, &stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error(sqlite3_errmsg(db_));

	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, text.c_str(), -1, SQLITE_TRANSIENT);

	result = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (result != SQLITE_DONE)
		throw std::runtime_error(sqlite3_errmsg(db_));
}

std::string Storage::loadPage(const std::string& url)
{
	const char* sql =
		"SELECT content FROM pages WHERE url = ?;";

	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

	int result = sqlite3_step(stmt);
	std::string content;

	if (result == SQLITE_ROW) {
		const auto* value = sqlite3_column_text(stmt, 0);

		if (value) {
			content = reinterpret_cast<const char*>(value);
		}
	}
	else if (result != SQLITE_DONE) {
		std::string error = sqlite3_errmsg(db_);
		sqlite3_finalize(stmt);
		throw std::runtime_error(error);
	}

	sqlite3_finalize(stmt);
	return content;
}

Index Storage::loadIndex()
{
	Index index;

	const char* sql = R"(
        SELECT words.word, postings.url, postings.frequency
        FROM words
        JOIN postings ON words.id = postings.word_id
    )";

	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		std::cerr << "Failed to load index: " << sqlite3_errmsg(db_) << '\n';
		return index;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		std::string word = reinterpret_cast<const char*>(
			sqlite3_column_text(stmt, 0));

		std::string url = reinterpret_cast<const char*>(
			sqlite3_column_text(stmt, 1));

		int frequency = sqlite3_column_int(stmt, 2);

		index[word][url] = frequency;
	}

	sqlite3_finalize(stmt);
	return index;
}

std::unordered_set<std::string> Storage::loadVisitedUrls()
{
	std::unordered_set<std::string> urls;

	sqlite3_stmt* stmt = nullptr;

	const char* sql = R"(SELECT url FROM crawl_urls;)";

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const unsigned char* value = sqlite3_column_text(stmt, 0);

		if (value) {
			urls.insert(reinterpret_cast<const char*>(value));
		}
	}

	sqlite3_finalize(stmt);

	return urls;
}

void Storage::saveIndex(const Index& index)
{
	char* error = nullptr;

	if (sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &error) != SQLITE_OK) {
		std::string msg = error ? error : sqlite3_errmsg(db_);
		sqlite3_free(error);
		throw std::runtime_error(msg);
	}

	sqlite3_stmt* insertWord = nullptr;
	sqlite3_stmt* selectWord = nullptr;
	sqlite3_stmt* insertPosting = nullptr;

	try {
		sqlite3_prepare_v2(db_,
			"INSERT OR IGNORE INTO words (word) VALUES (?)",
			-1, &insertWord, nullptr);

		sqlite3_prepare_v2(db_,
			"SELECT id FROM words WHERE word = ?",
			-1, &selectWord, nullptr);

		sqlite3_prepare_v2(db_,
			"INSERT INTO postings (word_id, url, frequency) VALUES (?, ?, ?) "
			"ON CONFLICT(word_id, url) DO UPDATE SET frequency = excluded.frequency",
			-1, &insertPosting, nullptr);

		for (const auto& [word, postings] : index) {
			sqlite3_bind_text(insertWord, 1, word.c_str(), -1, SQLITE_TRANSIENT);
			if (sqlite3_step(insertWord) != SQLITE_DONE)
				throw std::runtime_error(sqlite3_errmsg(db_));

			sqlite3_reset(insertWord);
			sqlite3_clear_bindings(insertWord);

			sqlite3_bind_text(selectWord, 1, word.c_str(), -1, SQLITE_TRANSIENT);
			if (sqlite3_step(selectWord) != SQLITE_ROW)
				throw std::runtime_error(sqlite3_errmsg(db_));

			int wordId = sqlite3_column_int(selectWord, 0);
			sqlite3_reset(selectWord);
			sqlite3_clear_bindings(selectWord);

			for (const auto& [url, frequency] : postings) {
				sqlite3_bind_int(insertPosting, 1, wordId);
				sqlite3_bind_text(insertPosting, 2, url.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(insertPosting, 3, frequency);

				if (sqlite3_step(insertPosting) != SQLITE_DONE)
					throw std::runtime_error(sqlite3_errmsg(db_));

				sqlite3_reset(insertPosting);
				sqlite3_clear_bindings(insertPosting);
			}
		}

		sqlite3_finalize(insertWord);
		sqlite3_finalize(selectWord);
		sqlite3_finalize(insertPosting);

		if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &error) != SQLITE_OK) {
			std::string msg = error ? error : sqlite3_errmsg(db_);
			sqlite3_free(error);
			throw std::runtime_error(msg);
		}
	}
	catch (...) {
		sqlite3_finalize(insertWord);
		sqlite3_finalize(selectWord);
		sqlite3_finalize(insertPosting);
		sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
		throw;
	}
}


void Storage::savePageIndex(
	const std::string& url,
	const PageIndex& pageIndex)
{
	char* error = nullptr;

	if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &error)
		!= SQLITE_OK) {
		std::string message = error ? error : sqlite3_errmsg(db_);
		sqlite3_free(error);
		throw std::runtime_error(message);
	}

	sqlite3_stmt* wordStmt = nullptr;
	sqlite3_stmt* wordIdStmt = nullptr;
	sqlite3_stmt* postingStmt = nullptr;
	sqlite3_stmt* deleteStmt = nullptr;

	try {
		auto prepare = [&](const char* sql, sqlite3_stmt** stmt) {
			if (sqlite3_prepare_v2(db_, sql, -1, stmt, nullptr) != SQLITE_OK)
				throw std::runtime_error(sqlite3_errmsg(db_));
			};

		prepare("DELETE FROM postings WHERE url = ?;", &deleteStmt);
		prepare(
			"INSERT INTO words(word) VALUES (?) "
			"ON CONFLICT(word) DO NOTHING;",
			&wordStmt);
		prepare(
			"SELECT id FROM words WHERE word = ?;",
			&wordIdStmt);
		prepare(
			"INSERT INTO postings(word_id, url, frequency) "
			"VALUES (?, ?, ?) "
			"ON CONFLICT(word_id, url) "
			"DO UPDATE SET frequency = excluded.frequency;",
			&postingStmt);

		// Remove stale postings for this URL.
		sqlite3_bind_text(
			deleteStmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

		if (sqlite3_step(deleteStmt) != SQLITE_DONE)
			throw std::runtime_error(sqlite3_errmsg(db_));

		for (const auto& [word, frequency] : pageIndex) {
			sqlite3_bind_text(
				wordStmt, 1, word.c_str(), -1, SQLITE_TRANSIENT);

			if (sqlite3_step(wordStmt) != SQLITE_DONE)
				throw std::runtime_error(sqlite3_errmsg(db_));

			sqlite3_reset(wordStmt);
			sqlite3_clear_bindings(wordStmt);

			sqlite3_bind_text(
				wordIdStmt, 1, word.c_str(), -1, SQLITE_TRANSIENT);

			if (sqlite3_step(wordIdStmt) != SQLITE_ROW)
				throw std::runtime_error(sqlite3_errmsg(db_));

			int wordId = sqlite3_column_int(wordIdStmt, 0);

			sqlite3_reset(wordIdStmt);
			sqlite3_clear_bindings(wordIdStmt);

			sqlite3_bind_int(postingStmt, 1, wordId);
			sqlite3_bind_text(
				postingStmt, 2, url.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(postingStmt, 3, frequency);

			if (sqlite3_step(postingStmt) != SQLITE_DONE)
				throw std::runtime_error(sqlite3_errmsg(db_));

			sqlite3_reset(postingStmt);
			sqlite3_clear_bindings(postingStmt);
		}

		sqlite3_finalize(deleteStmt);
		sqlite3_finalize(wordStmt);
		sqlite3_finalize(wordIdStmt);
		sqlite3_finalize(postingStmt);

		deleteStmt = wordStmt = wordIdStmt = postingStmt = nullptr;

		if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &error)
			!= SQLITE_OK) {
			std::string message = error ? error : sqlite3_errmsg(db_);
			sqlite3_free(error);
			error = nullptr;
			throw std::runtime_error(message);
		}
	}
	catch (...) {
		sqlite3_finalize(deleteStmt);
		sqlite3_finalize(wordStmt);
		sqlite3_finalize(wordIdStmt);
		sqlite3_finalize(postingStmt);
		sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
		throw;
	}
}

PostingList Storage::loadIndexOfWord(const std::string& word)
{
	PostingList postingList;
	sqlite3_stmt* stmt = nullptr;
	int result = SQLITE_OK;

	// Find the word ID.
	const char* sqlWord = R"(
        SELECT id
        FROM words
        WHERE word = ?;
    )";

	result = sqlite3_prepare_v2(db_, sqlWord, -1, &stmt, nullptr);
	if (result != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	result = sqlite3_bind_text(
		stmt, 1, word.c_str(), -1, SQLITE_TRANSIENT
	);
	if (result != SQLITE_OK) {
		sqlite3_finalize(stmt);
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	result = sqlite3_step(stmt);

	if (result == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return postingList; // Word doesn't exist.
	}

	if (result != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	int wordId = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	stmt = nullptr;

	// Load postings for this word.
	const char* sqlPostings = R"(
        SELECT url, frequency
        FROM postings
        WHERE word_id = ?;
    )";

	result = sqlite3_prepare_v2(
		db_, sqlPostings, -1, &stmt, nullptr
	);
	if (result != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	result = sqlite3_bind_int(stmt, 1, wordId);
	if (result != SQLITE_OK) {
		sqlite3_finalize(stmt);
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
		const auto* url = sqlite3_column_text(stmt, 0);
		int frequency = sqlite3_column_int(stmt, 1);

		if (url != nullptr) {
			postingList.emplace(
				reinterpret_cast<const char*>(url),
				frequency
			);
		}
	}

	sqlite3_finalize(stmt);

	if (result != SQLITE_DONE) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	return postingList;
}

void Storage::clearDatabase() {
	const char* sql = R"(
        DELETE FROM postings;
        DELETE FROM words;
        DELETE FROM pages;
        DELETE FROM crawl_urls;
        DELETE FROM domains;
    )";

	char* errorMessage = nullptr;

	int result = sqlite3_exec(db_, sql, nullptr, nullptr, &errorMessage);

	if (result != SQLITE_OK) {
		std::string error = errorMessage ? errorMessage : "Unknown SQLite error";
		sqlite3_free(errorMessage);
		throw std::runtime_error("Failed to clear database: " + error);
	}
}

bool Storage::checkVisited(const std::string& url) {
	const char* sql = "SELECT 1 FROM crawl_urls WHERE url = ? LIMIT 1;";
	sqlite3_stmt * stmt;

	sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

	int result = sqlite3_step(stmt);
	bool exists = (result == SQLITE_ROW);

	sqlite3_finalize(stmt);
	return exists;
}

void Storage::markFetching(const std::string& url)
{
	const char* sql =
		"INSERT OR IGNORE INTO crawl_urls (url, status) "
		"VALUES (?, 'fetching');";

	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		std::string error = sqlite3_errmsg(db_);
		sqlite3_finalize(stmt);
		throw std::runtime_error(error);
	}

	sqlite3_finalize(stmt);
}

int Storage::countSaved()
{
	const char* sql =
		"SELECT COUNT(*) FROM crawl_urls WHERE status = ? or status = ?;";

	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	sqlite3_bind_text(stmt, 1, "fetched", -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, "fetching", -1, SQLITE_STATIC);

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		std::string error = sqlite3_errmsg(db_);
		sqlite3_finalize(stmt);
		throw std::runtime_error(error);
	}

	int count = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	return count;
}

void Storage::markFailed(const std::string& url)
{
	const char* sql = R"(
        INSERT INTO crawl_urls (url, status)
        VALUES (?, 'failed')
        ON CONFLICT(url) DO UPDATE SET status = 'failed';
    )";

	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

	int result = sqlite3_step(stmt);

	if (result != SQLITE_DONE) {
		std::string error = sqlite3_errmsg(db_);
		sqlite3_finalize(stmt);
		throw std::runtime_error(error);
	}

	sqlite3_finalize(stmt);
}

std::string Storage::getStatus(const std::string& url)
{
	const char* sql =
		"SELECT status FROM crawl_urls WHERE url = ? LIMIT 1;";

	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

	int result = sqlite3_step(stmt);
	std::string status;

	if (result == SQLITE_ROW) {
		const auto* value = sqlite3_column_text(stmt, 0);
		if (value) {
			status = reinterpret_cast<const char*>(value);
		}
	}
	else if (result != SQLITE_DONE) {
		std::string error = sqlite3_errmsg(db_);
		sqlite3_finalize(stmt);
		throw std::runtime_error(error);
	}

	sqlite3_finalize(stmt);
	return status;
}