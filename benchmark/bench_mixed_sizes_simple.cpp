#include "arena.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <new>
#include <random>
#include <vector>

int main() {
    std::cout << "FastArena vs std::allocator - Mixed Sizes\n";
    std::cout << "==========================================\n\n";

    for (int allocation_count : {100, 500, 1000, 5000}) {
        std::cout << "Allocation count: " << allocation_count << "\n";

        std::mt19937 rng{42};
        std::uniform_int_distribution<std::size_t> size_dist(16, 1024);

        std::vector<std::size_t> sizes;
        sizes.reserve(allocation_count);
        for (int i = 0; i < allocation_count; ++i) {
            sizes.push_back(size_dist(rng));
        }

        // FastArena
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int run = 0; run < 100; ++run) {
            fa::arena a({
                .initial_capacity = 16384,
                .min_chunk_size = 16384,
                .max_chunk_size = 1 << 20,
                .growth_factor = 2,
            });

            for (std::size_t sz : sizes) {
                (void)a.allocate(sz, alignof(std::max_align_t));
            }
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        auto fa_duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

        // std::allocator
        auto t2 = std::chrono::high_resolution_clock::now();
        for (int run = 0; run < 100; ++run) {
            std::vector<void*> ptrs;
            ptrs.reserve(allocation_count);

            for (std::size_t sz : sizes) {
                ptrs.push_back(::operator new(sz));
            }

            for (void* ptr : ptrs) {
                ::operator delete(ptr);
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
