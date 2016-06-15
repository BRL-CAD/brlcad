/*                        F B C L E A R . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file libged/fbclear.c
 *
 * Clear a framebuffer.
 *
 */

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_WINSOCK_H
#  include <winsock.h>
#endif
#include "bio.h"

#include "fb.h"
#include "ged.h"


#define FB_CONSTRAIN(_v, _a, _b) \
    (((_v) > (_a)) ? ((_v) < (_b) ? (_v) : (_b)) : (_a))

int
ged_fbclear(struct ged *gedp, int argc, const char *argv[])
{
    static const char usage[] = "\nUsage: fbclear [rgb]";

    int ret;
    unsigned char clearColor[3] = {0.0, 0.0 ,0.0};

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_FBSERV(gedp, GED_ERROR);
    GED_CHECK_FBSERV_FBP(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 2) {
	int r, g, b;

	if (sscanf(argv[1], "%d %d %d", &r, &g, &b) != 3) {
	    bu_log("fb_clear: bad color spec - %s", argv[1]);
	    return GED_ERROR;
	}

	clearColor[RED] = FB_CONSTRAIN(r, 0, 255);
	clearColor[GRN] = FB_CONSTRAIN(g, 0, 255);
	clearColor[BLU] = FB_CONSTRAIN(b, 0, 255);

    } else if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    ret = fb_clear(gedp->ged_fbsp->fbs_fbp, clearColor);

    if (ret)
	return GED_ERROR;

    return GED_OK;
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
