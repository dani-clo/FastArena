#include "arena.hpp"

#include <array>
#include <iostream>

int main() {
    fa::arena frame_arena({
        .initial_capacity = 1024,
        .min_chunk_size = 1024,
        .max_chunk_size = 16 * 1024,
        .growth_factor = 2,
    });

    for (int frame = 0; frame < 5; ++frame) {
        auto m = frame_arena.mark();

        for (int i = 0; i < 200; ++i) {
            auto* temp = frame_arena.allocate_object<std::array<float, 4>>(1);
            (*temp)[0] = static_cast<float>(frame);
            (*temp)[1] = static_cast<float>(i);
        }

        std::cout << "frame=" << frame << " allocated=" << frame_arena.bytes_allocated() << '\n';
        frame_arena.rollback(m);
        std::cout << "frame=" << frame << " after rollback allocated=" << frame_arena.bytes_allocated() << '\n';
    }

    return 0;
}
