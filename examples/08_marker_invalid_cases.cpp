#include "arena.hpp"

#include <cstdint>
#include <iostream>

int main() {
    fa::arena first({
        .initial_capacity = 128,
        .min_chunk_size = 128,
        .max_chunk_size = 128,
        .growth_factor = 1,
    });

    fa::arena second({
        .initial_capacity = 128,
        .min_chunk_size = 128,
        .max_chunk_size = 128,
        .growth_factor = 1,
    });

    (void)first.allocate_object<std::uint32_t>(8);
    const auto baseline = first.bytes_allocated();

    const auto marker_from_other = second.mark();
    first.rollback(marker_from_other);
    std::cout << "cross-arena marker ignored: " << (first.bytes_allocated() == baseline) << '\n';

    const auto marker = first.mark();
    (void)first.allocate_object<std::uint64_t>(4);
    const auto after_more = first.bytes_allocated();

    fa::arena_marker forged = marker;
    forged.chunk_handle ^= static_cast<std::uintptr_t>(0x40U);
    first.rollback(forged);
    std::cout << "forged marker ignored: " << (first.bytes_allocated() == after_more) << '\n';

    first.rollback(marker);
    std::cout << "valid marker restores baseline: " << (first.bytes_allocated() == baseline) << '\n';
    return 0;
}
