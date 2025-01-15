/*                    S O L I D _ R E P O R T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/solid_report.c
 *
 * The solid_report command.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/opt.h"
#include "ged.h"
#include "../ged_private.h"

#define LAST_SOLID(_sp) DB_FULL_PATH_CUR_DIR(&(_sp)->s_fullpath)

static void
dl_print_schain(struct bu_list *hdlp, struct db_i *dbip, int lvl, int vlcmds, struct bu_vls *vls)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    struct bv_vlist *vp;

    if (!vlcmds) {
	/*
	 * Given a pointer to a member of the circularly linked list of solids
	 * (typically the head), chase the list and print out the information
	 * about each solid structure.
	 */
	size_t nvlist;
	size_t npts;

	if (dbip == DBI_NULL) return;

	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (!sp->s_u_data)
		    continue;
		struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

		if (lvl <= -2) {
		    /* print only leaves */
		    if (bdata && LAST_SOLID(bdata))
			bu_vls_printf(vls, "%s ", LAST_SOLID(bdata)->d_namep);
		    continue;
		}

		db_path_to_vls(vls, &bdata->s_fullpath);

		if ((lvl != -1) && (sp->s_iflag == UP))
		    bu_vls_printf(vls, " ILLUM");

		bu_vls_printf(vls, "\n");

		if (lvl <= 0)
		    continue;

		/* convert to the local unit for printing */
		bu_vls_printf(vls, "  cent=(%.3f, %.3f, %.3f) sz=%g ",
			      sp->s_center[X]*dbip->dbi_base2local,
			      sp->s_center[Y]*dbip->dbi_base2local,
			      sp->s_center[Z]*dbip->dbi_base2local,
			      sp->s_size*dbip->dbi_base2local);
		bu_vls_printf(vls, "reg=%d\n", sp->s_old.s_regionid);
		bu_vls_printf(vls, "  basecolor=(%d, %d, %d) color=(%d, %d, %d)%s%s%s\n",
			      sp->s_old.s_basecolor[0],
			      sp->s_old.s_basecolor[1],
			      sp->s_old.s_basecolor[2],
			      sp->s_color[0],
			      sp->s_color[1],
			      sp->s_color[2],
			      sp->s_old.s_uflag?" U":"",
			      sp->s_old.s_dflag?" D":"",
			      sp->s_old.s_cflag?" C":"");

		if (lvl <= 1)
		    continue;

		/* Print the actual vector list */
		nvlist = 0;
		npts = 0;
		for (BU_LIST_FOR(vp, bv_vlist, &(sp->s_vlist))) {
		    size_t i;
		    size_t nused = vp->nused;
		    int *cmd = vp->cmd;
		    point_t *pt = vp->pt;

		    BV_CK_VLIST(vp);
		    nvlist++;
		    npts += nused;

		    if (lvl <= 2)
			continue;

		    for (i = 0; i < nused; i++, cmd++, pt++) {
			bu_vls_printf(vls, "  %s (%g, %g, %g)\n",
				      bv_vlist_get_cmd_description(*cmd),
				      V3ARGS(*pt));
		    }
		}

		bu_vls_printf(vls, "  %zu vlist structures, %zu pts\n", nvlist, npts);
		bu_vls_printf(vls, "  %zu pts (via bv_ck_vlist)\n", bv_ck_vlist(&(sp->s_vlist)));
	    }

	    gdlp = next_gdlp;
	}

    } else {
	/*
	 * Given a pointer to a member of the circularly linked list of solids
	 * (typically the head), chase the list and print out the vlist cmds
	 * for each structure.
	 */

	if (dbip == DBI_NULL) return;

	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		bu_vls_printf(vls, "-1 %d %d %d\n",
			      sp->s_color[0],
			      sp->s_color[1],
			      sp->s_color[2]);

		/* Print the actual vector list */
		for (BU_LIST_FOR(vp, bv_vlist, &(sp->s_vlist))) {
		    size_t i;
		    size_t nused = vp->nused;
		    int *cmd = vp->cmd;
		    point_t *pt = vp->pt;

		    BV_CK_VLIST(vp);

		    for (i = 0; i < nused; i++, cmd++, pt++)
			bu_vls_printf(vls, "%d %g %g %g\n", *cmd, V3ARGS(*pt));
		}
	    }

	    gdlp = next_gdlp;
	}

    }
}


/*
 * Returns the list of displayed solids and/or vector list information
 * based on the provided level:
 *
 *   <= -2 print primitive names (path leaves)
 *   == -1 print paths
 *   == 0 print paths + ILLUM on illuminated
 *   == 1 print paths + ILLUM on illuminated + center/region/color info
 *   >= 2 print paths + ILLUM on illuminated + center/region/color info + vector lists
 *
 * Usage:
 * solid_report [lvl]
 *
 */
int
ged_solid_report_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int lvl = 0;
    static const char *usage = "[-2|-1|0|1|2|3]";
    const char *argv0 = argv[0];
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",      "",         NULL,  &print_help,  "Print help and exit");
    BU_OPT(d[1], "?",     "",      "",         NULL,  &print_help,  "");
    BU_OPT_NULL(d[2]);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (opt_ret < 1 || opt_ret > 2 || print_help) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return BRLCAD_ERROR;
    }

    if (opt_ret == 2)
	lvl = atoi(argv[1]);

    if (lvl > 3)
	lvl = 3;

    dl_print_schain(gedp->ged_gdp->gd_headDisplay, gedp->dbip, lvl, 0, gedp->ged_result_str);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl solid_report_cmd_impl = {"solid_report", ged_solid_report_core, GED_CMD_DEFAULT};
const struct ged_cmd solid_report_cmd = { &solid_report_cmd_impl };

struct ged_cmd_impl x_cmd_impl = {"x", ged_solid_report_core, GED_CMD_DEFAULT};
const struct ged_cmd x_cmd = { &x_cmd_impl };

const struct ged_cmd *solid_report_cmds[] = { &solid_report_cmd, &x_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  solid_report_cmds, 2 };

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
