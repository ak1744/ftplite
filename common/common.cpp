#include "common.hpp"
#include <cstring>

WinsockInit::WinsockInit() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        throw SocketError("WSAStartup failed");
    }
}
WinsockInit::~WinsockInit() { WSACleanup(); }

void sendAll(SOCKET s, const char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(s, buf + sent, len - sent, 0);
        if (n <= 0) throw SocketError("send failed");
        sent += n;
    }
}

void recvAll(SOCKET s, char* buf, int len) {
    int got = 0;
    while (got < len) {
        int n = recv(s, buf + got, len - got, 0);
        if (n <= 0) throw SocketError("recv failed");
        got += n;
    }
}

void sendMessage(SOCKET s, uint16_t type, const std::string& payload) {
    MsgHeader h{};
    h.magic = MAGIC;
    h.version = 1;
    h.type = type;
    h.length = static_cast<uint32_t>(payload.size());
    h.reserved = 0;

    sendAll(s, reinterpret_cast<const char*>(&h), sizeof(h));
    if (!payload.empty()) {
        sendAll(s, payload.data(), static_cast<int>(payload.size()));
    }
}

void recvMessage(SOCKET s, MsgHeader& hdr, std::string& payload) {
    recvAll(s, reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (hdr.magic != MAGIC || hdr.version != 1) {
        throw SocketError("bad header");
    }
    payload.clear();
    if (hdr.length) {
        payload.resize(hdr.length);
        recvAll(s, payload.data(), static_cast<int>(hdr.length));
    }
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size()) out += "\n";
    }
    return out;
}
