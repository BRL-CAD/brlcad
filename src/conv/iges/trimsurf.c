/*                      T R I M S U R F . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file iges/trimsurf.c
 *
 * This routine loops through all the directory entries and calls
 * appropriate routines to convert trimmed surface entities to BRL-CAD
 * NMG TNURBS
 *
 */

#include "bu/debug.h"
#include "./iges_struct.h"
#include "./iges_extern.h"
#include "./iges_brep.h"
#include "./iges_surf.h"


/* evaluate a cubic parametric-spline coefficient set (from splinef.c) */
extern fastf_t splinef(fastf_t c[4], fastf_t s);

/* translations to get knot vectors in first quadrant */
static fastf_t u_translation = 0.0;
static fastf_t v_translation = 0.0;

#define UV_TOL 1.0e-6

#define CTL_INDEX(_i, _j)	((_i * n_cols + _j) * ncoords)

struct snurb_hit
{
    struct bu_list l;
    fastf_t dist;
    point_t pt;
    vect_t norm;
    struct face *f;
};


struct face_g_snurb *
Get_nurb_surf(size_t entityno, struct model *m)
{
    struct face_g_snurb *srf;
    point_t pt;
    int entity_type = 0;
    int i;
    int rational;
    int ncoords;
    int n_rows, n_cols, u_order, v_order;
    int n_u, n_v;
    int pt_type;
    double a = 0.0;

    /* Acquiring Data */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n",
	       dir[entityno]->direct, dir[entityno]->name);
	return (struct face_g_snurb *)NULL;
    }

    Readrec(dir[entityno]->param);
    Readint(&entity_type, "");
    if (entity_type != 128) {
	bu_log("Only B-Spline surfaces allowed for faces (found type %d)\n", entity_type);
	return (struct face_g_snurb *)NULL;
    }
    Readint(&i, "");
    n_cols = i + 1;
    Readint(&i, "");
    n_rows = i + 1;
    Readint(&i, "");
    u_order = i + 1;
    Readint (&i, "");
    v_order = i + 1;
    Readint(&i, "");
    Readint(&i, "");
    Readint(&i, "");
    rational = !i;

    ncoords = 3+rational;
    pt_type = RT_NURB_MAKE_PT_TYPE(ncoords, RT_NURB_PT_XYZ, rational);
    Readint(&i, "");
    Readint(&i, "");

    n_u = n_cols+u_order;
    n_v = n_rows+v_order;
    if (!m) {
	srf = nmg_nurb_new_snurb(u_order, v_order, n_u, n_v, n_rows, n_cols, pt_type);
    } else {
	int pnum;

	GET_FACE_G_SNURB(srf, m);
	BU_LIST_INIT(&srf->l);
	BU_LIST_INIT(&srf->f_hd);
	srf->l.magic = NMG_FACE_G_SNURB_MAGIC;
	srf->order[0] = u_order;
	srf->order[1] = v_order;
	srf->dir = RT_NURB_SPLIT_ROW;
	srf->u.magic = NMG_KNOT_VECTOR_MAGIC;
	srf->v.magic = NMG_KNOT_VECTOR_MAGIC;
	srf->u.k_size = n_u;
	srf->v.k_size = n_v;
	srf->u.knots = (fastf_t *) bu_malloc (
	    n_u * sizeof(fastf_t), "Get_nurb_surf: u kv knot values");
	srf->v.knots = (fastf_t *) bu_malloc (
	    n_v * sizeof(fastf_t), "Get_nurb_surf: v kv knot values");

	srf->s_size[0] = n_rows;
	srf->s_size[1] = n_cols;
	srf->pt_type = pt_type;

	pnum = sizeof(fastf_t) * n_rows * n_cols * RT_NURB_EXTRACT_COORDS(pt_type);
	srf->ctl_points = (fastf_t *) bu_malloc(
	    pnum, "Get_nurb_surf: control mesh points");

    }
    NMG_CK_FACE_G_SNURB(srf);

    /* Read knot vectors */
    for (i = 0; i < n_u; i++) {
	Readdbl(&a, "");
	srf->u.knots[i] = a;
    }
    for (i = 0; i < n_v; i++) {
	Readdbl(&a, "");
	srf->v.knots[i] = a;
    }

    /* Insure that knot values are non-negative */
    if (srf->v.knots[0] < 0.0) {
	v_translation = (-srf->v.knots[0]);
	for (i = 0; i < n_v; i++)
	    srf->v.knots[i] += v_translation;
    } else
	v_translation = 0.0;

    if (srf->u.knots[0] < 0.0) {
	u_translation = (-srf->u.knots[0]);
	for (i = 0; i < n_u; i++)
	    srf->u.knots[i] += u_translation;
    } else
	u_translation = 0.0;

    /* Read weights */
    for (i = 0; i < n_cols*n_rows; i++) {
	Readdbl(&a, "");
	if (rational)
	    srf->ctl_points[i*ncoords + 3] = a;
    }

    /* Read control points */
    for (i = 0; i < n_cols*n_rows; i++) {
	Readdbl(&a, "");
	if (rational)
	    pt[X] = a*srf->ctl_points[i*ncoords+3];
	else
	    pt[X] = a;
	Readdbl(&a, "");
	if (rational)
	    pt[Y] = a*srf->ctl_points[i*ncoords+3];
	else
	    pt[Y] = a;
	Readdbl(&a, "");
	if (rational)
	    pt[Z] = a*srf->ctl_points[i*ncoords+3];
	else
	    pt[Z] = a;

	/* apply transformation */
	MAT4X3PNT(&srf->ctl_points[i*ncoords], *dir[entityno]->rot, pt);
    }

    Readdbl(&a, "");
    Readdbl(&a, "");
    Readdbl(&a, "");
    Readdbl(&a, "");

    return srf;
}


void
Assign_cnurb_to_eu(struct edgeuse *eu, struct edge_g_cnurb *crv)
{
    fastf_t *ctl_points;
    fastf_t *knots;
    int ncoords;
    int i;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_CNURB(crv);

    ncoords = RT_NURB_EXTRACT_COORDS(crv->pt_type);

    ctl_points = (fastf_t *)bu_calloc((size_t)ncoords*crv->c_size, sizeof(fastf_t),
				      "Assign_cnurb_to_eu: ctl_points");

    for (i = 0; i < crv->c_size; i++) {
	VMOVEN(&ctl_points[i*ncoords], &crv->ctl_points[i*ncoords], ncoords);
    }

    knots = (fastf_t *)bu_calloc(crv->k.k_size, sizeof(fastf_t),
				 "Assign_cnurb_to_eu: knots");

    for (i = 0; i < crv->k.k_size; i++)
	knots[i] = crv->k.knots[i];

    nmg_edge_g_cnurb(eu, crv->order, crv->k.k_size, knots, crv->c_size,
		     crv->pt_type, ctl_points);
}


struct edge_g_cnurb *
Get_cnurb(size_t entity_no)
{
    struct edge_g_cnurb *crv;
    double a = 0.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    int degree = 0;
    int entity_type = 0;
    int i = 0;
    int ncoords = 0;
    int num_pts = 0;
    int planar = 0;
    int pt_type = 0;
    int rational = 0;
    point_t pt = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;

    if (dir[entity_no]->param <= pstart) {
	bu_log("Get_cnurb: Illegal parameter pointer for entity D%07d (%s)\n",
	       dir[entity_no]->direct, dir[entity_no]->name);
	return (struct edge_g_cnurb *)NULL;
    }

    Readrec(dir[entity_no]->param);
    Readint(&entity_type, "");

    if (entity_type != 126) {
	bu_log("Get_cnurb: Was expecting spline curve, got type %d\n", entity_type);
	return (struct edge_g_cnurb *)NULL;
    }

    Readint(&i, "");
    num_pts = i + 1;
    Readint(&degree, "");

    /* properties */
    Readint(&planar, "");
    Readint(&i, "");	/* open or closed */
    Readint(&i, "");	/* polynomial */
    rational = !i;
    Readint(&i, "");	/* periodic */

    if (rational) {
	ncoords = 3;
	pt_type = RT_NURB_MAKE_PT_TYPE(ncoords, RT_NURB_PT_UV, RT_NURB_PT_RATIONAL);
    } else {
	ncoords = 2;
	pt_type = RT_NURB_MAKE_PT_TYPE(ncoords, RT_NURB_PT_UV, RT_NURB_PT_NONRAT);
    }

    crv = nmg_nurb_new_cnurb(degree+1, num_pts+degree+1, num_pts, pt_type);
    /* knot vector */
    for (i = 0; i < num_pts+degree+1; i++) {
	Readdbl(&a, "");
	crv->k.knots[i] = a;
    }

    /* weights */
    for (i = 0; i < num_pts; i++) {
	Readdbl(&a, "");
	if (rational)
	    crv->ctl_points[i*ncoords+2] = a;
    }

    /* control points */
    for (i = 0; i < num_pts; i++) {
	Readdbl(&x, "");
	Readdbl(&y, "");
	Readdbl(&z, "");

	if (rational) {
	    pt[X] = (x + u_translation) * crv->ctl_points[i*ncoords+2];
	    pt[Y] = (y + v_translation) * crv->ctl_points[i*ncoords+2];
	} else {
	    pt[X] = x + u_translation;
	    pt[Y] = y + v_translation;
	}
	pt[Z] = z;

	/* apply transformation */
	MAT4X3PNT(pt2, *dir[entity_no]->rot, pt);
	crv->ctl_points[i*ncoords] = pt2[X];
	crv->ctl_points[i*ncoords+1] = pt2[Y];
    }
    return crv;
}


/* arc-sinh, matching getcurve.c */
#define IGES_ARCSINH(a) log((a) + sqrt((a)*(a) + 1.0))

/* number of samples used per conic quadrant / per spline segment when
 * approximating a curved trimming entity by a degree-1 polyline
 */
#define UV_SAMPLES_PER_SEG 8

/*
 * Build an order-2 (degree-1) parameter-space (u, v) edge_g_cnurb whose
 * control points ARE the supplied sample points (a polyline), using a
 * clamped uniform knot vector.  The caller supplies "num_pts" (u, v)
 * samples in the "uv" array (packed u0, v0, u1, v1, ...).  The
 * u_translation/v_translation shifts are NOT applied here (the sampling
 * routines apply them, mirroring the existing 100/110 cases).
 *
 * Returns a newly allocated cnurb, or NULL on failure.
 */
static struct edge_g_cnurb *
Make_uv_polyline_cnurb(const fastf_t *uv, int num_pts)
{
    struct edge_g_cnurb *crv;
    int pt_type;
    int n_knots;
    int i;

    if (!uv || num_pts < 2) {
	bu_log("Make_uv_polyline_cnurb: need at least 2 points (got %d)\n", num_pts);
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
 * Curve (type 112) as a parameter-space trimming curve, returning an
 * order-2 (degree-1) UV polyline edge_g_cnurb.  The entity's x, y
 * coordinates ARE the u, v parameters (z is ignored); u_translation and
 * v_translation are applied to each sample and the entity's rotation
 * matrix is applied, matching the existing 100/110/126 cases.
 *
 * Returns a newly allocated cnurb, or NULL on any parse failure or
 * unhandled sub-form (graceful skip).
 */
static struct edge_g_cnurb *
Get_uv_curve(size_t entity_no)
{
    struct edge_g_cnurb *crv = (struct edge_g_cnurb *)NULL;
    fastf_t *uv = (fastf_t *)NULL;
    int num_pts = 0;
    int entity_type = 0;
    int i, j;
    double pi;

    pi = atan2(0.0, -1.0);

    if (dir[entity_no]->param <= pstart) {
	bu_log("Get_uv_curve: Illegal parameter pointer for entity D%07d (%s)\n",
	       dir[entity_no]->direct, dir[entity_no]->name);
	return (struct edge_g_cnurb *)NULL;
    }

    Readrec(dir[entity_no]->param);
    Readint(&entity_type, "");

    if (entity_type == 112) {
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
	    bu_log("Get_uv_curve: entity D%07d has invalid ndim: %d\n",
		   dir[entity_no]->direct, splroot->ndim);
	    bu_free(splroot, "Get_uv_curve: splroot");
	    return (struct edge_g_cnurb *)NULL;
	}
	if (splroot->nsegs < 1) {
	    bu_log("Get_uv_curve: entity D%07d has nsegs == %d\n",
		   dir[entity_no]->direct, splroot->nsegs);
	    bu_free(splroot, "Get_uv_curve: splroot");
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
				  "Get_uv_curve: spline uv");

	idx = 0;
	seg_no = 0;
	seg = splroot->start;
	while (seg != NULL) {
	    int start = (seg_no == 0) ? 0 : 1;

	    for (i = start; i < UV_SAMPLES_PER_SEG; i++) {
		point_t pt, pt2;

		a = (fastf_t)i/(fastf_t)(UV_SAMPLES_PER_SEG - 1)*(seg->tmax - seg->tmin);
		pt[X] = splinef(seg->cx, a) + u_translation;
		pt[Y] = splinef(seg->cy, a) + v_translation;
		pt[Z] = 0.0;

		MAT4X3PNT(pt2, *dir[entity_no]->rot, pt);
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
	    bu_free(seg1, "Get_uv_curve: seg1");
	}
	bu_free(splroot, "Get_uv_curve: splroot");

    } else if (entity_type == 104) {
	/* conic arc */
	double A, B, C, D, E, F, a, b, c, del, I, theta, dpi, t1, t2, xc, yc;
	point_t v1, v2;
	int type;
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

	type = 0;
	if (dir[entity_no]->form == 1) {
	    /* Ellipse */
	    if (fabs(E) < SQRT_SMALL_FASTF)
		E = 0.0;
	    if (fabs(B) < SQRT_SMALL_FASTF)
		B = 0.0;
	    if (fabs(D) < SQRT_SMALL_FASTF)
		D = 0.0;

	    if (ZERO(B) && ZERO(D) && ZERO(E))
		type = 1;
	    else
		bu_log("Get_uv_curve: entity D%07d is an incorrectly formatted ellipse\n",
		       dir[entity_no]->direct);
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

	if (!type) {
	    /* classify the conic */
	    del = A*(C*F-E*E/4.0)-0.5*B*(B*F/2.0-D*E/4.0)+0.5*D*(B*E/4.0-C*D/2.0);
	    I = A+C;
	    if (ZERO(del)) {
		bu_log("Get_uv_curve: entity D%07d claims to be a conic arc, but isn't\n",
		       dir[entity_no]->direct);
		return (struct edge_g_cnurb *)NULL;
	    } else if (a > 0.0 && del*I < 0.0)
		type = 1;	/* ellipse */
	    else if (a < 0.0)
		type = 2;	/* hyperbola */
	    else if (ZERO(a))
		type = 3;	/* parabola */
	    else {
		bu_log("Get_uv_curve: entity D%07d is an imaginary ellipse\n",
		       dir[entity_no]->direct);
		return (struct edge_g_cnurb *)NULL;
	    }
	}

	num_points = UV_SAMPLES_PER_SEG * 4;	/* samples over whole conic */

	if (type == 3) {
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
		bu_log("Get_uv_curve: cannot evaluate parabola entity D%07d, p=%g\n",
		       dir[entity_no]->direct, p);
		return (struct edge_g_cnurb *)NULL;
	    }

	    /* vertex (xc, yc) via parametric form */
	    a = 1.0/(4.0*p);
	    b = ((v1[0]-v2[0])*cos(theta) + (v1[1]-v2[1])*sin(theta))/a;
	    c = ((v1[1]-v2[1])*cos(theta) - (v1[0]-v2[0])*sin(theta));
	    if (fabs(c) < TOL*TOL) {
		bu_log("Get_uv_curve: cannot evaluate parabola entity D%07d\n",
		       dir[entity_no]->direct);
		return (struct edge_g_cnurb *)NULL;
	    }
	    b = b/c;
	    t1 = (b + c)/2.0;	/* value of 't' at v1 */
	    t2 = (b - c)/2.0;	/* value of 't' at v2 */
	    xc = v1[0] - a*t1*t1*cos(theta) + t1*sin(theta);
	    yc = v1[1] - a*t1*t1*sin(theta) - t1*cos(theta);

	    uv = (fastf_t *)bu_calloc((size_t)num_points * 2, sizeof(fastf_t),
				      "Get_uv_curve: parabola uv");

	    dpi = (t2-t1)/(double)(num_points - 1);	/* parameter increment */
	    b = cos(theta);
	    c = sin(theta);
	    for (i = 0; i < num_points; i++) {
		point_t pt, pt2;

		r1 = t1 + dpi*i;
		pt[X] = xc + a*r1*r1*b - r1*c + u_translation;
		pt[Y] = yc + a*r1*r1*c + r1*b + v_translation;
		pt[Z] = 0.0;

		MAT4X3PNT(pt2, *dir[entity_no]->rot, pt);
		uv[idx*2] = pt2[X];
		uv[idx*2 + 1] = pt2[Y];
		idx++;
	    }
	    num_pts = idx;

	} else {
	    /* ellipse (type 1) or hyperbola (type 2) */
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

	    if (type == 2 && F1/A1 > 0.0)
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

	    if (type == 1) {
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
	    bn_mat_mul(rot2, *(dir[entity_no]->rot), rot1);
#else
	    Matmult(*(dir[entity_no]->rot), rot1, rot2);
#endif

	    uv = (fastf_t *)bu_calloc((size_t)(num_points + 1) * 2, sizeof(fastf_t),
				      "Get_uv_curve: conic uv");

	    for (i = 0; i <= num_points; i++) {
		point_t tmp2 = VINIT_ZERO;
		point_t pt2;

		theta = alpha + (double)i/(double)num_points*beta;
		if (type == 2) {
		    tmp2[0] = a*cosh(theta);
		    tmp2[1] = b*sinh(theta);
		} else {
		    tmp2[0] = a*cos(theta);
		    tmp2[1] = b*sin(theta);
		}
		tmp2[2] = 0.0;

		MAT4X3PNT(pt2, rot2, tmp2);
		uv[idx*2] = pt2[X] + u_translation;
		uv[idx*2 + 1] = pt2[Y] + v_translation;
		idx++;
	    }
	    num_pts = idx;
	}

    } else {
	bu_log("Get_uv_curve: expected conic arc (104) or parametric spline (112), got type %d\n",
	       entity_type);
	return (struct edge_g_cnurb *)NULL;
    }

    if (!uv || num_pts < 2) {
	if (uv)
	    bu_free(uv, "Get_uv_curve: uv");
	bu_log("Get_uv_curve: entity D%07d produced too few points (%d)\n",
	       dir[entity_no]->direct, num_pts);
	return (struct edge_g_cnurb *)NULL;
    }

    crv = Make_uv_polyline_cnurb(uv, num_pts);
    bu_free(uv, "Get_uv_curve: uv");

    return crv;
}


void
Assign_vu_geom(struct vertexuse *vu, fastf_t u, fastf_t v, struct face_g_snurb *srf)
{
    point_t uvw;
    hpoint_t pt_on_srf = HINIT_ZERO;
    struct vertexuse *vu1;
    int moved = 0;

    NMG_CK_VERTEXUSE(vu);
    NMG_CK_SNURB(srf);

    if (u < srf->u.knots[0] || v < srf->v.knots[0] ||
	u > srf->u.knots[srf->u.k_size-1] || v > srf->v.knots[srf->v.k_size-1]) {
	bu_log("WARNING: UV point outside of domain of surface!:\n");
	bu_log("\tUV = (%g %g)\n", u, v);
	bu_log("\tsrf domain: (%g %g) <-> (%g %g)\n",
	       srf->u.knots[0], srf->v.knots[0],
	       srf->u.knots[srf->u.k_size-1], srf->v.knots[srf->v.k_size-1]);

	CLAMP(u, srf->u.knots[0], srf->u.knots[srf->u.k_size-1]);
	CLAMP(v, srf->v.knots[0], srf->v.knots[srf->v.k_size-1]);

	moved = 1;
    }

    nmg_nurb_s_eval(srf, u, v, pt_on_srf);
    if (RT_NURB_IS_PT_RATIONAL(srf->pt_type)) {
	fastf_t sca;

	sca = 1.0/pt_on_srf[3];
	VSCALE(pt_on_srf, pt_on_srf, sca);
    }
    if (moved) {
	bu_log("\tMoving VU geom to (%g %g %g) new UV = (%g %g)\n",
	       V3ARGS(pt_on_srf), u, v);
    }

    nmg_vertex_gv(vu->v_p, pt_on_srf);
    /* TODO: Need w coord! */
    VSET(uvw, u, v, 1.0);

    for (BU_LIST_FOR(vu1, vertexuse, &vu->v_p->vu_hd))
	nmg_vertexuse_a_cnurb(vu1, uvw);
}


void
Add_trim_curve(int entity_no, struct loopuse *lu, struct face_g_snurb *srf)
{
    int entity_type;
    struct edge_g_cnurb *crv;
    struct edgeuse *eu;
    struct edgeuse *new_eu;
    struct vertex *vp;
    double x, y, z;
    point_t pt, pt2;
    int ncoords;
    int i;

    NMG_CK_LOOPUSE(lu);
    NMG_CK_SNURB(srf);

    if (dir[entity_no]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n",
	       dir[entity_no]->direct, dir[entity_no]->name);
	return;
    }

    Readrec(dir[entity_no]->param);
    Readint(&entity_type, "");

    switch (entity_type) {
	case 102: {
	    /* composite curve */
	    int curve_count;
	    int *curve_list;

	    Readint(&curve_count, "");
	    curve_list = (int *)bu_calloc(curve_count, sizeof(int),
					  "Add_trim_curve: curve_list");

	    for (i = 0; i < curve_count; i++)
		Readint(&curve_list[i], "");

	    for (i = 0; i < curve_count; i++)
		Add_trim_curve(IGES_DE2INDEX(curve_list[i]), lu, srf);

	    bu_free(curve_list, "Add_trim_curve: curve_list");
	}
	    break;
	case 110:	/* line */
			/* get start point */
	    Readdbl(&x, "");
	    Readdbl(&y, "");
	    Readdbl(&z, "");
	    VSET(pt, x + u_translation, y + v_translation, z);

	    /* apply transformation */
	    MAT4X3PNT(pt2, *dir[entity_no]->rot, pt);

	    /* Split last edge in loop */
	    eu = BU_LIST_LAST(edgeuse, &lu->down_hd);
	    if (!eu) {
		break;
	    }
	    new_eu = nmg_eusplit((struct vertex *)NULL, eu, 0);
	    vp = eu->vu_p->v_p;

	    /* if old edge doesn't have vertex geometry, assign some */
	    if (!vp->vg_p)
		Assign_vu_geom(eu->vu_p, pt2[X], pt2[Y], srf);

	    /* read terminate point */
	    Readdbl(&x, "");
	    Readdbl(&y, "");
	    Readdbl(&z, "");
	    VSET(pt, x + u_translation, y + v_translation, z);

	    /* apply transformation */
	    MAT4X3PNT(pt2, *dir[entity_no]->rot, pt);

	    Assign_vu_geom(new_eu->vu_p, pt2[X], pt2[Y], srf);

	    /* assign edge geometry */
	    nmg_edge_g_cnurb_plinear(eu);
	    break;
	case 100: {
	    /* circular arc */
	    point_t center, start, end;

	    /* read Arc center start and end points */
	    Readdbl(&z, "");	/* common Z-coord */
	    Readdbl(&x, "");	/* center */
	    Readdbl(&y, "");	/* center */
	    VSET(center, x+u_translation, y+v_translation, z);

	    Readdbl(&x, "");	/* start */
	    Readdbl(&y, "");	/* start */
	    VSET(start, x+u_translation, y+v_translation, z);

	    Readdbl(&x, "");	/* end */
	    Readdbl(&y, "");	/* end */
	    VSET(end, x+u_translation, y+v_translation, z);

		/* build edge_g_cnurb arc */
	    crv = nmg_arc2d_to_cnurb(center, start, end, RT_NURB_PT_UV, &tol);

	    /* apply transformation to control points */
	    for (i = 0; i < crv->c_size; i++) {
		V2MOVE(pt2, &crv->ctl_points[i*3]);
		pt2[Z] = z;
		MAT4X3PNT(pt, *dir[entity_no]->rot, pt2);
		V2MOVE(&crv->ctl_points[i*3], pt);
	    }

	    /* add a new edge to loop */
	    eu = BU_LIST_LAST(edgeuse, &lu->down_hd);
	    new_eu = nmg_eusplit((struct vertex *)NULL, eu, 0);
	    vp = eu->vu_p->v_p;

	    /* if old edge doesn't have vertex geometry, assign some */
	    if (!vp->vg_p)
		Assign_vu_geom(eu->vu_p,
			       crv->ctl_points[0], crv->ctl_points[1], srf);

	    /* Assign geometry to new vertex */
	    Assign_vu_geom(new_eu->vu_p,
			   crv->ctl_points[(crv->c_size-1)*3],
			   crv->ctl_points[(crv->c_size-1)*3 + 1],
			   srf);

	    /* Assign edge geometry */
	    Assign_cnurb_to_eu(eu, crv);

	    nmg_nurb_free_cnurb(crv);
	}
	    break;

	case 104:	/* conic arc */
	case 112:	/* parametric spline curve */
	    /* Evaluate the curve to a UV polyline cnurb */
	    crv = Get_uv_curve(entity_no);
	    if (!crv) {
		/* graceful skip: don't add anything to the loop */
		break;
	    }

	    ncoords = RT_NURB_EXTRACT_COORDS(crv->pt_type);

	    /* add a new edge to loop */
	    eu = BU_LIST_LAST(edgeuse, &lu->down_hd);
	    new_eu = nmg_eusplit((struct vertex *)NULL, eu, 0);
	    vp = eu->vu_p->v_p;

	    /* if old edge doesn't have vertex geometry, assign some */
	    if (!vp->vg_p) {
		Assign_vu_geom(eu->vu_p, crv->ctl_points[0],
			       crv->ctl_points[1], srf);
	    }

	    /* Assign geometry to new vertex */
	    Assign_vu_geom(new_eu->vu_p, crv->ctl_points[(crv->c_size-1)*ncoords],
			   crv->ctl_points[(crv->c_size-1)*ncoords+1], srf);

	    /* Assign edge geometry */
	    Assign_cnurb_to_eu(eu, crv);

	    nmg_nurb_free_cnurb(crv);
	    break;

	case 126:	/* spline curve */
			/* Get spline curve */
	    crv = Get_cnurb(entity_no);

	    ncoords = RT_NURB_EXTRACT_COORDS(crv->pt_type);

	    /* add a new edge to loop */
	    eu = BU_LIST_LAST(edgeuse, &lu->down_hd);
	    new_eu = nmg_eusplit((struct vertex *)NULL, eu, 0);
	    vp = eu->vu_p->v_p;

	    /* if old edge doesn't have vertex geometry, assign some */
	    if (!vp->vg_p) {
		Assign_vu_geom(eu->vu_p, crv->ctl_points[0],
			       crv->ctl_points[1], srf);
	    }

	    /* Assign geometry to new vertex */
	    Assign_vu_geom(new_eu->vu_p, crv->ctl_points[(crv->c_size-1)*ncoords],
			   crv->ctl_points[(crv->c_size-1)*ncoords+1], srf);

	    /* Assign edge geometry */
	    Assign_cnurb_to_eu(eu, crv);

	    nmg_nurb_free_cnurb(crv);
	    break;
	default:
	    bu_log("Curves of type %d are not yet handled for trimmed surfaces\n", entity_type);
	    break;
    }
}


struct loopuse *
Make_trim_loop(int entity_no, int orientation, struct face_g_snurb *srf, struct faceuse *fu)
{
    struct loopuse *lu;
    struct edgeuse *eu;
    struct edgeuse *new_eu;
    struct vertexuse *vu;
    struct vertex *vp;
    int entity_type = 0;
    int ncoords = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    fastf_t u = 0.0;
    fastf_t v = 0.0;
    int i;

    NMG_CK_SNURB(srf);
    NMG_CK_FACEUSE(fu);

    if (dir[entity_no]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n",
	       dir[entity_no]->direct, dir[entity_no]->name);
	return (struct loopuse *)NULL;
    }

    Readrec(dir[entity_no]->param);
    Readint(&entity_type, "");

    lu = nmg_mlv(&fu->l.magic, (struct vertex *)NULL, orientation);

    switch (entity_type) {
	case 102: {
	    /* composite curve */
	    int curve_count;
	    int *curve_list;

	    Readint(&curve_count, "");
	    curve_list = (int *)bu_calloc(curve_count, sizeof(int),
					  "Make_trim_loop: curve_list");

	    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	    (void)nmg_meonvu(vu);

	    for (i = 0; i < curve_count; i++)
		Readint(&curve_list[i], "");

	    for (i = 0; i < curve_count; i++)
		Add_trim_curve(IGES_DE2INDEX(curve_list[i]), lu, srf);

	    bu_free(curve_list, "Make_trim_loop: curve_list");

	    /* if last EU is zero length, kill it */
	    eu = BU_LIST_LAST(edgeuse, &lu->down_hd);
	    if (eu &&
		eu->vu_p &&
		eu->vu_p->v_p &&
		eu->vu_p->v_p->vg_p &&
		eu->eumate_p &&
		eu->eumate_p->vu_p &&
		eu->eumate_p->vu_p->v_p &&
		eu->eumate_p->vu_p->v_p->vg_p &&
		bg_dist_pnt3_pnt3(eu->vu_p->v_p->vg_p->coord, eu->eumate_p->vu_p->v_p->vg_p->coord) < tol.dist) {
		nmg_keu(eu);
	    }
	}
	    break;
	case 100: {
	    /* circular arc (must be full circle here) */
	    struct edge_g_cnurb *crv;
	    struct edge_g_cnurb *crv1, *crv2;
	    point_t center, start, end;
	    struct bu_list curv_hd;

	    /* read Arc center start and end points */
	    Readdbl(&z, "");	/* common Z-coord */
	    Readdbl(&x, "");	/* center */
	    Readdbl(&y, "");	/* center */
	    VSET(center, x+u_translation, y+v_translation, z);

	    Readdbl(&x, "");	/* start */
	    Readdbl(&y, "");	/* start */
	    VSET(start, x+u_translation, y+v_translation, z);

	    Readdbl(&x, "");	/* end */
	    Readdbl(&y, "");	/* end */
	    VSET(end, x+u_translation, y+v_translation, z);

	    /* build edge_g_cnurb circle */
	    crv = nmg_arc2d_to_cnurb(center, start, end, RT_NURB_PT_UV, &tol);

	    /* split circle into two pieces */
	    BU_LIST_INIT(&curv_hd);
	    nmg_nurb_c_split(&curv_hd, crv);
	    crv1 = BU_LIST_FIRST(edge_g_cnurb, &curv_hd);
	    crv2 = BU_LIST_LAST(edge_g_cnurb, &curv_hd);

	    /* Split last edge in loop */
	    eu = BU_LIST_LAST(edgeuse, &lu->down_hd);
	    new_eu = nmg_eusplit((struct vertex *)NULL, eu, 0);
	    vp = eu->vu_p->v_p;

	    /* if old edge doesn't have vertex geometry, assign some */
	    if (!vp->vg_p) {
		u = crv1->ctl_points[0]/crv1->ctl_points[2];
		v = crv1->ctl_points[1]/crv1->ctl_points[2];
		Assign_vu_geom(eu->vu_p, u, v, srf);
	    }

	    vp = new_eu->vu_p->v_p;
	    if (!vp->vg_p) {
		/* Assign geometry to new_eu vertex */
		u = crv2->ctl_points[0]/crv2->ctl_points[2];
		v = crv2->ctl_points[1]/crv2->ctl_points[2];
		Assign_vu_geom(new_eu->vu_p, u, v, srf);
	    }

	    /* Assign edge geometry */
	    Assign_cnurb_to_eu(eu, crv1);
	    Assign_cnurb_to_eu(new_eu, crv2);

	    nmg_nurb_free_cnurb(crv);
	    nmg_nurb_free_cnurb(crv1);
	    nmg_nurb_free_cnurb(crv2);
	}
	    break;

	case 104:	/* conic arc */
	case 112: {
	    /* conic arc / parametric spline (may form the whole loop) */
	    struct edge_g_cnurb *crv;
	    struct edge_g_cnurb *crv1, *crv2;
	    struct bu_list curv_hd;

	    /* Evaluate the curve to a UV polyline cnurb */
	    crv = Get_uv_curve(entity_no);
	    if (!crv) {
		/* graceful skip: leave the (single-vertex) loop as-is */
		break;
	    }

	    ncoords = RT_NURB_EXTRACT_COORDS(crv->pt_type);

	    /* split curve into two pieces so the loop has >1 edge */
	    BU_LIST_INIT(&curv_hd);
	    nmg_nurb_c_split(&curv_hd, crv);
	    crv1 = BU_LIST_FIRST(edge_g_cnurb, &curv_hd);
	    crv2 = BU_LIST_LAST(edge_g_cnurb, &curv_hd);
	    /* Split last edge in loop */
	    eu = BU_LIST_LAST(edgeuse, &lu->down_hd);
	    new_eu = nmg_eusplit((struct vertex *)NULL, eu, 0);
	    vp = eu->vu_p->v_p;

	    /* if old edge doesn't have vertex geometry, assign some */
	    if (!vp->vg_p) {
		/* Don't divide out rational coord */
		if (RT_NURB_IS_PT_RATIONAL(crv1->pt_type)) {
		    u = crv1->ctl_points[0]/crv1->ctl_points[ncoords-1];
		    v = crv1->ctl_points[1]/crv1->ctl_points[ncoords-1];
		} else {
		    u = crv1->ctl_points[0];
		    v = crv1->ctl_points[1];
		}
		Assign_vu_geom(eu->vu_p, u, v, srf);
	    }

	    /* Assign geometry to new_eu vertex */
	    vp = new_eu->vu_p->v_p;
	    if (!vp->vg_p) {
		if (RT_NURB_IS_PT_RATIONAL(crv2->pt_type)) {
		    u = crv2->ctl_points[0]/crv2->ctl_points[ncoords-1];
		    v = crv2->ctl_points[1]/crv2->ctl_points[ncoords-1];
		} else {
		    u = crv2->ctl_points[0];
		    v = crv2->ctl_points[1];
		}
		Assign_vu_geom(new_eu->vu_p, u, v, srf);
	    }

	    /* Assign edge geometry */
	    Assign_cnurb_to_eu(eu, crv1);
	    Assign_cnurb_to_eu(new_eu, crv2);

	    nmg_nurb_free_cnurb(crv);
	    nmg_nurb_free_cnurb(crv1);
	    nmg_nurb_free_cnurb(crv2);
	}
	    break;

	case 126: {
	    /* spline curve (must be closed loop) */
	    struct edge_g_cnurb *crv;
	    struct edge_g_cnurb *crv1, *crv2;
	    struct bu_list curv_hd;

	    /* Get spline curve */
	    crv = Get_cnurb(entity_no);

	    ncoords = RT_NURB_EXTRACT_COORDS(crv->pt_type);

	    /* split circle into two pieces */
	    BU_LIST_INIT(&curv_hd);
	    nmg_nurb_c_split(&curv_hd, crv);
	    crv1 = BU_LIST_FIRST(edge_g_cnurb, &curv_hd);
	    crv2 = BU_LIST_LAST(edge_g_cnurb, &curv_hd);
	    /* Split last edge in loop */
	    eu = BU_LIST_LAST(edgeuse, &lu->down_hd);
	    new_eu = nmg_eusplit((struct vertex *)NULL, eu, 0);
	    vp = eu->vu_p->v_p;

	    /* if old edge doesn't have vertex geometry, assign some */
	    if (!vp->vg_p) {
		/* Don't divide out rational coord */
		if (RT_NURB_IS_PT_RATIONAL(crv1->pt_type)) {
		    u = crv1->ctl_points[0]/crv1->ctl_points[ncoords-1];
		    v = crv1->ctl_points[1]/crv1->ctl_points[ncoords-1];
		} else {
		    u = crv1->ctl_points[0];
		    v = crv1->ctl_points[1];
		}
		Assign_vu_geom(eu->vu_p, u, v, srf);
	    }

	    /* Assign geometry to new_eu vertex */
	    vp = new_eu->vu_p->v_p;
	    if (!vp->vg_p) {
		if (RT_NURB_IS_PT_RATIONAL(crv2->pt_type)) {
		    u = crv2->ctl_points[0]/crv2->ctl_points[ncoords-1];
		    v = crv2->ctl_points[1]/crv2->ctl_points[ncoords-1];
		} else {
		    u = crv2->ctl_points[0];
		    v = crv2->ctl_points[1];
		}
		Assign_vu_geom(new_eu->vu_p, u, v, srf);
	    }

	    /* Assign edge geometry */
	    Assign_cnurb_to_eu(eu, crv1);
	    Assign_cnurb_to_eu(new_eu, crv2);

	    nmg_nurb_free_cnurb(crv);
	    nmg_nurb_free_cnurb(crv1);
	    nmg_nurb_free_cnurb(crv2);
	}
	    break;
	default:
	    bu_log("Curves of type %d are not yet handled for trimmed surfaces\n", entity_type);
	    break;
    }

    /* make sure they form a loop geometrically and topologically */
    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
    new_eu = BU_LIST_LAST(edgeuse, &lu->down_hd);

    /* FIXME: total hack to associate last with first if we somehow
     * end up without geometry on the last edge use.
     */
    if (new_eu && !new_eu->g.magic_p) {
	new_eu->g = eu->g; /* struct copy */
	bu_log("Fixing entity %d\n", entity_no);
    }

    return lu;
}


struct loopuse *
Make_loop(size_t entity_no, int orientation, int on_surf_de, struct face_g_snurb *srf, struct faceuse *fu)
{
    struct loopuse *lu;
    int entity_type = 0;
    int surf_de = 0;
    int param_curve_de = 0;
    int model_curve_de = 0;
    int i = 0;

    NMG_CK_SNURB(srf);
    NMG_CK_FACEUSE(fu);

    /* Acquiring Data */
    if (dir[entity_no]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n",
	       dir[entity_no]->direct, dir[entity_no]->name);
	return 0;
    }

    Readrec(dir[entity_no]->param);
    Readint(&entity_type, "");
    if (entity_type != 142) {
	bu_log("Expected Curve on a Parametric Surface, found %s\n", iges_type(entity_type));
	return 0;
    }
    Readint(&i, "");
    Readint(&surf_de, "");
    if (surf_de != on_surf_de)
	bu_log("Curve is on surface at DE %d, should be on surface at DE %d\n", surf_de, on_surf_de);

    Readint(&param_curve_de, "");
    Readint(&model_curve_de, "");

    lu = Make_trim_loop(IGES_DE2INDEX(param_curve_de), orientation, srf, fu);

    return lu;
}


struct loopuse *
Make_default_loop(struct face_g_snurb *srf, struct faceuse *fu)
{
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertexuse *vu;
    fastf_t *knots;
    fastf_t u = 0, v = 0;
    fastf_t *ctl_points;
    int edge_no = 0;
    int pt_type;
    int ncoords;
    int planar = 0;
    int i;

    NMG_CK_FACEUSE(fu);
    NMG_CK_SNURB(srf);

    pt_type = RT_NURB_MAKE_PT_TYPE(2, RT_NURB_PT_UV, RT_NURB_PT_NONRAT);
    ncoords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

    if (srf->order[0] < 3 && srf->order[1] < 3)
	planar = 1;

    lu = nmg_mlv(&fu->l.magic, (struct vertex *)NULL, OT_SAME);
    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
    eu = nmg_meonvu(vu);
    for (i = 0; i < 3; i++)
	(void)nmg_eusplit((struct vertex *)NULL, eu, 0);


    /* assign vertex geometry */
    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	int u_index = 0, v_index = 0;

	NMG_CK_EDGEUSE(eu);
	vu = eu->vu_p;
	NMG_CK_VERTEXUSE(vu);
	switch (edge_no) {
	    case 0:
		u_index = 0;
		v_index = 0;
		u = srf->u.knots[0];
		v = srf->v.knots[0];
		break;
	    case 1:
		u_index = srf->s_size[0] - 1;
		v_index = 0;
		u = srf->u.knots[srf->u.k_size - 1];
		v = srf->v.knots[0];
		break;
	    case 2:
		u_index = srf->s_size[0] - 1;
		v_index = srf->s_size[1] - 1;
		u = srf->u.knots[srf->u.k_size - 1];
		v = srf->v.knots[srf->v.k_size - 1];
		break;
	    case 3:
		u_index = 0;
		v_index = srf->s_size[0] - 1;
		u = srf->u.knots[0];
		v = srf->v.knots[srf->v.k_size - 1];
		break;
	}
	nmg_vertex_gv(vu->v_p, &srf->ctl_points[(u_index*srf->s_size[1] + v_index)*ncoords]);
	Assign_vu_geom(vu, u, v, srf);
	edge_no++;
    }

    /* assign edge geometry */
    edge_no = 0;
    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	NMG_CK_EDGEUSE(eu);
	vu = eu->vu_p;
	NMG_CK_VERTEXUSE(vu);
	if (planar) {
	    nmg_edge_g(eu);
	} else {
	    ctl_points = (fastf_t *)bu_calloc(sizeof(fastf_t), 4, "ctl_points");
	    switch (edge_no) {
		case 0:
		    ctl_points[0] = 0.0;
		    ctl_points[1] = 0.0;
		    ctl_points[2] = 1.0;
		    ctl_points[3] = 0.0;
		    break;
		case 1:
		    ctl_points[0] = 1.0;
		    ctl_points[1] = 0.0;
		    ctl_points[2] = 1.0;
		    ctl_points[3] = 1.0;
		    break;
		case 2:
		    ctl_points[0] = 1.0;
		    ctl_points[1] = 1.0;
		    ctl_points[2] = 0.0;
		    ctl_points[3] = 1.0;
		    break;
		case 3:
		    ctl_points[0] = 0.0;
		    ctl_points[1] = 1.0;
		    ctl_points[2] = 0.0;
		    ctl_points[3] = 0.0;
		    break;
	    }
	    knots = (fastf_t *)bu_calloc(sizeof(fastf_t), 4, "knots");
	    knots[0] = 0.0;
	    knots[1] = 0.0;
	    knots[2] = 1.0;
	    knots[3] = 1.0;
	    nmg_edge_g_cnurb(eu, 2, 4, knots, 2, pt_type, ctl_points);
	}
	edge_no++;
    }

    return lu;
}


void
Assign_surface_to_fu(struct faceuse *fu, struct face_g_snurb *srf)
{
    struct face *f;

    NMG_CK_FACEUSE(fu);
    NMG_CK_SNURB(srf);

    f = fu->f_p;
    NMG_CK_FACE(f);

    if (f->g.snurb_p)
	bu_exit(1, "Assign_surface_to_fu: fu already has geometry\n");

    fu->orientation = OT_SAME;
    fu->fumate_p->orientation = OT_OPPOSITE;

    f->g.snurb_p = srf;
    f->flip = 0;
    BU_LIST_APPEND(&srf->f_hd, &f->l);
}


struct faceuse *
trim_surf(size_t entityno, struct shell *s)
{
    struct model *m;
    struct face_g_snurb *srf;
    struct faceuse *fu;
    struct loopuse *lu;
    struct loopuse *kill_lu;
    struct vertex *verts[3];
    int entity_type = 0;
    int surf_de = 0;
    int has_outer_boundary, inner_loop_count, outer_loop;
    int *inner_loop = NULL;
    int i;
    int lu_uv_orient;

    NMG_CK_SHELL(s);

    m = nmg_find_model(&s->l.magic);
    NMG_CK_MODEL(m);

    /* Acquiring Data */
    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n",
	       dir[entityno]->direct, dir[entityno]->name);
	return 0;
    }

    Readrec(dir[entityno]->param);
    Readint(&entity_type, "");
    if (entity_type != 144) {
	bu_log("Expected Trimmed Surface Entity found type %d\n", entity_type);
	return (struct faceuse *)NULL;
    }
    Readint(&surf_de, "");
    Readint(&has_outer_boundary, "");
    Readint(&inner_loop_count, "");
    Readint(&outer_loop, "");
    if (inner_loop_count) {
	inner_loop = (int *)bu_calloc(inner_loop_count, sizeof(int), "trim_surf: innerloop");
	for (i = 0; i < inner_loop_count; i++)
	    Readint(&inner_loop[i], "");
    }

    if (dir[IGES_DE2INDEX(surf_de)]->type == 128) {
	srf = Get_nurb_surf(IGES_DE2INDEX(surf_de), m);
    } else {
	/* Analytic / spline base surface (118/120/122/140/114): build it via
	 * OpenNURBS.  These surfaces use their own (unshifted) parameter
	 * domain, so clear the knot translations that Get_nurb_surf would
	 * otherwise set for the trim curves. */
	u_translation = 0.0;
	v_translation = 0.0;
	srf = Get_iges_nurb_surf(IGES_DE2INDEX(surf_de), m);
    }
    if (srf == (struct face_g_snurb *)NULL) {
	if (inner_loop_count)
	    bu_free(inner_loop, "trim_surf: inner_loop");
	return (struct faceuse *)NULL;
    }

    /* Make a face (with a loop to be destroyed later)
     * because loop routines insist that face and face geometry
     * must already be assigned
     */
    for (i = 0; i < 3; i++)
	verts[i] = (struct vertex *)NULL;

    fu = nmg_cface(s, verts, 3);
    Assign_surface_to_fu(fu, srf);

    kill_lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);

    if (!has_outer_boundary) {
	lu = Make_default_loop(srf, fu);
    } else {
	lu = Make_loop(IGES_DE2INDEX(outer_loop), OT_SAME, surf_de, srf, fu);
    }

    (void)nmg_klu(kill_lu);

    /* first loop is an outer loop, orientation must be OT_SAME */

    lu_uv_orient = nmg_snurb_calc_lu_uv_orient(lu);
    if (lu_uv_orient == OT_SAME)
	nmg_set_lu_orientation(lu, 0);
    else {
	nmg_reverse_face(fu);
	nmg_set_lu_orientation(lu, 0);
    }

    for (i = 0; i < inner_loop_count; i++) {
	lu = Make_loop(IGES_DE2INDEX(inner_loop[i]), OT_OPPOSITE, surf_de, srf, fu);

	/* These loops must all be OT_OPPOSITE */
	lu_uv_orient = nmg_snurb_calc_lu_uv_orient(lu);
	if ((lu_uv_orient == OT_OPPOSITE && !fu->f_p->flip) ||
	    (lu_uv_orient == OT_SAME && fu->f_p->flip))
	    continue;

	/* loop is in wrong direction, exchange lu and lu_mate */
	BU_LIST_DEQUEUE(&lu->l);
	BU_LIST_DEQUEUE(&lu->lumate_p->l);
	BU_LIST_APPEND(&fu->lu_hd, &lu->lumate_p->l);
	lu->lumate_p->up.fu_p = fu;
	BU_LIST_APPEND(&fu->fumate_p->lu_hd, &lu->l);
	lu->up.fu_p = fu->fumate_p;
    }

    if (inner_loop_count)
	bu_free(inner_loop, "trim_surf: inner_loop");

    NMG_CK_FACE_G_SNURB(fu->f_p->g.snurb_p);

    return fu;
}


int
uv_in_fu(fastf_t u, fastf_t v, struct faceuse *fu)
{
    int ot_sames, ot_opps;
    struct loopuse *lu;

    /* check if point is in face (trimming curves) */
    ot_sames = 0;
    ot_opps = 0;

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	if (lu->orientation == OT_SAME)
	    ot_sames += nmg_uv_in_lu(u, v, lu);
	else if (lu->orientation == OT_OPPOSITE)
	    ot_opps += nmg_uv_in_lu(u, v, lu);
	else {
	    bu_log("isect_ray_snurb_face: lu orientation = %s!\n",
		   nmg_orientation(lu->orientation));
	    bu_exit(1, "isect_ray_snurb_face: bad lu orientation\n");
	}
    }

    if (ot_sames == 0 || ot_opps == ot_sames) {
	/* not a hit */
	return 0;
    } else {
	/* this is a hit */
	return 1;
    }
}


/* find all the intersections of fu along line through "mid_pt",
 * in direction "ray_dir". Place hits on "hit_list"
 */
void
find_intersections(struct faceuse *fu, point_t mid_pt, vect_t ray_dir, struct bu_list *hit_list)
{
    plane_t pl1, pl2;
    struct bu_list bezier_l;
    struct face *f;
    struct face_g_snurb *fg;

    NMG_CK_FACEUSE(fu);

    f = fu->f_p;

    if (*f->g.magic_p != NMG_FACE_G_SNURB_MAGIC)
	bu_exit(1, "ERROR: find_intersections(): face is not a TNURB surface!\n");

    fg = f->g.snurb_p;

    bn_vec_ortho(pl2, ray_dir);
    VCROSS(pl1, pl2, ray_dir);
    pl1[W] = VDOT(mid_pt, pl1);
    pl2[W] = VDOT(mid_pt, pl2);

    BU_LIST_INIT(&bezier_l);

    nmg_nurb_bezier(&bezier_l, fg);
    while (BU_LIST_NON_EMPTY(&bezier_l)) {
	struct face_g_snurb *srf;
	struct nmg_nurb_uv_hit *hp;

	srf = BU_LIST_FIRST(face_g_snurb,  &bezier_l);
	BU_LIST_DEQUEUE(&srf->l);

	hp = nmg_nurb_intersect(srf, pl1, pl2, UV_TOL, NULL, 0);
	/* process each hit point */
	while (hp != (struct nmg_nurb_uv_hit *)NULL) {
	    struct nmg_nurb_uv_hit *next;
	    struct snurb_hit *myhit;
	    vect_t to_hit;
	    fastf_t homo_hit[4];

	    next = hp->next;

	    if (nmg_debug & NMG_DEBUG_RT_ISECT)
		bu_log("\tintersect snurb surface at uv=(%g %g)\n", hp->u, hp->v);

	    /* check if point is in face (trimming curves) */
	    if (!uv_in_fu(hp->u, hp->v, fu)) {
		/* not a hit */

		if (nmg_debug & NMG_DEBUG_RT_ISECT)
		    bu_log("\tNot a hit\n");

		bu_free(hp, "nurb_uv_hit");
		hp = next;
		continue;
	    }

	    BU_ALLOC(myhit, struct snurb_hit);
	    BU_LIST_INIT(&myhit->l);
	    myhit->f = f;

	    /* calculate actual hit point (x y z) */
	    nmg_nurb_s_eval(srf, hp->u, hp->v, homo_hit);
	    if (RT_NURB_IS_PT_RATIONAL(srf->pt_type)) {
		fastf_t inv_homo;

		inv_homo = 1.0/homo_hit[3];
		VSCALE(myhit->pt, homo_hit, inv_homo);
	    } else {
		VMOVE(myhit->pt, homo_hit);
	    }

	    VSUB2(to_hit, myhit->pt, mid_pt);
	    myhit->dist = VDOT(to_hit, ray_dir);

	    /* get surface normal */
	    nmg_nurb_s_norm(srf, hp->u, hp->v, myhit->norm);

	    /* may need to reverse it */
	    if (f->flip) {
		VREVERSE(myhit->norm, myhit->norm);
	    }

	    /* add hit to list */
	    if (BU_LIST_IS_EMPTY(hit_list)) {
		BU_LIST_APPEND(hit_list, &myhit->l);
	    } else {
		struct snurb_hit *tmp;

		for (BU_LIST_FOR(tmp, snurb_hit, hit_list)) {
		    if (tmp->dist >= myhit->dist) {
			BU_LIST_INSERT(&tmp->l, &myhit->l);
			break;
		    }
		}
		if (myhit->l.forw == (struct bu_list *)0) {
		    BU_LIST_INSERT(hit_list, &myhit->l);
		}
	    }

	    bu_free(hp, "nurb_uv_hit");
	    hp = next;
	}
	nmg_nurb_free_snurb(srf);
    }
}


/* adjust flip flag on faces using hit list data */
void
adjust_flips(struct bu_list *hit_list, vect_t ray_dir)
{
    struct snurb_hit *hit;
    int enter = 0;
    fastf_t prev_dist = (-MAX_FASTF);

    for (BU_LIST_FOR(hit, snurb_hit, hit_list)) {
	fastf_t dot;

	if (!NEAR_EQUAL(hit->dist, prev_dist, tol.dist))
	    enter = !enter;

	dot = VDOT(hit->norm, ray_dir);

	if ((enter && dot > 0.0) || (!enter && dot < 0.0)) {
	    struct snurb_hit *tmp;

	    /* reverse this face */
	    hit->f->flip = !(hit->f->flip);
	    for (BU_LIST_FOR(tmp, snurb_hit, hit_list)) {
		if (tmp->f == hit->f) {
		    VREVERSE(tmp->norm, tmp->norm);
		}
	    }
	}

	prev_dist = hit->dist;
    }
}


/* Find a uv point that is actually in the face */
int
Find_uv_in_fu(fastf_t *u_in, fastf_t *v_in, struct faceuse *fu)
{
    struct face *f;
    struct face_g_snurb *fg;
    struct loopuse *lu;
    fastf_t umin, umax, vmin, vmax;
    fastf_t u, v;
    int i;

    NMG_CK_FACEUSE(fu);

    f = fu->f_p;

    if (*f->g.magic_p != NMG_FACE_G_SNURB_MAGIC)
	return 1;

    fg = f->g.snurb_p;

    umin = fg->u.knots[0];
    umax = fg->u.knots[fg->u.k_size-1];
    vmin = fg->v.knots[0];
    vmax = fg->v.knots[fg->v.k_size-1];

    /* first try the center of the uv-plane */
    u = (umin + umax)/2.0;
    v = (vmin + vmax)/2.0;

    if (uv_in_fu(u, v, fu)) {
	*u_in = u;
	*v_in = v;
	return 0;
    }

    /* no luck, try a few points along the diagonals of the UV plane */
    for (i = 0; i < 10; i++) {
	u = umin + (umax - umin)*((double)i + 1.0)/11.0;
	v = vmin + (vmax - vmin)*((double)i + 1.0)/11.0;

	if (uv_in_fu(u, v, fu)) {
	    *u_in = u;
	    *v_in = v;
	    return 0;
	}
    }

    /* try other diagonal */
    for (i = 0; i < 10; i++) {
	u = umin + (umax - umin)*((double)i + 1)/11.0;
	v = vmax - (vmax - vmin)*((double)i + 1)/11.0;

	if (uv_in_fu(u, v, fu)) {
	    *u_in = u;
	    *v_in = v;
	    return 0;
	}
    }

    /* last resort, look at loops */
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	struct edgeuse *eu;

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct edge_g_cnurb *eg;

	    if (!eu || !eu->g.magic_p || *eu->g.magic_p != NMG_EDGE_G_CNURB_MAGIC)
		continue;

	    eg = eu->g.cnurb_p;

	    if (RT_NURB_IS_PT_RATIONAL(eg->pt_type)) {
		u = eu->vu_p->a.cnurb_p->param[0] / eu->vu_p->a.cnurb_p->param[2];
		v = eu->vu_p->a.cnurb_p->param[1] / eu->vu_p->a.cnurb_p->param[2];
	    } else {
		u = eu->vu_p->a.cnurb_p->param[0];
		v = eu->vu_p->a.cnurb_p->param[1];
	    }

	    if (uv_in_fu(u, v, fu)) {
		*u_in = u;
		*v_in = v;
		return 0;
	    }
	}
    }

    return 1;
}


/* find an xyz pt that s actually in the face
 * and the surface normal at that point
 */
int
Find_pt_in_fu(struct faceuse *fu, point_t pt, vect_t norm)
{
    struct face *f;
    struct face_g_snurb *fg;
    fastf_t u, v;
    fastf_t homo_hit[4];

    NMG_CK_FACEUSE(fu);

    f = fu->f_p;

    if (*f->g.magic_p != NMG_FACE_G_SNURB_MAGIC)
	return 1;

    fg = f->g.snurb_p;

    if (Find_uv_in_fu(&u, &v, fu))
	return 1;

    /* calculate actual hit point (x y z) */
    nmg_nurb_s_eval(fg, u, v, homo_hit);
    if (RT_NURB_IS_PT_RATIONAL(fg->pt_type)) {
	fastf_t inv_homo;

	inv_homo = 1.0/homo_hit[3];
	VSCALE(pt, homo_hit, inv_homo);
    } else {
	VMOVE(pt, homo_hit);
    }

    /* get surface normal */
    nmg_nurb_s_norm(fg, u, v, norm);

    /* may need to reverse it */
    if (f->flip) {
	VREVERSE(norm, norm);
    }

    return 0;
}


void
Convtrimsurfs(struct bu_list *vlfree)
{

    size_t i;
    size_t convsurf = 0;
    size_t totsurfs = 0;
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct bu_list hit_list;

    bu_log("\n\nConverting Trimmed Surface entities:\n");

    m = nmg_mm();
    r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &r->s_hd);

    for (i = 0; i < totentities; i++) {
	if (dir[i]->type == 144) {
	    totsurfs++;
	    fu = trim_surf(i, s);
	    if (fu) {
		nmg_face_bb(fu->f_p, &tol);
		convsurf++;
	    }
	}
    }

    nmg_rebound(m, &tol);

    bu_log("\n\t%zu surfaces converted, adjusting surface normals....\n", convsurf);

    /* do some raytracing to get face orientations correct */
    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	struct faceuse *fu2;
	point_t mid_pt = VINIT_ZERO;
	vect_t ray_dir;

	if (fu->orientation != OT_SAME)
	    continue;

	BU_LIST_INIT(&hit_list);
	if (Find_pt_in_fu(fu, mid_pt, ray_dir)) {
	    bu_log("Convtrimsurfs: cannot find a point in fu (%p), skipping normal adjustment for this face\n",
		   (void *)fu);
	    continue;
	}

	/* find intersections with all the faces
	 * must include current fu since there
	 * may be more than one intersection
	 */
	for (BU_LIST_FOR(fu2, faceuse, &s->fu_hd)) {
	    if (fu2->orientation != OT_SAME)
		continue;

	    find_intersections(fu2, mid_pt, ray_dir, &hit_list);
	}

	adjust_flips(&hit_list, ray_dir);

	while (BU_LIST_NON_EMPTY(&hit_list)) {
	    struct snurb_hit *myhit;

	    myhit = BU_LIST_FIRST(snurb_hit, &hit_list);
	    BU_LIST_DEQUEUE(&myhit->l);

	    bu_free(myhit, "myhit");
	}

    }

    bu_log("Converted %zu Trimmed Surfaces successfully out of %zu total Trimmed Surfaces\n", convsurf, totsurfs);

    if (convsurf) {
	const char *nm = !BU_STR_EMPTY(curr_file->obj_name) ? curr_file->obj_name : "Trimmed_surf";

	(void)nmg_vertex_fuse(&m->magic, vlfree, &tol);

	/* Preferred output is a faithful rt_brep (OpenNURBS) built from the
	 * trimmed NURBS faces.  Fall back to an NMG solid only when the user
	 * explicitly asked for one (-m/-p); when brep output was requested but
	 * construction fails, write nothing so main() can import the raw
	 * surfaces as an untrimmed brep instead of emitting an unrenderable
	 * NMG solid. */
	if (do_brep && iges_nmg_to_brep(fdout, nm, m, &tol)) {
	    nmg_km(m);		/* brep did not consume the model */
	} else if (!do_brep) {
	    mk_nmg(fdout, nm, m);	/* consumes m */
	} else {
	    bu_log("Faithful brep construction failed for %s; "
		   "deferring to untrimmed-surface import\n", nm);
	    nmg_km(m);
	}
    }

    if (!convsurf) {
	nmg_km(m);
    }
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
