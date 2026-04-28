#include "arena.hpp"

#include <benchmark/benchmark.h>
#include <array>
#include <cstddef>
#include <new>
#include <random>
#include <vector>

// Benchmark: FastArena mixed-size allocations
static void FastArena_MixedSizes(benchmark::State& state) {
    const int allocation_count = state.range(0);
    std::mt19937 rng{42};
    std::uniform_int_distribution<std::size_t> size_dist(16, 1024);

    std::vector<std::size_t> sizes;
    sizes.reserve(allocation_count);
    for (int i = 0; i < allocation_count; ++i) {
        sizes.push_back(size_dist(rng));
    }

    for (auto _ : state) {
        fa::arena a({
            .initial_capacity = 16384,
            .min_chunk_size = 16384,
            .max_chunk_size = 1 << 20,
            .growth_factor = 2,
        });

        for (std::size_t sz : sizes) {
            benchmark::DoNotOptimize(a.allocate(sz, alignof(std::max_align_t)));
        }

        benchmark::ClobberMemory();
    }
}

// Benchmark: std::allocator with mixed sizes
static void StdAllocator_MixedSizes(benchmark::State& state) {
    const int allocation_count = state.range(0);
    std::mt19937 rng{42};
    std::uniform_int_distribution<std::size_t> size_dist(16, 1024);

    std::vector<std::size_t> sizes;
    sizes.reserve(allocation_count);
    for (int i = 0; i < allocation_count; ++i) {
        sizes.push_back(size_dist(rng));
    }

    for (auto _ : state) {
        std::vector<void*> ptrs;
        ptrs.reserve(allocation_count);

        for (std::size_t sz : sizes) {
            ptrs.push_back(::operator new(sz));
        }

        benchmark::ClobberMemory();

        for (void* ptr : ptrs) {
            ::operator delete(ptr);
        }
    }
}

BENCHMARK(FastArena_MixedSizes)->Range(100, 5000)->Iterations(100);
BENCHMARK(StdAllocator_MixedSizes)->Range(100, 5000)->Iterations(100);

BENCHMARK_MAIN();
