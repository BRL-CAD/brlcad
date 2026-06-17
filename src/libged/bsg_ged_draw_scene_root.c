/*                B S G _ G E D _ D R A W _ R O O T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libged/b_s_g___g_e_d___d_r_a_w___r_o_o_t_._c.c
 *
 * Retained scene-root and view-root compatibility bridge.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/color.h"
#include "bu/hash.h"
#include "bsg/appearance.h"
#include "bsg/defines.h"
#include "bsg/database_source.h"
#include "bsg/draw_ctx.h"
#include "bsg/draw_intent.h"
#include "bsg/draw_set.h"
#include "bsg/draw_source.h"
#include "bsg/field.h"
#include "bsg/geometry.h"
#include "bsg/interaction.h"
#include "bsg/lod.h"
#include "bsg/material.h"
#include "bsg/payload.h"
#include "bsg/plot3.h"
#include "bsg/scene_builder.h"
#include "bsg/scene_object.h"
#include "bsg/selection.h"
#include "bsg/view_set.h"
#include "bsg/view_state.h"
#include "bsg/util.h"
#include "bg/clip.h"

#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "./ged_private.h"

static void
_sg_view_scene_root_set(struct bsg_view *v, bsg_scene_ref root)
{
    (void)bsg_view_scene_ref_attach(v, root);
}


/*
 * Lazily create (on first draw) and return the per-GED retained scene root.
 * Also attaches the root to each GED view handle so the render path can
 * traverse the shared scene without exposing the pointer through the public
 * view struct.
 */
static bsg_scene_ref
_sg_root(struct ged *gedp)
{
    struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    if (!bsg_scene_ref_is_null(root_ref)) {
	if (gedp->ged_gvp)
	    _sg_view_scene_root_set(gedp->ged_gvp, root_ref);
	if (views) {
	    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
		struct bsg_view *v = (struct bsg_view *)BU_PTBL_GET(views, i);
		if (!v)
		    continue;
		_sg_view_scene_root_set(v, root_ref);
	    }
	}
	return root_ref;
    }

    struct bsg_view *v = gedp->ged_gvp;
    if (!v)
        return bsg_scene_ref_null();

    root_ref = bsg_scene_group_create(v, "_draw_root");
    if (bsg_scene_ref_is_null(root_ref))
        return bsg_scene_ref_null();

    bsg_scene_set_visible(root_ref, 1);

    ged_scene_root_ref_set(gedp, root_ref);

    /* Store draw-tree bookkeeping on the root so freeing helpers can bump
     * gd_draw_rev and recycle nodes without carrying a ged pointer. */
    gedp->i->ged_gdp->bsg_ctx.draw_rev = &gedp->i->ged_gdp->gd_draw_rev;
    gedp->i->ged_gdp->bsg_ctx.fso      = bsg_set_fsos(&gedp->ged_views);
    gedp->i->ged_gdp->bsg_ctx.owner_data = gedp->i->ged_gdp;
    bsg_scene_set_draw_ctx(root_ref, &gedp->i->ged_gdp->bsg_ctx);

    /* Register in all views so the BSG render loop can traverse the shared
     * retained scene directly without reading gv_objs.  No per-frame
     * bsg_scene_root_sync rebuild is needed. */
    _sg_view_scene_root_set(v, root_ref);
    if (views) {
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bsg_view *gv = (struct bsg_view *)BU_PTBL_GET(views, i);
	    if (!gv)
		continue;
	    _sg_view_scene_root_set(gv, root_ref);
	}
    }

    return root_ref;
}



bsg_scene_ref
ged_draw_view_scene_root_ref(const struct bsg_view *v)
{
    return bsg_view_scene_ref(v);
}


int
ged_draw_view_has_scene_root(const struct bsg_view *v)
{
    return bsg_scene_ref_is_null(bsg_view_scene_ref(v)) ? 0 : 1;
}


bsg_scene_ref
ged_draw_ensure_root(struct ged *gedp)
{
    if (!gedp)
        return bsg_scene_ref_null();
    return _sg_root(gedp);
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
