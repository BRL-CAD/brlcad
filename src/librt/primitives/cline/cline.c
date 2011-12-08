/*                         C L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2011 United States Government as represented by
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
/** @file primitives/cline/cline.c
 *
 * Intersect a ray with a FASTGEN4 CLINE element.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "tcl.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"


/* ray tracing form of solid, including precomputed terms */
struct cline_specific {
    point_t V;
    vect_t height;
    fastf_t radius;
    fastf_t thickness;
    vect_t h;	/* unitized height */
};


#define RT_CLINE_O(m) bu_offsetof(struct rt_cline_internal, m)

const struct bu_structparse rt_cline_parse[] = {
    { "%f", 3, "V", RT_CLINE_O(v),  BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "H", RT_CLINE_O(h),  BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "r", RT_CLINE_O(radius), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "t", RT_CLINE_O(thickness), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

/**
 * R T _ C L I N E _ B B O X
 *
 * Calculate bounding RPP for cline
 */
int
rt_cline_bbox(struct rt_db_internal *ip, point_t *min, point_t *max) {
    struct rt_cline_internal *cline_ip;
    vect_t rad, work;
    point_t top;
    fastf_t max_tr;
    
    RT_CK_DB_INTERNAL(ip);
    cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
    RT_CLINE_CK_MAGIC(cline_ip);

    if (rt_cline_radius > 0.0)
	max_tr = rt_cline_radius;
    else
	max_tr = 0.0;

    VSETALL((*min), MAX_FASTF);
    VSETALL((*max), -MAX_FASTF);

    VSETALL(rad, cline_ip->radius + max_tr);
    VADD2(work, cline_ip->v, rad);
    VMINMAX((*min), (*max), work);
    VSUB2(work, cline_ip->v, rad);
    VMINMAX((*min), (*max), work);
    VADD2(top, cline_ip->v, cline_ip->h);
    VADD2(work, top, rad);
    VMINMAX((*min), (*max), work);
    VSUB2(work, top, rad);
    VMINMAX((*min), (*max), work);
    return 0;
}

/**
 * R T _ C L I N E _ P R E P
 *
 * Given a pointer to a GED database record, determine if this is a
 * valid cline solid, and if so, precompute various terms of the
 * formula.
 *
 * Returns -
 * 0 cline is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct cline_specific is created, and its address is stored
 * in stp->st_specific for use by rt_cline_shot().
 */
int
rt_cline_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_cline_internal *cline_ip;
    register struct cline_specific *cline;
    fastf_t tmp;
    fastf_t max_tr;

    RT_CK_DB_INTERNAL(ip);
    cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
    RT_CLINE_CK_MAGIC(cline_ip);
    if (rtip) RT_CK_RTI(rtip);

    BU_GET(cline, struct cline_specific);
    cline->thickness = cline_ip->thickness;
    cline->radius = cline_ip->radius;
    VMOVE(cline->V, cline_ip->v);
    VMOVE(cline->height, cline_ip->h);
    VMOVE(cline->h, cline_ip->h);
    VUNITIZE(cline->h);
    stp->st_specific = (genptr_t)cline;

    if (rt_cline_radius > 0.0)
	max_tr = rt_cline_radius;
    else
	max_tr = 0.0;
    tmp = MAGNITUDE(cline_ip->h) * 0.5;
    stp->st_aradius = sqrt(tmp*tmp + cline_ip->radius*cline_ip->radius);
    stp->st_bradius = stp->st_aradius + max_tr;

    if (rt_cline_bbox(ip, &(stp->st_min), &(stp->st_max))) return 1;

    return 0;
}


/**
 * R T _ C L I N E _ P R I N T
 */
void
rt_cline_print(register const struct soltab *stp)
{
    register const struct cline_specific *cline =
	(struct cline_specific *)stp->st_specific;

    VPRINT("V", cline->V);
    VPRINT("Height", cline->height);
    VPRINT("Unit Height", cline->h);
    bu_log("Radius: %g\n", cline->radius);
    if (cline->thickness > 0.0)
	bu_log("Plate Mode Thickness: %g\n", cline->thickness);
    else
	bu_log("Volume mode\n");
}


/**
 * R T _ C L I N E _ S H O T
 *
 * Intersect a ray with a cline mode solid.  If an intersection
 * occurs, at least one struct seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_cline_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    register struct cline_specific *cline =
	(struct cline_specific *)stp->st_specific;
    struct seg ref_seghead;
    register struct seg *segp;
    fastf_t reff;
    fastf_t dist[3];
    fastf_t cosa, sina;
    fastf_t half_los;
    point_t pt1, pt2;
    vect_t diff;
    fastf_t tmp;
    fastf_t distmin, distmax;
    fastf_t add_radius;

    if (ap) RT_CK_APPLICATION(ap);

    BU_LIST_INIT(&ref_seghead.l);

    /* This is a CLINE FASTGEN element */
    if (rt_cline_radius > 0.0) {
	add_radius = rt_cline_radius;
	reff = cline->radius + add_radius;
    } else {
	add_radius = 0.0;
	reff = cline->radius;
    }

    cosa = VDOT(rp->r_dir, cline->h);

    if (cosa > 0.0)
	tmp = cosa - 1.0;
    else
	tmp = cosa + 1.0;

    (void)bn_distsq_line3_line3(dist, cline->V, cline->height,
				rp->r_pt, rp->r_dir, pt1, pt2);

    if (NEAR_ZERO(tmp, RT_DOT_TOL)) {
	/* ray is parallel to CLINE */
#if 1
	/* FASTGEN developers claim they report hits on volume mode
	 * when ray is parallel to CLINE axis, but their code drops
	 * this case from consideration before their intersection code
	 * is even called (see SUBROUTINE BULK)
	 */
	return 0;
#else

	if (cline->thickness > 0.0)
	    return 0;	/* No end-on hits for plate mode cline */

	if (dist[2] > reff*reff)
	    return 0;	/* missed */

	VJOIN2(diff, cline->V, 1.0, cline->height, -1.0, rp->r_pt);
	dist[0] = VDOT(diff, rp->r_dir);
	if (dist[1] < dist[0]) {
	    dist[2] = dist[0];
	    dist[0] = dist[1];
	    dist[1] = dist[2];
	}

	/* vloume mode */

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in.hit_dist = dist[0];
	segp->seg_in.hit_surfno = 1;
	if (cosa > 0.0) {
	    VREVERSE(segp->seg_in.hit_normal, cline->h);
	} else {
	    VMOVE(segp->seg_in.hit_normal, cline->h);
	}

	segp->seg_out.hit_dist = dist[1];
	segp->seg_out.hit_surfno = -1;
	if (cosa < 0.0) {
	    VREVERSE(segp->seg_out.hit_normal, cline->h);
	} else {
	    VMOVE(segp->seg_out.hit_normal, cline->h);
	}
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
	return 1;
#endif
    }

    if (dist[2] > reff*reff)
	return 0;	/* missed */


    /* Exactly ==0 and ==1 are hits, not misses */
    if (dist[0] < 0.0 || dist[0] > 1.0)
	return 0;	/* missed */

    sina = sqrt(1.0 - cosa*cosa);
    tmp = sqrt(dist[2]) - add_radius;
    if (dist[2] > add_radius * add_radius)
	half_los = sqrt(cline->radius*cline->radius - tmp*tmp) / sina;
    else
	half_los = cline->radius / sina;

    VSUB2(diff, cline->V, rp->r_pt);
    distmin = VDOT(rp->r_dir, diff);
    VADD2(diff, cline->V, cline->height);
    VSUB2(diff, diff, rp->r_pt);
    distmax = VDOT(rp->r_dir, diff);

    if (distmin > distmax) {
	tmp = distmin;
	distmin = distmax;
	distmax = tmp;
    }

    distmin -= cline->radius;
    distmax += cline->radius;

    if (cline->thickness <= 0.0) {
	/* volume mode */

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in.hit_surfno = 2;
	segp->seg_in.hit_dist = dist[1] - half_los;
	if (segp->seg_in.hit_dist < distmin)
	    segp->seg_in.hit_dist = distmin;
	VMOVE(segp->seg_in.hit_vpriv, cline->h);

	segp->seg_out.hit_surfno = -2;
	segp->seg_out.hit_dist = dist[1] + half_los;
	if (segp->seg_out.hit_dist > distmax)
	    segp->seg_out.hit_dist = distmax;
	VMOVE(segp->seg_out.hit_vpriv, cline->h);
	BU_LIST_INSERT(&(seghead->l), &(segp->l));

	return 1;
    } else {
	/* plate mode */

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in.hit_surfno = 2;
	segp->seg_in.hit_dist = dist[1] - half_los;
	if (segp->seg_in.hit_dist < distmin)
	    segp->seg_in.hit_dist = distmin;
	VMOVE(segp->seg_in.hit_vpriv, cline->h);

	segp->seg_out.hit_surfno = -2;
	segp->seg_out.hit_dist = segp->seg_in.hit_dist + cline->thickness;
	VMOVE(segp->seg_out.hit_vpriv, cline->h);
	BU_LIST_INSERT(&(seghead->l), &(segp->l));

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in.hit_surfno = 2;
	segp->seg_in.hit_dist = dist[1] + half_los;
	if (segp->seg_in.hit_dist > distmax)
	    segp->seg_in.hit_dist = distmax;
	segp->seg_in.hit_dist -=  cline->thickness;
	VMOVE(segp->seg_in.hit_vpriv, cline->h);

	segp->seg_out.hit_surfno = -2;
	segp->seg_out.hit_dist = segp->seg_in.hit_dist + cline->thickness;
	VMOVE(segp->seg_out.hit_vpriv, cline->h);
	BU_LIST_INSERT(&(seghead->l), &(segp->l));

	return 2;
    }
}


/**
 * R T _ C L I N E _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_cline_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    vect_t tmp;
    fastf_t dot;

    if (hitp) RT_CK_HIT(hitp);

    if (hitp->hit_surfno == 1 || hitp->hit_surfno == -1)
	return;

    /* only need to do some calculations for surfno 2 or -2 */

    /* this is wrong, but agrees with FASTGEN */
    VCROSS(tmp, rp->r_dir, hitp->hit_vpriv);
    VCROSS(hitp->hit_normal, tmp, hitp->hit_vpriv);
    VUNITIZE(hitp->hit_normal);
    dot = VDOT(hitp->hit_normal, rp->r_dir);
    if (dot < 0.0 && hitp->hit_surfno < 0) {
	VREVERSE(hitp->hit_normal, hitp->hit_normal);
    } else if (dot > 0.0 && hitp->hit_surfno > 0) {
	VREVERSE(hitp->hit_normal, hitp->hit_normal);
    }

    if (MAGNITUDE(hitp->hit_normal) < 0.9) {
	bu_log("BAD normal for solid %s for ray -p %g %g %g -d %g %g %g\n",
	       stp->st_name, V3ARGS(rp->r_pt), V3ARGS(rp->r_dir));
	bu_bomb("BAD normal\n");
    }
    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
}


/**
 * R T _ C L I N E _ C U R V E
 *
 * Return the curvature of the cline.
 */
void
rt_cline_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);

    /* for now, don't do curvature */
    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


/**
 * R T _ C L I N E_ U V
 *
 * For a hit on the surface of an cline, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 */
void
rt_cline_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);

    uvp->uv_u = 0.0;
    uvp->uv_v = 0.0;
    uvp->uv_du = 0.0;
    uvp->uv_dv = 0.0;
}


/**
 * R T _ C L I N E _ F R E E
 */
void
rt_cline_free(register struct soltab *stp)
{
    register struct cline_specific *cline =
	(struct cline_specific *)stp->st_specific;

    if (stp) RT_CK_SOLTAB(stp);

    bu_free((char *)cline, "cline_specific");
}


/**
 * R T _ C L I N E _ C L A S S
 */
int
rt_cline_class(const struct soltab *stp, const fastf_t *min, const fastf_t *max, const struct bn_tol *tol)
{
    if (stp) RT_CK_SOLTAB(stp);
    if (!min) return 0;
    if (!max) return 0;
    if (tol) BN_CK_TOL(tol);

    return 0;
}


/**
 * R T _ C L I N E _ P L O T
 */
int
rt_cline_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    struct rt_cline_internal *cline_ip;
    fastf_t top[16*3];
    fastf_t bottom[16*3];
    point_t top_pt;
    vect_t unit_a, unit_b;
    vect_t a, b;
    fastf_t inner_radius;
    int i;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
    RT_CLINE_CK_MAGIC(cline_ip);

    VADD2(top_pt, cline_ip->v, cline_ip->h);
    bn_vec_ortho(unit_a, cline_ip->h);
    VCROSS(unit_b, unit_a, cline_ip->h);
    VUNITIZE(unit_b);
    VSCALE(a, unit_a, cline_ip->radius);
    VSCALE(b, unit_b, cline_ip->radius);

    rt_ell_16pts(bottom, cline_ip->v, a, b);
    rt_ell_16pts(top, top_pt, a, b);

    /* Draw the top */
    RT_ADD_VLIST(vhead, &top[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) {
	RT_ADD_VLIST(vhead, &top[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    }

    /* Draw the bottom */
    RT_ADD_VLIST(vhead, &bottom[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) {
	RT_ADD_VLIST(vhead, &bottom[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    }

    /* Draw connections */
    for (i=0; i<16; i += 4) {
	RT_ADD_VLIST(vhead, &top[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, &bottom[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    }

    if (cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius) {
	/* draw inner cylinder */

	inner_radius = cline_ip->radius - cline_ip->thickness;

	VSCALE(a, unit_a, inner_radius);
	VSCALE(b, unit_b, inner_radius);

	rt_ell_16pts(bottom, cline_ip->v, a, b);
	rt_ell_16pts(top, top_pt, a, b);

	/* Draw the top */
	RT_ADD_VLIST(vhead, &top[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
	for (i=0; i<16; i++) {
	    RT_ADD_VLIST(vhead, &top[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
	}

	/* Draw the bottom */
	RT_ADD_VLIST(vhead, &bottom[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
	for (i=0; i<16; i++) {
	    RT_ADD_VLIST(vhead, &bottom[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
	}

	/* Draw connections */
	for (i=0; i<16; i += 4) {
	    RT_ADD_VLIST(vhead, &top[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, &bottom[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
	}

    }

    return 0;
}


struct cline_vert {
    point_t pt;
    struct vertex *v;
};


/**
 * R T _ C L I N E _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_cline_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    struct shell *s;
    struct rt_cline_internal *cline_ip;
    fastf_t ang_tol, abs_tol, norm_tol, rel_tol;
    int nsegs, seg_no, i;
    struct cline_vert *base_outer, *base_inner, *top_outer, *top_inner;
    struct cline_vert base_center, top_center;
    vect_t v1, v2;
    point_t top;
    struct bu_ptbl faces;

    RT_CK_DB_INTERNAL(ip);
    cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
    RT_CLINE_CK_MAGIC(cline_ip);

    *r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    ang_tol = bn_halfpi;
    abs_tol = bn_halfpi;
    rel_tol = bn_halfpi;
    norm_tol = bn_halfpi;

    if (ttol->abs <= 0.0 && ttol->rel <= 0.0 && ttol->norm <= 0.0) {
	/* no tolerances specified, use 10% relative tolerance */
	ang_tol = 2.0 * acos(0.9);
    } else {
	if (ttol->abs > 0.0 && ttol->abs < cline_ip->radius)
	    abs_tol = 2.0 * acos(1.0 - ttol->abs / cline_ip->radius);
	if (ttol->rel > 0.0 && ttol->rel < 1.0)
	    rel_tol = 2.0 * acos(1.0 - ttol->rel);
	if (ttol->norm > 0.0)
	    norm_tol = 2.0 * ttol->norm;
    }

    if (abs_tol < ang_tol)
	ang_tol = abs_tol;
    if (rel_tol < ang_tol)
	ang_tol = rel_tol;
    if (norm_tol < ang_tol)
	ang_tol = norm_tol;

    /* get number of segments per quadrant */
    nsegs = (int)(bn_halfpi / ang_tol + 0.9999);
    if (nsegs < 2)
	nsegs = 2;

    ang_tol = bn_halfpi / nsegs;

    /* and for complete circle */
    nsegs *= 4;

    /* allocate memory for arrays of vertices */
    base_outer = (struct cline_vert *)bu_calloc(nsegs, sizeof(struct cline_vert), "base outer vertices");
    top_outer = (struct cline_vert *)bu_calloc(nsegs, sizeof(struct cline_vert), "top outer vertices");

    if (cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius) {
	base_inner = (struct cline_vert *)bu_calloc(nsegs, sizeof(struct cline_vert), "base inner vertices");
	top_inner = (struct cline_vert *)bu_calloc(nsegs, sizeof(struct cline_vert), "top inner vertices");
    } else {
	base_inner = NULL;
	top_inner = NULL;
    }

    /* calculate geometry for each vertex */
    bn_vec_ortho(v1, cline_ip->h);
    VCROSS(v2, cline_ip->h, v1);
    VUNITIZE(v2);
    VADD2(top, cline_ip->v, cline_ip->h);
    for (seg_no = 0; seg_no < nsegs; seg_no++) {
	fastf_t a, b, c, d, angle;

	angle = ang_tol * seg_no;

	a = cos(angle);
	b = sin(angle);

	if (cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius) {
	    c = a * (cline_ip->radius - cline_ip->thickness);
	    d = b * (cline_ip->radius - cline_ip->thickness);
	} else {
	    c = d = 0;
	}

	a *= cline_ip->radius;
	b *= cline_ip->radius;

	VJOIN2(base_outer[seg_no].pt, cline_ip->v, a, v1, b, v2);
	VADD2(top_outer[seg_no].pt, base_outer[seg_no].pt, cline_ip->h);

	if (cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius) {
	    VJOIN2(base_inner[seg_no].pt, cline_ip->v, c, v1, d, v2);
	    VADD2(top_inner[seg_no].pt, base_inner[seg_no].pt, cline_ip->h);
	}
    }

    bu_ptbl_init(&faces, 64, "faces");
    /* build outer faces */
    for (seg_no=0; seg_no<nsegs; seg_no++) {
	int next_seg;
	struct vertex **verts[3];
	struct faceuse *fu;

	next_seg = seg_no + 1;
	if (next_seg == nsegs)
	    next_seg = 0;

	verts[2] = &top_outer[seg_no].v;
	verts[1] = &top_outer[next_seg].v;
	verts[0] = &base_outer[seg_no].v;

	fu = nmg_cmface(s, verts, 3);
	bu_ptbl_ins(&faces, (long *)fu);

	verts[2] = &base_outer[seg_no].v;
	verts[1] = &top_outer[next_seg].v;
	verts[0] = &base_outer[next_seg].v;

	fu = nmg_cmface(s, verts, 3);
	bu_ptbl_ins(&faces, (long *)fu);
    }

    /* build inner faces */
    if (cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius) {
	for (seg_no=0; seg_no<nsegs; seg_no++) {
	    int next_seg;
	    struct vertex **verts[3];
	    struct faceuse *fu;

	    next_seg = seg_no + 1;
	    if (next_seg == nsegs)
		next_seg = 0;

	    verts[0] = &top_inner[seg_no].v;
	    verts[1] = &top_inner[next_seg].v;
	    verts[2] = &base_inner[seg_no].v;

	    fu = nmg_cmface(s, verts, 3);
	    bu_ptbl_ins(&faces, (long *)fu);

	    verts[0] = &base_inner[seg_no].v;
	    verts[1] = &top_inner[next_seg].v;
	    verts[2] = &base_inner[next_seg].v;

	    fu = nmg_cmface(s, verts, 3);
	    bu_ptbl_ins(&faces, (long *)fu);
	}
    }

    /* build top faces */
    top_center.v = (struct vertex *)NULL;
    VMOVE(top_center.pt, top);
    for (seg_no=0; seg_no<nsegs; seg_no++) {
	int next_seg;
	struct vertex **verts[3];
	struct faceuse *fu;

	next_seg = seg_no + 1;
	if (next_seg == nsegs)
	    next_seg = 0;

	if (cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius) {
	    verts[2] = &top_outer[seg_no].v;
	    verts[1] = &top_inner[seg_no].v;
	    verts[0] = &top_inner[next_seg].v;
	    fu = nmg_cmface(s, verts, 3);
	    bu_ptbl_ins(&faces, (long *)fu);

	    verts[2] = &top_inner[next_seg].v;
	    verts[1] = &top_outer[next_seg].v;
	    verts[0] = &top_outer[seg_no].v;
	    fu = nmg_cmface(s, verts, 3);
	    bu_ptbl_ins(&faces, (long *)fu);
	} else {
	    verts[2] = &top_outer[seg_no].v;
	    verts[1] = &top_center.v;
	    verts[0] = &top_outer[next_seg].v;
	    fu = nmg_cmface(s, verts, 3);
	    bu_ptbl_ins(&faces, (long *)fu);
	}
    }

    /* build base faces */
    base_center.v = (struct vertex *)NULL;
    VMOVE(base_center.pt, cline_ip->v);
    for (seg_no=0; seg_no<nsegs; seg_no++) {
	int next_seg;
	struct vertex **verts[3];
	struct faceuse *fu;

	next_seg = seg_no + 1;
	if (next_seg == nsegs)
	    next_seg = 0;

	if (cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius) {
	    verts[0] = &base_outer[seg_no].v;
	    verts[1] = &base_inner[seg_no].v;
	    verts[2] = &base_inner[next_seg].v;
	    fu = nmg_cmface(s, verts, 3);
	    bu_ptbl_ins(&faces, (long *)fu);

	    verts[0] = &base_inner[next_seg].v;
	    verts[1] = &base_outer[next_seg].v;
	    verts[2] = &base_outer[seg_no].v;
	    fu = nmg_cmface(s, verts, 3);
	    bu_ptbl_ins(&faces, (long *)fu);
	} else {
	    verts[0] = &base_outer[seg_no].v;
	    verts[1] = &base_center.v;
	    verts[2] = &base_outer[next_seg].v;
	    fu = nmg_cmface(s, verts, 3);
	    bu_ptbl_ins(&faces, (long *)fu);
	}
    }

    /* assign vertex geometry */
    if (top_center.v)
	nmg_vertex_gv(top_center.v, top_center.pt);
    if (base_center.v)
	nmg_vertex_gv(base_center.v, base_center.pt);

    for (seg_no=0; seg_no<nsegs; seg_no++) {
	nmg_vertex_gv(top_outer[seg_no].v, top_outer[seg_no].pt);
	nmg_vertex_gv(base_outer[seg_no].v, base_outer[seg_no].pt);
    }

    if (cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius) {
	for (seg_no=0; seg_no<nsegs; seg_no++) {
	    nmg_vertex_gv(top_inner[seg_no].v, top_inner[seg_no].pt);
	    nmg_vertex_gv(base_inner[seg_no].v, base_inner[seg_no].pt);
	}
    }

    bu_free((char *)base_outer, "base outer vertices");
    bu_free((char *)top_outer, "top outer vertices");
    if (cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius) {
	bu_free((char *)base_inner, "base inner vertices");
	bu_free((char *)top_inner, "top inner vertices");
    }

    /* Associate face plane equations */
    for (i=0; i<BU_PTBL_END(&faces); i++) {
	struct faceuse *fu;

	fu = (struct faceuse *)BU_PTBL_GET(&faces, i);
	NMG_CK_FACEUSE(fu);

	if (nmg_calc_face_g(fu)) {
	    bu_log("rt_tess_cline: failed to calculate plane equation\n");
	    nmg_pr_fu_briefly(fu, "");
	    return -1;
	}
    }

    nmg_region_a(*r, tol);
    bu_ptbl_free(&faces);

    return 0;
}


/**
 * R T _ C L I N E _ I M P O R T
 *
 * Import an cline from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_cline_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_cline_internal *cline_ip;
    union record *rp;
    point_t work;

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */

    if (rp->u_id != DBID_CLINE) {
	bu_log("rt_cline_import4: defective record\n");
	return -1;
    }

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_CLINE;
    ip->idb_meth = &rt_functab[ID_CLINE];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_cline_internal), "rt_cline_internal");
    cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
    cline_ip->magic = RT_CLINE_INTERNAL_MAGIC;
    if (mat == NULL) mat = bn_mat_identity;

    ntohd((unsigned char *)(&cline_ip->thickness), rp->cli.cli_thick, 1);
    cline_ip->thickness /= mat[15];
    ntohd((unsigned char *)(&cline_ip->radius), rp->cli.cli_radius, 1);
    cline_ip->radius /= mat[15];
    ntohd((unsigned char *)(&work), rp->cli.cli_V, 3);
    MAT4X3PNT(cline_ip->v, mat, work);
    ntohd((unsigned char *)(&work), rp->cli.cli_h, 3);
    MAT4X3VEC(cline_ip->h, mat, work);

    return 0;			/* OK */
}


/**
 * R T _ C L I N E _ E X P O R T
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_cline_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_cline_internal *cline_ip;
    union record *rec;
    fastf_t tmp;
    point_t work;

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_CLINE) return -1;
    cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
    RT_CLINE_CK_MAGIC(cline_ip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "cline external");
    rec = (union record *)ep->ext_buf;

    if (dbip) RT_CK_DBI(dbip);

    rec->s.s_id = ID_SOLID;
    rec->cli.cli_id = DBID_CLINE;	/* GED primitive type from db.h */

    tmp = cline_ip->thickness * local2mm;
    htond(rec->cli.cli_thick, (unsigned char *)(&tmp), 1);
    tmp = cline_ip->radius * local2mm;
    htond(rec->cli.cli_radius, (unsigned char *)(&tmp), 1);
    VSCALE(work, cline_ip->v, local2mm);
    htond(rec->cli.cli_V, (unsigned char *)work, 3);
    VSCALE(work, cline_ip->h, local2mm);
    htond(rec->cli.cli_h, (unsigned char *)work, 3);

    return 0;
}


/**
 * R T _ C L I N E _ I M P O R T 5
 *
 * Import an cline from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_cline_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_cline_internal *cline_ip;
    fastf_t vec[8];

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    BU_ASSERT_LONG(ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 8);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_CLINE;
    ip->idb_meth = &rt_functab[ID_CLINE];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_cline_internal), "rt_cline_internal");

    cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
    cline_ip->magic = RT_CLINE_INTERNAL_MAGIC;

    /* Convert from database (network) to internal (host) format */
    ntohd((unsigned char *)vec, ep->ext_buf, 8);

    if (mat == NULL) mat = bn_mat_identity;
    cline_ip->thickness = vec[0] / mat[15];
    cline_ip->radius = vec[1] / mat[15];
    MAT4X3PNT(cline_ip->v, mat, &vec[2]);
    MAT4X3VEC(cline_ip->h, mat, &vec[5]);

    return 0;			/* OK */
}


/**
 * R T _ C L I N E _ E X P O R T 5
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_cline_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_cline_internal *cline_ip;
    fastf_t vec[8];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_CLINE) return -1;
    cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
    RT_CLINE_CK_MAGIC(cline_ip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 8;
    ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "cline external");

    vec[0] = cline_ip->thickness * local2mm;
    vec[1] = cline_ip->radius * local2mm;
    VSCALE(&vec[2], cline_ip->v, local2mm);
    VSCALE(&vec[5], cline_ip->h, local2mm);

    /* Convert from internal (host) to database (network) format */
    htond(ep->ext_buf, (unsigned char *)vec, 8);

    return 0;
}


/**
 * R T _ C L I N E _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_cline_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    register struct rt_cline_internal *cline_ip =
	(struct rt_cline_internal *)ip->idb_ptr;
    char buf[256];
    point_t local_v;
    vect_t local_h;

    RT_CLINE_CK_MAGIC(cline_ip);
    bu_vls_strcat(str, "cline solid (CLINE)\n");

    if (!verbose)
	return 0;

    VSCALE(local_v, cline_ip->v, mm2local);
    VSCALE(local_h, cline_ip->h, mm2local);

    if (cline_ip->thickness > 0.0) {
	sprintf(buf, "\tV (%g %g %g)\n\tH (%g %g %g)\n\tradius %g\n\tplate mode thickness %g",
		V3INTCLAMPARGS(local_v), V3INTCLAMPARGS(local_h), INTCLAMP(cline_ip->radius*mm2local), INTCLAMP(cline_ip->thickness*mm2local));
    } else {
	sprintf(buf, "\tV (%g %g %g)\n\tH (%g %g %g)\n\tradius %g\n\tVolume mode\n",
		V3INTCLAMPARGS(local_v), V3INTCLAMPARGS(local_h), INTCLAMP(cline_ip->radius*mm2local));
    }
    bu_vls_strcat(str, buf);

    return 0;
}


/**
 * R T _ C L I N E _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_cline_ifree(struct rt_db_internal *ip)
{
    register struct rt_cline_internal *cline_ip;

    RT_CK_DB_INTERNAL(ip);

    cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
    RT_CLINE_CK_MAGIC(cline_ip);
    cline_ip->magic = 0;			/* sanity */

    bu_free((char *)cline_ip, "cline ifree");
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


int
rt_cline_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    register struct rt_cline_internal *cli =
	(struct rt_cline_internal *)intern->idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "cline");
	bu_vls_printf(logstr, " V {%.25G %.25G %.25G}", V3ARGS(cli->v));
	bu_vls_printf(logstr, " H {%.25G %.25G %.25G}", V3ARGS(cli->h));
	bu_vls_printf(logstr, " R %.25G T %.25G", cli->radius, cli->thickness);
    } else if (*attr == 'V')
	bu_vls_printf(logstr, "%.25G %.25G %.25G", V3ARGS(cli->v));
    else if (*attr == 'H')
	bu_vls_printf(logstr, "%.25G %.25G %.25G", V3ARGS(cli->h));
    else if (*attr == 'R')
	bu_vls_printf(logstr, "%.25G", cli->radius);
    else if (*attr == 'T')
	bu_vls_printf(logstr, "%.25G", cli->thickness);
    else {
	bu_vls_strcat(logstr, "ERROR: unrecognized attribute, must be V, H, R, or T!!!");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
rt_cline_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)intern->idb_ptr;
    fastf_t *new;

    RT_CK_DB_INTERNAL(intern);
    RT_CLINE_CK_MAGIC(cli);

    while (argc >= 2) {
	int array_len=3;

	if (*argv[0] == 'V') {
	    new = cli->v;
	    if (tcl_list_to_fastf_array(brlcad_interp, argv[1], &new, &array_len) !=
		array_len) {
		bu_vls_printf(logstr, "ERROR: Incorrect number of coordinates for vector\n");
		return BRLCAD_ERROR;
	    }
	} else if (*argv[0] == 'H') {
	    new = cli->h;
	    if (tcl_list_to_fastf_array(brlcad_interp, argv[1], &new, &array_len) !=
		array_len) {
		bu_vls_printf(logstr, "ERROR: Incorrect number of coordinates for point\n");
		return BRLCAD_ERROR;
	    }
	} else if (*argv[0] == 'R')
	    cli->radius = atof(argv[1]);
	else if (*argv[0] == 'T')
	    cli->thickness = atof(argv[1]);

	argc -= 2;
	argv += 2;
    }

    return BRLCAD_OK;
}


int
rt_cline_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr,
		  "V {%%f %%f %%f} H {%%f %%f %%f} R %%f T %%f");

    return BRLCAD_OK;

}


/**
 * R T _ C L I N E _ P A R A M S
 *
 */
int
rt_cline_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
