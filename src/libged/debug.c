/*                         D E B U G B U . C
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
/** @file libged/debug.c
 *
 * The debug command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/debug.h"
#include "rt/debug.h"
#include "nmg/debug.h"
#include "optical/debug.h"
#include "./ged_private.h"

#include "./debug_vars.c"

static void
print_all_lib_flags(struct ged *gedp, int lcnt, int max_strlen)
{
    int ecnt = 0;
    long eval = -1;
    while (eval != 0) {
	eval = dbg_lib_entries[lcnt][ecnt].val;
	if (eval > 0) {
	    bu_vls_printf(gedp->ged_result_str, "   %*s (0x%08lx): %s\n", max_strlen, dbg_lib_entries[lcnt][ecnt].key, dbg_lib_entries[lcnt][ecnt].val, dbg_lib_entries[lcnt][ecnt].info);
	}
	ecnt++;
    }
}


static void
print_set_lib_flags(struct ged *gedp, int lcnt, int max_strlen)
{
    int ecnt = 0;
    while (dbg_lib_entries[lcnt][ecnt].val) {
	unsigned int *cvect = dbg_vars[lcnt];
	if (*cvect & dbg_lib_entries[lcnt][ecnt].val) {
	    bu_vls_printf(gedp->ged_result_str, "   %*s (0x%08lx): %s\n", max_strlen, dbg_lib_entries[lcnt][ecnt].key, dbg_lib_entries[lcnt][ecnt].val, dbg_lib_entries[lcnt][ecnt].info);
	}
	ecnt++;
    }
    bu_vls_printf(gedp->ged_result_str, "\n");
}

static void
print_all_set_lib_flags(struct ged *gedp, int max_strlen)
{
    int lcnt = 0;
    while (dbg_lib_entries[lcnt]) {
	int ecnt = 0;
	int have_active = 0;
	while (dbg_lib_entries[lcnt][ecnt].val) {
	    unsigned int *cvect = dbg_vars[lcnt];
	    if (*cvect & dbg_lib_entries[lcnt][ecnt].val) {
		have_active = 1;
		break;
	    }
	    ecnt++;
	}

	if (!have_active) {
	    lcnt++;
	    continue;
	}

	bu_vls_printf(gedp->ged_result_str, "%s flags:\n", dbg_libs[lcnt]);
	print_set_lib_flags(gedp, lcnt, max_strlen);

	lcnt++;
    }
}

int
ged_debug(struct ged *gedp, int argc, const char **argv)
{
    int lcnt = 0;
    int max_strlen = -1;
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 3) {
	return GED_ERROR;
    }

    lcnt = 0;
    while (dbg_lib_entries[lcnt]) {
	int ecnt = 0;
	while (dbg_lib_entries[lcnt][ecnt].val != 0) {
	    int slen = strlen(dbg_lib_entries[lcnt][ecnt].key);
	    max_strlen = (slen > max_strlen) ? slen : max_strlen;
	    ecnt++;
	}
	lcnt++;
    }

    if (argc > 1) {

	if (BU_STR_EQUAL(argv[1], "-h")) {
	    bu_vls_printf(gedp->ged_result_str, "debug [-h] [-l [lib]] [lib] [flag]\n");
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "-l") && argc == 2) {
	    lcnt = 0;
	    while (dbg_lib_entries[lcnt]) {
		bu_vls_printf(gedp->ged_result_str, "%s\n", dbg_libs[lcnt]);
		lcnt++;
	    }
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "-l") && argc == 3) {
	    lcnt = 0;
	    while (dbg_lib_entries[lcnt]) {
		if (argc > 2 && !BU_STR_EQUAL(argv[2], "*") && !(BU_STR_EQUIV(argv[2], dbg_libs[lcnt]))) {
		    lcnt++;
		    continue;
		}
		bu_vls_printf(gedp->ged_result_str, "%s flags:\n", dbg_libs[lcnt]);
		print_all_lib_flags(gedp, lcnt, max_strlen);
		bu_vls_printf(gedp->ged_result_str, "\n");
		lcnt++;
	    }
	    return GED_OK;
	}

	lcnt = 0;
	while (dbg_lib_entries[lcnt]) {
	    if (BU_STR_EQUIV(argv[1], dbg_libs[lcnt])) {
		/* If we have a specified flag, toggle it.  Else, just print
		 * what is active */
		if (argc > 2) {
		    int found = 0;
		    int ecnt = 0;
		    while (dbg_lib_entries[lcnt][ecnt].val) {
			if (BU_STR_EQUIV(argv[2], dbg_lib_entries[lcnt][ecnt].key)) {
			    unsigned int *cvect = dbg_vars[lcnt];
			    if (*cvect & dbg_lib_entries[lcnt][ecnt].val) {
				*cvect = *cvect & ~(dbg_lib_entries[lcnt][ecnt].val);
			    } else {
				*cvect |= dbg_lib_entries[lcnt][ecnt].val;
			    }
			    found = 1;
			    break;
			}
			ecnt++;
		    }
		    if (!found) {
			bu_vls_printf(gedp->ged_result_str, "invalid %s paramter: %s\n", dbg_libs[lcnt], argv[2]);
			return GED_ERROR;
		    }
		}
		print_set_lib_flags(gedp, lcnt, max_strlen);
		return GED_OK;
	    }
	    lcnt++;
	}

	bu_vls_printf(gedp->ged_result_str, "invalid input: %s\n", argv[1]);
	return GED_ERROR;
    }

    lcnt = 0;
    print_all_set_lib_flags(gedp, max_strlen);

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
