/*                         W H O . C
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
/** @file libged/who.c
 *
 * The who command.
 *
 */

#include "ged.h"
#include "solid.h"


/*
 * List the objects currently prepped for drawing
 *
 * Usage:
 * who [r(eal)|p(hony)|b(oth)]
 *
 */
int
ged_who(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    int skip_real, skip_phony;
    static const char *usage = "[r(eal)|p(hony)|b(oth)]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (2 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    skip_real = 0;
    skip_phony = 1;
    if (argc == 2) {
	switch (argv[1][0]) {
	    case 'b':
		skip_real = 0;
		skip_phony = 0;
		break;
	    case 'p':
		skip_real = 1;
		skip_phony = 0;
		break;
	    case 'r':
		skip_real = 0;
		skip_phony = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "ged_who: argument not understood\n");
		return GED_ERROR;
	}
    }

    for (BU_LIST_FOR(gdlp, ged_display_list, &gedp->ged_gdp->gd_headDisplay)) {
	if (gdlp->gdl_dp->d_addr == RT_DIR_PHONY_ADDR) {
	    if (skip_phony) continue;
	} else {
	    if (skip_real) continue;
	}

	bu_vls_printf(gedp->ged_result_str, "%s ", bu_vls_addr(&gdlp->gdl_path));
    }

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
