#include "arena.hpp"

#include <iostream>
#include <memory_resource>
#include <string>
#include <vector>

int main() {
    fa::arena_resource resource({
        .initial_capacity = 2048,
        .min_chunk_size = 2048,
        .max_chunk_size = 1 << 20,
        .growth_factor = 2,
    });

    std::pmr::vector<int> values{&resource};
    for (int i = 0; i < 4096; ++i) {
        values.push_back(i);
    }

    std::pmr::string label{"pmr on FastArena", &resource};

    std::cout << label << " size=" << values.size() << '\n';
    std::cout << "bytes_allocated=" << resource.get_arena().bytes_allocated() << '\n';
    return 0;
}
