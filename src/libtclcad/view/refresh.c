/*                       R E F R E S H . C
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
	return GED_ERROR;
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
	return GED_ERROR;
    }

    to_refresh_all_views(current_top);

    return GED_OK;
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
	return GED_ERROR;
    }

    /* Get refresh_on state */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%d", current_top->to_gop->go_refresh_on);
	return GED_OK;
    }

    /* Set refresh_on state */
    if (bu_sscanf(argv[1], "%d", &on) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    current_top->to_gop->go_refresh_on = on;

    return GED_OK;
}

int
to_handle_refresh(struct ged *gedp,
		  const char *name)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), name))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", name);
	return GED_ERROR;
    }

    to_refresh_view(gdvp);

    return GED_OK;
}


void
to_refresh_handler(void *clientdata)
{
    struct ged_dm_view *gdvp = (struct ged_dm_view *)clientdata;

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
