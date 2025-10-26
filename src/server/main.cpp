#include <iostream>
#include <filesystem>
#include <string>
#include "../../common/common.hpp" 
#include "Server.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    try {
        WinsockInit _w;
        const char* port = (argc >= 2) ? argv[1] : "8021";
        std::filesystem::path root = (argc >= 3) ? std::filesystem::path(argv[2]) : std::filesystem::current_path();
        std::filesystem::path db = root / "ftplite.sqlite";

        std::cout << "FTP-Lite Server\nPort: " << port << "\nRoot: " << root.string() << "\nDB: " << db.string() << "\n\n";
        Server server(port, root, db);
        server.start();

    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
