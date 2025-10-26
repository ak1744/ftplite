#include "FileManager.hpp"
#include <fstream>
#include <string>


FileManager::FileManager(std::filesystem::path root) : root_(std::move(root)) {
    std::filesystem::create_directories(root_);
}

std::filesystem::path FileManager::filePath(int file_id) const {
    // Store by ID; extension doesn’t matter functionally
    return root_ / (std::to_string(file_id) + ".bin");
}

bool FileManager::allocateForNewFile(int file_id, const std::string&) {
    auto p = filePath(file_id);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    return bool(f);
}

bool FileManager::readChunk(int file_id, uint64_t offset, size_t maxBytes, std::vector<uint8_t>& out) {
    auto p = filePath(file_id);
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    out.resize(maxBytes);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(maxBytes));
    std::streamsize got = in.gcount();
    if (got < 0) return false;
    out.resize(static_cast<size_t>(got));
    return true;
}

bool FileManager::writeChunk(int file_id, uint64_t offset, const std::vector<uint8_t>& data) {
    auto p = filePath(file_id);
    std::fstream io(p, std::ios::binary | std::ios::in | std::ios::out);
    if (!io) {
        io.open(p, std::ios::binary | std::ios::out | std::ios::trunc);
        io.close();
        io.open(p, std::ios::binary | std::ios::in | std::ios::out);
        if (!io) return false;
    }
    io.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    io.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return bool(io);
}

