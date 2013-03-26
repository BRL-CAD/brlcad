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

/** @file affinity.c
 *
 * Contains utility to set affinity mask of a thread to the CPU set it
 * is currently running on.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif

/*
 * bu_set_affinity
 * 
 * Set affinity mask of current thread to the CPU set it is currently 
 * running on. If it is not running on any CPUs in the set, it is
 * migrated to CPU 0 by default.
 * 
 * Return: 0 on Suceess
 *         -1 on Failure
 * 
 */
int
bu_set_affinity(void)
{
#if defined(_GNU_SOURCE) && defined(HAVE_PTHREAD_H)
	
    int cpulim = bu_avail_cpus();    /* Max number of CPUs available for the process */
    int status;                      /* Status of thread setting/getting */
    int cpu = 0;                     /* Default CPU number */
    int j;                           /* Variable for iteration. */

    cpu_set_t cpuset;                          /* CPU set structure. Defined in sched.h */
    pthread_t curr_thread = pthread_self();	   /* Get current thread */

    CPU_ZERO(&cpuset);

    for(j = 0; j < cpulim; j++) {    /* Set affinity mask to include CPUs 0 to max available CPU */
	CPU_SET(j, &cpuset);
    }
    
    status = pthread_getaffinity_np(curr_thread, sizeof(cpu_set_t), &cpuset);    /* Check current affinity mask assigned to thread */

    if(status != 0) {    /* Error in getting affinity mask */
	return -1;
    }
	
    for(j = 0; j < CPU_SETSIZE; j++) {    /* Check which set has been returned by pthread_get_affinity */
	if(CPU_ISSET(j, &cpuset)) {
	cpu = j;
	break;	/* Break loop since CPU affinity mask has been found */
	}
    }

    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);        /* Clear CPU set and assign CPUs */

    status = pthread_setaffinity_np(curr_thread, sizeof(cpu_set_t), &cpuset);        /* Set affinity mask of the current */
                                                                                     /* thread to CPU set pointed by cpuset */
 
    if(status != 0) {    /* Error in setting affinity mask */
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
