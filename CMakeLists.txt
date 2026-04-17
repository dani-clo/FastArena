cmake_minimum_required(VERSION 3.21)

project(FastArena VERSION 0.1.0 LANGUAGES CXX)

add_library(fastarena STATIC
    arena.cpp
)

add_library(FastArena::fastarena ALIAS fastarena)

target_include_directories(fastarena
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(fastarena PUBLIC cxx_std_23)

if (MSVC)
    target_compile_options(fastarena PRIVATE /W4 /permissive-)
else()
    target_compile_options(fastarena PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(FASTARENA_BUILD_EXAMPLES "Build FastArena examples" ON)

if (FASTARENA_BUILD_EXAMPLES)
    add_executable(fastarena_example
        examples/smoke.cpp
    )
    target_link_libraries(fastarena_example PRIVATE FastArena::fastarena)
    target_compile_features(fastarena_example PRIVATE cxx_std_23)
endif()
