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

/* Given a view, construct either an oriented bounding box or a view frustum
 * extruded to contain scene objects visible in the view.  Conceptually, think
 * of it as a framebuffer pane pushed through the scene in the direction the
 * camera is looking.  If the view width and height are not set or there is
 * some other problem, no volume is computed. This function is intended
 * primarily to be set as an updating callback for the bview structure. */
BG_EXPORT extern void
bg_view_bounds(struct bview *v);


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
 * Note that "name" should be a full, unique path associated with the source
 * database.  libbu will manage where the context data is cached.
*/
BG_EXPORT struct bg_mesh_lod_context *
bg_mesh_lod_context_create(const char *name);

/* Free all memory associated with context c.  Note that this call does NOT
 * remove the on-disk data. */
BG_EXPORT void
bg_mesh_lod_context_destroy(struct bg_mesh_lod_context *c);

/* Remove cache data associated with key.  If key == 0, remove ALL cache data
 * associated with all LoD objects in c. (i.e. a full LoD cache reset for that
 * .g database).  If key == 0 AND c == NULL, clear all LoD cache data for all
 * .g databases associated with the current user's cache.
 */
BG_EXPORT void
bg_mesh_lod_clear_cache(struct bg_mesh_lod_context *c, unsigned long long key);

/**
 * Given a set of points and faces, calculate a lookup key and determine if the
 * cache has the LoD data for this particular mesh.  If it does not, do the
 * initial calculations to generate the cached LoD data needed for subsequent
 * lookups, otherwise just return the calculated key.
 *
 * This is potentially an expensive operation, particularly if the LoD data set
 * must be generated for the mesh.
 *
 * Note: to clear pre-existing cached data, run bg_mesh_lod_clear_cache();
 *
 * @return the lookup key calculated from the data */
BG_EXPORT unsigned long long
bg_mesh_lod_cache(struct bg_mesh_lod_context *c, const point_t *v, size_t vcnt, int *f, size_t fcnt);

/**
 * Given a name, see if the context has a key associated with that name.
 * If so return it, else return zero.
 *
 * Users of this feature need to be aware that it is the responsibility
 * of the application to maintain the LoD cache data and keep it current -
 * it is quite possible, in a data sense, for the LoD data to be out of
 * date if something has changed the named geometry object and not updated
 * the cache.
 *
 * The advantage of this feature, if the application does maintain the
 * data correctly, is to make it possible to load a high level view of
 * a large model without having to hash the full mesh data of the model
 * to retrieve the data.
 */
BG_EXPORT unsigned long long
bg_mesh_lod_key_get(struct bg_mesh_lod_context *c, const char *name);

/**
 * Given a name and a key, instruct the context to associate that name with
 * the key.
 *
 * Returns 0 if successful, else error
 */
BG_EXPORT int
bg_mesh_lod_key_put(struct bg_mesh_lod_context *c, const char *name, unsigned long long key);

/**
 * Set up the bg_mesh_lod container using cached LoD information associated
 * with key.  If no cached data has been prepared, a NULL container is
 * returned - to prepare cached data, call bg_mesh_lod_cache with the original
 * mesh input data.  This call is intended to be usable in situations where
 * we don't want to pull the full mesh data set into memory, so we can't assume
 * the original data is present.
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
BG_EXPORT struct bv_mesh_lod *
bg_mesh_lod_create(struct bg_mesh_lod_context *c, unsigned long long key);

/* Clean up the lod container. */
BG_EXPORT void
bg_mesh_lod_destroy(struct bv_mesh_lod *l);

/* Given a drawing context (typically a display manager), a scene object with
 * mesh LoD data stored in s->draw_data and a LoD drawing callback set with
 * bg_mesh_lod_draw_clbk, trigger a triangle drawing operation. */
BG_EXPORT void
bg_mesh_lod_draw(void *ctx, struct bv_scene_obj *s);

/**
 * Given a scene object with mesh LoD data stored in s->draw_data, reduce
 * memory footprint (for use after client codes have completed use of a
 * particular level's info, but aren't done with the object.  Main use case
 * currently is OpenGL display lists - once generated, we can clear the
 * internally stored LoD data until the level changes.  Note that there is a
 * re-loading performance penalty as a trade-off to the memory savings. */
BG_EXPORT void
bg_mesh_lod_memshrink(struct bv_scene_obj *s);

/**
 * Given a scene object with mesh LoD data stored in s->draw_data and a bview,
 * load the appropriate level of detail for displaying the mesh in that view.
 * Set reset == 1 if the caller wants to undo a memshrink operation even if the
 * level isn't changed by the current view settings.
 *
 * Returns the level selected.  If v == NULL, return current level of l.  If
 * there is an error or l == NULL, return -1; */
BG_EXPORT int
bg_mesh_lod_view(struct bv_scene_obj *s, struct bview *v, int reset);

/**
 * Given a scene object with mesh LoD data stored in s->draw_data and a detail
 * level, load the appropriate data.  This is not normally used by client codes
 * directly, but may be needed if an app needs  manipulate the level of detail
 * without a view.  Set reset == 1 if the caller wants to undo a memshrink
 * operation even if the level isn't changed by the current view settings.
 *
 * Returns the level selected.  If level == -1, return current level of l.  If
 * there is an error, return -1; */
BG_EXPORT int
bg_mesh_lod_level(struct bv_scene_obj *s, int level, int reset);

/* Free a scene object's LoD data.  Suitable as a s_free_callback function
 * for a bv_scene_obj.  This function will also trigger any additional
 * "free" callback functions which might be defined for the LoD container. */
BG_EXPORT void
bg_mesh_lod_free(struct bv_scene_obj *s);



/* In order to preserve library barriers, a number of operations needed to
 * realize LoD drawing are defined at other library layers and made available
 * to the core LoD management logic with callbacks */

/* Set drawing function callback */
BG_EXPORT void
bg_mesh_lod_draw_clbk(struct bv_mesh_lod *lod, int (*clbk)(void *, struct bv_scene_obj *));

/* Set function callbacks for retrieving and freeing high levels of mesh detail */
BG_EXPORT void
bg_mesh_lod_detail_setup_clbk(struct bv_mesh_lod *lod, int (*clbk)(struct bv_mesh_lod *, void *), void *cb_data);
BG_EXPORT void
bg_mesh_lod_detail_clear_clbk(struct bv_mesh_lod *lod, int (*clbk)(struct bv_mesh_lod *, void *));
BG_EXPORT void
bg_mesh_lod_detail_free_clbk(struct bv_mesh_lod *lod, int (*clbk)(struct bv_mesh_lod *, void *));

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
