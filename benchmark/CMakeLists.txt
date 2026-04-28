find_package(benchmark QUIET)

if (benchmark_FOUND)
    # Google Benchmark is available
    set(FASTARENA_BENCH_SOURCES
        bench_small_fixed.cpp
        bench_mixed_sizes.cpp
        bench_frame_pattern.cpp
    )

    foreach(bench_source IN LISTS FASTARENA_BENCH_SOURCES)
        get_filename_component(bench_name "${bench_source}" NAME_WE)
        set(bench_target "fastarena_${bench_name}")

        add_executable(${bench_target} ${bench_source})
        target_link_libraries(${bench_target}
            PRIVATE
            FastArena::fastarena
            benchmark::benchmark
        )
        target_compile_features(${bench_target} PRIVATE cxx_std_23)
    endforeach()

    add_custom_target(fastarena_benchmarks
        DEPENDS
        fastarena_bench_small_fixed
        fastarena_bench_mixed_sizes
        fastarena_bench_frame_pattern
    )
else()
    # Fallback: no Google Benchmark, use simple timer-based benchmarks
    message(STATUS "Google Benchmark not found, building benchmarks with manual timers")

    set(FASTARENA_BENCH_SOURCES_SIMPLE
        bench_small_fixed_simple.cpp
        bench_mixed_sizes_simple.cpp
        bench_frame_pattern_simple.cpp
    )

    foreach(bench_source IN LISTS FASTARENA_BENCH_SOURCES_SIMPLE)
        get_filename_component(bench_name "${bench_source}" NAME_WE)
        set(bench_target "fastarena_${bench_name}")

        add_executable(${bench_target} ${bench_source})
        target_link_libraries(${bench_target} PRIVATE FastArena::fastarena)
        target_compile_features(${bench_target} PRIVATE cxx_std_23)
    endforeach()

    add_custom_target(fastarena_benchmarks
        DEPENDS
        fastarena_bench_small_fixed_simple
        fastarena_bench_mixed_sizes_simple
        fastarena_bench_frame_pattern_simple
    )
endif()
