#include "Server.hpp"
#include "ClientHandler.hpp"
#include "../../common/common.hpp"

#include <iostream>
#include <ws2tcpip.h>
#include <filesystem>
#include <thread>
#include "MetadataStore.hpp"
#include "FileManager.hpp"


namespace fs = std::filesystem;

Server::Server(const std::string& port, const std::filesystem::path& rootDir, const std::filesystem::path& dbPath)
    : root(rootDir)
{
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* res = nullptr;
    if (getaddrinfo(nullptr, port.c_str(), &hints, &res) != 0) {
        throw SocketError("getaddrinfo failed");
    }

    listenSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        freeaddrinfo(res);
        throw SocketError("socket failed");
    }

    BOOL yes = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    if (bind(listenSocket, res->ai_addr, (int)res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        throw SocketError("bind failed");
    }

    freeaddrinfo(res);

    if (listen(listenSocket, SOMAXCONN) != 0) {
        throw SocketError("listen failed");
    }

    meta_ = std::make_unique<MetadataStore>(dbPath);
    fm_ = std::make_unique<FileManager>(root);

    std::cout << "Server setup complete. Listening..." << std::endl;
}

Server::~Server() {
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
    }
}

void Server::start() { acceptLoop(); }

void Server::acceptLoop() {
    for (;;) {
        SOCKET clientSock = accept(listenSocket, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) { std::cerr << "accept failed\n"; continue; }
        std::thread([this, clientSock]() {
            ClientHandler handler(clientSock, this->root, *this->meta_, *this->fm_);
            handler.process();
            }).detach();
    }
}
