add_executable(ftplite_client
    main.cpp
)

target_include_directories(ftplite_client PUBLIC
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries(ftplite_client
    ftplite_common
)
