/*                    H Y P _ M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2014 United States Government as represented by
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
/** @file primitives/hyp/hyp_mirror.c
 *
 * mirror support
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"


/**
 * Given a pointer to an internal GED database object, mirror the
 * object's values about the given transformation matrix.
 */
int
rt_hyp_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct rt_hyp_internal *hyp;

    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    vect_t mirror_dir;
    point_t mirror_pt;
    fastf_t ang;

    point_t pt;
    vect_t h;
    vect_t a;
    vect_t n;

    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    hyp = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp);

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

    VMOVE(pt, hyp->hyp_Vi);
    MAT4X3PNT(hyp->hyp_Vi, mirmat, pt);

    VMOVE(h, hyp->hyp_Hi);
    VUNITIZE(h);
    VCROSS(n, mirror_dir, hyp->hyp_Hi);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(h, mirror_dir));
    bn_mat_arb_rot(rmat, origin, n, ang*2);
    VMOVE(h, hyp->hyp_Hi);
    MAT4X3VEC(hyp->hyp_Hi, rmat, h);

    VMOVE(a, hyp->hyp_A);
    VUNITIZE(a);
    VCROSS(n, mirror_dir, hyp->hyp_A);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(a, mirror_dir));
    bn_mat_arb_rot(rmat, origin, n, ang*2);
    VMOVE(a, hyp->hyp_A);
    MAT4X3VEC(hyp->hyp_A, rmat, a);

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
