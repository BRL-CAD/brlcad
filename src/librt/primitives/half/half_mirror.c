/*                   H A L F _ M I R R O R . C
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
/** @file primitives/half/half_mirror.c
 *
 * mirror support
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"


/**
 * R T _ H A L F _ M I R R O R
 *
 * Given a pointer to an internal GED database object, mirror the
 * object's values about the given transformation matrix.
 */
int
rt_half_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct rt_half_internal *half;

    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    vect_t mirror_dir;
    point_t mirror_pt;
    fastf_t ang;

    vect_t n1;
    vect_t n2;

    static fastf_t tol_dist_sq = 0.0005 * 0.0005;
    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    half = (struct rt_half_internal *)ip->idb_ptr;
    RT_HALF_CK_MAGIC(half);

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

    /* FIXME: this is not using the mirmat we just computed, not clear
     * it's even right given it's only taking the mirror direction
     * into account and not the mirror point.
     */

    VMOVE(n1, half->eqn);
    VCROSS(n2, mirror_dir, n1);
    VUNITIZE(n2);
    ang = M_PI_2 - acos(VDOT(n1, mirror_dir));
    bn_mat_arb_rot(rmat, origin, n2, ang*2);
    MAT4X3VEC(half->eqn, rmat, n1);

    if (!NEAR_EQUAL(VDOT(n1, half->eqn), 1.0, tol_dist_sq)) {
	point_t ptA;
	point_t ptB;
	point_t ptC;
	vect_t h;
	fastf_t mag;
	fastf_t cosa;

	VSCALE(ptA, n1, half->eqn[H]);
	VADD2(ptB, ptA, mirror_dir);
	VSUB2(h, ptB, ptA);
	mag = MAGNITUDE(h);
	VUNITIZE(h);

	cosa = VDOT(h, mirror_dir);

	VSCALE(ptC, half->eqn, -mag * cosa);
	VADD2(ptC, ptC, ptA);
	half->eqn[H] = VDOT(half->eqn, ptC);
    }

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
