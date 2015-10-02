/*                       S O U P . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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

/** @file libgcv/soup.h
 *
 * BoT soup stuff
 *
 */

#ifndef  LIBGCV_SOUP_H
#define  LIBGCV_SOUP_H

#include "common.h"
#include "gcv/defines.h"

/* hijack the top four bits of mode. For these operations, the mode should
 * necessarily be 0x02 */
#define INSIDE		0x01
#define OUTSIDE		0x02
#define SAME		0x04
#define OPPOSITE	0x08
#define INVERTED	0x10

#define SOUP_MAGIC	0x534F5550	/* SOUP, 32b */
#define SOUP_CKMAG(_ptr) BU_CKMAG(_ptr, SOUP_MAGIC, "soup")

struct face_s {
    point_t vert[3], min, max;
    plane_t plane;
    uint32_t foo;
};

struct soup_s {
    uint32_t magic;
    struct face_s *faces;
    unsigned long int nfaces, maxfaces;
};

__BEGIN_DECLS

GCV_EXPORT int soup_add_face(struct soup_s *s, point_t a, point_t b, point_t c, const struct bn_tol *tol);
GCV_EXPORT int split_face_single(struct soup_s *s, unsigned long int fid, point_t isectpt[2], struct face_s *opp_face, const struct bn_tol *tol);
GCV_EXPORT int split_face(struct soup_s *left, unsigned long int left_face, struct soup_s *right, unsigned long int right_face, const struct bn_tol *tol);
GCV_EXPORT int gcv_tri_tri_intersect_with_isectline(struct soup_s *left, struct soup_s *right, struct face_s *lf, struct face_s *rf, int *coplanar, point_t *isectpt, const struct bn_tol *tol);
GCV_EXPORT union tree *compose(union tree *left_tree, union tree *right_tree, unsigned long int face_status1, unsigned long int face_status2, unsigned long int face_status3);
union tree *invert(union tree *tree);
union tree *evaluate(union tree *tr, const struct rt_tess_tol *ttol, const struct bn_tol *tol);

__END_DECLS

#endif /* LIBGCV_SOUP_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
