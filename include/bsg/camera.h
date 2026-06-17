/*                      C A M E R A . H
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
 * Camera snapshot — capture and restore bsg_view camera state.
 *
 * A `bsg_camera` records the subset of `bsg_view` fields that fully
 * describe the camera: scale, perspective, aet, eye_pos, rotation matrix,
 * center matrix, model-to-view and view-to-model matrices.
 *
 * bsg_camera_snapshot() extracts these fields from a live view into a
 * freshly-allocated bsg_camera.  bsg_camera_apply() restores them.
 * The snapshot/apply pair enables undo/redo, camera presets, and
 * renderer-neutral camera hand-off.
 */
/** @{ */
/* @file bsg/camera.h */

#ifndef BSG_CAMERA_H
#define BSG_CAMERA_H

#include "common.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/field.h"
#include "bsg/node.h"

__BEGIN_DECLS

#define BSG_CAMERA_ORTHOGRAPHIC 0
#define BSG_CAMERA_PERSPECTIVE  1

/**
 * Camera state snapshot.
 *
 * All fields are plain-value copies — no pointers to live view storage.
 * Allocate with bsg_camera_create() or bsg_camera_snapshot(); release
 * with bsg_camera_destroy().
 */
struct bsg_camera {
    fastf_t scale;            /**< @brief gv_scale at snapshot time */
    fastf_t perspective;      /**< @brief gv_perspective */
    vect_t  aet;              /**< @brief azimuth / elevation / twist */
    vect_t  eye_pos;          /**< @brief eye position in model space */
    mat_t   rotation;         /**< @brief gv_rotation */
    mat_t   center;           /**< @brief gv_center */
    mat_t   model2view;       /**< @brief gv_model2view */
    mat_t   view2model;       /**< @brief gv_view2model */
};

/**
 * Allocate an identity camera (scale=1, perspective=0, identity matrices).
 * Returns NULL on allocation failure.
 */
BSG_EXPORT extern struct bsg_camera *
bsg_camera_create(void);

/**
 * Release a camera previously allocated by bsg_camera_create() or
 * bsg_camera_snapshot().
 * No-op if @p cam is NULL.
 */
BSG_EXPORT extern void
bsg_camera_destroy(struct bsg_camera *cam);

/**
 * Capture the current camera state from @p v into a newly allocated
 * bsg_camera.
 * Returns NULL if @p v is NULL or on allocation failure.
 */
BSG_EXPORT extern struct bsg_camera *
bsg_camera_snapshot(const struct bsg_view *v);

/**
 * Restore the camera state stored in @p cam to @p v.
 * Only the camera fields (scale, perspective, aet, eye_pos, rotation,
 * center, model2view, view2model) are written; all other view state is
 * left unchanged.
 * No-op if either argument is NULL.
 */
BSG_EXPORT extern void
bsg_camera_apply(struct bsg_camera *cam, struct bsg_view *v);


/* -----------------------------------------------------------------------
 * Camera-node bindings
 *
 * A view can carry a persistent camera snapshot that is consulted by the
 * render request executor.  When a camera node is bound, multi-view
 * layouts can share a single scene root and bind per-view camera nodes
 * without duplicating scene geometry.
 *
 * The view does NOT take ownership of the camera node; callers must
 * ensure the node outlives the view or explicitly unbind it before
 * destroying it.
 * ----------------------------------------------------------------------- */

/**
 * Bind camera snapshot @p cam to view @p v.
 *
 * The snapshot is stored as a borrowed pointer on the view.
 * Passing NULL unbinds any previously bound camera.
 * No-op if @p v is NULL.
 */
BSG_EXPORT extern void
bsg_view_set_camera_node(struct bsg_view *v, struct bsg_camera *cam);

/**
 * Return the camera snapshot bound to view @p v, or NULL if none is bound.
 */
BSG_EXPORT extern struct bsg_camera *
bsg_view_get_camera_node(const struct bsg_view *v);

/**
 * Apply the camera node bound to @p v (if any) to restore the camera
 * state.  Equivalent to:
 *   bsg_camera_apply(bsg_view_get_camera_node(v), v)
 * but handles a missing camera node gracefully (no-op).
 */
BSG_EXPORT extern void
bsg_view_apply_camera_node(struct bsg_view *v);

BSG_EXPORT extern bsg_field_ref bsg_camera_ref_projection_field(bsg_camera_ref camera);
BSG_EXPORT extern bsg_field_ref bsg_camera_ref_perspective_field(bsg_camera_ref camera);
BSG_EXPORT extern bsg_field_ref bsg_camera_ref_position_field(bsg_camera_ref camera);
BSG_EXPORT extern bsg_field_ref bsg_camera_ref_orientation_field(bsg_camera_ref camera);
BSG_EXPORT extern bsg_field_ref bsg_camera_ref_near_distance_field(bsg_camera_ref camera);
BSG_EXPORT extern bsg_field_ref bsg_camera_ref_far_distance_field(bsg_camera_ref camera);

BSG_EXPORT extern int bsg_camera_ref_set_projection(bsg_camera_ref camera, int projection);
BSG_EXPORT extern int bsg_camera_ref_projection(bsg_camera_ref camera);
BSG_EXPORT extern int bsg_camera_ref_set_perspective(bsg_camera_ref camera, double perspective);
BSG_EXPORT extern double bsg_camera_ref_perspective(bsg_camera_ref camera);
BSG_EXPORT extern int bsg_camera_ref_set_position(bsg_camera_ref camera, const point_t position);
BSG_EXPORT extern int bsg_camera_ref_position(bsg_camera_ref camera, point_t position);
BSG_EXPORT extern int bsg_camera_ref_set_orientation(bsg_camera_ref camera, const mat_t orientation);
BSG_EXPORT extern int bsg_camera_ref_orientation(bsg_camera_ref camera, mat_t orientation);
BSG_EXPORT extern int bsg_camera_ref_set_near_distance(bsg_camera_ref camera, double distance);
BSG_EXPORT extern double bsg_camera_ref_near_distance(bsg_camera_ref camera);
BSG_EXPORT extern int bsg_camera_ref_set_far_distance(bsg_camera_ref camera, double distance);
BSG_EXPORT extern double bsg_camera_ref_far_distance(bsg_camera_ref camera);

__END_DECLS

#endif /* BSG_CAMERA_H */

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
