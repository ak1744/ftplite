add_executable(ftplite_server
    main.cpp
    Server.cpp
    ClientHandler.cpp
)

target_include_directories(ftplite_server PUBLIC
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries(ftplite_server
    ftplite_common
)
