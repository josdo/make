cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(MiniMake CXX)

find_package(GTest REQUIRED)
find_package(gflags REQUIRED)

add_compile_options(-O3 -g -Wall -Werror)

set(SRC_DIR "src")
set(INCLUDE_DIR "include")
set(TEST_DIR "tests")

include_directories(${INCLUDE_DIR})

set(SRCS
    src/makefile-parser.cpp
    src/task-graph.cpp
	src/makefile-builder.cpp
)

# Executable
add_executable(${CMAKE_PROJECT_NAME} ${SRCS} "src/main.cpp")
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE PRIVATE=private:)

# Tests
file(GLOB TEST_FILES "${TEST_DIR}/*.cpp")
foreach(test_file ${TEST_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(test_executable "${test_name}")
    add_executable(${test_executable} ${test_file} ${SRCS})
    target_compile_definitions(${test_executable} PRIVATE PRIVATE=public:)
    target_link_options(${test_executable} PRIVATE -no-pie)
    target_link_libraries(${test_executable} GTest::GTest GTest::Main gflags)
endforeach()
