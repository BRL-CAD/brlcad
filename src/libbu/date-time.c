/*                        D A T E - T I M E . C
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

#include "common.h"

#include <time.h>
#include <string.h>

#include "bu/time.h"
#include "bu/parallel.h"
#include "bu/vls.h"

void
bu_utctime(struct bu_vls *vls_gmtime, const int64_t time_val)
{
    static const char *nulltime = "0000-00-00T00:00:00Z";
    struct tm loctime;
    struct tm* retval;
    time_t some_time;
    int fail = 0;

    if (!vls_gmtime)
	return;

    BU_CK_VLS(vls_gmtime);

    some_time = (time_t)time_val;
    if (some_time == (time_t)(-1)) {
	/* time error: but set something, an invalid "NULL" time. */
	bu_vls_sprintf(vls_gmtime, nulltime);
	return;
    }

    memset(&loctime, 0, sizeof(loctime));

    bu_semaphore_acquire(BU_SEM_DATETIME);
    retval = gmtime(&some_time);
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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
