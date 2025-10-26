#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

struct MsgHeader {
    uint32_t magic;     // 'FTPL'
    uint16_t version;   // 1
    uint16_t type;      // see enum
    uint32_t length;    // payload bytes
    uint32_t reserved;  // 0
};

enum MsgType : uint16_t {
    PING = 1, PONG = 2,
    LIST_REQ = 10, LIST_RESP = 11,
    GET_REQ = 20, GET_RESP = 21,
    PUT_REQ = 30, PUT_RESP = 31,
    ERR = 1000
};



constexpr uint32_t MAGIC = 0x4654504C; // 'FTPL'

class SocketError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};

void sendAll(SOCKET s, const char* buf, int len);
void recvAll(SOCKET s, char* buf, int len);

void sendMessage(SOCKET s, uint16_t type, const std::string& payload);
void recvMessage(SOCKET s, MsgHeader& hdr, std::string& payload);

// Small RAII for Winsock
struct WinsockInit {
    WinsockInit();
    ~WinsockInit();
};

std::string joinLines(const std::vector<std::string>& lines);
