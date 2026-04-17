// Copyright (c) FastArena contributors.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace fa {

enum class backing { system_malloc, mmap };

struct arena_config {
    std::size_t initial_capacity = 0;
    std::size_t min_chunk_size = 4096;
    std::size_t max_chunk_size = std::numeric_limits<std::size_t>::max();
    std::size_t growth_factor = 2;
    backing backing_alloc = backing::system_malloc;
    bool debug = false;
};

struct arena_stats {
    std::size_t total_allocations{};
    std::size_t bytes_requested{};
    std::size_t bytes_allocated{};
    std::size_t wasted_bytes{};
    std::size_t peak_bytes_used{};
    std::size_t chunk_count{};
};

struct arena_marker {
    std::uintptr_t chunk_id{};
    std::size_t offset{};
};

struct single_thread_t {};
struct thread_local_t {};

struct no_lock {
    void lock() noexcept {}
    bool try_lock() noexcept { return true; }
    void unlock() noexcept {}
};

template <typename Policy = single_thread_t>
class basic_arena {
public:
    explicit basic_arena(arena_config cfg = {});
    ~basic_arena();

    basic_arena(const basic_arena&) = delete;
    basic_arena& operator=(const basic_arena&) = delete;
    basic_arena(basic_arena&& other) noexcept;
    basic_arena& operator=(basic_arena&& other) noexcept;

    [[nodiscard]] void* allocate(
        std::size_t size,
        std::size_t alignment = alignof(std::max_align_t));

    template <typename T>
    [[nodiscard]] T* allocate_object(std::size_t n = 1) {
        static_assert(!std::is_const_v<T>, "T must not be const");
        static_assert(!std::is_volatile_v<T>, "T must not be volatile");

        if (n > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
            throw std::bad_array_new_length();
        }

        return static_cast<T*>(allocate(sizeof(T) * n, alignof(T)));
    }

    template <typename T, typename... Args>
    [[nodiscard]] T* new_object(Args&&... args) {
        T* ptr = allocate_object<T>(1);
        return ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
    }

    void reset() noexcept;

    [[nodiscard]] arena_marker mark() const noexcept;
    void rollback(arena_marker marker) noexcept;

    [[nodiscard]] std::size_t bytes_requested() const noexcept;
    [[nodiscard]] std::size_t bytes_allocated() const noexcept;
    [[nodiscard]] std::size_t bytes_reserved() const noexcept;
    [[nodiscard]] arena_stats stats() const noexcept;

private:
    struct chunk;
    using lock_type = std::conditional_t<std::is_same_v<Policy, thread_local_t>, std::mutex, no_lock>;

    arena_config cfg_{};
    chunk* head_{};
    chunk* current_{};
    std::size_t bytes_reserved_{};
    std::size_t next_chunk_capacity_{};
    std::uintptr_t next_chunk_id_{1};
    arena_stats stats_{};
    mutable lock_type lock_{};

    void* allocate_from_existing_chunks(std::size_t size, std::size_t alignment, std::size_t* padding) noexcept;
    [[nodiscard]] chunk* make_chunk(std::size_t min_size);
    void destroy_chunks() noexcept;
};

using arena = basic_arena<single_thread_t>;
using tls_arena = basic_arena<thread_local_t>;

class arena_resource : public std::pmr::memory_resource {
public:
    explicit arena_resource(
        arena_config cfg = {},
        std::pmr::memory_resource* upstream = std::pmr::get_default_resource());

    [[nodiscard]] arena& get_arena() noexcept;
    [[nodiscard]] const arena& get_arena() const noexcept;

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept override;
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

private:
    arena arena_;
    std::pmr::memory_resource* upstream_;
    std::pmr::unordered_set<void*> upstream_allocations_;
};

template <typename T>
class arena_allocator {
public:
    using value_type = T;

    explicit arena_allocator(arena& a) noexcept : arena_(&a) {}

    template <typename U>
    arena_allocator(const arena_allocator<U>& other) noexcept : arena_(other.arena_) {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
            throw std::bad_array_new_length();
        }

        return static_cast<T*>(arena_->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T*, std::size_t) noexcept {}

    template <typename U>
    bool operator==(const arena_allocator<U>& other) const noexcept {
        return arena_ == other.arena_;
    }

    template <typename U>
    bool operator!=(const arena_allocator<U>& other) const noexcept {
        return !(*this == other);
    }

private:
    template <typename U>
    friend class arena_allocator;

    arena* arena_;
};

extern template class basic_arena<single_thread_t>;
extern template class basic_arena<thread_local_t>;

}  // namespace fa