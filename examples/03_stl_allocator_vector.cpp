#include "arena.hpp"

#include <iostream>
#include <vector>

int main() {
    fa::arena a({
        .initial_capacity = 1024,
        .min_chunk_size = 1024,
        .max_chunk_size = 64 * 1024,
        .growth_factor = 2,
    });

    std::vector<int, fa::arena_allocator<int>> values{fa::arena_allocator<int>(a)};
    for (int i = 0; i < 1000; ++i) {
        values.push_back(i);
    }

    std::cout << "vector size=" << values.size() << " first=" << values.front() << " last=" << values.back() << '\n';
    std::cout << "arena allocated=" << a.bytes_allocated() << " reserved=" << a.bytes_reserved() << '\n';
    return 0;
}
