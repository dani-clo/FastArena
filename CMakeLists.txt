cmake_minimum_required(VERSION 3.21)

project(FastArena VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(FASTARENA_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/FastArena")

add_library(fastarena STATIC
    src/arena.cpp
)

add_library(FastArena::fastarena ALIAS fastarena)

target_include_directories(fastarena
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(fastarena PUBLIC cxx_std_23)

if (MSVC)
    target_compile_options(fastarena PRIVATE /W4 /permissive-)
else()
    target_compile_options(fastarena PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

option(FASTARENA_BUILD_EXAMPLES "Build FastArena examples" ON)
option(FASTARENA_BUILD_TESTS "Build FastArena tests" ON)

if (FASTARENA_BUILD_EXAMPLES)
    add_executable(fastarena_example
        examples/smoke.cpp
    )
    target_link_libraries(fastarena_example PRIVATE FastArena::fastarena)
    target_compile_features(fastarena_example PRIVATE cxx_std_23)
endif()

if (FASTARENA_BUILD_TESTS)
    enable_testing()
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()

install(TARGETS fastarena
    EXPORT FastArenaTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT FastArenaTargets
    NAMESPACE FastArena::
    FILE FastArenaTargets.cmake
    DESTINATION ${FASTARENA_CMAKE_INSTALL_DIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/FastArenaConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FastArenaConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/FastArenaConfig.cmake"
    INSTALL_DESTINATION ${FASTARENA_CMAKE_INSTALL_DIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/FastArenaConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FastArenaConfigVersion.cmake"
    DESTINATION ${FASTARENA_CMAKE_INSTALL_DIR}
)
