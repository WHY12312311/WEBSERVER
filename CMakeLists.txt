cmake_minimum_required(VERSION 3.16.3)

project(WEBSERVER)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-pthread -g")
set(CMAKE_CXX_STANDARD 17)

aux_source_directory(./src DIR_SRC)
include_directories(./include)

add_executable(WEBSERVER ${DIR_SRC})