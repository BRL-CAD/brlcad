/*                          R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2020 United States Government as represented by
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
/** @file mged/rect.c
 *
 * Routines to implement MGED's rubber band rectangle capability.
 *
 */

#include "common.h"

#include <math.h>

#include "vmath.h"
#include "ged.h"
#include "dm.h"
#include "./mged.h"
#include "./mged_dm.h"

extern void mged_center(fastf_t *center); /* from chgview.c */
extern int mged_vscale(fastf_t sfactor);

static void adjust_rect_for_zoom(void);

struct _rubber_band default_rubber_band = {
    /* rb_rc */		1,
    /* rb_active */	0,
    /* rb_draw */	0,
    /* rb_linewidth */	0,
    /* rb_linestyle */	's',
    /* rb_pos */	{ 0, 0 },
    /* rb_dim */	{ 0, 0 },
    /* rb_x */		0.0,
    /* rb_y */		0.0,
    /* rb_width */	0.0,
    /* rb_height */	0.0
};


#define RB_O(_m) bu_offsetof(struct _rubber_band, _m)
struct bu_structparse rubber_band_vparse[] = {
    {"%d",	1, "draw",	RB_O(rb_draw),		rb_set_dirty_flag, NULL, NULL },
    {"%d",	1, "linewidth",	RB_O(rb_linewidth),	rb_set_dirty_flag, NULL, NULL },
    {"%c",	1, "linestyle",	RB_O(rb_linestyle),	rb_set_dirty_flag, NULL, NULL },
    {"%d",	2, "pos",	RB_O(rb_pos),		set_rect, NULL, NULL },
    {"%d",	2, "dim",	RB_O(rb_dim),		set_rect, NULL, NULL },
    {"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


void
rb_set_dirty_flag(const struct bu_structparse *UNUSED(sdp),
		  const char *UNUSED(name),
		  void *UNUSED(base),
		  const char *UNUSED(value),
		  void *UNUSED(data))
{
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (m_dmp->dm_rubber_band == rubber_band) {
	    m_dmp->dm_dirty = 1;
	    dm_set_dirty(m_dmp->dm_dmp, 1);
	}
    }
}


/*
 * Given position and dimensions in normalized view coordinates, calculate
 * position and dimensions in image coordinates.
 */
void
rect_view2image(void)
{
    int width;
    width = dm_get_width(DMP);
    rubber_band->rb_pos[X] = dm_Normal2Xx(DMP, rubber_band->rb_x);
    rubber_band->rb_pos[Y] = dm_get_height(DMP) - dm_Normal2Xy(DMP, rubber_band->rb_y, 1);
    rubber_band->rb_dim[X] = rubber_band->rb_width * (fastf_t)width * 0.5;
    rubber_band->rb_dim[Y] = rubber_band->rb_height * (fastf_t)width * 0.5;
}


/*
 * Given position and dimensions in image coordinates, calculate
 * position and dimensions in normalized view coordinates.
 */
void
rect_image2view(void)
{
    int width = dm_get_width(DMP);
    rubber_band->rb_x = dm_Xx2Normal(DMP, rubber_band->rb_pos[X]);
    rubber_band->rb_y = dm_Xy2Normal(DMP, dm_get_height(DMP) - rubber_band->rb_pos[Y], 1);
    rubber_band->rb_width = rubber_band->rb_dim[X] * 2.0 / (fastf_t)width;
    rubber_band->rb_height = rubber_band->rb_dim[Y] * 2.0 / (fastf_t)width;
}


void
set_rect(const struct bu_structparse *sdp,
	 const char *name,
	 void *base,
	 const char *value,
	 void *data)
{
    rect_image2view();
    rb_set_dirty_flag(sdp, name, base, value, data);
}


/*
 * Adjust the rubber band to have the same aspect ratio as the window.
 */
static void
adjust_rect_for_zoom(void)
{
    fastf_t width, height;

    if (rubber_band->rb_width >= 0.0)
	width = rubber_band->rb_width;
    else
	width = -rubber_band->rb_width;

    if (rubber_band->rb_height >= 0.0)
	height = rubber_band->rb_height;
    else
	height = -rubber_band->rb_height;

    if (width >= height) {
	if (rubber_band->rb_height >= 0.0)
	    rubber_band->rb_height = width / dm_get_aspect(DMP);
	else
	    rubber_band->rb_height = -width / dm_get_aspect(DMP);
    } else {
	if (rubber_band->rb_width >= 0.0)
	    rubber_band->rb_width = height * dm_get_aspect(DMP);
	else
	    rubber_band->rb_width = -height * dm_get_aspect(DMP);
    }
}


void
draw_rect(void)
{
    int line_style;

    if (ZERO(rubber_band->rb_width) &&
	ZERO(rubber_band->rb_height))
	return;

    if (rubber_band->rb_linestyle == 'd')
	line_style = 1; /* dashed lines */
    else
	line_style = 0; /* solid lines */

    if (rubber_band->rb_active && mged_variables->mv_mouse_behavior == 'z')
	adjust_rect_for_zoom();

    /* draw rectangle */
    dm_set_fg(DMP,
		   color_scheme->cs_rubber_band[0],
		   color_scheme->cs_rubber_band[1],
		   color_scheme->cs_rubber_band[2], 1, 1.0);
    dm_set_line_attr(DMP, rubber_band->rb_linewidth, line_style);

    dm_draw_line_2d(DMP,
		    rubber_band->rb_x,
		    rubber_band->rb_y * dm_get_aspect(DMP),
		    rubber_band->rb_x,
		    (rubber_band->rb_y + rubber_band->rb_height) * dm_get_aspect(DMP));
    dm_draw_line_2d(DMP,
		    rubber_band->rb_x,
		    (rubber_band->rb_y + rubber_band->rb_height) * dm_get_aspect(DMP),
		    rubber_band->rb_x + rubber_band->rb_width,
		    (rubber_band->rb_y + rubber_band->rb_height) * dm_get_aspect(DMP));
    dm_draw_line_2d(DMP,
		    rubber_band->rb_x + rubber_band->rb_width,
		    (rubber_band->rb_y + rubber_band->rb_height) * dm_get_aspect(DMP),
		    rubber_band->rb_x + rubber_band->rb_width,
		    rubber_band->rb_y * dm_get_aspect(DMP));
    dm_draw_line_2d(DMP,
		    rubber_band->rb_x + rubber_band->rb_width,
		    rubber_band->rb_y * dm_get_aspect(DMP),
		    rubber_band->rb_x,
		    rubber_band->rb_y * dm_get_aspect(DMP));
}


void
paint_rect_area(void)
{
    if (!fbp)
	return;

    (void)fb_refresh(fbp, rubber_band->rb_pos[X], rubber_band->rb_pos[Y],
		     rubber_band->rb_dim[X], rubber_band->rb_dim[Y]);
}


void
rt_rect_area(void)
{
    int xmin, xmax;
    int ymin, ymax;
    int width, height;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (!fbp)
	return;

    if (ZERO(rubber_band->rb_width) &&
	ZERO(rubber_band->rb_height))
	return;

    if (mged_variables->mv_port < 0) {
	bu_log("rt_rect_area: invalid port number - %d\n", mged_variables->mv_port);
	return;
    }

    xmin = rubber_band->rb_pos[X];
    ymin = rubber_band->rb_pos[Y];
    width = rubber_band->rb_dim[X];
    height = rubber_band->rb_dim[Y];

    if (width >= 0) {
	xmax = xmin + width;
    } else {
	xmax = xmin;
	xmin += width;
    }

    if (height >= 0) {
	ymax = ymin + height;
    } else {
	ymax = ymin;
	ymin += height;
    }

    bu_vls_printf(&vls, "rt -w %d -n %d -V %lf -F %d -j %d,%d,%d,%d -C%d/%d/%d",
		  dm_get_width(DMP), dm_get_height(DMP), dm_get_aspect(DMP),
		  mged_variables->mv_port, xmin, ymin, xmax, ymax,
		  color_scheme->cs_bg[0], color_scheme->cs_bg[1], color_scheme->cs_bg[2]);
    (void)Tcl_Eval(INTERP, bu_vls_addr(&vls));
    (void)Tcl_ResetResult(INTERP);
    bu_vls_free(&vls);
}


void
zoom_rect_area(void)
{
    fastf_t width, height;
    fastf_t sf;
    point_t old_model_center;
    point_t new_model_center;
    point_t old_view_center;
    point_t new_view_center;

    if (ZERO(rubber_band->rb_width) &&
	ZERO(rubber_band->rb_height))
	return;

    adjust_rect_for_zoom();

    /* find old view center */
    MAT_DELTAS_GET_NEG(old_model_center, view_state->vs_gvp->gv_center);
    MAT4X3PNT(old_view_center, view_state->vs_gvp->gv_model2view, old_model_center);

    /* calculate new view center */
    VSET(new_view_center,
	 rubber_band->rb_x + rubber_band->rb_width / 2.0,
	 rubber_band->rb_y + rubber_band->rb_height / 2.0,
	 old_view_center[Z]);

    /* find new model center */
    MAT4X3PNT(new_model_center, view_state->vs_gvp->gv_view2model, new_view_center);
    mged_center(new_model_center);

    /* zoom in to fill rectangle */
    if (rubber_band->rb_width >= 0.0)
	width = rubber_band->rb_width;
    else
	width = -rubber_band->rb_width;

    if (rubber_band->rb_height >= 0.0)
	height = rubber_band->rb_height;
    else
	height = -rubber_band->rb_height;

    if (width >= height)
	sf = width / 2.0;
    else
	sf = height / 2.0 * dm_get_aspect(DMP);

    mged_vscale(sf);

    rubber_band->rb_x = -1.0;
    rubber_band->rb_y = -1.0 / dm_get_aspect(DMP);
    rubber_band->rb_width = 2.0;
    rubber_band->rb_height = 2.0 / dm_get_aspect(DMP);

    rect_view2image();

    {
	/* need dummy values for func signature--they are unused in the func */
	const struct bu_structparse *sdp = 0;
	const char name[] = "name";
	void *base = 0;
	const char value[] = "value";
	rb_set_dirty_flag(sdp, name, base, value, NULL);
    }
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
