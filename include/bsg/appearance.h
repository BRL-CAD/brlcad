/*                   A P P E A R A N C E . H
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
 * Scene appearance properties independent of material.
 */
/** @{ */
/* @file bsg/appearance.h */

#ifndef BSG_APPEARANCE_H
#define BSG_APPEARANCE_H

#include "common.h"
#include "bsg/defines.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

BSG_EXPORT extern void
bsg_scene_set_line_style(bsg_scene_ref ref, int dashed);

BSG_EXPORT extern int
bsg_scene_line_style(bsg_scene_ref ref);

BSG_EXPORT extern void
bsg_scene_set_line_width(bsg_scene_ref ref, int line_width);

BSG_EXPORT extern int
bsg_scene_line_width(bsg_scene_ref ref);

BSG_EXPORT extern int
bsg_scene_apply_appearance_settings(bsg_scene_ref ref,
				    const struct bsg_appearance_settings *settings);

BSG_EXPORT extern void
bsg_scene_set_work_flag(bsg_scene_ref ref, int wflag);

BSG_EXPORT extern void
bsg_scene_set_legacy_color_info(bsg_scene_ref ref,
				const unsigned char basecolor[3],
				int user_color,
				int default_color);

BSG_EXPORT extern void
bsg_scene_legacy_basecolor(bsg_scene_ref ref, unsigned char basecolor[3]);

BSG_EXPORT extern int
bsg_scene_legacy_user_color(bsg_scene_ref ref);

BSG_EXPORT extern int
bsg_scene_legacy_default_color(bsg_scene_ref ref);

BSG_EXPORT extern void
bsg_scene_set_legacy_uses_default_color(bsg_scene_ref ref, int default_color);

BSG_EXPORT extern void
bsg_scene_set_legacy_eval_flag(bsg_scene_ref ref, int eflag);

BSG_EXPORT extern int
bsg_scene_legacy_eval_flag(bsg_scene_ref ref);

BSG_EXPORT extern void
bsg_scene_set_legacy_region_id(bsg_scene_ref ref, int region_id);

BSG_EXPORT extern int
bsg_scene_legacy_region_id(bsg_scene_ref ref);

/* -----------------------------------------------------------------------
 * Illumination / highlight state
 *
 * A highlighted node is currently illuminated/selected for highlighting in
 * the renderer.  Use these accessors in place of direct compatibility-field
 * reads and writes.  For batch selection work, prefer the bsg_selection API;
 * these accessors are for per-node one-shot queries.
 * ----------------------------------------------------------------------- */

/**
 * Set the highlight (illumination) state of @p ref.
 * @p highlighted non-zero highlights the scene object; zero clears
 * highlighting.
 */
BSG_EXPORT extern void
bsg_scene_set_highlighted(bsg_scene_ref ref, int highlighted);

/**
 * Return non-zero if @p ref is currently highlighted.
 */
BSG_EXPORT extern int
bsg_scene_highlighted(bsg_scene_ref ref);

/* -----------------------------------------------------------------------
 * Changed / dirty flag
 *
 * The changed flag is set by update callbacks when they regenerate geometry,
 * signalling the renderer that cached resources are stale.
 * ----------------------------------------------------------------------- */

/**
 * Set the changed flag on @p ref.
 * @p changed non-zero asserts the flag; zero clears it.
 */
BSG_EXPORT extern void
bsg_scene_set_changed(bsg_scene_ref ref, int changed);

/**
 * Return the current value of the changed flag for @p ref.
 */
BSG_EXPORT extern int
bsg_scene_changed(bsg_scene_ref ref);

/* -----------------------------------------------------------------------
 * Drawn-frame revision stamp
 *
 * This stamp is set by the renderer to the bsg_view's gv_frame_rev each time
 * the node is drawn.  Callers can detect "was this node drawn in the most
 * recent frame" by comparing the stamp to the view's current gv_frame_rev.
 * This replaces the legacy full-tree visibility sweep pattern.
 * ----------------------------------------------------------------------- */

/**
 * Return the drawn-frame revision stamp for @p ref.
 */
BSG_EXPORT extern uint64_t
bsg_scene_drawn_rev(bsg_scene_ref ref);

/* -----------------------------------------------------------------------
 * Draw mode
 *
 * The draw mode is stored in bsg_appearance_settings for compatibility with legacy
 * command paths.  New code should prefer the draw-intent and render-settings
 * layers rather than reading the raw field directly.
 * ----------------------------------------------------------------------- */

/**
 * Return the draw mode for @p ref.
 */
BSG_EXPORT extern int
bsg_scene_dmode(bsg_scene_ref ref);

/**
 * Set the draw mode for @p ref.
 */
BSG_EXPORT extern void
bsg_scene_set_dmode(bsg_scene_ref ref, int draw_mode);

/* -----------------------------------------------------------------------
 * Strict draw fallback
 *
 * By default, shaded and hidden-line primitive tessellation failures retain
 * legacy interactive behavior by falling back to typed wireframe geometry.
 * Strict fallback mode makes those failures explicit: geometry is cleared and
 * the realization remains stale.
 * ----------------------------------------------------------------------- */

/**
 * Return non-zero when shaded/hidden-line tessellation failures should not
 * fall back to wireframe geometry.
 */
BSG_EXPORT extern int
bsg_scene_strict_fallback(bsg_scene_ref ref);

/**
 * Set strict shaded/hidden-line fallback behavior for @p ref.
 */
BSG_EXPORT extern void
bsg_scene_set_strict_fallback(bsg_scene_ref ref, int strict_fallback);

/* -----------------------------------------------------------------------
 * Transparency (transparency)
 *
 * transparency is stored in bsg_appearance_settings as a fastf_t in [0.0, 1.0].
 * ----------------------------------------------------------------------- */

/**
 * Return the transparency value for @p ref.
 */
BSG_EXPORT extern fastf_t
bsg_scene_transparency(bsg_scene_ref ref);

/**
 * Set the transparency value for @p ref (range [0.0, 1.0]).
 */
BSG_EXPORT extern void
bsg_scene_set_transparency(bsg_scene_ref ref, fastf_t t);

__END_DECLS

#endif /* BSG_APPEARANCE_H */

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
