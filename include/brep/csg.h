/*                       C S G . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2022 United States Government as represented by
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
/** @addtogroup brep_csg
 * @brief
 * Reconstruct CSG Implicit Boolean Hierarchies from Non-Uniform Rational
 * B-Spline (NURBS) Boundary Representations.
 */
#ifndef BREP_CSG_H
#define BREP_CSG_H

#include "common.h"
#include "bu/ptbl.h"
#include "brep/defines.h"

/** @{ */
/** @file brep/csg.h */

#ifdef __cplusplus

extern "C++" {

/* Shape recognition functions - HIGHLY EXPERIMENTAL,
 * DO NOT RELY ON - the odds are quite good that this whole
 * setup will be moving to libanalyze and it's public API
 * there will likely be quite different */

/* Structure for holding parameters corresponding
 * to a csg primitive.  Not all parameters will be
 * used for all primitives - the structure includes
 * enough data slots to describe any primitive that may
 * be matched by the shape recognition logic */
struct csg_object_params {
    struct subbrep_shoal_data *s;
    int csg_type;
    int negative;
    /* Unique id number */
    int csg_id;
    char bool_op; /* Boolean operator - u = union (default), - = subtraction, + = intersection */
    point_t origin;
    vect_t hv;
    fastf_t radius;
    fastf_t r2;
    fastf_t height;
    int arb_type;
    point_t p[8];
    size_t plane_cnt;
    plane_t *planes;
    /* An implicit plane, if present, may close a face on a parent solid */
    int have_implicit_plane;
    point_t implicit_plane_origin;
    vect_t implicit_plane_normal;
    /* bot */
    int csg_face_cnt;
    int csg_vert_cnt;
    int *csg_faces;
    point_t *csg_verts;
    /* information flags */
    int half_cyl;
};

/* Forward declarations */
struct subbrep_island_data;

/* Topological shoal */
struct subbrep_shoal_data {
    struct subbrep_island_data *i;
    int shoal_type;
    /* Unique id number */
    int shoal_id;
    struct csg_object_params *params;
    /* struct csg_obj_params */
    struct bu_ptbl *shoal_children;

    /* Working information */
    int *shoal_loops;
    int shoal_loops_cnt;
};

/* Topological island */
struct subbrep_island_data {

    /* Overall type of island - typically comb or brep, but may
     * be an actual type if the nucleus corresponds to a single
     * implicit primitive */
    int island_type;

    /* Unique id number */
    int island_id;

    /* Context information */
    const ON_Brep *brep;

    /* Shape representation data */
    ON_Brep *local_brep;
    char local_brep_bool_op; /* Boolean operator - u = union (default), - = subtraction, + = intersection */

    /* Nucleus */
    struct subbrep_shoal_data *nucleus;
    /* struct subbrep_shoal_data */
    struct bu_ptbl *island_children;

    /* For union objects, we list the subtractions it needs */
    struct bu_ptbl *subtractions;

    /* subbrep metadata */
    struct bu_vls *key;
    ON_BoundingBox *bbox;

    /* Working information - should probably be in private struct */
    void *face_surface_types;
    int *obj_cnt;
    int *island_faces;
    int *island_loops;
    int *fol; /* Faces with outer loops in object loop network */
    int *fil; /* Faces with only inner loops in object loop network */
    int island_faces_cnt;
    int island_loops_cnt;
    int fol_cnt;
    int fil_cnt;
    int null_vert_cnt;
    int *null_verts;
    int null_edge_cnt;
    int *null_edges;
};


extern BREP_EXPORT struct bu_ptbl *brep_to_csg(struct bu_vls *msgs, const ON_Brep *brep);


} /* extern C++ */
#endif

#endif  /* BREP_CSG_H */
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
