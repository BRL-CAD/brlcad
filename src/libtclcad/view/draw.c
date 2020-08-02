/*                       D R A W . C
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
/** @file libtclcad/view/draw.c
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

void
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

void
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

int
to_edit_redraw(struct ged *gedp,
	       int argc,
	       const char *argv[])
{
    size_t i;
    register struct display_list *gdlp;
    register struct display_list *next_gdlp;
    struct db_full_path subpath;
    int ret = GED_OK;

    if (argc != 2)
	return GED_ERROR;

    gdlp = BU_LIST_NEXT(display_list, gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
	gdlp->dl_wflag = 0;
	gdlp = BU_LIST_PNEXT(display_list, gdlp);
    }

    if (db_string_to_path(&subpath, gedp->ged_wdbp->dbip, argv[1]) == 0) {
	for (i = 0; i < subpath.fp_len; ++i) {
	    gdlp = BU_LIST_NEXT(display_list, gedp->ged_gdp->gd_headDisplay);
	    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
		register struct solid *curr_sp;

		next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

		if (gdlp->dl_wflag) {
		    gdlp = next_gdlp;
		    continue;
		}

		FOR_ALL_SOLIDS(curr_sp, &gdlp->dl_headSolid) {
		    if (db_full_path_search(&curr_sp->s_fullpath, subpath.fp_names[i])) {
			struct display_list *last_gdlp;
			struct solid *sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
			struct bu_vls mflag = BU_VLS_INIT_ZERO;
			struct bu_vls xflag = BU_VLS_INIT_ZERO;
			char *av[5] = {0};
			int arg = 0;

			av[arg++] = (char *)argv[0];
			if (sp->s_hiddenLine) {
			    av[arg++] = "-h";
			} else {
			    bu_vls_printf(&mflag, "-m%d", sp->s_dmode);
			    bu_vls_printf(&xflag, "-x%f", sp->s_transparency);
			    av[arg++] = bu_vls_addr(&mflag);
			    av[arg++] = bu_vls_addr(&xflag);
			}
			av[arg] = bu_vls_strdup(&gdlp->dl_path);

			ret = ged_draw(gedp, arg + 1, (const char **)av);

			bu_free(av[arg], "to_edit_redraw");
			bu_vls_free(&mflag);
			bu_vls_free(&xflag);

			/* The function call above causes gdlp to be
			 * removed from the display list. A new one is
			 * then created and appended to the end.  Here
			 * we put it back where it belongs (i.e. as
			 * specified by the user).  This also prevents
			 * an infinite loop where the last and the
			 * second to last list items play leap frog
			 * with the end of list.
			 */
			last_gdlp = BU_LIST_PREV(display_list, gedp->ged_gdp->gd_headDisplay);
			BU_LIST_DEQUEUE(&last_gdlp->l);
			BU_LIST_INSERT(&next_gdlp->l, &last_gdlp->l);
			last_gdlp->dl_wflag = 1;

			goto end;
		    }
		}

	    end:
		gdlp = next_gdlp;
	    }
	}

	db_free_full_path(&subpath);
    }

    to_refresh_all_views(current_top);

    return ret;
}

int
to_redraw(struct ged *gedp,
	  int argc,
	  const char *argv[],
	  ged_func_ptr UNUSED(func),
	  const char *usage,
	  int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    return to_edit_redraw(gedp, argc, argv);
}

int
to_blast(struct ged *gedp,
	 int argc,
	 const char *argv[],
	 ged_func_ptr UNUSED(func),
	 const char *UNUSED(usage),
	 int UNUSED(maxargs))
{
    int ret;

    ret = ged_blast(gedp, argc, argv);

    if (ret != GED_OK)
	return ret;

    to_autoview_all_views(current_top);

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
