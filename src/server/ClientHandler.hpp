#pragma once
#include <filesystem>
#include <winsock2.h>

class MetadataStore;
class FileManager;

class ClientHandler {
public:
    ClientHandler(SOCKET sock, const std::filesystem::path& root, MetadataStore& meta, FileManager& fm);
    void process();

private:
    SOCKET clientSock;
    std::filesystem::path rootDir;
    MetadataStore& meta_;
    FileManager& fm_;

    std::string makeListPayload();
};
