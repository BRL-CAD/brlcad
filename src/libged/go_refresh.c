/*                           G 0 _ R E F R E S H . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file go_refresh.c
 *
 */


#include "common.h"

#include "solid.h"
#include "dm.h"

#include "ged.h"


HIDDEN void go_dm_draw_arrows(struct dm *dmp, struct ged_data_arrow_state *gdasp);
HIDDEN void go_dm_draw_labels(struct dm *dmp, struct ged_data_label_state *gdlsp, matp_t m2vmat);
HIDDEN void go_dm_draw_lines(struct dm *dmp, struct ged_data_line_state *gdasp);

HIDDEN void go_draw(struct ged_dm_view *gdvp);
#if 1
int go_draw_dlist(struct dm *dmp, struct bu_list *hsp);
#else
HIDDEN int go_draw_dlist(struct dm *dmp, struct bu_list *hsp);
#endif
HIDDEN void go_draw_faceplate(struct ged_obj *gop, struct ged_dm_view *gdvp);
HIDDEN void go_draw_solid(struct dm *dmp, struct solid *sp);


HIDDEN void
go_dm_draw_arrows(struct dm *dmp, struct ged_data_arrow_state *gdasp)
{
    register int i;
    int saveLineWidth;
    int saveLineStyle;

    if (gdasp->gdas_num_points < 1)
	return;

    saveLineWidth = dmp->dm_lineWidth;
    saveLineStyle = dmp->dm_lineStyle;

    /* set color */
    DM_SET_FGCOLOR(dmp,
		   gdasp->gdas_color[0],
		   gdasp->gdas_color[1],
		   gdasp->gdas_color[2], 1, 1.0);

    /* set linewidth */
    DM_SET_LINE_ATTR(dmp, gdasp->gdas_line_width, 0);  /* solid lines */

    DM_DRAW_LINES_3D(dmp,
		     gdasp->gdas_num_points,
		     gdasp->gdas_points);

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
	VSCALE(offset, BmA, -gdasp->gdas_tip_length);

	bn_vec_perp(perp1, BmA);
	VUNITIZE(perp1);

	VCROSS(perp2, BmA, perp1);
	VUNITIZE(perp2);

	VSCALE(perp1, perp1, gdasp->gdas_tip_width);
	VSCALE(perp2, perp2, gdasp->gdas_tip_width);

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

	DM_DRAW_LINES_3D(dmp, 16, points);
    }

    /* Restore the line attributes */
    DM_SET_LINE_ATTR(dmp, saveLineWidth, saveLineStyle);
}


HIDDEN void
go_dm_draw_labels(struct dm *dmp, struct ged_data_label_state *gdlsp, matp_t m2vmat)
{
    register int i;

    /* set color */
    DM_SET_FGCOLOR(dmp,
		   gdlsp->gdls_color[0],
		   gdlsp->gdls_color[1],
		   gdlsp->gdls_color[2], 1, 1.0);

    for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
	point_t vpoint;

	MAT4X3PNT(vpoint, m2vmat,
		  gdlsp->gdls_points[i]);
	DM_DRAW_STRING_2D(dmp, gdlsp->gdls_labels[i],
			  vpoint[X], vpoint[Y], 0, 1);
    }
}


HIDDEN void
go_dm_draw_lines(struct dm *dmp, struct ged_data_line_state *gdlsp)
{
    int saveLineWidth;
    int saveLineStyle;

    if (gdlsp->gdls_num_points < 1)
	return;

    saveLineWidth = dmp->dm_lineWidth;
    saveLineStyle = dmp->dm_lineStyle;

    /* set color */
    DM_SET_FGCOLOR(dmp,
		   gdlsp->gdls_color[0],
		   gdlsp->gdls_color[1],
		   gdlsp->gdls_color[2], 1, 1.0);

    /* set linewidth */
    DM_SET_LINE_ATTR(dmp, gdlsp->gdls_line_width, 0);  /* solid lines */

    DM_DRAW_LINES_3D(dmp,
		     gdlsp->gdls_num_points,
		     gdlsp->gdls_points);

    /* Restore the line attributes */
    DM_SET_LINE_ATTR(dmp, saveLineWidth, saveLineStyle);
}


HIDDEN void
go_draw(struct ged_dm_view *gdvp)
{
    mat_t new;
    matp_t mat;
    mat_t perspective_mat;

    mat = gdvp->gdv_view->gv_model2view;

    if (0 < gdvp->gdv_view->gv_perspective) {
#if 1
	point_t l, h;

	VSET(l, -1.0, -1.0, -1.0);
	VSET(h, 1.0, 1.0, 200.0);

	if (ZERO(gdvp->gdv_view->gv_eye_pos[Z] - 1.0)) {
	    /* This way works, with reasonable Z-clipping */
	    ged_persp_mat(perspective_mat, gdvp->gdv_view->gv_perspective,
			  (fastf_t)1.0f, (fastf_t)0.01f, (fastf_t)1.0e10f, (fastf_t)1.0f);
	} else {
	    /* This way does not have reasonable Z-clipping,
	     * but includes shear, for GDurf's testing.
	     */
	    ged_deering_persp_mat(perspective_mat, l, h, gdvp->gdv_view->gv_eye_pos);
	}
#else
	/*
	 * There are two strategies that could be used:
	 *
	 * 1) Assume a standard head location w.r.t. the screen, and
	 * fix the perspective angle.
	 *
	 * 2) Based upon the perspective angle, compute where the head
	 * should be to achieve that field of view.
	 *
	 * Try strategy #2 for now.
	 */
	fastf_t to_eye_scr;	/* screen space dist to eye */
	point_t l, h, eye;

	/* Determine where eye should be */
	to_eye_scr = 1 / tan(gdvp->gdv_view->gv_perspective * bn_degtorad * 0.5);

	VSET(l, -1.0, -1.0, -1.0);
	VSET(h, 1.0, 1.0, 200.0);
	VSET(eye, 0.0, 0.0, to_eye_scr);

	/* Non-stereo case */
	ged_mike_persp_mat(perspective_mat, gdvp->gdv_view->gv_eye_pos);
#endif

	bn_mat_mul(new, perspective_mat, mat);
	mat = new;
    }

    DM_LOADMATRIX(gdvp->gdv_dmp, mat, 0);
    go_draw_dlist(gdvp->gdv_dmp, &gdvp->gdv_gop->go_gedp->ged_gdp->gd_headDisplay);
}


/* Draw all display lists */
#if 1
int
#else
HIDDEN int
#endif
go_draw_dlist(struct dm *dmp, struct bu_list *hdlp)
{
    register struct ged_display_list *gdlp;
    register struct ged_display_list *next_gdlp;
    struct solid *sp;
    int line_style = -1;

    if (dmp->dm_transparency) {
	/* First, draw opaque stuff */
	gdlp = BU_LIST_NEXT(ged_display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		if (sp->s_transparency < 1.0)
		    continue;

		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, line_style);
		}

		go_draw_solid(dmp, sp);
	    }

	    gdlp = next_gdlp;
	}

	/* disable write to depth buffer */
	DM_SET_DEPTH_MASK(dmp, 0);

	/* Second, draw transparent stuff */
	gdlp = BU_LIST_NEXT(ged_display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		/* already drawn above */
		if (ZERO(sp->s_transparency - 1.0))
		    continue;

		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, line_style);
		}

		go_draw_solid(dmp, sp);
	    }

	    gdlp = next_gdlp;
	}

	/* re-enable write to depth buffer */
	DM_SET_DEPTH_MASK(dmp, 1);
    } else {
	gdlp = BU_LIST_NEXT(ged_display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, line_style);
		}

		go_draw_solid(dmp, sp);
	    }

	    gdlp = next_gdlp;
	}
    }

    return BRLCAD_OK;
}


HIDDEN void
go_draw_faceplate(struct ged_obj *gop, struct ged_dm_view *gdvp)
{
    /* Center dot */
    if (gdvp->gdv_view->gv_center_dot.gos_draw) {
	DM_SET_FGCOLOR(gdvp->gdv_dmp,
		       gdvp->gdv_view->gv_center_dot.gos_line_color[0],
		       gdvp->gdv_view->gv_center_dot.gos_line_color[1],
		       gdvp->gdv_view->gv_center_dot.gos_line_color[2],
		       1, 1.0);
	DM_DRAW_POINT_2D(gdvp->gdv_dmp, 0.0, 0.0);
    }

    /* Model axes */
    if (gdvp->gdv_view->gv_model_axes.gas_draw) {
	point_t map;
	point_t save_map;

	VMOVE(save_map, gdvp->gdv_view->gv_model_axes.gas_axes_pos);
	VSCALE(map, gdvp->gdv_view->gv_model_axes.gas_axes_pos, gop->go_gedp->ged_wdbp->dbip->dbi_local2base);
	MAT4X3PNT(gdvp->gdv_view->gv_model_axes.gas_axes_pos, gdvp->gdv_view->gv_model2view, map);

	dm_draw_axes(gdvp->gdv_dmp,
		     gdvp->gdv_view->gv_size,
		     gdvp->gdv_view->gv_rotation,
		     &gdvp->gdv_view->gv_model_axes);

	VMOVE(gdvp->gdv_view->gv_model_axes.gas_axes_pos, save_map);
    }

    /* View axes */
    if (gdvp->gdv_view->gv_view_axes.gas_draw) {
	fastf_t inv_aspect;
	fastf_t save_ypos;

	save_ypos = gdvp->gdv_view->gv_view_axes.gas_axes_pos[Y];
	inv_aspect = (fastf_t)gdvp->gdv_dmp->dm_height / (fastf_t)gdvp->gdv_dmp->dm_width;
	gdvp->gdv_view->gv_view_axes.gas_axes_pos[Y] = save_ypos * inv_aspect;
	dm_draw_axes(gdvp->gdv_dmp,
		     gdvp->gdv_view->gv_size,
		     gdvp->gdv_view->gv_rotation,
		     &gdvp->gdv_view->gv_view_axes);

	gdvp->gdv_view->gv_view_axes.gas_axes_pos[Y] = save_ypos;
    }

    /* View scale */
    if (gdvp->gdv_view->gv_view_scale.gos_draw)
	dm_draw_scale(gdvp->gdv_dmp,
		      gdvp->gdv_view->gv_size*gop->go_gedp->ged_wdbp->dbip->dbi_base2local,
		      gdvp->gdv_view->gv_view_scale.gos_line_color,
		      gdvp->gdv_view->gv_view_scale.gos_text_color);

    /* View parameters */
    if (gdvp->gdv_view->gv_view_params.gos_draw) {
	struct bu_vls vls;
	point_t center;
	char *ustr;

	MAT_DELTAS_GET_NEG(center, gdvp->gdv_view->gv_center);
	VSCALE(center, center, gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

	bu_vls_init(&vls);
	ustr = (char *)bu_units_string(gop->go_gedp->ged_wdbp->dbip->dbi_local2base);
	bu_vls_printf(&vls, "units:%s  size:%.2f  center:(%.2f, %.2f, %.2f)  az:%.2f  el:%.2f  tw::%.2f",
		      ustr,
		      gdvp->gdv_view->gv_size * gop->go_gedp->ged_wdbp->dbip->dbi_base2local,
		      V3ARGS(center),
		      V3ARGS(gdvp->gdv_view->gv_aet));
	DM_SET_FGCOLOR(gdvp->gdv_dmp,
		       gdvp->gdv_view->gv_view_params.gos_text_color[0],
		       gdvp->gdv_view->gv_view_params.gos_text_color[1],
		       gdvp->gdv_view->gv_view_params.gos_text_color[2],
		       1, 1.0);
	DM_DRAW_STRING_2D(gdvp->gdv_dmp, bu_vls_addr(&vls), -0.98, -0.965, 10, 0);
	bu_vls_free(&vls);
    }

    /* Draw the angle distance cursor */
    if (gdvp->gdv_view->gv_adc.gas_draw)
	dm_draw_adc(gdvp->gdv_dmp, gdvp->gdv_view);

    /* Draw grid */
    if (gdvp->gdv_view->gv_grid.ggs_draw)
	dm_draw_grid(gdvp->gdv_dmp, &gdvp->gdv_view->gv_grid, gdvp->gdv_view, gdvp->gdv_gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

    /* Draw rect */
    if (gdvp->gdv_view->gv_rect.grs_draw && gdvp->gdv_view->gv_rect.grs_line_width)
	dm_draw_rect(gdvp->gdv_dmp, &gdvp->gdv_view->gv_rect);
}


HIDDEN void
go_draw_solid(struct dm *dmp, struct solid *sp)
{
    if (sp->s_iflag == UP)
	DM_SET_FGCOLOR(dmp, 255, 255, 255, 0, sp->s_transparency);
    else
	DM_SET_FGCOLOR(dmp,
		       (unsigned char)sp->s_color[0],
		       (unsigned char)sp->s_color[1],
		       (unsigned char)sp->s_color[2], 0, sp->s_transparency);

    if (sp->s_hiddenLine) {
	DM_DRAW_VLIST_HIDDEN_LINE(dmp, (struct bn_vlist *)&sp->s_vlist);
    } else {
	DM_DRAW_VLIST(dmp, (struct bn_vlist *)&sp->s_vlist);
    }
}


HIDDEN void
go_draw_other(struct ged_obj *gop, struct ged_dm_view *gdvp)
{

    if (gdvp->gdv_view->gv_data_arrows.gdas_draw)
	go_dm_draw_arrows(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_arrows);

    if (gdvp->gdv_view->gv_sdata_arrows.gdas_draw)
	go_dm_draw_arrows(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_arrows);

    if (gdvp->gdv_view->gv_data_axes.gdas_draw)
	dm_draw_data_axes(gdvp->gdv_dmp,
			  gdvp->gdv_view->gv_size,
			  &gdvp->gdv_view->gv_data_axes);

    if (gdvp->gdv_view->gv_sdata_axes.gdas_draw)
	dm_draw_data_axes(gdvp->gdv_dmp,
			  gdvp->gdv_view->gv_size,
			  &gdvp->gdv_view->gv_sdata_axes);

    if (gdvp->gdv_view->gv_data_lines.gdls_draw)
	go_dm_draw_lines(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_lines);

    if (gdvp->gdv_view->gv_sdata_lines.gdls_draw)
	go_dm_draw_lines(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_lines);

    /* Restore to non-rotated, full brightness */
    DM_NORMAL(gdvp->gdv_dmp);
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
go_refresh(struct ged_obj *gop, struct ged_dm_view *gdvp)
{
    int restore_zbuffer = 0;

    /* Turn off the zbuffer if the framebuffer is active AND the zbuffer is on. */
    if (gdvp->gdv_fbs.fbs_mode != GED_OBJ_FB_MODE_OFF &&
	gdvp->gdv_dmp->dm_zbuffer) {
	DM_SET_ZBUFFER(gdvp->gdv_dmp, 0);
	restore_zbuffer = 1;
    }

    DM_DRAW_BEGIN(gdvp->gdv_dmp);
    go_refresh_draw(gop, gdvp);
    DM_DRAW_END(gdvp->gdv_dmp);

    if (restore_zbuffer) {
	DM_SET_ZBUFFER(gdvp->gdv_dmp, 1);
    }
}


void
go_refresh_draw(struct ged_obj *gop, struct ged_dm_view *gdvp)
{
    if (gdvp->gdv_fbs.fbs_mode == GED_OBJ_FB_MODE_OVERLAY) {
	if (gdvp->gdv_view->gv_rect.grs_draw) {
	    go_draw(gdvp);

	    go_draw_other(gop, gdvp);

	    fb_refresh(gdvp->gdv_fbs.fbs_fbp,
		       gdvp->gdv_view->gv_rect.grs_pos[X], gdvp->gdv_view->gv_rect.grs_pos[Y],
		       gdvp->gdv_view->gv_rect.grs_dim[X], gdvp->gdv_view->gv_rect.grs_dim[Y]);

	    if (gdvp->gdv_view->gv_rect.grs_line_width)
		dm_draw_rect(gdvp->gdv_dmp, &gdvp->gdv_view->gv_rect);
	} else
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
		       gdvp->gdv_dmp->dm_width, gdvp->gdv_dmp->dm_height);

	return;
    } else if (gdvp->gdv_fbs.fbs_mode == GED_OBJ_FB_MODE_INTERLAY) {
	go_draw(gdvp);

	if (gdvp->gdv_view->gv_rect.grs_draw) {
	    go_draw(gdvp);
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp,
		       gdvp->gdv_view->gv_rect.grs_pos[X], gdvp->gdv_view->gv_rect.grs_pos[Y],
		       gdvp->gdv_view->gv_rect.grs_dim[X], gdvp->gdv_view->gv_rect.grs_dim[Y]);
	} else
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
		       gdvp->gdv_dmp->dm_width, gdvp->gdv_dmp->dm_height);
    } else {
	if (gdvp->gdv_fbs.fbs_mode == GED_OBJ_FB_MODE_UNDERLAY) {
	    if (gdvp->gdv_view->gv_rect.grs_draw) {
		fb_refresh(gdvp->gdv_fbs.fbs_fbp,
			   gdvp->gdv_view->gv_rect.grs_pos[X], gdvp->gdv_view->gv_rect.grs_pos[Y],
			   gdvp->gdv_view->gv_rect.grs_dim[X], gdvp->gdv_view->gv_rect.grs_dim[Y]);
	    } else
		fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
			   gdvp->gdv_dmp->dm_width, gdvp->gdv_dmp->dm_height);
	}

	go_draw(gdvp);
    }

    go_draw_other(gop, gdvp);
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
