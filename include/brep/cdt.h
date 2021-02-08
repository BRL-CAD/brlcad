/*                      C D T . H
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

#include "bn/vlist.h"
#include "bn/tol.h"
#include "bg/defines.h"
#include "brep/defines.h"

__BEGIN_DECLS

/* Container that holds the state of a triangulation */
struct ON_Brep_CDT_State;

/* Create and initialize a CDT state with default tolerances.  bv
 * must be a pointer to an ON_Brep object. */
extern BREP_EXPORT struct ON_Brep_CDT_State *
ON_Brep_CDT_Create(void *bv, const char *objname);

/* Destroy a CDT state */
extern BREP_EXPORT void
ON_Brep_CDT_Destroy(struct ON_Brep_CDT_State *s);

extern BREP_EXPORT const char *
ON_Brep_CDT_ObjName(struct ON_Brep_CDT_State *s);

/* Set/get the CDT tolerances. */
extern BREP_EXPORT void
ON_Brep_CDT_Tol_Set(struct ON_Brep_CDT_State *s, const struct bg_tess_tol *t);
extern BREP_EXPORT void
ON_Brep_CDT_Tol_Get(struct bg_tess_tol *t, const struct ON_Brep_CDT_State *s);

/* Return the ON_Brep associated with state s. */
extern BREP_EXPORT void *
ON_Brep_CDT_Brep(struct ON_Brep_CDT_State *s);

/* Given a state, produce a triangulation.  Returns 0 if a solid, valid
 * triangulation was produced, 1 if a triangulation was produced but it
 * isn't solid, and -1 if no triangulation could be produced. If faces is
 * non-null, the triangulation will only attempt to triangulate the
 * specified face(s) and the return code will be the number of successfully
 * triangulated faces.  If the CDT tolerances have been updated since the
 * last Tessellate call, the old tessellation information will be replaced. */
extern BREP_EXPORT int
ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s, int face_cnt, int *faces);

/* Given a state, report the status of its triangulation. -3 indicates a
 * failed attempt to tessellate, -2 indicates a non-solid tessellation is
 * present after an attempt to tessellate all faces, -1 is a state which
 * has had no tessellation attempt made, 0 indicates a solid, valid full
 * brep tessellation is present, and >0 indicates that number of faces has
 * been tessellated but not the full brep. */
extern BREP_EXPORT int
ON_Brep_CDT_Status(struct ON_Brep_CDT_State *s);

/* Construct a vlist plot from the tessellation.  Modes are:
 *
 * 0 - shaded 3D triangles
 * 1 - 3D triangle wireframe
 * 2 - 2D triangle wireframe (from parametric space)
 *
 * Returns 0 if vlist was successfully generated, else -1
 */
extern BREP_EXPORT int
ON_Brep_CDT_VList(
    struct bn_vlblock *vbp,
    struct bu_list *vlfree,
    struct bu_color *c,
    int mode,
    struct ON_Brep_CDT_State *s);

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
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt, double lthreshold, int timeout);

#if 0
/* Report the number of other tessellation states which manifest unresolvable
 * overlaps with state s.  If the ovlps argument is non-null, populate with
 * the problematic states.  If no resolve step was performed on s, return -1 */
extern BREP_EXPORT int
ON_Brep_CDT_UnResolvable_Ovlps(std::vector<struct ON_Brep_CDT_State *> *ovlps, struct ON_Brep_CDT_State *s);
#endif

/* Retrieve the face, vertex and normal information from a tessellation state
 * in the form of integer and fastf_t arrays. */
/* TODO - need to allow optional specification of specific faces here -
 * have already hit one scenario where I want triangle information from
 * specific faces. */
extern BREP_EXPORT int
ON_Brep_CDT_Mesh(
    int **faces, int *fcnt,
    fastf_t **vertices, int *vcnt,
    int **face_normals, int *fn_cnt,
    fastf_t **normals, int *ncnt,
    struct ON_Brep_CDT_State *s,
    int exp_face_cnt, int *exp_faces
    );

/* Original (fast but not watertight) routine used for plotting */
#ifdef __cplusplus
extern BREP_EXPORT int
brep_facecdt_plot(struct bu_vls *vls, const char *solid_name,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const ON_Brep *brep, struct bu_list *p_vhead,
	struct bn_vlblock *vbp, struct bu_list *vlfree,
      	int index, int plottype, int num_points);
#endif

/* PImpl exposure of some mesh operations for use in tests - not to be considered public API */
struct cdt_bmesh_impl;
struct cdt_bmesh {
    struct cdt_bmesh_impl *i;
};
extern BREP_EXPORT int cdt_bmesh_create(struct cdt_bmesh **m);
extern BREP_EXPORT void cdt_bmesh_destroy(struct cdt_bmesh *m);
extern BREP_EXPORT int cdt_bmesh_deserialize(const char *fname, struct cdt_bmesh *m);
extern BREP_EXPORT int cdt_bmesh_repair(struct cdt_bmesh *m);

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
