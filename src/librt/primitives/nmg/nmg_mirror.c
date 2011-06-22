/*                    N M G _ M I R R O R . C
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
/** @file primitives/nmg/nmg_mirror.c
 *
 * mirror support
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"


/**
 * R T _ N M G _ M I R R O R
 *
 * Given a pointer to an internal GED database object, mirror the
 * object's values about the given transformation matrix.
 */
int
rt_nmg_mirror(struct rt_db_internal *ip, register const plane_t plane)
{
    struct model *nmg;

    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    vect_t mirror_dir;
    point_t mirror_pt;
    fastf_t ang;

    int i;
    struct nmgregion *r;
    struct shell *s;
    struct bu_ptbl table;
    struct vertex *v;

    static point_t origin = {0.0, 0.0, 0.0};

    static const struct bn_tol tol = {
	BN_TOL_MAGIC, 0.0005, 0.0005*0.0005, 1e-6, 1-1e-6
    };

    RT_CK_DB_INTERNAL(ip);

    nmg = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(nmg);

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

    /* move every vertex */
    nmg_vertex_tabulate(&table, &nmg->magic);
    for (i=0; i<BU_PTBL_END(&table); i++) {
	point_t pt;

	v = (struct vertex *)BU_PTBL_GET(&table, i);
	NMG_CK_VERTEX(v);

	VMOVE(pt, v->vg_p->coord);
	MAT4X3PNT(v->vg_p->coord, mirmat, pt);
    }

    bu_ptbl_reset(&table);

    nmg_face_tabulate(&table, &nmg->magic);
    for (i=0; i<BU_PTBL_END(&table); i++) {
	struct face *f;

	f = (struct face *)BU_PTBL_GET(&table, i);
	NMG_CK_FACE(f);

	if (!f->g.magic_p)
	    continue;

	if (*f->g.magic_p != NMG_FACE_G_PLANE_MAGIC) {
	    bu_log("Sorry, can only mirror NMG solids with planar faces\n");
	    bu_ptbl_free(&table);
	    return 1;
	}
    }

    for (BU_LIST_FOR (r, nmgregion, &nmg->r_hd)) {
	for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	    nmg_invert_shell(s);
	}
    }


    for (i=0; i<BU_PTBL_END(&table); i++) {
	struct face *f;
	struct faceuse *fu;

	f = (struct face *)BU_PTBL_GET(&table, i);
	NMG_CK_FACE(f);

	fu = f->fu_p;
	if (fu->orientation != OT_SAME) {
	    fu = fu->fumate_p;
	}
	if (fu->orientation != OT_SAME) {
	    bu_log("ERROR: Unexpected NMG face orientation\n");
	    bu_ptbl_free(&table);
	    return 2;
	}

	if (nmg_calc_face_g(fu)) {
	    bu_log("ERROR: Unable to calculate NMG faces for mirroring\n");
	    bu_ptbl_free(&table);
	    return 3;
	}
    }

    bu_ptbl_free(&table);

    /* FIXME: why do we need to rebound?  should mirroring really
     * require a tolerance??
     */
    nmg_rebound(nmg, &tol);

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
