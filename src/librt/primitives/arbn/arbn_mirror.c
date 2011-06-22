/*                   A R B N _ M I R R O R . C
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
/** @file primitives/arbn/arbn_mirror.c
 *
 * mirror support
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"


/**
 * R T _ A R B N _ M I R R O R
 *
 * Given a pointer to an internal GED database object, mirror the
 * object's values about the given transformation matrix.
 */
int
rt_arbn_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct rt_arbn_internal *arbn;

    size_t i;
    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    vect_t mirror_dir;
    point_t mirror_pt;
    fastf_t ang;

    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    arbn = (struct rt_arbn_internal *)ip->idb_ptr;
    RT_ARBN_CK_MAGIC(arbn);

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

    for (i=0; i<arbn->neqn; i++) {
	point_t orig_pt;
	point_t pt;
	vect_t norm;
	fastf_t factor;

	/* unitize the plane equation first */
	factor = 1.0 / MAGNITUDE(arbn->eqn[i]);
	VSCALE(arbn->eqn[i], arbn->eqn[i], factor);
	arbn->eqn[i][W] = arbn->eqn[i][W] * factor;

	/* Pick a point on the original halfspace */
	VSCALE(orig_pt, arbn->eqn[i], arbn->eqn[i][W]);

	/* Transform the point, and the normal */
	MAT4X3VEC(norm, mirmat, arbn->eqn[i]);
	MAT4X3PNT(pt, mirmat, orig_pt);

	/* Measure new distance from origin to new point */
	VUNITIZE(norm);
	VMOVE(arbn->eqn[i], norm);
	arbn->eqn[i][W] = VDOT(pt, norm);
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
