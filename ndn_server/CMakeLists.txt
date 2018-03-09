cmake_minimum_required(VERSION 3.5)
project(ndn_server)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -pipe")

file(GLOB SOURCE_FILES *.cpp *.h)

find_package(Boost COMPONENTS system filesystem thread REQUIRED)

find_library(ndn-cxx REQUIRED)
find_library(pthread REQUIRED)

add_executable(ndn_server ${SOURCE_FILES})

target_link_libraries(ndn_server ${Boost_LIBRARIES} ndn-cxx pthread)