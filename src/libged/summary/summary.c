/*                         S U M M A R Y . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/summary.c
 *
 * The summary command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


/*
 * Summarize the contents of the directory by categories
 * (solid, comb, region).  If flag is != 0, it is interpreted
 * as a request to print all the names in that category (e.g., RT_DIR_SOLID).
 */
static void
summary_dir(struct ged *gedp,
		int flag)
{
    struct directory *dp;
    int i;
    static int sol, comb, reg;
    struct directory **dirp;
    struct directory **dirp0 = (struct directory **)NULL;

    sol = comb = reg = 0;
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & RT_DIR_SOLID)
		sol++;
	    if (dp->d_flags & RT_DIR_COMB) {
		if (dp->d_flags & RT_DIR_REGION)
		    reg++;
		else
		    comb++;
	    }
	}
    }

    bu_vls_printf(gedp->ged_result_str, "Summary:\n");
    bu_vls_printf(gedp->ged_result_str, "  %5d primitives\n", sol);
    bu_vls_printf(gedp->ged_result_str, "  %5d region; %d non-region combinations\n", reg, comb);
    bu_vls_printf(gedp->ged_result_str, "  %5d total objects\n\n", sol+reg+comb);

    if (flag == 0)
	return;

    /* Print all names matching the flags parameter */
    /* THIS MIGHT WANT TO BE SEPARATED OUT BY CATEGORY */

    dirp = _ged_dir_getspace(gedp->dbip, 0);
    dirp0 = dirp;
    /*
     * Walk the directory list adding pointers (to the directory entries
     * of interest) to the array
     */
    for (i = 0; i < RT_DBNHASH; i++)
	for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw)
	    if (dp->d_flags & flag)
		*dirp++ = dp;

    _ged_vls_col_pr4v(gedp->ged_result_str, dirp0, (int)(dirp - dirp0), 0, 0);
    bu_free((void *)dirp0, "dir_getspace");
}


int
ged_summary_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    struct bu_opt_desc d[3];
    struct bu_vls usage = BU_VLS_INIT_ZERO;
    struct bu_vls obj_name = BU_VLS_INIT_ZERO;
    BU_OPT(d[0], "h", "help",      "",         NULL,  &print_help,  "Print help and exit");
    BU_OPT(d[1], "?",     "",      "",         NULL,  &print_help,  "");
    BU_OPT(d[1], "o",  "obj",  "name",  &bu_opt_vls,    &obj_name,  "Specify database object to summarize");
    BU_OPT_NULL(d[2]);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    /* done with command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    if (print_help) {
	_ged_cmd_help(gedp, bu_vls_cstr(&usage), d);
	bu_vls_free(&usage);
	bu_vls_free(&obj_name);
	return BRLCAD_OK;
    }
    bu_vls_free(&usage);


    if (!argc && !bu_vls_strlen(&obj_name)) {
	summary_dir(gedp, 0);
	bu_vls_free(&obj_name);
	return BRLCAD_OK;
    }

    if (argc == 1 && strlen(argv[0]) == 1) {
	// NOTE:  special casing of p, r and g is deprecated, but for now
	// handle these options as we originally would have.
	const char *cp = (const char *)argv[0];
	int flags = 0;
	while (*cp) {
	    switch (*cp++) {
		case 'p':
		    flags |= RT_DIR_SOLID;
		    break;
		case 'r':
		    flags |= RT_DIR_REGION;
		    break;
		case 'g':
		    flags |= RT_DIR_COMB;
		    break;
		default:
		    flags = 0;
	    }
	}

	if (flags) {
	    summary_dir(gedp, flags);
	    bu_vls_free(&obj_name);
	    return BRLCAD_OK;
	}
    }

    if (!bu_vls_strlen(&obj_name)) {
	if (argc != 1) {
	    bu_vls_printf(gedp->ged_result_str, "expecting a single object name.\n");
	    bu_vls_free(&obj_name);
	    return BRLCAD_ERROR;
	}
	bu_vls_sprintf(&obj_name, "%s", argv[0]);
    } else {
	if (argc) {
	    bu_vls_printf(gedp->ged_result_str, "object specified both via --obj option and as a command argument.\n");
	    bu_vls_free(&obj_name);
	    return BRLCAD_ERROR;
	}
    }

    // Summarize the object
    struct directory *dp = db_lookup(gedp->dbip, bu_vls_cstr(&obj_name), LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "object %s not found.\n", bu_vls_cstr(&obj_name));
	bu_vls_free(&obj_name);
	return BRLCAD_ERROR;
    }

    // TODO - primitive specific logic (walk combs, count BoT data, etc...)

    bu_vls_printf(gedp->ged_result_str, "TODO - object summary.\n");

    bu_vls_free(&obj_name);
    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl summary_cmd_impl = {
    "summary",
    ged_summary_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd summary_cmd = { &summary_cmd_impl };
const struct ged_cmd *summary_cmds[] = { &summary_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  summary_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
