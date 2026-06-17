/*         A P P E A R A N C E _ A C T I O N . C
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
/** @file libbsg/appearance_action.c
 *
 * Resolve the final appearance for a shape node by applying the active
 * render/default, material, inherited group, local node, and highlight layers.
 */

#include "common.h"

#include <string.h>

#include "bsg/defines.h"
#include "bsg/material.h"
#include "bsg/appearance.h"
#include "bsg/appearance_action.h"
#include "bsg/render_settings.h"
#include "appearance_private.h"
#include "field_private.h"
#include "material_private.h"
#include "node_private.h"


int
bsg_appearance_resolve_with_settings(const struct bsg_render_settings *settings,
				     const bsg_node *node,
				     const struct bsg_appearance_settings *inherited_os,
				     struct bsg_resolved_appearance *out)
{
    if (!node || !out)
	return 0;

    memset(out, 0, sizeof(struct bsg_resolved_appearance));

    /* ------------------------------------------------------------------ */
    /* 1. Start from the view/backend default style.                       */
    /* ------------------------------------------------------------------ */
    unsigned char gd[3] = { 255, 0, 0 };
    if (settings) {
	gd[0] = settings->geometry_default_color[0];
	gd[1] = settings->geometry_default_color[1];
	gd[2] = settings->geometry_default_color[2];
    }
    out->color[0] = gd[0];
    out->color[1] = gd[1];
    out->color[2] = gd[2];
    out->transparency = 1.0;
    out->draw_mode = 0;
    out->line_width = 1;
    out->active_layers |= BSG_ALAY_GEOM_DEFAULT;

    /* ------------------------------------------------------------------ */
    /* 2. Database material color on the shape.                            */
    /* ------------------------------------------------------------------ */
    unsigned char r = 0, g = 0, b = 0;
    bsg_material_get_rgb(node, &r, &g, &b);
    out->color[0] = r;
    out->color[1] = g;
    out->color[2] = b;
    out->active_layers |= BSG_ALAY_BASE;

    /* ------------------------------------------------------------------ */
    /* 3. Geometry-default-color request.                                  */
    /*                                                                     */
    /* When the node asked for the renderer's default wireframe color, use */
    /* it in place of the database material color.  Later inherited/local  */
    /* color overrides still win.                                          */
    /* ------------------------------------------------------------------ */
    if (bsg_appearance_uses_default_color(node)) {
	out->color[0] = gd[0];
	out->color[1] = gd[1];
	out->color[2] = gd[2];
	out->active_layers |= BSG_ALAY_GEOM_DEFAULT;
    }

    /* ------------------------------------------------------------------ */
    /* 4. Inherited ancestor group settings.                               */
    /* ------------------------------------------------------------------ */
    if (inherited_os) {
	out->transparency = inherited_os->transparency;
	out->draw_mode    = inherited_os->draw_mode;
	out->line_width   = inherited_os->s_line_width;
	if (inherited_os->color_override) {
	    out->color[0] = inherited_os->color[0];
	    out->color[1] = inherited_os->color[1];
	    out->color[2] = inherited_os->color[2];
	    out->active_layers |= BSG_ALAY_INHERITED;
	}
    }

    /* ------------------------------------------------------------------ */
    /* 5. Local shape settings.                                            */
    /* ------------------------------------------------------------------ */
    struct bsg_appearance_settings local_settings;
    int local_line_style = 0;
    bsg_appearance_settings_for_node(node, &local_settings);
    (void)bsg_field_get_int(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_DRAW_LINE_STYLE),
	    &local_line_style);

    if (local_settings.transparency < 1.0 || local_settings.draw_mode != 0 ||
	    local_settings.s_line_width != 1 || local_settings.color_override) {
	out->transparency = local_settings.transparency;
	out->draw_mode    = local_settings.draw_mode;
	out->line_width   = local_settings.s_line_width;
	if (local_settings.color_override) {
	    out->color[0] = local_settings.color[0];
	    out->color[1] = local_settings.color[1];
	    out->color[2] = local_settings.color[2];
	    out->active_layers |= BSG_ALAY_COMMAND;
	}
    }

    /* ------------------------------------------------------------------ */
    /* 5. Line style from the shape node                                   */
    /* ------------------------------------------------------------------ */
    out->line_style = local_line_style;
    out->evaluated_region = bsg_appearance_legacy_eval_flag(node);

    /* ------------------------------------------------------------------ */
    /* 6. Transparency layer flag                                           */
    /* ------------------------------------------------------------------ */
    if (out->transparency < 1.0)
	out->active_layers |= BSG_ALAY_TRANSPARENCY;

    /* ------------------------------------------------------------------ */
    /* 7. Hidden-line draw mode                                              */
    /* ------------------------------------------------------------------ */
    if (out->draw_mode == 4)
	out->active_layers |= BSG_ALAY_HIDDEN_LINE;

    /* ------------------------------------------------------------------ */
    /* 8. Highlight state                                                  */
    /* ------------------------------------------------------------------ */
    out->highlighted = bsg_appearance_is_highlighted(node);
    if (out->highlighted)
	out->active_layers |= BSG_ALAY_HIGHLIGHT;

    /* ------------------------------------------------------------------ */
    /* 9. Revision stamps for backend cache invalidation                   */
    /* ------------------------------------------------------------------ */
    out->material_rev   = bsg_material_revision(node);
    out->appearance_rev = (uint32_t)(bsg_appearance_drawn_rev(node) & 0xFFFFFFFFu);

    return 1;
}


int
bsg_appearance_resolve(const struct bsg_view *UNUSED(v),
		       const bsg_node *node,
		       const struct bsg_appearance_settings *inherited_os,
		       struct bsg_resolved_appearance *out)
{
    return bsg_appearance_resolve_with_settings(
	    NULL,
	    node,
	    inherited_os,
	    out);
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
