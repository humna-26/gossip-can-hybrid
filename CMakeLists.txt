cmake_minimum_required(VERSION 3.14)
project(gossip_can_hybrid VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect all .cpp source files from src/ and its subdirectories
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/**/*.cpp"
)

add_executable(gossip_can_hybrid ${SOURCES})

# Include the src/ directory so headers can be included as e.g. "simulator/Event.h"
target_include_directories(gossip_can_hybrid PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Enable warnings on both MSVC and GCC/MinGW
if(MSVC)
    target_compile_options(gossip_can_hybrid PRIVATE /W4)
else()
    target_compile_options(gossip_can_hybrid PRIVATE -Wall -Wextra -Wpedantic)
endif()
