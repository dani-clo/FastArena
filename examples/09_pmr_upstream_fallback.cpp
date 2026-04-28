#include "arena.hpp"

#include <iostream>
#include <memory_resource>
#include <mutex>
#include <unordered_set>

class counting_resource final : public std::pmr::memory_resource {
public:
    bool is_live(void* ptr) const {
        std::scoped_lock lock(lock_);
        return live_.find(ptr) != live_.end();
    }

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        void* ptr = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        std::scoped_lock lock(lock_);
        live_.insert(ptr);
        return ptr;
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept override {
        {
            std::scoped_lock lock(lock_);
            live_.erase(p);
        }
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    mutable std::mutex lock_;
    std::unordered_set<void*> live_;
};

int main() {
    counting_resource upstream;
    void* p = nullptr;

    {
        fa::arena_resource resource({
            .initial_capacity = 64,
            .min_chunk_size = 64,
            .max_chunk_size = 64,
            .growth_factor = 1,
        }, &upstream);

        p = resource.allocate(4096, alignof(std::max_align_t));
        std::cout << "allocated via upstream fallback: " << upstream.is_live(p) << '\n';

        resource.deallocate(p, 4096, alignof(std::max_align_t));
        std::cout << "deallocate forwarded to upstream: " << (!upstream.is_live(p)) << '\n';
    }

    return 0;
}
