#include "arena.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    fa::tls_arena a({
        .initial_capacity = 2048,
        .min_chunk_size = 2048,
        .max_chunk_size = 64 * 1024,
        .growth_factor = 2,
    });

    constexpr int thread_count = 4;
    constexpr int allocations_per_thread = 1000;

    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    std::atomic<int> ready{0};
    std::atomic<bool> start{false};

    for (int t = 0; t < thread_count; ++t) {
        workers.emplace_back([&]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
            }

            for (int i = 0; i < allocations_per_thread; ++i) {
                auto* value = a.new_object<std::uint64_t>(static_cast<std::uint64_t>(i));
                if (*value != static_cast<std::uint64_t>(i)) {
                    std::abort();
                }
            }
        });
    }

    while (ready.load(std::memory_order_acquire) != thread_count) {
    }
    start.store(true, std::memory_order_release);

    for (auto& worker : workers) {
        worker.join();
    }

    const auto s = a.stats();
    std::cout << "threads=" << thread_count << " allocations=" << s.total_allocations << '\n';
    std::cout << "allocated=" << a.bytes_allocated() << " chunk_count=" << s.chunk_count << '\n';
    return 0;
}
