/*              S C E N E _ S T O R E _ P R I V A T E . H
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
/** @file libbsg/scene_store_private.h
 *
 * Private scene-store storage.
 *
 * This isolates DB object records and the remaining recycled-node storage
 * behind libbsg-only helpers.  Do not widen this layer into public drawing
 * API.
 */

#ifndef LIBBSG_SCENE_STORE_PRIVATE_H
#define LIBBSG_SCENE_STORE_PRIVATE_H

#include "common.h"

#include "bsg/defines.h"
#include "node_fwd_private.h"

__BEGIN_DECLS

struct bsg_scene_recycle_pool;
typedef int (*bsg_scene_store_node_cb)(bsg_node *node, void *data);

extern struct bsg_scene_recycle_pool *bsg_scene_recycle_pool_create(void);
extern void bsg_scene_recycle_pool_destroy(struct bsg_scene_recycle_pool *pool);
extern bsg_node *bsg_scene_recycle_acquire(struct bsg_scene_recycle_pool *pool);
extern void bsg_scene_recycle_release(struct bsg_scene_recycle_pool *pool,
	bsg_node *node);
extern struct bsg_scene_recycle_pool *bsg_scene_node_recycle_pool(
	const bsg_node *node);
extern int bsg_scene_store_node_register_db_record(bsg_node *node);
extern int bsg_scene_store_node_unregister_db_record(bsg_node *node);
extern void bsg_scene_store_db_nodes_release(struct bsg_view *v, int type);
extern size_t bsg_scene_store_db_nodes_count(struct bsg_view *v, int type);
extern bsg_node *bsg_scene_store_db_node_find(struct bsg_view *v,
	const char *name, int path_match);
extern int bsg_scene_store_db_node_name_exists(struct bsg_view *v,
	const char *name);
extern int bsg_scene_store_db_nodes_visit(struct bsg_view *v, int type,
	bsg_scene_store_node_cb cb, void *data);
extern struct bsg_scene_recycle_pool *bsg_scene_recycle_pool_for_create(
	struct bsg_view *v, int type);
extern void bsg_scene_view_obj_pool_init(
	struct bsg_view_obj_pool *pool);
extern void bsg_scene_view_obj_pool_free(
	struct bsg_view_obj_pool *pool);
extern void bsg_scene_view_set_store_init(
	struct bsg_view_set_internal *store);
extern void bsg_scene_view_set_store_free(
	struct bsg_view_set_internal *store);
extern struct bsg_scene_recycle_pool *bsg_scene_view_set_recycle_pool(
	const struct bsg_view_set_internal *store);

__END_DECLS

#endif /* LIBBSG_SCENE_STORE_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
