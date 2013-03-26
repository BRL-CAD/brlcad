/*                         A F F I N I T Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2012 United States Government as represented by
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

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif


int
bu_set_affinity(void)
{
#if defined(_GNU_SOURCE) && defined(HAVE_PTHREAD_H)

    int cpulim = bu_avail_cpus();    /* Max number of CPUs available for the process */
    int status;                      /* Status of thread setting/getting */
    int cpu = 0;                     /* Default CPU number */
    int j;                           /* Variable for iteration. */

    cpu_set_t cpuset;				/* CPU set structure. Defined in sched.h */
    pthread_t curr_thread = pthread_self();	/* Get current thread */

    CPU_ZERO(&cpuset);

    for(j = 0; j < cpulim; j++) {
	/* Set affinity mask to include CPUs 0 to max available CPU */
	CPU_SET(j, &cpuset);
    }

    /* Check current affinity mask assigned to thread */
    status = pthread_getaffinity_np(curr_thread, sizeof(cpu_set_t), &cpuset);

    if(status != 0) {
	/* Error in getting affinity mask */
	return -1;
    }

    for(j = 0; j < CPU_SETSIZE; j++) {
	/* Check which set has been returned by pthread_get_affinity */
	if(CPU_ISSET(j, &cpuset)) {
	    /* found affinity mask */
	    cpu = j;
	    break;
	}
    }

    /* Clear CPU set and assign CPUs */
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    /* set affinity mask of current thread */
    status = pthread_setaffinity_np(curr_thread, sizeof(cpu_set_t), &cpuset);

    if(status != 0) {
	/* Error in setting affinity mask */
	return -1;
    }

#endif

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
