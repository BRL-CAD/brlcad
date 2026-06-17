/*              V I E W _ S T A T E _ P R I V A T E . H
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
/** @file libbsg/view_state_private.h */

#ifndef LIBBSG_VIEW_STATE_PRIVATE_H
#define LIBBSG_VIEW_STATE_PRIVATE_H

#include "bsg/view_state.h"

__BEGIN_DECLS

/* Private storage backing public view-state records.  Applications edit these
 * fields through bsg/view_state.h snapshot/commit APIs, not by dereferencing
 * bsg_view storage. */
struct bsg_view_settings {
    int            gv_snap_lines;
    double 	   gv_snap_tol_factor;
    int		   gv_snap_flags;
    bsg_feature_ref gv_snap_exclude_feature;
    int            gv_cleared;
    int            gv_zclip;
    int            gv_autoview;

    struct bsg_lod_source_policy_settings lod_source_policy;

    struct bsg_axes           gv_model_axes;
    struct bsg_axes           gv_view_axes;
    struct bsg_grid_state     gv_grid;
    struct bsg_other_state    gv_center_dot;
    struct bsg_params_state   gv_view_params;
    struct bsg_other_state    gv_view_scale;
    double                    gv_frametime;

    int                       gv_fb_mode;

    struct bsg_adc_state              gv_adc;
    struct bsg_interactive_rect_state gv_rect;

    struct bsg_selection              *gv_selected;
};

struct bsg_view_settings *
bsg_view_settings_active(struct bsg_view *v);

const struct bsg_view_settings *
bsg_view_settings_active_const(const struct bsg_view *v);

void
bsg_settings_init(struct bsg_view_settings *s);

struct bsg_adc_state *
bsg_view_adc(struct bsg_view *v);

const struct bsg_adc_state *
bsg_view_adc_const(const struct bsg_view *v);

struct bsg_other_state *
bsg_view_center_dot(struct bsg_view *v);

const struct bsg_other_state *
bsg_view_center_dot_const(const struct bsg_view *v);

struct bsg_grid_state *
bsg_view_grid(struct bsg_view *v);

const struct bsg_grid_state *
bsg_view_grid_const(const struct bsg_view *v);

struct bsg_axes *
bsg_view_model_axes(struct bsg_view *v);

const struct bsg_axes *
bsg_view_model_axes_const(const struct bsg_view *v);

struct bsg_axes *
bsg_view_view_axes(struct bsg_view *v);

const struct bsg_axes *
bsg_view_view_axes_const(const struct bsg_view *v);

struct bsg_other_state *
bsg_view_scale_state(struct bsg_view *v);

const struct bsg_other_state *
bsg_view_scale_state_const(const struct bsg_view *v);

struct bsg_params_state *
bsg_view_params(struct bsg_view *v);

const struct bsg_params_state *
bsg_view_params_const(const struct bsg_view *v);

struct bsg_interactive_rect_state *
bsg_view_interactive_rect(struct bsg_view *v);

const struct bsg_interactive_rect_state *
bsg_view_interactive_rect_const(const struct bsg_view *v);

__END_DECLS

#endif /* LIBBSG_VIEW_STATE_PRIVATE_H */
