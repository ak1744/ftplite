# FTP-Lite

A lightweight file transfer protocol implementation in C++ for Windows.

## Overview

FTP-Lite is a simple client-server file transfer system designed for reliability and ease of use. It supports basic file operations including upload, download, and listing, with built-in support for resumable downloads.

## Features

- **File Upload/Put**: Upload files to the server
- **File Download/Get**: Download files from the server with resume support
- **File Listing**: List all files stored on the server
- **Resumable Downloads**: Automatically resume interrupted downloads
- **SQLite Backend**: Persistent metadata storage using SQLite
- **Threaded Server**: Supports multiple concurrent client connections

## Requirements

- Windows 10 or later
- Visual Studio 2019 or later with "Desktop Development with C++" workload
- CMake 3.15 or later
- vcpkg for dependency management
- SQLite3 (installed via vcpkg)

## Building

### Install Dependencies

First, install SQLite3 using vcpkg:

```bash
vcpkg install sqlite3:x64-windows
```

### Build Commands

**Debug Build:**
```bash
cmake -B out/build/x64-Debug -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake -G Ninja
cmake --build out/build/x64-Debug --config Debug
```

**Release Build:**
```bash
cmake -B out/build/x64-Release -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/x64-Release --config Release
```

### Build with Visual Studio

Alternatively, open the project in Visual Studio and build from the IDE.

## Usage

### Starting the Server

Run the server with:
```bash
out/build/x64-Debug/src/server/ftplite_server.exe [port] [storage_directory]
```

- `port`: Server port (default: 8021)
- `storage_directory`: Directory to store uploaded files (default: current directory)

Example:
```bash
ftplite_server.exe 8021 C:\ftplite-storage
```

### Running the Client

Run the client with:
```bash
out/build/x64-Debug/src/client/ftplite_client.exe [host] [port]
```

- `host`: Server hostname or IP (default: 127.0.0.1)
- `port`: Server port (default: 8021)

Example:
```bash
ftplite_client.exe 127.0.0.1 8021
```

### Client Commands

Once connected, the client supports these commands:

- `ping` - Test server connectivity
- `list [path]` - List files on server (path is currently ignored)
- `get <file_id>` - Download a file by its ID
- `put <filename>` - Upload a file to the server
- `quit` or `exit` - Disconnect from server

## Protocol

FTP-Lite uses a binary protocol with fixed-size message headers:

```
struct MsgHeader {
    uint32_t magic;     // 'FTPL' (0x4654504C)
    uint16_t version;   // Protocol version (1)
    uint16_t type;      // Message type
    uint32_t length;    // Payload size in bytes
    uint32_t reserved;  // Reserved for future use
}
```

### Message Types

- `PING (1)` / `PONG (2)` - Keepalive
- `LIST_REQ (10)` / `LIST_RESP (11)` - File listing
- `GET_REQ (20)` / `GET_RESP (21)` - File download
- `PUT_REQ (30)` / `PUT_RESP (31)` - File upload
- `ERR (1000)` - Error response

## Project Structure

```
ftplite/
├── common/              # Shared protocol code
│   ├── common.hpp
│   └── common.cpp
├── src/
│   ├── server/           # Server implementation
│   │   ├── Server.cpp/hpp
│   │   ├── ClientHandler.cpp/hpp
│   │   ├── MetadataStore.cpp/hpp
│   │   ├── FileManager.cpp/hpp
│   │   └── main.cpp
│   └── client/           # Client implementation
│       └── main.cpp
├── CMakeLists.txt
└── README.md
```

## Database Schema

The server uses SQLite to store file metadata:

**files table:**
- `file_id` - Primary key
- `name` - File name
- `size` - File size in bytes
- `checksum` - Optional file checksum
- `uploaded_at` - Upload timestamp
- `download_count` - Number of downloads

**resume table:**
- `resume_id` - Unique resume identifier
- `file_id` - Foreign key to files table
- `offset` - Current download offset
- `chunk_size` - Chunk size used
- `timestamp` - Last update time

## Development

### Code Style

- C++17 standard
- Windows-specific (Winsock2 for networking)
- STL containers and algorithms
- RAII for resource management

### Adding Features

Key extension points:

1. **New message types**: Add to `MsgType` enum in `common.hpp`
2. **Client commands**: Extend client command parser in `src/client/main.cpp`
3. **Server handlers**: Add case statements in `ClientHandler::process()`

## License

[Specify your license here]

## Contributing

[Add contributing guidelines here]
