/*          P A R A L L E L _ C P P 1 1 T H R E A D . C P P
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

#include <thread>
#include <vector>
#include <stddef.h>


extern "C" void
parallel_cpp11thread(void (*func)(int, void *), size_t ncpu, void *arg)
{
    if (!func)
	return;

    if (!ncpu) {
	ncpu = std::thread::hardware_concurrency();

	/* If ncpu is 0 now, then the hardware either doesn't support
	 * threads, or they aren't known to the implementation.
	 * Revert to single threading.
	 */
	if (!ncpu) {
	    func(0, arg);
	    return;
	}
    }

    std::vector<std::thread> threads;
    size_t created = 0;
    try {
	threads.reserve(ncpu);
	/* Create and run threads. */
	for (size_t i = 0; i < ncpu; ++i) {
	    threads.emplace_back(func, (int)i, arg);
	    created++;
	}
    } catch (...) {
	/* If thread creation fails, join any threads that were already spawned
	 * to prevent std::terminate() in std::thread destructor.
	 */
	for (size_t i = 0; i < threads.size(); ++i) {
	    if (threads[i].joinable())
		threads[i].join();
	}
	/* Fallback: run remaining tasks sequentially */
	for (size_t i = created; i < ncpu; ++i) {
	    func((int)i, arg);
	}
	return;
    }

    /* Wait for the parallel task to complete. */
    for (size_t i = 0; i < threads.size(); ++i) {
	if (threads[i].joinable())
	    threads[i].join();
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
