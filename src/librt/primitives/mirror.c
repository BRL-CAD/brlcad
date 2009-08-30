/*                        M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2009 United States Government as represented by
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
/** @file mirror.c
 *
 * Routine(s) to mirror objects.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "bn.h"
#include "bu.h"
#include "vmath.h"
#include "nurb.h"


/* FIXME: temporary until all mirror functions are migrated and the
 * functab is utilized.
 */
#define RT_DECLARE_MIRROR(name) BU_EXTERN(int rt_##name##_mirror, (struct rt_db_internal *ip, const plane_t plane));

RT_DECLARE_MIRROR(tor);
RT_DECLARE_MIRROR(tgc);
RT_DECLARE_MIRROR(ell);
RT_DECLARE_MIRROR(arb);
RT_DECLARE_MIRROR(half);
RT_DECLARE_MIRROR(grip);
RT_DECLARE_MIRROR(poly);
RT_DECLARE_MIRROR(bspline);
RT_DECLARE_MIRROR(arbn);
RT_DECLARE_MIRROR(pipe);
RT_DECLARE_MIRROR(particle);
RT_DECLARE_MIRROR(rpc);
RT_DECLARE_MIRROR(rhc);
RT_DECLARE_MIRROR(epa);
RT_DECLARE_MIRROR(eto);
RT_DECLARE_MIRROR(hyp);
RT_DECLARE_MIRROR(nmg);
RT_DECLARE_MIRROR(ars);
RT_DECLARE_MIRROR(ebm);
RT_DECLARE_MIRROR(dsp);


/*
  FIXME: mysteriously missing
  RT_DECLARE_MIRROR(ehy);
*/


/**
 * Mirror an object about some axis at a specified point on the axis.
 * It is the caller's responsibility to retain and free the internal.
 *
 * Returns the modified internal object or NULL on error.
 **/
struct rt_db_internal *
rt_mirror(struct db_i *dbip,
	  struct rt_db_internal *ip,
	  point_t mirror_pt,
	  vect_t mirror_dir,
	  struct resource *resp)
{
    register int i, j;
    int id;
    int err;
    fastf_t ang;
    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    static fastf_t tol_dist_sq = 0.005 * 0.005;
    static point_t origin = {0.0, 0.0, 0.0};
    plane_t plane;

    RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);

    if (!NEAR_ZERO(MAGSQ(mirror_dir) - 1.0, tol_dist_sq)) {
	bu_log("ERROR: mirror direction is invalid\n");
	return NULL;
    }

    if (!resp) {
	resp=&rt_uniresource;
    }

    /* not the best, but consistent until v6 */
    id = ip->idb_type;

    /* set up the plane direction */
    VUNITIZE(mirror_dir);
    VMOVE(plane, mirror_dir);

    /* determine the plane offset */
    plane[W] = VDOT(mirror_pt, mirror_dir);

#if 0
    bu_log("XXX Mirroring about (%lf, %lf, %lf) -> (%lf, %lf, %lf)\n",
	   mirror_pt[X],
	   mirror_pt[Y],
	   mirror_pt[Z],
	   mirror_dir[X],
	   mirror_dir[Y],
	   mirror_dir[Z]);

    bu_log("XXX Mirror eqn is (%lf, %lf, %lf) -> (%lf)\n",
	   plane[X],
	   plane[Y],
	   plane[Z],
	   plane[W]);
#endif


    switch (id) {
	case ID_TOR: {
	    err = rt_tor_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_TGC:
	case ID_REC: {
	    err = rt_tgc_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ELL:
	case ID_SPH: {
	    err = rt_ell_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ARB8: {
	    err = rt_arb_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_HALF: {
	    err = rt_half_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_GRIP: {
	    err = rt_grip_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_POLY: {
	    err = rt_poly_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_BSPLINE: {
	    err = rt_nurb_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ARBN: {
	    err = rt_arbn_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_PIPE: {
	    err = rt_pipe_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_PARTICLE: {
	    err = rt_particle_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_RPC: {
	    err = rt_rpc_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_RHC: {
	    err = rt_rhc_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_EPA: {
	    err = rt_epa_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ETO: {
	    err = rt_eto_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_HYP: {
	    err = rt_hyp_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_NMG: {
	    err = rt_nmg_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ARS: {
	    err = rt_ars_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_EBM: {
	    err = rt_ebm_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_DSP: {
	    err = rt_dsp_mirror(ip, plane);
	    return err ? NULL : ip;
	}
#if 0
	case ID_VOL: {
	    err = rt_vol_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_SUPERELL: {
	    err = rt_superell_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_COMBINATION: {
	    err = rt_comb_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_BOT: {
	    err = rt_bot_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	default: {
	    bu_log("Unknown or unsupported object type (id==%d)\n", id);
	    return NULL;
	}
#endif
    }

    MAT_IDN(mirmat);

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
    mirmat[3 + X*4] += 2.0 * mirror_pt[X] * mirror_dir[X];
    mirmat[3 + Y*4] += 2.0 * mirror_pt[Y] * mirror_dir[Y];
    mirmat[3 + Z*4] += 2.0 * mirror_pt[Z] * mirror_dir[Z];

    switch (id) {
	case ID_VOL: {
	    struct rt_vol_internal *vol;

	    vol = (struct rt_vol_internal *)ip->idb_ptr;
	    RT_VOL_CK_MAGIC(vol);

	    bn_mat_mul(temp, mirmat, vol->mat);
	    MAT_COPY(vol->mat, temp);

	    break;
	}
	case ID_SUPERELL: {
	    struct rt_superell_internal *superell;
	    point_t pt;
	    vect_t a, b, c;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;


	    superell = (struct rt_superell_internal *)ip->idb_ptr;
	    RT_SUPERELL_CK_MAGIC(superell);

	    VMOVE(pt, superell->v);
	    MAT4X3PNT(superell->v, mirmat, pt);

	    VMOVE(a, superell->a);
	    VUNITIZE(a);
	    VCROSS(n, mirror_dir, superell->a);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(a, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(a, superell->a);
	    MAT4X3VEC(superell->a, mat, a);

	    VMOVE(b, superell->b);
	    VUNITIZE(b);
	    VCROSS(n, mirror_dir, superell->b);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(b, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(b, superell->b);
	    MAT4X3VEC(superell->b, mat, b);

	    VMOVE(c, superell->c);
	    VUNITIZE(c);
	    VCROSS(n, mirror_dir, superell->c);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(c, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(c, superell->c);
	    MAT4X3VEC(superell->c, mat, c);

	    superell->n = 1.0;
	    superell->e = 1.0;

	    break;
	}
	case ID_COMBINATION: {
	    struct rt_comb_internal *comb;

	    comb = (struct rt_comb_internal *)ip->idb_ptr;
	    RT_CK_COMB(comb);

	    if (comb->tree) {
		db_tree_mul_dbleaf(comb->tree, mirmat);
	    }
	    break;
	}
	case ID_BOT: {
	    struct rt_bot_internal *bot;

	    bot = (struct rt_bot_internal *)ip->idb_ptr;
	    RT_BOT_CK_MAGIC(bot);

	    /* mirror each vertex */
	    for (i=0; i<bot->num_vertices; i++) {
		point_t pt;

		VMOVE(pt, &bot->vertices[i*3]);
		MAT4X3PNT(&bot->vertices[i*3], mirmat, pt);
	    }

	    /* Reverse each faces' order */
	    for (i=0; i<bot->num_faces; i++) {
		int save_face = bot->faces[i*3];

		bot->faces[i*3] = bot->faces[i*3 + Z];
		bot->faces[i*3 + Z] = save_face;
	    }

	    /* fix normals */
	    for (i=0; i<bot->num_normals; i++) {
		vectp_t np = &bot->normals[i*3];
		vect_t n1;
		vect_t n2;
		fastf_t ang;
		mat_t mat;

		VMOVE(n1, np);
		VCROSS(n2, mirror_dir, n1);
		VUNITIZE(n2);
		ang = M_PI_2 - acos(VDOT(n1, mirror_dir));
		bn_mat_arb_rot(mat, origin, n2, ang*2);
		MAT4X3VEC(np, mat, n1);
	    }

	    break;
	}
	default: {
	    bu_log("Unknown or unsupported object type (id==%d)\n", id);
	    return NULL;
	}
    }

    return ip;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
