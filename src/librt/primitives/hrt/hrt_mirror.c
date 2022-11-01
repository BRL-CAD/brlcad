/*                    H R T _ M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2022 United States Government as represented by
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
/** @file primitives/hrt/hrt_mirror.c
 *
 * mirror support
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"


/**
 * Given a pointer to an internal GED database object, mirror the
 * object's values about the given transformation matrix.
 */
int
rt_hrt_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct rt_hrt_internal *hrt;

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
    vect_t x, y, z;
    vect_t n;

    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    hrt = (struct rt_hrt_internal *)ip->idb_ptr;
    RT_HRT_CK_MAGIC(hrt);

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

    VMOVE(pt, hrt->v);
    MAT4X3PNT(hrt->v, mirmat, pt);

    VMOVE(x, hrt->xdir);
    VUNITIZE(x);
    VCROSS(n, mirror_dir, hrt->xdir);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(x, mirror_dir));
    bn_mat_arb_rot(mat, origin, n, ang*2);
    VMOVE(x, hrt->xdir);
    MAT4X3VEC(hrt->xdir, mat, x);

    VMOVE(y, hrt->ydir);
    VUNITIZE(y);
    VCROSS(n, mirror_dir, hrt->ydir);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(y, mirror_dir));
    bn_mat_arb_rot(mat, origin, n, ang*2);
    VMOVE(y, hrt->ydir);
    MAT4X3VEC(hrt->ydir, mat, y);

    VMOVE(z, hrt->zdir);
    VUNITIZE(z);
    VCROSS(n, mirror_dir, hrt->zdir);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(z, mirror_dir));
    bn_mat_arb_rot(mat, origin, n, ang*2);
    VMOVE(z, hrt->zdir);
    MAT4X3VEC(hrt->zdir, mat, z);

    return 0;
}
