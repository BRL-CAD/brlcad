/*                            P O L Y . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/poly/poly.c
 *
 * Intersect a ray with a Polygonal Object that has no explicit
 * topology.  It is assumed that the solid has no holes.
 *
 */
/** @} */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../../librt_private.h"


#define TRI_NULL ((struct tri_specific *)0)

HIDDEN int rt_pgface(struct soltab *stp, fastf_t *ap, fastf_t *bp, fastf_t *cp, const struct bn_tol *tol);

/* Describe algorithm here */

/**
 * Calculate the bounding RPP for a poly
 */
int
rt_pg_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol))
{
    struct rt_pg_internal *pgp;
    size_t i;
    size_t p;

    pgp = (struct rt_pg_internal *)ip->idb_ptr;
    RT_PG_CK_MAGIC(pgp);

    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    for (p = 0; p < pgp->npoly; p++) {
	vect_t work[3];

	VMOVE(work[0], &pgp->poly[p].verts[0*3]);
	VMINMAX((*min), (*max), work[0]);
	VMOVE(work[1], &pgp->poly[p].verts[1*3]);
	VMINMAX((*min), (*max), work[1]);

	for (i=2; i < pgp->poly[p].npts; i++) {
	    VMOVE(work[2], &pgp->poly[p].verts[i*3]);
	    VMINMAX((*min), (*max), work[2]);

	    /* Chop off a triangle, and continue */
	    VMOVE(work[1], work[2]);
	}
    }

    return 0;		/* OK */
}


/**
 * This routine is used to prepare a list of planar faces for
 * being shot at by the triangle routines.
 *
 * Process a PG, which is represented as a vector
 * from the origin to the first point, and many vectors
 * from the first point to the remaining points.
 *
 */
int
rt_pg_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_pg_internal *pgp;
    size_t i;
    size_t p;

    pgp = (struct rt_pg_internal *)ip->idb_ptr;
    RT_PG_CK_MAGIC(pgp);

    if (rt_pg_bbox(ip, &(stp->st_min), &(stp->st_max), &rtip->rti_tol)) return 1;

    for (p = 0; p < pgp->npoly; p++) {
	vect_t work[3];

	VMOVE(work[0], &pgp->poly[p].verts[0*3]);
	VMOVE(work[1], &pgp->poly[p].verts[1*3]);

	for (i=2; i < pgp->poly[p].npts; i++) {
	    VMOVE(work[2], &pgp->poly[p].verts[i*3]);

	    /* output a face */
	    (void)rt_pgface(stp,
			    work[0], work[1], work[2], &rtip->rti_tol);

	    /* Chop off a triangle, and continue */
	    VMOVE(work[1], work[2]);
	}
    }
    if (stp->st_specific == (void *)0) {
	bu_log("pg(%s):  no faces\n", stp->st_name);
	return -1;		/* BAD */
    }

    {
	fastf_t dx, dy, dz;
	fastf_t f;

	VADD2SCALE(stp->st_center, stp->st_max, stp->st_min, 0.5);

	dx = (stp->st_max[X] - stp->st_min[X])/2;
	f = dx;
	dy = (stp->st_max[Y] - stp->st_min[Y])/2;
	if (dy > f) f = dy;
	dz = (stp->st_max[Z] - stp->st_min[Z])/2;
	if (dz > f) f = dz;
	stp->st_aradius = f;
	stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);
    }

    return 0;		/* OK */
}


/**
 * This function is called with pointers to 3 points,
 * and is used to prepare PG faces.
 * ap, bp, cp point to vect_t points.
 *
 * Return -
 * 0 if the 3 points didn't form a plane (e.g., collinear, etc.).
 * # pts (3) if a valid plane resulted.
 */
HIDDEN int
rt_pgface(struct soltab *stp, fastf_t *ap, fastf_t *bp, fastf_t *cp, const struct bn_tol *tol)
{
    struct tri_specific *trip;
    vect_t work;
    fastf_t m1, m2, m3, m4;

    BU_GET(trip, struct tri_specific);
    VMOVE(trip->tri_A, ap);
    VSUB2(trip->tri_BA, bp, ap);
    VSUB2(trip->tri_CA, cp, ap);
    VCROSS(trip->tri_wn, trip->tri_BA, trip->tri_CA);

    /* Check to see if this plane is a line or pnt */
    m1 = MAGNITUDE(trip->tri_BA);
    m2 = MAGNITUDE(trip->tri_CA);
    VSUB2(work, bp, cp);
    m3 = MAGNITUDE(work);
    m4 = MAGNITUDE(trip->tri_wn);
    if (m1 < tol->dist || m2 < tol->dist ||
	m3 < tol->dist || m4 < tol->dist) {
	BU_PUT(trip, struct tri_specific);
	if (RT_G_DEBUG & DEBUG_ARB8)
	    bu_log("pg(%s): degenerate facet\n", stp->st_name);
	return 0;			/* BAD */
    }

    /* wn is a normal of not necessarily unit length.
     * N is an outward pointing unit normal.
     * We depend on the points being given in CCW order here.
     */
    VMOVE(trip->tri_N, trip->tri_wn);
    VUNITIZE(trip->tri_N);

    /* Add this face onto the linked list for this solid */
    trip->tri_forw = (struct tri_specific *)stp->st_specific;
    stp->st_specific = (void *)trip;
    return 3;				/* OK */
}


void
rt_pg_print(const struct soltab *stp)
{
    const struct tri_specific *trip =
	(struct tri_specific *)stp->st_specific;

    if (trip == TRI_NULL) {
	bu_log("pg(%s):  no faces\n", stp->st_name);
	return;
    }
    do {
	VPRINT("A", trip->tri_A);
	VPRINT("B-A", trip->tri_BA);
	VPRINT("C-A", trip->tri_CA);
	VPRINT("BA x CA", trip->tri_wn);
	VPRINT("Normal", trip->tri_N);
	bu_log("\n");

	trip = trip->tri_forw;
    } while (trip);
}


/**
 * Function -
 * Shoot a ray at a polygonal object.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_pg_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct tri_specific *trip =
	(struct tri_specific *)stp->st_specific;
#define MAXHITS 128		/* # surfaces hit, must be even */
    struct hit hits[MAXHITS];
    struct hit *hp;
    size_t nhits;

    nhits = 0;
    hp = &hits[0];

    /* consider each face */
    for (; trip; trip = trip->tri_forw) {
	fastf_t dn;		/* Direction dot Normal */
	fastf_t abs_dn;
	fastf_t k;
	fastf_t alpha, beta;
	vect_t wxb;		/* vertex - ray_start */
	vect_t xp;		/* wxb cross ray_dir */

	/*
	 * Ray Direction dot N.  (N is outward-pointing normal)
	 * wn points inwards, and is not unit length.
	 */
	dn = VDOT(trip->tri_wn, rp->r_dir);

	/*
	 * If ray lies directly along the face, (i.e., dot product
	 * is zero), drop this face.
	 */
	abs_dn = dn >= 0.0 ? dn : (-dn);
	if (abs_dn < SQRT_SMALL_FASTF)
	    continue;
	VSUB2(wxb, trip->tri_A, rp->r_pt);
	VCROSS(xp, wxb, rp->r_dir);

	/* Check for exceeding along the one side */
	alpha = VDOT(trip->tri_CA, xp);
	if (dn < 0.0) alpha = -alpha;
	if (alpha < 0.0 || alpha > abs_dn)
	    continue;

	/* Check for exceeding along the other side */
	beta = VDOT(trip->tri_BA, xp);
	if (dn > 0.0) beta = -beta;
	if (beta < 0.0 || beta > abs_dn)
	    continue;
	if (alpha+beta > abs_dn)
	    continue;
	k = VDOT(wxb, trip->tri_wn) / dn;

	/* For hits other than the first one, might check
	 * to see it this is approx. equal to previous one */

	/* If dn < 0, we should be entering the solid.
	 * However, we just assume in/out sorting later will work.
	 * Really should mark and check this!
	 */
	VJOIN1(hp->hit_point, rp->r_pt, k, rp->r_dir);

	/* HIT is within planar face */
	hp->hit_magic = RT_HIT_MAGIC;
	hp->hit_dist = k;
	VMOVE(hp->hit_normal, trip->tri_N);
	hp->hit_surfno = trip->tri_surfno;
	if (++nhits >= MAXHITS) {
	    bu_log("rt_pg_shot(%s): too many hits (%zu)\n", stp->st_name, nhits);
	    break;
	}
	hp++;
    }
    if (nhits == 0)
	return 0;		/* MISS */

    /* Sort hits, Near to Far */
    rt_hitsort(hits, nhits);

    /* Remove duplicate hits.
       We remove one of a pair of hits when they are
       1) close together, and
       2) both "entry" or both "exit" occurrences.
       Two immediate "entry" or two immediate "exit" hits suggest
       that we hit both of two joined faces, while we want to hit only
       one.  An "entry" followed by an "exit" (or vice versa) suggests
       that we grazed an edge, and thus we should leave both
       in the hit list. */

    {
	size_t i, j;

	for (i=0; i<nhits-1; i++) {
	    fastf_t dist;

	    dist = hits[i].hit_dist - hits[i+1].hit_dist;
	    if (NEAR_ZERO(dist, ap->a_rt_i->rti_tol.dist) &&
		VDOT(hits[i].hit_normal, rp->r_dir) *
		VDOT(hits[i+1].hit_normal, rp->r_dir) > 0)
	    {
		for (j=i; j<nhits-1; j++)
		    hits[j] = hits[j+1];
		nhits--;
		i--;
	    }
	}
    }


    if (nhits == 1)
	nhits = 0;

    if (nhits&1) {
	size_t i;
	static int nerrors = 0;		/* message counter */
	/*
	 * If this condition exists, it is almost certainly due to
	 * the dn==0 check above.  Thus, we will make the last
	 * surface rather thin.
	 * This at least makes the
	 * presence of this solid known.  There may be something
	 * better we can do.
	 */

	if (nerrors++ < 6) {
	    bu_log("rt_pg_shot(%s): WARNING %zu hits:\n", stp->st_name, nhits);
	    bu_log("\tray start = (%g %g %g) ray dir = (%g %g %g)\n",
		   V3ARGS(rp->r_pt), V3ARGS(rp->r_dir));
	    for (i=0; i < nhits; i++) {
		point_t tmp_pt;

		VJOIN1(tmp_pt, rp->r_pt, hits[i].hit_dist, rp->r_dir);
		if (VDOT(rp->r_dir, hits[i].hit_normal) < 0.0)
		    bu_log("\tentrance at dist=%f (%g %g %g)\n", hits[i].hit_dist, V3ARGS(tmp_pt));
		else
		    bu_log("\texit at dist=%f (%g %g %g)\n", hits[i].hit_dist, V3ARGS(tmp_pt));
	    }
	}

	if (nhits > 2) {
	    fastf_t dot1, dot2;
	    size_t j;

	    /* likely an extra hit,
	     * look for consecutive entrances or exits */

	    dot2 = 1.0;
	    i = 0;
	    while (i<nhits) {
		dot1 = dot2;
		dot2 = VDOT(rp->r_dir, hits[i].hit_normal);
		if (dot1 > 0.0 && dot2 > 0.0) {
		    /* two consecutive exits,
		     * manufacture an entrance at same distance
		     * as second exit.
		     */
		    for (j=nhits; j>i; j--)
			hits[j] = hits[j-1];	/* struct copy */

		    VREVERSE(hits[i].hit_normal, hits[i].hit_normal);
		    dot2 = VDOT(rp->r_dir, hits[i].hit_normal);
		    nhits++;
		    bu_log("\t\tadding fictitious entry at %f (%s)\n", hits[i].hit_dist, stp->st_name);
		} else if (dot1 < 0.0 && dot2 < 0.0) {
		    /* two consecutive entrances,
		     * manufacture an exit between them.
		     */

		    for (j=nhits; j>i; j--)
			hits[j] = hits[j-1];	/* struct copy */

		    hits[i] = hits[i-1];	/* struct copy */
		    VREVERSE(hits[i].hit_normal, hits[i-1].hit_normal);
		    dot2 = VDOT(rp->r_dir, hits[i].hit_normal);
		    nhits++;
		    bu_log("\t\tadding fictitious exit at %f (%s)\n", hits[i].hit_dist, stp->st_name);
		}
		i++;
	    }

	} else {
	    hits[nhits] = hits[nhits-1];	/* struct copy */
	    VREVERSE(hits[nhits].hit_normal, hits[nhits-1].hit_normal);
	    bu_log("\t\tadding fictitious hit at %f (%s)\n", hits[nhits].hit_dist, stp->st_name);
	    nhits++;
	}
    }

    if (nhits&1) {
	if (nhits < MAXHITS) {
	    hits[nhits] = hits[nhits-1];	/* struct copy */
	    VREVERSE(hits[nhits].hit_normal, hits[nhits-1].hit_normal);
	    bu_log("\t\tadding fictitious hit at %f (%s)\n", hits[nhits].hit_dist, stp->st_name);
	    nhits++;
	} else
	    nhits--;
    }

    /* nhits is even, build segments */
    {
	struct seg *segp;
	size_t i;
	for (i=0; i < nhits; i += 2) {
	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;
	    segp->seg_in = hits[i];		/* struct copy */
	    segp->seg_out = hits[i+1];	/* struct copy */
	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	}
    }
    return nhits;			/* HIT */
}


void
rt_pg_free(struct soltab *stp)
{
    struct tri_specific *trip =
	(struct tri_specific *)stp->st_specific;

    while (trip != TRI_NULL) {
	struct tri_specific *nexttri = trip->tri_forw;

	BU_PUT(trip, struct tri_specific);
	trip = nexttri;
    }
}


void
rt_pg_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    if (!hitp || !stp || !rp)
	return;
    RT_CK_HIT(hitp);
    RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);

    /* Normals computed in rt_pg_shot, nothing to do here */
}


void
rt_pg_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);
    if (!uvp)
	return;

    /* Do nothing.  Really, should do what ARB does. */
    uvp->uv_u = uvp->uv_v = 0;
    uvp->uv_du = uvp->uv_dv = 0;
}


int
rt_pg_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    size_t i;
    size_t p;	/* current polygon number */
    struct rt_pg_internal *pgp;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    pgp = (struct rt_pg_internal *)ip->idb_ptr;
    RT_PG_CK_MAGIC(pgp);

    for (p = 0; p < pgp->npoly; p++) {
	struct rt_pg_face_internal *pp;

	pp = &pgp->poly[p];
	RT_ADD_VLIST(vhead, &pp->verts[3*(pp->npts-1)],
		     BN_VLIST_LINE_MOVE);
	for (i=0; i < pp->npts; i++) {
	    RT_ADD_VLIST(vhead, &pp->verts[3*i],
			 BN_VLIST_LINE_DRAW);
	}
    }
    return 0;		/* OK */
}


/**
 * Convert to vlist, draw as polygons.
 */
int
rt_pg_plot_poly(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    size_t i;
    size_t p;	/* current polygon number */
    struct rt_pg_internal *pgp;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    pgp = (struct rt_pg_internal *)ip->idb_ptr;
    RT_PG_CK_MAGIC(pgp);

    for (p = 0; p < pgp->npoly; p++) {
	struct rt_pg_face_internal *pp;
	vect_t aa, bb, norm;

	pp = &pgp->poly[p];
	if (pp->npts < 3)
	    continue;
	VSUB2(aa, &pp->verts[3*(0)], &pp->verts[3*(1)]);
	VSUB2(bb, &pp->verts[3*(0)], &pp->verts[3*(2)]);
	VCROSS(norm, aa, bb);
	VUNITIZE(norm);
	RT_ADD_VLIST(vhead, norm, BN_VLIST_POLY_START);

	RT_ADD_VLIST(vhead, &pp->verts[3*(pp->npts-1)], BN_VLIST_POLY_MOVE);
	for (i=0; i < pp->npts-1; i++) {
	    RT_ADD_VLIST(vhead, &pp->verts[3*i], BN_VLIST_POLY_DRAW);
	}
	RT_ADD_VLIST(vhead, &pp->verts[3*(pp->npts-1)], BN_VLIST_POLY_END);
    }
    return 0;		/* OK */
}


void
rt_pg_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp)
	return;
    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);

    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
    cvp->crv_c1 = cvp->crv_c2 = 0;
}


int
rt_pg_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *tol)
{
    size_t i;
    struct shell *s;
    struct vertex **verts;	/* dynamic array of pointers */
    struct vertex ***vertp;/* dynamic array of ptrs to pointers */
    struct faceuse *fu;
    size_t p;	/* current polygon number */
    struct rt_pg_internal *pgp;

    RT_CK_DB_INTERNAL(ip);
    pgp = (struct rt_pg_internal *)ip->idb_ptr;
    RT_PG_CK_MAGIC(pgp);

    *r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    verts = (struct vertex **)bu_malloc(
	pgp->max_npts * sizeof(struct vertex *), "pg_tess verts[]");
    vertp = (struct vertex ***)bu_malloc(
	pgp->max_npts * sizeof(struct vertex **), "pg_tess vertp[]");
    for (i=0; i < pgp->max_npts; i++)
	vertp[i] = &verts[i];

    for (p = 0; p < pgp->npoly; p++) {
	struct rt_pg_face_internal *pp;

	pp = &pgp->poly[p];

	/* Locate these points, if previously mentioned */
	for (i=0; i < pp->npts; i++) {
	    verts[i] = nmg_find_pt_in_shell(s,
					    &pp->verts[3*i], tol);
	}

	/* Construct the face.  Verts should be in CCW order */
	if ((fu = nmg_cmface(s, vertp, pp->npts)) == (struct faceuse *)0) {
	    bu_log("rt_pg_tess() nmg_cmface failed, skipping face %zu\n",
		   p);
	}

	/* Associate vertex geometry, where none existed before */
	for (i=0; i < pp->npts; i++) {
	    if (verts[i]->vg_p) continue;
	    nmg_vertex_gv(verts[i], &pp->verts[3*i]);
	}

	/* Associate face geometry */
	if (nmg_calc_face_g(fu)) {
	    nmg_pr_fu_briefly(fu, "");
	    bu_free((char *)verts, "pg_tess verts[]");
	    bu_free((char *)vertp, "pg_tess vertp[]");
	    return -1;			/* FAIL */
	}
    }

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

    /* Polysolids are often built with incorrect face normals.
     * Don't depend on them here.
     */
    nmg_fix_normals(s, tol);
    bu_free((char *)verts, "pg_tess verts[]");
    bu_free((char *)vertp, "pg_tess vertp[]");

    return 0;		/* OK */
}


/**
 * Read all the polygons in as a complex dynamic structure.
 * The caller is responsible for freeing the dynamic memory.
 * (vid rt_pg_ifree).
 */
int
rt_pg_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_pg_internal *pgp;
    union record *rp;
    size_t i;
    size_t rno;		/* current record number */
    size_t p;		/* current polygon index */

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    if (rp->u_id != ID_P_HEAD) {
	bu_log("rt_pg_import4: defective header record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_POLY;
    ip->idb_meth = &OBJ[ID_POLY];
    BU_ALLOC(ip->idb_ptr, struct rt_pg_internal);

    pgp = (struct rt_pg_internal *)ip->idb_ptr;
    pgp->magic = RT_PG_INTERNAL_MAGIC;

    pgp->npoly = (ep->ext_nbytes - sizeof(union record)) /
	sizeof(union record);
    if (pgp->npoly <= 0) {
	bu_log("rt_pg_import4: polysolid with no polygons!\n");
	return -1;
    }
    if (pgp->npoly)
	pgp->poly = (struct rt_pg_face_internal *)bu_malloc(
	    pgp->npoly * sizeof(struct rt_pg_face_internal), "rt_pg_face_internal");
    pgp->max_npts = 0;

    if (mat == NULL) mat = bn_mat_identity;
    for (p=0; p < pgp->npoly; p++) {
	struct rt_pg_face_internal *pp;

	pp = &pgp->poly[p];
	rno = p+1;
	if (rp[rno].q.q_id != ID_P_DATA) {
	    bu_log("rt_pg_import4: defective data record\n");
	    return -1;
	}
	pp->npts = rp[rno].q.q_count;
	pp->verts = (fastf_t *)bu_malloc(pp->npts * 3 * sizeof(fastf_t), "pg verts[]");
	pp->norms = (fastf_t *)bu_malloc(pp->npts * 3 * sizeof(fastf_t), "pg norms[]");
	for (i=0; i < pp->npts; i++) {
	    point_t pnt;
	    vect_t vec;

	    if (dbip->dbi_version < 0) {
		flip_fastf_float(pnt, rp[rno].q.q_verts[i], 1, 1);
		flip_fastf_float(vec, rp[rno].q.q_norms[i], 1, 1);
	    } else {
		VMOVE(pnt, rp[rno].q.q_verts[i]);
		VMOVE(vec, rp[rno].q.q_norms[i]);
	    }

	    /* Note:  side effect of importing dbfloat_t */
	    MAT4X3PNT(&pp->verts[i*3], mat, pnt);
	    MAT4X3VEC(&pp->norms[i*3], mat, vec);
	}
	if (pp->npts > pgp->max_npts) pgp->max_npts = pp->npts;
    }
    if (pgp->max_npts < 3) {
	bu_log("rt_pg_import4: polysolid with all polygons of less than %zu vertices!\n", pgp->max_npts);
	/* XXX free storage */
	return -1;
    }
    return 0;
}


/**
 * The name will be added by the caller.
 * Generally, only libwdb will set conv2mm != 1.0
 */
int
rt_pg_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_pg_internal *pgp;
    union record *rec;
    size_t i;
    size_t rno;		/* current record number */
    size_t p;		/* current polygon index */

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_POLY) return -1;
    pgp = (struct rt_pg_internal *)ip->idb_ptr;
    RT_PG_CK_MAGIC(pgp);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = (1 + pgp->npoly) * sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "pg external");
    rec = (union record *)ep->ext_buf;

    rec[0].p.p_id = ID_P_HEAD;

    for (p=0; p < pgp->npoly; p++) {
	struct rt_pg_face_internal *pp;

	rno = p+1;
	pp = &pgp->poly[p];
	if (pp->npts < 3 || pp->npts > 5) {
	    bu_log("rt_pg_export4:  unable to support npts=%zu\n",
		   pp->npts);
	    return -1;
	}

	rec[rno].q.q_id = ID_P_DATA;
	rec[rno].q.q_count = pp->npts;
	for (i=0; i < pp->npts; i++) {
	    /* NOTE: type conversion to dbfloat_t */
	    VSCALE(rec[rno].q.q_verts[i],
		   &pp->verts[i*3], local2mm);
	    VMOVE(rec[rno].q.q_norms[i], &pp->norms[i*3]);
	}
    }

    bu_log("DEPRECATED:  The 'poly' primitive is no longer supported.  Use the 'bot' or 'nmg' polygonal mesh instead.\n");
    bu_log("\tTo convert polysolids to BOT primitives, use 'dbupgrade'.\n");

    return 0;
}


int
rt_pg_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (!ep || !mat)
	return -1;
    if (dbip) RT_CK_DBI(dbip);

    bu_log("As of release 6.0 the polysolid is superceded by the BOT primitive.\n");
    bu_log("\tTo convert polysolids to BOT primitives, use 'dbupgrade'.\n");
    /* The rt_pg_to_bot() routine can also be used. */
    return -1;
}


int
rt_pg_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    if (!ep)
	return -1;
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);

    bu_log("DEPRECATED:  The 'poly' primitive is no longer supported.  Use the 'bot' or 'nmg' polygonal mesh instead.\n");
    bu_log("\tTo convert polysolids to BOT primitives, use 'dbupgrade'.\n");
    /* The rt_pg_to_bot() routine can also be used. */
    return -1;
}


/**
 * Make human-readable formatted presentation of this solid.
 * First line describes type of solid.
 * Additional lines are indented one tab, and give parameter values.
 */
int
rt_pg_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    size_t i, j;
    struct rt_pg_internal *pgp = (struct rt_pg_internal *)ip->idb_ptr;
    char buf[256] = {0};

    RT_PG_CK_MAGIC(pgp);
    bu_vls_strcat(str, "polygon solid with no topology (POLY)\n");

    sprintf(buf, "\t%ld polygons (faces)\n",
	    (long int)pgp->npoly);
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tMost complex face has %lu vertices\n", (long unsigned)pgp->max_npts);
    bu_vls_strcat(str, buf);

    if (pgp->npoly) {
	sprintf(buf, "\tFirst vertex (%g, %g, %g)\n",
		INTCLAMP(pgp->poly[0].verts[X] * mm2local),
		INTCLAMP(pgp->poly[0].verts[Y] * mm2local),
		INTCLAMP(pgp->poly[0].verts[Z] * mm2local));
	bu_vls_strcat(str, buf);
    }

    if (!verbose) return 0;

    /* Print out all the vertices of all the faces */
    for (i=0; i < pgp->npoly; i++) {
	fastf_t *v = pgp->poly[i].verts;
	fastf_t *n = pgp->poly[i].norms;

	sprintf(buf, "\tPolygon %lu: (%lu pts)\n", (long unsigned)i, (long unsigned)pgp->poly[i].npts);
	bu_vls_strcat(str, buf);
	for (j=0; j < pgp->poly[i].npts; j++) {
	    sprintf(buf, "\t\tV (%g, %g, %g)\n\t\t N (%g, %g, %g)\n",
		    INTCLAMP(v[X] * mm2local),
		    INTCLAMP(v[Y] * mm2local),
		    INTCLAMP(v[Z] * mm2local),
		    INTCLAMP(n[X] * mm2local),
		    INTCLAMP(n[Y] * mm2local),
		    INTCLAMP(n[Z] * mm2local));
	    bu_vls_strcat(str, buf);
	    v += ELEMENTS_PER_VECT;
	    n += ELEMENTS_PER_VECT;
	}
    }

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_pg_ifree(struct rt_db_internal *ip)
{
    struct rt_pg_internal *pgp;
    size_t i;

    RT_CK_DB_INTERNAL(ip);

    pgp = (struct rt_pg_internal *)ip->idb_ptr;
    RT_PG_CK_MAGIC(pgp);

    /*
     * Free storage for each polygon
     */
    for (i=0; i < pgp->npoly; i++) {
	bu_free((char *)pgp->poly[i].verts, "pg verts[]");
	bu_free((char *)pgp->poly[i].norms, "pg norms[]");
    }
    if (pgp->npoly)
	bu_free((char *)pgp->poly, "pg poly[]");
    pgp->magic = 0;			/* sanity */
    pgp->npoly = 0;
    bu_free((char *)pgp, "pg ifree");
    ip->idb_ptr = ((void *)0);	/* sanity */
}
int
rt_pg_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/**
 * Convert in-memory form of a polysolid (pg) to a bag of triangles (BoT)
 * There is no record in the V5 database for a polysolid.
 *
 * Depends on the "max_npts" parameter having been set.
 *
 * Returns -
 * -1 FAIL
 * 0 OK
 */
int
rt_pg_to_bot(struct rt_db_internal *ip, const struct bn_tol *tol, struct resource *resp)
{
    struct rt_pg_internal *ip_pg;
    struct rt_bot_internal *ip_bot;
    size_t max_pts;
    size_t max_tri;
    size_t p;
    size_t i;

    RT_CK_DB_INTERNAL(ip);
    BN_CK_TOL(tol);
    RT_CK_RESOURCE(resp);

    if (ip->idb_type != ID_POLY) {
	bu_log("ERROR: rt_pt_to_bot() called with a non-polysolid!!!\n");
	return -1;
    }
    ip_pg = (struct rt_pg_internal *)ip->idb_ptr;

    RT_PG_CK_MAGIC(ip_pg);

    BU_ALLOC(ip_bot, struct rt_bot_internal);
    ip_bot->magic = RT_BOT_INTERNAL_MAGIC;
    ip_bot->mode = RT_BOT_SOLID;
    ip_bot->orientation = RT_BOT_CCW;
    ip_bot->bot_flags = 0;

    /* maximum possible vertices */
    max_pts = ip_pg->npoly * ip_pg->max_npts;
    BU_ASSERT_SIZE_T(max_pts, >, 0);

    /* maximum possible triangular faces */
    max_tri = ip_pg->npoly * 3;
    BU_ASSERT_SIZE_T(max_tri, >, 0);

    ip_bot->num_vertices = 0;
    ip_bot->num_faces = 0;
    ip_bot->thickness = (fastf_t *)NULL;
    ip_bot->face_mode = (struct bu_bitv *)NULL;

    ip_bot->vertices = (fastf_t *)bu_calloc(max_pts * 3, sizeof(fastf_t), "BOT vertices");
    ip_bot->faces = (int *)bu_calloc(max_tri * 3, sizeof(int), "BOT faces");

    for (p=0; p<ip_pg->npoly; p++) {
	vect_t work[3], tmp;
	struct tri_specific trip;
	fastf_t m1, m2, m3, m4;
	size_t v0=0, v2=0;
	int first;

	first = 1;
	VMOVE(work[0], &ip_pg->poly[p].verts[0*3]);
	VMOVE(work[1], &ip_pg->poly[p].verts[1*3]);

	for (i=2; i < ip_pg->poly[p].npts; i++) {
	    VMOVE(work[2], &ip_pg->poly[p].verts[i*3]);

	    VSUB2(trip.tri_BA, work[1], work[0]);
	    VSUB2(trip.tri_CA, work[2], work[0]);
	    VCROSS(trip.tri_wn, trip.tri_BA, trip.tri_CA);

	    /* Check to see if this plane is a line or pnt */
	    m1 = MAGNITUDE(trip.tri_BA);
	    m2 = MAGNITUDE(trip.tri_CA);
	    VSUB2(tmp, work[1], work[2]);
	    m3 = MAGNITUDE(tmp);
	    m4 = MAGNITUDE(trip.tri_wn);
	    if (m1 >= tol->dist && m2 >= tol->dist &&
		m3 >= tol->dist && m4 >= tol->dist) {

		/* add this triangle to the BOT */
		if (first) {
		    ip_bot->faces[ip_bot->num_faces * 3] = ip_bot->num_vertices;
		    VMOVE(&ip_bot->vertices[ip_bot->num_vertices * 3], work[0]);
		    v0 = ip_bot->num_vertices;
		    ip_bot->num_vertices++;

		    ip_bot->faces[ip_bot->num_faces * 3 + 1] = ip_bot->num_vertices;
		    VMOVE(&ip_bot->vertices[ip_bot->num_vertices * 3], work[1]);
		    ip_bot->num_vertices++;
		    first = 0;
		} else {
		    ip_bot->faces[ip_bot->num_faces * 3] = v0;
		    ip_bot->faces[ip_bot->num_faces * 3 + 1] = v2;
		}
		VMOVE(&ip_bot->vertices[ip_bot->num_vertices * 3], work[2]);
		ip_bot->faces[ip_bot->num_faces * 3 + 2] = ip_bot->num_vertices;
		v2 = ip_bot->num_vertices;
		ip_bot->num_vertices++;

		ip_bot->num_faces++;
	    }

	    /* Chop off a triangle, and continue */
	    VMOVE(work[1], work[2]);
	}
    }

    rt_bot_vertex_fuse(ip_bot, tol);
    rt_bot_face_fuse(ip_bot);

    rt_db_free_internal(ip);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BOT;
    ip->idb_meth = &OBJ[ID_BOT];
    ip->idb_ptr = ip_bot;

    return 0;
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
