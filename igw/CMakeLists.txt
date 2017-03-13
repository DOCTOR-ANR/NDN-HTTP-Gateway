cmake_minimum_required(VERSION 3.5)
project(igw)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -pipe")

set(SOURCE_FILES main.cpp
        global.h
        module.h
        raw_stream.cpp raw_stream.h
        message.h
        http_request.cpp http_request.h
        http_response.cpp http_response.h
        http_server.cpp http_server.h
        sha1.cpp sha1.h
        http_sink.h
        ndn_message.cpp ndn_message.h
        http_ndn_interpreter.cpp http_ndn_interpreter.h
        ndn_sink.h
        ndn_client.cpp ndn_client.h
        ndn_server.cpp ndn_server.h)

find_package(Boost COMPONENTS system thread REQUIRED)

find_library(ndn-cxx REQUIRED)
find_library(pthread REQUIRED)

add_executable(igw ${SOURCE_FILES})

target_link_libraries(igw ${Boost_LIBRARIES} ndn-cxx pthread)