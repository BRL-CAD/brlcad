/*                        H E A P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>

#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"


namespace {

/* Preserve the original fast heap's supported allocation range. */
constexpr size_t heap_bin_count = 256;
/* Corpus profiling found a 96% cache hit rate with one MiB per worker.
 * Sixty-four reusable slots retain that benefit while bounding the total
 * process cache charge at 64 MiB. */
constexpr size_t process_cache_charge_limit = 64 * 1024 * 1024;
constexpr size_t thread_cache_charge_limit = 1024 * 1024;
constexpr size_t thread_cache_count =
    process_cache_charge_limit / thread_cache_charge_limit;
constexpr size_t allocator_metadata_charge = 2 * sizeof(void *);
constexpr size_t allocator_alignment = alignof(std::max_align_t);
static_assert(process_cache_charge_limit % thread_cache_charge_limit == 0,
    "cache charge limit must contain a whole number of thread caches");

struct heap_statistics {
    std::atomic<size_t> allocations{0};
    std::atomic<size_t> releases{0};
    std::atomic<size_t> live{0};
    std::atomic<size_t> peak_live{0};
    std::atomic<size_t> cache_hits{0};
    std::atomic<size_t> cache_misses{0};
};


heap_statistics heap_stats;
std::atomic<bu_heap_func_t> heap_logger{nullptr};
std::atomic<size_t> compaction_generation{0};


bool
cacheable(size_t size)
{
    return size >= sizeof(void *) && size <= heap_bin_count;
}


size_t
cache_charge(size_t size)
{
    /* There is no portable malloc_usable_size.  Include a conservative
     * header allowance and alignment rounding so tiny allocations cannot
     * evade the cache bound through allocator metadata. */
    const size_t estimated_size = size + allocator_metadata_charge;
    return ((estimated_size + allocator_alignment - 1) /
	allocator_alignment) * allocator_alignment;
}


class thread_heap_cache {
public:
    void *get(size_t size)
    {
	synchronize();
	if (!::cacheable(size))
	    return nullptr;

	void *allocation = available[size - 1];
	if (!allocation)
	    return nullptr;

	std::memcpy(&available[size - 1], allocation, sizeof(allocation));
	cached_charge -= cache_charge(size);
	return allocation;
    }

    bool put(void *allocation, size_t size)
    {
	synchronize();
	if (!::cacheable(size))
	    return false;

	const size_t charge = cache_charge(size);
	if (charge > thread_cache_charge_limit - cached_charge)
	    return false;

	std::memcpy(allocation, &available[size - 1], sizeof(allocation));
	available[size - 1] = allocation;
	cached_charge += charge;
	return true;
    }

    void compact(size_t generation)
    {
	clear();
	observed_generation = generation;
    }

    void synchronize()
    {
	synchronize(compaction_generation.load(std::memory_order_relaxed));
    }

private:
    void synchronize(size_t generation)
    {
	if (generation != observed_generation)
	    compact(generation);
    }

    void clear()
    {
	for (void *&head : available) {
	    while (head) {
		void *allocation = head;
		std::memcpy(&head, allocation, sizeof(head));
		std::free(allocation);
	    }
	}
	cached_charge = 0;
    }

    std::array<void *, heap_bin_count> available{};
    size_t cached_charge = 0;
    size_t observed_generation = 0;
};


class heap_cache_pool {
public:
    size_t acquire()
    {
	std::lock_guard<std::mutex> guard(lock);
	for (size_t slot = 0; slot < thread_cache_count; ++slot) {
	    if (in_use[slot])
		continue;

	    caches[slot].synchronize();
	    in_use[slot] = true;
	    return slot;
	}
	return thread_cache_count;
    }

    void release(size_t slot)
    {
	if (slot == thread_cache_count)
	    return;

	std::lock_guard<std::mutex> guard(lock);
	caches[slot].synchronize();
	in_use[slot] = false;
    }

    thread_heap_cache *get(size_t slot)
    {
	return slot < thread_cache_count ? &caches[slot] : nullptr;
    }

    void compact_inactive(size_t generation)
    {
	std::lock_guard<std::mutex> guard(lock);
	for (size_t slot = 0; slot < thread_cache_count; ++slot) {
	    if (!in_use[slot])
		caches[slot].compact(generation);
	}
    }

private:
    std::array<thread_heap_cache, thread_cache_count> caches;
    std::array<bool, thread_cache_count> in_use{};
    std::mutex lock;
};


heap_cache_pool &
cache_pool()
{
    /* Thread-local destructors can run late in process shutdown.  Keep the
     * registry alive so returning a cache slot never touches a dead mutex. */
    static heap_cache_pool *pool = []() {
	heap_cache_pool *allocation = new (std::nothrow) heap_cache_pool;
	if (!allocation)
	    bu_bomb("bu_heap_get(): unable to initialize cache pool\n");
	return allocation;
    }();
    return *pool;
}


thread_local thread_heap_cache *active_thread_cache = nullptr;


class thread_cache_handle {
public:
    thread_cache_handle() : pool(&cache_pool()), slot(pool->acquire()),
	cache(pool->get(slot))
    {
	active_thread_cache = cache;
    }

    thread_cache_handle(const thread_cache_handle &) = delete;
    thread_cache_handle &operator=(const thread_cache_handle &) = delete;

    ~thread_cache_handle()
    {
	active_thread_cache = nullptr;
	pool->release(slot);
    }

    thread_heap_cache *get()
    {
	return cache;
    }

private:
    heap_cache_pool *pool;
    size_t slot;
    thread_heap_cache *cache;
};


thread_heap_cache *
current_thread_cache()
{
    static thread_local thread_cache_handle handle;
    return handle.get();
}


thread_heap_cache *
existing_thread_cache()
{
    return active_thread_cache;
}


int
default_heap_logger(const char *fmt, ...)
{
    struct bu_vls output = BU_VLS_INIT_ZERO;
    va_list ap;

    va_start(ap, fmt);
    bu_vls_vprintf(&output, fmt, ap);
    bu_log("%s", bu_vls_addr(&output));
    bu_vls_free(&output);
    va_end(ap);

    return 0;
}


void
update_peak_live(size_t live)
{
    size_t peak = heap_stats.peak_live.load(std::memory_order_relaxed);
    while (live > peak &&
	!heap_stats.peak_live.compare_exchange_weak(peak, live,
	    std::memory_order_relaxed, std::memory_order_relaxed)) {
	/* compare_exchange_weak updates peak before the next test. */
    }
}


void
print_heap_statistics()
{
    bu_heap_func_t logger = heap_logger.load(std::memory_order_acquire);
    if (!logger)
	logger = default_heap_logger;

    logger("=======================\n"
	"Memory Heap Information\n"
	"-----------------------\n"
	"Allocations: %zu\n"
	"Releases: %zu\n"
	"Live allocations: %zu\n"
	"Peak live allocations: %zu\n"
	"Cache hits: %zu\n"
	"Cache misses: %zu\n"
	"=======================\n",
	heap_stats.allocations.load(std::memory_order_relaxed),
	heap_stats.releases.load(std::memory_order_relaxed),
	heap_stats.live.load(std::memory_order_relaxed),
	heap_stats.peak_live.load(std::memory_order_relaxed),
	heap_stats.cache_hits.load(std::memory_order_relaxed),
	heap_stats.cache_misses.load(std::memory_order_relaxed));
}


bool
heap_statistics_enabled()
{
    static const bool enabled = []() {
	const char *print_heap = std::getenv("BU_HEAP_PRINT");
	if (!print_heap || std::atoi(print_heap) <= 0)
	    return false;

	std::atexit(print_heap_statistics);
	return true;
    }();

    return enabled;
}

} // namespace


bu_heap_func_t
bu_heap_log(bu_heap_func_t logger)
{
    if (logger)
	heap_logger.store(logger, std::memory_order_release);

    bu_heap_func_t current = heap_logger.load(std::memory_order_acquire);
    return current ? current : default_heap_logger;
}


void *
bu_heap_get(size_t size)
{
    if (UNLIKELY(size == 0))
	bu_bomb("bu_heap_get(): zero-size allocation\n");

    const bool can_cache = cacheable(size);
    thread_heap_cache *cache = can_cache ? current_thread_cache() : nullptr;
    void *allocation = cache ? cache->get(size) : nullptr;
    const bool cache_hit = allocation != nullptr;
    if (!allocation) {
	allocation = std::calloc(1, size);
	if (UNLIKELY(!allocation))
	    bu_bomb("bu_heap_get(): insufficient memory\n");
    }

    if (heap_statistics_enabled()) {
	heap_stats.allocations.fetch_add(1, std::memory_order_relaxed);
	if (cache_hit)
	    heap_stats.cache_hits.fetch_add(1, std::memory_order_relaxed);
	else if (can_cache)
	    heap_stats.cache_misses.fetch_add(1, std::memory_order_relaxed);
	const size_t live = heap_stats.live.fetch_add(1,
	    std::memory_order_relaxed) + 1;
	update_peak_live(live);
    }

    return allocation;
}


void
bu_heap_put(void *allocation, size_t size)
{
    if (!allocation) {
	if (size == 0) {
	    /* Foreign thread-local caches purge before their next operation;
	     * unowned caches can be purged immediately. */
	    const size_t generation = compaction_generation.fetch_add(1,
		std::memory_order_relaxed) + 1;
	    thread_heap_cache *cache = existing_thread_cache();
	    if (cache)
		cache->compact(generation);
	    cache_pool().compact_inactive(generation);
	}
	return;
    }

    thread_heap_cache *cache = nullptr;
    if (cacheable(size))
	cache = current_thread_cache();
    if (!cache || !cache->put(allocation, size))
	std::free(allocation);

    if (heap_statistics_enabled()) {
	heap_stats.releases.fetch_add(1, std::memory_order_relaxed);
	heap_stats.live.fetch_sub(1, std::memory_order_relaxed);
    }
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
