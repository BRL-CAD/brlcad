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

    if (argc > 4) {
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
	    bu_vls_printf(gedp->ged_result_str, "debug [-h] [-l [lib]] [-V [lib] [val]] [[lib] [flag]]\n\n");
	    bu_vls_printf(gedp->ged_result_str, "Available libs:\n");
	    lcnt = 0;
	    while (dbg_lib_entries[lcnt]) {
		bu_vls_printf(gedp->ged_result_str, "\t%s\n", dbg_libs[lcnt]);
		lcnt++;
	    }
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "-l") && argc == 2) {
	    lcnt = 0;
	    while (dbg_lib_entries[lcnt]) {
		bu_vls_printf(gedp->ged_result_str, "%s flags:\n", dbg_libs[lcnt]);
		print_all_lib_flags(gedp, lcnt, max_strlen);
		bu_vls_printf(gedp->ged_result_str, "\n");
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


	if (BU_STR_EQUAL(argv[1], "-V")) {
	    lcnt = 0;
	    if (argc == 2) {
		/* Bare -v option - print all the hex values */
		while (dbg_lib_entries[lcnt]) {
		    unsigned int *cvect = dbg_vars[lcnt];
		    bu_vls_printf(gedp->ged_result_str, "%*s: 0x%08x\n", max_strlen, dbg_libs[lcnt], *cvect);
		    lcnt++;
		}
		return GED_OK;
	    }
	    if (argc == 3) {
		/* -v option with one arg - either printing a value for one library, or
		 * setting a hex value for all libraries.  This is usually useful as
		 * a way to clear all debugging settings by passing 0x0 */
		if (argv[2][0] == '0' && argv[2][1] == 'x') {
		    long fvall = strtol(argv[2], NULL, 0);
		    if (fvall < 0) {
			bu_vls_printf(gedp->ged_result_str, "unusable hex value %ld\n", fvall);
			return GED_ERROR;
		    }
		    while (dbg_lib_entries[lcnt]) {
			unsigned int *cvect = dbg_vars[lcnt];
			*cvect = (unsigned int) fvall;
			lcnt++;
		    }
		    print_all_set_lib_flags(gedp, max_strlen);
		    return GED_OK;
		} else {
		    while (dbg_lib_entries[lcnt]) {
			if (BU_STR_EQUIV(argv[2], dbg_libs[lcnt])) {
			    unsigned int *cvect = dbg_vars[lcnt];
			    bu_vls_printf(gedp->ged_result_str, "0x%08x\n", *cvect);
			    return GED_OK;
			}
			lcnt++;
		    }
		    bu_vls_printf(gedp->ged_result_str, "invalid input: %s\n", argv[2]);
		    return GED_ERROR;
		}
	    }
	    if (argc > 3) {
		/* Specific value for specific library */
		while (dbg_lib_entries[lcnt]) {
		    if (BU_STR_EQUIV(argv[2], dbg_libs[lcnt])) {
			unsigned int *cvect = dbg_vars[lcnt];
			/* If we have a hex number, set it */
			if (argv[3][0] == '0' && argv[3][1] == 'x') {
			    long fvall = strtol(argv[3], NULL, 0);
			    if (fvall < 0) {
				bu_vls_printf(gedp->ged_result_str, "unusable hex value %ld\n", fvall);
				return GED_ERROR;
			    }
			    *cvect = (unsigned int)fvall;
			} else {
			    bu_vls_printf(gedp->ged_result_str, "unusable value %s\n", argv[3]);
			    return GED_ERROR;
			}
			bu_vls_printf(gedp->ged_result_str, "0x%08x\n", *cvect);
			return GED_OK;
		    }
		}
		lcnt++;
		bu_vls_printf(gedp->ged_result_str, "invalid input: %s\n", argv[2]);
		return GED_ERROR;
	    }
	}

	lcnt = 0;
	while (dbg_lib_entries[lcnt]) {
	    if (BU_STR_EQUIV(argv[1], dbg_libs[lcnt])) {
		if (argc > 2) {
		    /* If we have a specified flag, toggle it.  Else, just print
		     * what is active */
		    unsigned int *cvect = dbg_vars[lcnt];
		    int found = 0;
		    int ecnt = 0;
		    while (dbg_lib_entries[lcnt][ecnt].val) {
			if (BU_STR_EQUIV(argv[2], dbg_lib_entries[lcnt][ecnt].key)) {
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
