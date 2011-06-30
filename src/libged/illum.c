/*                         I L L U M . C
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
/** @file libged/illum.c
 *
 * The illum command.
 *
 */

#include <string.h>

#include "ged.h"
#include "solid.h"


/*
 * Illuminate/highlight database object
 *
 * Usage:
 * illum [-n] obj
 *
 */
int
ged_illum(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    int found = 0;
    int illum = 1;
    static const char *usage = "[-n] obj";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc == 3) {
	if (argv[1][0] == '-' && argv[1][1] == 'n')
	    illum = 0;
	else
	    goto bad;

	--argc;
	++argv;
    }

    if (argc != 2)
	goto bad;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    size_t i;

	    for (i = 0; i < sp->s_fullpath.fp_len; ++i) {
		if (*argv[1] == *DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_namep &&
		    BU_STR_EQUAL(argv[1], DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_namep)) {
		    found = 1;
		    if (illum)
			sp->s_iflag = UP;
		    else
			sp->s_iflag = DOWN;
		}
	    }
	}

	gdlp = next_gdlp;
    }


    if (!found) {
	bu_vls_printf(gedp->ged_result_str, "illum: %s not found", argv[1]);
	return GED_ERROR;
    }

    return GED_OK;

bad:
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
