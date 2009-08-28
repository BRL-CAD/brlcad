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
#if 0
	case ID_GRIP: {
	    err = rt_grip_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_POLY: {
	    err = rt_poly_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_BSPLINE: {
	    err = rt_nurb_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_ARBN: {
	    err = rt_arbn_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_PIPE: {
	    err = rt_pipe_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_PARTICLE: {
	    err = rt_particle_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_RPC: {
	    err = rt_rpc_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_RHC: {
	    err = rt_rhc_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_EPA: {
	    err = rt_epa_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_ETO: {
	    err = rt_eto_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_HYP: {
	    err = rt_hyp_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_NMG: {
	    err = rt_nmg_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_ARS: {
	    err = rt_ars_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_EBM: {
	    err = rt_ebm_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
	case ID_DSP: {
	    err = rt_dsp_mirror(ip, &plane);
	    return err ? NULL : ip;
	}
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
	case ID_GRIP: {
	    struct rt_grip_internal *grp;
	    point_t pt;
	    vect_t h;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    grp = (struct rt_grip_internal *)ip->idb_ptr;
	    RT_GRIP_CK_MAGIC(grp);

	    VMOVE(pt, grp->center);
	    MAT4X3PNT(grp->center, mirmat, pt);

	    VMOVE(h, grp->normal);
	    VUNITIZE(h);

	    VCROSS(n, mirror_dir, grp->normal);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    MAT4X3VEC(grp->normal, mat, h);

	    break;
	}
	case ID_POLY: {
	    struct rt_pg_internal *pg;
	    fastf_t *verts;
	    fastf_t *norms;

	    pg = (struct rt_pg_internal *)ip->idb_ptr;
	    RT_PG_CK_MAGIC(pg);

	    verts = (fastf_t *)bu_calloc(pg->max_npts*3, sizeof(fastf_t), "rt_mirror: verts");
	    norms = (fastf_t *)bu_calloc(pg->max_npts*3, sizeof(fastf_t), "rt_mirror: norms");

	    for (i=0; i<pg->npoly; i++) {
		int last;

		last = (pg->poly[i].npts - 1)*3;
		/* mirror coords and temporarily store in reverse order */
		for (j=0; j<pg->poly[i].npts*3; j += 3) {
		    point_t pt;
		    vect_t n1;
		    vect_t n2;
		    mat_t mat;

		    VMOVE(pt, &pg->poly[i].verts[j]);
		    MAT4X3PNT(&pg->poly[i].verts[j], mirmat, pt);

		    VMOVE(n1, &pg->poly[i].norms[j]);
		    VUNITIZE(n1);

		    VCROSS(n2, mirror_dir, &pg->poly[i].norms[j]);
		    VUNITIZE(n2);
		    ang = M_PI_2 - acos(VDOT(n1, mirror_dir));
		    bn_mat_arb_rot(mat, origin, n2, ang*2);
		    MAT4X3VEC(&pg->poly[i].norms[j], mat, n1);

		    VMOVE(&norms[last-j], &pg->poly[i].norms[j]);
		}

		/* write back mirrored and reversed face loop */
		for (j=0; j<pg->poly[i].npts*3; j += 3) {
		    VMOVE(&pg->poly[i].norms[j], &norms[j]);
		    VMOVE(&pg->poly[i].verts[j], &verts[j]);
		}
	    }

	    bu_free((char *)verts, "rt_mirror: verts");
	    bu_free((char *)norms, "rt_mirror: norms");

	    break;
	}
	case ID_BSPLINE: {
	    struct rt_nurb_internal *nurb;

	    nurb = (struct rt_nurb_internal *)ip->idb_ptr;
	    RT_NURB_CK_MAGIC(nurb);

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

	    break;
	}
	case ID_ARBN: {
	    struct rt_arbn_internal *arbn;

	    arbn = (struct rt_arbn_internal *)ip->idb_ptr;
	    RT_ARBN_CK_MAGIC(arbn);

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

	    break;
	}
	case ID_PIPE: {
	    struct rt_pipe_internal *pipe;
	    struct wdb_pipept *ps;

	    pipe = (struct rt_pipe_internal *)ip->idb_ptr;
	    RT_PIPE_CK_MAGIC(pipe);

	    for (BU_LIST_FOR (ps, wdb_pipept, &pipe->pipe_segs_head)) {
		point_t pt;

		VMOVE(pt, ps->pp_coord);
		MAT4X3PNT(ps->pp_coord, mirmat, pt);
	    }

	    break;
	}
	case ID_PARTICLE: {
	    struct rt_part_internal *part;
	    point_t pt;
	    vect_t h;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    part = (struct rt_part_internal *)ip->idb_ptr;
	    RT_PART_CK_MAGIC(part);

	    VMOVE(pt, part->part_V);
	    MAT4X3PNT(part->part_V, mirmat, pt);

	    VMOVE(h, part->part_H);
	    VUNITIZE(h);

	    VCROSS(n, mirror_dir, part->part_H);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, part->part_H);
	    MAT4X3VEC(part->part_H, mat, h);

	    break;
	}
	case ID_RPC: {
	    struct rt_rpc_internal *rpc;
	    point_t pt;
	    vect_t h;
	    vect_t b;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    rpc = (struct rt_rpc_internal *)ip->idb_ptr;
	    RT_RPC_CK_MAGIC(rpc);

	    VMOVE(pt, rpc->rpc_V);
	    MAT4X3PNT(rpc->rpc_V, mirmat, pt);

	    VMOVE(h, rpc->rpc_H);
	    VMOVE(b, rpc->rpc_B);
	    VUNITIZE(h);
	    VUNITIZE(b);

	    VCROSS(n, mirror_dir, rpc->rpc_H);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, rpc->rpc_H);
	    MAT4X3VEC(rpc->rpc_H, mat, h);

	    VCROSS(n, mirror_dir, rpc->rpc_B);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(b, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(b, rpc->rpc_B);
	    MAT4X3VEC(rpc->rpc_B, mat, b);

	    break;
	}
	case ID_RHC: {
	    struct rt_rhc_internal *rhc;
	    point_t pt;
	    vect_t h;
	    vect_t b;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    rhc = (struct rt_rhc_internal *)ip->idb_ptr;
	    RT_RHC_CK_MAGIC(rhc);

	    VMOVE(pt, rhc->rhc_V);
	    MAT4X3PNT(rhc->rhc_V, mirmat, pt);

	    VMOVE(h, rhc->rhc_H);
	    VMOVE(b, rhc->rhc_B);
	    VUNITIZE(h);
	    VUNITIZE(b);

	    VCROSS(n, mirror_dir, rhc->rhc_H);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, rhc->rhc_H);
	    MAT4X3VEC(rhc->rhc_H, mat, h);

	    VCROSS(n, mirror_dir, rhc->rhc_B);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(b, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(b, rhc->rhc_B);
	    MAT4X3VEC(rhc->rhc_B, mat, b);

	    break;
	}
	case ID_EPA: {
	    struct rt_epa_internal *epa;
	    point_t pt;
	    vect_t h;
	    vect_t au;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    epa = (struct rt_epa_internal *)ip->idb_ptr;
	    RT_EPA_CK_MAGIC(epa);

	    VMOVE(pt, epa->epa_V);
	    MAT4X3PNT(epa->epa_V, mirmat, pt);

	    VMOVE(h, epa->epa_H);
	    VMOVE(au, epa->epa_Au);
	    VUNITIZE(h);
	    VUNITIZE(au);

	    VCROSS(n, mirror_dir, epa->epa_H);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, epa->epa_H);
	    MAT4X3VEC(epa->epa_H, mat, h);

	    VCROSS(n, mirror_dir, epa->epa_Au);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(au, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(au, epa->epa_Au);
	    MAT4X3VEC(epa->epa_Au, mat, au);

	    break;
	}
	case ID_ETO: {
	    struct rt_eto_internal *eto;
	    point_t pt;
	    vect_t n1;
	    vect_t n2;
	    vect_t c;
	    fastf_t ang;
	    mat_t mat;

	    eto = (struct rt_eto_internal *)ip->idb_ptr;
	    RT_ETO_CK_MAGIC(eto);

	    VMOVE(pt, eto->eto_V);
	    MAT4X3PNT(eto->eto_V, mirmat, pt);

	    VMOVE(n1, eto->eto_N);
	    VMOVE(c, eto->eto_C);
	    VUNITIZE(n1);
	    VUNITIZE(c);

	    VCROSS(n2, mirror_dir, eto->eto_N);
	    VUNITIZE(n2);
	    ang = M_PI_2 - acos(VDOT(n1, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n2, ang*2);
	    VMOVE(n1, eto->eto_N);
	    MAT4X3VEC(eto->eto_N, mat, n1);

	    VCROSS(n2, mirror_dir, eto->eto_C);
	    VUNITIZE(n2);
	    ang = M_PI_2 - acos(VDOT(c, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n2, ang*2);
	    VMOVE(c, eto->eto_C);
	    MAT4X3VEC(eto->eto_C, mat, c);

	    break;
	}

	case ID_HYP: {
	    struct rt_hyp_internal *hyp;
	    point_t pt;
	    vect_t h;
	    vect_t a;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    hyp = (struct rt_hyp_internal *)ip->idb_ptr;
	    RT_HYP_CK_MAGIC(hyp);

	    VMOVE(pt, hyp->hyp_Vi);
	    MAT4X3PNT(hyp->hyp_Vi, mirmat, pt);

	    VMOVE(h, hyp->hyp_Hi);
	    VUNITIZE(h);
	    VCROSS(n, mirror_dir, hyp->hyp_Hi);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, hyp->hyp_Hi);
	    MAT4X3VEC(hyp->hyp_Hi, mat, h);

	    VMOVE(a, hyp->hyp_A);
	    VUNITIZE(a);
	    VCROSS(n, mirror_dir, hyp->hyp_A);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(a, mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(a, hyp->hyp_A);
	    MAT4X3VEC(hyp->hyp_A, mat, a);

	    break;
	}
	case ID_NMG: {
	    struct model *m;
	    struct nmgregion *r;
	    struct shell *s;
	    struct bu_ptbl table;
	    struct vertex *v;

	    m = (struct model *)ip->idb_ptr;
	    NMG_CK_MODEL(m);

	    /* move every vertex */
	    nmg_vertex_tabulate(&table, &m->magic);
	    for (i=0; i<BU_PTBL_END(&table); i++) {
		point_t pt;

		v = (struct vertex *)BU_PTBL_GET(&table, i);
		NMG_CK_VERTEX(v);

		VMOVE(pt, v->vg_p->coord);
		MAT4X3PNT(v->vg_p->coord, mirmat, pt);
	    }

	    bu_ptbl_reset(&table);

	    nmg_face_tabulate(&table, &m->magic);
	    for (i=0; i<BU_PTBL_END(&table); i++) {
		struct face *f;

		f = (struct face *)BU_PTBL_GET(&table, i);
		NMG_CK_FACE(f);

		if (!f->g.magic_p)
		    continue;

		if (*f->g.magic_p != NMG_FACE_G_PLANE_MAGIC) {
		    bu_log("Sorry, can only mirror NMG solids with planar faces\n");
		    bu_ptbl_free(&table);
		    return NULL;
		}
	    }

	    for (BU_LIST_FOR (r, nmgregion, &m->r_hd)) {
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
		    return NULL;
		}

		if (nmg_calc_face_g(fu)) {
		    bu_log("ERROR: Unable to calculate NMG faces for mirroring\n");
		    bu_ptbl_free(&table);
		    return NULL;
		}
	    }

	    bu_ptbl_free(&table);
	    nmg_rebound(m, &(dbip->dbi_wdbp->wdb_tol));

	    break;
	}
	case ID_ARS: {
	    struct rt_ars_internal *ars;
	    fastf_t *tmp_curve;

	    ars = (struct rt_ars_internal *)ip->idb_ptr;
	    RT_ARS_CK_MAGIC(ars);

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

	    break;
	}
	case ID_EBM: {
	    struct rt_ebm_internal *ebm;

	    ebm = (struct rt_ebm_internal *)ip->idb_ptr;
	    RT_EBM_CK_MAGIC(ebm);

	    bn_mat_mul(temp, mirmat, ebm->mat);
	    MAT_COPY(ebm->mat, temp);

	    break;
	}
	case ID_DSP: {
	    struct rt_dsp_internal *dsp;

	    dsp = (struct rt_dsp_internal *)ip->idb_ptr;
	    RT_DSP_CK_MAGIC(dsp);

	    bn_mat_mul(temp, mirmat, dsp->dsp_mtos);
	    MAT_COPY(dsp->dsp_mtos, temp);

	    break;
	}
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
