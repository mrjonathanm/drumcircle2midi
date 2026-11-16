cmake_minimum_required(VERSION 3.13)
project(dc2m_core C)
set(CMAKE_C_STANDARD 99)

add_library(dc2m src/dc2m.c)
target_include_directories(dc2m PUBLIC include)
target_compile_options(dc2m PRIVATE -Wall -Wextra)
target_link_libraries(dc2m PUBLIC m)

add_executable(test_core test/test_core.c)
target_link_libraries(test_core dc2m)
enable_testing()
add_test(NAME core COMMAND test_core)
