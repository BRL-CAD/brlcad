/*               G E T _ C N U R B _ C U R V E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

/* evaluate a cubic parametric-spline coefficient set (from splinef.c) */
extern fastf_t splinef(fastf_t c[4], fastf_t s);

/* arc-sinh, matching getcurve.c */
#define IGES_ARCSINH(a) log((a) + sqrt((a)*(a) + 1.0))

/* number of samples used per conic quadrant / per spline segment when
 * approximating a curved parameter-space curve by a degree-1 polyline
 */
#define UV_SAMPLES_PER_SEG 8


/*
 * Build an order-2 (degree-1) parameter-space (u, v) edge_g_cnurb whose
 * control points ARE the supplied sample points (a polyline), using a
 * clamped uniform knot vector.  The caller supplies "num_pts" (u, v)
 * samples in the "uv" array (packed u0, v0, u1, v1, ...).
 *
 * Returns a newly allocated cnurb, or NULL on failure.
 */
static struct edge_g_cnurb *
Make_uv_polyline(const fastf_t *uv, int num_pts)
{
    struct edge_g_cnurb *crv;
    int pt_type;
    int n_knots;
    int i;

    if (!uv || num_pts < 2) {
	bu_log("Make_uv_polyline: need at least 2 points (got %d)\n", num_pts);
	return (struct edge_g_cnurb *)NULL;
    }

    pt_type = RT_NURB_MAKE_PT_TYPE(2, RT_NURB_PT_UV, RT_NURB_PT_NONRAT);

    /* order-2 polyline: knot vector length = num_pts + order */
    n_knots = num_pts + 2;
    crv = nmg_nurb_new_cnurb(2, n_knots, num_pts, pt_type);
    if (!crv)
	return (struct edge_g_cnurb *)NULL;

    /* clamped uniform knot vector: 0, 0, 1, 2, ..., num_pts-1, num_pts-1 */
    crv->k.knots[0] = 0.0;
    for (i = 1; i <= num_pts; i++)
	crv->k.knots[i] = (fastf_t)(i - 1);
    crv->k.knots[num_pts + 1] = (fastf_t)(num_pts - 1);

    /* control points are the samples themselves (2 coords: u, v) */
    for (i = 0; i < num_pts; i++) {
	crv->ctl_points[i*2] = uv[i*2];
	crv->ctl_points[i*2 + 1] = uv[i*2 + 1];
    }

    return crv;
}


/*
 * Read and evaluate an IGES Conic Arc (type 104) or Parametric Spline
 * Curve (type 112) as a parameter-space curve, returning an order-2
 * (degree-1) UV polyline edge_g_cnurb.  The entity's x, y coordinates
 * ARE the u, v parameters (z is ignored); the entity's rotation matrix
 * is applied, matching the existing 110 case.  No u/v translations are
 * applied (this path does not track the surface knot translations).
 *
 * Returns a newly allocated cnurb, or NULL on any parse failure or
 * unhandled sub-form (graceful skip).
 */
static struct edge_g_cnurb *
Get_uv_polyline_curve(size_t curve)
{
    struct edge_g_cnurb *crv = (struct edge_g_cnurb *)NULL;
    fastf_t *uv = (fastf_t *)NULL;
    int num_pts = 0;
    int type = 0;
    int i, j;
    double pi;

    pi = atan2(0.0, -1.0);

    Readrec(dir[curve]->param);
    Readint(&type, "");
    if (type != dir[curve]->type) {
	bu_log("Get_uv_polyline_curve: looking for curve type %d, found %d\n" ,
	       dir[curve]->type, type);
	return (struct edge_g_cnurb *)NULL;
    }

    if (dir[curve]->type == 112) {
	/* parametric spline curve */
	struct spline *splroot;
	struct segment *seg, *seg1;
	double a;
	int seg_no;
	int idx;

	Readint(&i, "");	/* spline type */
	Readint(&i, "");	/* continuity */

	BU_ALLOC(splroot, struct spline);
	splroot->start = NULL;

	Readint(&splroot->ndim, "");	/* 2->planar, 3->3d */
	Readint(&splroot->nsegs, "");	/* number of segments */
	Readdbl(&a, "");		/* first breakpoint */

	if (splroot->ndim != 2 && splroot->ndim != 3) {
	    bu_log("Get_uv_polyline_curve: entity D%07d has invalid ndim: %d\n",
		   dir[curve]->direct, splroot->ndim);
	    bu_free((char *)splroot, "Get_uv_polyline_curve: splroot");
	    return (struct edge_g_cnurb *)NULL;
	}
	if (splroot->nsegs < 1) {
	    bu_log("Get_uv_polyline_curve: entity D%07d has nsegs == %d\n",
		   dir[curve]->direct, splroot->nsegs);
	    bu_free((char *)splroot, "Get_uv_polyline_curve: splroot");
	    return (struct edge_g_cnurb *)NULL;
	}

	/* build a linked list of segments, reading the breakpoints */
	seg = splroot->start;
	for (i = 0; i < splroot->nsegs; i++) {
	    if (seg == NULL) {
		BU_ALLOC(seg, struct segment);
		splroot->start = seg;
	    } else {
		BU_ALLOC(seg->next, struct segment);
		seg = seg->next;
	    }
	    seg->segno = i+1;
	    seg->next = NULL;
	    seg->tmin = a;		/* minimum T for this segment */
	    Readflt(&seg->tmax, "");	/* maximum T for this segment */
	    a = seg->tmax;
	}

	/* read polynomial coefficients */
	seg = splroot->start;
	for (i = 0; i < splroot->nsegs; i++) {
	    for (j = 0; j < 4; j++)
		Readflt(&seg->cx[j], "");	/* x (== u) coeff's */
	    for (j = 0; j < 4; j++)
		Readflt(&seg->cy[j], "");	/* y (== v) coeff's */
	    for (j = 0; j < 4; j++)
		Readflt(&seg->cz[j], "");	/* z coeff's (ignored) */
	    seg = seg->next;
	}

	/* sample UV_SAMPLES_PER_SEG points per segment; adjacent
	 * segments share an endpoint, so drop the duplicated first
	 * sample of every segment after the first
	 */
	num_pts = splroot->nsegs * UV_SAMPLES_PER_SEG - (splroot->nsegs - 1);
	uv = (fastf_t *)bu_calloc((size_t)num_pts * 2, sizeof(fastf_t),
				  "Get_uv_polyline_curve: spline uv");

	idx = 0;
	seg_no = 0;
	seg = splroot->start;
	while (seg != NULL) {
	    int start = (seg_no == 0) ? 0 : 1;

	    for (i = start; i < UV_SAMPLES_PER_SEG; i++) {
		point_t pt, pt2;

		a = (fastf_t)i/(fastf_t)(UV_SAMPLES_PER_SEG - 1)*(seg->tmax - seg->tmin);
		pt[X] = splinef(seg->cx, a);
		pt[Y] = splinef(seg->cy, a);
		pt[Z] = 0.0;

		MAT4X3PNT(pt2, *dir[curve]->rot, pt);
		uv[idx*2] = pt2[X];
		uv[idx*2 + 1] = pt2[Y];
		idx++;
	    }
	    seg_no++;
	    seg = seg->next;
	}
	num_pts = idx;

	/* free segment list */
	seg = splroot->start;
	while (seg != NULL) {
	    seg1 = seg;
	    seg = seg->next;
	    bu_free((char *)seg1, "Get_uv_polyline_curve: seg1");
	}
	bu_free((char *)splroot, "Get_uv_polyline_curve: splroot");

    } else if (dir[curve]->type == 104) {
	/* conic arc */
	double A, B, C, D, E, F, a, b, c, del, I, theta, dpi, t1, t2, xc, yc;
	point_t v1, v2;
	int ctype;
	int num_points;
	int idx = 0;

	/* read coefficients */
	Readdbl(&A, "");
	Readdbl(&B, "");
	Readdbl(&C, "");
	Readdbl(&D, "");
	Readdbl(&E, "");
	Readdbl(&F, "");

	/* read common z-coordinate (ignored for UV) */
	Readflt(&v1[2], "");
	v2[2] = v1[2];

	/* read start point */
	Readflt(&v1[0], "");
	Readflt(&v1[1], "");

	/* read terminate point */
	Readflt(&v2[0], "");
	Readflt(&v2[1], "");

	ctype = 0;
	if (dir[curve]->form == 1) {
	    /* Ellipse */
	    if (fabs(E) < SQRT_SMALL_FASTF)
		E = 0.0;
	    if (fabs(B) < SQRT_SMALL_FASTF)
		B = 0.0;
	    if (fabs(D) < SQRT_SMALL_FASTF)
		D = 0.0;

	    if (ZERO(B) && ZERO(D) && ZERO(E))
		ctype = 1;
	    else
		bu_log("Get_uv_polyline_curve: entity D%07d is an incorrectly formatted ellipse\n",
		       dir[curve]->direct);
	}

	/* make coeff of X**2 equal to 1.0 */
	a = A*C - B*B/4.0;
	if (fabs(a) < 1.0 && fabs(a) > TOL) {
	    a = fabs(A);
	    if (fabs(B) < a && !ZERO(B))
		a = fabs(B);
	    V_MIN(a, fabs(C));

	    A = A/a;
	    B = B/a;
	    C = C/a;
	    D = D/a;
	    E = E/a;
	    F = F/a;
	    a = A*C - B*B/4.0;
	}

	if (!ctype) {
	    /* classify the conic */
	    del = A*(C*F-E*E/4.0)-0.5*B*(B*F/2.0-D*E/4.0)+0.5*D*(B*E/4.0-C*D/2.0);
	    I = A+C;
	    if (ZERO(del)) {
		bu_log("Get_uv_polyline_curve: entity D%07d claims to be a conic arc, but isn't\n",
		       dir[curve]->direct);
		return (struct edge_g_cnurb *)NULL;
	    } else if (a > 0.0 && del*I < 0.0)
		ctype = 1;	/* ellipse */
	    else if (a < 0.0)
		ctype = 2;	/* hyperbola */
	    else if (ZERO(a))
		ctype = 3;	/* parabola */
	    else {
		bu_log("Get_uv_polyline_curve: entity D%07d is an imaginary ellipse\n",
		       dir[curve]->direct);
		return (struct edge_g_cnurb *)NULL;
	    }
	}

	num_points = UV_SAMPLES_PER_SEG * 4;	/* samples over whole conic */

	if (ctype == 3) {
	    /* parabola */
	    double p, r1;

	    /* make A+C == 1.0 */
	    if (!EQUAL(A+C, 1.0)) {
		b = A+C;
		A = A/b;
		B = B/b;
		C = C/b;
		D = D/b;
		E = E/b;
		F = F/b;
	    }

	    /* theta: rotation of parabola axis from x-axis */
	    theta = 0.5*atan2(B, C-A);

	    /* p: distance from vertex to directrix */
	    p = (-E*sin(theta) - D*cos(theta))/4.0;
	    if (fabs(p) < TOL) {
		bu_log("Get_uv_polyline_curve: cannot evaluate parabola entity D%07d, p=%g\n",
		       dir[curve]->direct, p);
		return (struct edge_g_cnurb *)NULL;
	    }

	    /* vertex (xc, yc) via parametric form */
	    a = 1.0/(4.0*p);
	    b = ((v1[0]-v2[0])*cos(theta) + (v1[1]-v2[1])*sin(theta))/a;
	    c = ((v1[1]-v2[1])*cos(theta) - (v1[0]-v2[0])*sin(theta));
	    if (fabs(c) < TOL*TOL) {
		bu_log("Get_uv_polyline_curve: cannot evaluate parabola entity D%07d\n",
		       dir[curve]->direct);
		return (struct edge_g_cnurb *)NULL;
	    }
	    b = b/c;
	    t1 = (b + c)/2.0;	/* value of 't' at v1 */
	    t2 = (b - c)/2.0;	/* value of 't' at v2 */
	    xc = v1[0] - a*t1*t1*cos(theta) + t1*sin(theta);
	    yc = v1[1] - a*t1*t1*sin(theta) - t1*cos(theta);

	    uv = (fastf_t *)bu_calloc((size_t)num_points * 2, sizeof(fastf_t),
				      "Get_uv_polyline_curve: parabola uv");

	    dpi = (t2-t1)/(double)(num_points - 1);	/* parameter increment */
	    b = cos(theta);
	    c = sin(theta);
	    for (i = 0; i < num_points; i++) {
		point_t pt, pt2;

		r1 = t1 + dpi*i;
		pt[X] = xc + a*r1*r1*b - r1*c;
		pt[Y] = yc + a*r1*r1*c + r1*b;
		pt[Z] = 0.0;

		MAT4X3PNT(pt2, *dir[curve]->rot, pt);
		uv[idx*2] = pt2[X];
		uv[idx*2 + 1] = pt2[Y];
		idx++;
	    }
	    num_pts = idx;

	} else {
	    /* ellipse (ctype 1) or hyperbola (ctype 2) */
	    double A1, C1, F1, alpha, beta;
	    mat_t rot1, rot2;
	    mat_t rot_idn = MAT_INIT_IDN;
	    point_t tmp = VINIT_ZERO;

	    /* center of ellipse or hyperbola */
	    xc = (B*E/4.0 - D*C/2.0)/a;
	    yc = (B*D/4.0 - A*E/2.0)/a;

	    /* theta: rotation of curve axis from x-axis */
	    if (!ZERO(B))
		theta = 0.5*atan2(B, A-C);
	    else
		theta = 0.0;

	    /* coeff's for same curve at origin with theta == 0 */
	    A1 = A + 0.5*B*tan(theta);
	    C1 = C - 0.5*B*tan(theta);
	    F1 = F - A*xc*xc - B*xc*yc - C*yc*yc;

	    if (ctype == 2 && F1/A1 > 0.0)
		theta += pi/2.0;

	    /* translate and rotate start/terminate points to the
	     * simpler curve
	     */
	    for (i = 0; i < 16; i++)
		rot1[i] = rot_idn[i];
	    MAT_DELTAS(rot1, -xc, -yc, 0.0);
	    MAT4X3PNT(tmp, rot1, v1);
	    VMOVE(v1, tmp);
	    MAT4X3PNT(tmp, rot1, v2);
	    VMOVE(v2, tmp);
	    MAT_DELTAS(rot1, 0.0, 0.0, 0.0);
	    rot1[0] = cos(theta);
	    rot1[1] = sin(theta);
	    rot1[4] = (-rot1[1]);
	    rot1[5] = rot1[0];
	    MAT4X3PNT(tmp, rot1, v1);
	    VMOVE(v1, tmp);
	    MAT4X3PNT(tmp, rot1, v2);
	    VMOVE(v2, tmp);
	    MAT_DELTAS(rot1, 0.0, 0.0, 0.0);

	    /* alpha = start angle, beta = terminate angle */
	    beta = 0.0;
	    if (EQUAL(v2[0], v1[0]) && EQUAL(v2[1], v1[1])) {
		/* full circle */
		beta = 2.0*pi;
	    }
	    a = sqrt(fabs(F1/A1));	/* semi-axis length */
	    b = sqrt(fabs(F1/C1));	/* semi-axis length */

	    if (ctype == 1) {
		/* ellipse */
		alpha = atan2(a*v1[1], b*v1[0]);
		if (ZERO(beta)) {
		    beta = atan2(a*v2[1], b*v2[0]);
		    beta = beta - alpha;
		}
	    } else {
		/* hyperbola */
		alpha = IGES_ARCSINH(v1[1]/b);
		beta = IGES_ARCSINH(v2[1]/b);
		if (fabs(a*cosh(beta) - v2[0]) > 0.01)
		    a = (-a);
		beta = beta - alpha;
	    }

	    /* matrix to rotate/translate the simple curve back */
	    MAT_DELTAS(rot1, xc, yc, 0.0);
	    rot1[1] = (-rot1[1]);
	    rot1[4] = (-rot1[4]);
#if defined(USE_BN_MULT_)
	    bn_mat_mul(rot2, *(dir[curve]->rot), rot1);
#else
	    Matmult(*(dir[curve]->rot), rot1, rot2);
#endif

	    uv = (fastf_t *)bu_calloc((size_t)(num_points + 1) * 2, sizeof(fastf_t),
				      "Get_uv_polyline_curve: conic uv");

	    for (i = 0; i <= num_points; i++) {
		point_t tmp2 = VINIT_ZERO;
		point_t pt2;

		theta = alpha + (double)i/(double)num_points*beta;
		if (ctype == 2) {
		    tmp2[0] = a*cosh(theta);
		    tmp2[1] = b*sinh(theta);
		} else {
		    tmp2[0] = a*cos(theta);
		    tmp2[1] = b*sin(theta);
		}
		tmp2[2] = 0.0;

		MAT4X3PNT(pt2, rot2, tmp2);
		uv[idx*2] = pt2[X];
		uv[idx*2 + 1] = pt2[Y];
		idx++;
	    }
	    num_pts = idx;
	}

    } else {
	bu_log("Get_uv_polyline_curve: expected conic arc (104) or parametric spline (112), got type %d\n",
	       dir[curve]->type);
	return (struct edge_g_cnurb *)NULL;
    }

    if (!uv || num_pts < 2) {
	if (uv)
	    bu_free((char *)uv, "Get_uv_polyline_curve: uv");
	bu_log("Get_uv_polyline_curve: entity D%07d produced too few points (%d)\n",
	       dir[curve]->direct, num_pts);
	return (struct edge_g_cnurb *)NULL;
    }

    crv = Make_uv_polyline(uv, num_pts);
    bu_free((char *)uv, "Get_uv_polyline_curve: uv");

    return crv;
}


struct edge_g_cnurb *
Get_cnurb_curve(int curve_de, int *linear)
{
    int i;
    size_t curve;
    struct edge_g_cnurb *crv;

    *linear = 0;

    curve = IGES_DE2INDEX(curve_de);
    if (curve >= dirarraylen) {
	bu_log("Get_cnurb_curve: DE=%d is too large, dirarraylen = %zu\n", curve_de, dirarraylen);
	return (struct edge_g_cnurb *)NULL;
    }

    switch (dir[curve]->type) {
	case 110: {
	    /* line */
	    int pt_type = 0;
	    int type = 0;
	    point_t pt1;
	    point_t start_pt, end_pt;

	    Readrec(dir[curve]->param);
	    Readint(&type, "");
	    if (type != dir[curve]->type) {
		bu_log("Error in Get_cnurb_curve, looking for curve type %d, found %d\n" ,
		       dir[curve]->type, type);
		return (struct edge_g_cnurb *)NULL;

	    }
	    /* Read first point */
	    for (i = 0; i < 3; i++)
		Readcnv(&pt1[i], "");
	    MAT4X3PNT(start_pt, *dir[curve]->rot, pt1);

	    /* Read second point */
	    for (i = 0; i < 3; i++)
		Readcnv(&pt1[i], "");
	    MAT4X3PNT(end_pt, *dir[curve]->rot, pt1);

	    /* pt_type for rational UVW coords */
	    pt_type = RT_NURB_MAKE_PT_TYPE(3, 3, 1);

	    /* make a linear edge_g_cnurb (order=2) */
	    crv = nmg_nurb_new_cnurb(2, 4, 2, pt_type);

	    /* insert control mesh */
	    VMOVE(crv->ctl_points, start_pt);
	    VMOVE(&crv->ctl_points[3], end_pt);

	    /* insert knot values */
	    crv->k.knots[0] = 0.0;
	    crv->k.knots[1] = 0.0;
	    crv->k.knots[2] = 1.0;
	    crv->k.knots[3] = 1.0;

	    *linear = 1;

	    return crv;
	}
	case 104:	/* conic arc */
	case 112:	/* parametric spline curve */
	    /* Evaluate to an order-2 (degree-1) UV polyline */
	    crv = Get_uv_polyline_curve(curve);
	    if (!crv)
		return (struct edge_g_cnurb *)NULL;
	    *linear = 1;
	    return crv;
	case 126:	/* B-spline */
	    crv = Get_cnurb(curve);
	    if (!crv)
		return (struct edge_g_cnurb *)NULL;
	    if (crv->order < 3)
		*linear = 1;
	    return crv;
	default:
	    bu_log("Not yet handling curves of type: %s\n", iges_type(dir[curve]->type));
	    break;
    }

    return (struct edge_g_cnurb *)NULL;
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
