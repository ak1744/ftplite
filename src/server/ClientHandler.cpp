#include "ClientHandler.hpp"
#include "../../common/common.hpp"
#include "MetadataStore.hpp"
#include "FileManager.hpp"
#include <filesystem>
#include <sstream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

ClientHandler::ClientHandler(SOCKET sock, const fs::path& root, MetadataStore& meta, FileManager& fm)
    : clientSock(sock), rootDir(root), meta_(meta), fm_(fm) {
}

static std::string padLeft(uint64_t v, int w) {
    std::ostringstream oss; oss << std::setw(w) << v; return oss.str();
}

std::string ClientHandler::makeListPayload() {
    auto rows = meta_.listFilesNewestFirst(1000);
    std::vector<std::string> lines;
    lines.emplace_back("ID   SIZE(bytes)  UPLOADED_AT        DL  NAME");
    for (auto& r : rows) {
        std::ostringstream row;
        row << std::setw(4) << r.file_id << " "
            << std::setw(12) << r.size << "  "
            << std::setw(16) << r.uploaded_at << "  "
            << std::setw(3) << r.download_count << "  "
            << r.name;
        lines.push_back(row.str());
    }
    return joinLines(lines);
}

void ClientHandler::process() {
    try {
        for (;;) {
            MsgHeader hdr{}; std::string payload;
            recvMessage(clientSock, hdr, payload);

            switch (hdr.type) {
            case PING:
                sendMessage(clientSock, PONG, "OK");
                break;

            case LIST_REQ: {
                // payload ignored; we list from DB
                auto table = makeListPayload();
                sendMessage(clientSock, LIST_RESP, table);
                break;
            }

            case GET_REQ: {
                int file_id = 0;
                std::string resume_id;
                std::string file_id_str = payload;

                auto sep = payload.find('|');
                if (sep != std::string::npos) {
                    file_id_str = payload.substr(0, sep);
                    resume_id = payload.substr(sep + 1);
                }

                try { file_id = std::stoi(file_id_str); }
                catch (...) { file_id = 0; }

                FileRow fr{};
                if (!meta_.getFile(file_id, fr)) {
                    sendMessage(clientSock, ERR, "file-not-found");
                    break;
                }

                auto p = fm_.filePath(file_id);
                if (!fs::exists(p)) {
                    sendMessage(clientSock, ERR, "file-missing");
                    break;
                }

                uint64_t fileSize = static_cast<uint64_t>(fs::file_size(p));
                uint64_t offset = 0;

                if (!resume_id.empty()) {
                    ResumeRow rr{};
                    if (meta_.getResume(resume_id, rr) && rr.file_id == file_id) {
                        offset = rr.offset;
                        if (offset >= fileSize) offset = 0;
                    }
                }

                sendMessage(clientSock, GET_RESP, std::to_string(fileSize));

                std::ifstream in(p, std::ios::binary);
                if (!in) {
                    sendMessage(clientSock, ERR, "open-failed");
                    break;
                }

                in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

                const size_t BUF = 64 * 1024;
                std::vector<uint8_t> chunk;
                uint64_t sent = offset;

                while (sent < fileSize) {
                    size_t toRead = static_cast<size_t>(std::min<uint64_t>(BUF, fileSize - sent));
                    chunk.resize(toRead);
                    in.read(reinterpret_cast<char*>(chunk.data()), std::streamsize(toRead));
                    std::streamsize n = in.gcount();
                    if (n <= 0) break;
                    sendAll(clientSock, reinterpret_cast<const char*>(chunk.data()), (int)n);
                    sent += (uint64_t)n;

                    if (!resume_id.empty()) {
                        meta_.upsertResume(resume_id, file_id, sent, (uint32_t)BUF);
                    }
                }

                if (sent >= fileSize && !resume_id.empty()) {
                    meta_.deleteResume(resume_id);
                }

                meta_.incrementDownloadCount(file_id);

                break;
            }

            case PUT_REQ: {
                auto sep = payload.find('|');
                if (sep == std::string::npos) { sendMessage(clientSock, ERR, "bad-request"); break; }
                std::string name = payload.substr(0, sep);
                uint64_t size = 0; try { size = std::stoull(payload.substr(sep + 1)); }
                catch (...) { size = 0; }

                int file_id = -1;
                try {
                    file_id = meta_.insertFile(name, size, std::nullopt);
                }
                catch (...) {
                    sendMessage(clientSock, ERR, "insert-meta-failed"); break;
                }

                if (!fm_.allocateForNewFile(file_id, name)) { sendMessage(clientSock, ERR, "alloc-failed"); break; }

                sendMessage(clientSock, PUT_RESP, "OK");

                const size_t BUF = 64 * 1024;
                std::vector<uint8_t> buf(BUF);
                uint64_t received = 0;
                auto path = fm_.filePath(file_id);
                std::fstream out(path, std::ios::binary | std::ios::in | std::ios::out);
                if (!out) { sendMessage(clientSock, ERR, "open-failed"); break; }

                while (received < size) {
                    int toRead = static_cast<int>(std::min<uint64_t>(BUF, size - received));
                    recvAll(clientSock, reinterpret_cast<char*>(buf.data()), toRead);
                    out.seekp(static_cast<std::streamoff>(received), std::ios::beg);
                    out.write(reinterpret_cast<const char*>(buf.data()), toRead);
                    received += static_cast<uint64_t>(toRead);
                }
                out.flush();
                meta_.updateFileSize(file_id, size);
                break;
            }

            default:
                sendMessage(clientSock, ERR, "unknown-msg");
            }
        }
    }
    catch (...) {
        // client closed / error
    }
    closesocket(clientSock);
}
