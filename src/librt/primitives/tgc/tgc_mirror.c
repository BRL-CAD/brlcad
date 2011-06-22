/*                    T G C _ M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
/** @file primitives/tgc/tgc_mirror.c
 *
 * mirror support
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"


/**
 * R T _ T G C _ M I R R O R
 *
 * Given a pointer to an internal GED database object, mirror the
 * object's values about the given transformation matrix.
 */
int
rt_tgc_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct rt_tgc_internal *tgc;

    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    vect_t mirror_dir;
    point_t mirror_pt;
    fastf_t ang;

    mat_t mat;
    point_t pt;
    vect_t h;
    vect_t a, b, c, d;
    vect_t n;

    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    tgc = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    MAT_IDN(mirmat);

    VMOVE(mirror_dir, plane);
    VSCALE(mirror_pt, plane, plane[W]);

    /* Build mirror transform matrix, for those who need it. */
    /* First, perform a mirror down the X axis */
    mirmat[0] = -1.0;

    /* Create the rotation matrix */
    VSET(xvec, 1, 0, 0);
    VCROSS(nvec, xvec, mirror_dir);
    VUNITIZE(nvec);
    ang = -acos(VDOT(xvec, mirror_dir));
    bn_mat_arb_rot(rmat, origin, nvec, ang*2.0);

    /* Add the rotation to mirmat */
    MAT_COPY(temp, mirmat);
    bn_mat_mul(mirmat, temp, rmat);

    /* Add the translation to mirmat */
    mirmat[3 + X*4] += mirror_pt[X] * mirror_dir[X];
    mirmat[3 + Y*4] += mirror_pt[Y] * mirror_dir[Y];
    mirmat[3 + Z*4] += mirror_pt[Z] * mirror_dir[Z];

    VMOVE(pt, tgc->v);
    MAT4X3PNT(tgc->v, mirmat, pt);

    VMOVE(h, tgc->h);
    VUNITIZE(h);
    VCROSS(n, mirror_dir, tgc->h);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(h, mirror_dir));
    bn_mat_arb_rot(mat, origin, n, ang*2);
    VMOVE(h, tgc->h);
    MAT4X3VEC(tgc->h, mat, h);

    VMOVE(a, tgc->a);
    VUNITIZE(a);
    VCROSS(n, mirror_dir, tgc->a);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(a, mirror_dir));
    bn_mat_arb_rot(mat, origin, n, ang*2);
    VMOVE(a, tgc->a);
    MAT4X3VEC(tgc->a, mat, a);
    VMOVE(c, tgc->c);
    MAT4X3VEC(tgc->c, mat, c);

    VMOVE(b, tgc->b);
    VUNITIZE(b);
    VCROSS(n, mirror_dir, tgc->b);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(b, mirror_dir));
    bn_mat_arb_rot(mat, origin, n, ang*2);
    VMOVE(b, tgc->b);
    MAT4X3VEC(tgc->b, mat, b);
    VMOVE(d, tgc->d);
    MAT4X3VEC(tgc->d, mat, d);

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
