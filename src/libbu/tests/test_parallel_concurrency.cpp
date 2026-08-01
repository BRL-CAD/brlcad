/*            T E S T _ P A R A L L E L _ C O N C U R R E N C Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
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
/** @file test_parallel_concurrency.cpp
 *
 * Exercise concurrent and recursive bu_parallel() bookkeeping.
 */

#include "common.h"

#include <array>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include "bu/parallel.h"


struct concurrent_state {
    std::atomic<size_t> calls{0};
    std::atomic<bool> failed{false};
};


static void
concurrent_callback(int cpu, void *data)
{
    struct concurrent_state *state = static_cast<struct concurrent_state *>(data);

    if (cpu != bu_parallel_id())
	state->failed.store(true, std::memory_order_relaxed);
    state->calls.fetch_add(1, std::memory_order_relaxed);
}


struct recursive_state {
    size_t outer_count;
    std::atomic<size_t> ready{0};
    std::atomic<bool> failed{false};
    std::array<std::atomic<bool>, MAX_PSW> active{};
};


struct single_state {
    int expected_id;
    std::atomic<bool> *failed;
};


static void
single_callback(int cpu, void *data)
{
    struct single_state *state = static_cast<struct single_state *>(data);

    if (cpu != 0 || bu_parallel_id() != state->expected_id)
	state->failed->store(true, std::memory_order_relaxed);
}


static void
reuse_probe(int cpu, void *data)
{
    struct recursive_state *state = static_cast<struct recursive_state *>(data);
    int id = bu_parallel_id();

    if (cpu != id || id < 0 || id >= MAX_PSW) {
	state->failed.store(true, std::memory_order_relaxed);
	return;
    }

    if (state->active[(size_t)id].load(std::memory_order_acquire))
	state->failed.store(true, std::memory_order_relaxed);
}


static void
recursive_callback(int cpu, void *data)
{
    struct recursive_state *state = static_cast<struct recursive_state *>(data);
    int id = bu_parallel_id();

    if (cpu != id || id < 0 || id >= MAX_PSW) {
	state->failed.store(true, std::memory_order_relaxed);
	state->ready.fetch_add(1, std::memory_order_release);
	while (state->ready.load(std::memory_order_acquire) != state->outer_count)
	    std::this_thread::yield();
	return;
    }

    if (state->active[(size_t)id].exchange(true, std::memory_order_acq_rel))
	state->failed.store(true, std::memory_order_relaxed);

    struct single_state single = {id, &state->failed};
    bu_parallel(single_callback, 1, &single);
    if (bu_parallel_id() != id)
	state->failed.store(true, std::memory_order_relaxed);

    state->ready.fetch_add(1, std::memory_order_release);
    while (state->ready.load(std::memory_order_acquire) != state->outer_count)
	std::this_thread::yield();

    /* A live outer ID must not be available to these child workers. */
    bu_parallel(reuse_probe, 2, state);
    state->active[(size_t)id].store(false, std::memory_order_release);
}


int
main()
{
    const size_t caller_count = 8;
    const size_t workers_per_caller = 2;
    std::atomic<size_t> ready{0};
    std::atomic<bool> start{false};
    struct concurrent_state concurrent;
    std::vector<std::thread> callers;

    for (size_t i = 0; i < caller_count; i++) {
	callers.emplace_back([&]() {
	    ready.fetch_add(1, std::memory_order_release);
	    while (!start.load(std::memory_order_acquire))
		std::this_thread::yield();
	    bu_parallel(concurrent_callback, workers_per_caller, &concurrent);
	});
    }
    while (ready.load(std::memory_order_acquire) != caller_count)
	std::this_thread::yield();
    start.store(true, std::memory_order_release);
    for (std::thread &caller : callers)
	caller.join();

    if (concurrent.failed.load(std::memory_order_relaxed) ||
	concurrent.calls.load(std::memory_order_relaxed) != caller_count * workers_per_caller) {
	std::fprintf(stderr, "concurrent top-level bu_parallel calls failed\n");
	return 1;
    }

    struct recursive_state recursive;
    recursive.outer_count = 4;
    for (std::atomic<bool> &active : recursive.active)
	active.store(false, std::memory_order_relaxed);
    bu_parallel(recursive_callback, recursive.outer_count, &recursive);
    if (recursive.failed.load(std::memory_order_relaxed)) {
	std::fprintf(stderr, "recursive bu_parallel reused a live worker ID\n");
	return 1;
    }

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
