/*                         D A T E T I M E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2016 United States Government as represented by
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

#include <time.h>
#include <string.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SCHED_H
#  include <sched.h>
#endif

#include "bio.h"

#include "bu/log.h"
#include "bu/time.h"
#include "bu/parallel.h"
#include "bu/vls.h"

#include "y2038/time64.h"


void
bu_utctime(struct bu_vls *vls_gmtime, const int64_t time_val)
{
    static const char *nulltime = "0000-00-00T00:00:00Z";
    struct tm loctime;
    struct tm* retval;
    Time64_T some_time;
    int fail = 0;

    if (!vls_gmtime)
	return;

    BU_CK_VLS(vls_gmtime);

    some_time = (Time64_T)time_val;
    if (some_time == (Time64_T)(-1)) {
	/* time error: but set something, an invalid "NULL" time. */
	bu_vls_sprintf(vls_gmtime, nulltime);
	return;
    }

    memset(&loctime, 0, sizeof(loctime));

    bu_semaphore_acquire(BU_SEM_DATETIME);
    retval = gmtime64(&some_time);
    if (retval)
	loctime = *retval; /* struct copy */
    else
	fail = 1;
    bu_semaphore_release(BU_SEM_DATETIME);

    if (fail) {
	/* time error: but set something, an invalid "NULL" time. */
	bu_vls_sprintf(vls_gmtime, nulltime);
	return;
    }

    /* put the UTC time in the desired ISO format: "yyyy-mm-ddThh:mm:ssZ" */
    bu_vls_sprintf(vls_gmtime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
		   loctime.tm_year + 1900,
		   loctime.tm_mon + 1,
		   loctime.tm_mday,
		   loctime.tm_hour,
		   loctime.tm_min,
		   loctime.tm_sec);
}


int64_t
bu_gettime(void)
{
#ifdef HAVE_SYS_TIME_H

    struct timeval nowTime;

    gettimeofday(&nowTime, NULL);
    return ((int64_t)nowTime.tv_sec * (int64_t)1000000
	    + (int64_t)nowTime.tv_usec);

#else /* HAVE_SYS_TIME_H */
#  ifdef HAVE_WINDOWS_H

    LARGE_INTEGER count;
	static LARGE_INTEGER freq = {0};

    if (freq.QuadPart == 0)
	if (QueryPerformanceFrequency(&freq) == 0) {
	    bu_log("QueryPerformanceFrequency failed\n");
	    return -1;
	}

    if (QueryPerformanceCounter(&count) == 0) {
	bu_log("QueryPerformanceCounter failed\n");
	return -1;
    }

    return 1e6*count.QuadPart/freq.QuadPart;

#  else /* HAVE_WINDOWS_H */
#    warning "bu_gettime() implementation missing for this machine type"
    bu_log("WARNING, no bu_gettime implementation for this machine type.\n");
    return -1;

#  endif /* HAVE_WINDOWS_H */
#endif /* HAVE_SYS_TIME_H */
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
