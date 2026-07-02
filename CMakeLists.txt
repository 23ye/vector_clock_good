cmake_minimum_required(VERSION 3.10)
project(logagg C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include_directories(include)

add_library(logagg STATIC
    src/vector_clock.c
    src/log_entry.c
)

add_executable(test_vector_clock
    test/test_vector_clock.c
)

target_link_libraries(test_vector_clock PRIVATE logagg)

enable_testing()
add_test(NAME vector_clock_tests COMMAND test_vector_clock)
