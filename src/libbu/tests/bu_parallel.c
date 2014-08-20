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

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#  include <signal.h>
#else
#  include <windows.h>
#endif


struct parallel_data {
    size_t iterations;
};

/* intentionally not in struct so we can test the data arg */
size_t counter;


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
	    bu_semaphore_acquire(BU_SEM_THREAD);
	    counter += 1;
	    bu_semaphore_release(BU_SEM_THREAD);
	}
    }

    return;
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
    counter = 0;
    bu_parallel(callback, ncpu, NULL);
    if (counter != ncpu-1) {
	bu_log("bu_parallel simple callback [FAIL]\n");
	return 1;
    }

    /* test calling a simple hook function with data */
    counter = 0;
    data.iterations = 0;
    bu_parallel(callback, ncpu, &data);
    if (counter != 0) {
	bu_log("bu_parallel simple callback with data, no iterations [FAIL]\n");
	return 1;
    }

    /* test calling a simple hook function again with data */
    counter = 0;
    data.iterations = 10;
    bu_parallel(callback, ncpu, &data);
    if (counter != (ncpu-1)*data.iterations) {
	bu_log("bu_parallel simple callback with data, few iterations [FAIL] (got %zd, expected %zd)\n", counter, (ncpu-1)*data.iterations);
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
