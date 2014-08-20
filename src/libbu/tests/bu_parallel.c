/*                  B U _ S E M A P H O R E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
    size_t iterations = 1;

    if (data)
	iterations = data->iterations;

    for (i=0; i < iterations; i++) {
	if (cpu > 0) {
	    counter[cpu] += 1;
	}
    }

    return;
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

    while ((c = bu_getopt(argc, argv, "P:")) != -1) {
	switch (c) {
	    case 'P':
		ncpu_opt = (size_t)strtoul(bu_optarg, NULL, 0);
		if (ncpu_opt > 0 && ncpu_opt < ncpu)
		    ncpu = ncpu_opt;
		break;
	    default:
		bu_exit(1, USAGE, argv[0]);
	}
    }

    /* test calling without a hook function */
    bu_parallel(NULL, ncpu, NULL);

    /* test calling a simple hook function */
    memset(counter, 0, sizeof(counter));
    bu_parallel(callback, ncpu, NULL);
    if (tally(ncpu) != ncpu-1) {
	bu_log("bu_parallel simple callback [FAIL]\n");
	return 1;
    }

    /* test calling a simple hook function with data */
    memset(counter, 0, sizeof(counter));
    data.iterations = 0;
    bu_parallel(callback, ncpu, &data);
    if (tally(ncpu) != 0) {
	bu_log("bu_parallel simple callback with data, no iterations [FAIL]\n");
	return 1;
    }

    /* test calling a simple hook function with data, minimal potential for collisions */
    memset(counter, 0, sizeof(counter));
    data.iterations = 10;
    bu_parallel(callback, ncpu, &data);
    if (tally(ncpu) != (ncpu-1)*data.iterations) {
	bu_log("bu_parallel simple callback with data, few iterations [FAIL] (got %zd, expected %zd)\n", tally(ncpu), (ncpu-1)*data.iterations);
	return 1;
    }

    /* test calling a simple hook function again with data, but lots of collision potential */
    memset(counter, 0, sizeof(counter));
    data.iterations = 1000000;
    bu_parallel(callback, ncpu, &data);
    if (tally(ncpu) != (ncpu-1)*data.iterations) {
	bu_log("bu_parallel simple callback with data, few iterations [FAIL] (got %zd, expected %zd)\n", tally(ncpu), (ncpu-1)*data.iterations);
	return 1;
    }

    /* test calling a simple hook function with data, zero cpus, potential for collisions */
    memset(counter, 0, sizeof(counter));
    data.iterations = 1000000;
    bu_parallel(callback, 0, &data);
    if (tally(MAX_PSW) != (ncpu-1)*data.iterations) {
	bu_log("bu_parallel simple callback with data, few iterations [FAIL] (got %zd, expected %zd)\n", tally(MAX_PSW), (ncpu-1)*data.iterations);
	return 1;
    }

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
