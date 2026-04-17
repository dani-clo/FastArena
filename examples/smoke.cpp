#include "arena.hpp"

#include <iostream>
#include <memory_resource>
#include <string>
#include <vector>

int main() {
    fa::arena_resource resource({
        .initial_capacity = 1024,
        .min_chunk_size = 1024,
        .max_chunk_size = 1 << 20,
        .growth_factor = 2,
        .backing_alloc = fa::backing::system_malloc,
        .debug = true,
    });

    std::pmr::vector<int> values{&resource};
    for (int i = 0; i < 1000; ++i) {
        values.push_back(i);
    }

    std::pmr::string text{"FastArena PMR", &resource};

    auto& a = resource.get_arena();
    auto* x = a.new_object<std::uint64_t>(42ULL);

    std::cout << text << " size=" << values.size() << " x=" << *x << '\n';
    std::cout << "reserved=" << a.bytes_reserved() << " allocated=" << a.bytes_allocated() << '\n';
    return 0;
}
