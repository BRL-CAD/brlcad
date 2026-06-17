/*                       R E F R E S H . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2026 United States Government as represented by
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
/** @file libtclcad/views/refresh.c
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

void
go_refresh_draw(struct ged *gedp, struct bsg_view *gdvp, int restore_zbuffer)
{
    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    struct bsg_interactive_rect_state rect = {0};
    (void)bsg_view_interactive_rect_get(gdvp, &rect);
    if (tvd->gdv_fbs.fbs_mode == TCLCAD_OBJ_FB_MODE_OVERLAY) {
	if (rect.draw) {
	    go_draw(gdvp);

	    /* Phase T2-final: replaced dm_draw_viewobjs with dm_draw_objs.
	     * Stash/restore gv_local2base|base2local to keep faceplate unit
	     * display consistent with the database unit factors. */
	    double l2b = gdvp->gv_local2base;
	    double b2l = gdvp->gv_base2local;
	    gdvp->gv_local2base = gedp->dbip->dbi_local2base;
	    gdvp->gv_base2local = gedp->dbip->dbi_base2local;
	    dm_draw_objs(gdvp);
	    gdvp->gv_local2base = l2b;
	    gdvp->gv_base2local = b2l;

	    /* disable write to depth buffer */
	    (void)dm_set_depth_mask((struct dm *)gdvp->dmp, 0);

	    fb_refresh(tvd->gdv_fbs.fbs_fbp,
		       rect.pos[X], rect.pos[Y],
		       rect.dim[X], rect.dim[Y]);

	    /* enable write to depth buffer */
	    (void)dm_set_depth_mask((struct dm *)gdvp->dmp, 1);

	    if (rect.line_width)
		dm_draw_rect((struct dm *)gdvp->dmp, &rect);
	} else {
	    /* disable write to depth buffer */
	    (void)dm_set_depth_mask((struct dm *)gdvp->dmp, 0);

	    fb_refresh(tvd->gdv_fbs.fbs_fbp, 0, 0,
		       dm_get_width((struct dm *)gdvp->dmp), dm_get_height((struct dm *)gdvp->dmp));

	    /* enable write to depth buffer */
	    (void)dm_set_depth_mask((struct dm *)gdvp->dmp, 1);
	}

	if (restore_zbuffer) {
	    (void)dm_set_zbuffer((struct dm *)gdvp->dmp, 1);
	}

	return;
    } else if (tvd->gdv_fbs.fbs_mode == TCLCAD_OBJ_FB_MODE_INTERLAY) {
	go_draw(gdvp);

	/* disable write to depth buffer */
	(void)dm_set_depth_mask((struct dm *)gdvp->dmp, 0);

	if (rect.draw) {
	    fb_refresh(tvd->gdv_fbs.fbs_fbp,
		       rect.pos[X], rect.pos[Y],
		       rect.dim[X], rect.dim[Y]);
	} else
	    fb_refresh(tvd->gdv_fbs.fbs_fbp, 0, 0,
		       dm_get_width((struct dm *)gdvp->dmp), dm_get_height((struct dm *)gdvp->dmp));

	/* enable write to depth buffer */
	(void)dm_set_depth_mask((struct dm *)gdvp->dmp, 1);

	if (restore_zbuffer) {
	    (void)dm_set_zbuffer((struct dm *)gdvp->dmp, 1);
	}
    } else {
	if (tvd->gdv_fbs.fbs_mode == TCLCAD_OBJ_FB_MODE_UNDERLAY) {
	    /* disable write to depth buffer */
	    (void)dm_set_depth_mask((struct dm *)gdvp->dmp, 0);

	    if (rect.draw) {
		fb_refresh(tvd->gdv_fbs.fbs_fbp,
			   rect.pos[X], rect.pos[Y],
			   rect.dim[X], rect.dim[Y]);
	    } else
		fb_refresh(tvd->gdv_fbs.fbs_fbp, 0, 0,
			   dm_get_width((struct dm *)gdvp->dmp), dm_get_height((struct dm *)gdvp->dmp));

	    /* enable write to depth buffer */
	    (void)dm_set_depth_mask((struct dm *)gdvp->dmp, 1);
	}

	if (restore_zbuffer) {
	    (void)dm_set_zbuffer((struct dm *)gdvp->dmp, 1);
	}

	go_draw(gdvp);
    }

    /* Phase T2-final: replaced dm_draw_viewobjs with dm_draw_objs so the
     * full BSG view-scope object set (including T1-migrated overlays) and
     * faceplate are rendered through the modern path.  Stash/restore the
     * unit-conversion factors as before. */
    double l2b = gdvp->gv_local2base;
    double b2l = gdvp->gv_base2local;
    gdvp->gv_local2base = gedp->dbip->dbi_local2base;
    gdvp->gv_base2local = gedp->dbip->dbi_base2local;
    dm_draw_objs(gdvp);
    gdvp->gv_local2base = l2b;
    gdvp->gv_base2local = b2l;
}

void
go_refresh(struct ged *gedp, struct bsg_view *gdvp)
{
    int restore_zbuffer = 0;

    /* Turn off the zbuffer if the framebuffer is active AND the zbuffer is on. */
    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    if (tvd->gdv_fbs.fbs_mode != TCLCAD_OBJ_FB_MODE_OFF && dm_get_zbuffer((struct dm *)gdvp->dmp)) {
	(void)dm_set_zbuffer((struct dm *)gdvp->dmp, 0);
	restore_zbuffer = 1;
    }

    (void)dm_draw_begin((struct dm *)gdvp->dmp);
    go_refresh_draw(gedp, gdvp, restore_zbuffer);
    (void)dm_draw_end((struct dm *)gdvp->dmp);
}

void
to_refresh_view(struct bsg_view *gdvp)
{

    if (current_top == NULL || gdvp == NULL)
	return;

    if (!bsg_view_refresh_enabled(gdvp) || bsg_view_refresh_suppressed(gdvp))
	return;

    bsg_view_refresh_request(gdvp, BSG_VIEW_REFRESH_ALL);
    if (to_is_viewable(gdvp)) {
	(void)bsg_view_refresh_consume(gdvp);
	go_refresh(current_top->to_gedp, gdvp);
	bsg_view_refresh_complete(gdvp);
    }
}

void
to_refresh_all_views(struct tclcad_obj *top)
{
    struct bsg_view *gdvp;

    struct bu_ptbl *views = bsg_set_views(&top->to_gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	gdvp = (struct bsg_view *)BU_PTBL_GET(views, i);
	to_refresh_view(gdvp);
    }
}

int
to_refresh_all_enabled(struct tclcad_obj *top)
{
    struct bsg_view *gdvp;

    if (!top)
	return 1;

    struct bu_ptbl *views = bsg_set_views(&top->to_gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	gdvp = (struct bsg_view *)BU_PTBL_GET(views, i);
	if (!bsg_view_refresh_enabled(gdvp))
	    return 0;
    }

    return 1;
}

void
to_refresh_all_set_enabled(struct tclcad_obj *top, int enabled)
{
    struct bsg_view *gdvp;

    if (!top)
	return;

    struct bu_ptbl *views = bsg_set_views(&top->to_gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	gdvp = (struct bsg_view *)BU_PTBL_GET(views, i);
	bsg_view_refresh_set_enabled(gdvp, enabled);
    }
}

void
to_refresh_suppress_all_begin(struct tclcad_obj *top)
{
    struct bsg_view *gdvp;

    if (!top)
	return;

    struct bu_ptbl *views = bsg_set_views(&top->to_gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	gdvp = (struct bsg_view *)BU_PTBL_GET(views, i);
	bsg_view_refresh_suppress_begin(gdvp);
    }
}

void
to_refresh_suppress_all_end(struct tclcad_obj *top)
{
    struct bsg_view *gdvp;

    if (!top)
	return;

    struct bu_ptbl *views = bsg_set_views(&top->to_gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	gdvp = (struct bsg_view *)BU_PTBL_GET(views, i);
	bsg_view_refresh_suppress_end(gdvp);
    }
}

int
to_refresh(struct ged *gedp,
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
	return BRLCAD_ERROR;
    }

    return to_handle_refresh(gedp, argv[1]);
}


int
to_refresh_all(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *UNUSED(usage),
	       int UNUSED(maxargs))
{
    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    to_refresh_all_views(current_top);

    return BRLCAD_OK;
}


int
to_refresh_on(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *UNUSED(usage),
	      int UNUSED(maxargs))
{
    int on;
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (2 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    /* Get refresh_on state */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%d", to_refresh_all_enabled(current_top));
	return BRLCAD_OK;
    }

    /* Set refresh_on state */
    if (bu_sscanf(argv[1], "%d", &on) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    to_refresh_all_set_enabled(current_top, on);

    return BRLCAD_OK;
}

int
to_handle_refresh(struct ged *gedp,
		  const char *name)
{
    struct bsg_view *gdvp;

    gdvp = bsg_set_find_view(&gedp->ged_views, name);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", name);
	return BRLCAD_ERROR;
    }

    to_refresh_view(gdvp);

    return BRLCAD_OK;
}


void
to_refresh_handler(void *clientdata)
{
    struct bsg_view *gdvp = (struct bsg_view *)clientdata;

    /* Possibly do more here */

    to_refresh_view(gdvp);
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
