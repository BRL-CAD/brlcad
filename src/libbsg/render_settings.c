/*          R E N D E R _ S E T T I N G S . C
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
/** @file libbsg/render_settings.c
 *
 * Per-view rendering policy records.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"

#include "bsg/defines.h"
#include "bsg/render_settings.h"
#include "bsg/view_state.h"
#include "view_state_private.h"


struct bsg_render_settings *
bsg_render_settings_create(void)
{
    struct bsg_render_settings *rs;
    BU_ALLOC(rs, struct bsg_render_settings);
    bsg_render_settings_init_defaults(rs);
    return rs;
}


void
bsg_render_settings_destroy(struct bsg_render_settings *rs)
{
    if (!rs)
	return;
    bu_free(rs, "bsg_render_settings");
}


void
bsg_render_settings_init_defaults(struct bsg_render_settings *rs)
{
    if (!rs)
	return;
    memset(rs, 0, sizeof(struct bsg_render_settings));
    rs->draw_mode             = 0;   /* wireframe */
    rs->zclip                 = 0;
    rs->transparency_policy   = BSG_TRANSPARENCY_SORTED;
    rs->lod_source_policy.policy = BSG_LOD_AUTO;
    rs->lod_source_policy.forced_level = 0;
    rs->lod_source_policy.scale = 1.0;
    rs->lod_source_policy.curve_scale = 1.0;
    rs->lod_source_policy.point_scale = 1.0;
    rs->fb_mode               = BSG_FB_OFF;
    rs->hud_enabled           = 1;
    rs->hud_view_scale        = 0;
    rs->hud_view_params       = 0;
    rs->geometry_default_color[0] = 255;
    rs->geometry_default_color[1] = 0;
    rs->geometry_default_color[2] = 0;
}


void
bsg_render_settings_from_view(struct bsg_render_settings *rs,
			       const struct bsg_view *v)
{
    if (!rs || !v)
	return;

    bsg_render_settings_init_defaults(rs);

    const struct bsg_view_settings *s = bsg_view_settings_active_const(v);
    if (!s)
	return;

    rs->zclip               = s->gv_zclip;
    rs->fb_mode             = (bsg_framebuffer_mode)s->gv_fb_mode;
    rs->lod_source_policy   = s->lod_source_policy;

    /* View params and scale HUD features mirror their settings flags */
    rs->hud_view_params     = s->gv_view_params.draw;
    rs->hud_view_scale      = s->gv_view_scale.gos_draw;

    /* Geometry default color is request/backend policy.  It has no
     * bsg_view_settings backing, so keep the default initialized above unless
     * the render request sets it explicitly. */
}


void
bsg_render_settings_apply_to_view(const struct bsg_render_settings *rs,
				  struct bsg_view *v)
{
    if (!rs || !v)
	return;

    struct bsg_view_settings *s = bsg_view_settings_active(v);
    if (!s)
	return;

    s->gv_zclip            = rs->zclip;
    s->gv_fb_mode          = (int)rs->fb_mode;
    s->lod_source_policy   = rs->lod_source_policy;

    /* HUD faceplate enable flags mirror their bsg_view_settings sources. */
    s->gv_view_params.draw      = rs->hud_view_params;
    s->gv_view_scale.gos_draw   = rs->hud_view_scale;
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
