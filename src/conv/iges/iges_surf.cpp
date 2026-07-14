/*                     I G E S _ S U R F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file iges/iges_surf.cpp
 *
 * Read the IGES analytic and spline surface entities that the classic
 * importer's Manifold BREP (type 186) face path currently drops, and
 * build an NMG rational B-spline surface (face_g_snurb) for the face.
 *
 * Each surface is constructed exactly as an OpenNURBS ON_NurbsSurface
 * (through libbrep) and then copied into a face_g_snurb.  That copy is
 * the inverse of snurb_to_on() in iges_brep.cpp: the two clamped end
 * knots that OpenNURBS omits are added back, and rational control points
 * are stored homogeneous (weight-premultiplied), row-major.
 *
 * Supported IGES surface types:
 *
 *   118  Ruled Surface          (linear loft between two curves)
 *   120  Surface of Revolution  (ON_RevSurface -> NURBS)
 *   122  Tabulated Cylinder     (extrude directrix along generatrix)
 *   140  Offset Surface         (base surface offset by a distance)
 *   114  Parametric Spline      (bicubic patches -> bicubic NURBS)
 */

#include "common.h"

/* OpenNURBS (pulls in the full ON_* API through libbrep) */
#include "brep.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/* The IGES importer's globals and readers are C-linkage. */
extern "C" {
#include "./iges_struct.h"
#include "./iges_extern.h"
}

#include "./iges_surf.h"


namespace {


/* Copy an exact ON_NurbsSurface into a freshly allocated NMG
 * face_g_snurb.  This is the inverse of snurb_to_on() in iges_brep.cpp
 * and mirrors the field setup done by Get_nurb_surf() in trimsurf.c.
 *
 * The ON surface stores only the interior knots (KnotCount == order +
 * cv - 2); the NMG snurb wants the two clamped end knots added back, so
 * we bracket the ON knots with SuperfluousKnot().  Rational control
 * points are stored homogeneous (weight-premultiplied); non-rational as
 * plain XYZ.  The control mesh is row-major: index = (v*n_cols +
 * u)*ncoords, with the v index selecting the row. */
struct face_g_snurb *
snurb_from_on(const ON_NurbsSurface *ns, struct model *m)
{
    if (!ns || !m)
	return NULL;

    /* OpenNURBS dimension must be 3 (XYZ) for us to store XYZ(W). */
    if (ns->Dimension() != 3)
	return NULL;

    const int u_order = ns->Order(0);
    const int v_order = ns->Order(1);
    const int n_cols = ns->CVCount(0);	/* u direction, columns */
    const int n_rows = ns->CVCount(1);	/* v direction, rows */
    const bool rational = ns->IsRational() ? true : false;

    if (u_order < 2 || v_order < 2 || n_cols < u_order || n_rows < v_order)
	return NULL;

    const int ncoords = rational ? 4 : 3;
    const int pt_type = rational ?
	RT_NURB_MAKE_PT_TYPE(4, RT_NURB_PT_XYZ, RT_NURB_PT_RATIONAL) :
	RT_NURB_MAKE_PT_TYPE(3, RT_NURB_PT_XYZ, RT_NURB_PT_NONRAT);

    const int n_u = n_cols + u_order;	/* full u knot count */
    const int n_v = n_rows + v_order;	/* full v knot count */

    struct face_g_snurb *srf;
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
    srf->u.knots = (fastf_t *)bu_malloc(n_u * sizeof(fastf_t),
					"snurb_from_on: u knots");
    srf->v.knots = (fastf_t *)bu_malloc(n_v * sizeof(fastf_t),
					"snurb_from_on: v knots");
    srf->s_size[0] = n_rows;
    srf->s_size[1] = n_cols;
    srf->pt_type = pt_type;
    srf->ctl_points = (fastf_t *)bu_malloc(
	sizeof(fastf_t) * n_rows * n_cols * ncoords,
	"snurb_from_on: control mesh points");

    /* u knots: clamped end knot, ON internal knots, clamped end knot */
    srf->u.knots[0] = ns->SuperfluousKnot(0, 0);
    for (int i = 0; i < ns->KnotCount(0); i++)
	srf->u.knots[i + 1] = ns->Knot(0, i);
    srf->u.knots[n_u - 1] = ns->SuperfluousKnot(0, 1);

    /* v knots */
    srf->v.knots[0] = ns->SuperfluousKnot(1, 0);
    for (int i = 0; i < ns->KnotCount(1); i++)
	srf->v.knots[i + 1] = ns->Knot(1, i);
    srf->v.knots[n_v - 1] = ns->SuperfluousKnot(1, 1);

    /* control mesh: row-major, v selects the row, u the column */
    for (int v = 0; v < n_rows; v++) {
	for (int u = 0; u < n_cols; u++) {
	    fastf_t *cv = &srf->ctl_points[(v * n_cols + u) * ncoords];
	    if (rational) {
		ON_4dPoint p;
		ns->GetCV(u, v, p);	/* homogeneous (weighted) */
		cv[0] = p.x;
		cv[1] = p.y;
		cv[2] = p.z;
		cv[3] = p.w;
	    } else {
		ON_3dPoint p;
		ns->GetCV(u, v, p);
		cv[0] = p.x;
		cv[1] = p.y;
		cv[2] = p.z;
	    }
	}
    }

    NMG_CK_FACE_G_SNURB(srf);
    return srf;
}


/* Read an IGES 126 (rational B-spline) curve at directory-array index
 * @p curve into a 3d ON_NurbsCurve in model space.  Mirrors Get_cnurb()
 * in trimsurf.c but keeps full XYZ coordinates and applies the entity's
 * transformation.  Returns a new ON_NurbsCurve (caller owns) or NULL. */
ON_NurbsCurve *
read_bspline_curve_on(size_t curve)
{
    int entity_type = 0;
    int i = 0;
    int num_pts = 0;
    int degree = 0;
    int junk = 0;
    int rational = 0;
    double a = 0.0;

    Readrec(dir[curve]->param);
    Readint(&entity_type, "");
    if (entity_type != 126) {
	bu_log("read_bspline_curve_on: expected spline curve, got type %d\n", entity_type);
	return NULL;
    }

    Readint(&i, "");
    num_pts = i + 1;
    Readint(&degree, "");

    Readint(&junk, "");		/* planar flag */
    Readint(&junk, "");		/* open or closed */
    Readint(&junk, "");		/* polynomial flag (0 => rational) */
    rational = !junk;
    Readint(&junk, "");		/* periodic */

    const int order = degree + 1;
    if (order < 2 || num_pts < order)
	return NULL;

    ON_NurbsCurve *nc = ON_NurbsCurve::New(3, rational ? true : false, order, num_pts);
    if (!nc)
	return NULL;

    /* IGES stores a full (clamped) knot vector of length num_pts+order;
     * ON wants the interior knots only (KnotCount == num_pts+order-2). */
    const int n_knots = num_pts + order;
    double *knots = (double *)bu_malloc(n_knots * sizeof(double),
					"read_bspline_curve_on: knots");
    for (i = 0; i < n_knots; i++)
	Readdbl(&knots[i], "");
    for (i = 0; i < nc->KnotCount(); i++)
	nc->SetKnot(i, knots[i + 1]);
    bu_free(knots, "read_bspline_curve_on: knots");

    /* weights */
    double *wts = (double *)bu_malloc(num_pts * sizeof(double),
				      "read_bspline_curve_on: weights");
    for (i = 0; i < num_pts; i++) {
	Readdbl(&a, "");
	wts[i] = a;
    }

    /* control points (model space, then transformed) */
    for (i = 0; i < num_pts; i++) {
	point_t pt = VINIT_ZERO;
	point_t xpt = VINIT_ZERO;

	Readdbl(&pt[X], "");
	Readdbl(&pt[Y], "");
	Readdbl(&pt[Z], "");

	MAT4X3PNT(xpt, *dir[curve]->rot, pt);

	if (rational)
	    /* ON_4dPoint control vertices are homogeneous (weight-premultiplied) */
	    nc->SetCV(i, ON_4dPoint(xpt[X] * wts[i], xpt[Y] * wts[i], xpt[Z] * wts[i], wts[i]));
	else
	    nc->SetCV(i, ON_3dPoint(xpt[X], xpt[Y], xpt[Z]));
    }

    bu_free(wts, "read_bspline_curve_on: weights");

    return nc;
}


/* Read an IGES 110 (line) at directory-array index @p curve into a
 * degree-1, two control-point ON_NurbsCurve in model space.  Uses
 * Readcnv() so the file-unit scale is applied (matching
 * Get_cnurb_curve()). */
ON_NurbsCurve *
read_line_on(size_t curve)
{
    int entity_type = 0;
    int i = 0;
    point_t p1 = VINIT_ZERO;
    point_t p2 = VINIT_ZERO;
    point_t s1 = VINIT_ZERO;
    point_t e1 = VINIT_ZERO;

    Readrec(dir[curve]->param);
    Readint(&entity_type, "");
    if (entity_type != 110) {
	bu_log("read_line_on: expected line, got type %d\n", entity_type);
	return NULL;
    }

    for (i = 0; i < 3; i++)
	Readcnv(&p1[i], "");
    MAT4X3PNT(s1, *dir[curve]->rot, p1);

    for (i = 0; i < 3; i++)
	Readcnv(&p2[i], "");
    MAT4X3PNT(e1, *dir[curve]->rot, p2);

    ON_NurbsCurve *nc = ON_NurbsCurve::New(3, false, 2, 2);
    if (!nc)
	return NULL;

    nc->SetKnot(0, 0.0);
    nc->SetKnot(1, 1.0);
    nc->SetCV(0, ON_3dPoint(s1[X], s1[Y], s1[Z]));
    nc->SetCV(1, ON_3dPoint(e1[X], e1[Y], e1[Z]));

    return nc;
}


/* Read a referenced IGES curve (by directory-sequence-number DE) into an
 * ON_NurbsCurve.  Handles lines (110) and rational B-spline curves
 * (126).  Returns a new curve (caller owns) or NULL. */
ON_NurbsCurve *
read_curve_on(int curve_de)
{
    const size_t curve = (size_t)((curve_de - 1) / 2);
    if (curve >= dirarraylen) {
	bu_log("read_curve_on: DE=%d out of range (dirarraylen=%zu)\n",
	       curve_de, dirarraylen);
	return NULL;
    }

    switch (dir[curve]->type) {
	case 110:
	    return read_line_on(curve);
	case 126:
	    return read_bspline_curve_on(curve);
	default:
	    bu_log("read_curve_on: curve type %s not yet supported for surface boundary\n",
		   iges_type(dir[curve]->type));
	    return NULL;
    }
}


/* Read a referenced IGES surface (by directory-sequence-number DE) into
 * an ON_NurbsSurface.  Handles rational B-spline surfaces (128) directly
 * and recurses (via the type dispatch) for the analytic types this file
 * builds.  Returns a new surface (caller owns) or NULL. */
ON_NurbsSurface *build_surface_on(size_t entityno);


/* Read an IGES 128 (rational B-spline surface) directly into an
 * ON_NurbsSurface, mirroring Get_nurb_surf() in trimsurf.c but keeping
 * the surface in OpenNURBS form. */
ON_NurbsSurface *
read_bspline_surf_on(size_t entityno)
{
    int entity_type = 0;
    int i = 0;
    int n_cols, n_rows, u_order, v_order;
    int rational;
    double a = 0.0;

    Readrec(dir[entityno]->param);
    Readint(&entity_type, "");
    if (entity_type != 128) {
	bu_log("read_bspline_surf_on: expected B-spline surface, got type %d\n", entity_type);
	return NULL;
    }

    Readint(&i, "");
    n_cols = i + 1;
    Readint(&i, "");
    n_rows = i + 1;
    Readint(&i, "");
    u_order = i + 1;
    Readint(&i, "");
    v_order = i + 1;
    Readint(&i, "");		/* u closed */
    Readint(&i, "");		/* v closed */
    Readint(&i, "");		/* polynomial (0 => rational) */
    rational = !i;
    Readint(&i, "");		/* u periodic */
    Readint(&i, "");		/* v periodic */

    const int n_u = n_cols + u_order;
    const int n_v = n_rows + v_order;

    if (u_order < 2 || v_order < 2 || n_cols < u_order || n_rows < v_order)
	return NULL;

    ON_NurbsSurface *ns =
	ON_NurbsSurface::New(3, rational ? true : false, u_order, v_order, n_cols, n_rows);
    if (!ns)
	return NULL;

    /* u knots: skip the clamped end knots that ON omits */
    double *uk = (double *)bu_malloc(n_u * sizeof(double), "read_bspline_surf_on: uk");
    for (i = 0; i < n_u; i++)
	Readdbl(&uk[i], "");
    for (i = 0; i < ns->KnotCount(0); i++)
	ns->SetKnot(0, i, uk[i + 1]);
    bu_free(uk, "read_bspline_surf_on: uk");

    /* v knots */
    double *vk = (double *)bu_malloc(n_v * sizeof(double), "read_bspline_surf_on: vk");
    for (i = 0; i < n_v; i++)
	Readdbl(&vk[i], "");
    for (i = 0; i < ns->KnotCount(1); i++)
	ns->SetKnot(1, i, vk[i + 1]);
    bu_free(vk, "read_bspline_surf_on: vk");

    /* weights, stored col-fastest (u fastest) as row-major in IGES */
    const int npts = n_cols * n_rows;
    double *wts = (double *)bu_malloc(npts * sizeof(double), "read_bspline_surf_on: wts");
    for (i = 0; i < npts; i++) {
	Readdbl(&a, "");
	wts[i] = a;
    }

    /* control points: IGES lists them with u (column) fastest, v (row)
     * slowest -- index = row*n_cols + col */
    for (int row = 0; row < n_rows; row++) {
	for (int col = 0; col < n_cols; col++) {
	    point_t pt = VINIT_ZERO;
	    point_t xpt = VINIT_ZERO;
	    const int idx = row * n_cols + col;

	    Readdbl(&pt[X], "");
	    Readdbl(&pt[Y], "");
	    Readdbl(&pt[Z], "");

	    MAT4X3PNT(xpt, *dir[entityno]->rot, pt);

	    if (rational)
		/* ON_4dPoint control vertices are homogeneous (weight-premultiplied) */
		ns->SetCV(col, row, ON_4dPoint(xpt[X] * wts[idx], xpt[Y] * wts[idx], xpt[Z] * wts[idx], wts[idx]));
	    else
		ns->SetCV(col, row, ON_3dPoint(xpt[X], xpt[Y], xpt[Z]));
	}
    }

    bu_free(wts, "read_bspline_surf_on: wts");

    return ns;
}


/* --- 122 Tabulated Cylinder -------------------------------------------
 *
 * Parameters: directrix curve DE, then Lx, Ly, Lz -- the end point of
 * the generatrix line whose start is the start point of the directrix.
 * The surface is the directrix extruded along (L - directrix_start).  We
 * build a 2-row (v) control net: row 0 is the directrix control points,
 * row 1 is those points translated by the generatrix direction, degree 1
 * in v. */
ON_NurbsSurface *
build_tabcyl_on(size_t entityno)
{
    int sol_num = 0;
    int curve_de = 0;
    point_t lpt = VINIT_ZERO;
    point_t lx = VINIT_ZERO;

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    Readint(&curve_de, "");
    Readcnv(&lpt[X], "");
    Readcnv(&lpt[Y], "");
    Readcnv(&lpt[Z], "");

    /* the L point is in the surface entity's own space */
    MAT4X3PNT(lx, *dir[entityno]->rot, lpt);

    ON_NurbsCurve *base = read_curve_on(curve_de);
    if (!base) {
	bu_log("build_tabcyl_on: could not read directrix curve (DE=%d)\n", curve_de);
	return NULL;
    }

    /* generatrix direction = L - (start of directrix) */
    const ON_3dPoint start = base->PointAtStart();
    const ON_3dVector genvec(lx[X] - start.x, lx[Y] - start.y, lx[Z] - start.z);

    const bool rational = base->IsRational() ? true : false;
    const int u_order = base->Order();
    const int u_cv = base->CVCount();

    /* v: degree 1 (order 2), 2 control points */
    ON_NurbsSurface *ns =
	ON_NurbsSurface::New(3, rational, u_order, 2, u_cv, 2);
    if (!ns) {
	delete base;
	return NULL;
    }

    /* u knots come from the base curve */
    for (int i = 0; i < base->KnotCount(); i++)
	ns->SetKnot(0, i, base->Knot(i));
    /* v knots: [0, 1] clamped for degree 1 */
    ns->SetKnot(1, 0, 0.0);
    ns->SetKnot(1, 1, 1.0);

    for (int u = 0; u < u_cv; u++) {
	if (rational) {
	    ON_4dPoint p;
	    base->GetCV(u, p);		/* homogeneous */
	    const double w = ZERO(p.w) ? 1.0 : p.w;
	    /* Euclidean location for the offset row */
	    const ON_3dPoint loc(p.x / w, p.y / w, p.z / w);
	    const ON_3dPoint loc2 = loc + genvec;
	    ns->SetCV(u, 0, ON_4dPoint(p.x, p.y, p.z, p.w));
	    ns->SetCV(u, 1, ON_4dPoint(loc2.x * w, loc2.y * w, loc2.z * w, w));
	} else {
	    ON_3dPoint p;
	    base->GetCV(u, p);
	    ns->SetCV(u, 0, p);
	    ns->SetCV(u, 1, p + genvec);
	}
    }

    delete base;
    return ns;
}


/* --- 118 Ruled Surface ------------------------------------------------
 *
 * Parameters: curve1 DE, curve2 DE, DirFlag, DevFlag.  A linear (degree
 * 1 in v) loft between the two curves.  DirFlag == 1 means the second
 * curve is traversed in reverse.  The two boundary curves may have
 * different degree / knot vectors; make them compatible by elevating to
 * a common degree and merging knot vectors (via ON's own machinery),
 * falling back to a point-sampled net if that is unavailable. */
ON_NurbsSurface *
build_ruled_on(size_t entityno)
{
    int sol_num = 0;
    int curve1_de = 0;
    int curve2_de = 0;
    int dirflag = 0;
    int devflag = 0;

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    Readint(&curve1_de, "");
    Readint(&curve2_de, "");
    Readint(&dirflag, "");
    Readint(&devflag, "");

    ON_NurbsCurve *c1 = read_curve_on(curve1_de);
    ON_NurbsCurve *c2 = read_curve_on(curve2_de);
    if (!c1 || !c2) {
	bu_log("build_ruled_on: could not read boundary curves (DE=%d, DE=%d)\n",
	       curve1_de, curve2_de);
	delete c1;
	delete c2;
	return NULL;
    }

    /* DirFlag == 1: reverse the second curve's direction */
    if (dirflag)
	c2->Reverse();

    /* Reparameterize both to [0,1] so their knot vectors are comparable. */
    c1->SetDomain(0.0, 1.0);
    c2->SetDomain(0.0, 1.0);

    /* Make the two curves share order and control-point count.  Elevate
     * the lower-order curve, then merge knot vectors so both have the
     * same knots (and therefore the same CV count). */
    const int order = (c1->Order() > c2->Order()) ? c1->Order() : c2->Order();
    while (c1->Order() < order)
	if (!c1->IncreaseDegree(order - 1))
	    break;
    while (c2->Order() < order)
	if (!c2->IncreaseDegree(order - 1))
	    break;

    if (c1->Order() != c2->Order()) {
	bu_log("build_ruled_on: unable to reconcile boundary curve degrees\n");
	delete c1;
	delete c2;
	return NULL;
    }

    /* Insert each curve's interior knots into the other so they end up
     * with an identical knot vector (and identical CV count). */
    {
	int i;
	for (i = 0; i < c2->KnotCount(); i++) {
	    const double k = c2->Knot(i);
	    if (k > 0.0 && k < 1.0)
		c1->InsertKnot(k, 1);
	}
	for (i = 0; i < c1->KnotCount(); i++) {
	    const double k = c1->Knot(i);
	    if (k > 0.0 && k < 1.0)
		c2->InsertKnot(k, 1);
	}
    }

    if (c1->CVCount() != c2->CVCount()) {
	bu_log("build_ruled_on: boundary curves did not become compatible (%d vs %d CVs)\n",
	       c1->CVCount(), c2->CVCount());
	delete c1;
	delete c2;
	return NULL;
    }

    const bool rational = (c1->IsRational() || c2->IsRational()) ? true : false;
    const int u_order = c1->Order();
    const int u_cv = c1->CVCount();

    ON_NurbsSurface *ns =
	ON_NurbsSurface::New(3, rational, u_order, 2, u_cv, 2);
    if (!ns) {
	delete c1;
	delete c2;
	return NULL;
    }

    for (int i = 0; i < c1->KnotCount(); i++)
	ns->SetKnot(0, i, c1->Knot(i));
    ns->SetKnot(1, 0, 0.0);
    ns->SetKnot(1, 1, 1.0);

    for (int u = 0; u < u_cv; u++) {
	if (rational) {
	    ON_4dPoint p1, p2;
	    c1->GetCV(u, p1);
	    c2->GetCV(u, p2);
	    /* if a curve is non-rational GetCV(4d) yields w==1 */
	    ns->SetCV(u, 0, p1);
	    ns->SetCV(u, 1, p2);
	} else {
	    ON_3dPoint p1, p2;
	    c1->GetCV(u, p1);
	    c2->GetCV(u, p2);
	    ns->SetCV(u, 0, p1);
	    ns->SetCV(u, 1, p2);
	}
    }

    delete c1;
    delete c2;
    return ns;
}


/* --- 120 Surface of Revolution ----------------------------------------
 *
 * Parameters: axis line DE, generatrix curve DE, start angle, end angle
 * (radians).  Build an ON_RevSurface and extract its NURBS form. */
ON_NurbsSurface *
build_revsurf_on(size_t entityno)
{
    int sol_num = 0;
    int axis_de = 0;
    int gen_de = 0;
    double sa = 0.0;
    double ea = 0.0;

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    Readint(&axis_de, "");
    Readint(&gen_de, "");
    Readdbl(&sa, "");
    Readdbl(&ea, "");

    /* the axis is an IGES line: start + end point */
    const size_t axis = (size_t)((axis_de - 1) / 2);
    if (axis >= dirarraylen || dir[axis]->type != 110) {
	bu_log("build_revsurf_on: axis (DE=%d) is not a line\n", axis_de);
	return NULL;
    }

    ON_NurbsCurve *axis_crv = read_line_on(axis);
    if (!axis_crv) {
	bu_log("build_revsurf_on: could not read axis line (DE=%d)\n", axis_de);
	return NULL;
    }
    const ON_3dPoint a0 = axis_crv->PointAtStart();
    const ON_3dPoint a1 = axis_crv->PointAtEnd();
    delete axis_crv;

    ON_NurbsCurve *gen = read_curve_on(gen_de);
    if (!gen) {
	bu_log("build_revsurf_on: could not read generatrix (DE=%d)\n", gen_de);
	return NULL;
    }

    double angle = ea - sa;
    if (angle < 0.0)
	angle = -angle;
    if (angle < ON_ZERO_TOLERANCE || angle > 2.0 * ON_PI)
	angle = 2.0 * ON_PI;

    ON_RevSurface *rev = ON_RevSurface::New();
    if (!rev) {
	delete gen;
	return NULL;
    }
    rev->m_curve = gen;			/* rev takes ownership of gen */
    rev->m_axis = ON_Line(a0, a1);
    rev->m_angle = ON_Interval(0.0, angle);

    ON_NurbsSurface *ns = ON_NurbsSurface::New();
    if (!ns || !rev->GetNurbForm(*ns, 0.0)) {
	bu_log("build_revsurf_on: GetNurbForm failed\n");
	delete rev;			/* also deletes gen */
	if (ns)
	    delete ns;
	return NULL;
    }

    delete rev;				/* also deletes gen */
    return ns;
}


/* --- 140 Offset Surface -----------------------------------------------
 *
 * Parameters: Nx, Ny, Nz (offset indicator), D (distance), then the base
 * surface DE.  Offset the base surface by D along its normal.  If a
 * clean offset is not available, log and return NULL (graceful skip). */
ON_NurbsSurface *
build_offset_on(size_t entityno)
{
    int sol_num = 0;
    double nx = 0.0;
    double ny = 0.0;
    double nz = 0.0;
    double dist = 0.0;
    int surf_de = 0;

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    Readdbl(&nx, "");
    Readdbl(&ny, "");
    Readdbl(&nz, "");
    Readcnv(&dist, "");
    Readint(&surf_de, "");

    const size_t base = (size_t)((surf_de - 1) / 2);
    if (base >= dirarraylen) {
	bu_log("build_offset_on: base surface DE=%d out of range\n", surf_de);
	return NULL;
    }

    ON_NurbsSurface *ns = build_surface_on(base);
    if (!ns) {
	bu_log("build_offset_on: could not read base surface (DE=%d)\n", surf_de);
	return NULL;
    }

    /* Approximate the offset by translating each control point along the
     * surface normal evaluated at the corresponding Greville abscissa.
     * This is exact for planar surfaces and a good approximation for
     * gently curved ones; if the offset cannot be evaluated, fall back
     * to a graceful skip. */
    const int u_cv = ns->CVCount(0);
    const int v_cv = ns->CVCount(1);
    const bool rational = ns->IsRational() ? true : false;

    for (int u = 0; u < u_cv; u++) {
	const double us = ns->GrevilleAbcissa(0, u);
	for (int v = 0; v < v_cv; v++) {
	    const double vs = ns->GrevilleAbcissa(1, v);

	    ON_3dVector du, dv;
	    ON_3dPoint sp;
	    if (!ns->Ev1Der(us, vs, sp, du, dv)) {
		bu_log("build_offset_on: normal evaluation failed, skipping offset surface\n");
		delete ns;
		return NULL;
	    }
	    ON_3dVector n = ON_CrossProduct(du, dv);
	    if (!n.Unitize()) {
		bu_log("build_offset_on: degenerate normal, skipping offset surface\n");
		delete ns;
		return NULL;
	    }

	    if (rational) {
		ON_4dPoint p;
		ns->GetCV(u, v, p);
		const double w = ZERO(p.w) ? 1.0 : p.w;
		const ON_3dPoint loc(p.x / w, p.y / w, p.z / w);
		const ON_3dPoint loc2 = loc + dist * n;
		ns->SetCV(u, v, ON_4dPoint(loc2.x * w, loc2.y * w, loc2.z * w, w));
	    } else {
		ON_3dPoint p;
		ns->GetCV(u, v, p);
		ns->SetCV(u, v, p + dist * n);
	    }
	}
    }

    return ns;
}


/* --- 114 Parametric Spline Surface ------------------------------------
 *
 * Parameters (after the entity type number):
 *   CTYPE  spline boundary type (1 linear, 2 quadratic, 3 cubic,
 *          4/5 Wilson-Fowler, 6 B-spline) -- every coefficient set is
 *          stored as a full bicubic, so we treat them all as degree 3.
 *   PTYPE  patch type (1 = Cartesian).
 *   M      number of u segments.
 *   N      number of v segments.
 *   TU[0..M]   (M+1) u breakpoint values.
 *   TV[0..N]   (N+1) v breakpoint values.
 *   Then, for each patch, the coefficients A(i,j)..S(i,j) for X, then Y,
 *   then Z.  Per the IGES spec the patches are written with the u-segment
 *   index i as the OUTER loop and the v-segment index j as the INNER loop
 *   (running from patch (1,1) to (M,N)).  Within a coordinate block the 16
 *   coefficients are ordered with the t-power (v) grouping outer and the
 *   s-power (u) inner, i.e. the sequence
 *        A  B  C  D   (t^0: s^0 s^1 s^2 s^3)
 *        E  F  G  H   (t^1: s^0 s^1 s^2 s^3)
 *        K  L  M  N   (t^2: s^0 s^1 s^2 s^3)
 *        P  Q  R  S   (t^3: s^0 s^1 s^2 s^3)
 *   so coefficient c[q][p] multiplies s^p * t^q (p is the u/s power, q is
 *   the v/t power), each patch's polynomial being valid over the local
 *   domain s in [0, TU[i+1]-TU[i]], t in [0, TV[j+1]-TV[j]].
 *
 * Each patch is converted from the power basis to a bicubic Bezier (4x4
 * control points) and the M*N patches are assembled into one non-rational
 * bicubic ON_NurbsSurface: order 4 in u and v, interior breakpoints of
 * multiplicity 3 (a C0 join between cubic patches), 3*M+1 control points
 * in u and 3*N+1 in v, with adjacent patches sharing boundary CVs.  Only
 * the common cubic Cartesian case (CTYPE 3, PTYPE 1) is built; other
 * sub-cases are a graceful skip. */
ON_NurbsSurface *
build_spline_surf_on(size_t entityno)
{
    int entity_type = 0;
    int ctype = 0;
    int ptype = 0;
    int M = 0;
    int N = 0;
    int i, j, p, q, c;

    Readrec(dir[entityno]->param);
    Readint(&entity_type, "");
    if (entity_type != 114) {
	bu_log("build_spline_surf_on: expected parametric spline surface, got type %d\n",
	       entity_type);
	return NULL;
    }

    Readint(&ctype, "");
    Readint(&ptype, "");
    Readint(&M, "");
    Readint(&N, "");

    /* Only the cubic Cartesian case is built; anything else is a graceful
     * skip so we never emit wrong geometry.  (Lower-degree coefficient
     * sets are still stored as full bicubics in the file, so treating them
     * as cubic would be exact, but we stay conservative and require the
     * common CTYPE 3 that real files use.) */
    if (ctype != 3) {
	bu_log("build_spline_surf_on: spline boundary type %d not supported (only cubic, CTYPE 3), skipping D%07d (%s)\n",
	       ctype, dir[entityno]->direct, dir[entityno]->name);
	return NULL;
    }
    if (ptype != 1) {
	bu_log("build_spline_surf_on: patch type %d not supported (only Cartesian, PTYPE 1), skipping D%07d (%s)\n",
	       ptype, dir[entityno]->direct, dir[entityno]->name);
	return NULL;
    }
    if (M < 1 || N < 1) {
	bu_log("build_spline_surf_on: illegal segment counts M=%d N=%d\n", M, N);
	return NULL;
    }

    /* breakpoints */
    fastf_t *TU = (fastf_t *)bu_malloc((M + 1) * sizeof(fastf_t),
				       "build_spline_surf_on: TU");
    fastf_t *TV = (fastf_t *)bu_malloc((N + 1) * sizeof(fastf_t),
				       "build_spline_surf_on: TV");
    for (i = 0; i <= M; i++)
	Readflt(&TU[i], "");
    for (j = 0; j <= N; j++)
	Readflt(&TV[j], "");

    /* control-point counts and the ON surface */
    const int ucv = 3 * M + 1;
    const int vcv = 3 * N + 1;

    ON_NurbsSurface *ns = ON_NurbsSurface::New(3, false, 4, 4, ucv, vcv);
    if (!ns) {
	bu_free(TU, "build_spline_surf_on: TU");
	bu_free(TV, "build_spline_surf_on: TV");
	return NULL;
    }

    /* Knot vectors: ON stores the interior knots only (KnotCount ==
     * order + cv - 2).  For a chain of cubic Bezier patches joined C0 the
     * full clamped knot vector is the start breakpoint with
     * multiplicity 4 (== order), each interior breakpoint with
     * multiplicity 3, and the end breakpoint with multiplicity 4.  ON
     * drops one clamped knot from each end, so its stored form is the
     * start breakpoint x3, each interior breakpoint x3, and the end
     * breakpoint x3.
     *
     * u: KnotCount(0) == 4 + ucv - 2 == 3*M + 3 */
    {
	int k = 0;
	ns->SetKnot(0, k++, TU[0]);
	ns->SetKnot(0, k++, TU[0]);
	ns->SetKnot(0, k++, TU[0]);
	for (i = 1; i < M; i++) {
	    ns->SetKnot(0, k++, TU[i]);
	    ns->SetKnot(0, k++, TU[i]);
	    ns->SetKnot(0, k++, TU[i]);
	}
	ns->SetKnot(0, k++, TU[M]);
	ns->SetKnot(0, k++, TU[M]);
	ns->SetKnot(0, k++, TU[M]);
    }
    {
	int k = 0;
	ns->SetKnot(1, k++, TV[0]);
	ns->SetKnot(1, k++, TV[0]);
	ns->SetKnot(1, k++, TV[0]);
	for (j = 1; j < N; j++) {
	    ns->SetKnot(1, k++, TV[j]);
	    ns->SetKnot(1, k++, TV[j]);
	    ns->SetKnot(1, k++, TV[j]);
	}
	ns->SetKnot(1, k++, TV[N]);
	ns->SetKnot(1, k++, TV[N]);
	ns->SetKnot(1, k++, TV[N]);
    }

    /* Read the patches (i outer, j inner) and fill the control net.
     * Each patch is a bicubic power-basis polynomial per coordinate; we
     * convert it to a 4x4 Bezier control-point grid over its local domain
     * and place those into the shared control net at the appropriate
     * 3*i .. 3*i+3 (u) and 3*j .. 3*j+3 (v) block, so adjacent patches
     * share their boundary CVs. */
    for (i = 0; i < M; i++) {
	const fastf_t hu = TU[i + 1] - TU[i];
	for (j = 0; j < N; j++) {
	    const fastf_t hv = TV[j + 1] - TV[j];

	    /* power-basis coefficients: coef[coord][q][p] multiplies
	     * s^p * t^q; coord 0=X 1=Y 2=Z */
	    double coef[3][4][4];
	    for (c = 0; c < 3; c++) {
		for (q = 0; q < 4; q++) {
		    for (p = 0; p < 4; p++) {
			double a = 0.0;
			Readdbl(&a, "");
			coef[c][q][p] = a;
		    }
		}
	    }

	    /* Power-to-Bezier conversion of a cubic on [0,h]:
	     *   b0 = a0
	     *   b1 = a0 + a1*h/3
	     *   b2 = a0 + 2*a1*h/3 + a2*h*h/3
	     *   b3 = a0 + a1*h + a2*h*h + a3*h*h*h
	     * Apply tensor-product-wise: first along u (s / index p) for
	     * each of the 4 t-rows, then along v (t / index q) for each of
	     * the 4 resulting u-columns.  Done per coordinate. */
	    for (c = 0; c < 3; c++) {
		double tmp[4][4];	/* [q][p] after u-conversion */
		double bez[4][4];	/* [q][p] final Bezier control values */

		/* u (s, power index p) conversion, one row per t-power q */
		for (q = 0; q < 4; q++) {
		    const double a0 = coef[c][q][0];
		    const double a1 = coef[c][q][1];
		    const double a2 = coef[c][q][2];
		    const double a3 = coef[c][q][3];
		    tmp[q][0] = a0;
		    tmp[q][1] = a0 + a1 * hu / 3.0;
		    tmp[q][2] = a0 + 2.0 * a1 * hu / 3.0 + a2 * hu * hu / 3.0;
		    tmp[q][3] = a0 + a1 * hu + a2 * hu * hu + a3 * hu * hu * hu;
		}

		/* v (t, power index q) conversion, one column per u-Bezier
		 * index p; the tmp rows are still power-basis in t. */
		for (p = 0; p < 4; p++) {
		    const double a0 = tmp[0][p];
		    const double a1 = tmp[1][p];
		    const double a2 = tmp[2][p];
		    const double a3 = tmp[3][p];
		    bez[0][p] = a0;
		    bez[1][p] = a0 + a1 * hv / 3.0;
		    bez[2][p] = a0 + 2.0 * a1 * hv / 3.0 + a2 * hv * hv / 3.0;
		    bez[3][p] = a0 + a1 * hv + a2 * hv * hv + a3 * hv * hv * hv;
		}

		/* overwrite this coordinate's grid with its Bezier CVs */
		for (q = 0; q < 4; q++)
		    for (p = 0; p < 4; p++)
			coef[c][q][p] = bez[q][p];
	    }

	    /* place the 4x4 Bezier control points (transformed) into the
	     * shared net; p indexes u (columns), q indexes v (rows) */
	    for (p = 0; p < 4; p++) {
		for (q = 0; q < 4; q++) {
		    point_t pt = VINIT_ZERO;
		    point_t xpt = VINIT_ZERO;
		    pt[X] = coef[0][q][p];
		    pt[Y] = coef[1][q][p];
		    pt[Z] = coef[2][q][p];

		    MAT4X3PNT(xpt, *dir[entityno]->rot, pt);

		    ns->SetCV(3 * i + p, 3 * j + q,
			      ON_3dPoint(xpt[X], xpt[Y], xpt[Z]));
		}
	    }
	}
    }

    bu_free(TU, "build_spline_surf_on: TU");
    bu_free(TV, "build_spline_surf_on: TV");

    return ns;
}


/* Dispatch on IGES surface type and build an ON_NurbsSurface. */
ON_NurbsSurface *
build_surface_on(size_t entityno)
{
    if (dir[entityno]->param <= pstart) {
	bu_log("build_surface_on: illegal parameter pointer for entity D%07d (%s)\n",
	       dir[entityno]->direct, dir[entityno]->name);
	return NULL;
    }

    switch (dir[entityno]->type) {
	case 128:
	    return read_bspline_surf_on(entityno);
	case 122:
	    return build_tabcyl_on(entityno);
	case 118:
	    return build_ruled_on(entityno);
	case 120:
	    return build_revsurf_on(entityno);
	case 140:
	    return build_offset_on(entityno);
	case 114:
	    return build_spline_surf_on(entityno);
	default:
	    bu_log("build_surface_on: surface type %s not supported\n",
		   iges_type(dir[entityno]->type));
	    return NULL;
    }
}


} /* anonymous namespace */


extern "C" struct face_g_snurb *
Get_iges_nurb_surf(size_t entityno, struct model *m)
{
    if (!m)
	return NULL;

    if (entityno >= dirarraylen) {
	bu_log("Get_iges_nurb_surf: entity index %zu out of range (dirarraylen=%zu)\n",
	       entityno, dirarraylen);
	return NULL;
    }

    ON::Begin();

    ON_NurbsSurface *ns = build_surface_on(entityno);
    if (!ns)
	return NULL;

    if (!ns->IsValid()) {
	bu_log("Get_iges_nurb_surf: constructed surface for D%07d (%s) is invalid, skipping\n",
	       dir[entityno]->direct, dir[entityno]->name);
	delete ns;
	return NULL;
    }

    struct face_g_snurb *srf = snurb_from_on(ns, m);
    delete ns;

    if (!srf)
	bu_log("Get_iges_nurb_surf: could not convert surface for D%07d (%s)\n",
	       dir[entityno]->direct, dir[entityno]->name);

    return srf;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
