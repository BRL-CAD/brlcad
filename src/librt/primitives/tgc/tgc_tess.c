/*                        T G C _ T E S S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
/** @file primitives/tgc/tgc_tess.c
 *
 * Truncated General Cone Tessellation.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/cv.h"
#include "vmath.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../../librt_private.h"

#define MAX_RATIO 10.0	/* maximum allowed height-to-width ration for triangles */

/**
 * Tessellation of the TGC.
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */

struct tgc_pts
{
    point_t pt;
    vect_t tan_axb;
    struct vertex *v;
    char dont_use;
};


/* version using tolerances */
int
rt_tgc_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    struct shell *s;		/* shell to hold facetted TGC */
    struct faceuse *fu, *fu_top, *fu_base;
    struct rt_tgc_internal *tip;
    fastf_t radius;		/* bounding sphere radius */
    fastf_t max_radius; /* max of a, b, c, d */
    fastf_t h, a_axis_len, b_axis_len, c_axis_len, d_axis_len;	/* lengths of TGC vectors */
    vect_t unit_a, unit_b, unit_c, unit_d; /* units vectors in a, b, c, d directions */
    fastf_t rel, absolute, norm;	/* interpreted tolerances */
    fastf_t alpha_tol;	/* final tolerance for ellipse parameter */
    fastf_t abs_tol;	/* handle invalid ttol->abs */
    size_t nells;		/* total number of ellipses */
    size_t nsegs;		/* number of vertices/ellipse */
    vect_t *A;		/* array of A vectors for ellipses */
    vect_t *B;		/* array of B vectors for ellipses */
    fastf_t *factors;	/* array of ellipse locations along height vector */
    vect_t vtmp;
    vect_t normal;		/* normal vector */
    vect_t rev_norm;	/* reverse normal */
    struct tgc_pts **pts;		/* array of points (pts[ellipse#][seg#]) */
    struct bu_ptbl verts;		/* table of vertices used for top and bottom faces */
    struct bu_ptbl faces;		/* table of faceuses for nmg_gluefaces */
    struct vertex **v[3];		/* array for making triangular faces */

    size_t i;

    VSETALL(unit_a, 0);
    VSETALL(unit_b, 0);
    VSETALL(unit_c, 0);
    VSETALL(unit_d, 0);

    RT_CK_DB_INTERNAL(ip);
    tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    if (ttol->abs > 0.0 && ttol->abs < tol->dist) {
	bu_log("WARNING: tessellation tolerance is %fmm while calculational tolerance is %fmm\n",
	       ttol->abs, tol->dist);
	bu_log("Cannot tessellate a TGC to finer tolerance than the calculational tolerance\n");
	abs_tol = tol->dist;
    } else {
	abs_tol = ttol->abs;
    }

    h = MAGNITUDE(tip->h);
    a_axis_len = MAGNITUDE(tip->a);
    if (2.0*a_axis_len <= tol->dist)
	a_axis_len = 0.0;
    b_axis_len = MAGNITUDE(tip->b);
    if (2.0*b_axis_len <= tol->dist)
	b_axis_len = 0.0;
    c_axis_len = MAGNITUDE(tip->c);
    if (2.0*c_axis_len <= tol->dist)
	c_axis_len = 0.0;
    d_axis_len = MAGNITUDE(tip->d);
    if (2.0*d_axis_len <= tol->dist)
	d_axis_len = 0.0;

    if (ZERO(a_axis_len) && ZERO(b_axis_len) && (ZERO(c_axis_len) || ZERO(d_axis_len))) {
	bu_log("Illegal TGC a, b, and c or d less than tolerance\n");
	return -1;
    } else if (ZERO(c_axis_len) && ZERO(d_axis_len) && (ZERO(a_axis_len) || ZERO(b_axis_len))) {
	bu_log("Illegal TGC c, d, and a or b less than tolerance\n");
	return -1;
    }

    if (a_axis_len > 0.0) {
	fastf_t inv_length = 1.0/a_axis_len;
	VSCALE(unit_a, tip->a, inv_length);
    }
    if (b_axis_len > 0.0) {
	fastf_t inv_length = 1.0/b_axis_len;
	VSCALE(unit_b, tip->b, inv_length);
    }
    if (c_axis_len > 0.0) {
	fastf_t inv_length = 1.0/c_axis_len;
	VSCALE(unit_c, tip->c, inv_length);
    }
    if (d_axis_len > 0.0) {
	fastf_t inv_length = 1.0/d_axis_len;
	VSCALE(unit_d, tip->d, inv_length);
    }

    /* get bounding sphere radius for relative tolerance */
    radius = h/2.0;
    max_radius = 0.0;
    if (a_axis_len > max_radius)
	max_radius = a_axis_len;
    if (b_axis_len > max_radius)
	max_radius = b_axis_len;
    if (c_axis_len > max_radius)
	max_radius = c_axis_len;
    if (d_axis_len > max_radius)
	max_radius = d_axis_len;

    if (max_radius > radius)
	radius = max_radius;

    if (abs_tol <= 0.0 && ttol->rel <= 0.0 && ttol->norm <= 0.0) {
	/* no tolerances specified, use 10% relative tolerance */
	if ((radius * 0.2) < max_radius)
	    alpha_tol = 2.0 * acos(1.0 - 2.0 * radius * 0.1 / max_radius);
	else
	    alpha_tol = M_PI_2;
    } else {
	if (abs_tol > 0.0)
	    absolute = 2.0 * acos(1.0 - abs_tol/max_radius);
	else
	    absolute = M_PI_2;

	if (ttol->rel > 0.0) {
	    if (ttol->rel * 2.0 * radius < max_radius)
		rel = 2.0 * acos(1.0 - ttol->rel * 2.0 * radius/max_radius);
	    else
		rel = M_PI_2;
	} else
	    rel = M_PI_2;

	if (ttol->norm > 0.0) {
	    fastf_t norm_top, norm_bot;

	    if (a_axis_len < b_axis_len)
		norm_bot = 2.0 * atan(tan(ttol->norm) * (a_axis_len / b_axis_len));
	    else
		norm_bot = 2.0 * atan(tan(ttol->norm) * (b_axis_len / a_axis_len));

	    if (c_axis_len < d_axis_len)
		norm_top = 2.0 * atan(tan(ttol->norm) * (c_axis_len / d_axis_len));
	    else
		norm_top = 2.0 * atan(tan(ttol->norm) * (d_axis_len / c_axis_len));

	    if (norm_bot < norm_top)
		norm = norm_bot;
	    else
		norm = norm_top;
	} else
	    norm = M_PI_2;

	if (absolute < rel)
	    alpha_tol = absolute;
	else
	    alpha_tol = rel;
	if (norm < alpha_tol)
	    alpha_tol = norm;
    }

    /* get number of segments per quadrant */
    nsegs = (int)(M_PI_2 / alpha_tol + 0.9999);
    if (nsegs < 2)
	nsegs = 2;

    /* and for complete ellipse */
    nsegs *= 4;

    /* get number and placement of intermediate ellipses */
    {
	fastf_t ratios[4], max_ratio;
	fastf_t new_ratio = 0;
	int which_ratio;
	fastf_t len_ha, len_hb;
	vect_t ha, hb;
	fastf_t ang;
	fastf_t sin_ang, cos_ang, cos_m_1_sq, sin_sq;
	fastf_t len_A, len_B, len_C, len_D;
	size_t bot_ell=0;
	size_t top_ell=1;
	int reversed=0;

	nells = 2;

	max_ratio = MAX_RATIO + 1.0;

	factors = (fastf_t *)bu_malloc(nells*sizeof(fastf_t), "factors");
	A = (vect_t *)bu_malloc(nells*sizeof(vect_t), "A vectors");
	B = (vect_t *)bu_malloc(nells*sizeof(vect_t), "B vectors");

	factors[bot_ell] = 0.0;
	factors[top_ell] = 1.0;
	VMOVE(A[bot_ell], tip->a);
	VMOVE(A[top_ell], tip->c);
	VMOVE(B[bot_ell], tip->b);
	VMOVE(B[top_ell], tip->d);

	/* make sure that AxB points in the general direction of H */
	VCROSS(vtmp, A[0], B[0]);
	if (VDOT(vtmp, tip->h) < 0.0) {
	    VMOVE(A[bot_ell], tip->b);
	    VMOVE(A[top_ell], tip->d);
	    VMOVE(B[bot_ell], tip->a);
	    VMOVE(B[top_ell], tip->c);
	    reversed = 1;
	}
	ang = M_2PI/((double)nsegs);
	sin_ang = sin(ang);
	cos_ang = cos(ang);
	cos_m_1_sq = (cos_ang - 1.0)*(cos_ang - 1.0);
	sin_sq = sin_ang*sin_ang;

	VJOIN2(ha, tip->h, 1.0, tip->c, -1.0, tip->a);
	VJOIN2(hb, tip->h, 1.0, tip->d, -1.0, tip->b);
	len_ha = MAGNITUDE(ha);
	len_hb = MAGNITUDE(hb);

	while (max_ratio > MAX_RATIO) {
	    fastf_t tri_width;

	    len_A = MAGNITUDE(A[bot_ell]);
	    if (2.0*len_A <= tol->dist)
		len_A = 0.0;
	    len_B = MAGNITUDE(B[bot_ell]);
	    if (2.0*len_B <= tol->dist)
		len_B = 0.0;
	    len_C = MAGNITUDE(A[top_ell]);
	    if (2.0*len_C <= tol->dist)
		len_C = 0.0;
	    len_D = MAGNITUDE(B[top_ell]);
	    if (2.0*len_D <= tol->dist)
		len_D = 0.0;

	    if ((len_B > 0.0 && len_D > 0.0) ||
		(len_B > 0.0 && (ZERO(len_D) && ZERO(len_C))))
	    {
		tri_width = sqrt(cos_m_1_sq*len_A*len_A + sin_sq*len_B*len_B);
		ratios[0] = (factors[top_ell] - factors[bot_ell])*len_ha
		    /tri_width;
	    } else
		ratios[0] = 0.0;

	    if ((len_A > 0.0 && len_C > 0.0) ||
		(len_A > 0.0 && (ZERO(len_C) && ZERO(len_D))))
	    {
		tri_width = sqrt(sin_sq*len_A*len_A + cos_m_1_sq*len_B*len_B);
		ratios[1] = (factors[top_ell] - factors[bot_ell])*len_hb
		    /tri_width;
	    } else
		ratios[1] = 0.0;

	    if ((len_D > 0.0 && len_B > 0.0) ||
		(len_D > 0.0 && (ZERO(len_A) && ZERO(len_B))))
	    {
		tri_width = sqrt(cos_m_1_sq*len_C*len_C + sin_sq*len_D*len_D);
		ratios[2] = (factors[top_ell] - factors[bot_ell])*len_ha
		    /tri_width;
	    } else
		ratios[2] = 0.0;

	    if ((len_C > 0.0 && len_A > 0.0) ||
		(len_C > 0.0 && (ZERO(len_A) && ZERO(len_B))))
	    {
		tri_width = sqrt(sin_sq*len_C*len_C + cos_m_1_sq*len_D*len_D);
		ratios[3] = (factors[top_ell] - factors[bot_ell])*len_hb
		    /tri_width;
	    } else
		ratios[3] = 0.0;

	    which_ratio = -1;
	    max_ratio = 0.0;

	    for (i=0; i<4; i++) {
		if (ratios[i] > max_ratio) {
		    max_ratio = ratios[i];
		    which_ratio = i;
		}
	    }

	    if (ZERO(len_A) && ZERO(len_B) && ZERO(len_C) && ZERO(len_D)) {
		if (top_ell == nells - 1) {
		    VMOVE(A[top_ell-1], A[top_ell]);
		    VMOVE(B[top_ell-1], A[top_ell]);
		    factors[top_ell-1] = factors[top_ell];
		} else if (bot_ell == 0) {
		    for (i=0; i<nells-1; i++) {
			VMOVE(A[i], A[i+1]);
			VMOVE(B[i], B[i+1]);
			factors[i] = factors[i+1];
		    }
		}

		nells -= 1;
		break;
	    }

	    if (max_ratio <= MAX_RATIO)
		break;

	    if (which_ratio == 0 || which_ratio == 1) {
		new_ratio = MAX_RATIO/max_ratio;
		if (bot_ell == 0 && new_ratio > 0.5)
		    new_ratio = 0.5;
	    } else if (which_ratio == 2 || which_ratio == 3) {
		new_ratio = 1.0 - MAX_RATIO/max_ratio;
		if (top_ell == nells - 1 && new_ratio < 0.5)
		    new_ratio = 0.5;
	    } else {
		/* no MAX? */
		bu_bomb("rt_tgc_tess: Should never get here!!\n");
	    }

	    nells++;
	    factors = (fastf_t *)bu_realloc(factors, nells*sizeof(fastf_t), "factors");
	    A = (vect_t *)bu_realloc(A, nells*sizeof(vect_t), "A vectors");
	    B = (vect_t *)bu_realloc(B, nells*sizeof(vect_t), "B vectors");

	    for (i=nells-1; i>top_ell; i--) {
		factors[i] = factors[i-1];
		VMOVE(A[i], A[i-1]);
		VMOVE(B[i], B[i-1]);
	    }

	    factors[top_ell] = factors[bot_ell] +
		new_ratio*(factors[top_ell+1] - factors[bot_ell]);

	    if (reversed) {
		VBLEND2(A[top_ell], (1.0-factors[top_ell]), tip->b, factors[top_ell], tip->d);
		VBLEND2(B[top_ell], (1.0-factors[top_ell]), tip->a, factors[top_ell], tip->c);
	    } else {
		VBLEND2(A[top_ell], (1.0-factors[top_ell]), tip->a, factors[top_ell], tip->c);
		VBLEND2(B[top_ell], (1.0-factors[top_ell]), tip->b, factors[top_ell], tip->d);
	    }

	    if (which_ratio == 0 || which_ratio == 1) {
		top_ell++;
		bot_ell++;
	    }

	}

    }

    /* get memory for points */
    pts = (struct tgc_pts **)bu_calloc(nells, sizeof(struct tgc_pts *), "rt_tgc_tess: pts");
    for (i=0; i<nells; i++)
	pts[i] = (struct tgc_pts *)bu_calloc(nsegs, sizeof(struct tgc_pts), "rt_tgc_tess: pts");

    /* calculate geometry for points */
    for (i=0; i<nells; i++) {
	fastf_t h_factor;
	size_t j;

	h_factor = factors[i];
	for (j=0; j<nsegs; j++) {
	    double alpha;
	    double sin_alpha, cos_alpha;

	    alpha = M_2PI * (double)(2*j+1)/(double)(2*nsegs);
	    sin_alpha = sin(alpha);
	    cos_alpha = cos(alpha);

	    /* vertex geometry */
	    if (i == 0 && ZERO(a_axis_len) && ZERO(b_axis_len)) {
		VMOVE(pts[i][j].pt, tip->v);
	    } else if (i == nells-1 && ZERO(c_axis_len) && ZERO(d_axis_len)) {
		VADD2(pts[i][j].pt, tip->v, tip->h);
	    } else {
		VJOIN3(pts[i][j].pt, tip->v, h_factor, tip->h, cos_alpha, A[i], sin_alpha, B[i]);
	    }

	    /* Storing the tangent here while sines and cosines are available */
	    if (i == 0 && ZERO(a_axis_len) && ZERO(b_axis_len)) {
		VCOMB2(pts[0][j].tan_axb, -sin_alpha, unit_c, cos_alpha, unit_d);
	    } else if (i == nells-1 && ZERO(c_axis_len) && ZERO(d_axis_len)) {
		VCOMB2(pts[i][j].tan_axb, -sin_alpha, unit_a, cos_alpha, unit_b);
	    } else {
		VCOMB2(pts[i][j].tan_axb, -sin_alpha, A[i], cos_alpha, B[i]);
	    }
	}
    }

    /* make sure no edges will be created with length < tol->dist */
    for (i=0; i<nells; i++) {
	size_t j;
	point_t curr_pt;
	vect_t edge_vect;

	if (i == 0 && (ZERO(a_axis_len) || ZERO(b_axis_len)))
	    continue;
	else if (i == nells-1 && (ZERO(c_axis_len) || ZERO(d_axis_len)))
	    continue;

	VMOVE(curr_pt, pts[i][0].pt);
	for (j=1; j<nsegs; j++) {
	    fastf_t edge_len_sq;

	    VSUB2(edge_vect, curr_pt, pts[i][j].pt);
	    edge_len_sq = MAGSQ(edge_vect);
	    if (edge_len_sq > tol->dist_sq) {
		VMOVE(curr_pt, pts[i][j].pt);
	    } else {
		/* don't use this point, it will create a too short edge */
		pts[i][j].dont_use = 'n';
	    }
	}
    }

    /* make region, shell, vertex */
    *r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    bu_ptbl_init(&verts, 64, " &verts ");
    bu_ptbl_init(&faces, 64, " &faces ");
    /* Make bottom face */
    if (a_axis_len > 0.0 && b_axis_len > 0.0) {
	for (i=nsegs; i>0; i--) {
	    /* reverse order to get outward normal */
	    if (!pts[0][i-1].dont_use)
		bu_ptbl_ins(&verts, (long *)&pts[0][i-1].v);
	}

	if (BU_PTBL_LEN(&verts) > 2) {
	    fu_base = nmg_cmface(s, (struct vertex ***)BU_PTBL_BASEADDR(&verts), BU_PTBL_LEN(&verts));
	    bu_ptbl_ins(&faces, (long *)fu_base);
	} else
	    fu_base = (struct faceuse *)NULL;
    } else
	fu_base = (struct faceuse *)NULL;


    /* Make top face */
    if (c_axis_len > 0.0 && d_axis_len > 0.0) {
	bu_ptbl_reset(&verts);
	for (i=0; i<nsegs; i++) {
	    if (!pts[nells-1][i].dont_use)
		bu_ptbl_ins(&verts, (long *)&pts[nells-1][i].v);
	}

	if (BU_PTBL_LEN(&verts) > 2) {
	    fu_top = nmg_cmface(s, (struct vertex ***)BU_PTBL_BASEADDR(&verts), BU_PTBL_LEN(&verts));
	    bu_ptbl_ins(&faces, (long *)fu_top);
	} else
	    fu_top = (struct faceuse *)NULL;
    } else
	fu_top = (struct faceuse *)NULL;

    /* Free table of vertices */
    bu_ptbl_free(&verts);

    /* Make triangular faces */
    for (i=0; i<nells-1; i++) {
	size_t j;
	struct vertex **curr_top;
	struct vertex **curr_bot;

	curr_bot = &pts[i][0].v;
	curr_top = &pts[i+1][0].v;
	for (j=0; j<nsegs; j++) {
	    size_t k;

	    k = j+1;
	    if (k == nsegs)
		k = 0;
	    if (i != 0 || a_axis_len > 0.0 || b_axis_len > 0.0) {
		if (!pts[i][k].dont_use) {
		    v[0] = curr_bot;
		    v[1] = &pts[i][k].v;
		    if (i+1 == nells-1 && ZERO(c_axis_len) && ZERO(d_axis_len))
			v[2] = &pts[i+1][0].v;
		    else
			v[2] = curr_top;
		    fu = nmg_cmface(s, v, 3);
		    bu_ptbl_ins(&faces, (long *)fu);
		    curr_bot = &pts[i][k].v;
		}
	    }

	    if (i != nells-2 || c_axis_len > 0.0 || d_axis_len > 0.0) {
		if (!pts[i+1][k].dont_use) {
		    v[0] = &pts[i+1][k].v;
		    v[1] = curr_top;
		    if (i == 0 && ZERO(a_axis_len) && ZERO(b_axis_len))
			v[2] = &pts[i][0].v;
		    else
			v[2] = curr_bot;
		    fu = nmg_cmface(s, v, 3);
		    bu_ptbl_ins(&faces, (long *)fu);
		    curr_top = &pts[i+1][k].v;
		}
	    }
	}
    }

    /* Assign geometry */
    for (i=0; i<nells; i++) {
	size_t j;

	for (j=0; j<nsegs; j++) {
	    point_t pt_geom;
	    double alpha;
	    double sin_alpha, cos_alpha;

	    alpha = M_2PI * (double)(2*j+1)/(double)(2*nsegs);
	    sin_alpha = sin(alpha);
	    cos_alpha = cos(alpha);

	    /* vertex geometry */
	    if (i == 0 && ZERO(a_axis_len) && ZERO(b_axis_len)) {
		if (j == 0)
		    nmg_vertex_gv(pts[0][0].v, tip->v);
	    } else if (i == nells-1 && ZERO(c_axis_len) && ZERO(d_axis_len)) {
		if (j == 0) {
		    VADD2(pt_geom, tip->v, tip->h);
		    nmg_vertex_gv(pts[i][0].v, pt_geom);
		}
	    } else if (pts[i][j].v)
		nmg_vertex_gv(pts[i][j].v, pts[i][j].pt);

	    /* Storing the tangent here while sines and cosines are available */
	    if (i == 0 && ZERO(a_axis_len) && ZERO(b_axis_len)) {
		VCOMB2(pts[0][j].tan_axb, -sin_alpha, unit_c, cos_alpha, unit_d);
	    } else if (i == nells-1 && ZERO(c_axis_len) && ZERO(d_axis_len)) {
		VCOMB2(pts[i][j].tan_axb, -sin_alpha, unit_a, cos_alpha, unit_b);
	    } else {
		VCOMB2(pts[i][j].tan_axb, -sin_alpha, A[i], cos_alpha, B[i]);
	    }
	}
    }

    /* Associate face plane equations */
    for (i=0; i<BU_PTBL_LEN(&faces); i++) {
	fu = (struct faceuse *)BU_PTBL_GET(&faces, i);
	NMG_CK_FACEUSE(fu);

	if (nmg_calc_face_g(fu, &RTG.rtg_vlfree)) {
	    bu_log("rt_tess_tgc: failed to calculate plane equation\n");
	    nmg_pr_fu_briefly(fu, "");
	    return -1;
	}
    }


    /* Calculate vertexuse normals */
    for (i=0; i<nells; i++) {
	size_t j, k;

	k = i + 1;
	if (k == nells)
	    k = i - 1;

	for (j=0; j<nsegs; j++) {
	    vect_t tan_h;		/* vector tangent from one ellipse to next */
	    struct vertexuse *vu;

	    /* normal at vertex */
	    if (i == nells - 1) {
		if (ZERO(c_axis_len) && ZERO(d_axis_len)) {
		    VSUB2(tan_h, pts[i][0].pt, pts[k][j].pt);
		} else if (k == 0 && ZERO(c_axis_len) && ZERO(d_axis_len)) {
		    VSUB2(tan_h, pts[i][j].pt, pts[k][0].pt);
		} else {
		    VSUB2(tan_h, pts[i][j].pt, pts[k][j].pt);
		}
	    } else if (i == 0) {
		if (ZERO(a_axis_len) && ZERO(b_axis_len)) {
		    VSUB2(tan_h, pts[k][j].pt, pts[i][0].pt);
		} else if (k == nells-1 && ZERO(c_axis_len) && ZERO(d_axis_len)) {
		    VSUB2(tan_h, pts[k][0].pt, pts[i][j].pt);
		} else {
		    VSUB2(tan_h, pts[k][j].pt, pts[i][j].pt);
		}
	    } else if (k == 0 && ZERO(a_axis_len) && ZERO(b_axis_len)) {
		VSUB2(tan_h, pts[k][0].pt, pts[i][j].pt);
	    } else if (k == nells-1 && ZERO(c_axis_len) && ZERO(d_axis_len)) {
		VSUB2(tan_h, pts[k][0].pt, pts[i][j].pt);
	    } else {
		VSUB2(tan_h, pts[k][j].pt, pts[i][j].pt);
	    }

	    VCROSS(normal, pts[i][j].tan_axb, tan_h);
	    VUNITIZE(normal);
	    VREVERSE(rev_norm, normal);

	    if (!(i == 0 && ZERO(a_axis_len) && ZERO(b_axis_len)) &&
		!(i == nells-1 && ZERO(c_axis_len) && ZERO(d_axis_len)) &&
		pts[i][j].v)
	    {
		for (BU_LIST_FOR(vu, vertexuse, &pts[i][j].v->vu_hd)) {
		    NMG_CK_VERTEXUSE(vu);

		    fu = nmg_find_fu_of_vu(vu);
		    NMG_CK_FACEUSE(fu);

		    /* don't need vertexuse normals for faces that are really flat */
		    if (fu == fu_base || fu->fumate_p == fu_base ||
			fu == fu_top  || fu->fumate_p == fu_top)
			continue;

		    if (fu->orientation == OT_SAME)
			nmg_vertexuse_nv(vu, normal);
		    else if (fu->orientation == OT_OPPOSITE)
			nmg_vertexuse_nv(vu, rev_norm);
		}
	    }
	}
    }

    /* Finished with storage, so free it */
    bu_free((char *)factors, "rt_tgc_tess: factors");
    bu_free((char *)A, "rt_tgc_tess: A");
    bu_free((char *)B, "rt_tgc_tess: B");
    for (i=0; i<nells; i++)
	bu_free((char *)pts[i], "rt_tgc_tess: pts[i]");
    bu_free((char *)pts, "rt_tgc_tess: pts");

    /* mark real edges for top and bottom faces */
    for (i=0; i<2; i++) {
	struct loopuse *lu;

	if (i == 0)
	    fu = fu_base;
	else
	    fu = fu_top;

	if (fu == (struct faceuse *)NULL)
	    continue;

	NMG_CK_FACEUSE(fu);

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;

	    NMG_CK_LOOPUSE(lu);

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		struct edge *e;

		NMG_CK_EDGEUSE(eu);
		e = eu->e_p;
		NMG_CK_EDGE(e);
		e->is_real = 1;
	    }
	}
    }

    nmg_region_a(*r, tol);

    /* glue faces together */
    nmg_gluefaces((struct faceuse **)BU_PTBL_BASEADDR(&faces), BU_PTBL_LEN(&faces), &RTG.rtg_vlfree, tol);
    bu_ptbl_free(&faces);

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
