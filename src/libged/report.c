/*                         R E P O R T . C
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
/** @file libged/report.c
 *
 * The report command.
 *
 */

#include <stdlib.h>

#include "ged.h"
#include "solid.h"


/*
 * Given a pointer to a member of the circularly linked list of solids
 * (typically the head), chase the list and print out the information
 * about each solid structure.
 */
static void
print_schain(struct ged *gedp, int lvl)
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    struct bn_vlist *vp;
    int nvlist;
    int npts;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    if (lvl <= -2) {
		/* print only leaves */
		bu_vls_printf(&gedp->ged_result_str, "%s ", LAST_SOLID(sp)->d_namep);
		continue;
	    }

	    db_path_to_vls(&gedp->ged_result_str, &sp->s_fullpath);

	    if ((lvl != -1) && (sp->s_iflag == UP))
		bu_vls_printf(&gedp->ged_result_str, " ILLUM");

	    bu_vls_printf(&gedp->ged_result_str, "\n");

	    if (lvl <= 0)
		continue;

	    /* convert to the local unit for printing */
	    bu_vls_printf(&gedp->ged_result_str, "  cent=(%.3f, %.3f, %.3f) sz=%g ",
			  sp->s_center[X]*gedp->ged_wdbp->dbip->dbi_base2local,
			  sp->s_center[Y]*gedp->ged_wdbp->dbip->dbi_base2local,
			  sp->s_center[Z]*gedp->ged_wdbp->dbip->dbi_base2local,
			  sp->s_size*gedp->ged_wdbp->dbip->dbi_base2local);
	    bu_vls_printf(&gedp->ged_result_str, "reg=%d\n", sp->s_regionid);
	    bu_vls_printf(&gedp->ged_result_str, "  basecolor=(%d, %d, %d) color=(%d, %d, %d)%s%s%s\n",
			  sp->s_basecolor[0],
			  sp->s_basecolor[1],
			  sp->s_basecolor[2],
			  sp->s_color[0],
			  sp->s_color[1],
			  sp->s_color[2],
			  sp->s_uflag?" U":"",
			  sp->s_dflag?" D":"",
			  sp->s_cflag?" C":"");

	    if (lvl <= 1)
		continue;

	    /* Print the actual vector list */
	    nvlist = 0;
	    npts = 0;
	    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		int i;
		int nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;

		BN_CK_VLIST(vp);
		nvlist++;
		npts += nused;

		if (lvl <= 2)
		    continue;

		for (i = 0; i < nused; i++, cmd++, pt++) {
		    bu_vls_printf(&gedp->ged_result_str, "  %s (%g, %g, %g)\n",
				  rt_vlist_cmd_descriptions[*cmd],
				  V3ARGS(*pt));
		}
	    }

	    bu_vls_printf(&gedp->ged_result_str, "  %d vlist structures, %d pts\n", nvlist, npts);
	    bu_vls_printf(&gedp->ged_result_str, "  %d pts (via rt_ck_vlist)\n", rt_ck_vlist(&(sp->s_vlist)));
	}

	gdlp = next_gdlp;
    }
}


/*
 * D G O _ P R _ S C H A I N _ V L C M D S
 *
 * Given a pointer to a member of the circularly linked list of solids
 * (typically the head), chase the list and print out the vlist cmds
 * for each structure.
 */
static void
print_schain_vlcmds(struct ged *gedp)
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    struct bn_vlist *vp;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    bu_vls_printf(&gedp->ged_result_str, "-1 %d %d %d\n",
			  sp->s_color[0],
			  sp->s_color[1],
			  sp->s_color[2]);

	    /* Print the actual vector list */
	    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		int i;
		int nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;

		BN_CK_VLIST(vp);

		for (i = 0; i < nused; i++, cmd++, pt++)
		    bu_vls_printf(&gedp->ged_result_str, "%d %g %g %g\n", *cmd, V3ARGS(*pt));
	    }
	}

	gdlp = next_gdlp;
    }
}



/*
 * Returns the solid table & vector list as a string
 *
 * Usage:
 * report [lvl]
 *
 */
int
ged_report(struct ged *gedp, int argc, const char *argv[])
{
    int lvl = 0;
    static const char *usage = "lvl";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    lvl = atoi(argv[1]);

    if (lvl <= 3)
	print_schain(gedp, lvl);
    else
	print_schain_vlcmds(gedp);

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
