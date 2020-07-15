/*                       T C L C A D _ V I E W S . C
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
/** @file libtclcad/tclcad_views.c
 *
 * A quasi-object-oriented database interface.
 *
 * A GED object contains the attributes and methods for controlling a
 * BRL-CAD geometry edit object.
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


struct path_match_data {
    struct db_full_path *s_fpath;
    struct db_i *dbip;
};


fastf_t
screen_to_view_x(struct dm *dmp, fastf_t x)
{
    int width = dm_get_width(dmp);
    return x / (fastf_t)width * 2.0 - 1.0;
}


fastf_t
screen_to_view_y(struct dm *dmp, fastf_t y)
{
    int height = dm_get_height(dmp);
    return (y / (fastf_t)height * -2.0 + 1.0) / dm_get_aspect(dmp);
}


int
to_is_viewable(struct ged_dm_view *gdvp)
{
    Tcl_Obj *our_result;
    Tcl_Obj *saved_result;
    int result_int;
    const char *pathname = bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp));

    /* stash any existing result so we can inspect our own */
    saved_result = Tcl_GetObjResult(current_top->to_interp);
    Tcl_IncrRefCount(saved_result);

    if (pathname && tclcad_eval(current_top->to_interp, "winfo viewable", 1, &pathname) != TCL_OK) {
	return 0;
    }

    our_result = Tcl_GetObjResult(current_top->to_interp);
    Tcl_GetIntFromObj(current_top->to_interp, our_result, &result_int);

    /* restore previous result */
    Tcl_SetObjResult(current_top->to_interp, saved_result);
    Tcl_DecrRefCount(saved_result);

    if (!result_int) {
	return 0;
    }

    return 1;
}

static void
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

static struct bu_hash_entry *
key_matches_paths(struct bu_hash_tbl *t, void *udata)
{
    struct path_match_data *data = (struct path_match_data *)udata;
    struct db_full_path entry_fpath;
    uint8_t *key;
    char *path_string;
    struct bu_hash_entry *entry = bu_hash_next(t, NULL);

    while (entry) {
	(void)bu_hash_key(entry, &key, NULL);
	path_string = (char *)key;
	if (db_string_to_path(&entry_fpath, data->dbip, path_string) < 0) {
	    continue;
	}

	if (db_full_path_match_top(&entry_fpath, data->s_fpath)) {
	    db_free_full_path(&entry_fpath);
	    return entry;
	}

	db_free_full_path(&entry_fpath);
	entry = bu_hash_next(t, entry);
    }

    return NULL;
}

static void
go_draw_solid(struct ged_dm_view *gdvp, struct solid *sp)
{
    struct ged_obj *gop = gdvp->gdv_gop;
    struct dm *dmp = gdvp->gdv_dmp;
    struct bu_hash_entry *entry;
    struct path_edit_params *params = NULL;
    mat_t save_mat, edit_model2view;
    struct path_match_data data;

    data.s_fpath = &sp->s_fullpath;
    data.dbip = gop->go_gedp->ged_wdbp->dbip;
    entry = key_matches_paths(gop->go_edited_paths, &data);

    if (entry != NULL) {
	params = (struct path_edit_params *)bu_hash_value(entry, NULL);
    }
    if (params) {
	MAT_COPY(save_mat, gdvp->gdv_view->gv_model2view);
	bn_mat_mul(edit_model2view, gdvp->gdv_view->gv_model2view, params->edit_mat);
	dm_loadmatrix(dmp, edit_model2view, 0);
    }

    if (gop->go_dlist_on) {
	dm_draw_dlist(dmp, sp->s_dlist);
    } else {
	if (sp->s_iflag == UP)
	    (void)dm_set_fg(dmp, 255, 255, 255, 0, sp->s_transparency);
	else
	    (void)dm_set_fg(dmp,
			    (unsigned char)sp->s_color[0],
			    (unsigned char)sp->s_color[1],
			    (unsigned char)sp->s_color[2], 0, sp->s_transparency);

	if (sp->s_hiddenLine) {
	    (void)dm_draw_vlist_hidden_line(dmp, (struct bn_vlist *)&sp->s_vlist);
	} else {
	    (void)dm_draw_vlist(dmp, (struct bn_vlist *)&sp->s_vlist);
	}
    }
    if (params) {
	dm_loadmatrix(dmp, save_mat, 0);
    }
}

/* Draw all display lists */
static int
go_draw_dlist(struct ged_dm_view *gdvp)
{
    register struct display_list *gdlp;
    register struct display_list *next_gdlp;
    struct solid *sp;
    int line_style = -1;
    struct dm *dmp = gdvp->gdv_dmp;
    struct bu_list *hdlp = gdvp->gdv_gop->go_gedp->ged_gdp->gd_headDisplay;

    if (dm_get_transparency(dmp)) {
	/* First, draw opaque stuff */
	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		if (sp->s_transparency < 1.0)
		    continue;

		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    (void)dm_set_line_attr(dmp, dm_get_linewidth(dmp), line_style);
		}

		go_draw_solid(gdvp, sp);
	    }

	    gdlp = next_gdlp;
	}

	/* disable write to depth buffer */
	(void)dm_set_depth_mask(dmp, 0);

	/* Second, draw transparent stuff */
	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		/* already drawn above */
		if (ZERO(sp->s_transparency - 1.0))
		    continue;

		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    (void)dm_set_line_attr(dmp, dm_get_linewidth(dmp), line_style);
		}

		go_draw_solid(gdvp, sp);
	    }

	    gdlp = next_gdlp;
	}

	/* re-enable write to depth buffer */
	(void)dm_set_depth_mask(dmp, 1);
    } else {
	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    (void)dm_set_line_attr(dmp, dm_get_linewidth(dmp), line_style);
		}

		go_draw_solid(gdvp, sp);
	    }

	    gdlp = next_gdlp;
	}
    }

    return GED_OK;
}

static void
go_draw(struct ged_dm_view *gdvp)
{
    (void)dm_loadmatrix(gdvp->gdv_dmp, gdvp->gdv_view->gv_model2view, 0);

    if (SMALL_FASTF < gdvp->gdv_view->gv_perspective)
	(void)dm_loadpmatrix(gdvp->gdv_dmp, gdvp->gdv_view->gv_pmat);
    else
	(void)dm_loadpmatrix(gdvp->gdv_dmp, (fastf_t *)NULL);

    go_draw_dlist(gdvp);
}


#define GO_DM_DRAW_POLY(_dmp, _gdpsp, _i, _last_poly, _mode) {	\
	size_t _j; \
	\
	/* set color */ \
	(void)dm_set_fg((_dmp), \
			(_gdpsp)->gdps_polygons.polygon[_i].gp_color[0], \
			(_gdpsp)->gdps_polygons.polygon[_i].gp_color[1], \
			(_gdpsp)->gdps_polygons.polygon[_i].gp_color[2], \
			1, 1.0);					\
	\
	for (_j = 0; _j < (_gdpsp)->gdps_polygons.polygon[_i].num_contours; ++_j) { \
	    size_t _last = (_gdpsp)->gdps_polygons.polygon[_i].contour[_j].num_points-1; \
	    int _line_style; \
	    \
	    /* always draw holes using segmented lines */ \
	    if ((_gdpsp)->gdps_polygons.polygon[_i].hole[_j]) { \
		_line_style = 1; \
	    } else { \
		_line_style = (_gdpsp)->gdps_polygons.polygon[_i].gp_line_style; \
	    } \
	    \
	    /* set the linewidth and linestyle for polygon i, contour j */	\
	    (void)dm_set_line_attr((_dmp), \
				   (_gdpsp)->gdps_polygons.polygon[_i].gp_line_width, \
				   _line_style); \
	    \
	    (void)dm_draw_lines_3d((_dmp),				\
				   (_gdpsp)->gdps_polygons.polygon[_i].contour[_j].num_points, \
				   (_gdpsp)->gdps_polygons.polygon[_i].contour[_j].point, 1); \
	    \
	    if (_mode != TCLCAD_POLY_CONTOUR_MODE || _i != _last_poly || (_gdpsp)->gdps_cflag == 0) { \
		(void)dm_draw_line_3d((_dmp),				\
				      (_gdpsp)->gdps_polygons.polygon[_i].contour[_j].point[_last], \
				      (_gdpsp)->gdps_polygons.polygon[_i].contour[_j].point[0]); \
	    } \
	}}


static void
go_dm_draw_polys(struct dm *dmp, bview_data_polygon_state *gdpsp, int mode)
{
    register size_t i, last_poly;
    int saveLineWidth;
    int saveLineStyle;

    if (gdpsp->gdps_polygons.num_polygons < 1)
	return;

    saveLineWidth = dm_get_linewidth(dmp);
    saveLineStyle = dm_get_linestyle(dmp);

    last_poly = gdpsp->gdps_polygons.num_polygons - 1;
    for (i = 0; i < gdpsp->gdps_polygons.num_polygons; ++i) {
	if (i == gdpsp->gdps_target_polygon_i)
	    continue;

	GO_DM_DRAW_POLY(dmp, gdpsp, i, last_poly, mode);
    }

    /* draw the target poly last */
    GO_DM_DRAW_POLY(dmp, gdpsp, gdpsp->gdps_target_polygon_i, last_poly, mode);

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}

static void
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

static void
go_dm_draw_labels(struct dm *dmp, struct bview_data_label_state *gdlsp, matp_t m2vmat)
{
    register int i;

    /* set color */
    (void)dm_set_fg(dmp,
		    gdlsp->gdls_color[0],
		    gdlsp->gdls_color[1],
		    gdlsp->gdls_color[2], 1, 1.0);

    for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
	point_t vpoint;

	MAT4X3PNT(vpoint, m2vmat,
		  gdlsp->gdls_points[i]);
	(void)dm_draw_string_2d(dmp, gdlsp->gdls_labels[i],
				vpoint[X], vpoint[Y], 0, 1);
    }
}

static void
go_draw_other(struct ged_obj *gop, struct ged_dm_view *gdvp)
{
    int width = dm_get_width(gdvp->gdv_dmp);
    fastf_t sf = (fastf_t)(gdvp->gdv_view->gv_size) / (fastf_t)width;

    if (gdvp->gdv_view->gv_data_arrows.gdas_draw)
	go_dm_draw_arrows(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_arrows, sf);

    if (gdvp->gdv_view->gv_sdata_arrows.gdas_draw)
	go_dm_draw_arrows(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_arrows, sf);

    if (gdvp->gdv_view->gv_data_axes.draw)
	dm_draw_data_axes(gdvp->gdv_dmp,
			  sf,
			  &gdvp->gdv_view->gv_data_axes);

    if (gdvp->gdv_view->gv_sdata_axes.draw)
	dm_draw_data_axes(gdvp->gdv_dmp,
			  sf,
			  &gdvp->gdv_view->gv_sdata_axes);

    if (gdvp->gdv_view->gv_data_lines.gdls_draw)
	go_dm_draw_lines(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_lines);

    if (gdvp->gdv_view->gv_sdata_lines.gdls_draw)
	go_dm_draw_lines(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_lines);

    if (gdvp->gdv_view->gv_data_polygons.gdps_draw)
	go_dm_draw_polys(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_polygons, gdvp->gdv_view->gv_mode);

    if (gdvp->gdv_view->gv_sdata_polygons.gdps_draw)
	go_dm_draw_polys(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_polygons, gdvp->gdv_view->gv_mode);

    /* Restore to non-rotated, full brightness */
    (void)dm_normal(gdvp->gdv_dmp);
    go_draw_faceplate(gop, gdvp);

    if (gdvp->gdv_view->gv_data_labels.gdls_draw)
	go_dm_draw_labels(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_labels, gdvp->gdv_view->gv_model2view);

    if (gdvp->gdv_view->gv_sdata_labels.gdls_draw)
	go_dm_draw_labels(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_labels, gdvp->gdv_view->gv_model2view);

    /* Draw labels */
    if (gdvp->gdv_view->gv_prim_labels.gos_draw) {
	register int i;

	for (i = 0; i < gop->go_prim_label_list_size; ++i) {
	    dm_draw_labels(gdvp->gdv_dmp,
			   gop->go_gedp->ged_wdbp,
			   bu_vls_addr(&gop->go_prim_label_list[i]),
			   gdvp->gdv_view->gv_model2view,
			   gdvp->gdv_view->gv_prim_labels.gos_text_color,
			   NULL, NULL);
	}
    }
}

void
go_refresh_draw(struct ged_obj *gop, struct ged_dm_view *gdvp, int restore_zbuffer)
{
    if (gdvp->gdv_fbs.fbs_mode == TCLCAD_OBJ_FB_MODE_OVERLAY) {
	if (gdvp->gdv_view->gv_rect.draw) {
	    go_draw(gdvp);

	    go_draw_other(gop, gdvp);

	    /* disable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 0);

	    fb_refresh(gdvp->gdv_fbs.fbs_fbp,
		       gdvp->gdv_view->gv_rect.pos[X], gdvp->gdv_view->gv_rect.pos[Y],
		       gdvp->gdv_view->gv_rect.dim[X], gdvp->gdv_view->gv_rect.dim[Y]);

	    /* enable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 1);

	    if (gdvp->gdv_view->gv_rect.line_width)
		dm_draw_rect(gdvp->gdv_dmp, &gdvp->gdv_view->gv_rect);
	} else {
	    /* disable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 0);

	    fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
		       dm_get_width(gdvp->gdv_dmp), dm_get_height(gdvp->gdv_dmp));

	    /* enable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 1);
	}

	if (restore_zbuffer) {
	    (void)dm_set_zbuffer(gdvp->gdv_dmp, 1);
	}

	return;
    } else if (gdvp->gdv_fbs.fbs_mode == TCLCAD_OBJ_FB_MODE_INTERLAY) {
	go_draw(gdvp);

	/* disable write to depth buffer */
	(void)dm_set_depth_mask(gdvp->gdv_dmp, 0);

	if (gdvp->gdv_view->gv_rect.draw) {
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp,
		       gdvp->gdv_view->gv_rect.pos[X], gdvp->gdv_view->gv_rect.pos[Y],
		       gdvp->gdv_view->gv_rect.dim[X], gdvp->gdv_view->gv_rect.dim[Y]);
	} else
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
		       dm_get_width(gdvp->gdv_dmp), dm_get_height(gdvp->gdv_dmp));

	/* enable write to depth buffer */
	(void)dm_set_depth_mask(gdvp->gdv_dmp, 1);

	if (restore_zbuffer) {
	    (void)dm_set_zbuffer(gdvp->gdv_dmp, 1);
	}
    } else {
	if (gdvp->gdv_fbs.fbs_mode == TCLCAD_OBJ_FB_MODE_UNDERLAY) {
	    /* disable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 0);

	    if (gdvp->gdv_view->gv_rect.draw) {
		fb_refresh(gdvp->gdv_fbs.fbs_fbp,
			   gdvp->gdv_view->gv_rect.pos[X], gdvp->gdv_view->gv_rect.pos[Y],
			   gdvp->gdv_view->gv_rect.dim[X], gdvp->gdv_view->gv_rect.dim[Y]);
	    } else
		fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
			   dm_get_width(gdvp->gdv_dmp), dm_get_height(gdvp->gdv_dmp));

	    /* enable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 1);
	}

	if (restore_zbuffer) {
	    (void)dm_set_zbuffer(gdvp->gdv_dmp, 1);
	}

	go_draw(gdvp);
    }

    go_draw_other(gop, gdvp);
}

void
go_refresh(struct ged_obj *gop, struct ged_dm_view *gdvp)
{
    int restore_zbuffer = 0;

    /* Turn off the zbuffer if the framebuffer is active AND the zbuffer is on. */
    if (gdvp->gdv_fbs.fbs_mode != TCLCAD_OBJ_FB_MODE_OFF && dm_get_zbuffer(gdvp->gdv_dmp)) {
	(void)dm_set_zbuffer(gdvp->gdv_dmp, 0);
	restore_zbuffer = 1;
    }

    (void)dm_draw_begin(gdvp->gdv_dmp);
    go_refresh_draw(gop, gdvp, restore_zbuffer);
    (void)dm_draw_end(gdvp->gdv_dmp);
}

void
to_refresh_view(struct ged_dm_view *gdvp)
{
    if (current_top == NULL || !current_top->to_gop->go_refresh_on)
	return;

    if (to_is_viewable(gdvp))
	go_refresh(current_top->to_gop, gdvp);
}

void
to_refresh_all_views(struct tclcad_obj *top)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &top->to_gop->go_head_views.l)) {
	to_refresh_view(gdvp);
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
