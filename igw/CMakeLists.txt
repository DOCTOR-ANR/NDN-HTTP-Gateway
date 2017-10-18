cmake_minimum_required(VERSION 3.5)
project(igw)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -pipe")

set(SOURCE_FILES main.cpp
        seekable_raw_stream.cpp
        http_request.cpp
        http_response.cpp
        http_server.cpp
        ndn_content.cpp
        sha1.cpp
        http_ndn_interpreter.cpp
        ndn_resolver.cpp
        ndn_receiver.cpp
        ndn_sender.cpp)

find_package(Boost COMPONENTS system thread REQUIRED)

find_library(ndn-cxx REQUIRED)
find_library(pthread REQUIRED)

add_executable(igw ${SOURCE_FILES})

target_link_libraries(igw ${Boost_LIBRARIES} ndn-cxx pthread)