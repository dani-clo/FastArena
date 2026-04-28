#include "arena.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <new>
#include <vector>

struct uint64_wrapper {
    std::uint64_t value;
};

int main() {
    std::cout << "FastArena vs std::allocator - Small Fixed Allocations\n";
    std::cout << "=====================================================\n\n";

    for (int allocation_count : {100, 500, 1000, 5000, 10000}) {
        std::cout << "Allocation count: " << allocation_count << "\n";

        // FastArena
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int run = 0; run < 100; ++run) {
            fa::arena a({
                .initial_capacity = 4096,
                .min_chunk_size = 4096,
                .max_chunk_size = 1 << 20,
                .growth_factor = 2,
            });

            for (int i = 0; i < allocation_count; ++i) {
                (void)a.allocate_object<uint64_wrapper>(1);
            }
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        auto fa_duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

        // std::allocator
        auto t2 = std::chrono::high_resolution_clock::now();
        for (int run = 0; run < 100; ++run) {
            std::vector<uint64_wrapper*> ptrs;
            ptrs.reserve(allocation_count);

            for (int i = 0; i < allocation_count; ++i) {
                ptrs.push_back(new uint64_wrapper{});
            }

            for (auto* ptr : ptrs) {
                delete ptr;
            }
        }
        auto t3 = std::chrono::high_resolution_clock::now();
        auto std_duration = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);

        std::cout << "  FastArena:   " << std::setw(10) << fa_duration.count() << " us\n";
        std::cout << "  std::alloc:  " << std::setw(10) << std_duration.count() << " us";
        if (fa_duration.count() > 0) {
            double ratio = static_cast<double>(std_duration.count()) / static_cast<double>(fa_duration.count());
            std::cout << " (" << std::fixed << std::setprecision(2) << ratio << "x)";
        }
        std::cout << "\n\n";
    }

    return 0;
}
