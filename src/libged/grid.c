/*                          G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file grid.c
 *
 * Routines to implement MGED's snap to grid capability.
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"

#include "./ged_private.h"


static void ged_grid_vls_print(struct ged *gedp);
static void ged_snap_to_grid(struct ged *gedp, fastf_t *mx, fastf_t *my);
static void ged_grid_vsnap(struct ged *gedp);

static char ged_grid_syntax[] = "\
 grid vname color [r g b]	set or get the color\n\
 grid vname draw [0|1]		set or get the draw parameter\n\
 grid vname help		prints this help message\n\
 grid vname mrh [ival]		set or get the major resolution (horizontal)\n\
 grid vname mrv [ival]		set or get the major resolution (vertical)\n\
 grid vname rh [fval]		set or get the resolution (horizontal)\n\
 grid vname rv [fval]		set or get the resolution (vertical)\n\
 grid vname snap [0|1]		set or get the snap parameter\n\
 grid vname vars		print a list of all variables (i.e. var = val)\n\
 grid vname vsnap		snaps the view center to the nearest grid point\n\
";


/*
 * Note - this needs to be rewritten to accept keyword/value pairs so
 *        that multiple attributes can be set with a single command call.
 */
int
ged_grid(struct ged	*gedp,
	 int		argc,
	 const char	*argv[])
{
    char *command;
    char *parameter;
    char **argp = (char **)argv;
    point_t user_pt;		/* Value(s) provided by user */
    int i;
    static const char *usage = ged_grid_syntax;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_OK;
    }

    if (argc < 2 || 5 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    command = (char *)argv[0];
    parameter = (char *)argv[1];
    argc -= 2;
    argp += 2;

    for (i = 0; i < argc; ++i)
	if (sscanf(argp[i], "%lf", &user_pt[i]) != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

    if (strcmp(parameter, "draw") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_grid.ggs_draw);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_grid.ggs_draw = 1;
	    else
		gedp->ged_gvp->gv_grid.ggs_draw = 0;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s draw' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "vsnap") == 0) {
	if (argc == 0) {
	    ged_grid_vsnap(gedp);
	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s vsnap' command accepts no arguments\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "snap") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_grid.ggs_snap);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_grid.ggs_snap = 1;
	    else
		gedp->ged_gvp->gv_grid.ggs_snap = 0;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s snap' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "rh") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g",
			  gedp->ged_gvp->gv_grid.ggs_res_h * gedp->ged_wdbp->dbip->dbi_base2local);
	    return GED_OK;
	} else if (argc == 1) {
	    gedp->ged_gvp->gv_grid.ggs_res_h = user_pt[X] * gedp->ged_wdbp->dbip->dbi_local2base;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s rh' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "rv") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g",
			  gedp->ged_gvp->gv_grid.ggs_res_v * gedp->ged_wdbp->dbip->dbi_base2local);
	    return GED_OK;
	} else if (argc == 1) {
	    gedp->ged_gvp->gv_grid.ggs_res_v = user_pt[X] * gedp->ged_wdbp->dbip->dbi_local2base;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s rv' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "mrh") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_grid.ggs_res_major_h);
	    return GED_OK;
	} else if (argc == 1) {
	    gedp->ged_gvp->gv_grid.ggs_res_major_h = (int)user_pt[X];

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s mrh' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "mrv") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_grid.ggs_res_major_v);
	    return GED_OK;
	} else if (argc == 1) {
	    gedp->ged_gvp->gv_grid.ggs_res_major_v = (int)user_pt[X];

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s mrv' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "anchor") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g %g %g",
			  gedp->ged_gvp->gv_grid.ggs_anchor[X] * gedp->ged_wdbp->dbip->dbi_base2local,
			  gedp->ged_gvp->gv_grid.ggs_anchor[Y] * gedp->ged_wdbp->dbip->dbi_base2local,
			  gedp->ged_gvp->gv_grid.ggs_anchor[Z] * gedp->ged_wdbp->dbip->dbi_base2local);
	    return GED_OK;
	} else if (argc == 3) {
	    gedp->ged_gvp->gv_grid.ggs_anchor[0] = user_pt[X] * gedp->ged_wdbp->dbip->dbi_local2base;
	    gedp->ged_gvp->gv_grid.ggs_anchor[1] = user_pt[Y] * gedp->ged_wdbp->dbip->dbi_local2base;
	    gedp->ged_gvp->gv_grid.ggs_anchor[2] = user_pt[Z] * gedp->ged_wdbp->dbip->dbi_local2base;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s color' command requires 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "color") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gvp->gv_grid.ggs_color[X],
			  gedp->ged_gvp->gv_grid.ggs_color[Y],
			  gedp->ged_gvp->gv_grid.ggs_color[Z]);
	    return GED_OK;
	} else if (argc == 3) {
	    gedp->ged_gvp->gv_grid.ggs_color[0] = (int)user_pt[X];
	    gedp->ged_gvp->gv_grid.ggs_color[1] = (int)user_pt[Y];
	    gedp->ged_gvp->gv_grid.ggs_color[2] = (int)user_pt[Z];

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s color' command requires 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "vars") == 0) {
	ged_grid_vls_print(gedp);
	return GED_OK;
    }

    if (strcmp(parameter, "help") == 0) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", command, usage);
	return GED_HELP;
    }

    bu_vls_printf(&gedp->ged_result_str, "%s: unrecognized command '%s'\nUsage: %s %s\n",
		  command, parameter, command, usage);
    return GED_ERROR;
}

static void
ged_grid_vls_print(struct ged *gedp)
{
    bu_vls_printf(&gedp->ged_result_str, "anchor = %g %g %g\n",
		  gedp->ged_gvp->gv_grid.ggs_anchor[0] * gedp->ged_wdbp->dbip->dbi_base2local,
		  gedp->ged_gvp->gv_grid.ggs_anchor[1] * gedp->ged_wdbp->dbip->dbi_base2local,
		  gedp->ged_gvp->gv_grid.ggs_anchor[2] * gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(&gedp->ged_result_str, "color = %d %d %d\n",
		  gedp->ged_gvp->gv_grid.ggs_color[0],
		  gedp->ged_gvp->gv_grid.ggs_color[1],
		  gedp->ged_gvp->gv_grid.ggs_color[2]);
    bu_vls_printf(&gedp->ged_result_str, "draw = %d\n", gedp->ged_gvp->gv_grid.ggs_draw);
    bu_vls_printf(&gedp->ged_result_str, "mrh = %d\n", gedp->ged_gvp->gv_grid.ggs_res_major_h);
    bu_vls_printf(&gedp->ged_result_str, "mrv = %d\n", gedp->ged_gvp->gv_grid.ggs_res_major_v);
    bu_vls_printf(&gedp->ged_result_str, "rh = %g\n", gedp->ged_gvp->gv_grid.ggs_res_h * gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(&gedp->ged_result_str, "rv = %g\n", gedp->ged_gvp->gv_grid.ggs_res_v * gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(&gedp->ged_result_str, "snap = %d\n", gedp->ged_gvp->gv_grid.ggs_snap);
}

static void
ged_snap_to_grid(struct ged *gedp, fastf_t *mx, fastf_t *my)
{
    int nh, nv;		/* whole grid units */
    point_t view_pt;
    point_t view_grid_anchor;
    fastf_t grid_units_h;		/* eventually holds only fractional horizontal grid units */
    fastf_t grid_units_v;		/* eventually holds only fractional vertical grid units */
    fastf_t sf;
    fastf_t inv_sf;
    fastf_t local2base = 1.0 / gedp->ged_wdbp->dbip->dbi_base2local;

    if (NEAR_ZERO(gedp->ged_gvp->gv_grid.ggs_res_h, (fastf_t)SMALL_FASTF) ||
	NEAR_ZERO(gedp->ged_gvp->gv_grid.ggs_res_v, (fastf_t)SMALL_FASTF))
	return;

    sf = gedp->ged_gvp->gv_scale*gedp->ged_wdbp->dbip->dbi_base2local;
    inv_sf = 1 / sf;

    VSET(view_pt, *mx, *my, 0.0);
    VSCALE(view_pt, view_pt, sf);  /* view_pt now in local units */

    MAT4X3PNT(view_grid_anchor, gedp->ged_gvp->gv_model2view, gedp->ged_gvp->gv_grid.ggs_anchor);
    VSCALE(view_grid_anchor, view_grid_anchor, sf);  /* view_grid_anchor now in local units */

    grid_units_h = (view_grid_anchor[X] - view_pt[X]) / (gedp->ged_gvp->gv_grid.ggs_res_h * gedp->ged_wdbp->dbip->dbi_base2local);
    grid_units_v = (view_grid_anchor[Y] - view_pt[Y]) / (gedp->ged_gvp->gv_grid.ggs_res_v * gedp->ged_wdbp->dbip->dbi_base2local);
    nh = grid_units_h;
    nv = grid_units_v;

    grid_units_h -= nh;		/* now contains only the fraction part */
    grid_units_v -= nv;		/* now contains only the fraction part */

    if (grid_units_h <= -0.5)
	*mx = view_grid_anchor[X] - ((nh - 1) * gedp->ged_gvp->gv_grid.ggs_res_h * gedp->ged_wdbp->dbip->dbi_base2local);
    else if (0.5 <= grid_units_h)
	*mx = view_grid_anchor[X] - ((nh + 1) * gedp->ged_gvp->gv_grid.ggs_res_h * gedp->ged_wdbp->dbip->dbi_base2local);
    else
	*mx = view_grid_anchor[X] - (nh * gedp->ged_gvp->gv_grid.ggs_res_h * gedp->ged_wdbp->dbip->dbi_base2local);

    if (grid_units_v <= -0.5)
	*my = view_grid_anchor[Y] - ((nv - 1) * gedp->ged_gvp->gv_grid.ggs_res_v * gedp->ged_wdbp->dbip->dbi_base2local);
    else if (0.5 <= grid_units_v)
	*my = view_grid_anchor[Y] - ((nv + 1) * gedp->ged_gvp->gv_grid.ggs_res_v * gedp->ged_wdbp->dbip->dbi_base2local);
    else
	*my = view_grid_anchor[Y] - (nv  * gedp->ged_gvp->gv_grid.ggs_res_v * gedp->ged_wdbp->dbip->dbi_base2local);

    *mx *= inv_sf;
    *my *= inv_sf;
}

static void
ged_grid_vsnap(struct ged *gedp)
{
    point_t view_pt;
    point_t model_pt;

    MAT_DELTAS_GET_NEG(model_pt, gedp->ged_gvp->gv_center);
    MAT4X3PNT(view_pt, gedp->ged_gvp->gv_model2view, model_pt);
    ged_snap_to_grid(gedp, &view_pt[X], &view_pt[Y]);
    MAT4X3PNT(model_pt, gedp->ged_gvp->gv_view2model, view_pt);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, model_pt);
    ged_view_update(gedp->ged_gvp);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
