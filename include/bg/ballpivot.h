/*                     B A L L P I V O T . H
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file ballpivot.h */
/** @addtogroup bg_surf_recon_ballpivot */
/** @{ */

/**
 *  @brief Surface Reconstruction using Ball Pivoting
 *
 *  This functionality is based on the Bernardini et. al. work from
 *  1999 describing a method of defining a surface on a point cloud
 *  with normals by "rolling" a 3D ball over the set of points and
 *  creating triangles where the ball does not fall through.
 */


#ifndef BG_BALLPIVOT_H
#define BG_BALLPIVOT_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 *@brief
 * Build a surface reconstruction (triangle mesh) using the Ball Pivot
 * methodology.  Note that this has fewer topology guarantees as compared to
 * the SPSR method, so callers must validate that the output has the
 * properties they are looking for.
 *
 * @param[out]  faces set of faces in the output surface, stored as integer indices to the vertices.  The first three indices are the vertices of the face, the second three define the second face, and so forth.
 * @param[out]  num_faces the number of faces in the faces array
 * @param[out]  vertices the set of vertices used by the surface.
 * @param[out]  num_vertices the number of vertices in the surface.
 * @param       input_points_3d The input points
 * @param       num_input_pnts the number of points in the input set
 * @param       radii array of ball radii to be used.  If NULL, a default radius set is calculated based on the point data.
 * @param       radii_cnt number of radii in radii array
 * @return 0 if successful, else error
 *
 */
#define BG_3D_BALLPIVOT_DEFAULT_RADIUS -1
BG_EXPORT int bg_3d_ballpivot(int **faces, int *num_faces, point_t **vertices, int *num_vertices,
			 const point_t *input_points_3d, const vect_t *input_normals_3d,
			 int num_input_pnts, const double *radii, int radii_cnt);

__END_DECLS

#endif  /* BG_BALLPIVOT_H */
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
