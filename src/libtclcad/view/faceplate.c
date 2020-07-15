/*                     F A C E P L A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2020 United States Government as represented by
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
/** @file libtclcad/faceplate.c
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
to_faceplate(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    int i;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 7 < argc)
	goto bad;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[2], "center_dot")) {
	if (BU_STR_EQUAL(argv[3], "draw")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_view->gv_center_dot.gos_draw);
		return GED_OK;
	    } else if (argc == 5) {
		if (bu_sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_center_dot.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_center_dot.gos_draw = 0;

		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	if (BU_STR_EQUAL(argv[3], "color")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_center_dot.gos_line_color));
		return GED_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (bu_sscanf(argv[4], "%d", &r) != 1 ||
		    bu_sscanf(argv[5], "%d", &g) != 1 ||
		    bu_sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_center_dot.gos_line_color, r, g, b);
		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "prim_labels")) {
	if (BU_STR_EQUAL(argv[3], "draw")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_view->gv_prim_labels.gos_draw);
		return GED_OK;
	    } else if (argc == 5) {
		if (bu_sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_prim_labels.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_prim_labels.gos_draw = 0;

		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	if (BU_STR_EQUAL(argv[3], "color")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_prim_labels.gos_text_color));
		return GED_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (bu_sscanf(argv[4], "%d", &r) != 1 ||
		    bu_sscanf(argv[5], "%d", &g) != 1 ||
		    bu_sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_prim_labels.gos_text_color, r, g, b);
		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "view_params")) {
	if (BU_STR_EQUAL(argv[3], "draw")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_view->gv_view_params.gos_draw);
		return GED_OK;
	    } else if (argc == 5) {
		if (bu_sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_view_params.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_view_params.gos_draw = 0;

		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	if (BU_STR_EQUAL(argv[3], "color")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_view_params.gos_text_color));
		return GED_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (bu_sscanf(argv[4], "%d", &r) != 1 ||
		    bu_sscanf(argv[5], "%d", &g) != 1 ||
		    bu_sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_view_params.gos_text_color, r, g, b);
		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "view_scale")) {
	if (BU_STR_EQUAL(argv[3], "draw")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_view->gv_view_scale.gos_draw);
		return GED_OK;
	    } else if (argc == 5) {
		if (bu_sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_view_scale.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_view_scale.gos_draw = 0;

		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	if (BU_STR_EQUAL(argv[3], "color")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_view_scale.gos_line_color));
		return GED_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (bu_sscanf(argv[4], "%d", &r) != 1 ||
		    bu_sscanf(argv[5], "%d", &g) != 1 ||
		    bu_sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_view_scale.gos_line_color, r, g, b);
		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	goto bad;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


void
go_draw_faceplate(struct ged_obj *gop, struct ged_dm_view *gdvp)
{
    /* Center dot */
    if (gdvp->gdv_view->gv_center_dot.gos_draw) {
	(void)dm_set_fg(gdvp->gdv_dmp,
			gdvp->gdv_view->gv_center_dot.gos_line_color[0],
			gdvp->gdv_view->gv_center_dot.gos_line_color[1],
			gdvp->gdv_view->gv_center_dot.gos_line_color[2],
			1, 1.0);
	(void)dm_draw_point_2d(gdvp->gdv_dmp, 0.0, 0.0);
    }

    /* Model axes */
    if (gdvp->gdv_view->gv_model_axes.draw) {
	point_t map;
	point_t save_map;

	VMOVE(save_map, gdvp->gdv_view->gv_model_axes.axes_pos);
	VSCALE(map, gdvp->gdv_view->gv_model_axes.axes_pos, gop->go_gedp->ged_wdbp->dbip->dbi_local2base);
	MAT4X3PNT(gdvp->gdv_view->gv_model_axes.axes_pos, gdvp->gdv_view->gv_model2view, map);

	dm_draw_axes(gdvp->gdv_dmp,
		     gdvp->gdv_view->gv_size,
		     gdvp->gdv_view->gv_rotation,
		     &gdvp->gdv_view->gv_model_axes);

	VMOVE(gdvp->gdv_view->gv_model_axes.axes_pos, save_map);
    }

    /* View axes */
    if (gdvp->gdv_view->gv_view_axes.draw) {
	int width, height;
	fastf_t inv_aspect;
	fastf_t save_ypos;

	save_ypos = gdvp->gdv_view->gv_view_axes.axes_pos[Y];
	width = dm_get_width(gdvp->gdv_dmp);
	height = dm_get_height(gdvp->gdv_dmp);
	inv_aspect = (fastf_t)height / (fastf_t)width;
	gdvp->gdv_view->gv_view_axes.axes_pos[Y] = save_ypos * inv_aspect;
	dm_draw_axes(gdvp->gdv_dmp,
		     gdvp->gdv_view->gv_size,
		     gdvp->gdv_view->gv_rotation,
		     &gdvp->gdv_view->gv_view_axes);

	gdvp->gdv_view->gv_view_axes.axes_pos[Y] = save_ypos;
    }


    /* View scale */
    if (gdvp->gdv_view->gv_view_scale.gos_draw)
	dm_draw_scale(gdvp->gdv_dmp,
		      gdvp->gdv_view->gv_size*gop->go_gedp->ged_wdbp->dbip->dbi_base2local,
		      bu_units_string(1/gop->go_gedp->ged_wdbp->dbip->dbi_base2local),
		      gdvp->gdv_view->gv_view_scale.gos_line_color,
		      gdvp->gdv_view->gv_view_params.gos_text_color);

    /* View parameters */
    if (gdvp->gdv_view->gv_view_params.gos_draw) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	point_t center;
	char *ustr;

	MAT_DELTAS_GET_NEG(center, gdvp->gdv_view->gv_center);
	VSCALE(center, center, gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

	ustr = (char *)bu_units_string(gop->go_gedp->ged_wdbp->dbip->dbi_local2base);
	bu_vls_printf(&vls, "units:%s  size:%.2f  center:(%.2f, %.2f, %.2f) az:%.2f  el:%.2f  tw::%.2f",
		      ustr,
		      gdvp->gdv_view->gv_size * gop->go_gedp->ged_wdbp->dbip->dbi_base2local,
		      V3ARGS(center),
		      V3ARGS(gdvp->gdv_view->gv_aet));
	(void)dm_set_fg(gdvp->gdv_dmp,
			gdvp->gdv_view->gv_view_params.gos_text_color[0],
			gdvp->gdv_view->gv_view_params.gos_text_color[1],
			gdvp->gdv_view->gv_view_params.gos_text_color[2],
			1, 1.0);
	(void)dm_draw_string_2d(gdvp->gdv_dmp, bu_vls_addr(&vls), -0.98, -0.965, 10, 0);
	bu_vls_free(&vls);
    }

    /* Draw the angle distance cursor */
    if (gdvp->gdv_view->gv_adc.draw)
	dm_draw_adc(gdvp->gdv_dmp, &(gdvp->gdv_view->gv_adc), gdvp->gdv_view->gv_view2model, gdvp->gdv_view->gv_model2view);

    /* Draw grid */
    if (gdvp->gdv_view->gv_grid.draw)
	dm_draw_grid(gdvp->gdv_dmp, &gdvp->gdv_view->gv_grid, gdvp->gdv_view->gv_scale, gdvp->gdv_view->gv_model2view, gdvp->gdv_gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

    /* Draw rect */
    if (gdvp->gdv_view->gv_rect.draw && gdvp->gdv_view->gv_rect.line_width)
	dm_draw_rect(gdvp->gdv_dmp, &gdvp->gdv_view->gv_rect);
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
