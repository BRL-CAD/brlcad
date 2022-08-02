/*                          G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2022 United States Government as represented by
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
/** @file libged/grid.c
 *
 * Routines that provide the basics for a snap to grid capability.
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>


#include "vmath.h"
#include "bv.h"
#include "bg/lseg.h"

#include "../ged_private.h"


HIDDEN void
grid_vsnap(struct ged *gedp)
{
    point_t view_pt;
    point_t model_pt;

    MAT_DELTAS_GET_NEG(model_pt, gedp->ged_gvp->gv_center);
    MAT4X3PNT(view_pt, gedp->ged_gvp->gv_model2view, model_pt);
    bv_snap_grid_2d(gedp->ged_gvp, &view_pt[X], &view_pt[Y]);
    MAT4X3PNT(model_pt, gedp->ged_gvp->gv_view2model, view_pt);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, model_pt);
    bv_update(gedp->ged_gvp);
}


HIDDEN void
grid_vls_print(struct ged *gedp)
{
    bu_vls_printf(gedp->ged_result_str, "anchor = %g %g %g\n",
		  gedp->ged_gvp->gv_s->gv_grid.anchor[0] * gedp->dbip->dbi_base2local,
		  gedp->ged_gvp->gv_s->gv_grid.anchor[1] * gedp->dbip->dbi_base2local,
		  gedp->ged_gvp->gv_s->gv_grid.anchor[2] * gedp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "color = %d %d %d\n",
		  gedp->ged_gvp->gv_s->gv_grid.color[0],
		  gedp->ged_gvp->gv_s->gv_grid.color[1],
		  gedp->ged_gvp->gv_s->gv_grid.color[2]);
    bu_vls_printf(gedp->ged_result_str, "draw = %d\n", gedp->ged_gvp->gv_s->gv_grid.draw);
    bu_vls_printf(gedp->ged_result_str, "mrh = %d\n", gedp->ged_gvp->gv_s->gv_grid.res_major_h);
    bu_vls_printf(gedp->ged_result_str, "mrv = %d\n", gedp->ged_gvp->gv_s->gv_grid.res_major_v);
    bu_vls_printf(gedp->ged_result_str, "rh = %g\n", gedp->ged_gvp->gv_s->gv_grid.res_h * gedp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "rv = %g\n", gedp->ged_gvp->gv_s->gv_grid.res_v * gedp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "snap = %d\n", gedp->ged_gvp->gv_s->gv_grid.snap);
}


HIDDEN void
grid_usage(struct ged *gedp, const char *argv0)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", argv0);
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname color [r g b]	set or get the color\n");
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname draw [0|1]		set or get the draw parameter\n");
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname help		prints this help message\n");
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname mrh [ival]		set or get the major resolution (horizontal)\n");
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname mrv [ival]		set or get the major resolution (vertical)\n");
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname rh [fval]		set or get the resolution (horizontal)\n");
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname rv [fval]		set or get the resolution (vertical)\n");
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname snap [0|1]		set or get the snap parameter\n");
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname vars		print a list of all variables (i.e. var = val)\n");
    bu_vls_printf(gedp->ged_result_str, "%s", "  grid vname vsnap		snaps the view center to the nearest grid point\n");
}


/*
 * Note - this needs to be rewritten to accept keyword/value pairs so
 * that multiple attributes can be set with a single command call.
 */
int
ged_grid_core(struct ged *gedp, int argc, const char *argv[])
{
    char *command;
    char *parameter;
    char **argp = (char **)argv;
    double user_pt[3];		/* Value(s) provided by user */
    int i;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	grid_usage(gedp, argv[0]);
	return BRLCAD_OK;
    }

    if (argc < 2 || 5 < argc) {
	grid_usage(gedp, argv[0]);
	return BRLCAD_ERROR;
    }

    command = (char *)argv[0];
    parameter = (char *)argv[1];
    argc -= 2;
    argp += 2;

    for (i = 0; i < argc; ++i)
	if (sscanf(argp[i], "%lf", &user_pt[i]) != 1) {
	    grid_usage(gedp, argv[0]);
	    return BRLCAD_ERROR;
	}

    // TODO - need more sophisticated grid drawing - when zoomed out too far
    // grid disappears.  Need to simply draw a coarse grid that aligns with the
    // finer grid under it.
    //
    // TODO - when zoomed in so close the nearest grid points are not visible,
    // should probably disable grid snapping automatically and assume the goal
    // is free-form movement at that scale, since there are no visible snapping
    // points to target...
    if (BU_STR_EQUAL(parameter, "draw")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_grid.draw);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_s->gv_grid.draw = 1;
	    else
		gedp->ged_gvp->gv_s->gv_grid.draw = 0;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s draw' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "vsnap")) {
	if (argc == 0) {
	    grid_vsnap(gedp);
	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s vsnap' command accepts no arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "snap")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_grid.snap);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_s->gv_grid.snap = 1;
	    else
		gedp->ged_gvp->gv_s->gv_grid.snap = 0;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s snap' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "rh")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g",
			  gedp->ged_gvp->gv_s->gv_grid.res_h * gedp->dbip->dbi_base2local);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    gedp->ged_gvp->gv_s->gv_grid.res_h = user_pt[X] * gedp->dbip->dbi_local2base;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s rh' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "rv")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g",
			  gedp->ged_gvp->gv_s->gv_grid.res_v * gedp->dbip->dbi_base2local);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    gedp->ged_gvp->gv_s->gv_grid.res_v = user_pt[X] * gedp->dbip->dbi_local2base;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s rv' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "mrh")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_grid.res_major_h);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    gedp->ged_gvp->gv_s->gv_grid.res_major_h = (int)user_pt[X];

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s mrh' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "mrv")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_grid.res_major_v);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    gedp->ged_gvp->gv_s->gv_grid.res_major_v = (int)user_pt[X];

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s mrv' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g %g %g",
			  gedp->ged_gvp->gv_s->gv_grid.anchor[X] * gedp->dbip->dbi_base2local,
			  gedp->ged_gvp->gv_s->gv_grid.anchor[Y] * gedp->dbip->dbi_base2local,
			  gedp->ged_gvp->gv_s->gv_grid.anchor[Z] * gedp->dbip->dbi_base2local);
	    return BRLCAD_OK;
	} else if (argc == 3) {
	    gedp->ged_gvp->gv_s->gv_grid.anchor[0] = user_pt[X] * gedp->dbip->dbi_local2base;
	    gedp->ged_gvp->gv_s->gv_grid.anchor[1] = user_pt[Y] * gedp->dbip->dbi_local2base;
	    gedp->ged_gvp->gv_s->gv_grid.anchor[2] = user_pt[Z] * gedp->dbip->dbi_local2base;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s anchor' command requires 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "color")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gvp->gv_s->gv_grid.color[X],
			  gedp->ged_gvp->gv_s->gv_grid.color[Y],
			  gedp->ged_gvp->gv_s->gv_grid.color[Z]);
	    return BRLCAD_OK;
	} else if (argc == 3) {
	    gedp->ged_gvp->gv_s->gv_grid.color[0] = (int)user_pt[X];
	    gedp->ged_gvp->gv_s->gv_grid.color[1] = (int)user_pt[Y];
	    gedp->ged_gvp->gv_s->gv_grid.color[2] = (int)user_pt[Z];

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s color' command requires 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "vars")) {
	grid_vls_print(gedp);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(parameter, "help")) {
	grid_usage(gedp, argv[0]);
	return BRLCAD_HELP;
    }

    bu_vls_printf(gedp->ged_result_str, "%s: unrecognized command '%s'\n", argv[0], command);
    grid_usage(gedp, argv[0]);

    return BRLCAD_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl grid_cmd_impl = {
    "grid",
    ged_grid_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd grid_cmd = { &grid_cmd_impl };
const struct ged_cmd *grid_cmds[] = { &grid_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  grid_cmds, 1 };

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
