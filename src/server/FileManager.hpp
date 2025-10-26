#pragma once
#include <filesystem>
#include <vector>
#include <cstdint>

class FileManager {
public:
    explicit FileManager(std::filesystem::path root);
    bool readChunk(int file_id, uint64_t offset, size_t maxBytes, std::vector<uint8_t>& out);
    bool writeChunk(int file_id, uint64_t offset, const std::vector<uint8_t>& data);
    bool allocateForNewFile(int file_id, const std::string& name);

    std::filesystem::path filePath(int file_id) const;

private:
    std::filesystem::path root_;
};
