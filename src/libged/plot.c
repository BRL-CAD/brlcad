/*                         P L O T . C
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
/** @file libged/plot.c
 *
 * The plot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "plot3.h"
#include "solid.h"

#include "./qray.h"
#include "./ged_private.h"


/*
 * plot file [opts]
 * potential options might include:
 * grid, 3d w/color, |filter, infinite Z
 */
int
ged_plot(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    struct bn_vlist *vp;
    FILE *fp;
    static vect_t clipmin, clipmax;
    static vect_t last;		/* last drawn point */
    static vect_t fin;
    static vect_t start;
    int Three_D;			/* 0=2-D -vs- 1=3-D */
    int Z_clip;			/* Z clipping */
    int Dashing;			/* linetype is dashed */
    int floating;			/* 3-D floating point plot */
    int is_pipe = 0;
    static const char *usage = "file [2|3] [f] [g] [z]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Process any options */
    Three_D = 1;				/* 3-D w/color, by default */
    Z_clip = 0;				/* NO Z clipping, by default*/
    floating = 0;
    while (argv[1] != (char *)0 && argv[1][0] == '-') {
	switch (argv[1][1]) {
	    case 'f':
		floating = 1;
		break;
	    case '3':
		Three_D = 1;
		break;
	    case '2':
		Three_D = 0;		/* 2-D, for portability */
		break;
	    case 'g':
		/* do grid */
		bu_vls_printf(gedp->ged_result_str, "%s: grid unimplemented\n", argv[0]);
		break;
	    case 'z':
	    case 'Z':
		/* Enable Z clipping */
		bu_vls_printf(gedp->ged_result_str, "%s: Clipped in Z to viewing cube\n", argv[0]);
		Z_clip = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "%s: bad PLOT option %s\n", argv[0], argv[1]);
		break;
	}
	argv++;
    }
    if (argv[1] == (char *)0) {
	bu_vls_printf(gedp->ged_result_str, "%s: no filename or filter specified\n", argv[0]);
	return GED_ERROR;
    }
    if (argv[1][0] == '|') {
	struct bu_vls str;
	bu_vls_init(&str);
	bu_vls_strcpy(&str, &argv[1][1]);
	while ((++argv)[1] != (char *)0) {
	    bu_vls_strcat(&str, " ");
	    bu_vls_strcat(&str, argv[1]);
	}
	if ((fp = popen(bu_vls_addr(&str), "w")) == NULL) {
	    perror(bu_vls_addr(&str));
	    return GED_ERROR;
	}

	bu_vls_printf(gedp->ged_result_str, "piped to %s\n", bu_vls_addr(&str));
	bu_vls_free(&str);
	is_pipe = 1;
    } else {
	if ((fp = fopen(argv[1], "w")) == NULL) {
	    perror(argv[1]);
	    return GED_ERROR;
	}

	bu_vls_printf(gedp->ged_result_str, "plot stored in %s\n", argv[1]);
	is_pipe = 0;
    }

    if (floating) {
	pd_3space(fp,
		  -gedp->ged_gvp->gv_center[MDX] - gedp->ged_gvp->gv_scale,
		  -gedp->ged_gvp->gv_center[MDY] - gedp->ged_gvp->gv_scale,
		  -gedp->ged_gvp->gv_center[MDZ] - gedp->ged_gvp->gv_scale,
		  -gedp->ged_gvp->gv_center[MDX] + gedp->ged_gvp->gv_scale,
		  -gedp->ged_gvp->gv_center[MDY] + gedp->ged_gvp->gv_scale,
		  -gedp->ged_gvp->gv_center[MDZ] + gedp->ged_gvp->gv_scale);
	Dashing = 0;
	pl_linmod(fp, "solid");

	gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		/* Could check for differences from last color */
		pl_color(fp,
			 sp->s_color[0],
			 sp->s_color[1],
			 sp->s_color[2]);
		if (Dashing != sp->s_soldash) {
		    if (sp->s_soldash)
			pl_linmod(fp, "dotdashed");
		    else
			pl_linmod(fp, "solid");
		    Dashing = sp->s_soldash;
		}
		rt_vlist_to_uplot(fp, &(sp->s_vlist));
	    }

	    gdlp = next_gdlp;
	}

	goto out;
    }

    /*
     * Integer output version, either 2-D or 3-D.
     * Viewing region is from -1.0 to +1.0
     * which is mapped to integer space -2048 to +2048 for plotting.
     * Compute the clipping bounds of the screen in view space.
     */
    clipmin[X] = -1.0;
    clipmax[X] =  1.0;
    clipmin[Y] = -1.0;
    clipmax[Y] =  1.0;
    if (Z_clip) {
	clipmin[Z] = -1.0;
	clipmax[Z] =  1.0;
    } else {
	clipmin[Z] = -1.0e20;
	clipmax[Z] =  1.0e20;
    }

    if (Three_D)
	pl_3space(fp, (int)DG_GED_MIN, (int)DG_GED_MIN, (int)DG_GED_MIN, (int)DG_GED_MAX, (int)DG_GED_MAX, (int)DG_GED_MAX);
    else
	pl_space(fp, (int)DG_GED_MIN, (int)DG_GED_MIN, (int)DG_GED_MAX, (int)DG_GED_MAX);
    pl_erase(fp);
    Dashing = 0;
    pl_linmod(fp, "solid");

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    if (Dashing != sp->s_soldash) {
		if (sp->s_soldash)
		    pl_linmod(fp, "dotdashed");
		else
		    pl_linmod(fp, "solid");
		Dashing = sp->s_soldash;
	    }
	    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		int i;
		int nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;
		for (i = 0; i < nused; i++, cmd++, pt++) {
		    switch (*cmd) {
			case BN_VLIST_POLY_START:
			case BN_VLIST_POLY_VERTNORM:
			    continue;
			case BN_VLIST_POLY_MOVE:
			case BN_VLIST_LINE_MOVE:
			    /* Move, not draw */
			    MAT4X3PNT(last, gedp->ged_gvp->gv_model2view, *pt);
			    continue;
			case BN_VLIST_POLY_DRAW:
			case BN_VLIST_POLY_END:
			case BN_VLIST_LINE_DRAW:
			    /* draw */
			    MAT4X3PNT(fin, gedp->ged_gvp->gv_model2view, *pt);
			    VMOVE(start, last);
			    VMOVE(last, fin);
			    break;
		    }
		    if (ged_vclip(start, fin, clipmin, clipmax) == 0)
			continue;

		    if (Three_D) {
			/* Could check for differences from last color */
			pl_color(fp,
				 sp->s_color[0],
				 sp->s_color[1],
				 sp->s_color[2]);
			pl_3line(fp,
				 (int)(start[X] * DG_GED_MAX),
				 (int)(start[Y] * DG_GED_MAX),
				 (int)(start[Z] * DG_GED_MAX),
				 (int)(fin[X] * DG_GED_MAX),
				 (int)(fin[Y] * DG_GED_MAX),
				 (int)(fin[Z] * DG_GED_MAX));
		    } else {
			pl_line(fp,
				(int)(start[0] * DG_GED_MAX),
				(int)(start[1] * DG_GED_MAX),
				(int)(fin[0] * DG_GED_MAX),
				(int)(fin[1] * DG_GED_MAX));
		    }
		}
	    }
	}

	gdlp = next_gdlp;
    }
out:
    if (is_pipe)
	(void)pclose(fp);
    else
	(void)fclose(fp);

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
