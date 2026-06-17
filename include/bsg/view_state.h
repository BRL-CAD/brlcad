/*                  V I E W _ S T A T E . H
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
 * @brief Canonical command/UI view state accessors.
 *
 * A bsg_view_state is the semantic boundary between command/UI code and the
 * internal runtime view caches.  Callers should edit camera, rendering,
 * framebuffer, snapping, HUD/faceplate, and interaction records through this
 * API rather than reaching into bsg_view storage directly.
 */
/** @{ */
/* @file bsg/view_state.h */

#ifndef BSG_VIEW_STATE_H
#define BSG_VIEW_STATE_H

#include "common.h"

#include "bsg/defines.h"
#include "bsg/camera.h"
#include "bsg/render_settings.h"
#include "bsg/snap_action.h"

__BEGIN_DECLS

struct bsg_view_state;

#define BSG_VIEW_REFRESH_VIEW        0x00000001u
#define BSG_VIEW_REFRESH_DRAW        0x00000002u
#define BSG_VIEW_REFRESH_EDIT        0x00000004u
#define BSG_VIEW_REFRESH_FRAMEBUFFER 0x00000008u
#define BSG_VIEW_REFRESH_OVERLAY     0x00000010u
#define BSG_VIEW_REFRESH_FORCE       0x80000000u
#define BSG_VIEW_REFRESH_ALL         0xffffffffu

struct bsg_view_refresh_state {
    uint32_t dirty_flags;
    uint64_t request_revision;
    uint64_t completed_revision;
    int suppress_depth;
    int enabled;
    int drawn_count;
};

BSG_EXPORT extern struct bsg_view_state *
bsg_view_state_create(void);

BSG_EXPORT extern void
bsg_view_state_destroy(struct bsg_view_state *state);

BSG_EXPORT extern struct bsg_view_state *
bsg_view_state_active(struct bsg_view *v);

BSG_EXPORT extern const struct bsg_view_state *
bsg_view_state_active_const(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_state_sync_from_view(struct bsg_view_state *state, const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_state_apply_to_view(const struct bsg_view_state *state, struct bsg_view *v);

BSG_EXPORT extern struct bsg_camera *
bsg_view_state_camera(struct bsg_view_state *state);

BSG_EXPORT extern const struct bsg_camera *
bsg_view_state_camera_const(const struct bsg_view_state *state);

BSG_EXPORT extern struct bsg_render_settings *
bsg_view_state_render_settings(struct bsg_view_state *state);

BSG_EXPORT extern const struct bsg_render_settings *
bsg_view_state_render_settings_const(const struct bsg_view_state *state);

BSG_EXPORT extern int
bsg_view_refresh_get(const struct bsg_view *v, struct bsg_view_refresh_state *record);

BSG_EXPORT extern void
bsg_view_refresh_set(struct bsg_view *v, const struct bsg_view_refresh_state *record);

BSG_EXPORT extern void
bsg_view_refresh_request(struct bsg_view *v, uint32_t flags);

BSG_EXPORT extern int
bsg_view_refresh_dirty(const struct bsg_view *v);

BSG_EXPORT extern uint32_t
bsg_view_refresh_consume(struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_refresh_complete(struct bsg_view *v);

BSG_EXPORT extern int
bsg_view_refresh_enabled(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_refresh_set_enabled(struct bsg_view *v, int enabled);

BSG_EXPORT extern void
bsg_view_refresh_suppress_begin(struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_refresh_suppress_end(struct bsg_view *v);

BSG_EXPORT extern int
bsg_view_refresh_suppressed(const struct bsg_view *v);

BSG_EXPORT extern int
bsg_view_refresh_drawn_count(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_refresh_set_drawn_count(struct bsg_view *v, int count);

BSG_EXPORT extern struct bsg_selection *
bsg_view_selection(struct bsg_view *v);

BSG_EXPORT extern const struct bsg_selection *
bsg_view_selection_const(const struct bsg_view *v);

BSG_EXPORT extern int
bsg_view_scene_attached(const struct bsg_view *v);

BSG_EXPORT extern int
bsg_view_scene_shared(const struct bsg_view *a, const struct bsg_view *b);

BSG_EXPORT extern int
bsg_view_adc_get(const struct bsg_view *v, struct bsg_adc_state *record);

BSG_EXPORT extern void
bsg_view_adc_set(struct bsg_view *v, const struct bsg_adc_state *record);

BSG_EXPORT extern int
bsg_view_center_dot_get(const struct bsg_view *v, struct bsg_other_state *record);

BSG_EXPORT extern void
bsg_view_center_dot_set(struct bsg_view *v, const struct bsg_other_state *record);

BSG_EXPORT extern int
bsg_view_grid_get(const struct bsg_view *v, struct bsg_grid_state *record);

BSG_EXPORT extern void
bsg_view_grid_set(struct bsg_view *v, const struct bsg_grid_state *record);

BSG_EXPORT extern int
bsg_view_model_axes_get(const struct bsg_view *v, struct bsg_axes *record);

BSG_EXPORT extern void
bsg_view_model_axes_set(struct bsg_view *v, const struct bsg_axes *record);

BSG_EXPORT extern int
bsg_view_view_axes_get(const struct bsg_view *v, struct bsg_axes *record);

BSG_EXPORT extern void
bsg_view_view_axes_set(struct bsg_view *v, const struct bsg_axes *record);

BSG_EXPORT extern int
bsg_view_scale_state_get(const struct bsg_view *v, struct bsg_other_state *record);

BSG_EXPORT extern void
bsg_view_scale_state_set(struct bsg_view *v, const struct bsg_other_state *record);

BSG_EXPORT extern int
bsg_view_params_get(const struct bsg_view *v, struct bsg_params_state *record);

BSG_EXPORT extern void
bsg_view_params_set(struct bsg_view *v, const struct bsg_params_state *record);

BSG_EXPORT extern int
bsg_view_interactive_rect_get(const struct bsg_view *v, struct bsg_interactive_rect_state *record);

BSG_EXPORT extern void
bsg_view_interactive_rect_set(struct bsg_view *v, const struct bsg_interactive_rect_state *record);

BSG_EXPORT extern int
bsg_view_settings_shared(const struct bsg_view *a, const struct bsg_view *b);

BSG_EXPORT extern int
bsg_view_framebuffer_mode(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_set_framebuffer_mode(struct bsg_view *v, int mode);

BSG_EXPORT extern int
bsg_view_cleared(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_set_cleared(struct bsg_view *v, int cleared);

BSG_EXPORT extern int
bsg_view_zclip(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_set_zclip(struct bsg_view *v, int zclip);

BSG_EXPORT extern int
bsg_view_lod_source_policy_get(const struct bsg_view *v,
			       struct bsg_lod_source_policy_settings *policy);

BSG_EXPORT extern void
bsg_view_lod_source_policy_set(struct bsg_view *v,
			       const struct bsg_lod_source_policy_settings *policy);

BSG_EXPORT extern uint64_t
bsg_view_frametime(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_set_frametime(struct bsg_view *v, uint64_t frametime);

BSG_EXPORT extern int
bsg_view_snap_lines(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_set_snap_lines(struct bsg_view *v, int enabled);

BSG_EXPORT extern double
bsg_view_snap_tolerance_factor(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_set_snap_tolerance_factor(struct bsg_view *v, double factor);

BSG_EXPORT extern int
bsg_view_snap_source_flags(const struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_set_snap_source_flags(struct bsg_view *v, int flags);

BSG_EXPORT extern bsg_snap_kind_mask
bsg_view_snap_kind_mask(const struct bsg_view *v);

BSG_EXPORT extern bsg_snap_kind_mask
bsg_view_prepare_tcl_snap(struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_snap_exclude_feature_clear(struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_snap_exclude_feature_set(struct bsg_view *v, bsg_feature_ref ref);

BSG_EXPORT extern int
bsg_view_snap_exclude_feature_get(const struct bsg_view *v, bsg_feature_ref *ref);

__END_DECLS

#endif /* BSG_VIEW_STATE_H */

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
