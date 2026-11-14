cmake_minimum_required(VERSION 3.16)
project(SDL3Example)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the official SDL3 package installed via pacman
find_package(SDL3 REQUIRED)

add_executable(SDL3Example main.cpp rectangle.cpp circle.cpp)

# Link against the SDL3 target
target_link_libraries(SDL3Example PRIVATE SDL3::SDL3)