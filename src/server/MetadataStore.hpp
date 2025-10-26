#pragma once
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <cstdint>

struct FileRow {
    int         file_id{};
    std::string name;
    uint64_t    size{};
    std::optional<std::string> checksum;
    std::string uploaded_at;
    int         download_count{};
};

struct ResumeRow {
    std::string resume_id;
    int         file_id{};
    uint64_t    offset{};
    uint32_t    chunk_size{};
    std::string timestamp;
};

class MetadataStore {
public:
    explicit MetadataStore(const std::filesystem::path& db_path);
    ~MetadataStore();

    int  insertFile(const std::string& name, uint64_t size, std::optional<std::string> checksum);
    bool getFile(int file_id, FileRow& out);
    std::vector<FileRow> listFilesNewestFirst(int limit = 1000);
    bool updateFileSize(int file_id, uint64_t size);
    bool updateFileChecksum(int file_id, const std::string& checksum);
    bool incrementDownloadCount(int file_id);
    bool upsertResume(const std::string& resume_id, int file_id, uint64_t offset, uint32_t chunk_size);
    bool getResume(const std::string& resume_id, ResumeRow& out);
    bool deleteResume(const std::string& resume_id);


private:
    struct sqlite3* db_{};

    void exec(const char* sql);
    void ensureSchema();
};
