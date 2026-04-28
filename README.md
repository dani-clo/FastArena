# FastArena

FastArena is a high-performance monotonic arena allocator for modern C++23.

The project provides:

* chunk-based bump allocation
* reset and marker-based rollback
* optional thread-safe arena type
* `std::pmr::memory_resource` adapter
* allocator adapter for STL containers

## What Is Implemented

Public API is declared in `include/arena.hpp` under namespace `fa`.

Main types:

* `fa::arena_config`
* `fa::arena` (`basic_arena<single_thread_t>`)
* `fa::tls_arena` (`basic_arena<thread_local_t>`, mutex-protected)
* `fa::arena_resource` (`std::pmr::memory_resource` wrapper)
* `fa::arena_allocator<T>`

Backends:

* `fa::backing::system_malloc` (default)
* `fa::backing::mmap` (Linux only)

## Repository Structure

```text
FastArena/
├── benchmark/
├── cmake/
├── examples/
├── include/
├── src/
└── tests/
```

## Build

Configure:

```bash
cmake -S . -B build
```

Build library:

```bash
cmake --build build --target fastarena
```

Build everything enabled in CMake options (`FASTARENA_BUILD_EXAMPLES`, `FASTARENA_BUILD_TESTS`):

```bash
cmake --build build
```

Install into local prefix:

```bash
cmake --install build --prefix build/install
```

## Examples

FastArena includes 10 focused examples, one per feature/use case. Build them with:

```bash
cmake -S . -B build -DFASTARENA_BUILD_EXAMPLES=ON
cmake --build build --target fastarena_examples
```

**Examples overview**:

| Example | Feature |
|---------|---------|
| `smoke.cpp` | PMR integration overview |
| `01_basic_allocate_reset.cpp` | Basic typed allocation + reset |
| `02_marker_rollback_frame.cpp` | Frame-based pattern (temporary data) |
| `03_stl_allocator_vector.cpp` | STL vector with `arena_allocator<T>` |
| `04_pmr_vector_string.cpp` | STL PMR containers on `arena_resource` |
| `05_stats_and_growth.cpp` | Statistics and chunk growth analysis |
| `06_linux_mmap_backing.cpp` | Linux mmap backend |
| `07_tls_arena_multithread.cpp` | Multi-threaded `tls_arena` |
| `08_marker_invalid_cases.cpp` | Robust marker validation (edge cases) |
| `09_pmr_upstream_fallback.cpp` | PMR fallback allocation behavior |
| `10_alignment_contract.cpp` | Alignment and edge-case handling |

Run any example:

```bash
./build/fastarena_example
./build/fastarena_example_01_basic_allocate_reset
```

## Benchmarks

FastArena includes three comparative benchmarks against `std::allocator` (using `operator new`/`delete`):

```bash
cmake -S . -B build -DFASTARENA_BUILD_BENCHMARKS=ON
cmake --build build --target fastarena_benchmarks
```

**Benchmarks**:

1. **Small Fixed Allocations** (`bench_small_fixed`):
   - Allocating many small, uniform-sized objects
   - FastArena: **1.3x–2.0x faster** than std::allocator

2. **Mixed Sizes** (`bench_mixed_sizes`):
   - Realistic variable-size allocation pattern (16–1024 bytes)
   - FastArena: **1.2x–6.0x faster**, scaling with allocation count

3. **Frame Pattern** (`bench_frame_pattern`):
   - Simulates game loop or frame-based temporary allocation
   - Uses `mark()`/`rollback()` to reclaim frame-local data
   - FastArena: **1.2x–1.7x faster** per frame cycle

See [benchmark/README.md](benchmark/README.md) for detailed results and interpretation.

## Basic Usage

```cpp
#include "arena.hpp"
#include <cstdint>

int main() {
    fa::arena a({
        .initial_capacity = 1024,
        .min_chunk_size = 1024,
        .max_chunk_size = 1 << 20,
        .growth_factor = 2,
        .backing_alloc = fa::backing::system_malloc,
    });

    auto* x = a.new_object<std::uint64_t>(42ULL);
    auto* y = a.allocate_object<int>(8);
    (void)x;
    (void)y;

    a.reset();
}
```

Notes:

* `allocate(size, alignment)` requires `alignment` to be a power of two.
* `allocate(0, ...)` is normalized to 1 byte.
* Individual deallocation is not supported.

## Mark And Rollback

```cpp
auto m = a.mark();
auto* tmp = a.allocate_object<int>(32);
(void)tmp;

a.rollback(m);
```

Semantics:

* Markers are validated with an internal owner cookie and generation counter.
* Markers become invalid after `reset()` (generation changes).
* Rollback with an invalid marker is ignored (no-op).
* No destructors are invoked by rollback/reset.

## PMR Integration

```cpp
#include "arena.hpp"
#include <memory_resource>
#include <vector>

fa::arena_resource resource;
std::pmr::vector<int> values{&resource};
values.push_back(1);
values.push_back(2);
```

Behavior:

* `do_allocate` first tries arena allocation.
* If arena allocation throws `std::bad_alloc`, allocation falls back to upstream resource.
* `do_deallocate` only forwards deallocation for pointers that came from upstream fallback.

## `arena_allocator<T>` Integration

```cpp
#include "arena.hpp"
#include <vector>

fa::arena a;
std::vector<int, fa::arena_allocator<int>> v{fa::arena_allocator<int>(a)};
v.push_back(10);
```

## How It Works

FastArena stores chunks in a singly linked list. Each chunk tracks:

* `data pointer`
* `capacity`
* `used`
* `next`

Allocation algorithm:

1. Start from current chunk (or head).
2. Align `data + used`.
3. If there is space, bump `used` and return pointer.
4. Otherwise scan next chunks.
5. If no chunk fits, allocate a new chunk and retry.

Chunk growth:

* next capacity is multiplied by `growth_factor`
* result is clamped to `[min_chunk_size, max_chunk_size]`

## Stats

`arena_stats` exposes:

* `total_allocations`
* `bytes_requested`
* `bytes_allocated` (requested + alignment padding)
* `wasted_bytes` (alignment padding only)
* `peak_bytes_used`
* `chunk_count` (computed when requesting stats)

Utility methods:

* `bytes_requested()`
* `bytes_allocated()` (current used bytes across chunks)
* `bytes_reserved()` (sum of chunk capacities)
* `stats()`
* `reset_stats()`

## Complexity Notes

Current implementation characteristics:

* Allocation is typically constant-time when the current chunk has room.
* Allocation can degrade to linear in number of scanned chunks.
* `reset()` is linear in number of chunks (it zeroes `used` for each chunk).
* `rollback()` is linear in number of chunks after the marker chunk.

## Thread Safety

* `fa::arena` uses a no-op lock (single-thread use).
* `fa::tls_arena` uses `std::mutex` and serializes operations.

## Limitations

* Not a general-purpose allocator.
* No per-object `free`.
* Memory is released to OS/system only when arena is destroyed.
* Object destructors must be managed by caller.
* `arena_config::debug` exists but is currently unused by implementation.



