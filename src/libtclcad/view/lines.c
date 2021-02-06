/*                          L I N E S . C
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
/** @file libtclcad/view/lines.c
 *
 */
/** @} */

#include "common.h"
#include "bu/units.h"
#include "ged.h"
#include "tclcad.h"

/* Private headers */
#include "../../libged/ged_private.h" /* for ged_view_data_lines */
#include "../tclcad_private.h"
#include "../view/view.h"

void
go_dm_draw_lines(struct dm *dmp, struct bview_data_line_state *gdlsp)
{
    int saveLineWidth;
    int saveLineStyle;

    if (gdlsp->gdls_num_points < 1)
	return;

    saveLineWidth = dm_get_linewidth(dmp);
    saveLineStyle = dm_get_linestyle(dmp);

    /* set color */
    (void)dm_set_fg(dmp,
		    gdlsp->gdls_color[0],
		    gdlsp->gdls_color[1],
		    gdlsp->gdls_color[2], 1, 1.0);

    /* set linewidth */
    (void)dm_set_line_attr(dmp, gdlsp->gdls_line_width, 0);  /* solid lines */

    (void)dm_draw_lines_3d(dmp,
			   gdlsp->gdls_num_points,
			   gdlsp->gdls_points, 0);

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}

int
go_data_lines(Tcl_Interp *UNUSED(interp),
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
	tgd->go_refresh_on = 0;
    }


    struct bview *btmp = gedp->ged_gvp;
    gedp->ged_gvp = gdvp;

    ret = ged_view_data_lines(gedp, argc, argv);

    gedp->ged_gvp = btmp;

    to_refresh_view(gdvp);
    if (ret & GED_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}


int
to_data_lines(struct ged *gedp,
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

    /* turn the argv into a ged command */
    argv[1] = argv[0];
    argv[0] = "view";

    struct bview *btmp = gedp->ged_gvp;
    gedp->ged_gvp = gdvp;

    ret = ged_view_func(gedp, argc, argv);

    gedp->ged_gvp = btmp;

    to_refresh_view(gdvp);

    if (ret == GED_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
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
