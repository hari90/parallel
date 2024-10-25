cmake_minimum_required(VERSION 3.10)

# Set the project name
project(ParallelCommands)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the executable
add_executable(parallel parallel.cpp)

# Link pthread library
target_link_libraries(parallel pthread)