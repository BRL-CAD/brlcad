/*                 T R I _ I N T E R S E C T . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2013 United States Government as represented by
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

/** @file libgcv/tri_intersect.h
 *
 * Intersect 2 triangles using a modified Möller routine.
 */

#ifndef __TRI_INTERSECT_H__
#define __TRI_INTERSECT_H__

__BEGIN_DECLS

int gcv_tri_tri_intersect_with_isectline(
		struct soup_s *UNUSED(left),
		struct soup_s *UNUSED(right),
		struct face_s *lf,
		struct face_s *rf,
		int *coplanar,
		point_t *isectpt,
		const struct bn_tol *tol);

__END_DECLS

#endif /* __TRI_INTERSECT_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * coding: utf-8
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
