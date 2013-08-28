/*                        D A T E - T I M E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2013 United States Government as represented by
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

/*
#include <stdio.h>
#include <string.h>
*/

#include <time.h>

#include "bu.h"

void
bu_gmtime(struct bu_vls *vls_gmtime)
{
    struct tm loctime;
    struct tm* retval;
    time_t curr_time = time(0);
    BU_CK_VLS(vls_gmtime);

    if (curr_time == (time_t)(-1)) {
	/* time error: but set something */
	bu_vls_sprintf(vls_gmtime, "TIME_ERROR");
	return;
    }

#ifdef HAVE_GMTIME_S
    retval = gmtime_s(&loctime, &curr_time);
#else
    retval = gmtime_r(&curr_time, &loctime);
#endif
    if (retval != &loctime) {
	/* time error: but set something */
	bu_vls_sprintf(vls_gmtime, "TIME_ERROR");
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
