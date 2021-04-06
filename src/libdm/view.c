/*                          V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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

#include "common.h"

#include "bu/units.h"
#include "bu/vls.h"
#include "bview/defines.h"
#include "dm/defines.h"
#include "dm.h"

void
dm_draw_faceplate(struct bview *v, double base2local, double local2base)
{
    /* Center dot */
    if (v->gv_center_dot.gos_draw) {
	(void)dm_set_fg((struct dm *)v->dmp,
			v->gv_center_dot.gos_line_color[0],
			v->gv_center_dot.gos_line_color[1],
			v->gv_center_dot.gos_line_color[2],
			1, 1.0);
	(void)dm_draw_point_2d((struct dm *)v->dmp, 0.0, 0.0);
    }

    /* Model axes */
    if (v->gv_model_axes.draw) {
	point_t map;
	point_t save_map;

	VMOVE(save_map, v->gv_model_axes.axes_pos);
	VSCALE(map, v->gv_model_axes.axes_pos, local2base);
	MAT4X3PNT(v->gv_model_axes.axes_pos, v->gv_model2view, map);

	dm_draw_axes((struct dm *)v->dmp,
		     v->gv_size,
		     v->gv_rotation,
		     &v->gv_model_axes);

	VMOVE(v->gv_model_axes.axes_pos, save_map);
    }

    /* View axes */
    if (v->gv_view_axes.draw) {
	int width, height;
	fastf_t inv_aspect;
	fastf_t save_ypos;

	save_ypos = v->gv_view_axes.axes_pos[Y];
	width = dm_get_width((struct dm *)v->dmp);
	height = dm_get_height((struct dm *)v->dmp);
	inv_aspect = (fastf_t)height / (fastf_t)width;
	v->gv_view_axes.axes_pos[Y] = save_ypos * inv_aspect;
	dm_draw_axes((struct dm *)v->dmp,
		     v->gv_size,
		     v->gv_rotation,
		     &v->gv_view_axes);

	v->gv_view_axes.axes_pos[Y] = save_ypos;
    }


    /* View scale */
    if (v->gv_view_scale.gos_draw)
	dm_draw_scale((struct dm *)v->dmp,
		      v->gv_size*base2local,
		      bu_units_string(1/base2local),
		      v->gv_view_scale.gos_line_color,
		      v->gv_view_params.gos_text_color);

    /* View parameters */
    if (v->gv_view_params.gos_draw) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	point_t center;
	char *ustr;

	MAT_DELTAS_GET_NEG(center, v->gv_center);
	VSCALE(center, center, base2local);

	ustr = (char *)bu_units_string(local2base);
	bu_vls_printf(&vls, "units:%s  size:%.2f  center:(%.2f, %.2f, %.2f) az:%.2f  el:%.2f  tw::%.2f",
		      ustr,
		      v->gv_size * base2local,
		      V3ARGS(center),
		      V3ARGS(v->gv_aet));
	(void)dm_set_fg((struct dm *)v->dmp,
			v->gv_view_params.gos_text_color[0],
			v->gv_view_params.gos_text_color[1],
			v->gv_view_params.gos_text_color[2],
			1, 1.0);
	(void)dm_draw_string_2d((struct dm *)v->dmp, bu_vls_addr(&vls), -0.98, -0.965, 10, 0);
	bu_vls_free(&vls);
    }

    /* Draw the angle distance cursor */
    if (v->gv_adc.draw)
	dm_draw_adc((struct dm *)v->dmp, &(v->gv_adc), v->gv_view2model, v->gv_model2view);

    /* Draw grid */
    if (v->gv_grid.draw) {
	dm_draw_grid((struct dm *)v->dmp, &v->gv_grid, v->gv_scale, v->gv_model2view, base2local);
    }

    /* Draw rect */
    if (v->gv_rect.draw && v->gv_rect.line_width)
	dm_draw_rect((struct dm *)v->dmp, &v->gv_rect);
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
