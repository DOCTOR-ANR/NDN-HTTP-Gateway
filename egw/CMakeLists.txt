cmake_minimum_required(VERSION 3.5)
project(egw)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -pipe")

set(SOURCE_FILES main.cpp
        ndn_receiver.cpp
        ndn_sender.cpp
        ndn_resolver.cpp
        ndn_http_interpreter.cpp
        http_client.cpp
        ndn_content.cpp
        http_request.cpp
        http_response.cpp
        raw_stream.cpp)

find_package(Boost COMPONENTS system thread REQUIRED)

find_library(ndn-cxx REQUIRED)
find_library(pthread REQUIRED)

add_executable(egw ${SOURCE_FILES})

target_link_libraries(egw ${Boost_LIBRARIES} ndn-cxx pthread)