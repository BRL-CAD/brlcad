/*               B S G _ M O V E _ H E L P E R S . H
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
/**
 * @file bsg_move_helpers.h
 *
 * Private helpers for BSG-native pick/move/scale operations in libtclcad.
 *
 * Included by commands.c, mouse.c, view/arrows.c, and view/axes.c.
 *
 * These are drawn from the Phase T3 (drawing_stack_modernization) migration
 * work that removed gv_tcl as the source of truth for interactive pick, move,
 * and scale operations.  The BSG vlist (or child table for labels) is now the
 * sole store; gv_tcl is no longer mirrored for these paths.
 */

#ifndef LIBTCLCAD_BSG_MOVE_HELPERS_H
#define LIBTCLCAD_BSG_MOVE_HELPERS_H

#include "common.h"

#include "vmath.h"
#include "bu/malloc.h"
#include "bsg/feature.h"
#include "bsg/hud.h"
#include "bsg/overlay.h"
#include "dm.h"

__BEGIN_DECLS

#if defined(__GNUC__)
#  define _BSG_HELPER_STATIC static __attribute__((unused))
#else
#  define _BSG_HELPER_STATIC static
#endif

/* --------------------------------------------------------------------------
 * Point extraction
 * -------------------------------------------------------------------------- */

/**
 * Extract a flat point_t array from the vlist of a BSG scene object.
 * On success, *pts_out points to a bu_calloc'd array of npts points.
 * Caller must bu_free(*pts_out, "bsg pts").
 * Returns the number of points extracted (0 if none or s is NULL).
 */
_BSG_HELPER_STATIC int
_bsg_extract_pts(bsg_feature_ref ref, point_t **pts_out)
{
    if (!pts_out) return 0;
    *pts_out = NULL;
    size_t total = 0;
    if (!bsg_feature_points_copy(ref, pts_out, NULL, &total) || !*pts_out)
	return 0;
    return (int)total;
}

/**
 * Extract the center points stored in a data-axes BSG object.
 *
 * Each axes point generates 6 vlist entries (X/Y/Z-axis MOVE+DRAW pairs).
 * The center for group i is the midpoint of vlist[6i] and vlist[6i+1]
 * (the X-axis endpoints).
 *
 * On success, *pts_out points to a bu_calloc'd array of ncenters center
 * points.  Caller must bu_free(*pts_out, "bsg axes pts").
 * Returns the number of center points extracted.
 */
_BSG_HELPER_STATIC int
_bsg_extract_axes_centers(bsg_feature_ref ref, point_t **pts_out)
{
    if (!pts_out) return 0;
    *pts_out = NULL;
    size_t ncenters = 0;
    if (!bsg_feature_axes_centers_copy(ref, pts_out, &ncenters) || !*pts_out)
	return 0;
    return (int)ncenters;
}

/* --------------------------------------------------------------------------
 * Object rebuild helpers
 * -------------------------------------------------------------------------- */

#define _BSG_HELPERS_DEFAULT_DM_WIDTH 512

/**
 * Rebuild a BSG arrow object from an explicit flat point array.
 * Consecutive pairs of points define arrow shafts (pt[0]→pt[1], pt[2]→pt[3]).
 * Existing object (if any) is removed first.
 */
_BSG_HELPER_STATIC void
_bsg_rebuild_arrows(struct bsg_view *v,
		    const char *bsg_name,
		    point_t *pts, int npts,
		    int color[3], int lw,
		    int tip_len, int tip_wid,
		    int visible)
{
    if (!v || !bsg_name) return;
    if (!pts || npts < 2) return;

    struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
    style.visible = visible;
    style.color_valid = color ? 1 : 0;
    if (color) {
	style.color[0] = (unsigned char)color[0];
	style.color[1] = (unsigned char)color[1];
	style.color[2] = (unsigned char)color[2];
    }
    style.line_width = lw;
    style.arrow = 1;
    style.arrow_tip_length = (fastf_t)tip_len;
    style.arrow_tip_width = (fastf_t)tip_wid;

    bsg_feature_ref ref = bsg_feature_replace_arrow(v, bsg_name, 1 /* local */,
	    (const point_t *)pts, (size_t)npts, &style);
    if (bsg_feature_ref_is_null(ref)) return;
    bsg_feature_overlay_register_owner(ref, NULL,
	    BSG_OVERLAY_ROLE_MODEL,
	    BSG_OVERLAY_CLASS_TCL_OVERLAY,
	    BSG_OVERLAY_LC_PER_COMMAND,
	    BSG_OVERLAY_ORDER_POST_TRANSPARENT,
	    NULL,
	    0);
}

/**
 * Rebuild a BSG lines object from an explicit flat point array.
 * Consecutive pairs of points define line segments.
 */
_BSG_HELPER_STATIC void
_bsg_rebuild_lines(struct bsg_view *v,
		   const char *bsg_name,
		   point_t *pts, int npts,
		   int color[3], int lw,
		   int visible)
{
    if (!v || !bsg_name) return;
    if (!pts || npts < 2) return;

    struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
    style.visible = visible;
    style.color_valid = color ? 1 : 0;
    if (color) {
	style.color[0] = (unsigned char)color[0];
	style.color[1] = (unsigned char)color[1];
	style.color[2] = (unsigned char)color[2];
    }
    style.line_width = lw;

    bsg_feature_ref ref = bsg_feature_replace_lines(v, bsg_name, 1 /* local */,
	    (const point_t *)pts, (size_t)npts, &style);
    if (bsg_feature_ref_is_null(ref)) return;
    bsg_feature_overlay_register_owner(ref, NULL,
	    BSG_OVERLAY_ROLE_MODEL,
	    BSG_OVERLAY_CLASS_TCL_OVERLAY,
	    BSG_OVERLAY_LC_PER_COMMAND,
	    BSG_OVERLAY_ORDER_POST_TRANSPARENT,
	    NULL,
	    0);
}

/**
 * Rebuild a BSG data-axes object from an array of center points and a
 * half-axes-size.  Generates 6 vlist entries per center (X/Y/Z axis pairs).
 */
_BSG_HELPER_STATIC void
_bsg_rebuild_axes(struct bsg_view *v,
		  const char *bsg_name,
		  point_t *centers, int ncenters,
		  fastf_t halfAxesSize,
		  int color[3], int lw,
		  int visible)
{
    if (!v || !bsg_name) return;
    if (!centers || ncenters < 1) return;

    struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
    style.visible = visible;
    style.color_valid = color ? 1 : 0;
    if (color) {
	style.color[0] = (unsigned char)color[0];
	style.color[1] = (unsigned char)color[1];
	style.color[2] = (unsigned char)color[2];
    }
    style.line_width = lw;

    bsg_feature_ref ref = bsg_feature_replace_axes(v, bsg_name, 1 /* local */,
	    (const point_t *)centers, (size_t)ncenters, halfAxesSize, &style);
    if (bsg_feature_ref_is_null(ref)) return;
    bsg_feature_overlay_register_owner(ref, NULL,
	    BSG_OVERLAY_ROLE_MODEL,
	    BSG_OVERLAY_CLASS_TCL_OVERLAY,
	    BSG_OVERLAY_LC_PER_COMMAND,
	    BSG_OVERLAY_ORDER_POST_TRANSPARENT,
	    NULL,
	    0);
}

/* --------------------------------------------------------------------------
 * Style extraction from existing BSG object
 * -------------------------------------------------------------------------- */

/**
 * Read display style fields from an existing BSG scene object into caller-
 * supplied output variables.  Safe to call with a NULL @p s (fills defaults).
 */
_BSG_HELPER_STATIC void
_bsg_read_style(bsg_feature_ref ref,
		int color_out[3],
		int *lw_out,
		int *tip_len_out,
		int *tip_wid_out,
		int *visible_out)
{
    /* Defaults */
    if (color_out)   { color_out[0] = 255; color_out[1] = 255; color_out[2] = 0; }
    if (lw_out)       *lw_out       = 0;
    if (tip_len_out)  *tip_len_out  = 0;
    if (tip_wid_out)  *tip_wid_out  = 0;
    if (visible_out)  *visible_out  = 1;

    if (bsg_feature_ref_is_null(ref)) return;

    struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
    if (!bsg_feature_style_get(ref, &style))
	return;
    if (color_out && style.color_valid) {
	color_out[0] = (int)style.color[0];
	color_out[1] = (int)style.color[1];
	color_out[2] = (int)style.color[2];
    }
    if (lw_out)
	*lw_out = style.line_width;
    if (tip_len_out)
	*tip_len_out = (int)style.arrow_tip_length;
    if (tip_wid_out)
	*tip_wid_out = (int)style.arrow_tip_width;
    if (visible_out)
	*visible_out = style.visible;
}

__END_DECLS

#undef _BSG_HELPER_STATIC

#endif /* LIBTCLCAD_BSG_MOVE_HELPERS_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
