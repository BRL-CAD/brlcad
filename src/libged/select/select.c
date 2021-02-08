/*                         S E L E C T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/select.c
 *
 * The select command.
 *
 */

#include "common.h"

#include <string.h>


#include "bu/getopt.h"
#include "../ged_private.h"


/*
 * Returns a list of items within the specified rectangle or circle.
 * If bot is specified, the bot points within the specified area are returned.
 *
 * Usage:
 * select [-b bot] [-p] [-z vminz] vx vy {vr | vw vh}
 *
 */
int
ged_select_core(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    double vx, vy, vw, vh, vr;
    static const char *usage = "[-b bot] [-p] [-z vminz] vx vy {vr | vw vh}";
    const char *cmd = argv[0];
    struct rt_db_internal intern;
    struct rt_bot_internal *botip = NULL;
    int pflag = 0;
    double vminz = -1000.0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Get command line options. */
    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "b:pz:")) != -1) {
	switch (c) {
	case 'b':
	{
	    mat_t mat;

	    /* skip subsequent bot specifications */
	    if (botip != (struct rt_bot_internal *)NULL)
		break;

	    if (wdb_import_from_path2(gedp->ged_result_str, &intern, bu_optarg, gedp->ged_wdbp, mat) & GED_ERROR) {
		bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", cmd, bu_optarg);
		return GED_ERROR;
	    }

	    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
		intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
		bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT", cmd, bu_optarg);
		rt_db_free_internal(&intern);

		return GED_ERROR;
	    }

	    botip = (struct rt_bot_internal *)intern.idb_ptr;
	}

	break;
	case 'p':
	    pflag = 1;
	    break;
	case 'z':
	    if (sscanf(bu_optarg, "%lf", &vminz) != 1) {
		if (botip != (struct rt_bot_internal *)NULL)
		    rt_db_free_internal(&intern);

		bu_vls_printf(gedp->ged_result_str, "%s: bad vminz - %s", cmd, bu_optarg);
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    }

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return GED_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_ERROR;
    }

    if (argc == 4) {
	if (sscanf(argv[1], "%lf", &vx) != 1 ||
	    sscanf(argv[2], "%lf", &vy) != 1 ||
	    sscanf(argv[3], "%lf", &vr) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return GED_ERROR;
	}

	if (botip != (struct rt_bot_internal *)NULL) {
	    int ret;

	    ret = _ged_select_botpts(gedp, botip, vx, vy, vr, vr, vminz, 1);
	    rt_db_free_internal(&intern);

	    return ret;
	} else {
	    if (pflag)
		return dl_select_partial(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vr, vr, 1);
	    else
		return dl_select(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vr, vr, 1);
	}
    } else {
	if (sscanf(argv[1], "%lf", &vx) != 1 ||
	    sscanf(argv[2], "%lf", &vy) != 1 ||
	    sscanf(argv[3], "%lf", &vw) != 1 ||
	    sscanf(argv[4], "%lf", &vh) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return GED_ERROR;
	}

	if (botip != (struct rt_bot_internal *)NULL) {
	    int ret;

	    ret = _ged_select_botpts(gedp, botip, vx, vy, vw, vh, vminz, 0);
	    rt_db_free_internal(&intern);

	    return ret;
	} else {
	    if (pflag)
		return dl_select_partial(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vw, vh, 0);
	    else
		return dl_select(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vw, vh, 0);
	}
    }
}


/*
 * Returns a list of items within the previously defined rectangle.
 *
 * Usage:
 * rselect [-b bot] [-p] [-z vminz]
 *
 */
int
ged_rselect_core(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    static const char *usage = "[-b bot] [-p] [-z vminz]";
    const char *cmd = argv[0];
    struct rt_db_internal intern;
    struct rt_bot_internal *botip = (struct rt_bot_internal *)NULL;
    int pflag = 0;
    double vminz = -1000.0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Get command line options. */
    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "b:pz:")) != -1) {
	switch (c) {
	case 'b':
	{
	    mat_t mat;

	    /* skip subsequent bot specifications */
	    if (botip != (struct rt_bot_internal *)NULL)
		break;

	    if (wdb_import_from_path2(gedp->ged_result_str, &intern, bu_optarg, gedp->ged_wdbp, mat) == GED_ERROR) {
		bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", cmd, bu_optarg);
		return GED_ERROR;
	    }

	    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
		intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
		bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT", cmd, bu_optarg);
		rt_db_free_internal(&intern);

		return GED_ERROR;
	    }

	    botip = (struct rt_bot_internal *)intern.idb_ptr;
	}

	break;
	case 'p':
	    pflag = 1;
	    break;
	case 'z':
	    if (sscanf(bu_optarg, "%lf", &vminz) != 1) {
		if (botip != (struct rt_bot_internal *)NULL)
		    rt_db_free_internal(&intern);

		bu_vls_printf(gedp->ged_result_str, "%s: bad vminz - %s", cmd, bu_optarg);
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    }

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return GED_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_ERROR;
    }

    if (botip != (struct rt_bot_internal *)NULL) {
	int ret;

	ret = _ged_select_botpts(gedp, botip,
				  gedp->ged_gvp->gv_rect.x,
				  gedp->ged_gvp->gv_rect.y,
				  gedp->ged_gvp->gv_rect.width,
				  gedp->ged_gvp->gv_rect.height,
				  vminz,
				  0);

	rt_db_free_internal(&intern);
	return ret;
    } else {
	if (pflag)
	    return dl_select_partial(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, 				       gedp->ged_gvp->gv_rect.x,
				     gedp->ged_gvp->gv_rect.y,
				     gedp->ged_gvp->gv_rect.width,
				     gedp->ged_gvp->gv_rect.height,
				     0);
	else
	    return dl_select(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, 				       gedp->ged_gvp->gv_rect.x,
			     gedp->ged_gvp->gv_rect.y,
			     gedp->ged_gvp->gv_rect.width,
			     gedp->ged_gvp->gv_rect.height,
			     0);
    }
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl select_cmd_impl = {"select", ged_select_core, GED_CMD_DEFAULT};
const struct ged_cmd select_cmd = { &select_cmd_impl };

struct ged_cmd_impl rselect_cmd_impl = {"rselect", ged_rselect_core, GED_CMD_DEFAULT};
const struct ged_cmd rselect_cmd = { &rselect_cmd_impl };

const struct ged_cmd *select_cmds[] = { &select_cmd, &rselect_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  select_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
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
