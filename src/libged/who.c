/*                         W H O . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file who.c
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
 *        who [r(eal)|p(hony)|b(oth)]
 *
 */
int
ged_who(struct ged *gedp, int argc, const char *argv[])
{
    register struct solid *sp;
    int skip_real, skip_phony;
    static const char *usage = "[r(eal)|p(hony)|b(oth)]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc < 1 || 2 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
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
		bu_vls_printf(&gedp->ged_result_str, "ged_who: argument not understood\n", (char *)NULL);
		return GED_ERROR;
	}
    }


    /* Find all unique top-level entries.
     *  Mark ones already done with s_flag == UP
     */
    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid)
	sp->s_flag = DOWN;
    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid) {
	register struct solid *forw;	/* XXX */

	if (sp->s_flag == UP)
	    continue;
	if (FIRST_SOLID(sp)->d_addr == RT_DIR_PHONY_ADDR) {
	    if (skip_phony) continue;
	} else {
	    if (skip_real) continue;
	}
	bu_vls_printf(&gedp->ged_result_str, "%s ", FIRST_SOLID(sp)->d_namep);
	sp->s_flag = UP;
	FOR_REST_OF_SOLIDS(forw, sp, &gedp->ged_gdp->gd_headSolid) {
	    if (FIRST_SOLID(forw) == FIRST_SOLID(sp))
		forw->s_flag = UP;
	}
    }
    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid)
	sp->s_flag = DOWN;

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
