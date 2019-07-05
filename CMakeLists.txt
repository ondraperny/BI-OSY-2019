cmake_minimum_required(VERSION 3.10)
project(1_assignment)

set(CMAKE_CXX_STANDARD 14)

link_directories(C:/Users/ondra/Desktop/OSY/1_assignment/i686-w64-mingw32)

add_executable(1_assignment common.h solution.cpp progtest_solver.h sample_tester.cpp sample_tester.h )

target_link_libraries(1_assignment pthread progtest_solver)
