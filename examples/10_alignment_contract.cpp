#include "arena.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    fa::arena a({
        .initial_capacity = 256,
        .min_chunk_size = 256,
        .max_chunk_size = 1024,
        .growth_factor = 2,
    });

    void* zero_size = a.allocate(0, alignof(std::max_align_t));
    std::cout << "allocate(0, alignof(max_align_t)) returned non-null: " << (zero_size != nullptr) << '\n';

    try {
        (void)a.allocate(32, 3);
        std::cout << "unexpected: invalid alignment accepted\n";
    } catch (const std::invalid_argument& ex) {
        std::cout << "caught expected invalid_argument: " << ex.what() << '\n';
    }

    return 0;
}
