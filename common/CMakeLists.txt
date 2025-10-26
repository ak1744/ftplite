add_library(ftplite_common STATIC
    common.cpp
    common.hpp
)

target_include_directories(ftplite_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ftplite_common ws2_32)
