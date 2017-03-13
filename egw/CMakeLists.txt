cmake_minimum_required(VERSION 3.5)
project(egw)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -pipe")

set(SOURCE_FILES main.cpp
        global.h
        module.h
        raw_stream.cpp raw_stream.h
        message.h
        ndn_message.cpp ndn_message.h
        ndn_server.cpp ndn_server.h
        ndn_sink.h
        ndn_client.cpp ndn_client.h
        http_request.cpp http_request.h
        http_response.cpp http_response.h
        ndn_http_interpreter.cpp ndn_http_interpreter.h
        http_sink.h
        http_client.cpp http_client.h)

find_package(Boost COMPONENTS system thread REQUIRED)

find_library(ndn-cxx REQUIRED)
find_library(pthread REQUIRED)

add_executable(egw ${SOURCE_FILES})

target_link_libraries(egw ${Boost_LIBRARIES} ndn-cxx pthread)