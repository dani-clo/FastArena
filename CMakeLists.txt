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
    target_link_libraries(fastarena_example PRIVATE FastArena::fastarena)
    target_compile_features(fastarena_example PRIVATE cxx_std_23)

    set(FASTARENA_EXAMPLE_SOURCES
        examples/01_basic_allocate_reset.cpp
        examples/02_marker_rollback_frame.cpp
        examples/03_stl_allocator_vector.cpp
        examples/04_pmr_vector_string.cpp
        examples/05_stats_and_growth.cpp
        examples/06_linux_mmap_backing.cpp
        examples/07_tls_arena_multithread.cpp
        examples/08_marker_invalid_cases.cpp
        examples/09_pmr_upstream_fallback.cpp
        examples/10_alignment_contract.cpp
    )

    set(FASTARENA_EXAMPLE_TARGETS)
    foreach(example_source IN LISTS FASTARENA_EXAMPLE_SOURCES)
        get_filename_component(example_name "${example_source}" NAME_WE)
        set(example_target "fastarena_example_${example_name}")

        add_executable(${example_target} ${example_source})
        target_link_libraries(${example_target} PRIVATE FastArena::fastarena)
        target_compile_features(${example_target} PRIVATE cxx_std_23)

        list(APPEND FASTARENA_EXAMPLE_TARGETS ${example_target})
    endforeach()

    add_custom_target(fastarena_examples
        DEPENDS fastarena_example ${FASTARENA_EXAMPLE_TARGETS}
    )
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
