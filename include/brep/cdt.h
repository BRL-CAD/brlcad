/*                      C D T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2019 United States Government as represented by
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

#include "brep/defines.h"

__BEGIN_DECLS

#ifdef __cplusplus
extern "C++" {

    /* Container that holds the state of a triangulation */
    struct ON_Brep_CDT_State;

    /* Create and initialize a CDT state with default tolerances (defaults
     * will be based on an analysis of the brep to try to give "good" results,
     * but Tols will provide handles to override them.) */
    struct ON_Brep_CDT_State *ON_Brep_CDT_Create(ON_Brep *brep);

    /* Destroy a CDT state */
    void ON_Brep_CDT_Destroy(struct ON_Brep_CDT_State *s);

#if 0
    /* Set/get the CDT tolerances. */
    void ON_Brep_CDT_Set_Tols(struct ON_Brep_CDT_State *s, TOL_INFO...);
    void ON_Brep_CDT_Get_Tols(TOL_INFO..., struct ON_Brep_CDT_State *s);

    /* Return the ON_Brep associated with state s. */
    ON_Brep *ON_Brep_CDT_brep(struct ON_Brep_CDT_State *s);
#endif

    /* Given a state, produce a triangulation.  Returns 0 if a solid, valid
     * triangulation was produced, 1 if a triangulation was produced but it
     * isn't solid, and -1 if no triangulation could be produced. If faces is
     * non-null, the triangulation will only attempt to triangulate the
     * specified face(s) and the return code will be the number of successfully
     * triangulated faces.  If the CDT tolerances have been updated since the
     * last Tessellate call, the old tessellation information will be replaced. */
    int ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s, std::vector<int> *faces);

#if 0
    /* Given a state, report the status of its triangulation. -3 indicates a
     * failed attempt to tessellate, -2 indicates a non-solid tessellation is
     * present after an attempt to tessellate all faces, -1 is a state which
     * has had no tessellation attempt made, 0 indicates a solid, valid full
     * brep tessellation is present, and >0 indicates that number of faces has
     * been tessellated but not the full brep. */
    int ON_Brep_CDT_Status(struct ON_Brep_CDT_State *s);

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
    int ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt);

    /* Report the number of other tessellation states which manifest unresolvable
     * overlaps with state s.  If the ovlps argument is non-null, populate with
     * the problematic states.  If no resolve step was performed on s, return -1 */
    int ON_Brep_CDT_UnResolvable_Ovlps(std::vector<struct ON_Brep_CDT_State *> *ovlps, struct ON_Brep_CDT_State *s);

    /* Retrieve the face, vertex and normal information from a tessellation state
     * in the form of integer and fastf_t arrays. */
    int ON_Brep_CDT_Mesh(
	    int **faces, int *fcnt,
	    int **face_normals, int *fn_cnt,
	    fastf_t **vertices, int *vcnt,
	    fastf_t **normals, int *ncnt,
	    const struct ON_Brep_CDT_State *s);

#endif

} /* extern C++ */

__END_DECLS

#endif /* __cplusplus */

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
