/*                           A X E S . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2021 United States Government as represented by
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
/** @file libtclcad/view/axes.c
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
to_axes(struct ged *gedp,
	struct bview *gdvp,
	struct bview_axes *gasp,
	int argc,
	const char *argv[],
	const char *usage)
{

    if (BU_STR_EQUAL(argv[2], "draw")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->draw);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->draw = 1;
	    else
		gasp->draw = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "axes_size")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%lf", gasp->axes_size);
	    return GED_OK;
	}

	if (argc == 4) {
	    double size; /* must be double for scanf */

	    if (bu_sscanf(argv[3], "%lf", &size) != 1)
		goto bad;

	    gasp->axes_size = size;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "axes_pos")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%lf %lf %lf",
			  V3ARGS(gasp->axes_pos));
	    return GED_OK;
	}

	if (argc == 6) {
	    double x, y, z; /* must be double for scanf */

	    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
		bu_sscanf(argv[4], "%lf", &y) != 1 ||
		bu_sscanf(argv[5], "%lf", &z) != 1)
		goto bad;

	    VSET(gasp->axes_pos, x, y, z);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "axes_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->axes_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->axes_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "label_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->label_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->label_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "line_width")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->line_width);
	    return GED_OK;
	}

	if (argc == 4) {
	    int line_width;

	    if (bu_sscanf(argv[3], "%d", &line_width) != 1)
		goto bad;

	    gasp->line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "pos_only")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->pos_only);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->pos_only = 1;
	    else
		gasp->pos_only = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->tick_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->tick_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_enable")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_enabled);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->tick_enabled = 1;
	    else
		gasp->tick_enabled = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_interval")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%f", gasp->tick_interval);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tick_interval;

	    if (bu_sscanf(argv[3], "%d", &tick_interval) != 1)
		goto bad;

	    gasp->tick_interval = tick_interval;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_length")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_length);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tick_length;

	    if (bu_sscanf(argv[3], "%d", &tick_length) != 1)
		goto bad;

	    gasp->tick_length = tick_length;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_major_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->tick_major_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->tick_major_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_major_length")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_major_length);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tick_major_length;

	    if (bu_sscanf(argv[3], "%d", &tick_major_length) != 1)
		goto bad;

	    gasp->tick_major_length = tick_major_length;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "ticks_per_major")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->ticks_per_major);
	    return GED_OK;
	}

	if (argc == 4) {
	    int ticks_per_major;

	    if (bu_sscanf(argv[3], "%d", &ticks_per_major) != 1)
		goto bad;

	    gasp->ticks_per_major = ticks_per_major;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_threshold")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_threshold);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tick_threshold;

	    if (bu_sscanf(argv[3], "%d", &tick_threshold) != 1)
		goto bad;

	    if (tick_threshold < 1)
		tick_threshold = 1;

	    gasp->tick_threshold = tick_threshold;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "triple_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->triple_color);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->triple_color = 1;
	    else
		gasp->triple_color = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}

int
go_data_axes(Tcl_Interp *interp,
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
	return GED_HELP;
    }

    if (argc < 2 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    ret = to_data_axes_func(interp, gedp, gdvp, argc, argv);
    if (ret & GED_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}

int
to_data_axes(struct ged *gedp,
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
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_data_axes_func */
    argv[1] = argv[0];
    ret = to_data_axes_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1);
    if (ret == GED_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}

int
to_data_axes_func(Tcl_Interp *interp,
		  struct ged *gedp,
		  struct bview *gdvp,
		  int argc,
		  const char *argv[])
{
    struct bview_data_axes_state *gdasp;

    if (argv[0][0] == 's')
	gdasp = &gdvp->gv_tcl.gv_sdata_axes;
    else
	gdasp = &gdvp->gv_tcl.gv_data_axes;

    if (BU_STR_EQUAL(argv[1], "draw")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->draw);
	    return GED_OK;
	}

	if (argc == 3) {
	    int i;

	    if (bu_sscanf(argv[2], "%d", &i) != 1)
		goto bad;

	    if (0 <= i && i <= 2)
		gdasp->draw = i;
	    else
		gdasp->draw = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "color")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdasp->color));
	    return GED_OK;
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

	    VSET(gdasp->color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "line_width")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->line_width);
	    return GED_OK;
	}

	if (argc == 3) {
	    int line_width;

	    if (bu_sscanf(argv[2], "%d", &line_width) != 1)
		goto bad;

	    gdasp->line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "size")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%lf", gdasp->size);
	    return GED_OK;
	}

	if (argc == 3) {
	    double size; /* must be double for scanf */

	    if (bu_sscanf(argv[2], "%lf", &size) != 1)
		goto bad;

	    gdasp->size = size;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "points")) {
	register int i;

	if (argc == 2) {
	    for (i = 0; i < gdasp->num_points; ++i) {
		bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ",
			      V3ARGS(gdasp->points[i]));
	    }
	    return GED_OK;
	}

	if (argc == 3) {
	    int ac;
	    const char **av;

	    if (Tcl_SplitList(interp, argv[2], &ac, &av) != TCL_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(interp));
		return GED_ERROR;
	    }

	    if (gdasp->num_points) {
		bu_free((void *)gdasp->points, "data points");
		gdasp->points = (point_t *)0;
		gdasp->num_points = 0;
	    }

	    /* Clear out data points */
	    if (ac < 1) {
		to_refresh_view(gdvp);
		Tcl_Free((char *)av);
		return GED_OK;
	    }

	    gdasp->num_points = ac;
	    gdasp->points = (point_t *)bu_calloc(ac, sizeof(point_t), "data points");
	    for (i = 0; i < ac; ++i) {
		double scan[3];

		if (bu_sscanf(av[i], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "bad data point - %s\n", av[i]);

		    bu_free((void *)gdasp->points, "data points");
		    gdasp->points = (point_t *)0;
		    gdasp->num_points = 0;

		    to_refresh_view(gdvp);
		    Tcl_Free((char *)av);
		    return GED_ERROR;
		}
		/* convert double to fastf_t */
		VMOVE(gdasp->points[i], scan);
	    }

	    to_refresh_view(gdvp);
	    Tcl_Free((char *)av);
	    return GED_OK;
	}
    }

bad:
    return GED_ERROR;
}

int
to_model_axes(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *usage,
	      int UNUSED(maxargs))
{
    struct bview *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
        bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
        return GED_ERROR;
    }

    return to_axes(gedp, gdvp, &gdvp->gv_model_axes, argc, argv, usage);
}

int
go_view_axes(struct ged *gedp,
	     struct bview *gdvp,
	     int argc,
	     const char *argv[],
	     const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    return to_axes(gedp, gdvp, &gdvp->gv_view_axes, argc, argv, usage);
}


int
to_view_axes(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    struct bview *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    return to_axes(gedp, gdvp, &gdvp->gv_view_axes, argc, argv, usage);
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
