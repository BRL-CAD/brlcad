/*                    P A R A L L E L . C
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
/** @file bu_semaphore.c
 *
 * Tests libbu parallel processing.
 *
 */

#include "common.h"

#include "bu.h"

#include <string.h>


struct parallel_data {
    size_t iterations;
    void (*call)(int, void*);
};

/* intentionally not in struct so we can test the data arg,
 * intentionally per-cpu so we can avoid calling
 * bu_semaphore_acquire().
 */
size_t counter[MAX_PSW] = {0};


static void
callback(int cpu, void *d)
{
    size_t i;
    struct parallel_data *data = (struct parallel_data *)d;
    size_t iterations = 0;

    /* bu_log("I'm child %d (id=%d)\n", cpu, bu_parallel_id()); */

    if (data)
	iterations = data->iterations;

    for (i=0; i < iterations; i++) {
	counter[cpu] += 1;
    }

    return;
}


static void
recursive_callback(int UNUSED(cpu), void *d)
{
    struct parallel_data *parent = (struct parallel_data *)d;
    struct parallel_data data;
    data.iterations = parent->iterations;
    data.call = NULL;

    /* bu_log("I'm parent %d (id=%d)\n", cpu, bu_parallel_id()); */

    bu_parallel(parent->call, 0, &data);
}


static size_t
tally(size_t ncpu)
{
    size_t total = 0;
    size_t i;

    for (i = 0; i < ncpu; i++) {
	total += counter[i];
    }

    return total;
}


int
main(int argc, char *argv[])
{
    const char * const USAGE = "Usage: %s [-P ncpu]\n";

    int c;
    size_t ncpu = bu_avail_cpus();
    unsigned long ncpu_opt;
    struct parallel_data data;

    bu_setprogname(argv[0]);

    while ((c = bu_getopt(argc, argv, "P:!:")) != -1) {
	switch (c) {
	    case 'P':
		ncpu_opt = (size_t)strtoul(bu_optarg, NULL, 0);
		if (ncpu_opt > 0 && ncpu_opt < MAX_PSW)
		    ncpu = ncpu_opt;
		break;
	    case '!':
		sscanf(bu_optarg, "%x", (unsigned int *)&bu_debug);
		break;
	    default:
		bu_exit(1, USAGE, argv[0]);
	}
    }

    /* test calling without a hook function */
    bu_parallel(NULL, ncpu, NULL);
    if (tally(ncpu) != 0) {
	bu_log("bu_parallel zero callback [FAIL]\n");
	return 1;
    }
    bu_log("bu_parallel zero callback [PASS]\n");

    /* test calling a simple hook function */
    memset(counter, 0, sizeof(counter));
    bu_parallel(callback, ncpu, NULL);
    if (tally(ncpu) != 0) {
	bu_log("bu_parallel simple callback [FAIL]\n");
	return 1;
    }
    bu_log("bu_parallel simple callback [PASS]\n");

    /* test calling a simple hook function with data */
    memset(counter, 0, sizeof(counter));
    data.iterations = 0;
    bu_parallel(callback, ncpu, &data);
    if (tally(ncpu) != 0) {
	bu_log("bu_parallel simple callback with data, no iterations [FAIL]\n");
	return 1;
    }
    bu_log("bu_parallel simple callback with data, no iterations [PASS]\n");

    /* test calling a simple hook function with data, minimal potential for collisions */
    memset(counter, 0, sizeof(counter));
    data.iterations = 10;
    bu_parallel(callback, ncpu, &data);
    if (tally(MAX_PSW) != ncpu*data.iterations) {
	bu_log("bu_parallel simple callback with data, few iterations [FAIL] (got %zd, expected %zd)\n", tally(ncpu), ncpu*data.iterations);
	return 1;
    }
    bu_log("bu_parallel simple callback with data, few iterations [PASS]\n");

    /* test calling a simple hook function again with data, but lots of collision potential */
    memset(counter, 0, sizeof(counter));
    data.iterations = 1000000;
    bu_parallel(callback, ncpu, &data);
    if (tally(MAX_PSW) != ncpu*data.iterations) {
	bu_log("bu_parallel simple callback with data, many iterations [FAIL] (got %zd, expected %zd)\n", tally(ncpu), ncpu*data.iterations);
	return 1;
    }
    bu_log("bu_parallel simple callback with data, many iterations [PASS]\n");

    /* test calling a simple hook function with data, zero cpus, potential for collisions */
    memset(counter, 0, sizeof(counter));
    data.iterations = 1000000;
    bu_parallel(callback, 0, &data);
    if (tally(MAX_PSW) != bu_avail_cpus()*data.iterations) {
	bu_log("bu_parallel simple callback with data, many iterations [FAIL] (got %zd, expected %zd)\n", tally(MAX_PSW), bu_avail_cpus()*data.iterations);
	return 1;
    }
    bu_log("bu_parallel simple callback with data, many iterations [PASS]\n");

    /* test calling a recursive hook function without data */
    memset(counter, 0, sizeof(counter));
    data.iterations = 10;
    data.call = &callback;
    bu_parallel(recursive_callback, ncpu, &data);
    if (tally(MAX_PSW) != ncpu*ncpu*data.iterations) {
	bu_log("bu_parallel recursive callback, few iterations [FAIL] (got %zd, expected %zd)\n", tally(MAX_PSW), ncpu*ncpu*data.iterations);
	return 1;
    }
    bu_log("bu_parallel recursive callback, few iterations [PASS]\n");

    /* test calling a recursive hook function without data, more collision potential */
    memset(counter, 0, sizeof(counter));
    data.iterations = 1000000;
    data.call = &callback;
    bu_parallel(recursive_callback, ncpu, &data);
    if (tally(MAX_PSW) != ncpu*ncpu*data.iterations) {
	bu_log("bu_parallel recursive callback, many iterations [FAIL] (got %zd, expected %zd)\n", tally(MAX_PSW), ncpu*ncpu*data.iterations);
	return 1;
    }
    bu_log("bu_parallel recursive callback, many iterations [PASS]\n");

    return 0;
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
