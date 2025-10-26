#include "common.hpp"
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <sstream>

static std::unordered_map<int, std::string> resumeIdMap;
static std::unordered_map<int, uint64_t> resumeOffsetMap;

static std::string resumeFilename(int file_id) {
    return ".resume_" + std::to_string(file_id) + ".txt";
}

static void loadResume(int file_id) {
    std::ifstream in(resumeFilename(file_id));
    if (!in) return;
    std::string id;
    uint64_t off;
    if (in >> id >> off) {
        resumeIdMap[file_id] = id;
        resumeOffsetMap[file_id] = off;
        std::cout << "Resuming " << file_id << " from offset " << off << "\n";
    }
}

static void saveResume(int file_id) {
    if (resumeIdMap.find(file_id) == resumeIdMap.end()) return;
    std::ofstream out(resumeFilename(file_id), std::ios::trunc);
    if (!out) return;
    out << resumeIdMap[file_id] << "\n";
    out << resumeOffsetMap[file_id] << "\n";
}

static void deleteResumeFile(int file_id) {
    std::remove(resumeFilename(file_id).c_str());
}

static SOCKET connectTo(const char* host, const char* port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    if (getaddrinfo(host, port, &hints, &res) != 0)
        throw SocketError("getaddrinfo failed");

    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET)
        throw SocketError("socket failed");
    if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0)
        throw SocketError("connect failed");

    freeaddrinfo(res);
    return s;
}

static void doPing(SOCKET s) {
    sendMessage(s, PING, "");
    MsgHeader h{};
    std::string payload;
    recvMessage(s, h, payload);
    std::cout << "PONG: " << payload << "\n";
}

static void doList(SOCKET s, const std::string& path) {
    sendMessage(s, LIST_REQ, path);
    MsgHeader h{};
    std::string payload;
    recvMessage(s, h, payload);

    if (h.type == LIST_RESP) {
        std::cout << payload << "\n";
    }
    else {
        std::cout << "ERR: " << payload << "\n";
    }
}

static void doGet(SOCKET s, const std::string& filename) {
    std::string payload = filename;
    int file_id = 0;
    try { file_id = std::stoi(filename); }
    catch (...) {}

    loadResume(file_id);

    if (resumeIdMap.count(file_id)) {
        payload += "|" + resumeIdMap[file_id];
    }

    sendMessage(s, GET_REQ, payload);

    MsgHeader h{};
    recvMessage(s, h, payload);

    if (h.type != GET_RESP) {
        std::cout << "Download failed: " << payload << "\n";
        return;
    }

    uint64_t fileSize = std::stoull(payload);
    std::ofstream out(filename, std::ios::binary);

    const size_t BUF_SIZE = 64 * 1024;
    char buffer[BUF_SIZE];
    uint64_t received = 0;

    while (received < fileSize) {
        int toRead = (int)std::min<uint64_t>(BUF_SIZE, fileSize - received);
        recvAll(s, buffer, toRead);
        out.write(buffer, toRead);
        received += toRead;
        resumeOffsetMap[file_id] = received;
        saveResume(file_id);

        std::cout << "Downloaded " << received << "/" << fileSize << " bytes\r";
    }

    out.close();
    resumeIdMap.erase(file_id);
    resumeOffsetMap.erase(file_id);
    deleteResumeFile(file_id);

    std::cout << "\nDownload complete\n";
}

static void doPut(SOCKET s, const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        std::cout << "File not found: " << filename << "\n";
        return;
    }

    in.seekg(0, std::ios::end);
    uint64_t fileSize = in.tellg();
    in.seekg(0, std::ios::beg);

    std::string payload = filename + "|" + std::to_string(fileSize);
    sendMessage(s, PUT_REQ, payload);

    MsgHeader hdr{};
    std::string resp;
    recvMessage(s, hdr, resp);

    if (hdr.type != PUT_RESP) {
        std::cout << "Server rejected upload: " << resp << "\n";
        return;
    }

    const size_t BUF_SIZE = 64 * 1024;
    char buffer[BUF_SIZE];
    uint64_t sent = 0;

    while (in) {
        in.read(buffer, BUF_SIZE);
        std::streamsize n = in.gcount();
        if (n > 0) {
            sendAll(s, buffer, (int)n);
            sent += n;
            std::cout << "Uploaded " << sent << "/" << fileSize << " bytes\r";
        }
    }

    std::cout << "\nUpload complete\n";
}


int main(int argc, char** argv) {
    try {
        WinsockInit _w;

        const char* host = (argc >= 2) ? argv[1] : "127.0.0.1";
        const char* port = (argc >= 3) ? argv[2] : "8021";

        SOCKET s = connectTo(host, port);

        std::cout << "Connected to FTP-Lite\n";
        std::cout << "Commands:\n"
            "  ping\n"
            "  list [path]\n"
            "  get <filename>\n"
			"  put <filename>\n"
            "  quit\n\n";

        for (;;) {
            std::cout << "ftp> ";
            std::string cmd;
            std::getline(std::cin, cmd);

            if (cmd == "ping") {
                doPing(s);
            }
            else if (cmd.rfind("list", 0) == 0) {
                std::string arg = "";
                if (cmd.size() > 5)
                    arg = cmd.substr(5);
                doList(s, arg);
            }
            else if (cmd.rfind("get ", 0) == 0) {
                std::string filename = cmd.substr(4);
                doGet(s, filename);
            }
            else if (cmd.rfind("put ", 0) == 0) {
                std::string filename = cmd.substr(4);
                doPut(s, filename);
            }
            else if (cmd == "quit" || cmd == "exit") {
                break;
            }
            else if (!cmd.empty()) {
                std::cout << "Unknown command\n";
            }
        }

        closesocket(s);
        std::cout << "Disconnected.\n";
        return 0;

    }
    catch (const std::exception& ex) {
        std::cerr << "Client error: " << ex.what() << "\n";
        return 1;
    }
}
