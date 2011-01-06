/*                           T I M E R . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * Copyright (c) Tim Riker
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

#include <time.h>
#if !defined(_WIN32)
#  ifdef HAVE_SYS_TIME_H
#     include <sys/time.h>
#  endif
#  ifdef HAVE_SYS_TYPES_H
#     include <sys/types.h>
#  endif
#  ifdef HAVE_SCHED_H
#    include <sched.h>
#  endif
#else /* !defined(_WIN32) */
#  include <windows.h>
#  include <mmsystem.h>
#endif /* !defined(_WIN32) */

#include "bu.h"

int64_t bu_gettime(void)
{
#if !defined(_WIN32)
    struct timeval nowTime;

    gettimeofday(&nowTime, NULL);
    return ((int64_t)nowTime.tv_sec * (int64_t)1000000
	    + (int64_t)nowTime.tv_usec);
#else /* !defined(_WIN32) */
    LARGE_INTEGER count;
    static LARGE_INTEGER freq = 0;
    int rval;

    if(freq == 0)
	if(QueryPerformanceFrequency(&freq) == 0) {
	    bu_log("QueryPerformanceFrequency failed\n");
	    return -1;
	}

    if(QueryPerformanceCounter(&freq) == 0) {
	bu_log("QueryPerformanceCounter failed\n");
	return -1;
    }

    return 1e6*count/freq;

#endif /* !defined(_WIN32) */

    bu_log("This should never happen.\n");
    return -1;
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
