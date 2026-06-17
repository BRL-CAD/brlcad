/*          A P P E A R A N C E _ A C T I O N . H
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
/** @addtogroup libbsg
 *
 * @brief
 * Resolved appearance action.
 *
 * `bsg_appearance_resolve` combines the active appearance stack for one
 * drawable shape:
 *   - backend/view defaults
 *   - database material color on the shape
 *   - inherited group object settings supplied by traversal
 *   - local shape object settings
 *   - selection/highlight state
 *
 * The result is stored in a `bsg_resolved_appearance` and stamped onto the
 * `bsg_render_item` by the render collector in `bsg_render_request_collect`.
 * Backends read the resolved appearance from the render item and MUST NOT
 * re-derive it from node fields during drawing.
 *
 * Appearance layer composition
 * ----------------------------
 * Layers are applied in priority order (lowest to highest):
 *   BSG_ALAY_GEOM_DEFAULT — renderer/view default style
 *   BSG_ALAY_BASE        — database material color on the shape
 *   BSG_ALAY_INHERITED   — group/ancestor object settings
 *   BSG_ALAY_COMMAND     — local shape command/explicit settings
 *   BSG_ALAY_TRANSPARENCY — resolved transparency < 1.0
 *   BSG_ALAY_GHOSTING    — dim/phantom for objects drawn by reference
 *   BSG_ALAY_HIDDEN_LINE — hidden-line mode (overrides display mode)
 *   BSG_ALAY_EDIT_PREVIEW — edit-plugin preview tint (yellow-ish)
 *   BSG_ALAY_HIGHLIGHT   — selection/illumination highlight (white)
 *
 * Higher-priority layers win for color, transparency, line width, and display
 * mode.  Backends consume only the resolved result.
 */
/** @{ */
/* @file bsg/appearance_action.h */

#ifndef BSG_APPEARANCE_ACTION_H
#define BSG_APPEARANCE_ACTION_H

#include "common.h"
#include "vmath.h"
#include "bsg/defines.h"

__BEGIN_DECLS

/* -----------------------------------------------------------------------
 * Appearance layer enum
 * ----------------------------------------------------------------------- */

/**
 * Appearance layer flags.
 *
 * These bits are OR'd into bsg_resolved_appearance::active_layers to
 * record which appearance overrides are active for a given render item.
 * They are NOT a priority order — they are bit flags for post-resolve
 * queries such as "is this item being highlighted?".
 */
typedef enum bsg_appearance_layer {
    /** Raw material color from the node (always set). */
    BSG_ALAY_BASE         = 0x001,

    /** Ancestor group provided an inherited color override. */
    BSG_ALAY_INHERITED    = 0x002,

    /** bsg_appearance_settings color override is active. */
    BSG_ALAY_COMMAND      = 0x004,

    /** Transparency layer: transparency < 1.0. */
    BSG_ALAY_TRANSPARENCY = 0x008,

    /** Ghosting: object is drawn at reduced opacity (phantom/reference). */
    BSG_ALAY_GHOSTING     = 0x010,

    /** Hidden-line: draw mode forces hidden-line rendering. */
    BSG_ALAY_HIDDEN_LINE  = 0x020,

    /** Edit-preview tint: active in an editor's per-tool scope. */
    BSG_ALAY_EDIT_PREVIEW = 0x040,

    /** Highlight: node is selected or otherwise highlighted. */
    BSG_ALAY_HIGHLIGHT    = 0x080,

    /**
     * Geometry default color: the node requested the renderer's default
     * wireframe color and no command/inherited color
     * override applied.  Render collection supplies this color from the
     * request render settings (bsg_render_settings::geometry_default_color).
     */
    BSG_ALAY_GEOM_DEFAULT = 0x100
} bsg_appearance_layer;


/* -----------------------------------------------------------------------
 * Resolved appearance struct
 * ----------------------------------------------------------------------- */

/**
 * Fully resolved appearance for one render item.
 *
 * Produced by `bsg_appearance_resolve`.  The backend reads these fields
 * directly from the `bsg_render_item` and must not re-derive them.
 *
 * Revision fields allow backends to detect stale cached resources:
 * - `material_rev`: bumped by `bsg_material_set_rgb` or color overrides.
 * - `appearance_rev`: bumped by any highlight/transparency/draw_mode change.
 *
 * Both revisions are per-node values copied out at resolve time; a change
 * in either indicates the backend cache key must be invalidated.
 */
struct bsg_resolved_appearance {
    /** Resolved RGB color (after all layer overrides). */
    unsigned char   color[3];

    /** Resolved transparency in [0.0, 1.0]; 1.0 = fully opaque. */
    fastf_t         transparency;

    /** Resolved draw mode (BSG_WIREFRAME / BSG_SHADED / etc.). */
    int             draw_mode;

    /** Resolved line width in pixels. */
    int             line_width;

    /** Resolved line style: 0 = solid, non-zero = dashed. */
    int             line_style;

    /** Non-zero when this draw item represents evaluated boolean output. */
    int             evaluated_region;

    /** Non-zero when the node is highlighted. */
    int             highlighted;

    /**
     * Active layer bitmask — which appearance overrides are in effect.
     * OR of `bsg_appearance_layer` values.
     */
    unsigned int    active_layers;

    /** Material revision at resolve time (for backend cache invalidation). */
    uint32_t        material_rev;

    /** Appearance revision at resolve time (for backend cache invalidation). */
    uint32_t        appearance_rev;
};


/* Appearance resolution is performed by libbsg render collection and exposed
 * to public callers through bsg_render_item records. */

__END_DECLS

#endif /* BSG_APPEARANCE_ACTION_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
