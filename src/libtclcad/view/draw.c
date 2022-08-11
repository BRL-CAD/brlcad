/*                       D R A W . C
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
/** @file libtclcad/view/draw.c
 *
 */
/** @} */

#include "common.h"
#include "dm/view.h"
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
go_draw_solid(struct bview *gdvp, struct bv_scene_obj *sp)
{
    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    struct ged *gedp = tvd->gedp;
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)gedp->u_data;
    struct dm *dmp = (struct dm *)gdvp->dmp;
    struct bu_hash_entry *entry;
    struct dm_path_edit_params *params = NULL;
    mat_t save_mat, edit_model2view;
    struct path_match_data data;

    if (!sp->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

    data.s_fpath = &bdata->s_fullpath;
    data.dbip = gedp->dbip;
    entry = key_matches_paths(tgd->go_dmv.edited_paths, &data);

    if (entry != NULL) {
	params = (struct dm_path_edit_params *)bu_hash_value(entry, NULL);
    }
    if (params) {
	MAT_COPY(save_mat, gdvp->gv_model2view);
	bn_mat_mul(edit_model2view, gdvp->gv_model2view, params->edit_mat);
	dm_loadmatrix(dmp, edit_model2view, 0);
    }

    if (tgd->go_dmv.dlist_on) {
	dm_draw_dlist(dmp, sp->s_dlist);
    } else {
	if (sp->s_iflag == UP)
	    (void)dm_set_fg(dmp, 255, 255, 255, 0, sp->s_os->transparency);
	else
	    (void)dm_set_fg(dmp,
			    (unsigned char)sp->s_color[0],
			    (unsigned char)sp->s_color[1],
			    (unsigned char)sp->s_color[2], 0, sp->s_os->transparency);

	if (sp->s_os->s_dmode == 4) {
	    (void)dm_draw_vlist_hidden_line(dmp, (struct bv_vlist *)&sp->s_vlist);
	} else {
	    (void)dm_draw_vlist(dmp, (struct bv_vlist *)&sp->s_vlist);
	}
    }
    if (params) {
	dm_loadmatrix(dmp, save_mat, 0);
    }
}

/* Draw all display lists */
static int
go_draw_dlist(struct bview *gdvp)
{
    register struct display_list *gdlp;
    register struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    int line_style = -1;
    struct dm *dmp = (struct dm *)gdvp->dmp;
    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    struct bu_list *hdlp = tvd->gedp->ged_gdp->gd_headDisplay;

    if (dm_get_transparency(dmp)) {
	/* First, draw opaque stuff */
	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (sp->s_os->transparency < 1.0)
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

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		/* already drawn above */
		if (ZERO(sp->s_os->transparency - 1.0))
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

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    (void)dm_set_line_attr(dmp, dm_get_linewidth(dmp), line_style);
		}

		go_draw_solid(gdvp, sp);
	    }

	    gdlp = next_gdlp;
	}
    }

    return BRLCAD_OK;
}

void
go_draw(struct bview *gdvp)
{
    (void)dm_loadmatrix((struct dm *)gdvp->dmp, gdvp->gv_model2view, 0);

    if (SMALL_FASTF < gdvp->gv_perspective)
	(void)dm_loadpmatrix((struct dm *)gdvp->dmp, gdvp->gv_pmat);
    else
	(void)dm_loadpmatrix((struct dm *)gdvp->dmp, (fastf_t *)NULL);

    go_draw_dlist(gdvp);
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
    int ret = BRLCAD_OK;

    if (argc != 2)
	return BRLCAD_ERROR;

    gdlp = BU_LIST_NEXT(display_list, gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
	gdlp->dl_wflag = 0;
	gdlp = BU_LIST_PNEXT(display_list, gdlp);
    }

    if (db_string_to_path(&subpath, gedp->dbip, argv[1]) == 0) {
	for (i = 0; i < subpath.fp_len; ++i) {
	    gdlp = BU_LIST_NEXT(display_list, gedp->ged_gdp->gd_headDisplay);
	    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
		register struct bv_scene_obj *curr_sp;

		next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

		if (gdlp->dl_wflag) {
		    gdlp = next_gdlp;
		    continue;
		}

		for (BU_LIST_FOR(curr_sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {

		    if (!curr_sp->s_u_data)
			continue;
		    struct ged_bv_data *bdata = (struct ged_bv_data *)curr_sp->s_u_data;

		    if (db_full_path_search(&bdata->s_fullpath, subpath.fp_names[i])) {
			struct display_list *last_gdlp;
			struct bv_scene_obj *sp = BU_LIST_NEXT(bv_scene_obj, &gdlp->dl_head_scene_obj);
			struct bu_vls mflag = BU_VLS_INIT_ZERO;
			struct bu_vls xflag = BU_VLS_INIT_ZERO;
			char *av[5] = {0};
			int arg = 0;

			av[arg++] = (char *)argv[0];
			if (sp->s_os->s_dmode == 4) {
			    av[arg++] = "-h";
			} else {
			    bu_vls_printf(&mflag, "-m%d", sp->s_os->s_dmode);
			    bu_vls_printf(&xflag, "-x%f", sp->s_os->transparency);
			    av[arg++] = bu_vls_addr(&mflag);
			    av[arg++] = bu_vls_addr(&xflag);
			}
			av[arg] = bu_vls_strdup(&gdlp->dl_path);

			ret = ged_exec(gedp, arg + 1, (const char **)av);

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
	return BRLCAD_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
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

    ret = ged_exec(gedp, argc, argv);

    if (ret != BRLCAD_OK)
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
