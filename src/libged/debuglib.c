/*                         D E B U G L I B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file debuglib.c
 *
 * The debuglib command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_debuglib(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[hex_code]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (argc > 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* get librt's debug bit vector */
    if (argc == 1) {
	bu_vls_printb(&gedp->ged_result_str, "Possible flags", 0xffffffffL, DEBUG_FORMAT);
	bu_vls_printf(&gedp->ged_result_str, "\n");
    } else {
	/* set librt's debug bit vector */
	if (sscanf(argv[1], "%x", (unsigned int *)&rt_g.debug) != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

	if (RT_G_DEBUG) bu_debug |= BU_DEBUG_COREDUMP;
    }

    bu_vls_printb(&gedp->ged_result_str, "librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT);
    bu_vls_printf(&gedp->ged_result_str, "\n");

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
