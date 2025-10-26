find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_executable(ftplite_server
    main.cpp
    Server.cpp
    ClientHandler.cpp
    MetadataStore.cpp
    FileManager.cpp
)

target_include_directories(ftplite_server PUBLIC
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}  # for the new headers
)

target_link_libraries(ftplite_server
    ftplite_common
    unofficial::sqlite3::sqlite3
)