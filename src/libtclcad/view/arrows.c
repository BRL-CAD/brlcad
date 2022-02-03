/*                       A R R O W S . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2022 United States Government as represented by
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
/** @addtogroup libtclcad */
/** @{ */
/** @file libtclcad/view/arrows.c
 *
 */
/** @} */

#include "common.h"
#include "bu/units.h"
#include "ged.h"
#include "tclcad.h"

/* Private headers */
#include "../tclcad_private.h"
#include "../view/view.h"

int
go_data_arrows(Tcl_Interp *interp,
	       struct ged *gedp,
	       struct bview *gdvp,
	       int argc,
	       const char *argv[],
	       const char *usage)
{
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc < 2 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    ret = to_data_arrows_func(interp, gedp, gdvp, argc, argv);
    if (ret & BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}


int
to_data_arrows(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    struct bview *gdvp;
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* shift the command name to argv[1] before calling to_data_arrows_func */
    argv[1] = argv[0];
    ret = to_data_arrows_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1);
    if (ret == BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}


int
to_data_arrows_func(Tcl_Interp *interp,
		    struct ged *gedp,
		    struct bview *gdvp,
		    int argc,
		    const char *argv[])
{
    struct bv_data_arrow_state *gdasp;

    if (argv[0][0] == 's')
	gdasp = &gdvp->gv_tcl.gv_sdata_arrows;
    else
	gdasp = &gdvp->gv_tcl.gv_data_arrows;

    if (BU_STR_EQUAL(argv[1], "draw")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->gdas_draw);
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int i;

	    if (bu_sscanf(argv[2], "%d", &i) != 1)
		goto bad;

	    if (i)
		gdasp->gdas_draw = 1;
	    else
		gdasp->gdas_draw = 0;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "color")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdasp->gdas_color));
	    return BRLCAD_OK;
	}

	if (argc == 5) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[2], "%d", &r) != 1 ||
		bu_sscanf(argv[3], "%d", &g) != 1 ||
		bu_sscanf(argv[4], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gdasp->gdas_color, r, g, b);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "line_width")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->gdas_line_width);
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int line_width;

	    if (bu_sscanf(argv[2], "%d", &line_width) != 1)
		goto bad;

	    gdasp->gdas_line_width = line_width;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "points")) {
	register int i;

	if (argc == 2) {
	    for (i = 0; i < gdasp->gdas_num_points; ++i) {
		bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ",
			      V3ARGS(gdasp->gdas_points[i]));
	    }
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int ac;
	    const char **av;

	    if (Tcl_SplitList(interp, argv[2], &ac, &av) != TCL_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(interp));
		return BRLCAD_ERROR;
	    }

	    if (ac % 2) {
		bu_vls_printf(gedp->ged_result_str, "%s: must be an even number of points", argv[0]);
		return BRLCAD_ERROR;
	    }

	    bu_free((void *)gdasp->gdas_points, "data points");
	    gdasp->gdas_points = (point_t *)0;
	    gdasp->gdas_num_points = 0;

	    /* Clear out data points */
	    if (ac < 1) {
		to_refresh_view(gdvp);
		Tcl_Free((char *)av);
		return BRLCAD_OK;
	    }

	    gdasp->gdas_num_points = ac;
	    gdasp->gdas_points = (point_t *)bu_calloc(ac, sizeof(point_t), "data points");
	    for (i = 0; i < ac; ++i) {
		double scan[ELEMENTS_PER_VECT];

		if (bu_sscanf(av[i], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {

		    bu_vls_printf(gedp->ged_result_str, "bad data point - %s\n", av[i]);

		    bu_free((void *)gdasp->gdas_points, "data points");
		    gdasp->gdas_points = (point_t *)0;
		    gdasp->gdas_num_points = 0;

		    to_refresh_view(gdvp);
		    Tcl_Free((char *)av);
		    return BRLCAD_ERROR;
		}
		/* convert double to fastf_t */
		VMOVE(gdasp->gdas_points[i], scan);
	    }

	    to_refresh_view(gdvp);
	    Tcl_Free((char *)av);
	    return BRLCAD_OK;
	}
    }

    if (BU_STR_EQUAL(argv[1], "tip_length")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->gdas_tip_length);
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int tip_length;

	    if (bu_sscanf(argv[2], "%d", &tip_length) != 1)
		goto bad;

	    gdasp->gdas_tip_length = tip_length;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "tip_width")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->gdas_tip_width);
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int tip_width;

	    if (bu_sscanf(argv[2], "%d", &tip_width) != 1)
		goto bad;

	    gdasp->gdas_tip_width = tip_width;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

bad:
    return BRLCAD_ERROR;
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
