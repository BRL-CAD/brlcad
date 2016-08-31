/*                    S U P E R E L L _ M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2016 United States Government as represented by
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
/** @file primitives/superell/superell_mirror.c
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
rt_superell_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct rt_superell_internal *superell;

    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    vect_t mirror_dir;
    point_t mirror_pt;
    fastf_t ang;

    point_t pt;
    vect_t a, b, c;
    vect_t n;

    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    superell = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);

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

    VMOVE(pt, superell->v);
    MAT4X3PNT(superell->v, mirmat, pt);

    VMOVE(a, superell->a);
    VUNITIZE(a);
    VCROSS(n, mirror_dir, superell->a);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(a, mirror_dir));
    bn_mat_arb_rot(rmat, origin, n, ang*2);
    VMOVE(a, superell->a);
    MAT4X3VEC(superell->a, rmat, a);

    VMOVE(b, superell->b);
    VUNITIZE(b);
    VCROSS(n, mirror_dir, superell->b);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(b, mirror_dir));
    bn_mat_arb_rot(rmat, origin, n, ang*2);
    VMOVE(b, superell->b);
    MAT4X3VEC(superell->b, rmat, b);

    VMOVE(c, superell->c);
    VUNITIZE(c);
    VCROSS(n, mirror_dir, superell->c);
    VUNITIZE(n);
    ang = M_PI_2 - acos(VDOT(c, mirror_dir));
    bn_mat_arb_rot(rmat, origin, n, ang*2);
    VMOVE(c, superell->c);
    MAT4X3VEC(superell->c, rmat, c);

    superell->n = 1.0;
    superell->e = 1.0;

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
