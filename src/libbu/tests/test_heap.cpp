/*                    T E S T _ H E A P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "bu.h"


namespace {

constexpr size_t small_allocation_size = 64;
constexpr size_t large_allocation_size = 1024;
constexpr size_t concurrent_threads = 8;
constexpr size_t concurrent_allocations = 4096;
/* Exercise the direct fallback beyond the implementation's 64 cache slots. */
constexpr size_t oversubscribed_threads = 80;
constexpr size_t stress_iterations = 10000;


void
join_threads(std::vector<std::thread> &threads)
{
    for (auto &thread : threads)
	thread.join();
}


int
test_allocation(size_t allocation_size)
{
    void *allocation = bu_heap_get(allocation_size);
    std::memset(allocation, 0xa5, allocation_size);
    bu_heap_put(allocation, allocation_size);
    return 0;
}


int
test_small_reuse()
{
    void *first = bu_heap_get(small_allocation_size);
    bu_heap_put(first, small_allocation_size);
    void *second = bu_heap_get(small_allocation_size);
    if (first != second) {
	bu_log("bu_heap_get did not reuse a released small allocation\n");
	return 1;
    }

    bu_heap_put(second, small_allocation_size);
    return 0;
}


int
test_cross_thread_release()
{
    struct allocation {
	void *pointer;
	size_t size;
    };
    std::array<std::vector<allocation>, concurrent_threads> allocations;
    std::vector<std::thread> threads;
    threads.reserve(concurrent_threads);

    for (size_t thread_id = 0; thread_id < concurrent_threads; ++thread_id) {
	threads.emplace_back([thread_id, &allocations]() {
	    auto &thread_allocations = allocations[thread_id];
	    thread_allocations.reserve(concurrent_allocations);
	    for (size_t i = 0; i < concurrent_allocations; ++i) {
		const size_t size = 1 + ((thread_id + i) % large_allocation_size);
		void *pointer = bu_heap_get(size);
		std::memset(pointer, 0xa5, size);
		thread_allocations.push_back({pointer, size});
	    }
	});
    }
    join_threads(threads);

    threads.clear();
    for (size_t thread_id = 0; thread_id < concurrent_threads; ++thread_id) {
	threads.emplace_back([thread_id, &allocations]() {
	    auto &other_allocations =
		allocations[(thread_id + 1) % concurrent_threads];
	    for (const auto &allocation : other_allocations)
		bu_heap_put(allocation.pointer, allocation.size);
	});
    }
    join_threads(threads);

    return 0;
}


int
test_thread_oversubscription()
{
    std::atomic<size_t> ready{0};
    std::atomic<bool> release{false};
    std::vector<std::thread> threads;
    threads.reserve(oversubscribed_threads);

    for (size_t thread_id = 0; thread_id < oversubscribed_threads;
	++thread_id) {
	threads.emplace_back([&ready, &release]() {
	    void *allocation = bu_heap_get(small_allocation_size);
	    ready.fetch_add(1, std::memory_order_release);
	    while (!release.load(std::memory_order_acquire))
		std::this_thread::yield();
	    std::memset(allocation, 0xa5, small_allocation_size);
	    bu_heap_put(allocation, small_allocation_size);
	});
    }

    while (ready.load(std::memory_order_acquire) < oversubscribed_threads)
	std::this_thread::yield();
    release.store(true, std::memory_order_release);
    join_threads(threads);

    return 0;
}


int
test_concurrent_compaction()
{
    void *live_allocation = bu_heap_get(small_allocation_size);
    bu_heap_put(nullptr, 0);
    std::memset(live_allocation, 0xa5, small_allocation_size);
    bu_heap_put(live_allocation, small_allocation_size);

    std::vector<std::thread> threads;
    threads.reserve(concurrent_threads);
    for (size_t thread_id = 0; thread_id < concurrent_threads; ++thread_id) {
	threads.emplace_back([thread_id]() {
	    for (size_t i = 0; i < stress_iterations; ++i) {
		const size_t size = 1 + ((thread_id + i) % large_allocation_size);
		void *allocation = bu_heap_get(size);
		std::memset(allocation, 0xa5, size);
		bu_heap_put(allocation, size);
		if (thread_id == 0 && i % concurrent_allocations == 0)
		    bu_heap_put(nullptr, 0);
	    }
	});
    }
    join_threads(threads);

    return 0;
}

} // namespace


int
main(int argc, char *argv[])
{
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc != 1) {
	std::fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    if (test_allocation(small_allocation_size))
	return 1;
    if (test_allocation(large_allocation_size))
	return 1;
    if (test_small_reuse())
	return 1;
    if (test_cross_thread_release())
	return 1;
    if (test_thread_oversubscription())
	return 1;
    if (test_concurrent_compaction())
	return 1;

    return 0;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
