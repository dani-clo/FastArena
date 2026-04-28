#include "arena.hpp"

#include <benchmark/benchmark.h>
#include <array>
#include <cstddef>
#include <new>
#include <vector>

// Benchmark: FastArena frame pattern (mark/rollback)
static void FastArena_FramePattern(benchmark::State& state) {
    const int frame_count = state.range(0);
    const int allocs_per_frame = state.range(1);

    fa::arena frame_arena({
        .initial_capacity = 8192,
        .min_chunk_size = 8192,
        .max_chunk_size = 1 << 20,
        .growth_factor = 2,
    });

    for (auto _ : state) {
        for (int frame = 0; frame < frame_count; ++frame) {
            auto m = frame_arena.mark();

            for (int i = 0; i < allocs_per_frame; ++i) {
                auto* temp = frame_arena.allocate_object<std::array<float, 16>>(1);
                benchmark::DoNotOptimize(temp);
            }

            benchmark::ClobberMemory();
            frame_arena.rollback(m);
        }
    }
}

// Benchmark: std::allocator frame pattern (allocate + delete per frame)
static void StdAllocator_FramePattern(benchmark::State& state) {
    const int frame_count = state.range(0);
    const int allocs_per_frame = state.range(1);

    for (auto _ : state) {
        for (int frame = 0; frame < frame_count; ++frame) {
            std::vector<void*> frame_ptrs;
            frame_ptrs.reserve(allocs_per_frame);

            for (int i = 0; i < allocs_per_frame; ++i) {
                frame_ptrs.push_back(::operator new(sizeof(std::array<float, 16>)));
                benchmark::DoNotOptimize(frame_ptrs.back());
            }

            benchmark::ClobberMemory();

            for (void* ptr : frame_ptrs) {
                ::operator delete(ptr);
            }
        }
    }
}

BENCHMARK(FastArena_FramePattern)->Args({100, 50})->Args({100, 100})->Args({100, 200})->Iterations(50);
BENCHMARK(StdAllocator_FramePattern)->Args({100, 50})->Args({100, 100})->Args({100, 200})->Iterations(50);

BENCHMARK_MAIN();
