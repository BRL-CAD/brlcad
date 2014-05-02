/*                         A F F I N I T Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

#ifdef HAVE_MACH_THREAD_POLICY_H
#  include <mach/thread_policy.h>
#  include <mach/mach.h>
#endif

#ifdef HAVE_WINDOWS_H
#  include <windows.h>
#endif

#include "bu/parallel.h"

int
parallel_set_affinity(int cpu)
{
#if defined(HAVE_PTHREAD_H) && defined(CPU_ZERO)

    /* Linux and BSD pthread affinity */

#ifdef HAVE_CPU_SET_T
    cpu_set_t set_of_cpus; /* bsd */
#else
    cpuset_t set_of_cpus; /* linux */
#endif
    int ret;

    /* Clear CPU set and assign our number */
    CPU_ZERO(&set_of_cpus);
    CPU_SET(cpu % bu_avail_cpus(), &set_of_cpus);

    /* set affinity mask of current thread */
    ret = pthread_setaffinity_np(pthread_self(), sizeof(set_of_cpus), &set_of_cpus);
    if (ret != 0) {
	/* Error in setting affinity mask */
	return -1;
    }

    return 0;

#elif defined(HAVE_MACH_THREAD_POLICY_H)

    /* Mac OS X mach thread affinity hinting.  Mach implements a CPU
     * affinity policy by default so this just sets up an additional
     * hint on how threads can be grouped/ungrouped.  Here we set all
     * threads up into their own group so threads will get their own
     * cpu and hopefully be kept in place by Mach from there.
     */

    thread_extended_policy_data_t epolicy;
    thread_affinity_policy_data_t apolicy;
    thread_t curr_thread = mach_thread_self();
    kern_return_t ret;

    /* discourage interrupting this thread */
    epolicy.timeshare = FALSE;
    ret = thread_policy_set(curr_thread, THREAD_EXTENDED_POLICY, (thread_policy_t) &epolicy, THREAD_EXTENDED_POLICY_COUNT);
    if (ret != KERN_SUCCESS)
	return -1;

    /* put each thread into a separate group */
    apolicy.affinity_tag = cpu % bu_avail_cpus();
    ret = thread_policy_set(curr_thread, THREAD_EXTENDED_POLICY, (thread_policy_t) &apolicy, THREAD_EXTENDED_POLICY_COUNT);
    if (ret != KERN_SUCCESS)
	return -1;

    return 0;

#elif defined(HAVE_WINDOWS_H)

    BOOL ret = SetThreadAffinityMask(GetCurrentThread(), 1ul << cpu % bu_avail_cpus());
    if (ret  == 0)
	return -1;

    return 0;

#else

    /* don't know how to set thread affinity on this platform */
    cpu = 0;	/* do something with cpu to avoid an unused parameter warning */
    return cpu;

#endif
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
