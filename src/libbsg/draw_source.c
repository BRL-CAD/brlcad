/*               D R A W _ S O U R C E . C
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
/** @file libbsg/draw_source.c
 *
 * Draw-source accessors for view binding, draw extents, and callbacks.
 */

#include "common.h"

#include "vmath.h"
#include "bsg/draw_source.h"
#include "bsg/scene_builder.h"
#include "bsg/scene_object.h"
#include "bsg_private.h"
#include "draw_source_private.h"
#include "node_private.h"


void
bsg_node_set_view(bsg_node *node, struct bsg_view *v)
{
    if (!node)
	return;
    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    if (ni) {
	ni->view_binding.view = v;
	ni->view_binding.valid = 1;
    }
}


struct bsg_view *
bsg_node_get_view(const bsg_node *node)
{
    if (!node)
	return NULL;
    if (node->i && node->i->view_binding.valid)
	return node->i->view_binding.view;
    return NULL;
}


static void
_draw_extent_init(struct bsg_node_draw_extent_state *state)
{
    if (!state)
	return;

    VSETALL(state->center, 0.0);
    state->size = 0.0;
    state->valid = 1;
}

static struct bsg_node_draw_extent_state *
_draw_extent_ensure(bsg_node *node)
{
    if (!node)
	return NULL;

    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    if (!ni)
	return NULL;

    if (!ni->draw_extent.valid)
	_draw_extent_init(&ni->draw_extent);

    return &ni->draw_extent;
}


void
bsg_node_set_release_callback(bsg_node *node, bsg_release_cb_t cb)
{
    if (!node)
	return;
    bsg_node_internal_ensure(node)->release_callback = cb;
}


void
bsg_node_invoke_release_callback(bsg_node *node)
{
    if (!node || !node->i || !node->i->release_callback)
	return;
    node->i->release_callback(node);
    node->i->release_callback = NULL;
}


void
bsg_node_set_draw_mat(bsg_node *node, const mat_t mat)
{
    if (!node || !mat)
	return;
    bsg_node_set_transform(node, mat);
}

void
bsg_scene_set_draw_mat(bsg_scene_ref ref, const mat_t mat)
{
    bsg_node_set_draw_mat((bsg_node *)ref.opaque, mat);
}


void
bsg_node_get_draw_mat(const bsg_node *node, mat_t mat)
{
    if (!node || !mat)
	return;
    bsg_node_transform(node, mat);
}

void
bsg_scene_draw_mat(bsg_scene_ref ref, mat_t mat)
{
    bsg_node_get_draw_mat((const bsg_node *)ref.opaque, mat);
}


fastf_t
bsg_node_draw_size(const bsg_node *node)
{
    if (!node)
	return 0.0;
    if (node->i && node->i->draw_extent.valid)
	return node->i->draw_extent.size;
    return 0.0;
}

fastf_t
bsg_scene_draw_size(bsg_scene_ref ref)
{
    return bsg_node_draw_size((const bsg_node *)ref.opaque);
}


void
bsg_node_set_draw_size(bsg_node *node, fastf_t size)
{
    if (!node)
	return;
    struct bsg_node_draw_extent_state *state = _draw_extent_ensure(node);
    if (state) {
	state->size = size;
	state->valid = 1;
    }
}

void
bsg_scene_set_draw_size(bsg_scene_ref ref, fastf_t size)
{
    bsg_node_set_draw_size((bsg_node *)ref.opaque, size);
}


void
bsg_node_get_draw_center(const bsg_node *node, vect_t center)
{
    if (!node || !center)
	return;
    if (node->i && node->i->draw_extent.valid) {
	VMOVE(center, node->i->draw_extent.center);
	return;
    }
    VSETALL(center, 0.0);
}

void
bsg_scene_draw_center(bsg_scene_ref ref, vect_t center)
{
    bsg_node_get_draw_center((const bsg_node *)ref.opaque, center);
}


void
bsg_node_set_draw_center(bsg_node *node, const vect_t center)
{
    if (!node || !center)
	return;
    struct bsg_node_draw_extent_state *state = _draw_extent_ensure(node);
    if (state) {
	VMOVE(state->center, center);
	state->valid = 1;
    }
}

void
bsg_scene_set_draw_center(bsg_scene_ref ref, const vect_t center)
{
    bsg_node_set_draw_center((bsg_node *)ref.opaque, center);
}


void
bsg_node_source_realization_set_roles(bsg_node *node, int csg_obj, int mesh_obj)
{
    if (!node)
	return;
    struct bsg_source_realization_state *dc = &bsg_node_internal_ensure(node)->source_realization;
    dc->csg_obj = csg_obj ? 1 : 0;
    dc->mesh_obj = mesh_obj ? 1 : 0;
}


int
bsg_node_source_realization_is_mesh(const bsg_node *node)
{
    if (!node)
	return 0;
    return (node->i && node->i->source_realization.mesh_obj) ? 1 : 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
