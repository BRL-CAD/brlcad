/*                         W H O . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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


size_t
ged_who_argc(struct ged *gedp)
{
    struct display_list *gdlp = NULL;
    size_t visibleCount = 0;

    if (!gedp || !gedp->ged_gdp || !gedp->ged_gdp->gd_headDisplay)
	return 0;

    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
	visibleCount++;
    }

    return visibleCount;
}


/**
 * Build a command line vector of the tops of all objects in view.
 *
 * Returns the number of items displayed.
 *
 * FIXME: crazy inefficient for massive object lists.  needs to work
 * with preallocated memory.
 */
int
ged_who_argv(struct ged *gedp, char **start, const char **end)
{
    struct display_list *gdlp;
    char **vp = start;

    if (!gedp || !gedp->ged_gdp || !gedp->ged_gdp->gd_headDisplay)
	return 0;

    if (UNLIKELY(!start || !end)) {
	bu_vls_printf(gedp->ged_result_str, "INTERNAL ERROR: ged_who_argv() called with NULL args\n");
	return 0;
    }

    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
	if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	if ((vp != NULL) && ((const char **)vp < end)) {
	    *vp++ = bu_strdup(bu_vls_addr(&gdlp->dl_path));
	} else {
	    bu_vls_printf(gedp->ged_result_str, "INTERNAL ERROR: ged_who_argv() ran out of space at %s\n", ((struct directory *)gdlp->dl_dp)->d_namep);
	    break;
	}
    }

    if ((vp != NULL) && ((const char **)vp < end)) {
	*vp = (char *) 0;
    }

    return vp-start;
}


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
    struct display_list *gdlp;
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

    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
	if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR) {
	    if (skip_phony) continue;
	} else {
	    if (skip_real) continue;
	}

	bu_vls_printf(gedp->ged_result_str, "%s ", bu_vls_addr(&gdlp->dl_path));
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
