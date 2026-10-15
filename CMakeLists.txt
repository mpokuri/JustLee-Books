cmake_minimum_required(VERSION 4.3)
project(J_BookStore)

set(CMAKE_CXX_STANDARD 20)

find_package(SQLite3 REQUIRED)

add_executable(J_BookStore main.cpp)

target_link_libraries(J_BookStore PRIVATE SQLite::SQLite3)

