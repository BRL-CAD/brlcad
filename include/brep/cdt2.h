/*                      C D T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @{ */
/** @file brep/cdt.h */
/** @addtogroup brep_util
 *
 * @brief
 * Constrained Delaunay Triangulation of brep solids.
 *
 */

#ifndef BREP_CDT_H
#define BREP_CDT_H

#include "common.h"

#include "bg/defines.h"
#include "brep/defines.h"

__BEGIN_DECLS

/* Container that holds the state of a triangulation */
struct brep_cdt_impl;
struct brep_cdt {
    struct brep_cdt_impl *i;
    struct bu_vls *msgs;
};

/* Create and initialize a CDT state with default tolerances.  bv
 * must be a pointer to an ON_Brep object. */
extern BREP_EXPORT int
brep_cdt_create(struct brep_cdt **cdt, void *bv, const char *objname);

/* Destroy a CDT state */
extern BREP_EXPORT void
brep_cdt_destroy(struct brep_cdt *s);

extern BREP_EXPORT const char *
brep_cdt_objname(struct brep_cdt *s);

/* Set/get the CDT tolerances. */
extern BREP_EXPORT void
brep_cdt_tol_set(struct brep_cdt *s, const struct bg_tess_tol *t);
extern BREP_EXPORT void
brep_cdt_tol_get(struct bg_tess_tol *t, const struct brep_cdt *s);

/* Return a pointer to the ON_Brep associated with state s. */
extern BREP_EXPORT void *
brep_cdt_brep_get(struct brep_cdt *s);

/* Given a state, produce a triangulation.  Returns 0 if a solid, valid
 * triangulation was produced, 1 if a triangulation was produced but it
 * isn't solid, and -1 if no triangulation could be produced. If faces is
 * non-null, the triangulation will only attempt to triangulate the
 * specified face(s) and the return code will be the number of successfully
 * triangulated faces.  If the CDT tolerances have been updated since the
 * last Tessellate call, the old tessellation information will be replaced. */
extern BREP_EXPORT int
brep_cdt_triangulate(struct brep_cdt *s, int face_cnt, int *faces, long flags);

/* The default triangulation attempts to satisfy a number of mesh quality constraints,
 * which slows the triangulation process.  The following flags allow the caller to
 * disable them to trade off increased speed for lower quality mesh outputs */
#define BG_CDT_NO_WATERTIGHT       0x1   /**< @brief Disable watertight meshing. */
#define BG_CDT_NO_EDGE_OPTIMIZE    0x2   /**< @brief Disable distortion correction near face edges. */
#define BG_CDT_NO_VALIDITY         0x4   /**< @brief Disable triangle validity checking. */

/* Given a state, report the status of its triangulation. -4 indicate an
 * unsuitable ON_Brep is present, -3 indicates a failed attempt on a
 * theoretically usable ON_Brep, -2 indicates a non-solid result is present
 * after an attempt to process all faces, -1 indicates a state which no attempt
 * has been made, 0 indicates a solid, valid full brep triangulation is
 * present, and >0 indicates the number of faces has been triangulated short of
 * processing the full brep, in case a partial list had been supplied to
 * brep_cdt_triangulate. */
extern BREP_EXPORT int
brep_cdt_status(struct brep_cdt *s);

/* Construct a vlist plot from the triangulation.  Modes are:
 *
 * 0 - shaded 3D triangles
 * 1 - 3D triangle wireframe
 * 2 - 2D triangle wireframe (from parametric space)
 *
 * Returns 0 if vlist was successfully generated, else -1
 */
extern BREP_EXPORT int
brep_cdt_vlist(
    struct bn_vlblock *vbp,
    struct bu_list *vlfree,
    struct bu_color *c,
    int mode,
    struct brep_cdt *s);

/* Given two or more triangulation states, refine them to clear any face
 * overlaps introduced by the triangulation.  If any of the states are
 * un-tessellated, first perform the tessellation indicated by the state
 * settings and then proceed to resolve after all states have an initial
 * tessellation.  Returns 0 if no changes were needed, the number of
 * updated CDT states if changes were made, and -1 if one or more
 * unresolvable overlaps were encountered.  Individual CDT states may
 * subsequently be queried for other information about their specific
 * states with other function calls - this function returns only the
 * overall result. */
extern BREP_EXPORT int
brep_cdt_ovlp_resolve(struct brep_cdt **s_a, int s_cnt, double lthreshold, int timeout);

/* Retrieve the face, vertex and normal information from a tessellation state
 * in the form of integer and fastf_t arrays. */
extern BREP_EXPORT int
brep_cdt_mesh(
    int **faces, int *fcnt, fastf_t **vertices, int *vcnt,
    int **face_normals, int *fn_cnt, fastf_t **normals, int *ncnt,
    struct brep_cdt *s, int exp_face_cnt, int *exp_faces
    );

__END_DECLS

/** @} */

#endif  /* BREP_CDT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
