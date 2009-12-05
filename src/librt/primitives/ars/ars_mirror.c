/*                    A R S _ M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2009 United States Government as represented by
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
/** @file ars_mirror.c
 *
 * mirror support
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "raytrace.h"
#include "rtgeom.h"


/**
 * R T _ A R S _ M I R R O R
 *
 * Given a pointer to an internal GED database object, mirror the
 * object's values about the given transformation matrix.
 */
int
rt_ars_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct rt_ars_internal *ars;

    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    vect_t mirror_dir;
    point_t mirror_pt;
    fastf_t ang;

    int i;
    int j;
    fastf_t *tmp_curve;

    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    ars = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(ars);

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

    /* mirror each vertex */
    for (i=0; i<ars->ncurves; i++) {
	for (j=0; j<ars->pts_per_curve; j++) {
	    point_t pt;

	    VMOVE(pt, &ars->curves[i][j*3]);
	    MAT4X3PNT(&ars->curves[i][j*3], mirmat, pt);
	}
    }

    /* now reverse order of vertices in each curve */
    tmp_curve = (fastf_t *)bu_calloc(3*ars->pts_per_curve, sizeof(fastf_t), "rt_mirror: tmp_curve");
    for (i=0; i<ars->ncurves; i++) {
	/* reverse vertex order */
	for (j=0; j<ars->pts_per_curve; j++) {
	    VMOVE(&tmp_curve[(ars->pts_per_curve-j-1)*3], &ars->curves[i][j*3]);
	}

	/* now copy back */
	memcpy(ars->curves[i], tmp_curve, ars->pts_per_curve*3*sizeof(fastf_t));
    }

    bu_free((char *)tmp_curve, "rt_mirror: tmp_curve");

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
