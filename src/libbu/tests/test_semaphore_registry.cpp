/*           T E S T _ S E M A P H O R E _ R E G I S T R Y . C P P
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

#include "common.h"

#include <atomic>
#include <cstdio>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "bu/parallel.h"


int
main()
{
    const size_t thread_count = 16;
    const size_t reinit_rounds = 32;
    std::vector<int> same_ids(thread_count, 0);
    std::vector<int> distinct_ids(thread_count, 0);
    std::vector<std::thread> threads;

    for (size_t i = 0; i < thread_count; i++) {
	threads.emplace_back([&, i]() {
	    std::string name("temporary-shared-registration-name");
	    same_ids[i] = bu_semaphore_register(name.c_str());
	});
    }
    for (std::thread &thread : threads)
	thread.join();
    threads.clear();

    for (size_t i = 0; i < thread_count; i++) {
	if (same_ids[i] != same_ids[0] || same_ids[i] < BU_SEM_DYNAMIC_BASE) {
	    std::fprintf(stderr, "concurrent same-name registration failed\n");
	    return 1;
	}
    }

    for (size_t i = 0; i < thread_count; i++) {
	threads.emplace_back([&, i]() {
	    std::string name = "temporary-distinct-registration-name-" + std::to_string(i);
	    distinct_ids[i] = bu_semaphore_register(name.c_str());
	});
    }
    for (std::thread &thread : threads)
	thread.join();

    std::set<int> unique_ids(distinct_ids.begin(), distinct_ids.end());
    if (unique_ids.size() != thread_count || *unique_ids.begin() < BU_SEM_DYNAMIC_BASE) {
	std::fprintf(stderr, "concurrent distinct-name registration failed\n");
	return 1;
    }

    /* The original caller strings no longer exist; the registry owns names. */
    if (bu_semaphore_register("temporary-shared-registration-name") != same_ids[0]) {
	std::fprintf(stderr, "registry did not preserve its copy of a name\n");
	return 1;
    }

    /*
     * Recreate the native semaphore range from multiple threads.  This
     * exercises publication of the initialized range independently of the
     * registry lock and is intended to be run under ThreadSanitizer as well
     * as in the normal test suite.
     */
    std::atomic<size_t> ready(0);
    std::atomic<bool> start(false);
    for (size_t round = 0; round < reinit_rounds; round++) {
	bu_semaphore_free();
	ready.store(0, std::memory_order_relaxed);
	start.store(false, std::memory_order_relaxed);
	threads.clear();
	for (size_t i = 0; i < thread_count; i++) {
	    threads.emplace_back([&, i]() {
		ready.fetch_add(1, std::memory_order_release);
		while (!start.load(std::memory_order_acquire))
		    std::this_thread::yield();
		bu_semaphore_acquire(distinct_ids[i]);
		bu_semaphore_release(distinct_ids[i]);
	    });
	}
	while (ready.load(std::memory_order_acquire) != thread_count)
	    std::this_thread::yield();
	start.store(true, std::memory_order_release);
	for (std::thread &thread : threads)
	    thread.join();
    }

    if (bu_semaphore_register("temporary-shared-registration-name") != same_ids[0]) {
	std::fprintf(stderr, "registry mapping changed after semaphore free\n");
	return 1;
    }

    for (size_t i = 0; i < thread_count; i++) {
	std::string name = "temporary-distinct-registration-name-" + std::to_string(i);
	if (bu_semaphore_register(name.c_str()) != distinct_ids[i]) {
	    std::fprintf(stderr, "distinct mapping changed after semaphore free\n");
	    return 1;
	}
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
