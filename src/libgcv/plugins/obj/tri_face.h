/*                      T R I _ F A C E . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
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
 * Shared OBJ face triangulation.
 *
 */

#ifndef LIBGCV_WFOBJ_TRI_FACE_H
#define LIBGCV_WFOBJ_TRI_FACE_H

/* Output triangles contain three indices into the input array of 3D points. */
void
obj_triangulate_face(int **faces, size_t *num_faces, const double points[], size_t num_points,
		     const struct bn_tol *tol, struct bu_list *vlfree);

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
