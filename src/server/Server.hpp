#pragma once
#include <string>
#include <filesystem>
#include <winsock2.h>

class MetadataStore;
class FileManager;

class Server {
public:
    Server(const std::string& port, const std::filesystem::path& rootDir, const std::filesystem::path& dbPath);
    ~Server();
    void start();

private:
    SOCKET listenSocket = INVALID_SOCKET;
    std::filesystem::path root;

    std::unique_ptr<MetadataStore> meta_;
    std::unique_ptr<FileManager>   fm_;

	void acceptLoop();

    void handleClient(SOCKET clientSocket);
};
