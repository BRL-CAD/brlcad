/*                        M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2008 United States Government as represented by
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
 * Authors -
 *   Christopher Sean Morrison
 *   Michael John Muuss
 *
 * Source -
 *   BRL-CAD Open Source
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

extern int wdb_get_obj_bounds();


/**
 * Mirror an object about some axis at a specified point on the axis.
 *
 * Returns a directory pointer to the new mirrored object
 *
 * Note: mirror_dir is expected to be normalized.
 *
 **/
struct directory *
rt_mirror(struct db_i		*dbip,
	  const char 		*from,
	  const char 		*to,
	  point_t		mirror_origin,
	  vect_t		mirror_dir,
	  fastf_t 		mirror_pt,
	  struct resource 	*resp)
{
    register struct directory *dp;
    struct rt_db_internal internal;

    register int i, j;
    int	id;
    mat_t rmat;
    mat_t mirmat;
    mat_t temp;
    vect_t xvec;
    vect_t nvec;
    fastf_t ang;
    static fastf_t tol_dist_sq = 0.005 * 0.005;
    static point_t origin = {0.0, 0.0, 0.0};

    if (!NEAR_ZERO(MAGSQ(mirror_dir) - 1.0, tol_dist_sq)) {
	bu_log("rt_mirror: mirror_dir is invalid!\n");
	return DIR_NULL;
    }

    if (!resp) {
	resp=&rt_uniresource;
    }

    /* look up the object being mirrored */
    if ((dp = db_lookup(dbip, from, LOOKUP_NOISY)) == DIR_NULL) {
	return DIR_NULL;
    }

    /* make sure object mirroring to does not already exist */
    if (db_lookup(dbip, to, LOOKUP_QUIET) != DIR_NULL) {
	bu_log("%s already exists\n", to);
	return DIR_NULL;
    }

    /* get object being mirrored */
    id = rt_db_get_internal(&internal, dp, dbip, NULL, resp);
    if ( id < 0 )  {
	bu_log("ERROR: Unable to load solid [%s]\n", from);
	return DIR_NULL;
    }
    RT_CK_DB_INTERNAL( &internal );

    mirror_pt *= dbip->dbi_local2base;
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

    /* Factor in the mirror_origin */
    {
	vect_t v;

	VSUB2(v, origin, mirror_origin);
	VUNITIZE(v);

	if (NEAR_ZERO(MAGSQ(v) - 1.0, tol_dist_sq)) {
	    fastf_t h = MAGNITUDE(mirror_origin);
	    fastf_t cosa = VDOT(v, mirror_dir);

	    mirror_pt = mirror_pt - h * cosa;
	}
    }

    /* Add the translation to mirmat */
    mirmat[3 + X*4] += 2 * mirror_pt * mirror_dir[X];
    mirmat[3 + Y*4] += 2 * mirror_pt * mirror_dir[Y];
    mirmat[3 + Z*4] += 2 * mirror_pt * mirror_dir[Z];

    switch (id) {
	case ID_TOR:
	{
	    struct rt_tor_internal *tor;
	    point_t pt;
	    vect_t h;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    tor = (struct rt_tor_internal *)internal.idb_ptr;
	    RT_TOR_CK_MAGIC(tor);

	    VMOVE(pt, tor->v);
	    MAT4X3PNT(tor->v, mirmat, pt);

	    VMOVE(h, tor->h);
	    VUNITIZE(h);

	    VCROSS(n, mirror_dir, tor->h);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    MAT4X3VEC(tor->h, mat, h);

	    break;
	}
	case ID_TGC:
	case ID_REC:
	{
	    struct rt_tgc_internal *tgc;
	    point_t pt;
	    vect_t h;
	    vect_t a, b, c, d;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    tgc = (struct rt_tgc_internal *)internal.idb_ptr;
	    RT_TGC_CK_MAGIC(tgc);

	    VMOVE(pt, tgc->v);
	    MAT4X3PNT(tgc->v, mirmat, pt);

	    VMOVE(h, tgc->h);
	    VUNITIZE(h);
	    VCROSS(n, mirror_dir, tgc->h);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, tgc->h);
	    MAT4X3VEC(tgc->h, mat, h);

	    VMOVE(a, tgc->a);
	    VUNITIZE(a);
	    VCROSS(n, mirror_dir, tgc->a);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(a,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(a, tgc->a);
	    MAT4X3VEC(tgc->a, mat, a);
	    VMOVE(b, tgc->b);
	    MAT4X3VEC(tgc->b, mat, b);

	    VMOVE(c, tgc->c);
	    VUNITIZE(c);
	    VCROSS(n, mirror_dir, tgc->c);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(c,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(c, tgc->c);
	    MAT4X3VEC(tgc->c, mat, c);
	    VMOVE(d, tgc->d);
	    MAT4X3VEC(tgc->d, mat, d);

	    break;
	}
	case ID_ELL:
	case ID_SPH:
	{
	    struct rt_ell_internal *ell;
	    point_t pt;
	    vect_t a, b, c;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    ell = (struct rt_ell_internal *)internal.idb_ptr;
	    RT_ELL_CK_MAGIC(ell);

	    VMOVE(pt, ell->v);
	    MAT4X3PNT(ell->v, mirmat, pt);

	    VMOVE(a, ell->a);
	    VUNITIZE(a);
	    VCROSS(n, mirror_dir, ell->a);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(a,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(a, ell->a);
	    MAT4X3VEC(ell->a, mat, a);

	    VMOVE(b, ell->b);
	    VUNITIZE(b);
	    VCROSS(n, mirror_dir, ell->b);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(b,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(b, ell->b);
	    MAT4X3VEC(ell->b, mat, b);

	    VMOVE(c, ell->c);
	    VUNITIZE(c);
	    VCROSS(n, mirror_dir, ell->c);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(c,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(c, ell->c);
	    MAT4X3VEC(ell->c, mat, c);

	    break;
	}
	case ID_ARB8:
	{
	    struct rt_arb_internal *arb;

	    arb = (struct rt_arb_internal *)internal.idb_ptr;
	    RT_ARB_CK_MAGIC(arb);

	    /* mirror each vertex */
	    for (i=0; i<8; i++) {
		point_t pt;

		VMOVE(pt, arb->pt[i]);
		MAT4X3PNT(arb->pt[i], mirmat, pt);
	    }

	    break;
	}
	case ID_HALF:
	{
	    struct rt_half_internal *haf;
	    vect_t n1;
	    vect_t n2;
	    fastf_t ang;
	    mat_t mat;

	    haf = (struct rt_half_internal *)internal.idb_ptr;
	    RT_HALF_CK_MAGIC(haf);

	    VMOVE(n1, haf->eqn);
	    VCROSS(n2, mirror_dir, n1);
	    VUNITIZE(n2);
	    ang = M_PI_2 - acos(VDOT(n1,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n2, ang*2);
	    MAT4X3VEC(haf->eqn, mat, n1);

	    if (!NEAR_ZERO(VDOT(n1, haf->eqn) - 1.0, tol_dist_sq)) {
		point_t ptA;
		point_t ptB;
		point_t ptC;
		vect_t h;
		vect_t v;
		fastf_t mag;
		fastf_t cosa;

		VSCALE(ptA, n1, haf->eqn[H]);
		VSCALE(v, mirror_dir, 2 * mirror_pt);
		VADD2(ptB, ptA, v);
		VSUB2(h, ptB, ptA);
		mag = MAGNITUDE(h);
		VUNITIZE(h);

		cosa = VDOT(h, mirror_dir);

		VSCALE(ptC, haf->eqn, -mag * cosa);
		VADD2(ptC, ptC, ptA);
		haf->eqn[H] = VDOT(haf->eqn, ptC);
	    }

	    break;
	}
	case ID_GRIP:
	{
	    struct rt_grip_internal *grp;
	    point_t pt;
	    vect_t h;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    grp = (struct rt_grip_internal *)internal.idb_ptr;
	    RT_GRIP_CK_MAGIC(grp);

	    VMOVE(pt, grp->center);
	    MAT4X3PNT(grp->center, mirmat, pt);

	    VMOVE(h, grp->normal);
	    VUNITIZE(h);

	    VCROSS(n, mirror_dir, grp->normal);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    MAT4X3VEC(grp->normal, mat, h);

	    break;
	}
	case ID_POLY:
	{
	    struct rt_pg_internal *pg;
	    fastf_t *verts;
	    fastf_t *norms;

	    pg = (struct rt_pg_internal *)internal.idb_ptr;
	    RT_PG_CK_MAGIC(pg);

	    verts = (fastf_t *)bu_calloc( pg->max_npts*3, sizeof( fastf_t ), "rt_mirror: verts" );
	    norms = (fastf_t *)bu_calloc( pg->max_npts*3, sizeof( fastf_t ), "rt_mirror: norms" );

	    for ( i=0; i<pg->npoly; i++ ) {
		int last;

		last = (pg->poly[i].npts - 1)*3;
		/* mirror coords and temporarily store in reverse order */
		for ( j=0; j<pg->poly[i].npts*3; j += 3 ) {
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
		    ang = M_PI_2 - acos(VDOT(n1,mirror_dir));
		    bn_mat_arb_rot(mat, origin, n2, ang*2);
		    MAT4X3VEC(&pg->poly[i].norms[j], mat, n1);

		    VMOVE(&norms[last-j], &pg->poly[i].norms[j]);
		}

		/* write back mirrored and reversed face loop */
		for ( j=0; j<pg->poly[i].npts*3; j += 3 ) {
		    VMOVE( &pg->poly[i].norms[j], &norms[j] );
		    VMOVE( &pg->poly[i].verts[j], &verts[j] );
		}
	    }

	    bu_free( (char *)verts, "rt_mirror: verts" );
	    bu_free( (char *)norms, "rt_mirror: norms" );

	    break;
	}
	case ID_BSPLINE:
	{
	    struct rt_nurb_internal *nurb;

	    nurb = (struct rt_nurb_internal *)internal.idb_ptr;
	    RT_NURB_CK_MAGIC(nurb);

	    for ( i=0; i<nurb->nsrf; i++ ) {
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
		ncoords = RT_NURB_EXTRACT_COORDS( nurb->srfs[i]->pt_type );
		ptr = (fastf_t *)bu_calloc( orig_size[0]*orig_size[1]*ncoords, sizeof( fastf_t ), "rt_mirror: ctl mesh ptr" );

		/* mirror each control point */
		for ( j=0; j<orig_size[0]*orig_size[1]; j++ ) {
		    point_t pt;

		    VMOVE(pt, &nurb->srfs[i]->ctl_points[j*ncoords]);
		    MAT4X3PNT(&nurb->srfs[i]->ctl_points[j*ncoords], mirmat, pt);
		}

		/* copy mirrored control points into new mesh
		 * while swaping u and v */
		m = 0;
		for ( j=0; j<orig_size[0]; j++ ) {
		    for ( l=0; l<orig_size[1]; l++ ) {
			VMOVEN( &ptr[(l*orig_size[0]+j)*ncoords], &nurb->srfs[i]->ctl_points[m*ncoords], ncoords );
			m++;
		    }
		}

		/* free old mesh */
		bu_free( (char *)nurb->srfs[i]->ctl_points, "rt_mirror: ctl points" );

		/* put new mesh in place */
		nurb->srfs[i]->ctl_points = ptr;
	    }

	    break;
	}
	case ID_ARBN:
	{
	    struct rt_arbn_internal *arbn;

	    arbn = (struct rt_arbn_internal *)internal.idb_ptr;
	    RT_ARBN_CK_MAGIC(arbn);

	    for (i=0; i<arbn->neqn; i++) {
		point_t	orig_pt;
		point_t	pt;
		vect_t	norm;
		fastf_t factor;

		/* unitize the plane equation first */
		factor = 1.0 / MAGNITUDE(arbn->eqn[i]);
		VSCALE(arbn->eqn[i], arbn->eqn[i], factor);
		arbn->eqn[i][3] = arbn->eqn[i][3] * factor;

		/* Pick a point on the original halfspace */
		VSCALE(orig_pt, arbn->eqn[i], arbn->eqn[i][3]);

		/* Transform the point, and the normal */
		MAT4X3VEC(norm, mirmat, arbn->eqn[i]);
		MAT4X3PNT(pt, mirmat, orig_pt);

		/* Measure new distance from origin to new point */
		VUNITIZE(norm);
		VMOVE(arbn->eqn[i], norm);
		arbn->eqn[i][3] = VDOT(pt, norm);
	    }

	    break;
	}
	case ID_PIPE:
	{
	    struct rt_pipe_internal *pipe;
	    struct wdb_pipept *ps;

	    pipe = (struct rt_pipe_internal *)internal.idb_ptr;
	    RT_PIPE_CK_MAGIC(pipe);

	    for (BU_LIST_FOR(ps, wdb_pipept, &pipe->pipe_segs_head)) {
		point_t pt;

		VMOVE(pt, ps->pp_coord);
		MAT4X3PNT(ps->pp_coord, mirmat, pt);
	    }

	    break;
	}
	case ID_PARTICLE:
	{
	    struct rt_part_internal *part;
	    point_t pt;
	    vect_t h;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    part = (struct rt_part_internal *)internal.idb_ptr;
	    RT_PART_CK_MAGIC(part);

	    VMOVE(pt, part->part_V);
	    MAT4X3PNT(part->part_V, mirmat, pt);

	    VMOVE(h, part->part_H);
	    VUNITIZE(h);

	    VCROSS(n, mirror_dir, part->part_H);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, part->part_H);
	    MAT4X3VEC(part->part_H, mat, h);

	    break;
	}
	case ID_RPC:
	{
	    struct rt_rpc_internal *rpc;
	    point_t pt;
	    vect_t h;
	    vect_t b;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    rpc = (struct rt_rpc_internal *)internal.idb_ptr;
	    RT_RPC_CK_MAGIC(rpc);

	    VMOVE(pt, rpc->rpc_V);
	    MAT4X3PNT(rpc->rpc_V, mirmat, pt);

	    VMOVE(h, rpc->rpc_H);
	    VMOVE(b, rpc->rpc_B);
	    VUNITIZE(h);
	    VUNITIZE(b);

	    VCROSS(n, mirror_dir, rpc->rpc_H);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, rpc->rpc_H);
	    MAT4X3VEC(rpc->rpc_H, mat, h);

	    VCROSS(n, mirror_dir, rpc->rpc_B);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(b,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(b, rpc->rpc_B);
	    MAT4X3VEC(rpc->rpc_B, mat, b);

	    break;
	}
	case ID_RHC:
	{
	    struct rt_rhc_internal *rhc;
	    point_t pt;
	    vect_t h;
	    vect_t b;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    rhc = (struct rt_rhc_internal *)internal.idb_ptr;
	    RT_RHC_CK_MAGIC(rhc);

	    VMOVE(pt, rhc->rhc_V);
	    MAT4X3PNT(rhc->rhc_V, mirmat, pt);

	    VMOVE(h, rhc->rhc_H);
	    VMOVE(b, rhc->rhc_B);
	    VUNITIZE(h);
	    VUNITIZE(b);

	    VCROSS(n, mirror_dir, rhc->rhc_H);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, rhc->rhc_H);
	    MAT4X3VEC(rhc->rhc_H, mat, h);

	    VCROSS(n, mirror_dir, rhc->rhc_B);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(b,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(b, rhc->rhc_B);
	    MAT4X3VEC(rhc->rhc_B, mat, b);

	    break;
	}
	case ID_EPA:
	{
	    struct rt_epa_internal *epa;
	    point_t pt;
	    vect_t h;
	    vect_t au;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;

	    epa = (struct rt_epa_internal *)internal.idb_ptr;
	    RT_EPA_CK_MAGIC(epa);

	    VMOVE(pt, epa->epa_V);
	    MAT4X3PNT(epa->epa_V, mirmat, pt);

	    VMOVE(h, epa->epa_H);
	    VMOVE(au, epa->epa_Au);
	    VUNITIZE(h);
	    VUNITIZE(au);

	    VCROSS(n, mirror_dir, epa->epa_H);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(h,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(h, epa->epa_H);
	    MAT4X3VEC(epa->epa_H, mat, h);

	    VCROSS(n, mirror_dir, epa->epa_Au);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(au,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(au, epa->epa_Au);
	    MAT4X3VEC(epa->epa_Au, mat, au);

	    break;
	}
	case ID_ETO:
	{
	    struct rt_eto_internal *eto;
	    point_t pt;
	    vect_t n1;
	    vect_t n2;
	    vect_t c;
	    fastf_t ang;
	    mat_t mat;

	    eto = (struct rt_eto_internal *)internal.idb_ptr;
	    RT_ETO_CK_MAGIC(eto);

	    VMOVE(pt, eto->eto_V);
	    MAT4X3PNT(eto->eto_V, mirmat, pt);

	    VMOVE(n1, eto->eto_N);
	    VMOVE(c, eto->eto_C);
	    VUNITIZE(n1);
	    VUNITIZE(c);

	    VCROSS(n2, mirror_dir, eto->eto_N);
	    VUNITIZE(n2);
	    ang = M_PI_2 - acos(VDOT(n1,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n2, ang*2);
	    VMOVE(n1, eto->eto_N);
	    MAT4X3VEC(eto->eto_N, mat, n1);

	    VCROSS(n2, mirror_dir, eto->eto_C);
	    VUNITIZE(n2);
	    ang = M_PI_2 - acos(VDOT(c,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n2, ang*2);
	    VMOVE(c, eto->eto_C);
	    MAT4X3VEC(eto->eto_C, mat, c);

	    break;
	}
	case ID_NMG:
	{
	    struct model *m;
	    struct nmgregion *r;
	    struct shell *s;
	    struct bu_ptbl table;
	    struct vertex *v;

	    m = (struct model *)internal.idb_ptr;
	    NMG_CK_MODEL( m );

	    /* move every vertex */
	    nmg_vertex_tabulate( &table, &m->magic );
	    for ( i=0; i<BU_PTBL_END( &table ); i++ ) {
		point_t pt;

		v = (struct vertex *)BU_PTBL_GET( &table, i );
		NMG_CK_VERTEX( v );

		VMOVE(pt, v->vg_p->coord);
		MAT4X3PNT(v->vg_p->coord, mirmat, pt);
	    }

	    bu_ptbl_reset( &table );

	    nmg_face_tabulate( &table, &m->magic );
	    for ( i=0; i<BU_PTBL_END( &table ); i++ ) {
		struct face *f;

		f = (struct face *)BU_PTBL_GET( &table, i );
		NMG_CK_FACE( f );

		if ( !f->g.magic_p )
		    continue;

		if ( *f->g.magic_p != NMG_FACE_G_PLANE_MAGIC ) {
		    bu_log("Sorry, can only mirror NMG solids with planar faces\n");
		    bu_log("Error [%s] has \n", from);
		    bu_ptbl_free( &table );
		    rt_db_free_internal( &internal, resp );
		    return DIR_NULL;
		}
	    }

	    for ( BU_LIST_FOR( r, nmgregion, &m->r_hd ) ) {
		for ( BU_LIST_FOR( s, shell, &r->s_hd ) ) {
		    nmg_invert_shell(s);
		}
	    }


	    for ( i=0; i<BU_PTBL_END( &table ); i++ ) {
		struct face *f;
		struct faceuse *fu;

		f = (struct face *)BU_PTBL_GET( &table, i );
		NMG_CK_FACE( f );

		fu = f->fu_p;
		if ( fu->orientation != OT_SAME ) {
		    fu = fu->fumate_p;
		}
		if ( fu->orientation != OT_SAME ) {
		    bu_log("Error with orientation of the NMG faces for [%s]\n", from);
		    bu_ptbl_free( &table );
		    rt_db_free_internal( &internal, resp );
		    return DIR_NULL;
		}

		if ( nmg_calc_face_g( fu ) ) {
		    bu_log("Error calculating the NMG faces for [%s]\n", from);
		    bu_ptbl_free( &table );
		    rt_db_free_internal( &internal, resp );
		    return DIR_NULL;
		}
	    }

	    bu_ptbl_free( &table );
	    nmg_rebound( m, &(dbip->dbi_wdbp->wdb_tol) );

	    break;
	}
	case ID_ARS:
	{
	    struct rt_ars_internal *ars;
	    fastf_t *tmp_curve;

	    ars = (struct rt_ars_internal *)internal.idb_ptr;
	    RT_ARS_CK_MAGIC(ars);

	    /* mirror each vertex */
	    for ( i=0; i<ars->ncurves; i++ ) {
		for ( j=0; j<ars->pts_per_curve; j++ ) {
		    point_t pt;

		    VMOVE(pt, &ars->curves[i][j*3]);
		    MAT4X3PNT(&ars->curves[i][j*3], mirmat, pt);
		}
	    }

	    /* now reverse order of vertices in each curve */
	    tmp_curve = (fastf_t *)bu_calloc( 3*ars->pts_per_curve, sizeof( fastf_t ), "rt_mirror: tmp_curve" );
	    for ( i=0; i<ars->ncurves; i++ ) {
		/* reverse vertex order */
		for ( j=0; j<ars->pts_per_curve; j++ ) {
		    VMOVE( &tmp_curve[(ars->pts_per_curve-j-1)*3], &ars->curves[i][j*3] );
		}

		/* now copy back */
		memcpy(ars->curves[i], tmp_curve, ars->pts_per_curve*3*sizeof( fastf_t ));
	    }

	    bu_free( (char *)tmp_curve, "rt_mirror: tmp_curve" );

	    break;
	}
	case ID_EBM:
	{
	    struct rt_ebm_internal *ebm;

	    ebm = (struct rt_ebm_internal *)internal.idb_ptr;
	    RT_EBM_CK_MAGIC(ebm);

	    bn_mat_mul( temp, mirmat, ebm->mat );
	    MAT_COPY( ebm->mat, temp );

	    break;
	}
	case ID_DSP:
	{
	    struct rt_dsp_internal *dsp;

	    dsp = (struct rt_dsp_internal *)internal.idb_ptr;
	    RT_DSP_CK_MAGIC(dsp);

	    bn_mat_mul( temp, mirmat, dsp->dsp_mtos);
	    MAT_COPY( dsp->dsp_mtos, temp);

	    break;
	}
	case ID_VOL:
	{
	    struct rt_vol_internal *vol;

	    vol = (struct rt_vol_internal *)internal.idb_ptr;
	    RT_VOL_CK_MAGIC(vol);

	    bn_mat_mul( temp, mirmat, vol->mat );
	    MAT_COPY( vol->mat, temp );

	    break;
	}
	case ID_SUPERELL:
	{
	    struct rt_superell_internal *superell;
	    point_t pt;
	    vect_t a, b, c;
	    vect_t n;
	    fastf_t ang;
	    mat_t mat;


	    superell = (struct rt_superell_internal *)internal.idb_ptr;
	    RT_SUPERELL_CK_MAGIC(superell);

	    VMOVE(pt, superell->v);
	    MAT4X3PNT(superell->v, mirmat, pt);

	    VMOVE(a, superell->a);
	    VUNITIZE(a);
	    VCROSS(n, mirror_dir, superell->a);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(a,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(a, superell->a);
	    MAT4X3VEC(superell->a, mat, a);

	    VMOVE(b, superell->b);
	    VUNITIZE(b);
	    VCROSS(n, mirror_dir, superell->b);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(b,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(b, superell->b);
	    MAT4X3VEC(superell->b, mat, b);

	    VMOVE(c, superell->c);
	    VUNITIZE(c);
	    VCROSS(n, mirror_dir, superell->c);
	    VUNITIZE(n);
	    ang = M_PI_2 - acos(VDOT(c,mirror_dir));
	    bn_mat_arb_rot(mat, origin, n, ang*2);
	    VMOVE(c, superell->c);
	    MAT4X3VEC(superell->c, mat, c);

	    superell->n = 1.0;
	    superell->e = 1.0;

	    break;
	}
	case ID_COMBINATION:
	{
	    struct rt_comb_internal	*comb;

	    comb = (struct rt_comb_internal *)internal.idb_ptr;
	    RT_CK_COMB(comb);

	    if ( comb->tree ) {
		db_tree_mul_dbleaf( comb->tree, mirmat );
	    }
	    break;
	}
	case ID_BOT:
	{
	    struct rt_bot_internal *bot;

	    bot = (struct rt_bot_internal *)internal.idb_ptr;
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

#if 0
		/*XXX should already be normalized */
		VUNITIZE(n1);
#endif

		VCROSS(n2, mirror_dir, n1);
		VUNITIZE(n2);
		ang = M_PI_2 - acos(VDOT(n1,mirror_dir));
		bn_mat_arb_rot(mat, origin, n2, ang*2);
		MAT4X3VEC(np, mat, n1);
	    }

	    break;
	}
	default:
	{
	    rt_db_free_internal( &internal, resp );
	    bu_log("Unknown or unsupported object type (id==%d)\n", id);
	    bu_log("ERROR: cannot mirror object [%s]\n", from);
	    return DIR_NULL;
	}
    }

    /* save the mirrored object to disk */
    if ( (dp = db_diradd( dbip, to, -1L, 0, dp->d_flags, (genptr_t)&internal.idb_type)) == DIR_NULL )  {
	bu_log("ERROR: Unable to add [%s] to the database directory", to);
	return DIR_NULL;
    }
    if ( rt_db_put_internal( dp, dbip, &internal, resp ) < 0 )  {
	bu_log("ERROR: Unable to store [%s] to the database", to);
	return DIR_NULL;
    }

    return dp;
}

#if 0
/**
 * Call rt_mirror using the specified object's center as the mirror_origin.
 *
 **/
struct directory *
rt_mirror2(struct rt_wdb	*wdbp,
	   Tcl_Interp		*interp,
	   const char 		*from,
	   const char 		*to,
	   vect_t		mirror_dir,
	   fastf_t 		mirror_pt,
	   struct resource 	*resp)
{
    point_t center;
    point_t rpp_min, rpp_max;
    char *av[2];
    int use_air = 1;

    av[0] = (char *)from;
    av[1] = (char *)0;

    /*XXX This is known to fail on grips and half spaces. */
    if (wdb_get_obj_bounds(wdbp, interp, 1, av, use_air, rpp_min, rpp_max) == TCL_ERROR) {
	bu_log("rt_mirror1: unable to acquire bounds for %s\n", from);
	return DIR_NULL;
    }

    VADD2(center, rpp_min, rpp_max);
    VSCALE(center, center, 0.5);

    return rt_mirror(wdbp->dbip, from, to, center, mirror_dir, mirror_pt, resp);
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
