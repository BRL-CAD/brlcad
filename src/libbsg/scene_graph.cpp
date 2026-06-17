/*                 S C E N E _ G R A P H . C P P
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
/** @file libbsg/scene_graph.cpp
 *
 * Core BSG scene-graph lifecycle and query helpers.
 *
 * bsg_node is a typedef for struct bsg_node; this file implements:
 *   bsg_view_scene_root_ensure           — borrow or allocate an attached root
 *   bsg_view_scene_root_sync             — synchronize an attached root
 *   bsg_view_scene_root_detach_from_root — clear the root's owning view handle
 *
 * The view stores an opaque scene handle in gv_scene.  Internally that
 * handle currently points at the retained scene root, but application code
 * no longer reads or writes the root pointer directly.
 *
 * dm_draw_objs() lives in src/libdm/view.c so that display-manager frame
 * setup and backend adapter dispatch stay on the libdm side of the boundary.
 */

#include "common.h"

#include "bu/malloc.h"
#include "bsg/defines.h"
#include "bsg/sensor.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"

#include "bsg/scene_set.h"
#include "bsg_private.h"
#include "draw_source_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "payload_private.h"

/* ---------------------------------------------------------------------- */
/* Public API                                                               */
/* ---------------------------------------------------------------------- */

bsg_node *
bsg_view_scene_root_ensure(struct bsg_view *v)
{
    if (!v)
	return NULL;

    /* If this view is part of a set and doesn't yet have a draw root, inherit
     * the active draw root from another view in the set.  This keeps secondary
     * views (e.g. libtclcad null-DM views) aligned with the GED draw tree. */
    if (!bsg_view_scene_root(v) && v->vset) {
	struct bu_ptbl *views = bsg_set_views(v->vset);
	if (views) {
	    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
		struct bsg_view *sv = (struct bsg_view *)BU_PTBL_GET(views, i);
		if (!sv || sv == v)
		    continue;
		if (bsg_view_scene_root(sv)) {
		    bsg_view_scene_root_set(v, bsg_view_scene_root(sv));
		    break;
		}
	    }
	}
    }

    /* GED-backed views attach a retained scene through libged.  Standalone
     * libbsg/libbv consumers and unit tests allocate a minimal retained root
     * here so the scene-root API remains usable without libged. */
    if (!bsg_view_scene_root(v)) {
	/* This path is for non-GED callers.  GED command flows continue to use
	 * libged's draw-scene initialization so the registry context is
	 * installed. */
	bsg_node *root = bsg_scene_node_create_detached(v, BSG_SOURCE_CHILD);
	if (!root) {
	    bu_log("bsg_view_scene_root_ensure: failed to allocate standalone draw root\n");
	    return NULL;
	}
	bsg_node_set_object_type(root, bsg_group_type());
	bsg_node_set_visible_state(root, 1);
	root->parent = NULL;
	bsg_node_set_name(root, "_draw_root");
	bsg_view_scene_root_set(v, root);
    }

    return bsg_view_scene_root(v);
}


/* The retained root's children are maintained live by the draw-tree mutation
 * helpers (bsg_group_ensure_child / bsg_free_group / ged_draw_clear).  No
 * per-frame rebuild is required; this internal compatibility helper remains
 * a deliberate no-op. */
void
bsg_view_scene_root_sync(bsg_node *UNUSED(root), struct bsg_view *UNUSED(v))
{
}


void
bsg_view_scene_root_detach_from_root(bsg_node *root)
{
    if (!root)
	return;

    /* The retained scene root is owned elsewhere.  Do NOT release it here;
     * just clear the view's back-reference. */
    bsg_node *r = (bsg_node *)root;
    struct bsg_view *view = bsg_node_get_view(r);
    if (view && bsg_view_scene_root(view) == root)
	bsg_view_scene_root_set(view, NULL);
}


/* ---------------------------------------------------------------------- */
/* bsg_scene_set retained-scene registry                                  */
/* ---------------------------------------------------------------------- */

/* Simple linked-list cell for the registry. */
struct _bsg_ss_entry {
    struct bsg_view *v;
    bsg_node *root;
    struct _bsg_ss_entry *next;
};

struct bsg_scene_set {
    struct _bsg_ss_entry *head;
};

struct bsg_scene_set *
bsg_scene_set_create(void)
{
    struct bsg_scene_set *ss;
    BU_ALLOC(ss, struct bsg_scene_set);
    ss->head = NULL;
    return ss;
}

void
bsg_scene_set_destroy(struct bsg_scene_set *ss)
{
    if (!ss)
	return;

    struct _bsg_ss_entry *e = ss->head;
    while (e) {
	struct _bsg_ss_entry *next = e->next;
	bu_free(e, "bsg_ss_entry");
	e = next;
    }
    bu_free(ss, "bsg_scene_set");
}

void
bsg_scene_set_add(struct bsg_scene_set *ss, struct bsg_view *v, bsg_node *root)
{
    if (!ss || !v)
	return;

    /* Update existing entry if present. */
    for (struct _bsg_ss_entry *e = ss->head; e; e = e->next) {
	if (e->v == v) {
	    e->root = root;
	    return;
	}
    }

    /* Otherwise prepend a new entry. */
    struct _bsg_ss_entry *ne;
    BU_ALLOC(ne, struct _bsg_ss_entry);
    ne->v = v;
    ne->root = root;
    ne->next = ss->head;
    ss->head = ne;
}

bsg_node *
bsg_scene_set_get(struct bsg_scene_set *ss, struct bsg_view *v)
{
    if (!ss || !v)
	return NULL;

    for (struct _bsg_ss_entry *e = ss->head; e; e = e->next) {
	if (e->v == v)
	    return e->root;
    }
    return NULL;
}

void
bsg_scene_set_remove(struct bsg_scene_set *ss, struct bsg_view *v)
{
    if (!ss || !v)
	return;

    struct _bsg_ss_entry **pp = &ss->head;
    while (*pp) {
	if ((*pp)->v == v) {
	    struct _bsg_ss_entry *del = *pp;
	    *pp = del->next;
	    bu_free(del, "bsg_ss_entry");
	    return;
	}
	pp = &(*pp)->next;
    }
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
