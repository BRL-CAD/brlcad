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
 * pre-existing cached data, run bg_mesh_lod_clear();
 *
 * returns the lookup key calculated from the data, which is used in subsequent
 * lookups of the cached data. */
BG_EXPORT unsigned long long
bg_mesh_lod_cache(const point_t *v, int vcnt, int *f, int fcnt);

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
BG_EXPORT struct bg_mesh_lod *
bg_mesh_lod_init(unsigned long long key);

/**
 * Given a bview, load the appropriate level of detail for displaying the mesh
 * in that view.  A scale factor may also be supplied to adjust the default
 * level assignments (say, for example, if a parent application wants to drive
 * detail down to increase frame rates.)  Negative values will be removed from
 * the view-based selected level, reducing detail (for example, if the view
 * selection is level 8, and the scale is -2, the mesh will be rendered using
 * level 6.)  Likewise, positive values will be added to increase detail.
 *
 * Returns the level selected.  If v == NULL, return current level of l.  If
 * there is an error or l == NULL, return -1; */
BG_EXPORT int
bg_mesh_lod_view(struct bg_mesh_lod *l, struct bview *v, int scale);

/**
 * Given a detail level, load the appropriate data.
 *
 * Returns the level selected.  If level == -1, return current level of l.  If
 * there is an error, return -1; */
BG_EXPORT int
bg_mesh_lod_level(struct bg_mesh_lod *l, int level);

/**
 * Get a pointer to the current vertex array.  Returns the count of vertices
 * defined, and a pointer to the vertex array in v.
 *
 * This info (both pointer and count) become invalid if lod_view or lod_level
 * are used to change the current detail level.
 */
BG_EXPORT size_t
bg_mesh_lod_verts(const point_t **v, struct bg_mesh_lod *l);

/**
 * The "raw" vertices stored in bg_mesh_lod are not necessarily the final
 * positions a given LoD level setting expects - to calculate the current
 * position for the active level, call vsnap on a point from the verts array
 * retrieved above.  (Drawing using the original points rather than applying
 * this transformation will typically draw a subset of the original mesh
 * triangles in their "final" positions, leaving significant holes in the
 * visible mesh.)
 */
BG_EXPORT void
bg_mesh_lod_vsnap(point_t *o, const point_t *v, struct bg_mesh_lod *l);

/**
 * Get a pointer to the current faces array.  Returns the count
 * of faces defined, and a pointer to the faces array in f.
 */
BG_EXPORT size_t
bg_mesh_lod_faces(const int **f, struct bg_mesh_lod *l);


/* Clean up the lod container. */
BG_EXPORT void
bg_mesh_lod_destroy(struct bg_mesh_lod *lod);

/* Remove cache data associated with key.  If key == 0, remove ALL cache data
 * associated with all LoD objects (i.e. a full LoD cache reset). */
BG_EXPORT void
bg_mesh_lod_clear(unsigned long long key);

/* Set drawing function callback */
BG_EXPORT void
bg_mesh_lod_set_draw_callback(
	struct bg_mesh_lod *lod,
	int (*clbk)(void *ctx, int fset_cnt, int *fset, int fcnt, const int *faces, const point_t *points, const int *face_normals, const vect_t *normals, int mode)
	);

/* Trigger a triangle drawing operation. */
BG_EXPORT void
bg_mesh_lod_draw(struct bg_mesh_lod *lod, void *ctx, int mode);

/* Callback for updating level settings on an object. */
BG_EXPORT int
bg_mesh_lod_update(struct bv_scene_obj *s, int offset);

/* Given a view and a bg_mesh_lod container, return the appropriate vlist of
 * edges for display.  This is the core of the LoD functionality.  The vlist
 * components are managed using the vlfree list supplied by the bview struct,
 * so the caller should treat them like other scene object vlists.
 *
 * Note that this routine and the LoD container behind it are focused only on
 * wireframes - visual artifacts that would be problematic if we were returning
 * a triangle vlist (flipped faces, mesh topology, etc.) are not considered.
 * The focus of this routine is to produce fast, reasonable quality edge
 * visualizations for large meshes to support classic BRL-CAD wireframe mesh
 * views.
 */
BG_EXPORT int
bg_lod_elist(struct bu_list *elist, struct bview *v, struct bg_mesh_lod *l, const char *pname);

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
