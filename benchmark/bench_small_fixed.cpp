#include "arena.hpp"

#include <benchmark/benchmark.h>
#include <cstdint>
#include <new>
#include <vector>

struct uint64_wrapper {
    std::uint64_t value;
};

// Benchmark: FastArena small fixed allocations
static void FastArena_SmallFixed(benchmark::State& state) {
    const int allocation_count = state.range(0);

    for (auto _ : state) {
        fa::arena a({
            .initial_capacity = 4096,
            .min_chunk_size = 4096,
            .max_chunk_size = 1 << 20,
            .growth_factor = 2,
        });

        for (int i = 0; i < allocation_count; ++i) {
            benchmark::DoNotOptimize(a.allocate_object<uint64_wrapper>(1));
        }

        benchmark::ClobberMemory();
    }
}

// Benchmark: std::allocator with batch delete
static void StdAllocator_SmallFixed(benchmark::State& state) {
    const int allocation_count = state.range(0);

    for (auto _ : state) {
        std::vector<uint64_wrapper*> ptrs;
        ptrs.reserve(allocation_count);

        for (int i = 0; i < allocation_count; ++i) {
            ptrs.push_back(new uint64_wrapper{});
        }

        benchmark::ClobberMemory();

        for (auto* ptr : ptrs) {
            delete ptr;
        }
    }
}

BENCHMARK(FastArena_SmallFixed)->Range(100, 10000)->Iterations(100);
BENCHMARK(StdAllocator_SmallFixed)->Range(100, 10000)->Iterations(100);

BENCHMARK_MAIN();
