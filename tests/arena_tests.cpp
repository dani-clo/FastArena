#include "arena.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <memory_resource>
#include <unordered_set>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct test_context {
	int failures = 0;

	void expect(bool condition, std::string_view message) {
		if (!condition) {
			std::cerr << "FAILED: " << message << '\n';
			failures += 1;
		}
	}
};

class counting_resource final : public std::pmr::memory_resource {
public:
	bool is_live(void* ptr) const {
		std::scoped_lock lock(lock_);
		return live_allocations_.find(ptr) != live_allocations_.end();
	}

protected:
	void* do_allocate(std::size_t bytes, std::size_t alignment) override {
		void* ptr = std::pmr::new_delete_resource()->allocate(bytes, alignment);
		std::scoped_lock lock(lock_);
		live_allocations_.insert(ptr);
		return ptr;
	}

	void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept override {
		{
			std::scoped_lock lock(lock_);
			live_allocations_.erase(p);
		}
		std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
	}

	bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
		return this == &other;
	}

private:
	mutable std::mutex lock_;
	std::unordered_set<void*> live_allocations_;
};

void test_invalid_and_cross_arena_markers(test_context& ctx) {
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
	auto baseline = first.bytes_allocated();
	auto cross_marker = second.mark();
	first.rollback(cross_marker);
	ctx.expect(first.bytes_allocated() == baseline, "cross-arena marker must be ignored");

	auto valid_marker = first.mark();
	(void)first.allocate_object<std::uint64_t>(4);
	auto after_more_allocations = first.bytes_allocated();

	fa::arena_marker forged = valid_marker;
	forged.chunk_handle ^= static_cast<std::uintptr_t>(0x40U);
	first.rollback(forged);
	ctx.expect(first.bytes_allocated() == after_more_allocations, "forged marker must be ignored safely");

	first.rollback(valid_marker);
	ctx.expect(first.bytes_allocated() == baseline, "valid marker must restore previous allocation point");

	auto reset_marker = first.mark();
	(void)first.allocate_object<std::uint16_t>(16);
	first.reset();
	(void)first.allocate_object<std::uint16_t>(8);
	const auto after_reset_reuse = first.bytes_allocated();
	first.rollback(reset_marker);
	ctx.expect(first.bytes_allocated() == after_reset_reuse, "marker from previous generation must be ignored");
}

void test_upstream_fallback_cleanup(test_context& ctx) {
	counting_resource upstream;
	void* p1 = nullptr;
	void* p2 = nullptr;
	{
		fa::arena_resource resource({
			.initial_capacity = 64,
			.min_chunk_size = 64,
			.max_chunk_size = 64,
			.growth_factor = 1,
		}, &upstream);

		p1 = resource.allocate(256, alignof(std::max_align_t));
		p2 = resource.allocate(512, alignof(std::max_align_t));
		ctx.expect(p1 != nullptr && p2 != nullptr, "fallback allocations must succeed");
		ctx.expect(upstream.is_live(p1), "first fallback pointer must be owned by upstream while resource is alive");
		ctx.expect(upstream.is_live(p2), "second fallback pointer must be owned by upstream while resource is alive");
	}

	ctx.expect(!upstream.is_live(p1), "resource destructor must release first fallback pointer");
	ctx.expect(!upstream.is_live(p2), "resource destructor must release second fallback pointer");
}

void test_tls_arena_concurrent_allocations(test_context& ctx) {
	fa::tls_arena arena({
		.initial_capacity = 1024,
		.min_chunk_size = 1024,
		.max_chunk_size = 1024,
		.growth_factor = 1,
	});

	constexpr int thread_count = 4;
	constexpr int allocations_per_thread = 512;
	std::atomic<int> ready{0};
	std::atomic<bool> start{false};
	std::vector<std::thread> threads;
	threads.reserve(thread_count);

	for (int i = 0; i < thread_count; ++i) {
		threads.emplace_back([&arena, &ready, &start]() {
			ready.fetch_add(1, std::memory_order_release);
			while (!start.load(std::memory_order_acquire)) {
			}

			for (int index = 0; index < allocations_per_thread; ++index) {
				auto* value = arena.new_object<std::uint64_t>(static_cast<std::uint64_t>(index));
				if (*value != static_cast<std::uint64_t>(index)) {
					std::abort();
				}
			}
		});
	}

	while (ready.load(std::memory_order_acquire) != thread_count) {
	}
	start.store(true, std::memory_order_release);

	for (auto& thread : threads) {
		thread.join();
	}

	const auto stats = arena.stats();
	ctx.expect(stats.total_allocations == static_cast<std::size_t>(thread_count * allocations_per_thread), "tls_arena must account for all concurrent allocations");
	ctx.expect(arena.bytes_allocated() >= stats.bytes_requested, "allocated bytes should include at least requested bytes");
	ctx.expect(stats.chunk_count >= 1, "tls_arena must have at least one chunk after allocations");
}

}  // namespace

int main() {
	test_context ctx;

	try {
		test_invalid_and_cross_arena_markers(ctx);
		test_upstream_fallback_cleanup(ctx);
		test_tls_arena_concurrent_allocations(ctx);
	} catch (const std::exception& ex) {
		std::cerr << "Unexpected exception: " << ex.what() << '\n';
		return 1;
	} catch (...) {
		std::cerr << "Unexpected non-standard exception\n";
		return 1;
	}

	if (ctx.failures != 0) {
		std::cerr << ctx.failures << " test expectation(s) failed\n";
		return 1;
	}

	std::cout << "All FastArena tests passed\n";
	return 0;
}