/*                    E T O _ M I R R O R . C
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
/** @file primitives/eto/eto_mirror.c
 *
 * mirror support
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"


/**
 * R T _ E T O _ M I R R O R
 *
 * Given a pointer to an internal GED database object, mirror the
 * object's values about the given transformation matrix.
 */
int
rt_eto_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct rt_eto_internal *eto;

    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    vect_t mirror_dir;
    point_t mirror_pt;
    fastf_t ang;

    point_t pt;
    vect_t n1;
    vect_t n2;
    vect_t c;

    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    eto = (struct rt_eto_internal *)ip->idb_ptr;
    RT_ETO_CK_MAGIC(eto);

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

    VMOVE(pt, eto->eto_V);
    MAT4X3PNT(eto->eto_V, mirmat, pt);

    VMOVE(n1, eto->eto_N);
    VMOVE(c, eto->eto_C);
    VUNITIZE(n1);
    VUNITIZE(c);

    VCROSS(n2, mirror_dir, eto->eto_N);
    VUNITIZE(n2);
    ang = M_PI_2 - acos(VDOT(n1, mirror_dir));
    bn_mat_arb_rot(rmat, origin, n2, ang*2);
    VMOVE(n1, eto->eto_N);
    MAT4X3VEC(eto->eto_N, rmat, n1);

    VCROSS(n2, mirror_dir, eto->eto_C);
    VUNITIZE(n2);
    ang = M_PI_2 - acos(VDOT(c, mirror_dir));
    bn_mat_arb_rot(rmat, origin, n2, ang*2);
    VMOVE(c, eto->eto_C);
    MAT4X3VEC(eto->eto_C, rmat, c);

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
