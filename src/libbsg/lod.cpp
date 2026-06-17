/*                          L O D . C P P
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
/** @file libbsg/lod.cpp
 *
 * Level-of-detail update helpers for the BSG scene graph.
 *
 * bsg_lod_sources_update() does a depth-first walk of the BSG tree and, at
 * each BSG_NODE_LOD node, realizes the typed source policy bound to the node.
 * The traversal handles nested LoD nodes and skips any node that has no source
 * policy installed.
 *
 * bsg_lod_source_stale() is a thin accessor over the source record's per-view
 * state.
 */

#include "common.h"

#include "bu/ptbl.h"
#include "bsg/defines.h"
#include "bsg/lod.h"
#include "bsg/object.h"
#include "lod_private.h"
#include "node_private.h"
#include "object_private.h"


/* ------------------------------------------------------------------ */
/* Internal recursive walker                                           */
/* ------------------------------------------------------------------ */

static int
_lod_node_is_lod(const bsg_node *node)
{
    return bsg_node_type_is_a(node, bsg_lod_type());
}


static int
_lod_node_is_shape(const bsg_node *node)
{
    return bsg_node_type_is_a(node, bsg_shape_type());
}

static void
_lod_update_recursive(bsg_node *node, struct bsg_view *v)
{
    if (!node || !v)
	return;

    bsg_node *n = (bsg_node *)node;

    if (_lod_node_is_lod(n)) {
	struct bsg_lod_payload *pl = bsg_lod_node_payload(n);
	if (pl && pl->ops) {
	    /* Ensure a cursor exists for this view. */
	    bsg_lod_node_get_cursor(node, v);

	    if (pl->ops->is_stale(node, v)) {
		int lvl = pl->ops->select_level(node, v);
		pl->ops->activate_level(node, v, lvl);
	    }
	}
	/* Recurse into the LoD node's children (level representations
	 * may themselves contain further LoD nodes in nested cases). */
    }

    /* Walk children regardless of node type so we find nested LoD nodes. */
    for (size_t i = 0; i < BU_PTBL_LEN(&n->children); i++) {
	bsg_node *child =
	    (bsg_node *)BU_PTBL_GET(&n->children, i);
	if (!child)
	    continue;
	/* Skip ordinary leaf nodes; LoD wrappers can also carry shape bits. */
	if (_lod_node_is_shape(child) && !_lod_node_is_lod(child))
	    continue;
	_lod_update_recursive((bsg_node *)child, v);
    }
}


/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void
bsg_lod_sources_update(bsg_node *root, struct bsg_view *v)
{
    if (!root || !v)
	return;

    /* Walk the entire subtree looking for BSG_NODE_LOD nodes. */
    _lod_update_recursive(root, v);
}


void
bsg_lod_sources_update_scene_ref(bsg_scene_ref root, struct bsg_view *v)
{
    bsg_lod_sources_update((bsg_node *)root.opaque, v);
}


static int
_bsg_lod_source_node_stale(bsg_node *n, struct bsg_view *v)
{
    if (!n || !v)
	return 0;

    /* Only BSG_NODE_LOD nodes carry staleness state. */
    bsg_node *s = (bsg_node *)n;
    if (!_lod_node_is_lod(s))
	return 0;

    struct bsg_lod_payload *pl = bsg_lod_node_payload(s);
    if (!pl || !pl->ops || !pl->ops->is_stale)
	return 0;

    return pl->ops->is_stale(n, v);
}

int
bsg_lod_source_stale(bsg_lod_source_ref ref, struct bsg_view *v)
{
    bsg_node *node = (ref.token) ? (bsg_node *)ref.token : NULL;
    return _bsg_lod_source_node_stale(node, v);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
