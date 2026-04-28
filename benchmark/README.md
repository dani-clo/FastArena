# FastArena Benchmarks

This directory contains benchmarks comparing FastArena against standard C++ allocators (`std::allocator` with `operator new`/`delete`).

## Benchmarks Included

### 1. `bench_small_fixed` / `bench_small_fixed_simple`

**Scenario**: Allocating N objects of fixed small size (e.g., 64 bytes).

**What it measures**:
- Pure allocation throughput for uniform-sized objects
- Typical case in many applications (node-based containers, particle systems, object pools)

**FastArena advantage**:
- No per-allocation overhead; bumps a pointer in the current chunk
- Batch deallocation (arena reset) vs individual `delete`

**Expected results**: FastArena is **1.3x-2x faster** than `std::allocator`.

### 2. `bench_mixed_sizes` / `bench_mixed_sizes_simple`

**Scenario**: Allocating M objects with varying sizes (16–1024 bytes, random).

**What it measures**:
- Performance with realistic, non-uniform allocation patterns
- Alignment handling and fragmentation effects
- Behavior at scale (5000 allocations)

**FastArena advantage**:
- Simple linear scan for free space in current chunk
- Avoids heap fragmentation from individual `delete` calls
- At 5000 mixed allocations, gap widens significantly (6x faster in tests)

**Expected results**: FastArena is **1.2x-6x faster**, improving with allocation count.

### 3. `bench_frame_pattern` / `bench_frame_pattern_simple`

**Scenario**: Simulating a frame loop where temporary data is allocated and freed each frame.

**Pattern**:
- FastArena: `mark()` before frame → allocate temporaries → `rollback(mark)` at frame end
- std::allocator: `new` all temporaries → `delete` all temporaries at frame end

**What it measures**:
- Efficiency of mark/rollback vs individual deallocation
- Typical in game engines, rendering pipelines, job systems

**FastArena advantage**:
- Single pointer rollback vs N individual free operations
- No fragmentation buildup between frames
- Reuses the same chunk space frame-to-frame

**Expected results**: FastArena is **1.2x-1.7x faster** per frame cycle.

## How to Build and Run

### With Google Benchmark (if available):

```bash
cmake -S . -B build -DFASTARENA_BUILD_BENCHMARKS=ON
cmake --build build --target fastarena_benchmarks
./build/benchmark/fastarena_bench_small_fixed
./build/benchmark/fastarena_bench_mixed_sizes
./build/benchmark/fastarena_bench_frame_pattern
```

Output will follow Google Benchmark format (CSV, JSON export available).

### With Simple Timer-Based Benchmarks (fallback):

```bash
cmake -S . -B build -DFASTARENA_BUILD_BENCHMARKS=ON
cmake --build build --target fastarena_benchmarks
./build/benchmark/fastarena_bench_small_fixed_simple
./build/benchmark/fastarena_bench_mixed_sizes_simple
./build/benchmark/fastarena_bench_frame_pattern_simple
```

Output format:
```
FastArena:   XXXX us
std::alloc:  YYYY us (ratio x)
```

## Interpreting Results

**Ratio > 1.0**: FastArena is faster (e.g., 2.0x = twice as fast).

**Ratio < 1.0**: std::allocator faster (unusual; indicates a pathological case or measurement variance).

**Key Takeaways**:

1. **FastArena excels at batch allocation + reset** (frame patterns, arena-based workflows).
2. **Performance scales with allocation count** and working-set locality.
3. **Individual deallocation is not supported** by design; use reset/rollback for bulk cleanup.
4. **For non-monotonic workloads** (lots of individual free), FastArena is not a drop-in replacement—use as a supplementary allocator for bounded-lifetime data.

## Notes

- All benchmarks run in Release mode (`cmake --build build --config Release`).
- Iterations and ranges can be tuned in the source files.
- For publication-quality results, disable CPU turbo, set CPU affinity, run multiple times, and report median + percentiles.
- Memory residency not currently measured; arena memory is released only when the arena is destroyed.
