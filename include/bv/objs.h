/*                           O B J S . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
/** @addtogroup bv_objs
 *
 * Routines for managing and operating on libbv scene objects.
 *
 */
/** @{ */
/** @file bv/objs.h */

#ifndef BV_OBJS_H
#define BV_OBJS_H

#include "common.h"
#include "bu/ptbl.h"
#include "bn/tol.h"
#include "bv/defines.h"
#include "dm/defines.h"

__BEGIN_DECLS

/* Copy settings.  Return 0 if no changes were made to dest.  If dest did have
 * one or more settings updated from src, return 1. */
BV_EXPORT extern int bv_obj_settings_sync(struct bv_obj_settings *dest, struct bv_obj_settings *src);

/* Scene objects are one of the things we sometimes want to reuse.  The
 * following routines support the creation an opaque memory pool libbv routines
 * can optionally pull from. */
struct bv_obj_pool_internal;
struct bv_obj_pool {
    struct bv_obj_pool_internal *i;
};

/* Create an empty object pool */
BV_EXPORT struct bv_obj_pool *
bv_obj_pool_create();

/* Destroy an object pool and all internal contents */
BV_EXPORT void
bv_obj_pool_destroy(struct bv_obj_pool *p);


/* Create a bv_scene_obj.  If p is not NULL, first try to pull from the pool
 * before mallocing a new object. */
BV_EXPORT struct bv_scene_obj *
bv_obj_get(struct bv_obj_pool *p);

/* Given an object, create an object that is a child of that object.  Will use
 * s->obj_pool (if set) for memory allocation. */
BV_EXPORT struct bv_scene_obj *
bv_obj_get_child(struct bv_scene_obj *s);

/* Given a scene object and a name vname, glob match child names and uuids to
 * attempt to locate a child of s that matches vname */
BV_EXPORT struct bv_scene_obj *
bv_obj_find_child(struct bv_scene_obj *s, const char *vname);

/* Copy object attributes (but not geometry) from src to dest */
BV_EXPORT extern void
bv_obj_sync(struct bv_scene_obj *dest, struct bv_scene_obj *src);

/* Clear the contents of an object (including releasing its children).
 * Generally used when redrawing an object */
BV_EXPORT void
bv_obj_reset(struct bv_scene_obj *s);

/* Release an object. If o->obj_pool is not NULL, clear it and add to the pool
 * of available objects.  If o->obj_pool is NULL, free o. */
BV_EXPORT void
bv_obj_put(struct bv_scene_obj *o);

/* Compute the min, max, and center points of the scene object.
 * Return 1 if a bound was computed, else 0 */
BV_EXPORT extern int
bv_obj_bound(struct bv_scene_obj *s);

/* Set the illumination state on the object and its children to ill_state.
 * Returns 0 if no states were changed, and 1 if one or more states were
 * updated. */
BV_EXPORT int
bv_obj_illum(struct bv_scene_obj *s, char ill_state);

/* Given a view, and a set of scene objects, construct either an oriented
 * bounding box or a view frustum extruded to contain scene objects visible in
 * the view.  Conceptually, think of it as a framebuffer pane pushed through
 * the scene in the direction the camera is looking.  If the view width and
 * height are not set or there is some other problem, no volume is computed.
 * This function is intended primarily to be set as an updating callback for
 * the bview structure. */
BV_EXPORT extern void
bv_view_bounds(struct bview *v, const struct bu_ptbl *so);

/* Given a view and an object, return whether or or not that object is
 * visible in the view - 1 is visible, 0 is not visible. */
BV_EXPORT extern int
bv_view_obj_vis(const struct bview *v, const struct bv_scene_obj *s);

/* Given a screen x,y coordinate, construct the set of all scene objects whose
 * AABB intersect with the OBB created by the projection of that pixel through
 * the scene. sset holds the struct bv_scene_obj pointers of the selected
 * objects.  */
BV_EXPORT int
bv_view_objs_select(struct bu_ptbl *sset, const struct bview *v, const struct bu_ptbl *so, int x, int y);

/* Given a screen x1,y1,x2,y2 rectangle, construct and return the set of all scene
 * objects whose AABB intersect with the OBB created by the projection of that
 * rectangle through the scene. */
BV_EXPORT int
bv_view_objs_rect_select(struct bu_ptbl *sset, const struct bview *v, const struct bu_ptbl *so, int x1, int y1, int x2, int y2);


__END_DECLS

/** @} */

#endif /* BV_OBJS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
