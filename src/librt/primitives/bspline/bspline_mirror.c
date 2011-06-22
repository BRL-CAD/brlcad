/*               B S P L I N E _ M I R R O R . C
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
/** @file primitives/bspline/bspline_mirror.c
 *
 * mirror support
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "nurb.h"


/**
 * R T _ N U R B _ M I R R O R
 *
 * Given a pointer to an internal GED database object, mirror the
 * object's values about the given transformation matrix.
 */
int
rt_nurb_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct rt_nurb_internal *nurb;

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

    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    nurb = (struct rt_nurb_internal *)ip->idb_ptr;
    RT_PG_CK_MAGIC(nurb);

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

    for (i=0; i<nurb->nsrf; i++) {
	fastf_t *ptr;
	int tmp;
	int orig_size[2];
	int ncoords;
	int m;
	int l;

	/* swap knot vetcors between u and v */
	ptr = nurb->srfs[i]->u.knots;
	tmp = nurb->srfs[i]->u.k_size;

	nurb->srfs[i]->u.knots = nurb->srfs[i]->v.knots;
	nurb->srfs[i]->u.k_size = nurb->srfs[i]->v.k_size;
	nurb->srfs[i]->v.knots = ptr;
	nurb->srfs[i]->v.k_size = tmp;

	/* swap order */
	tmp = nurb->srfs[i]->order[0];
	nurb->srfs[i]->order[0] = nurb->srfs[i]->order[1];
	nurb->srfs[i]->order[1] = tmp;

	/* swap mesh size */
	orig_size[0] = nurb->srfs[i]->s_size[0];
	orig_size[1] = nurb->srfs[i]->s_size[1];

	nurb->srfs[i]->s_size[0] = orig_size[1];
	nurb->srfs[i]->s_size[1] = orig_size[0];

	/* allocat memory for a new control mesh */
	ncoords = RT_NURB_EXTRACT_COORDS(nurb->srfs[i]->pt_type);
	ptr = (fastf_t *)bu_calloc(orig_size[0]*orig_size[1]*ncoords, sizeof(fastf_t), "rt_mirror: ctl mesh ptr");

	/* mirror each control point */
	for (j=0; j<orig_size[0]*orig_size[1]; j++) {
	    point_t pt;

	    VMOVE(pt, &nurb->srfs[i]->ctl_points[j*ncoords]);
	    MAT4X3PNT(&nurb->srfs[i]->ctl_points[j*ncoords], mirmat, pt);
	}

	/* copy mirrored control points into new mesh
	 * while swaping u and v */
	m = 0;
	for (j=0; j<orig_size[0]; j++) {
	    for (l=0; l<orig_size[1]; l++) {
		VMOVEN(&ptr[(l*orig_size[0]+j)*ncoords], &nurb->srfs[i]->ctl_points[m*ncoords], ncoords);
		m++;
	    }
	}

	/* free old mesh */
	bu_free((char *)nurb->srfs[i]->ctl_points, "rt_mirror: ctl points");

	/* put new mesh in place */
	nurb->srfs[i]->ctl_points = ptr;
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
