/*                      T R I _ F A C E . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file tri_face.h
 *
 * triangulateFace routine for triangulating convex/concave planar N-gons.
 *
 */

#ifndef TRI_FACE_H
#define TRI_FACE_H

struct faceuse*
make_faceuse_from_face(const double points[], int numPoints);

void
triangulateFace(
    int **faces,
    size_t *numFaces,
    const double points[],
    size_t numPoints,
    struct bn_tol tol);

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
