/*                B S G _ G E D _ D R A W _ U T I L . C
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
/** @file libged/b_s_g___g_e_d___d_r_a_w___u_t_i_l_._c.c
 *
 * Small draw-scene utility entry points shared by the split bridge files.
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
#include "bsg/node.h"
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

const char *
ged_draw_dbpath_skip_lead_slash(const char *s)
{
    if (s && *s == '/')
        return s + 1;
    return s;
}

const char *
ged_draw_stale_reason_name(ged_draw_stale_reason reason)
{
    switch (reason) {
	case GED_DRAW_STALE_NONE:
	    return "current";
	case GED_DRAW_STALE_SOURCE_CHANGED:
	    return "source-changed";
	case GED_DRAW_STALE_VIEW_INPUT_CHANGED:
	    return "view-input-changed";
	case GED_DRAW_STALE_SETTINGS_CHANGED:
	    return "settings-changed";
	case GED_DRAW_STALE_FORCED:
	    return "forced";
	case GED_DRAW_STALE_UPDATE_FAILED:
	    return "update-failed";
	default:
	    return "unknown";
    }
}

bsg_draw_mode
ged_draw_default_mode(const struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return BSG_DRAW_MODE_WIRE;
    return (bsg_draw_mode)gedp->i->ged_gdp->gd_shaded_mode;
}

unsigned long long
ged_draw_scene_hash(struct ged *gedp)
{
    if (!gedp)
        return 0;
    return (unsigned long long)gedp->i->ged_gdp->gd_draw_rev;
}

uint64_t
ged_draw_scene_revision(struct ged *gedp)
{
    if (!gedp)
        return 0;
    return gedp->i->ged_gdp->gd_draw_rev;
}

int
ged_draw_shape_ref_is_null(ged_draw_shape_ref ref)
{
    return ref.token == 0;
}

int
ged_draw_group_ref_is_null(ged_draw_group_ref ref)
{
    return ref.token == 0;
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
