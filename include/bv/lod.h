/*                            L O D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/** @addtogroup bv_lod
 *
 *  @brief
 *  Functions for generating, storing and retrieving view dependent
 *  level-of-detail data.
 */
/** @{ */
/* @file lod.h */

#ifndef BV_LOD_H
#define BV_LOD_H

#include "common.h"
#include "vmath.h"
#include "bu/ptbl.h"
#include "bv/defines.h"

__BEGIN_DECLS

#define LOD_MESH_MAGIC 0x4C4F44D /**< LODM */

/* Other libraries may care if libbv's format changes */
#define BV_CACHE_CURRENT_FORMAT 3

/* The primary "working" data for mesh Level-of-Detail (LoD) drawing is stored
 * in a bv_mesh_lod container.
 *
 * Most LoD information is deliberately hidden in the internal, but the key
 * data needed for drawing routines and view setup are exposed.
 *
 * TODO - this container can probably be more generic than just triangle
 * data - for example, we might want to cache wireframes for CSG objects.
 *
 * Should the dlist arrays for OpenGL live here too?  This might be a logical
 * place to put them...
 */
struct bv_lod_mesh {
    uint32_t magic;

    // The set of triangle faces to be used when drawing
    int fcnt;
    const int *faces;

    // The vertices used by the faces array
    int pcnt;
    const point_t *points;      // If using snapped points, that's this array.  Else, points == points_orig.
    int porig_cnt;
    const point_t *points_orig;

    // Optional: per-face-vertex normals (one normal per triangle vertex - NOT
    // one normal per vertex.  I.e., a given point from points_orig may have
    // multiple normals associated with it in different faces.)
    const vect_t *normals;

    // Bounding box of the original full-detail data
    point_t bmin;
    point_t bmax;

    // libbu cache data is being retrieved from
    struct bu_cache *c;

    // The scene object using this LoD structure
    struct bv_scene_obj *s;

    // Pointer to internal LoD implementation information specific to this object
    void *i;
};

/**
 * Given a set of points and faces, calculate a lookup key and determine if the
 * cache has the LoD data for this particular mesh.  If it does not, do the
 * initial calculations to generate the LoD data needed for subsequent
 * lookups and return a set of struct bu_cache_item containers.  Caller should
 * write these to the cache of their choice.
 *
 * Name is passed in to allow the internal logic to generate the cache keys
 * needed.  While these are technically visible in the bu_cache_item containers
 * and used in bu_cache_write calls, they are NOT intended to be user modifiable
 * and constitute internal implementation details of the LoD data system.
 *
 * This is potentially an expensive operation, particularly if the LoD data set
 * must be generated for the mesh.
 *
 * Note: to clear pre-existing cached data first, use bv_lod_clear_gen(tbl, name, c) to generate bu_cache_items and then call bu_cache_clear on the active cache using the item keys;
 *
 * @return 0 if no data create, 1 if data items were created. */
BV_EXPORT int
bv_lod_mesh_gen(struct bu_ptbl *cache_items, const char *name, const point_t *v, size_t vcnt, const vect_t *vn, int *f, size_t fcnt, fastf_t fratio);

/**
 * Generate a set of LoD cache data items associated with name
 * to be cleared.  (The cache is needed because keys are sometimes
 * used to look up other associated keys.)
 */
BV_EXPORT void
bv_lod_clear_gen(struct bu_ptbl *cache_items, const char *name, struct bu_cache *c);

/**
 * Set up the bv_lod_mesh container using cached mesh LoD information associated
 * with name.  If no cached mesh data is associated with name, a NULL container is
 * returned.
 *
 * To prepare cached data, call bv_lod_mesh_cache with the original
 * mesh input data.
 *
 * This call is intended to be usable in situations where we don't want to pull
 * the full mesh data set into memory, so we can't assume the original data is
 * present.
 *
 * A bv_lod_mesh return from this function will be initialized only to the
 * lowest level of data (i.e. the coarsest possible representation of the
 * object.) To tailor the data, use the bv_lod_mesh_view function.  For lower
 * level control, the bv_lod_mesh_level function may be used to explicitly
 * manipulate the loaded LoD (usually used for debugging, but also useful if an
 * application wishes to visualize levels explicitly.)
 */
BV_EXPORT struct bv_lod_mesh *
bv_lod_mesh_get(struct bu_cache *c, const char *name);

/* Clean up the lod container. */
BV_EXPORT void
bv_lod_mesh_put(struct bv_lod_mesh *l);

/**
 * Given a scene object with LoD data stored in s->draw_data, reduce memory
 * footprint if possible.   Intended for use after client codes have completed
 * use of a particular level's info, but aren't done with the object.
 *
 * Main use case currently is OpenGL display lists - once generated, we can
 * clear the internally stored LoD data until the level changes.  Note that
 * there is a re-loading performance penalty as a trade-off to the memory
 * savings. */
BV_EXPORT void
bv_lod_memshrink(struct bv_scene_obj *s);

/**
 * Given a scene object with LoD data stored in s->draw_data and a bview,
 * calculate the appropriate level of detail for that view.
 *
 * This function simply returns the level without changing any data, which
 * allows applications to decide if any further action is warranted.
 *
 * Returns the level appropriate for the view.  If v == NULL, return current
 * level of l.  If there is an error or l == NULL, return -1; */
BV_EXPORT int
bv_lod_calc_level(struct bv_scene_obj *s, const struct bview *v);

/**
 * Given a scene object with LoD data stored in s->draw_data and a bview, load
 * the appropriate level of detail for displaying in that view.  Set reset == 1
 * if the caller wants to undo a memshrink operation even if the level isn't
 * changed by the current view settings.
 *
 * Returns the level selected.  If v == NULL, return current level of l.  If
 * there is an error or l == NULL, return -1; */
BV_EXPORT int
bv_lod_view(struct bv_scene_obj *s, const struct bview *v, int reset);

/**
 * Given a scene object with LoD data stored in s->draw_data and a detail
 * level, load the appropriate data.  This is not normally used by client codes
 * directly, but may be needed if an app needs  manipulate the level of detail
 * without a view.  Set reset == 1 if the caller wants to undo a memshrink
 * operation even if the level isn't changed by the current view settings.
 *
 * Returns the level selected.  If level == -1, return current level of l.  If
 * there is an error, return -1; */
BV_EXPORT int
bv_lod_level(struct bv_scene_obj *s, int level, int reset);

/* Free a scene object's LoD data.  Suitable as a s_free_callback function
 * for a bv_scene_obj.  This function will also trigger any additional
 * "free" callback functions which might be defined for the LoD container. */
BV_EXPORT void
bv_lod_free(struct bv_scene_obj *s);


/* In order to preserve library barriers, a number of operations needed to
 * realize LoD drawing are defined at other library layers and made available
 * to the core LoD management logic with callbacks */

/* Set function callbacks for retrieving and freeing high levels of mesh detail */
BV_EXPORT void
bv_lod_mesh_detail_setup_clbk(struct bv_lod_mesh *lod, int (*clbk)(struct bv_lod_mesh *, void *), void *cb_data);
BV_EXPORT void
bv_lod_mesh_detail_clear_clbk(struct bv_lod_mesh *lod, int (*clbk)(struct bv_lod_mesh *, void *));
BV_EXPORT void
bv_lod_mesh_detail_free_clbk(struct bv_lod_mesh *lod, int (*clbk)(struct bv_lod_mesh *, void *));

__END_DECLS

#endif  /* BV_LOD_H */
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
