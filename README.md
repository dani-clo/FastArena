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



