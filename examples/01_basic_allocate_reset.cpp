#include "arena.hpp"

#include <cstdint>
#include <iostream>

int main() {
    fa::arena a({
        .initial_capacity = 256,
        .min_chunk_size = 256,
        .max_chunk_size = 4096,
        .growth_factor = 2,
    });

    auto* x = a.new_object<std::uint64_t>(42ULL);
    auto* values = a.allocate_object<int>(8);
    for (int i = 0; i < 8; ++i) {
        values[i] = i * i;
    }

    std::cout << "x=" << *x << " allocated=" << a.bytes_allocated() << " reserved=" << a.bytes_reserved() << '\n';

    a.reset();
    std::cout << "after reset allocated=" << a.bytes_allocated() << '\n';
    return 0;
}
