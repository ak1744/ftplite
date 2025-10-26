#include "MetadataStore.hpp"
#include <stdexcept>
#include <sstream>
#include <iostream>

#define SQLITE_OK 0
#include <sqlite3.h>

static int64_t to_i64(uint64_t v) { return static_cast<int64_t>(v); }

MetadataStore::MetadataStore(const std::filesystem::path& db_path) {
    if (sqlite3_open(db_path.string().c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("sqlite3_open failed");
    }
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("PRAGMA foreign_keys=ON;");
    ensureSchema();
}

MetadataStore::~MetadataStore() {
    if (db_) sqlite3_close(db_);
}

void MetadataStore::exec(const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "sqlite error";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}

void MetadataStore::ensureSchema() {
    // 'files' and 'resume' tables per design doc §2.4.  :contentReference[oaicite:2]{index=2}
    exec(
        "CREATE TABLE IF NOT EXISTS files ("
        "  file_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  size INTEGER NOT NULL,"
        "  checksum TEXT,"
        "  uploaded_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  download_count INTEGER NOT NULL DEFAULT 0,"
        "  UNIQUE(name)"
        ");"
    );
    exec(
        "CREATE TABLE IF NOT EXISTS resume ("
        "  resume_id TEXT PRIMARY KEY,"
        "  file_id INTEGER NOT NULL,"
        "  offset INTEGER NOT NULL,"
        "  chunk_size INTEGER NOT NULL,"
        "  timestamp DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY(file_id) REFERENCES files(file_id)"
        ");"
    );
    exec("CREATE INDEX IF NOT EXISTS idx_files_uploaded_at ON files(uploaded_at DESC);");
}

int MetadataStore::insertFile(const std::string& name, uint64_t size, std::optional<std::string> checksum) {
    const char* sql = "INSERT INTO files(name,size,checksum) VALUES(?,?,?);";
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error("prepare failed");
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, to_i64(size));
    if (checksum.has_value()) sqlite3_bind_text(st, 3, checksum->c_str(), -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, 3);

    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        throw std::runtime_error("insertFile failed");
    }
    sqlite3_finalize(st);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

bool MetadataStore::getFile(int file_id, FileRow& out) {
    const char* sql =
        "SELECT file_id,name,size,checksum,uploaded_at,download_count "
        "FROM files WHERE file_id=?;";
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(st, 1, file_id);

    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out.file_id = sqlite3_column_int(st, 0);
        out.name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        out.size = static_cast<uint64_t>(sqlite3_column_int64(st, 2));
        if (sqlite3_column_type(st, 3) == SQLITE_NULL) out.checksum.reset();
        else out.checksum = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st, 3)));
        out.uploaded_at = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
        out.download_count = sqlite3_column_int(st, 5);
        ok = true;
    }
    sqlite3_finalize(st);
    return ok;
}

std::vector<FileRow> MetadataStore::listFilesNewestFirst(int limit) {
    const char* sql =
        "SELECT file_id,name,size,checksum,uploaded_at,download_count "
        "FROM files ORDER BY uploaded_at DESC, file_id DESC LIMIT ?;";
    sqlite3_stmt* st{};
    std::vector<FileRow> rows;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) return rows;
    sqlite3_bind_int(st, 1, limit);

    while (sqlite3_step(st) == SQLITE_ROW) {
        FileRow r{};
        r.file_id = sqlite3_column_int(st, 0);
        r.name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        r.size = static_cast<uint64_t>(sqlite3_column_int64(st, 2));
        if (sqlite3_column_type(st, 3) == SQLITE_NULL) r.checksum.reset();
        else r.checksum = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st, 3)));
        r.uploaded_at = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
        r.download_count = sqlite3_column_int(st, 5);
        rows.push_back(std::move(r));
    }
    sqlite3_finalize(st);
    return rows;
}

bool MetadataStore::updateFileSize(int file_id, uint64_t size) {
    const char* sql = "UPDATE files SET size=? WHERE file_id=?;";
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(st, 1, to_i64(size));
    sqlite3_bind_int(st, 2, file_id);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

bool MetadataStore::updateFileChecksum(int file_id, const std::string& checksum) {
    const char* sql = "UPDATE files SET checksum=? WHERE file_id=?;";
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, checksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, file_id);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

bool MetadataStore::incrementDownloadCount(int file_id) {
    const char* sql = "UPDATE files SET download_count=download_count+1 WHERE file_id=?;";
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(st, 1, file_id);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

bool MetadataStore::upsertResume(const std::string& resume_id, int file_id, uint64_t offset, uint32_t chunk_size) {
    const char* sql =
        "INSERT INTO resume(resume_id,file_id,offset,chunk_size) VALUES(?,?,?,?) "
        "ON CONFLICT(resume_id) DO UPDATE SET "
        "  file_id=excluded.file_id, offset=excluded.offset, chunk_size=excluded.chunk_size, "
        "  timestamp=CURRENT_TIMESTAMP;";
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, resume_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, file_id);
    sqlite3_bind_int64(st, 3, to_i64(offset));
    sqlite3_bind_int(st, 4, static_cast<int>(chunk_size));
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

bool MetadataStore::getResume(const std::string& resume_id, ResumeRow& out) {
    const char* sql =
        "SELECT resume_id,file_id,offset,chunk_size,timestamp FROM resume WHERE resume_id=?;";
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, resume_id.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out.resume_id = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
        out.file_id = sqlite3_column_int(st, 1);
        out.offset = static_cast<uint64_t>(sqlite3_column_int64(st, 2));
        out.chunk_size = static_cast<uint32_t>(sqlite3_column_int(st, 3));
        out.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
        ok = true;
    }
    sqlite3_finalize(st);
    return ok;
}

bool MetadataStore::deleteResume(const std::string& resume_id) {
    const char* sql = "DELETE FROM resume WHERE resume_id=?;";
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, resume_id.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    return ok;
}

