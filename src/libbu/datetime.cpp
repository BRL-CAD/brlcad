/*                       D A T E T I M E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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

#include <string.h>

#include <chrono>

#include "bio.h"

#include "bu/log.h"
#include "bu/time.h"
#include "bu/parallel.h"
#include "bu/vls.h"

#include "y2038/time64.h"

/* for strict c90 */
#ifndef HAVE_DECL_GETTIMEOFDAY
extern int gettimeofday(struct timeval *, void *);
#endif

int BU_SEM_DATETIME;


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
	bu_vls_sprintf(vls_gmtime, "%s", nulltime);
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
	bu_vls_sprintf(vls_gmtime, "%s", nulltime);
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



/* FIXME: Need to document whether this function should
 * be returning wallclock or cpu time.
 */
int64_t
bu_gettime(void)
{
    auto etime =std::chrono::high_resolution_clock::now().time_since_epoch();
    auto mtime = std::chrono::duration_cast<std::chrono::microseconds>(etime);
    int64_t msec = mtime.count();
    return msec;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
