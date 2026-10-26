#include<string>
#include<unordered_set>
#include"storage/storage.h"
#include<filesystem>
#include<iostream>
#include<sqlite3.h>

namespace fs = std::filesystem;


Storage::Storage(const fs::path& dataDir)
{
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

		throw std::runtime_error(
			"Failed to open database: " + error
		);
	}

	try {
		initializeDatabase();
		prepareStatements();
	}
	catch (...) {
		finalizeStatements();

		if (db_) {
			sqlite3_close(db_);
			db_ = nullptr;
		}

		throw;
	}
}

Storage::~Storage()
{
	finalizeStatements();

	if (db_) {
		sqlite3_close(db_);
		db_ = nullptr;
	}
}

void Storage::prepareStatements()
{
	const char* markFetchingSql =
		"INSERT OR IGNORE INTO crawl_urls (url, status) "
		"VALUES (?, 'fetching');";

	if (sqlite3_prepare_v2(
		db_,
		markFetchingSql,
		-1,
		&markFetchingStmt_,
		nullptr
	) != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	const char* checkVisitedSql =
		"SELECT 1 FROM crawl_urls WHERE url = ? LIMIT 1;";

	if (sqlite3_prepare_v2(db_, checkVisitedSql, -1, &checkVisitedStmt_, nullptr) != SQLITE_OK) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}
}


void Storage::finalizeStatements()
{
	if (markFetchingStmt_) {
		sqlite3_finalize(markFetchingStmt_);
		markFetchingStmt_ = nullptr;
	}

	if (checkVisitedStmt_) {
		sqlite3_finalize(checkVisitedStmt_);
		checkVisitedStmt_ = nullptr;
	}
}


void Storage::initializeDatabase()
{
	char* error = nullptr;

	if (sqlite3_exec(
		db_,
		"PRAGMA journal_mode=WAL;",
		nullptr,
		nullptr,
		&error
	) != SQLITE_OK) {
		std::string message = error ? error : sqlite3_errmsg(db_);
		sqlite3_free(error);
		throw std::runtime_error("Failed to enable WAL mode: " + message);
	}

	sqlite3_busy_timeout(db_, 5000);

	const char* sql = R"(
		CREATE TABLE IF NOT EXISTS crawl_urls (
			url TEXT PRIMARY KEY,
			status TEXT NOT NULL,
			redirect_to TEXT
		);

		CREATE TABLE IF NOT EXISTS crawl_frontier (
			url TEXT PRIMARY KEY
		);

		CREATE TABLE IF NOT EXISTS pages (
			url TEXT PRIMARY KEY,
			content TEXT NOT NULL,
			title TEXT,
			description TEXT
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

		CREATE INDEX IF NOT EXISTS idx_postings_url
		ON postings(url);

		CREATE INDEX IF NOT EXISTS idx_postings_word_id 
		ON postings(word_id);

		CREATE INDEX IF NOT EXISTS idx_postings_word_id_url_frequency
		ON postings(word_id, url, frequency);
	)";

	if (sqlite3_exec(db_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
		std::string message = error ? error : sqlite3_errmsg(db_);
		sqlite3_free(error);
		throw std::runtime_error("Failed to create tables: " + message);
	}
}

void Storage::savePageContent(
	const std::string& url,
	const std::string& title,
	const std::string& description,
	const std::string& text
) {
	const char* sql = R"(
        INSERT INTO pages (url, title, description, content)
        VALUES (?, ?, ?, ?)
        ON CONFLICT(url) DO UPDATE SET
            title = excluded.title,
            description = excluded.description,
            content = excluded.content;
    )";

	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error(sqlite3_errmsg(db_));

	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, description.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, text.c_str(), -1, SQLITE_TRANSIENT);

	int result = sqlite3_step(stmt);
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

	const char* sql = R"(
        SELECT p.url, p.frequency
        FROM words w
        JOIN postings p ON p.word_id = w.id
        WHERE w.word = ?;
    )";

	int result = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

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

std::pair<std::string, std::string> Storage::loadPageMetadata(
	const std::string& url)
{
	const char* sql =
		"SELECT title, description FROM pages WHERE url = ?;";

	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error(sqlite3_errmsg(db_));

	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

	std::pair<std::string, std::string> metadata;

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		const auto* title = sqlite3_column_text(stmt, 0);
		const auto* description = sqlite3_column_text(stmt, 1);

		metadata.first = title
			? reinterpret_cast<const char*>(title) : "";
		metadata.second = description
			? reinterpret_cast<const char*>(description) : "";
	}

	sqlite3_finalize(stmt);
	return metadata;
}

std::unordered_map<std::string, std::pair<std::string, std::string>>
Storage::loadPageMetadataBulk(const std::vector<std::string>& urls)
{
	std::unordered_map<std::string, std::pair<std::string, std::string>> metadata;

	if (urls.empty()) {
		return metadata;
	}

	constexpr std::size_t BATCH_SIZE = 500;

	const char* sql =
		"SELECT url, title, description "
		"FROM pages "
		"WHERE url IN (";

	for (std::size_t i = 0; i < urls.size(); i += BATCH_SIZE) {
		std::size_t batchEnd = std::min(i + BATCH_SIZE, urls.size());
		std::size_t batchSize = batchEnd - i;

		std::string query = sql;

		for (std::size_t j = 0; j < batchSize; ++j) {
			query += (j == 0 ? "?" : ",?");
		}

		query += ");";

		sqlite3_stmt* stmt = nullptr;

		int result = sqlite3_prepare_v2(
			db_, query.c_str(), -1, &stmt, nullptr
		);

		if (result != SQLITE_OK) {
			throw std::runtime_error(sqlite3_errmsg(db_));
		}

		for (std::size_t j = 0; j < batchSize; ++j) {
			result = sqlite3_bind_text(
				stmt,
				static_cast<int>(j + 1),
				urls[i + j].c_str(),
				-1,
				SQLITE_TRANSIENT
			);

			if (result != SQLITE_OK) {
				sqlite3_finalize(stmt);
				throw std::runtime_error(sqlite3_errmsg(db_));
			}
		}

		while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
			const auto* url = sqlite3_column_text(stmt, 0);
			const auto* title = sqlite3_column_text(stmt, 1);
			const auto* description = sqlite3_column_text(stmt, 2);

			if (url != nullptr) {
				metadata.emplace(
					reinterpret_cast<const char*>(url),
					std::make_pair(
						title ? reinterpret_cast<const char*>(title) : "",
						description ? reinterpret_cast<const char*>(description) : ""
					)
				);
			}
		}

		sqlite3_finalize(stmt);

		if (result != SQLITE_DONE) {
			throw std::runtime_error(sqlite3_errmsg(db_));
		}
	}

	return metadata;
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

bool Storage::checkVisited(const std::string& url)
{
	int rc = sqlite3_bind_text(
		checkVisitedStmt_, 1, url.c_str(), -1, SQLITE_TRANSIENT
	);

	if (rc != SQLITE_OK) {
		std::string error = sqlite3_errmsg(db_);
		sqlite3_reset(checkVisitedStmt_);
		sqlite3_clear_bindings(checkVisitedStmt_);
		throw std::runtime_error(error);
	}

	rc = sqlite3_step(checkVisitedStmt_);
	bool exists = (rc == SQLITE_ROW);

	std::string error;
	if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
		error = sqlite3_errmsg(db_);
	}

	sqlite3_reset(checkVisitedStmt_);
	sqlite3_clear_bindings(checkVisitedStmt_);

	if (!error.empty()) {
		throw std::runtime_error(error);
	}

	return exists;
}


void Storage::markFetching(const std::string& url)
{
	int result = sqlite3_bind_text(
		markFetchingStmt_,
		1,
		url.c_str(),
		-1,
		SQLITE_TRANSIENT
	);

	if (result != SQLITE_OK) {
		sqlite3_reset(markFetchingStmt_);
		throw std::runtime_error(sqlite3_errmsg(db_));
	}

	result = sqlite3_step(markFetchingStmt_);

	sqlite3_reset(markFetchingStmt_);
	sqlite3_clear_bindings(markFetchingStmt_);

	if (result != SQLITE_DONE) {
		throw std::runtime_error(sqlite3_errmsg(db_));
	}
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

void Storage::markFetched(const std::string& url)
{
	const char* sql = R"(
        INSERT INTO crawl_urls (url, status)
        VALUES (?, 'fetched')
        ON CONFLICT(url) DO UPDATE
        SET status = 'fetched';
    )";

	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error(sqlite3_errmsg(db_));

	sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

	int result = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (result != SQLITE_DONE)
		throw std::runtime_error(sqlite3_errmsg(db_));
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

void Storage::saveFrontier(Frontier& frontier)
{
	char* error = nullptr;

	if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &error) != SQLITE_OK)
		throw std::runtime_error(error);

	try {
		sqlite3_exec(db_, "DELETE FROM crawl_frontier;", nullptr, nullptr, nullptr);

		const char* sql =
			"INSERT OR IGNORE INTO crawl_frontier (url) VALUES (?);";

		sqlite3_stmt* stmt = nullptr;

		if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
			throw std::runtime_error(sqlite3_errmsg(db_));

		// Copy the queue so the original remains unchanged
		auto temp = frontier.frontier_;

		while (!temp.empty()) {
			const auto& url = temp.front();

			sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

			if (sqlite3_step(stmt) != SQLITE_DONE)
				throw std::runtime_error(sqlite3_errmsg(db_));

			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);

			temp.pop();
		}

		sqlite3_finalize(stmt);

		if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
			throw std::runtime_error(sqlite3_errmsg(db_));
	}
	catch (...) {
		sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
		throw;
	}
}

void Storage::loadFrontier(Frontier& frontier)
{
	const char* sql = "SELECT url FROM crawl_frontier;";

	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error(sqlite3_errmsg(db_));

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const auto* value = sqlite3_column_text(stmt, 0);

		if (value) {
			frontier.push(reinterpret_cast<const char*>(value));
		}
	}

	sqlite3_finalize(stmt);
}