/*                            L O D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file lod.h */
/** @addtogroup bg_lod */
/** @{ */

/**
 *  @brief Functions for generating view dependent level-of-detail data,
 *  particularly for meshes.
 */

#ifndef BG_LOD_H
#define BG_LOD_H

#include "common.h"
#include "vmath.h"
#include "bv.h"
#include "bg/defines.h"

__BEGIN_DECLS

/* Given a view, construct an oriented bounding box extruded to contain scene
 * objects visible in the view.  Conceptually, think of it as a framebuffer
 * pane pushed through the scene in the direction the camera is looking.  If
 * the view width and height are not set or there is some other problem, no box
 * is computed. */
BG_EXPORT extern void
bg_view_obb(struct bview *v);


/* Storing and reading from a lot of small, individual files doesn't work very
 * well on some platforms.  We provide a "context" to manage bookkeeping of data
 * across objects. The details are implementation internal - the application
 * will simply create a context with a file name and provide it to the various
 * LoD calls. */
struct bg_mesh_lod_context_internal;
struct bg_mesh_lod_context {
    struct bg_mesh_lod_context_internal *i;
};

/* Create an LoD context using "name". If data is already present associated
 * with that name it will be loaded, otherwise a new storage structure will be
 * initialized.  If creation or loading fails for any reason the return value
 * NULL.
 *
 * Note that "name" should be a single name, usually corresponding to a .g
 * database, and not a full path.  libbu will manage where the context data is
 * cached.
*/
BG_EXPORT struct bg_mesh_lod_context *
bg_mesh_lod_context_create(const char *name);

/* Free all memory associated with context c.  Does not destroy the on-disk
 * data. */
BG_EXPORT void
bg_mesh_lod_context_destroy(struct bg_mesh_lod_context *c);


/* We hide the details of the internal LoD structures. */
struct bg_mesh_lod_internal;
struct bg_mesh_lod {
    struct bg_mesh_lod_internal *i;
};

/**
 * Given a set of points and faces, do the initial calculations to generate the
 * cached LoD data needed for subsequent lookups.  This only needs to be done
 * once per un-cached data set, but is potentially an expensive operation.
 *
 * If pre-existing cached data is found, the key is just returned - to clear
 * pre-existing cached data, run bg_mesh_lod_clear_cache();
 *
 * returns the lookup key calculated from the data, which is used in subsequent
 * lookups of the cached data. */
BG_EXPORT unsigned long long
bg_mesh_lod_cache(const point_t *v, size_t vcnt, int *f, size_t fcnt);

/**
 * Set up the bg_mesh_lod data using cached LoD information associated with
 * key.  If no cached data has been prepared, a NULL container is returned.
 *
 * Note: bg_mesh_lod assumes a non-changing mesh - if the mesh is changed after
 * it is created, the internal container does *NOT* automatically update.  In
 * that case the old struct should be destroyed and a new one created.
 *
 * A bg_mesh_lod return from this function will be initialized only to the
 * lowest level of data (i.e. the coarsest possible representation of the
 * object.) To tailor the data, use the bg_mesh_lod_view function.  For lower
 * level control, the bg_mesh_lod_level function may be used to explicitly
 * manipulate the loaded LoD (usually used for debugging, but also useful if an
 * application wishes to visualize levels explicitly.)
 */
BG_EXPORT struct bv_mesh_lod_info *
bg_mesh_lod_init(unsigned long long key);

/**
 * Given a bview, load the appropriate level of detail for displaying the mesh
 * in that view. Set reset == 1 if the caller wants to undo a memshrink
 * operation even if the level isn't changed by the current view settings.
 *
 * Returns the level selected.  If v == NULL, return current level of l.  If
 * there is an error or l == NULL, return -1; */
BG_EXPORT int
bg_mesh_lod_view(struct bg_mesh_lod *l, struct bview *v, int reset);

/**
 * Given a detail level, load the appropriate data.  This is not normally used
 * by client codes directly, but may be needed if an app needs  manipulate
 * the level of detail without a view.  Set reset == 1 if the caller wants to
 * undo a memshrink operation even if the level isn't changed by the current
 * view settings.
 *
 * Returns the level selected.  If level == -1, return current level of l.  If
 * there is an error, return -1; */
BG_EXPORT int
bg_mesh_lod_level(struct bg_mesh_lod *l, int level, int reset);

/* Clean up the lod container. */
BG_EXPORT void
bg_mesh_lod_destroy(struct bv_mesh_lod_info *i);

/* Remove cache data associated with key.  If key == 0, remove ALL cache data
 * associated with all LoD objects (i.e. a full LoD cache reset). */
BG_EXPORT void
bg_mesh_lod_clear_cache(unsigned long long key);

/* Set drawing function callback */
BG_EXPORT void
bg_mesh_lod_set_draw_callback(struct bg_mesh_lod *lod, int (*clbk)(void *ctx, struct bv_mesh_lod_info *info));


/* Set function callback for retrieving full mesh detail */
BG_EXPORT void
bg_mesh_lod_set_detail_callback(struct bg_mesh_lod *lod, int (*clbk)(struct bv_mesh_lod_info *, void *, void *));


/* Trigger a triangle drawing operation. */
BG_EXPORT void
bg_mesh_lod_draw(void *ctx, struct bv_scene_obj *s);

/* Reduce memory footprint (for use after client codes have completed
 * use of a particular level's info, but aren't done with the object.
 * Main use case currently is OpenGL display lists - once generated,
 * we can clear the internally stored LoD data until the level changes.
 * Note that there is a re-loading performance penalty as a trade-off
 * to the memory savings. */
BG_EXPORT void
bg_mesh_lod_memshrink(struct bv_scene_obj *s);

/* Callback for updating level settings on an object.  Set reset == 1 if the
 * caller wants to undo a memshrink operation even if the level isn't
 * changed by the current view settings. */
BG_EXPORT int
bg_mesh_lod_update(struct bv_scene_obj *s, struct bview *v, int reset);

/* Free a scene object's LoD data.  Suitable as a s_free_callback function
 * for a bv_scene_obj */
BG_EXPORT void
bg_mesh_lod_free(struct bv_scene_obj *s);

__END_DECLS

#endif  /* BG_LOD_H */
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
