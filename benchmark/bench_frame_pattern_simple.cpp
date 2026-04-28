#include "arena.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <new>
#include <vector>

int main() {
    std::cout << "FastArena vs std::allocator - Frame Pattern (mark/rollback)\n";
    std::cout << "===========================================================\n\n";

    for (auto [frames, per_frame] : std::vector<std::pair<int, int>>{{10, 50}, {10, 100}, {10, 200}, {50, 100}}) {
        std::cout << "Frames: " << frames << ", Allocs/Frame: " << per_frame << "\n";

        // FastArena frame pattern
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int run = 0; run < 50; ++run) {
            fa::arena frame_arena({
                .initial_capacity = 8192,
                .min_chunk_size = 8192,
                .max_chunk_size = 1 << 20,
                .growth_factor = 2,
            });

            for (int frame = 0; frame < frames; ++frame) {
                auto m = frame_arena.mark();

                for (int i = 0; i < per_frame; ++i) {
                    (void)frame_arena.allocate_object<std::array<float, 16>>(1);
                }

                frame_arena.rollback(m);
            }
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        auto fa_duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

        // std::allocator frame pattern
        auto t2 = std::chrono::high_resolution_clock::now();
        for (int run = 0; run < 50; ++run) {
            for (int frame = 0; frame < frames; ++frame) {
                std::vector<void*> frame_ptrs;
                frame_ptrs.reserve(per_frame);

                for (int i = 0; i < per_frame; ++i) {
                    frame_ptrs.push_back(::operator new(sizeof(std::array<float, 16>)));
                }

                for (void* ptr : frame_ptrs) {
                    ::operator delete(ptr);
                }
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
