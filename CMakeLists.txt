cmake_minimum_required(VERSION 3.30)
project(miner)

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/app/app.cpp
        src/algorithms/crypto.cpp
        src/datatypes/compact_size_uint.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused -Wno-write-strings -Wno-stringop-overread)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} wsock32 ws2_32)
endif()