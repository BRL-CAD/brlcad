/*                       A C T I O N . H
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
 * BSG action framework.
 *
 * Actions are owning opaque objects applied to typed node refs.  They are the
 * public execution model for render, backend-scene, bounds, pick, snap,
 * measure, search, and export/write operations.  Implementations share the
 * source-private traversal engine and receive resolved bsg_state_ref frames;
 * public consumers do not walk raw graph nodes.
 */
/** @{ */
/* @file bsg/action.h */

#ifndef BSG_ACTION_H
#define BSG_ACTION_H

#include "common.h"
#include "vmath.h"
#include "bu/ptbl.h"
#include "bsg/defines.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg/state.h"

__BEGIN_DECLS

struct bsg_backend_scene;
struct bsg_backend_scene_update_result;
struct bsg_view;
struct bu_list;
struct bu_vls;
struct bsg_export_request;
struct bsg_export_result;
struct bsg_measure_result;
struct bsg_pick_result;
struct bsg_render_batch;
struct bsg_render_request;
struct bsg_snap_result;

typedef struct bsg_action_ref {
    void *opaque;
} bsg_action_ref;

#define BSG_ACTION_REF_NULL_INIT { NULL }

BSG_EXPORT extern bsg_action_ref bsg_action_ref_null(void);
BSG_EXPORT extern int bsg_action_ref_is_null(bsg_action_ref action);
BSG_EXPORT extern bsg_type_id bsg_action_ref_type(bsg_action_ref action);
BSG_EXPORT extern void bsg_action_ref_destroy(bsg_action_ref action);
BSG_EXPORT extern int bsg_action_apply(bsg_action_ref action,
				       bsg_node_ref root);

BSG_EXPORT extern bsg_type_id bsg_bbox_action_type(void);
BSG_EXPORT extern bsg_type_id bsg_collect_action_type(void);
BSG_EXPORT extern bsg_type_id bsg_render_action_type(void);
BSG_EXPORT extern bsg_type_id bsg_backend_scene_action_type(void);
BSG_EXPORT extern bsg_type_id bsg_pick_action_type(void);
BSG_EXPORT extern bsg_type_id bsg_snap_action_type(void);
BSG_EXPORT extern bsg_type_id bsg_measure_action_type(void);
BSG_EXPORT extern bsg_type_id bsg_search_action_type(void);
BSG_EXPORT extern bsg_type_id bsg_export_records_action_type(void);

BSG_EXPORT extern bsg_action_ref bsg_bbox_action_create(void);
BSG_EXPORT extern int bsg_bbox_action_result(bsg_action_ref action,
					     point_t bmin,
					     point_t bmax);

BSG_EXPORT extern bsg_action_ref bsg_collect_action_create(bsg_type_id type);
BSG_EXPORT extern size_t bsg_collect_action_count(bsg_action_ref action);
BSG_EXPORT extern bsg_node_ref bsg_collect_action_get(bsg_action_ref action,
						      size_t idx);

BSG_EXPORT extern bsg_action_ref bsg_search_action_create(const char *name);
BSG_EXPORT extern int bsg_search_action_set_name(bsg_action_ref action,
						 const char *name);
BSG_EXPORT extern size_t bsg_search_action_count(bsg_action_ref action);
BSG_EXPORT extern bsg_node_ref bsg_search_action_get(bsg_action_ref action,
						     size_t idx);

BSG_EXPORT extern bsg_action_ref bsg_render_action_create(struct bsg_render_request *req,
							  struct bsg_render_batch *batch);

BSG_EXPORT extern bsg_action_ref bsg_backend_scene_action_create(
	struct bsg_backend_scene *scene,
	struct bsg_view *view,
	unsigned int flags,
	struct bsg_backend_scene_update_result *result,
	struct bu_vls *diagnostics);

BSG_EXPORT extern struct bsg_backend_scene *
bsg_backend_scene_action_result(bsg_action_ref action,
				struct bsg_backend_scene_update_result *result);

BSG_EXPORT extern bsg_action_ref bsg_pick_ray_action_create(struct bsg_view *view,
							    const point_t orig,
							    const vect_t dir,
							    unsigned int flags);
BSG_EXPORT extern struct bsg_pick_result *bsg_pick_action_result(bsg_action_ref action);

BSG_EXPORT extern bsg_action_ref bsg_snap_action_create(struct bsg_view *view,
							const point_t sample,
							double tol,
							unsigned long long kinds,
							struct bsg_snap_result *result);

BSG_EXPORT extern bsg_action_ref bsg_measure_action_create(struct bsg_view *view,
							   const point_t a,
							   const point_t b,
							   struct bsg_measure_result *result);

BSG_EXPORT extern bsg_action_ref bsg_export_records_action_create(
	const struct bsg_export_request *request,
	struct bsg_export_result *result);

__END_DECLS

#endif /* BSG_ACTION_H */

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
