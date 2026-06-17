/*              S C E N E _ O B J E C T _ P R I V A T E . H
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
/** @file libbsg/scene_object_private.h
 *
 * Private scene-object storage bridge.
 *
 * These entry points expose the current node-backed storage model.  They are
 * intentionally not installed; public callers should use semantic feature,
 * draw-source, render, action, and export records instead.
 */

#ifndef LIBBSG_SCENE_OBJECT_PRIVATE_H
#define LIBBSG_SCENE_OBJECT_PRIVATE_H

#include "common.h"

#include "bu/list.h"
#include "bu/ptbl.h"
#include "vmath.h"
#include "bsg/scene_object.h"
#include "node_fwd_private.h"

__BEGIN_DECLS

typedef int (*bsg_scene_db_visit_cb)(bsg_node *node, void *data);

extern void bsg_scene_visit_db(struct bsg_view *v,
	bsg_scene_db_visit_cb cb, void *data);

BSG_EXPORT extern int bsg_scene_node_update_bounds(bsg_node *node, struct bsg_view *v);

typedef void (*bsg_release_cb_t)(bsg_node *node);

extern void bsg_node_set_release_callback(bsg_node *node, bsg_release_cb_t cb);
extern void bsg_node_invoke_release_callback(bsg_node *node);

extern bsg_node *bsg_scene_node_create(struct bsg_view *v, int type);
extern bsg_node *bsg_scene_node_create_registered(struct bsg_view *v, int type);
extern bsg_node *bsg_scene_node_create_detached(struct bsg_view *v, int type);
extern bsg_node *bsg_scene_node_create_child(bsg_node *parent);
BSG_EXPORT extern void bsg_scene_node_reset(bsg_node *node);
BSG_EXPORT extern void bsg_scene_node_release(bsg_node *node);
extern void bsg_scene_node_sync(bsg_node *dest, bsg_node *src);
extern void bsg_scene_node_invalidate(bsg_node *node);

extern bsg_node *bsg_scene_child_find(bsg_node *parent, const char *name);
extern bsg_node *bsg_scene_node_find(struct bsg_view *v, const char *name);

extern fastf_t bsg_scene_node_view_depth(bsg_node *node, struct bsg_view *v, int mode);

__END_DECLS

#endif /* LIBBSG_SCENE_OBJECT_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
