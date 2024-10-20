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


struct summary_specifics {
    const char* name;	// name, if requested summary of object

    /* combinations */
    int combs;		// num combinations (non-region)
    int regs;		// num regions

    /* solids */
    int bots;		// num bot solids
    int brep;		// num brep solids
    int othr;		// num primitives (not bot/brep)

    /* solid specifics */
    unsigned long bot_triangles;
};

static void comb_counter(struct db_i* UNUSED(dbip), struct directory* dp, void* cdata)
{

    struct summary_specifics* ssp = (struct summary_specifics*)cdata;

    if (dp->d_flags & RT_DIR_REGION) {
	ssp->regs++;
    } else {
	ssp->combs++;
    }
}

static void solid_counter(struct db_i* dbip, struct directory* dp, void* cdata)
{
    struct summary_specifics* ssp = (struct summary_specifics*)cdata;

    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t*)NULL, NULL) < 0)
	return;

    // filter items we care about summarizing
    int mtype = intern.idb_minor_type;
    if (mtype == ID_BOT) {
	struct rt_bot_internal* botip = (struct rt_bot_internal*)intern.idb_ptr;
	ssp->bots++;
	ssp->bot_triangles += botip->num_faces;
    } else if (mtype == ID_BREP) {
	ssp->brep++;
    } else {
	ssp->othr++;
    }
}

static void
print_counts(struct bu_vls* output, struct summary_specifics* ssp)
{
    if (ssp->name)
	bu_vls_printf(output, "Summary (%s):\n", ssp->name);
    else
	bu_vls_printf(output, "Summary:\n");

    int sol_total = ssp->bots + ssp->brep + ssp->othr;
    bu_vls_printf(output, "  %5d region; %d non-region combinations\n", ssp->regs, ssp->combs);
    bu_vls_printf(output, "  %5d solids\n", sol_total);

    if (ssp->bot_triangles)
	bu_vls_printf(output, "%10d bots (%lu faces)\n", ssp->bots, ssp->bot_triangles);
    else
	bu_vls_printf(output, "%10d bots\n", ssp->bots);
    bu_vls_printf(output, "%10d breps\n", ssp->brep);
    bu_vls_printf(output, "%10d BRL-CAD primitives\n", ssp->othr);
}

/*
 * Summarize the contents of the directory by categories
 * (solid, comb, region).  If flag is != 0, it is interpreted
 * as a request to print all the names in that category (e.g., RT_DIR_SOLID).
 */
static void
summary_dir(struct ged *gedp, int flag, struct bu_vls* specific)
{
    struct directory* dp = NULL;
    struct directory* specific_dp = NULL;
    struct summary_specifics ss = {0};

    if (specific) {
	const char* specific_cstr = bu_vls_cstr(specific);
	specific_dp = db_lookup(gedp->dbip, specific_cstr, 1);
	if (!specific_dp) {
	    // print usage?
	    return;
	}

	ss.name = specific_cstr;
	db_functree(gedp->dbip, specific_dp, comb_counter, solid_counter, NULL, (void*)&ss);

	print_counts(gedp->ged_result_str, &ss);
	return;
    }

    // get generic counts for database
    FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	// do stuff with count
	if (dp->d_flags & RT_DIR_SOLID) {
	    ss.othr++;
	} else if (dp->d_flags & RT_DIR_REGION) {
	    ss.regs++;
	} else if (dp->d_flags & RT_DIR_COMB) {
	    ss.combs++;
	} // else - we don't care
    } FOR_ALL_DIRECTORY_END

    print_counts(gedp->ged_result_str, &ss);

    if (flag == 0)
	return;

    /* Print all names matching the flags parameter */
    /* THIS MIGHT WANT TO BE SEPARATED OUT BY CATEGORY */

    struct directory** dirp = _ged_dir_getspace(gedp->dbip, 0);
    struct directory** dirp0 = dirp;
    /*
     * Walk the directory list adding pointers (to the directory entries
     * of interest) to the array
     */
    FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	if (dp->d_flags & flag)
	    *dirp++ = dp;
    } FOR_ALL_DIRECTORY_END

    _ged_vls_col_pr4v(gedp->ged_result_str, dirp0, (int)(dirp - dirp0), 0, 0);
    bu_free((void *)dirp0, "dir_getspace");
}


int
ged_summary_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    struct bu_opt_desc d[4];
    struct bu_vls usage = BU_VLS_INIT_ZERO;
    struct bu_vls obj_name = BU_VLS_INIT_ZERO;
    BU_OPT(d[0], "h", "help",      "",         NULL,  &print_help,  "Print help and exit");
    BU_OPT(d[1], "?",     "",      "",         NULL,  &print_help,  "");
    BU_OPT(d[2], "o",  "obj",  "name",  &bu_opt_vls,    &obj_name,  "Specify database object to summarize");
    BU_OPT_NULL(d[3]);

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

    /* must be wanting database summary */
    if (!argc && !bu_vls_strlen(&obj_name)) {
	summary_dir(gedp, 0, NULL);
	bu_vls_free(&obj_name);
	return BRLCAD_OK;
    }

    /* TODO: deprecate me */
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
	    summary_dir(gedp, flags, NULL);
	    bu_vls_free(&obj_name);
	    return BRLCAD_OK;
	}
    }

    /* ensure we have one object name */
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

    /* Summarize the object */
    summary_dir(gedp, 0, &obj_name);

    /* cleanup */
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
