#include "arena.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

#if defined(__linux__)
#include <sys/mman.h>
#endif

namespace fa {

namespace {

[[nodiscard]] constexpr bool is_power_of_two(std::size_t value) noexcept {
	return value != 0 && (value & (value - 1)) == 0;
}

[[nodiscard]] constexpr std::uintptr_t align_up(std::uintptr_t ptr, std::size_t alignment) noexcept {
	return (ptr + (alignment - 1)) & ~(static_cast<std::uintptr_t>(alignment) - 1U);
}

[[nodiscard]] std::size_t saturating_mul(std::size_t lhs, std::size_t rhs) noexcept {
	if (lhs == 0 || rhs == 0) {
		return 0;
	}
	if (lhs > (std::numeric_limits<std::size_t>::max() / rhs)) {
		return std::numeric_limits<std::size_t>::max();
	}
	return lhs * rhs;
}

[[nodiscard]] std::size_t checked_add(std::size_t lhs, std::size_t rhs) {
	if (lhs > (std::numeric_limits<std::size_t>::max() - rhs)) {
		throw std::bad_alloc();
	}
	return lhs + rhs;
}

}  // namespace

template <typename Policy>
struct basic_arena<Policy>::chunk {
	std::byte* data{};
	std::size_t capacity{};
	std::size_t used{};
	std::uintptr_t id{};
	chunk* next{};
};

template <typename Policy>
basic_arena<Policy>::basic_arena(arena_config cfg)
	: cfg_(cfg),
	  next_chunk_capacity_(cfg_.initial_capacity == 0 ? cfg_.min_chunk_size : cfg_.initial_capacity) {
	if (cfg_.min_chunk_size == 0) {
		throw std::invalid_argument("arena_config.min_chunk_size must be > 0");
	}
	if (cfg_.growth_factor < 1) {
		throw std::invalid_argument("arena_config.growth_factor must be >= 1");
	}
	if (cfg_.max_chunk_size < cfg_.min_chunk_size) {
		throw std::invalid_argument("arena_config.max_chunk_size must be >= min_chunk_size");
	}
	if (cfg_.initial_capacity > cfg_.max_chunk_size && cfg_.initial_capacity != 0) {
		throw std::invalid_argument("arena_config.initial_capacity must be <= max_chunk_size");
	}

	next_chunk_capacity_ = std::max(next_chunk_capacity_, cfg_.min_chunk_size);
	next_chunk_capacity_ = std::min(next_chunk_capacity_, cfg_.max_chunk_size);

	if (cfg_.initial_capacity != 0) {
		head_ = make_chunk(cfg_.initial_capacity);
		current_ = head_;
	}
}

template <typename Policy>
basic_arena<Policy>::~basic_arena() {
	destroy_chunks();
}

template <typename Policy>
basic_arena<Policy>::basic_arena(basic_arena&& other) noexcept {
	std::scoped_lock lock(other.lock_);
	cfg_ = other.cfg_;
	head_ = other.head_;
	current_ = other.current_;
	bytes_reserved_ = other.bytes_reserved_;
	next_chunk_capacity_ = other.next_chunk_capacity_;
	next_chunk_id_ = other.next_chunk_id_;
	stats_ = other.stats_;

	other.head_ = nullptr;
	other.current_ = nullptr;
	other.bytes_reserved_ = 0;
	other.next_chunk_capacity_ = other.cfg_.min_chunk_size;
	other.next_chunk_id_ = 1;
	other.stats_ = {};
}

template <typename Policy>
basic_arena<Policy>& basic_arena<Policy>::operator=(basic_arena&& other) noexcept {
	if (this == &other) {
		return *this;
	}

	std::scoped_lock lock(lock_, other.lock_);
	destroy_chunks();

	cfg_ = other.cfg_;
	head_ = other.head_;
	current_ = other.current_;
	bytes_reserved_ = other.bytes_reserved_;
	next_chunk_capacity_ = other.next_chunk_capacity_;
	next_chunk_id_ = other.next_chunk_id_;
	stats_ = other.stats_;

	other.head_ = nullptr;
	other.current_ = nullptr;
	other.bytes_reserved_ = 0;
	other.next_chunk_capacity_ = other.cfg_.min_chunk_size;
	other.next_chunk_id_ = 1;
	other.stats_ = {};

	return *this;
}

template <typename Policy>
void* basic_arena<Policy>::allocate(std::size_t size, std::size_t alignment) {
	if (size == 0) {
		size = 1;
	}
	if (!is_power_of_two(alignment)) {
		throw std::invalid_argument("alignment must be a power of two");
	}

	std::scoped_lock lock(lock_);

	std::size_t padding = 0;
	if (void* ptr = allocate_from_existing_chunks(size, alignment, &padding); ptr != nullptr) {
		stats_.total_allocations += 1;
		stats_.bytes_requested = checked_add(stats_.bytes_requested, size);
		stats_.bytes_allocated = checked_add(stats_.bytes_allocated, size + padding);
		stats_.wasted_bytes = checked_add(stats_.wasted_bytes, padding);
		stats_.peak_bytes_used = std::max(stats_.peak_bytes_used, bytes_allocated());
		return ptr;
	}

	const std::size_t required = checked_add(size, alignment - 1);
	chunk* created = make_chunk(required);
	if (current_ != nullptr) {
		current_->next = created;
	} else {
		head_ = created;
	}
	current_ = created;

	void* ptr = allocate_from_existing_chunks(size, alignment, &padding);
	if (ptr == nullptr) {
		throw std::bad_alloc();
	}

	stats_.total_allocations += 1;
	stats_.bytes_requested = checked_add(stats_.bytes_requested, size);
	stats_.bytes_allocated = checked_add(stats_.bytes_allocated, size + padding);
	stats_.wasted_bytes = checked_add(stats_.wasted_bytes, padding);
	stats_.peak_bytes_used = std::max(stats_.peak_bytes_used, bytes_allocated());
	return ptr;
}

template <typename Policy>
void basic_arena<Policy>::reset() noexcept {
	std::scoped_lock lock(lock_);
	for (chunk* it = head_; it != nullptr; it = it->next) {
		it->used = 0;
	}
	current_ = head_;
}

template <typename Policy>
arena_marker basic_arena<Policy>::mark() const noexcept {
	std::scoped_lock lock(lock_);
	if (current_ == nullptr) {
		return {};
	}
	return {.chunk_id = current_->id, .offset = current_->used};
}

template <typename Policy>
void basic_arena<Policy>::rollback(arena_marker marker) noexcept {
	std::scoped_lock lock(lock_);
	if (marker.chunk_id == 0 || head_ == nullptr) {
		return;
	}

	chunk* target = nullptr;
	for (chunk* it = head_; it != nullptr; it = it->next) {
		if (it->id == marker.chunk_id) {
			target = it;
			break;
		}
	}
	if (target == nullptr || marker.offset > target->capacity) {
		return;
	}

	target->used = marker.offset;
	for (chunk* it = target->next; it != nullptr; it = it->next) {
		it->used = 0;
	}
	current_ = target;
}

template <typename Policy>
std::size_t basic_arena<Policy>::bytes_requested() const noexcept {
	std::scoped_lock lock(lock_);
	return stats_.bytes_requested;
}

template <typename Policy>
std::size_t basic_arena<Policy>::bytes_allocated() const noexcept {
	std::size_t used = 0;
	for (chunk* it = head_; it != nullptr; it = it->next) {
		used += it->used;
	}
	return used;
}

template <typename Policy>
std::size_t basic_arena<Policy>::bytes_reserved() const noexcept {
	std::scoped_lock lock(lock_);
	return bytes_reserved_;
}

template <typename Policy>
arena_stats basic_arena<Policy>::stats() const noexcept {
	std::scoped_lock lock(lock_);
	arena_stats copy = stats_;
	copy.chunk_count = 0;
	for (chunk* it = head_; it != nullptr; it = it->next) {
		copy.chunk_count += 1;
	}
	return copy;
}

template <typename Policy>
void* basic_arena<Policy>::allocate_from_existing_chunks(std::size_t size, std::size_t alignment, std::size_t* padding) noexcept {
	chunk* start = current_ == nullptr ? head_ : current_;
	for (chunk* it = start; it != nullptr; it = it->next) {
		const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(it->data + it->used);
		const std::uintptr_t aligned = align_up(raw, alignment);
		const std::size_t pad = static_cast<std::size_t>(aligned - raw);
		if (it->used + pad + size > it->capacity) {
			continue;
		}

		it->used += pad + size;
		current_ = it;
		*padding = pad;
		return reinterpret_cast<void*>(aligned);
	}
	return nullptr;
}

template <typename Policy>
typename basic_arena<Policy>::chunk* basic_arena<Policy>::make_chunk(std::size_t min_size) {
	std::size_t capacity = std::max(min_size, next_chunk_capacity_);
	capacity = std::max(capacity, cfg_.min_chunk_size);
	if (capacity > cfg_.max_chunk_size) {
		capacity = cfg_.max_chunk_size;
	}
	if (capacity < min_size) {
		throw std::bad_alloc();
	}

	std::byte* data = nullptr;
	if (cfg_.backing_alloc == backing::mmap) {
#if defined(__linux__)
		void* mapped = ::mmap(nullptr, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (mapped == MAP_FAILED) {
			throw std::bad_alloc();
		}
		data = static_cast<std::byte*>(mapped);
#else
		throw std::invalid_argument("backing::mmap is only supported on Linux");
#endif
	} else {
		data = static_cast<std::byte*>(::operator new(capacity));
	}

	auto* node = new chunk{.data = data, .capacity = capacity, .used = 0, .id = next_chunk_id_++, .next = nullptr};
	bytes_reserved_ += capacity;

	const std::size_t grown = saturating_mul(capacity, cfg_.growth_factor);
	next_chunk_capacity_ = std::clamp(grown, cfg_.min_chunk_size, cfg_.max_chunk_size);
	return node;
}

template <typename Policy>
void basic_arena<Policy>::destroy_chunks() noexcept {
	chunk* it = head_;
	while (it != nullptr) {
		chunk* next = it->next;
		if (cfg_.backing_alloc == backing::mmap) {
#if defined(__linux__)
			::munmap(it->data, it->capacity);
#endif
		} else {
			::operator delete(it->data);
		}
		delete it;
		it = next;
	}

	head_ = nullptr;
	current_ = nullptr;
	bytes_reserved_ = 0;
}

arena_resource::arena_resource(arena_config cfg, std::pmr::memory_resource* upstream)
	: arena_(std::move(cfg)),
	  upstream_(upstream == nullptr ? std::pmr::get_default_resource() : upstream),
	  upstream_allocations_(std::pmr::polymorphic_allocator<void*>(upstream_)) {}

arena& arena_resource::get_arena() noexcept {
	return arena_;
}

const arena& arena_resource::get_arena() const noexcept {
	return arena_;
}

void* arena_resource::do_allocate(std::size_t bytes, std::size_t alignment) {
	try {
		return arena_.allocate(bytes, alignment);
	} catch (const std::bad_alloc&) {
		void* ptr = upstream_->allocate(bytes, alignment);
		upstream_allocations_.insert(ptr);
		return ptr;
	}
}

void arena_resource::do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept {
	if (p == nullptr) {
		return;
	}

	auto it = upstream_allocations_.find(p);
	if (it != upstream_allocations_.end()) {
		upstream_allocations_.erase(it);
		upstream_->deallocate(p, bytes, alignment);
	}
}

bool arena_resource::do_is_equal(const std::pmr::memory_resource& other) const noexcept {
	return this == &other;
}

template class basic_arena<single_thread_t>;
template class basic_arena<thread_local_t>;

}  // namespace fa
