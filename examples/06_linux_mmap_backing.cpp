#include "arena.hpp"

#include <cstdint>
#include <iostream>

int main() {
#if defined(__linux__)
    fa::arena a({
        .initial_capacity = 4096,
        .min_chunk_size = 4096,
        .max_chunk_size = 64 * 1024,
        .growth_factor = 2,
        .backing_alloc = fa::backing::mmap,
    });

    auto* value = a.new_object<std::uint64_t>(123456789ULL);
    std::cout << "mmap backing value=" << *value << '\n';
    std::cout << "reserved=" << a.bytes_reserved() << " allocated=" << a.bytes_allocated() << '\n';
#else
    std::cout << "backing::mmap is supported only on Linux\n";
#endif
    return 0;
}
