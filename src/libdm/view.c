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

#include "bu/time.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "bview/defines.h"
#include "bview/util.h"
#include "dm.h"

void
dm_draw_arrow(struct dm *dmp, point_t A, point_t B, fastf_t tip_length, fastf_t tip_width, fastf_t sf)
{
    point_t points[16];
    point_t BmA;
    point_t offset;
    point_t perp1, perp2;
    point_t a_base;
    point_t a_pt1, a_pt2, a_pt3, a_pt4;

    VSUB2(BmA, B, A);

    VUNITIZE(BmA);
    VSCALE(offset, BmA, -tip_length * sf);

    bn_vec_perp(perp1, BmA);
    VUNITIZE(perp1);

    VCROSS(perp2, BmA, perp1);
    VUNITIZE(perp2);

    VSCALE(perp1, perp1, tip_width * sf);
    VSCALE(perp2, perp2, tip_width * sf);

    VADD2(a_base, B, offset);
    VADD2(a_pt1, a_base, perp1);
    VADD2(a_pt2, a_base, perp2);
    VSUB2(a_pt3, a_base, perp1);
    VSUB2(a_pt4, a_base, perp2);

    VMOVE(points[0], B);
    VMOVE(points[1], a_pt1);
    VMOVE(points[2], B);
    VMOVE(points[3], a_pt2);
    VMOVE(points[4], B);
    VMOVE(points[5], a_pt3);
    VMOVE(points[6], B);
    VMOVE(points[7], a_pt4);
    VMOVE(points[8], a_pt1);
    VMOVE(points[9], a_pt2);
    VMOVE(points[10], a_pt2);
    VMOVE(points[11], a_pt3);
    VMOVE(points[12], a_pt3);
    VMOVE(points[13], a_pt4);
    VMOVE(points[14], a_pt4);
    VMOVE(points[15], a_pt1);

    (void)dm_draw_lines_3d(dmp, 16, points, 0);
}

// Draw an arrow head for each MOVE+LAST_DRAW paring
void
dm_add_arrows(struct dm *dmp, struct bview_scene_obj *s)
{
    struct bn_vlist *vp = (struct bn_vlist *)&s->s_vlist;
    struct bn_vlist *tvp;
    point_t A, B;
    int pcnt = 0;
    if (!s->s_arrow)
	return;
    if (NEAR_ZERO(s->s_arrow_tip_length, SMALL_FASTF) || NEAR_ZERO(s->s_arrow_tip_width, SMALL_FASTF))
       return;	
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (int i = 0; i < nused; i++, cmd++, pt++) {
	    pcnt++;
	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    if (pcnt > 1) {
			// We have a move and more than one point - add an arrow
			// to the A -> B segment at B
			dm_draw_arrow(dmp, A, B, s->s_arrow_tip_length, s->s_arrow_tip_width, 1.0);
		    }
		    VMOVE(B,*pt);
		    break;
		case BN_VLIST_LINE_DRAW:
		    VMOVE(A,B);
		    VMOVE(B,*pt);
		    break;
		default:
		    // For these purposes, we're only interested in lines
		    break;
	    }

	}
    }
    // Get the last pairing
    if (pcnt > 1)
	dm_draw_arrow(dmp, A, B, s->s_arrow_tip_length, s->s_arrow_tip_width, 1.0);
}

void
dm_draw_arrows(struct dm *dmp, struct bview_data_arrow_state *gdasp, fastf_t sf)
{
    register int i;
    int saveLineWidth;
    int saveLineStyle;

    if (gdasp->gdas_num_points < 1)
	return;

    saveLineWidth = dm_get_linewidth(dmp);
    saveLineStyle = dm_get_linestyle(dmp);

    /* set color */
    (void)dm_set_fg(dmp,
		    gdasp->gdas_color[0],
		    gdasp->gdas_color[1],
		    gdasp->gdas_color[2], 1, 1.0);

    /* set linewidth */
    (void)dm_set_line_attr(dmp, gdasp->gdas_line_width, 0);  /* solid lines */

    (void)dm_draw_lines_3d(dmp,
			   gdasp->gdas_num_points,
			   gdasp->gdas_points, 0);

    for (i = 0; i < gdasp->gdas_num_points; i += 2) {
	point_t points[16];
	point_t A, B;
	point_t BmA;
	point_t offset;
	point_t perp1, perp2;
	point_t a_base;
	point_t a_pt1, a_pt2, a_pt3, a_pt4;

	VMOVE(A, gdasp->gdas_points[i]);
	VMOVE(B, gdasp->gdas_points[i+1]);
	VSUB2(BmA, B, A);

	VUNITIZE(BmA);
	VSCALE(offset, BmA, -gdasp->gdas_tip_length * sf);

	bn_vec_perp(perp1, BmA);
	VUNITIZE(perp1);

	VCROSS(perp2, BmA, perp1);
	VUNITIZE(perp2);

	VSCALE(perp1, perp1, gdasp->gdas_tip_width * sf);
	VSCALE(perp2, perp2, gdasp->gdas_tip_width * sf);

	VADD2(a_base, B, offset);
	VADD2(a_pt1, a_base, perp1);
	VADD2(a_pt2, a_base, perp2);
	VSUB2(a_pt3, a_base, perp1);
	VSUB2(a_pt4, a_base, perp2);

	VMOVE(points[0], B);
	VMOVE(points[1], a_pt1);
	VMOVE(points[2], B);
	VMOVE(points[3], a_pt2);
	VMOVE(points[4], B);
	VMOVE(points[5], a_pt3);
	VMOVE(points[6], B);
	VMOVE(points[7], a_pt4);
	VMOVE(points[8], a_pt1);
	VMOVE(points[9], a_pt2);
	VMOVE(points[10], a_pt2);
	VMOVE(points[11], a_pt3);
	VMOVE(points[12], a_pt3);
	VMOVE(points[13], a_pt4);
	VMOVE(points[14], a_pt4);
	VMOVE(points[15], a_pt1);

	(void)dm_draw_lines_3d(dmp, 16, points, 0);
    }

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}

#define DM_DRAW_POLY(_dmp, _gdpsp, _i, _last_poly, _mode) {	\
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
	    if (_mode != BVIEW_POLY_CONTOUR_MODE || _i != _last_poly || (_gdpsp)->gdps_cflag == 0) { \
		(void)dm_draw_line_3d((_dmp),				\
				      (_gdpsp)->gdps_polygons.polygon[_i].contour[_j].point[_last], \
				      (_gdpsp)->gdps_polygons.polygon[_i].contour[_j].point[0]); \
	    } \
	}}


void
dm_draw_polys(struct dm *dmp, bview_data_polygon_state *gdpsp, int mode)
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

	DM_DRAW_POLY(dmp, gdpsp, i, last_poly, mode);
    }

    /* draw the target poly last */
    DM_DRAW_POLY(dmp, gdpsp, gdpsp->gdps_target_polygon_i, last_poly, mode);

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}

void
dm_draw_lines(struct dm *dmp, struct bview_data_line_state *gdlsp)
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

    /* Frames per second */
    if (v->gv_fps) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	int width = dm_get_width((struct dm *)v->dmp);
	int height = dm_get_height((struct dm *)v->dmp);
	int64_t elapsed_time = bu_gettime() - ((struct dm *)v->dmp)->start_time;
	/* Only use reasonable measurements */
	if (elapsed_time > 10LL && elapsed_time < 30000000LL) {
	    /* Smoothly transition to new speed */
	    v->gv_frametime = 0.9 * v->gv_frametime + 0.1 * elapsed_time / 1000000LL;
	}
	bu_vls_printf(&vls, "FPS:%.2f", 1/v->gv_frametime);
	// TODO - set up separate FPS color
	(void)dm_set_fg((struct dm *)v->dmp,
			v->gv_view_params.gos_text_color[0],
			v->gv_view_params.gos_text_color[1],
			v->gv_view_params.gos_text_color[2],
			1, 1.0);
	(void)dm_draw_string_2d((struct dm *)v->dmp, bu_vls_cstr(&vls), -1.0 + 10.0/(double)width, 1.0 - 40.0/(double)height, 10, 0);
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

void
dm_draw_label(struct dm *dmp, struct bview_scene_obj *s)
{
    struct bview_label *l = (struct bview_label *)s->s_i_data;

    /* set color */
    (void)dm_set_fg(dmp, s->s_color[0], s->s_color[1], s->s_color[2], 1, 1.0);

    point_t vpoint;
    MAT4X3PNT(vpoint, s->s_v->gv_model2view, l->p);

    // Check that we can calculate the bbox before drawing text
    vect2d_t bmin = V2INIT_ZERO;
    vect2d_t bmax = V2INIT_ZERO;
    (void)dm_hud_begin(dmp);
    int txt_ok = dm_string_bbox_2d(dmp, &bmin, &bmax, bu_vls_cstr(&l->label), vpoint[X], vpoint[Y], 1, 1);
    (void)dm_hud_end(dmp);

    // Have bbox - go ahead and write the label
    if (txt_ok == BRLCAD_OK) {
	(void)dm_hud_begin(dmp);
	(void)dm_draw_string_2d(dmp, bu_vls_cstr(&l->label), vpoint[X], vpoint[Y], 0, 1);
	(void)dm_hud_end(dmp);
    }

    if (!l->line_flag)
	return;

    point_t l3d, mpt;

    if (txt_ok == BRLCAD_OK) {
	vect2d_t bmid;
	bmid[0] = (bmax[0] - bmin[0]) * 0.5 + bmin[0];
	bmid[1] = (bmax[1] - bmin[1]) * 0.5 + bmin[1];
	bu_log("bmid: %f,%f\n", bmid[0], bmid[1]);

	vect2d_t anchor = V2INIT_ZERO;
	if (l->anchor == BVIEW_ANCHOR_AUTO) {
	    fastf_t xvals[3];
	    fastf_t yvals[3];
	    xvals[0] = bmin[0];
	    xvals[1] = bmid[0];
	    xvals[2] = bmax[0];
	    yvals[0] = bmin[1];
	    yvals[1] = bmid[1];
	    yvals[2] = bmax[1];
	    fastf_t closest_dist = MAX_FASTF;
	    for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
		    point_t t3d, tpt;
		    if (bview_screen_to_view(s->s_v, &t3d[0], &t3d[1], (int)xvals[i], (int)yvals[j]) < 0) {
			return;
		    }
		    t3d[2] = 0;
		    MAT4X3PNT(tpt, s->s_v->gv_view2model, t3d);
		    double dsq = DIST_PNT_PNT_SQ(tpt, l->target);
		    if (dsq < closest_dist) {
			V2SET(anchor, xvals[i], yvals[j]);
			closest_dist = dsq;
		    }
		}
	    }
	} else {
	    switch (l->anchor) {
		case BVIEW_ANCHOR_BOTTOM_LEFT:
		    V2SET(anchor, bmin[0], bmin[1]);
		    break;
		case BVIEW_ANCHOR_BOTTOM_CENTER:
		    V2SET(anchor, bmid[0], bmin[1]);
		    break;
		case BVIEW_ANCHOR_BOTTOM_RIGHT:
		    V2SET(anchor, bmax[0], bmin[1]);
		    break;
		case BVIEW_ANCHOR_MIDDLE_LEFT:
		    V2SET(anchor, bmin[0], bmid[1]);
		    break;
		case BVIEW_ANCHOR_MIDDLE_CENTER:
		    V2SET(anchor, bmid[0], bmid[1]);
		    break;
		case BVIEW_ANCHOR_MIDDLE_RIGHT:
		    V2SET(anchor, bmax[0], bmid[1]);
		    break;
		case BVIEW_ANCHOR_TOP_LEFT:
		    V2SET(anchor, bmin[0], bmax[1]);
		    break;
		case BVIEW_ANCHOR_TOP_CENTER:
		    V2SET(anchor, bmid[0], bmax[1]);
		    break;
		case BVIEW_ANCHOR_TOP_RIGHT:
		    V2SET(anchor, bmax[0], bmax[1]);
		    break;
		default:
		    bu_log("Unhandled anchor case: %d\n", l->anchor);
		    return;
	    }
	}
	bview_screen_to_view(s->s_v, &l3d[0], &l3d[1], (int)anchor[0], (int)anchor[1]);
	MAT4X3PNT(mpt, s->s_v->gv_view2model, l3d);
    } else {
	VMOVE(mpt, l->p);
    }

    if (l->arrow) {
	dm_draw_arrow(dmp, mpt, l->target, s->s_arrow_tip_length, s->s_arrow_tip_width, 1.0);
    } else {
	dm_draw_line_3d(dmp, mpt, l->target);
    }
}

void
dm_draw_labels(struct dm *dmp, struct bview_data_label_state *gdlsp, matp_t m2vmat)
{
    /* set color */
    (void)dm_set_fg(dmp,
		    gdlsp->gdls_color[0],
		    gdlsp->gdls_color[1],
		    gdlsp->gdls_color[2], 1, 1.0);

    for (int i = 0; i < gdlsp->gdls_num_labels; ++i) {
	point_t vpoint;

	MAT4X3PNT(vpoint, m2vmat,
		  gdlsp->gdls_points[i]);
	(void)dm_draw_string_2d(dmp, gdlsp->gdls_labels[i],
				vpoint[X], vpoint[Y], 0, 1);
    }
}

void
dm_draw_viewobjs(struct rt_wdb *wdbp, struct bview *v, struct dm_view_data *vd, double base2local, double local2base)
{
    struct dm *dmp = (struct dm *)v->dmp;
    int width = dm_get_width(dmp);
    fastf_t sf = (fastf_t)(v->gv_size) / (fastf_t)width;

    if (v->gv_data_arrows.gdas_draw)
	dm_draw_arrows(dmp, &v->gv_data_arrows, sf);

    if (v->gv_sdata_arrows.gdas_draw)
	dm_draw_arrows(dmp, &v->gv_sdata_arrows, sf);

    if (v->gv_data_axes.draw)
	dm_draw_data_axes(dmp, sf, &v->gv_data_axes);

    if (v->gv_sdata_axes.draw)
	dm_draw_data_axes(dmp, sf, &v->gv_sdata_axes);

    if (v->gv_data_lines.gdls_draw)
	dm_draw_lines(dmp, &v->gv_data_lines);

    if (v->gv_sdata_lines.gdls_draw)
	dm_draw_lines(dmp, &v->gv_sdata_lines);

    if (v->gv_data_polygons.gdps_draw)
	dm_draw_polys(dmp, &v->gv_data_polygons, v->gv_mode);

    if (v->gv_sdata_polygons.gdps_draw)
	dm_draw_polys(dmp, &v->gv_sdata_polygons, v->gv_mode);

    // Update selections (if any)

    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_selected); i++) {
	struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(v->gv_selected, i);
	if (s->s_update_callback)
	    (*s->s_update_callback)(s);
	// Update any child objects
	for (size_t j = 0; j < BU_PTBL_LEN(&s->children); j++) {
	    struct bview_scene_obj *s_c = (struct bview_scene_obj *)BU_PTBL_GET(&s->children, j);
	    if (s_c->s_update_callback)
		(*s_c->s_update_callback)(s_c);
	}
    }

    // Draw view objects
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_scene_objs); i++) {
	struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(v->gv_scene_objs, i);
	if (s->s_flag == DOWN)
	    continue;
	// Draw any child objects
	for (size_t j = 0; j < BU_PTBL_LEN(&s->children); j++) {
	    struct bview_scene_obj *s_c = (struct bview_scene_obj *)BU_PTBL_GET(&s->children, j);
	    dm_set_fg(dmp, s_c->s_color[0], s_c->s_color[1], s_c->s_color[2], 1, 1.0);
	    dm_set_line_attr(dmp, s_c->s_line_width, s_c->s_soldash);
	    dm_draw_vlist(dmp, (struct bn_vlist *)&s_c->s_vlist);
	    // Generally arrow settings on child objects are going to be set as part
	    // of the drawing scheme.  If there are cases where we want to control this
	    // manually we'll have to come up with a syntax to specify child objects of
	    // top level scene objects - we don't want these to be top level.
	    dm_add_arrows(dmp, s);

	    if (s_c->s_type_flags & BVIEW_AXES) {
		struct bview_axes *a = (struct bview_axes *)s->s_i_data;
		dm_draw_axes(dmp, s->s_v->gv_size, s->s_v->gv_rotation, a);
	    }

	    if (s_c->s_type_flags & BVIEW_LABELS) {
		dm_draw_label(dmp, s);
	    }
	}
	// Draw primary wireframe.  TODO - drawing children first may not
	// always be the desired behavior - might need interior and exterior
	// children tables to provide some control
	dm_set_fg(dmp, s->s_color[0], s->s_color[1], s->s_color[2], 1, 1.0);
	dm_set_line_attr(dmp, s->s_line_width, s->s_soldash);
	dm_draw_vlist(dmp, (struct bn_vlist *)&s->s_vlist);
	dm_add_arrows(dmp, s);

	if (s->s_type_flags & BVIEW_AXES) {
	    struct bview_axes *a = (struct bview_axes *)s->s_i_data;
	    dm_draw_axes(dmp, s->s_v->gv_size, s->s_v->gv_rotation, a);
	}

	if (s->s_type_flags & BVIEW_LABELS) {
	    dm_draw_label(dmp, s);
	}
    }

    /* Set up matrices for HUD drawing, rather than 3D scene drawing. */
    (void)dm_hud_begin(dmp);

    dm_draw_faceplate(v, base2local, local2base);

    if (v->gv_data_labels.gdls_draw)
	dm_draw_labels(dmp, &v->gv_data_labels, v->gv_model2view);

    if (v->gv_sdata_labels.gdls_draw)
	dm_draw_labels(dmp, &v->gv_sdata_labels, v->gv_model2view);

    /* Draw labels */
    if (wdbp && vd && v->gv_prim_labels.gos_draw) {
	for (int i = 0; i < vd->prim_label_list_size; ++i) {
	    dm_draw_prim_labels(dmp,
			   wdbp,
			   bu_vls_cstr(&vd->prim_label_list[i]),
			   v->gv_model2view,
			   v->gv_prim_labels.gos_text_color,
			   NULL, NULL);
	}
    }

    /* Restore non-HUD settings. */
    (void)dm_hud_end(dmp);

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
