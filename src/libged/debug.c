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
print_all_lib_flags(struct bu_vls *vls, int lcnt, int max_strlen)
{
    int ecnt = 0;
    long eval = -1;
    while (eval != 0) {
	eval = dbg_lib_entries[lcnt][ecnt].val;
	if (eval > 0) {
	    bu_vls_printf(vls, "   %*s (0x%08lx): %s\n", max_strlen, dbg_lib_entries[lcnt][ecnt].key, dbg_lib_entries[lcnt][ecnt].val, dbg_lib_entries[lcnt][ecnt].info);
	}
	ecnt++;
    }
}


static void
print_set_lib_flags(struct bu_vls *vls, int lcnt, int max_strlen)
{
    int ecnt = 0;
    while (dbg_lib_entries[lcnt][ecnt].val) {
	unsigned int *cvect = dbg_vars[lcnt];
	if (*cvect & dbg_lib_entries[lcnt][ecnt].val) {
	    bu_vls_printf(vls, "   %*s (0x%08lx): %s\n", max_strlen, dbg_lib_entries[lcnt][ecnt].key, dbg_lib_entries[lcnt][ecnt].val, dbg_lib_entries[lcnt][ecnt].info);
	}
	ecnt++;
    }
    bu_vls_printf(vls, "\n");
}

static void
print_all_set_lib_flags(struct bu_vls *vls, int max_strlen)
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

	bu_vls_printf(vls, "%s flags:\n", dbg_libs[lcnt]);
	print_set_lib_flags(vls, lcnt, max_strlen);

	lcnt++;
    }
}

static int
debug_max_strlen()
{
    int max_strlen = -1;
    int lcnt = 0;
    while (dbg_lib_entries[lcnt]) {
	int ecnt = 0;
	while (dbg_lib_entries[lcnt][ecnt].val != 0) {
	    int slen = strlen(dbg_lib_entries[lcnt][ecnt].key);
	    max_strlen = (slen > max_strlen) ? slen : max_strlen;
	    ecnt++;
	}
	lcnt++;
    }
    return max_strlen;
}

static void
debug_print_help(struct bu_vls *vls)
{
    int lcnt = 0;
    bu_vls_printf(vls, "debug [-h] [-l [lib]] [-C [lib]] [-V [lib] [val]] [[lib] [flag]]\n\n");
    bu_vls_printf(vls, "Available libs:\n");
    while (dbg_lib_entries[lcnt]) {
	bu_vls_printf(vls, "\t%s\n", dbg_libs[lcnt]);
	lcnt++;
    }
}

static void
print_all_flags(struct bu_vls *vls, int max_strlen)
{
    int lcnt = 0;
    while (dbg_lib_entries[lcnt]) {
	bu_vls_printf(vls, "%s flags:\n", dbg_libs[lcnt]);
	print_all_lib_flags(vls, lcnt, max_strlen);
	bu_vls_printf(vls, "\n");
	lcnt++;
    }
}

static void
print_select_flags(struct bu_vls *vls, const char *filter, int max_strlen)
{
    int lcnt = 0;
    while (dbg_lib_entries[lcnt]) {
	if (!BU_STR_EQUAL(filter, "*") && !(BU_STR_EQUIV(filter, dbg_libs[lcnt]))) {
	    lcnt++;
	    continue;
	}
	bu_vls_printf(vls, "%s flags:\n", dbg_libs[lcnt]);
	print_all_lib_flags(vls, lcnt, max_strlen);
	bu_vls_printf(vls, "\n");
	lcnt++;
    }
}

static int
toggle_debug_flag(struct bu_vls *vls, const char *lib_filter, const char *flag_filter)
{
    int	lcnt = 0;
    while (dbg_lib_entries[lcnt]) {
	if (BU_STR_EQUIV(lib_filter, dbg_libs[lcnt])) {
	    unsigned int *cvect = dbg_vars[lcnt];
	    int ecnt = 0;
	    int found = 0;
	    while (dbg_lib_entries[lcnt][ecnt].val) {
		if (BU_STR_EQUIV(flag_filter, dbg_lib_entries[lcnt][ecnt].key)) {
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
		if (vls) {
		    bu_vls_printf(vls, "invalid %s flag paramter: %s\n", dbg_libs[lcnt], flag_filter);
		}
		return -1;
	    } else {
		return lcnt;
	    }
	}
	lcnt++;
    }

    if (vls) {
	bu_vls_printf(vls, "invalid lib paramter: %s\n", lib_filter);
    }
    return -1;
}

static void
print_flag_hex_val(struct bu_vls *vls, int lcnt, int max_strlen, int labeled)
{
    unsigned int *cvect = dbg_vars[lcnt];
    if (labeled) {
	bu_vls_printf(vls, "%*s: 0x%08x\n", max_strlen, dbg_libs[lcnt], *cvect);
    } else {
	bu_vls_printf(vls, "0x%08x\n", *cvect);
    }
}

static int
set_flag_hex_value(struct bu_vls *vls, const char *lib_filter, const char *hexstr, int max_strlen)
{
    int lcnt = 0;
    while (dbg_lib_entries[lcnt]) {
	if (BU_STR_EQUIV(lib_filter, dbg_libs[lcnt])) {
	    unsigned int *cvect = dbg_vars[lcnt];
	    /* If we have a hex number, set it */
	    if (hexstr[0] == '0' && hexstr[1] == 'x') {
		long fvall = strtol(hexstr, NULL, 0);
		if (fvall < 0) {
		    if (vls) {
			bu_vls_printf(vls, "unusable hex value %ld\n", fvall);
		    }
		    return -1;
		}
		*cvect = (unsigned int)fvall;
	    } else {
		if (vls) {
		    bu_vls_printf(vls, "invalid hex string %s\n", hexstr);
		}
		return -1;
	    }
	    if (vls) {
		print_flag_hex_val(vls, lcnt, max_strlen, 0);
	    }
	    return 0;
	}
	lcnt++;
    }
    if (vls) {
	bu_vls_printf(vls, "invalid input: %s\n", lib_filter);
    }
    return -1;
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

    max_strlen = debug_max_strlen();

    if (argc > 1) {

	if (BU_STR_EQUAL(argv[1], "-h")) {
	    debug_print_help(gedp->ged_result_str);
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "-l") && argc == 2) {
	    print_all_flags(gedp->ged_result_str, max_strlen);
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "-l") && argc == 3) {
	    print_select_flags(gedp->ged_result_str, argv[2], max_strlen);
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "-C")) {
	    if (argc == 2) {
		/* Bare -C option - zero all the hex values */
		lcnt = 0;
		while (dbg_lib_entries[lcnt]) {
		    unsigned int *cvect = dbg_vars[lcnt];
		    *cvect = 0;
		    lcnt++;
		}
		return GED_OK;
	    }
	    if (argc == 3) {
		/* -C with arg - clear a specific library */
		lcnt = 0;
		while (dbg_lib_entries[lcnt]) {
		    if (BU_STR_EQUIV(argv[2], dbg_libs[lcnt])) {
			unsigned int *cvect = dbg_vars[lcnt];
			*cvect = 0;
			return GED_OK;
		    }
		    lcnt++;
		}
		bu_vls_printf(gedp->ged_result_str, "invalid input: %s\n", argv[2]);
		return GED_ERROR;
	    }
	    if (argc > 3) {
		if (set_flag_hex_value(gedp->ged_result_str, argv[2], argv[3], max_strlen)) {
		    return GED_ERROR;
		}
		return GED_OK;
	    }
	}


	if (BU_STR_EQUAL(argv[1], "-V")) {
	    if (argc == 2) {
		/* Bare -v option - print all the hex values */
		lcnt = 0;
		while (dbg_lib_entries[lcnt]) {
		    print_flag_hex_val(gedp->ged_result_str, lcnt, max_strlen, 1);
		    lcnt++;
		}
		return GED_OK;
	    }
	    if (argc == 3) {
		lcnt = 0;
		while (dbg_lib_entries[lcnt]) {
		    if (BU_STR_EQUIV(argv[2], dbg_libs[lcnt])) {
			print_flag_hex_val(gedp->ged_result_str, lcnt, max_strlen, 0);
			return GED_OK;
		    }
		    lcnt++;
		}
		bu_vls_printf(gedp->ged_result_str, "invalid input: %s\n", argv[2]);
		return GED_ERROR;
	    }
	    if (argc > 3) {
		if (set_flag_hex_value(gedp->ged_result_str, argv[2], argv[3], max_strlen)) {
		    return GED_ERROR;
		}
		return GED_OK;
	    }
	}


	if (argc > 2) {
	    lcnt = toggle_debug_flag(gedp->ged_result_str, argv[1], argv[2]);
	    if (lcnt < 0) {
		return GED_ERROR;
	    } else {
		print_set_lib_flags(gedp->ged_result_str, lcnt, max_strlen);
		return GED_OK;
	    }
	} else {
	    lcnt = 0;
	    while (dbg_lib_entries[lcnt]) {
		if (BU_STR_EQUIV(argv[1], dbg_libs[lcnt])) {
		    print_set_lib_flags(gedp->ged_result_str, lcnt, max_strlen);
		    return GED_OK;
		}
		lcnt++;
	    }
	}
	bu_vls_printf(gedp->ged_result_str, "invalid input: %s\n", argv[1]);
	return GED_ERROR;
    }

    lcnt = 0;
    print_all_set_lib_flags(gedp->ged_result_str, max_strlen);

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
