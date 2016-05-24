cmake_minimum_required(VERSION 3.5)
project(Linux_C_Web_Server)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS}   -O3  ")
set(CMAKE_Cxx_FLAGS_DUBUG "$ENV{CXXFLAGS}  -O0  -g -ggdb")

set(SOURCE_FILES
        main.cpp
        daemon_init.cpp
        log.cpp
        protocal.cpp
        protocal.h
        parse.h
        parse_config.cpp
        parse_option.cpp
        secure_access.cpp
        wrap.cpp
        wrap.h
        processpool/processhttp.cpp
        processpool/processhttp.h
        processpool/processpool.h)

add_executable(Linux_C_Web_Server ${SOURCE_FILES})
target_link_libraries(Linux_C_Web_Server ssl crypto)
add_definitions(-DHTTPS)
