/*                  V I E W _ S T A T E . C
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
/** @file libbsg/view_state.c */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bsg/camera.h"
#include "bsg/render_settings.h"
#include "bsg/view_state.h"
#include "view_state_private.h"

struct bsg_view_state {
    struct bsg_view_settings *settings;
    struct bsg_camera camera;
    struct bsg_render_settings render_settings;
    struct bsg_view_refresh_state refresh;
};

static void
_refresh_init(struct bsg_view_refresh_state *refresh)
{
    if (!refresh)
	return;
    memset(refresh, 0, sizeof(*refresh));
    refresh->enabled = 1;
}

static void
_camera_from_view(struct bsg_camera *cam, const struct bsg_view *v)
{
    if (!cam)
	return;

    MAT_IDN(cam->rotation);
    MAT_IDN(cam->center);
    MAT_IDN(cam->model2view);
    MAT_IDN(cam->view2model);
    cam->scale = 1.0;
    cam->perspective = 0.0;
    VSETALL(cam->aet, 0.0);
    VSET(cam->eye_pos, 0.0, 0.0, 1.0);

    if (!v)
	return;

    cam->scale = v->gv_scale;
    cam->perspective = v->gv_perspective;
    VMOVE(cam->aet, v->gv_aet);
    VMOVE(cam->eye_pos, v->gv_eye_pos);
    MAT_COPY(cam->rotation, v->gv_rotation);
    MAT_COPY(cam->center, v->gv_center);
    MAT_COPY(cam->model2view, v->gv_model2view);
    MAT_COPY(cam->view2model, v->gv_view2model);
}

struct bsg_view_state *
bsg_view_state_create(void)
{
    struct bsg_view_state *state;

    BU_ALLOC(state, struct bsg_view_state);
    state->settings = NULL;
    _camera_from_view(&state->camera, NULL);
    bsg_render_settings_init_defaults(&state->render_settings);
    _refresh_init(&state->refresh);

    return state;
}

void
bsg_view_state_destroy(struct bsg_view_state *state)
{
    if (!state)
	return;
    BU_PUT(state, struct bsg_view_state);
}

struct bsg_view_state *
bsg_view_state_active(struct bsg_view *v)
{
    if (!v)
	return NULL;

    if (!v->gv_state)
	v->gv_state = bsg_view_state_create();

    v->gv_state->settings = bsg_view_settings_active(v);
    return v->gv_state;
}

const struct bsg_view_state *
bsg_view_state_active_const(const struct bsg_view *v)
{
    if (!v)
	return NULL;
    return v->gv_state;
}

void
bsg_view_state_sync_from_view(struct bsg_view_state *state, const struct bsg_view *v)
{
    if (!state)
	return;

    state->settings = (struct bsg_view_settings *)bsg_view_settings_active_const(v);
    _camera_from_view(&state->camera, v);
    bsg_render_settings_from_view(&state->render_settings, v);
}

void
bsg_view_state_apply_to_view(const struct bsg_view_state *state, struct bsg_view *v)
{
    struct bsg_camera cam;

    if (!state || !v)
	return;

    cam = state->camera;
    bsg_camera_apply(&cam, v);
    bsg_render_settings_apply_to_view(&state->render_settings, v);
    if (v->gv_state)
	bsg_view_state_sync_from_view(v->gv_state, v);
}

struct bsg_camera *
bsg_view_state_camera(struct bsg_view_state *state)
{
    return state ? &state->camera : NULL;
}

const struct bsg_camera *
bsg_view_state_camera_const(const struct bsg_view_state *state)
{
    return state ? &state->camera : NULL;
}

struct bsg_render_settings *
bsg_view_state_render_settings(struct bsg_view_state *state)
{
    return state ? &state->render_settings : NULL;
}

const struct bsg_render_settings *
bsg_view_state_render_settings_const(const struct bsg_view_state *state)
{
    return state ? &state->render_settings : NULL;
}

int
bsg_view_refresh_get(const struct bsg_view *v, struct bsg_view_refresh_state *record)
{
    const struct bsg_view_state *state;

    if (!record)
	return 0;

    state = bsg_view_state_active_const(v);
    if (state) {
	*record = state->refresh;
	return 1;
    }

    _refresh_init(record);
    return v != NULL;
}

void
bsg_view_refresh_set(struct bsg_view *v, const struct bsg_view_refresh_state *record)
{
    struct bsg_view_state *state;

    if (!v || !record)
	return;

    state = bsg_view_state_active(v);
    if (!state)
	return;

    state->refresh = *record;
    if (state->refresh.suppress_depth < 0)
	state->refresh.suppress_depth = 0;
    state->refresh.enabled = state->refresh.enabled ? 1 : 0;
    if (state->refresh.drawn_count < 0)
	state->refresh.drawn_count = 0;
}

void
bsg_view_refresh_request(struct bsg_view *v, uint32_t flags)
{
    struct bsg_view_state *state = bsg_view_state_active(v);

    if (!state)
	return;

    state->refresh.dirty_flags |= flags ? flags : BSG_VIEW_REFRESH_ALL;
    state->refresh.request_revision++;
}

int
bsg_view_refresh_enabled(const struct bsg_view *v)
{
    const struct bsg_view_state *state = bsg_view_state_active_const(v);

    return state ? state->refresh.enabled : 1;
}

void
bsg_view_refresh_set_enabled(struct bsg_view *v, int enabled)
{
    struct bsg_view_state *state = bsg_view_state_active(v);

    if (!state)
	return;

    state->refresh.enabled = enabled ? 1 : 0;
}

int
bsg_view_refresh_suppressed(const struct bsg_view *v)
{
    const struct bsg_view_state *state = bsg_view_state_active_const(v);

    return state ? (state->refresh.suppress_depth > 0) : 0;
}

void
bsg_view_refresh_suppress_begin(struct bsg_view *v)
{
    struct bsg_view_state *state = bsg_view_state_active(v);

    if (!state)
	return;

    state->refresh.suppress_depth++;
}

void
bsg_view_refresh_suppress_end(struct bsg_view *v)
{
    struct bsg_view_state *state = bsg_view_state_active(v);

    if (!state || state->refresh.suppress_depth <= 0)
	return;

    state->refresh.suppress_depth--;
}

int
bsg_view_refresh_dirty(const struct bsg_view *v)
{
    const struct bsg_view_state *state = bsg_view_state_active_const(v);

    if (!state)
	return 0;

    return (state->refresh.enabled &&
	    state->refresh.suppress_depth == 0 &&
	    state->refresh.dirty_flags != 0);
}

uint32_t
bsg_view_refresh_consume(struct bsg_view *v)
{
    struct bsg_view_state *state = bsg_view_state_active(v);
    uint32_t flags;

    if (!state || !state->refresh.enabled || state->refresh.suppress_depth > 0)
	return 0;

    flags = state->refresh.dirty_flags;
    state->refresh.dirty_flags = 0;
    state->refresh.completed_revision = state->refresh.request_revision;
    return flags;
}

void
bsg_view_refresh_complete(struct bsg_view *v)
{
    struct bsg_view_state *state = bsg_view_state_active(v);

    if (!state)
	return;

    state->refresh.dirty_flags = 0;
    state->refresh.completed_revision = state->refresh.request_revision;
}

int
bsg_view_refresh_drawn_count(const struct bsg_view *v)
{
    const struct bsg_view_state *state = bsg_view_state_active_const(v);

    return state ? state->refresh.drawn_count : 0;
}

void
bsg_view_refresh_set_drawn_count(struct bsg_view *v, int count)
{
    struct bsg_view_state *state = bsg_view_state_active(v);

    if (!state)
	return;

    state->refresh.drawn_count = count > 0 ? count : 0;
}

struct bsg_view_settings *
bsg_view_settings_active(struct bsg_view *v)
{
    if (!v)
	return NULL;
    return v->settings_active ? v->settings_active : v->settings_local;
}

const struct bsg_view_settings *
bsg_view_settings_active_const(const struct bsg_view *v)
{
    if (!v)
	return NULL;
    return v->settings_active ? v->settings_active : v->settings_local;
}

static struct bsg_view_settings *
_view_state_settings(struct bsg_view *v)
{
    struct bsg_view_state *state = bsg_view_state_active(v);

    if (state && state->settings)
	return state->settings;

    return bsg_view_settings_active(v);
}

static const struct bsg_view_settings *
_view_state_settings_const(const struct bsg_view *v)
{
    const struct bsg_view_state *state = bsg_view_state_active_const(v);

    if (state && state->settings)
	return state->settings;

    return bsg_view_settings_active_const(v);
}

struct bsg_selection *
bsg_view_selection(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    return s ? s->gv_selected : NULL;
}

const struct bsg_selection *
bsg_view_selection_const(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? s->gv_selected : NULL;
}

int
bsg_view_scene_attached(const struct bsg_view *v)
{
    return (v && v->gv_scene) ? 1 : 0;
}


int
bsg_view_scene_shared(const struct bsg_view *a, const struct bsg_view *b)
{
    return (a && b && a->gv_scene && a->gv_scene == b->gv_scene) ? 1 : 0;
}


struct bsg_adc_state *
bsg_view_adc(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    return s ? &s->gv_adc : NULL;
}

const struct bsg_adc_state *
bsg_view_adc_const(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? &s->gv_adc : NULL;
}

struct bsg_other_state *
bsg_view_center_dot(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    return s ? &s->gv_center_dot : NULL;
}

const struct bsg_other_state *
bsg_view_center_dot_const(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? &s->gv_center_dot : NULL;
}

struct bsg_grid_state *
bsg_view_grid(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    return s ? &s->gv_grid : NULL;
}

const struct bsg_grid_state *
bsg_view_grid_const(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? &s->gv_grid : NULL;
}

struct bsg_axes *
bsg_view_model_axes(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    return s ? &s->gv_model_axes : NULL;
}

const struct bsg_axes *
bsg_view_model_axes_const(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? &s->gv_model_axes : NULL;
}

struct bsg_axes *
bsg_view_view_axes(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    return s ? &s->gv_view_axes : NULL;
}

const struct bsg_axes *
bsg_view_view_axes_const(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? &s->gv_view_axes : NULL;
}

struct bsg_other_state *
bsg_view_scale_state(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    return s ? &s->gv_view_scale : NULL;
}

const struct bsg_other_state *
bsg_view_scale_state_const(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? &s->gv_view_scale : NULL;
}

struct bsg_params_state *
bsg_view_params(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    return s ? &s->gv_view_params : NULL;
}

const struct bsg_params_state *
bsg_view_params_const(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? &s->gv_view_params : NULL;
}

struct bsg_interactive_rect_state *
bsg_view_interactive_rect(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    return s ? &s->gv_rect : NULL;
}

const struct bsg_interactive_rect_state *
bsg_view_interactive_rect_const(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? &s->gv_rect : NULL;
}

int
bsg_view_adc_get(const struct bsg_view *v, struct bsg_adc_state *record)
{
    const struct bsg_adc_state *src = bsg_view_adc_const(v);
    if (!src || !record)
	return 0;
    *record = *src;
    return 1;
}

void
bsg_view_adc_set(struct bsg_view *v, const struct bsg_adc_state *record)
{
    struct bsg_adc_state *dst = bsg_view_adc(v);
    if (dst && record)
	*dst = *record;
}

int
bsg_view_center_dot_get(const struct bsg_view *v, struct bsg_other_state *record)
{
    const struct bsg_other_state *src = bsg_view_center_dot_const(v);
    if (!src || !record)
	return 0;
    *record = *src;
    return 1;
}

void
bsg_view_center_dot_set(struct bsg_view *v, const struct bsg_other_state *record)
{
    struct bsg_other_state *dst = bsg_view_center_dot(v);
    if (dst && record)
	*dst = *record;
}

int
bsg_view_grid_get(const struct bsg_view *v, struct bsg_grid_state *record)
{
    const struct bsg_grid_state *src = bsg_view_grid_const(v);
    if (!src || !record)
	return 0;
    *record = *src;
    return 1;
}

void
bsg_view_grid_set(struct bsg_view *v, const struct bsg_grid_state *record)
{
    struct bsg_grid_state *dst = bsg_view_grid(v);
    if (dst && record)
	*dst = *record;
}

int
bsg_view_model_axes_get(const struct bsg_view *v, struct bsg_axes *record)
{
    const struct bsg_axes *src = bsg_view_model_axes_const(v);
    if (!src || !record)
	return 0;
    *record = *src;
    return 1;
}

void
bsg_view_model_axes_set(struct bsg_view *v, const struct bsg_axes *record)
{
    struct bsg_axes *dst = bsg_view_model_axes(v);
    if (dst && record)
	*dst = *record;
}

int
bsg_view_view_axes_get(const struct bsg_view *v, struct bsg_axes *record)
{
    const struct bsg_axes *src = bsg_view_view_axes_const(v);
    if (!src || !record)
	return 0;
    *record = *src;
    return 1;
}

void
bsg_view_view_axes_set(struct bsg_view *v, const struct bsg_axes *record)
{
    struct bsg_axes *dst = bsg_view_view_axes(v);
    if (dst && record)
	*dst = *record;
}

int
bsg_view_scale_state_get(const struct bsg_view *v, struct bsg_other_state *record)
{
    const struct bsg_other_state *src = bsg_view_scale_state_const(v);
    if (!src || !record)
	return 0;
    *record = *src;
    return 1;
}

void
bsg_view_scale_state_set(struct bsg_view *v, const struct bsg_other_state *record)
{
    struct bsg_other_state *dst = bsg_view_scale_state(v);
    if (dst && record)
	*dst = *record;
}

int
bsg_view_params_get(const struct bsg_view *v, struct bsg_params_state *record)
{
    const struct bsg_params_state *src = bsg_view_params_const(v);
    if (!src || !record)
	return 0;
    *record = *src;
    return 1;
}

void
bsg_view_params_set(struct bsg_view *v, const struct bsg_params_state *record)
{
    struct bsg_params_state *dst = bsg_view_params(v);
    if (dst && record)
	*dst = *record;
}

int
bsg_view_interactive_rect_get(const struct bsg_view *v, struct bsg_interactive_rect_state *record)
{
    const struct bsg_interactive_rect_state *src = bsg_view_interactive_rect_const(v);
    if (!src || !record)
	return 0;
    *record = *src;
    return 1;
}

void
bsg_view_interactive_rect_set(struct bsg_view *v, const struct bsg_interactive_rect_state *record)
{
    struct bsg_interactive_rect_state *dst = bsg_view_interactive_rect(v);
    if (dst && record)
	*dst = *record;
}

int
bsg_view_settings_shared(const struct bsg_view *a, const struct bsg_view *b)
{
    const struct bsg_view_settings *as = _view_state_settings_const(a);
    const struct bsg_view_settings *bs = _view_state_settings_const(b);

    return (as && as == bs);
}

int
bsg_view_framebuffer_mode(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? s->gv_fb_mode : 0;
}

void
bsg_view_set_framebuffer_mode(struct bsg_view *v, int mode)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    if (!s)
	return;
    s->gv_fb_mode = mode;
}

int
bsg_view_cleared(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? s->gv_cleared : 0;
}

void
bsg_view_set_cleared(struct bsg_view *v, int cleared)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    if (!s)
	return;
    s->gv_cleared = cleared ? 1 : 0;
}

int
bsg_view_zclip(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? s->gv_zclip : 0;
}

void
bsg_view_set_zclip(struct bsg_view *v, int zclip)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    if (!s)
	return;
    s->gv_zclip = zclip;
}

int
bsg_view_lod_source_policy_get(const struct bsg_view *v,
			       struct bsg_lod_source_policy_settings *policy)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    if (!s || !policy)
	return 0;
    *policy = s->lod_source_policy;
    return 1;
}

void
bsg_view_lod_source_policy_set(struct bsg_view *v,
			       const struct bsg_lod_source_policy_settings *policy)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    if (!s || !policy)
	return;
    s->lod_source_policy = *policy;
}

uint64_t
bsg_view_frametime(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? s->gv_frametime : 0;
}

void
bsg_view_set_frametime(struct bsg_view *v, uint64_t frametime)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    if (!s)
	return;
    s->gv_frametime = frametime;
}

int
bsg_view_snap_lines(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? s->gv_snap_lines : 0;
}

void
bsg_view_set_snap_lines(struct bsg_view *v, int enabled)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    if (!s)
	return;
    s->gv_snap_lines = enabled ? 1 : 0;
}

double
bsg_view_snap_tolerance_factor(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? s->gv_snap_tol_factor : 1.0;
}

void
bsg_view_set_snap_tolerance_factor(struct bsg_view *v, double factor)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    if (!s)
	return;
    s->gv_snap_tol_factor = factor;
}

int
bsg_view_snap_source_flags(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    return s ? s->gv_snap_flags : 0;
}

void
bsg_view_set_snap_source_flags(struct bsg_view *v, int flags)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    if (!s)
	return;
    s->gv_snap_flags = flags;
}

bsg_snap_kind_mask
bsg_view_snap_kind_mask(const struct bsg_view *v)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    bsg_snap_kind_mask kinds = 0;

    if (!s)
	return 0;

    if (s->gv_snap_lines)
	kinds |= (bsg_snap_kind_mask)BSG_SNAP_KIND_ENDPOINT;
    if (s->gv_grid.snap)
	kinds |= (bsg_snap_kind_mask)BSG_SNAP_KIND_GRID;

    return kinds;
}

bsg_snap_kind_mask
bsg_view_prepare_tcl_snap(struct bsg_view *v)
{
    bsg_snap_kind_mask kinds = bsg_view_snap_kind_mask(v);

    if (bsg_view_snap_lines(v))
	bsg_view_set_snap_source_flags(v, BSG_SNAP_TCL);

    return kinds;
}

void
bsg_view_snap_exclude_feature_clear(struct bsg_view *v)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    bsg_feature_ref null_ref = BSG_FEATURE_REF_NULL_INIT;
    if (s)
	s->gv_snap_exclude_feature = null_ref;
}

void
bsg_view_snap_exclude_feature_set(struct bsg_view *v, bsg_feature_ref ref)
{
    struct bsg_view_settings *s = _view_state_settings(v);
    if (s)
	s->gv_snap_exclude_feature = ref;
}

int
bsg_view_snap_exclude_feature_get(const struct bsg_view *v, bsg_feature_ref *ref)
{
    const struct bsg_view_settings *s = _view_state_settings_const(v);
    if (!s || !ref)
	return 0;
    *ref = s->gv_snap_exclude_feature;
    return !bsg_feature_ref_is_null(*ref);
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
