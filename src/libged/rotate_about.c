/*                         R O T A T E _ A B O U T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/rotate_about.c
 *
 * The rotate_about command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_rotate_about(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[e|k|m|v]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get "rotate about" point */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%c", gedp->ged_gvp->gv_rotate_about);
	return GED_OK;
    }

    /* Set rotate_about */
    if (argc == 2 && argv[1][1] == '\0') {
	switch (argv[1][0]) {
	    case 'e':
	    case 'k':
	    case 'm':
	    case 'v':
		gedp->ged_gvp->gv_rotate_about = argv[1][0];
		return GED_OK;
	}
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
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
