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

/* Given a set of points and faces, create the internal data structures needed
 * to support LoD queries.  Note that this is a static container, assuming a
 * non-changing mesh - if the mesh is changed after it is created, the internal
 * container does *NOT* automatically update.  In that case the old struct
 * should be destroyed and a new one created. */
BG_EXPORT struct bg_mesh_lod *
bg_mesh_lod_create(const point_t *v, int vcnt, int *f, int fcnt);

BG_EXPORT struct bg_mesh_lod *
bg_mesh_lod_load(const char *name);

/* Clean up the lod container. */
BG_EXPORT void
bg_mesh_lod_destroy(struct bg_mesh_lod *lod);

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
