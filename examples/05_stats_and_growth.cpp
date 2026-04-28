#include "arena.hpp"

#include <array>
#include <cstddef>
#include <iostream>

int main() {
    fa::arena a({
        .initial_capacity = 128,
        .min_chunk_size = 128,
        .max_chunk_size = 4096,
        .growth_factor = 2,
    });

    constexpr std::array<std::size_t, 8> sizes{16, 24, 40, 64, 96, 128, 256, 512};

    for (int round = 0; round < 100; ++round) {
        for (std::size_t size : sizes) {
            (void)a.allocate(size, alignof(std::max_align_t));
        }
    }

    const auto s = a.stats();
    std::cout << "allocations=" << s.total_allocations << '\n';
    std::cout << "bytes_requested=" << s.bytes_requested << '\n';
    std::cout << "bytes_allocated=" << s.bytes_allocated << '\n';
    std::cout << "wasted_bytes=" << s.wasted_bytes << '\n';
    std::cout << "peak_bytes_used=" << s.peak_bytes_used << '\n';
    std::cout << "chunk_count=" << s.chunk_count << '\n';
    std::cout << "bytes_reserved=" << a.bytes_reserved() << '\n';
    return 0;
}
