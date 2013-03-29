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

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE /* must come before common.h */
#endif

#include "common.h"

#ifdef HAVE_SCHED_H
#  include <sched.h>
#endif

#ifdef HAVE_SYS_CPUSET_H
#  include <sys/cpuset.h>
#endif

#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif

#ifdef HAVE_PTHREAD_NP_H
#  include <pthread_np.h>
#endif

#include "bu.h"


int
parallel_set_affinity(void)
{
#if defined(HAVE_PTHREAD_H)

    pthread_t curr_thread = pthread_self();	/* Get current thread */
    int cpulim = bu_avail_cpus();		/* Max number of CPUs available for the process */
    int status;					/* Status of thread setting/getting */
    int cpu = 0;				/* Default CPU number */
    int j;					/* Variable for iteration. */

#ifdef HAVE_CPU_SET_T
    cpu_set_t set_of_cpus;
#else
    cpuset_t set_of_cpus;
#endif


    CPU_ZERO(&set_of_cpus);

    for(j = 0; j < cpulim; j++) {
	/* Set affinity mask to include CPUs 0 to max available CPU */
	CPU_SET(j, &set_of_cpus);
    }

    /* Check current affinity mask assigned to thread */
    status = pthread_getaffinity_np(curr_thread, sizeof(set_of_cpus), &set_of_cpus);

    if(status != 0) {
	/* Error in getting affinity mask */
	return -1;
    }

    for(j = 0; j < CPU_SETSIZE; j++) {
	/* Check which set has been returned by pthread_get_affinity */
	if(CPU_ISSET(j, &set_of_cpus)) {
	    /* found affinity mask */
	    cpu = j;
	    break;
	}
    }

    /* Clear CPU set and assign CPUs */
    CPU_ZERO(&set_of_cpus);
    CPU_SET(cpu, &set_of_cpus);

    /* set affinity mask of current thread */
    status = pthread_setaffinity_np(curr_thread, sizeof(set_of_cpus), &set_of_cpus);

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
