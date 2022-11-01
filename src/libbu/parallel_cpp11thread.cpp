/*          P A R A L L E L _ C P P 1 1 T H R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
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
    std::vector<std::thread> threads;

    if (!ncpu) {
	ncpu = std::thread::hardware_concurrency();

	/* If ncpu is 0 now, then the hardware either doesn't support
	 * threads, or they aren't known to the implementation.
	 * Revert to single threading.
	 */
	if (!ncpu)
	    return func((int)ncpu, arg);
    }

    /* Create and run threads. */
    for (size_t i = 0; i < ncpu; ++i)
	threads.emplace_back(func, i, arg);

    /* Wait for the parallel task to complete. */
    for (size_t i = 0; i < threads.size(); ++i)
	threads[i].join();
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

