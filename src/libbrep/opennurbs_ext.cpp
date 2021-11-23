/*               O P E N N U R B S _ E X T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2022 United States Government as represented by
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
/** @file opennurbs_ext.cpp
 *
 * Implementation of routines openNURBS left out.
 *
 */

#include "common.h"

#include "bio.h"
#include <assert.h>
#include <vector>

#include "vmath.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "brep/defines.h"
#include "brep/curvetree.h"
#include "brep/surfacetree.h"
#include "brep/ray.h"
#include "brep/pullback.h"
#include "brep/cdt.h" // for ON_Brep_Report_Faces - should go away
#include "bg/tri_ray.h" // for ON_Brep_Report_Faces - should go away
#include "bg/lseg.h"
#include "./tools/tools.h"
#include "bn/dvec.h"

#define RANGE_HI 0.55
#define RANGE_LO 0.45
#define UNIVERSAL_SAMPLE_COUNT 1001

#define BBOX_GROW 0.0

/// grows 3D BBox along each axis by this factor
#define BBOX_GROW_3D 0.1

/// another arbitrary calculation tolerance (need to try VDIVIDE_TOL or VUNITIZE_TOL to tighten the bounds)
#define TOL2 0.00001

int
brep_get_plane_ray(const ON_Ray& r, plane_ray& pr)
{
    vect_t v1;
    VMOVE(v1, r.m_dir);
    fastf_t min = MAX_FASTF;
    int index = -1;
    for (int i = 0; i < 3; i++) {
	// find the smallest component
	if (fabs(v1[i]) < min) {
	    min = fabs(v1[i]);
	    index = i;
	}
    }
    if (index == -1) {
	bu_log("brep_get_plane_ray: error finding smallest component\n");
	return -1;
    }
    v1[index] += 1; // alter the smallest component
    VCROSS(pr.n1, v1, r.m_dir); // n1 is perpendicular to v1
    VUNITIZE(pr.n1);
    VCROSS(pr.n2, pr.n1, r.m_dir);       // n2 is perpendicular to v1 and n1
    VUNITIZE(pr.n2);
    pr.d1 = VDOT(pr.n1, r.m_origin);
    pr.d2 = VDOT(pr.n2, r.m_origin);
    TRACE1("n1:" << ON_PRINT3(pr.n1) << " n2:" << ON_PRINT3(pr.n2) << " d1:" << pr.d1 << " d2:" << pr.d2);
    return 0;
}


void
brep_r(const ON_Surface* surf, const plane_ray& pr, pt2d_t uv, ON_3dPoint& pt, ON_3dVector& su, ON_3dVector& sv, pt2d_t R)
{
    int ret;
    vect_t vp;

    ret = surf->Ev1Der(uv[0], uv[1], pt, su, sv);
    if(!ret) return;

    VMOVE(vp, pt);

    R[0] = VDOT(pr.n1, vp) - pr.d1;
    R[1] = VDOT(pr.n2, vp) - pr.d2;
}


void
brep_newton_iterate(const plane_ray& pr, pt2d_t R, const ON_3dVector& su, const ON_3dVector& sv, pt2d_t uv, pt2d_t out_uv)
{
    vect_t vsu, vsv;
    VMOVE(vsu, su);
    VMOVE(vsv, sv);

    mat2d_t jacob = { VDOT(pr.n1, vsu), VDOT(pr.n1, vsv),
		      VDOT(pr.n2, vsu), VDOT(pr.n2, vsv) };
    mat2d_t inv_jacob;
    if (mat2d_inverse(inv_jacob, jacob)) {
	// check inverse validity
	pt2d_t tmp;
	mat2d_pt2d_mul(tmp, inv_jacob, R);
	pt2dsub(out_uv, uv, tmp);
    } else {
	// FIXME: how to handle this?
	// TRACE2("inverse failed");
	move(out_uv, uv);
    }
}

// This doesn't belong here - just not sure where to put it yet.  By rights
// this should do a ray intersection and report on the faces with that info,
// which means this should really be in libanalyze.  For now, just do a quick
// and dirty bbox check to at least let us narrow down what faces are active
// along a ray.
int
ON_Brep_Report_Faces(struct bu_vls *log, void *bp, const vect_t center, const vect_t dir)
{
    struct bu_vls faces = BU_VLS_INIT_ZERO;
    point_t p1, p2;
    if (!log || !bp) return -1;
    VMOVE(p1, center);
    VADD2(p2, center, dir);
    ON_Line l(ON_3dPoint(p1[X], p1[Y], p1[Z]), ON_3dPoint(p2[X], p2[Y], p2[Z]));

    ON_Brep *brep = (ON_Brep *)bp;
    ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)brep, NULL);
    ON_Brep_CDT_Tessellate(s_cdt, 0, NULL);

    for (int i = 0; i < brep->m_F.Count(); i++) {
	ON_BrepFace &face = brep->m_F[i];
	ON_3dPoint bmin, bmax;
	face.SurfaceOf()->GetBoundingBox(bmin, bmax);
	ON_BoundingBox bb(bmin, bmax);
	double t1, t2;
	if (bb.Intersection(l, &t1, &t2)) {
	    int faces_array = face.m_face_index;
	    int fcnt, vcnt;
	    int *csg_faces;
	    point_t *csg_vertices;
	    ON_Brep_CDT_Mesh(&csg_faces, &fcnt, (fastf_t **)&csg_vertices, &vcnt, NULL, NULL, NULL, NULL, s_cdt, 1, &faces_array);
	    int is_hit = 0;
	    for (int j = 0; j < fcnt; j++) {
		point_t cp1, cp2, cp3, isect;
		VMOVE(cp1, csg_vertices[csg_faces[j*3+0]]);
		VMOVE(cp2, csg_vertices[csg_faces[j*3+1]]);
		VMOVE(cp3, csg_vertices[csg_faces[j*3+2]]);
		is_hit = bg_isect_tri_ray(center, dir, cp1, cp2, cp3, &isect);
		if (is_hit) {
		    if (!bu_vls_strlen(&faces)) {
			bu_vls_printf(&faces, "%d", face.m_face_index);
		    } else {
			bu_vls_printf(&faces, ",%d", face.m_face_index);
		    }
		    break;
		}
	    }
	    bu_free(csg_faces, "free faces");
	    bu_free(csg_vertices, "free faces");
	}
    }

    ON_Brep_CDT_Destroy(s_cdt);
    bu_vls_printf(log, "%s", bu_vls_cstr(&faces));
    bu_vls_free(&faces);

    return 0;
}


/* Implement the closest point on curve algorithm from
 * https://github.com/pboyer/verb - specifically:
 *
 * verb/src/verb/eval/Tess.hx 
 * verb/src/verb/eval/Analyze.hx 
 */

struct curve_search_args {
    const ON_NurbsCurve *nc;
    ON_3dPoint tp;
};

int
curve_closest_search_func(void *farg, double t, double *ft, double *dft)
{
    struct curve_search_args *cargs = (struct curve_search_args *)farg;
    const ON_NurbsCurve *nc = cargs->nc;
    ON_3dPoint tp = cargs->tp;
    ON_3dPoint crv_pnt;
    ON_3dVector crv_d1;
    ON_3dVector crv_d2;
    if (!nc->Ev2Der(t, crv_pnt, crv_d1, crv_d2)) return -1;
    ON_3dVector fparam2 = crv_pnt - tp;
    *ft = ON_DotProduct(crv_d1, fparam2);
    *dft = ON_DotProduct(crv_d2, fparam2) + ON_DotProduct(crv_d1, crv_d1);

    if (*ft < ON_ZERO_TOLERANCE) return 1;
    return 0;
}

bool
ON_NurbsCurve_GetClosestPoint(
	double *t,
	const ON_NurbsCurve *nc,
	const ON_3dPoint &p,
	double maximum_distance = 0.0,
	const ON_Interval *sub_domain = NULL
	)
{
    if (!t || !nc) return false;

    ON_Interval domain = (sub_domain) ? *sub_domain : nc->Domain();
    size_t init_sample_cnt = nc->CVCount() * nc->Degree();
    double span = 1.0/(double)(init_sample_cnt);

    // Get an initial sampling of uniform points along the active
    // curve domain
    std::vector<ON_3dPoint> pnts;
    for (size_t i = 0; i <= init_sample_cnt; i++) {
	double st = domain.ParameterAt(i * span);
	pnts.push_back(nc->PointAt(st));
    }

    // Find an initial domain subset based on the breakdown into segments
    double d1 = domain.Min();
    double d2 = domain.Max();
    double vmin = DBL_MAX;
    for (size_t i = 0; i < pnts.size() - 1; i++) {
	ON_Line l(pnts[i], pnts[i+1]);
	double lt;
	l.ClosestPointTo(p, &lt);
	ON_3dPoint pl = l.PointAt(lt);
	if ((lt < 0 || NEAR_ZERO(lt, ON_ZERO_TOLERANCE))) {
	    pl = l.PointAt(0);
	}
	if ((lt > 1 || NEAR_EQUAL(lt, 1, ON_ZERO_TOLERANCE))) {
	    pl = l.PointAt(1);
	}
	if (pl.DistanceTo(p) < vmin) {
	    vmin = pl.DistanceTo(p);
	    d1 = domain.ParameterAt(i*span);
	    d2 = domain.ParameterAt((i+1)*span);
	}
    }

    // Iterate to find the closest point
    double vdist = DBL_MAX;
    double u = (d1 + d2) * 0.5;
    struct curve_search_args cargs;
    cargs.nc = nc;
    cargs.tp = p;
    double st;
    int osearch = 0;

    if (!nc->IsLinear(TOL2)) {
	osearch = ON_FindLocalMinimum(curve_closest_search_func, &cargs, d1, u, d2, ON_EPSILON, 0.5*ON_ZERO_TOLERANCE, 100, &st);
    }

    if (osearch == 1) {

	(*t) = st;
	vdist = (p.DistanceTo(nc->PointAt(st)));

    } else {

	// ON_FindLocalMinimum failed, fall back on binary search
	double vmin_delta = DBL_MAX;
	ON_3dPoint p1 = nc->PointAt(d1);
	ON_3dPoint p2 = nc->PointAt(d2);
	double vmin_prev = (p1.DistanceTo(p) > p2.DistanceTo(p)) ? p1.DistanceTo(p) : p2.DistanceTo(p);
	while (vmin_delta > ON_ZERO_TOLERANCE) {
	    u = (d1 + d2) * 0.5;
	    if (p1.DistanceTo(p) < p2.DistanceTo(p)) {
		d2 = u;
		p2 = nc->PointAt(u);
	    } else {
		d1 = u;
		p1 = nc->PointAt(u);
	    }
	    vdist = (p.DistanceTo(nc->PointAt(u)));
	    vmin_delta = fabs(vmin_prev - vdist);
	    vmin_prev = vdist;
	}

	(*t) = u;

    }

    if (maximum_distance > 0 && vdist > maximum_distance) return false;

    return true;
}

struct curve_line_search_args {
    const ON_NurbsCurve *nc;
    const ON_Line *l;
    double vdist;
};

int
curve_line_closest_search_func(void *farg, double t, double *ft, double *dft)
{
    struct curve_line_search_args *largs = (struct curve_line_search_args *)farg;
    const ON_NurbsCurve *nc = largs->nc;
    ON_3dPoint crv_pnt;
    ON_3dVector crv_d1;
    ON_3dVector crv_d2;
    if (!nc->Ev2Der(t, crv_pnt, crv_d1, crv_d2)) return -1;

    // Unlike the closet point to test point case, we don't have a single target point
    // to use.  Instead, for the current curve point, find the closest point on the
    // line to that point and use it as the "current" test point.
    double lt;
    largs->l->ClosestPointTo(crv_pnt, &lt);
    ON_3dPoint tp = largs->l->PointAt(lt);
  
    // Since the test point is ephemeral, calculate and stash the distance
    // while we have it  
    largs->vdist = crv_pnt.DistanceTo(tp);
   
    ON_3dVector fparam2 = crv_pnt - tp;
    *ft = ON_DotProduct(crv_d1, fparam2);
    *dft = ON_DotProduct(crv_d2, fparam2) + ON_DotProduct(crv_d1, crv_d1);

    if (*ft < ON_ZERO_TOLERANCE) return 1;
    return 0;
}

bool
ON_NurbsCurve_ClosestPointToLineSegment(
	double *dist,
	double *t,
	const ON_NurbsCurve *nc,
	const ON_Line &l,
	double maximum_distance,
	const ON_Interval *sub_domain
	)
{
    if (!nc) return false;

    ON_Interval domain = (sub_domain) ? *sub_domain : nc->Domain();

    if (nc->IsLinear(TOL2)) {
	// If the curve is linear, this reduces to an lseg/lseg test and
	// (optionally depending on what the user has requested) a get closest
	// point operation to figure out the corresponding t paramater.
	ON_3dPoint onl1 = l.PointAt(0);
	ON_3dPoint onl2 = l.PointAt(1);
	ON_3dPoint onc1 = nc->PointAt(domain.ParameterAt(0));
	ON_3dPoint onc2 = nc->PointAt(domain.ParameterAt(1));
	point_t C0, C1, L0, L1;
	VSET(C0, onc1.x, onc1.y, onc1.z);
	VSET(C1, onc2.x, onc2.y, onc2.z);
	VSET(L0, onl1.x, onl1.y, onl1.z);
	VSET(L1, onl2.x, onl2.y, onl2.z);
	double ndist;
	if (t) {
	    point_t c1i, l1i;
	    ndist = sqrt(bg_distsq_lseg3_lseg3(&c1i, &l1i, C0, C1, L0, L1));
	    double nt;
	    ON_3dPoint tp(c1i[0], c1i[1], c1i[2]);
	    if (!ON_NurbsCurve_GetClosestPoint(&nt, nc, tp, maximum_distance, &domain)) {
		bu_log("ON_NurbsCurve_ClosestPointToLineSegment: linear closest point t param search failed\n");
		if (dist) {
		    (*dist) = ndist;
		}
		return false;
	    }
	    (*t) = nt;
	} else {
	    ndist = sqrt(bg_distsq_lseg3_lseg3(NULL, NULL, C0, C1, L0, L1));
	}
	if (dist) {
	    (*dist) = ndist;
	}

	if (maximum_distance > 0 && ndist > maximum_distance) return false;
	return true;
    }

    size_t init_sample_cnt = nc->CVCount() * nc->Degree();
    double span = 1.0/(double)(init_sample_cnt);

    // Get an initial sampling of uniform points along the active
    // curve domain
    std::vector<ON_3dPoint> pnts;
    for (size_t i = 0; i <= init_sample_cnt; i++) {
	double st = domain.ParameterAt(i * span);
	pnts.push_back(nc->PointAt(st));
    }

    // Find an initial domain subset based on the breakdown into segments
    double d1 = domain.Min();
    double d2 = domain.Max();
    double vmin = DBL_MAX;
    for (size_t i = 0; i < pnts.size() - 1; i++) {
	ON_Line lseg(pnts[i], pnts[i+1]);
	double lseglen = lseg.MinimumDistanceTo(l);
	if (lseglen < vmin) {
	    vmin = lseglen;
	    d1 = domain.ParameterAt(i*span);
	    d2 = domain.ParameterAt((i+1)*span);
	}
    }

    // Iterate to find the closest point
    double u = (d1 + d2) * 0.5;
    struct curve_line_search_args largs;
    largs.nc = nc;
    largs.l = &l;
    double st;
    int osearch = ON_FindLocalMinimum(curve_line_closest_search_func, &largs, d1, u, d2, ON_EPSILON, 0.5*ON_ZERO_TOLERANCE, 100, &st);

    if (osearch == 1) {

	if (t) {
	    (*t) = st;
	}
	if (dist) {
	    (*dist) = largs.vdist;
	}

    } else {

	bu_log("ON_NurbsCurve_ClosestPointToLineSegment: ON_FindLocalMinimum search failed\n");
	return false;

    }

    if (maximum_distance > 0 && largs.vdist > maximum_distance) return false;

    return true;
}



static double
trim_binary_search(fastf_t *tparam, const ON_BrepTrim *trim, double tstart, double tend, const ON_3dPoint &edge_3d, double tol, int depth, int force)
{
    double tcparam = (tstart + tend) / 2.0;
    ON_3dPoint trim_2d = trim->PointAt(tcparam);
    const ON_Surface *s = trim->SurfaceOf();
    ON_3dPoint trim_3d = s->PointAt(trim_2d.x, trim_2d.y);
    double dist = edge_3d.DistanceTo(trim_3d);

    if (dist > tol && !force) {
	ON_3dPoint trim_start_2d = trim->PointAt(tstart);
	ON_3dPoint trim_end_2d = trim->PointAt(tend);
	ON_3dPoint trim_start_3d = s->PointAt(trim_start_2d.x, trim_start_2d.y);
	ON_3dPoint trim_end_3d = s->PointAt(trim_end_2d.x, trim_end_2d.y);

	ON_3dVector v1 = edge_3d - trim_start_3d;
	ON_3dVector v2 = edge_3d - trim_end_3d;
	double vdot = ON_DotProduct(v1,v2);

	if (vdot < 0 && dist > ON_ZERO_TOLERANCE) {
	    double tlparam = DBL_MAX;
	    double trparam = DBL_MAX;
	    double fldist = trim_binary_search(&tlparam, trim, tstart, tcparam, edge_3d, tol, depth+1, 0);
	    double frdist = trim_binary_search(&trparam, trim, tcparam, tend, edge_3d, tol, depth+1, 0);
	    if (fldist >= 0 && frdist < -1) {
		(*tparam) = tlparam;
		return fldist;
	    }
	    if (frdist >= 0 && fldist < -1) {
		(*tparam) = trparam;
		return frdist;
	    }
	    if (fldist < -1 && frdist < -1) {
		fldist = trim_binary_search(&tlparam, trim, tstart, tcparam, edge_3d, tol, depth+1, 1);
		frdist = trim_binary_search(&trparam, trim, tcparam, tend, edge_3d, tol, depth+1, 1);
		if ((fldist < frdist) && (fldist < dist)) {
		    (*tparam) = tlparam;
		    return fldist;
		}
		if ((frdist < fldist) && (frdist < dist)) {
		    (*tparam) = trparam;
		    return frdist;
		}
		(*tparam) = tcparam;
		return dist;

	    }
	} else if (NEAR_ZERO(vdot, ON_ZERO_TOLERANCE)) {
	    (*tparam) = tcparam;
	    return dist;
	} else {
	    // Not in this span
	    if (depth == 0) {
		(*tparam) = tcparam;
		return dist;
	    } else {
		return -2;
	    }
	}
    }

    // close enough - this works
    (*tparam) = tcparam;
    return dist;
}

bool
ON_TrimCurve_GetClosestPoint(
	double *t,
	const ON_BrepTrim *trim,
	const ON_3dPoint &p,
	double maximum_distance,
	const ON_Interval *sub_domain
	)
{
    if (!t || !trim) {
	return false;
    }

    ON_Interval domain = (sub_domain) ? *sub_domain : trim->Domain();

    if (trim->m_type == ON_BrepTrim::singular) {
	// If the trim is singular, there's only one point to check.
	if (maximum_distance > 0) {
	    ON_3dPoint trim_3d = trim->Brep()->m_V[trim->m_vi[0]].Point();
	    if (trim_3d.DistanceTo(p) > maximum_distance) {
		return false;
	    }
	}
	(*t) = domain.ParameterAt(0);
	return true;
    }

    double vdist = trim_binary_search(t, trim, domain.ParameterAt(0), domain.ParameterAt(1), p, maximum_distance, 0, 0);
    if (vdist < 0) {
	return false;
    }

    if (maximum_distance > 0 && vdist > maximum_distance) {
	return false;
    }

    return true;
}

// Extraction of the old Pushup method from openNURBS - not
// present in newer code?
ON_Curve*
ON_Surface_Pushup(
	const ON_Surface *s,
	const ON_Curve& curve_2d,
	const ON_Interval* curve_2d_subdomain
	)
{
    if (!s)
	return NULL;
    // if the 2d curve is an isocurve, then ON_Surface::Pushup
    // will return the answer.  Otherwise, the virtual override
    // will have to do the real work.
    ON_Curve* curve = NULL;
    ON_Surface::ISO iso = s->IsIsoparametric(curve_2d,curve_2d_subdomain);
    int dir = -1;
    switch (iso)
    {
	case ON_Surface::x_iso:
	case ON_Surface::W_iso:
	case ON_Surface::E_iso:
	    dir = 1;
	    break;
	case ON_Surface::y_iso:
	case ON_Surface::S_iso:
	case ON_Surface::N_iso:
	    dir = 0;
	    break;
	default:
	    // intentionally ignoring other ON_Surface::ISO enum values
	    break;
    }
    if ( dir >= 0 )
    {
	double c;
	ON_Interval c2_dom = curve_2d.Domain();
	if ( !curve_2d_subdomain )
	    curve_2d_subdomain = &c2_dom;
	ON_3dPoint p0 = curve_2d.PointAt( curve_2d_subdomain->Min() );
	ON_3dPoint p1 = curve_2d.PointAt( curve_2d_subdomain->Max() );
	ON_Interval c3_dom( p0[dir], p1[dir] );
	bool bRev = c3_dom.IsDecreasing();
	if ( bRev )
	    c3_dom.Swap();
	if ( c3_dom.IsIncreasing() )
	{
	    if ( NEAR_EQUAL(p0[1-dir], p1[1-dir], SMALL_FASTF) )
		c = p0[1-dir];
	    else
		c = 0.5*(p0[1-dir] + p1[1-dir]);
	    curve = s->IsoCurve( dir, c );
	    if ( curve && curve->Domain() != c3_dom )
	    {
		if ( !curve->Trim( c3_dom ) )
		{
		    delete curve;
		    curve = 0;
		}
	    }
	    if ( curve ) {
		if ( bRev )
		    curve->Reverse();
		curve->SetDomain( curve_2d_subdomain->Min(), curve_2d_subdomain->Max() );
	    }
	}
    }
    return curve;
}

namespace brlcad {

inline void
distribute(const int count, const ON_3dVector* v, double x[], double y[], double z[])
{
    for (int i = 0; i < count; i++) {
	x[i] = v[i].x;
	y[i] = v[i].y;
	z[i] = v[i].z;
    }
}


//--------------------------------------------------------------------------------
// CurveTree
CurveTree::CurveTree(const ON_BrepFace* face) :
    m_face(face),
    m_root(new BRNode(initialLoopBBox(*face))),
    m_stl(new Stl),
    m_sortedX_indices(NULL)
{
    for (int li = 0; li < face->LoopCount(); li++) {
	bool innerLoop = (li > 0) ? true : false;
	const ON_BrepLoop* loop = face->Loop(li);
	// for each trim
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    int adj_face_index = -1;
	    const int trim_index = loop->m_ti[ti];
	    const ON_BrepTrim& trim = face->Brep()->m_T[trim_index];

	    if (trim.m_ei != -1) { // does not lie on a portion of a singular surface side
		const ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
		switch (trim.m_type) {
		    case ON_BrepTrim::unknown:
			bu_log("ON_BrepTrim::unknown on Face:%d\n", face->m_face_index);
			break;
		    case ON_BrepTrim::boundary:
			//bu_log("ON_BrepTrim::boundary on Face:%d\n", face->m_face_index);
			break;
		    case ON_BrepTrim::mated:
			if (edge.m_ti.Count() == 2) {
			    if (face->m_face_index == face->Brep()->m_T[edge.m_ti[0]].FaceIndexOf()) {
				adj_face_index = face->Brep()->m_T[edge.m_ti[1]].FaceIndexOf();
			    } else {
				adj_face_index = face->Brep()->m_T[edge.m_ti[0]].FaceIndexOf();
			    }
			} else {
			    bu_log("Mated Edge should have 2 adjacent faces, right?  Face(%d) has %d trim indexes\n", face->m_face_index, edge.m_ti.Count());
			}
			break;
		    case ON_BrepTrim::seam:
			if (edge.m_ti.Count() == 2) {
			    if ((face->m_face_index == face->Brep()->m_T[edge.m_ti[0]].FaceIndexOf()) && (face->m_face_index == face->Brep()->m_T[edge.m_ti[1]].FaceIndexOf())) {
				adj_face_index = face->m_face_index;
			    } else {
				bu_log("Seamed Edge should have 1 faces sharing the trim so trim index should be one, right? Face(%d) has %d trim indexes\n", face->m_face_index, edge.m_ti.Count());
				bu_log("Face(%d) has %d, %d trim indexes\n", face->m_face_index, face->Brep()->m_T[edge.m_ti[0]].FaceIndexOf(), face->Brep()->m_T[edge.m_ti[1]].FaceIndexOf());
			    }
			} else if (edge.m_ti.Count() == 1) {
			    adj_face_index = face->m_face_index;
			} else {
			    bu_log("Seamed Edge should have 1 faces sharing the trim so trim index should be one, right? Face(%d) has %d trim indexes\n", face->m_face_index, edge.m_ti.Count());
			}
			break;
		    case ON_BrepTrim::singular:
			bu_log("ON_BrepTrim::singular on Face:%d\n", face->m_face_index);
			break;
		    case ON_BrepTrim::crvonsrf:
			bu_log("ON_BrepTrim::crvonsrf on Face:%d\n", face->m_face_index);
			break;
		    case ON_BrepTrim::ptonsrf:
			bu_log("ON_BrepTrim::ptonsrf on Face:%d\n", face->m_face_index);
			break;
		    case ON_BrepTrim::slit:
			bu_log("ON_BrepTrim::slit on Face:%d\n", face->m_face_index);
			break;
		    default:
			bu_log("ON_BrepTrim::default on Face:%d\n", face->m_face_index);
		}
	    }
	    const ON_Curve* trimCurve = trim.TrimCurveOf();
	    double min, max;
	    (void) trimCurve->GetDomain(&min, &max);
	    ON_Interval t(min, max);

	    TRACE("need to subdivide");
	    // divide on param interval

	    if (!trimCurve->IsLinear()) {
		int knotcnt = trimCurve->SpanCount();
		double *knots = new double[knotcnt + 1];

		trimCurve->GetSpanVector(knots);
		std::list<fastf_t> splitlist;
		for (int knot_index = 1; knot_index <= knotcnt; knot_index++) {
		    ON_Interval range(knots[knot_index - 1], knots[knot_index]);

		    if (range.Length() > BREP_UV_DIST_FUZZ)
			getHVTangents(trimCurve, range, splitlist);
		}
		for (std::list<fastf_t>::const_iterator l = splitlist.begin(); l != splitlist.end(); l++) {
		    double xmax = *l;
		    if (!NEAR_EQUAL(xmax, min, BREP_UV_DIST_FUZZ)) {
			m_root->addChild(subdivideCurve(trimCurve, trim_index, adj_face_index, min, xmax, innerLoop, 0));
		    }
		    min = xmax;
		}
		delete [] knots;
	    } else {
		int knotcnt = trimCurve->SpanCount();
		double *knots = new double[knotcnt + 1];

		trimCurve->GetSpanVector(knots);
		for (int knot_index = 1; knot_index <= knotcnt; knot_index++) {
		    double xmax = knots[knot_index];
		    if (!NEAR_EQUAL(xmax, min, BREP_UV_DIST_FUZZ)) {
			m_root->addChild(subdivideCurve(trimCurve, trim_index, adj_face_index, min, xmax, innerLoop, 0));
		    }
		    min = xmax;
		}
		delete [] knots;
	    }

	    if (!NEAR_EQUAL(max, min, BREP_UV_DIST_FUZZ)) {
		m_root->addChild(subdivideCurve(trimCurve, trim_index, adj_face_index, min, max, innerLoop, 0));
	    }
	}
    }

    std::list<const BRNode *> temp;
    getLeaves(temp);
    temp.sort(sortX);
    m_stl->m_sortedX.insert(m_stl->m_sortedX.end(), temp.begin(), temp.end());

    return;
}


CurveTree::~CurveTree()
{
    delete m_root;
    delete m_stl;
    delete m_sortedX_indices;
}


CurveTree::CurveTree(Deserializer &deserializer, const ON_BrepFace &face) :
    m_face(&face),
    m_root(NULL),
    m_stl(new Stl),
    m_sortedX_indices(NULL)
{
    m_root = new BRNode(deserializer, *m_face->Brep());

    std::list<const BRNode *> temp;
    getLeaves(temp);
    temp.sort(sortX);
    m_stl->m_sortedX.insert(m_stl->m_sortedX.end(), temp.begin(), temp.end());
}


void
CurveTree::serialize(Serializer &serializer) const
{
    m_root->serialize(serializer);
}


std::vector<std::size_t>
CurveTree::serialize_get_leaves_keys(const std::list<const BRNode *> &leaves) const
{
    if (!m_sortedX_indices) {
	m_sortedX_indices = new std::map<const BRNode *, std::size_t>;
	std::size_t index = 0;

	for (std::vector<const BRNode *>::const_iterator it = m_stl->m_sortedX.begin(); it != m_stl->m_sortedX.end(); ++it, ++index)
	    m_sortedX_indices->insert(std::make_pair(*it, index));
    }

    std::vector<std::size_t> result;

    for (std::list<const BRNode *>::const_iterator it = leaves.begin(); it != leaves.end(); ++it)
	result.push_back(m_sortedX_indices->at(*it));

    return result;
}


std::list<const BRNode *>
CurveTree::serialize_get_leaves(const std::size_t *keys, std::size_t num_keys) const
{
    std::list<const BRNode *> result;

    for (std::size_t i = 0; i < num_keys; ++i)
	result.push_back(m_stl->m_sortedX.at(keys[i]));

    return result;
}


void
CurveTree::serialize_cleanup() const
{
    delete m_sortedX_indices;
    m_sortedX_indices = NULL;
}


const BRNode*
CurveTree::getRootNode() const
{
    return m_root;
}


int
CurveTree::depth() const
{
    return m_root->depth();
}


ON_2dPoint
CurveTree::getClosestPointEstimate(const ON_3dPoint& pt) const
{
    return m_root->getClosestPointEstimate(pt);
}


ON_2dPoint
CurveTree::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) const
{
    return m_root->getClosestPointEstimate(pt, u, v);
}


void
CurveTree::getLeaves(std::list<const BRNode*>& out_leaves) const
{
    m_root->getLeaves(out_leaves);
}


void
CurveTree::getLeavesAbove(std::list<const BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v) const
{
    point_t bmin, bmax;
    double dist;
    for (std::vector<const BRNode*>::const_iterator i = m_stl->m_sortedX.begin(); i != m_stl->m_sortedX.end(); i++) {
	const BRNode* br = *i;
	br->GetBBox(bmin, bmax);

	dist = BREP_UV_DIST_FUZZ;//0.03*DIST_PNT_PNT(bmin, bmax);
	if (bmax[X]+dist < u[0])
	    continue;
	if (bmin[X]-dist < u[1]) {
	    if (bmax[Y]+dist > v[0]) {
		out_leaves.push_back(br);
	    }
	}
    }
}


void
CurveTree::getLeavesAbove(std::list<const BRNode*>& out_leaves, const ON_2dPoint& pt, fastf_t tol) const
{
    point_t bmin, bmax;
    for (std::vector<const BRNode*>::const_iterator i = m_stl->m_sortedX.begin(); i != m_stl->m_sortedX.end(); i++) {
	const BRNode* br = *i;
	br->GetBBox(bmin, bmax);

	if (bmax[X]+tol < pt.x)
	    continue;
	if (bmin[X]-tol < pt.x) {
	    if (bmax[Y]+tol > pt.y) {
		out_leaves.push_back(br);
	    }
	}
    }
}


void
CurveTree::getLeavesRight(std::list<const BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v) const
{
    point_t bmin, bmax;
    double dist;
    for (std::vector<const BRNode*>::const_iterator i = m_stl->m_sortedX.begin(); i != m_stl->m_sortedX.end(); i++) {
	const BRNode* br = *i;
	br->GetBBox(bmin, bmax);

	dist = BREP_UV_DIST_FUZZ;//0.03*DIST_PNT_PNT(bmin, bmax);
	if (bmax[Y]+dist < v[0])
	    continue;
	if (bmin[Y]-dist < v[1]) {
	    if (bmax[X]+dist > u[0]) {
		out_leaves.push_back(br);
	    }
	}
    }
}


void
CurveTree::getLeavesRight(std::list<const BRNode*>& out_leaves, const ON_2dPoint& pt, fastf_t tol) const
{
    point_t bmin, bmax;
    for (std::vector<const BRNode*>::const_iterator i = m_stl->m_sortedX.begin(); i != m_stl->m_sortedX.end(); i++) {
	const BRNode* br = *i;
	br->GetBBox(bmin, bmax);

	if (bmax[Y]+tol < pt.y)
	    continue;
	if (bmin[Y]-tol < pt.y) {
	    if (bmax[X]+tol > pt.x) {
		out_leaves.push_back(br);
	    }
	}
    }
}


bool
CurveTree::getHVTangents(const ON_Curve* curve, const ON_Interval& t, std::list<fastf_t>& list) const
{
    double x;
    double midpoint = (t[1]+t[0])/2.0;
    ON_Interval left(t[0], midpoint);
    ON_Interval right(midpoint, t[1]);
    int status = ON_Curve_Has_Tangent(curve, t[0], t[1], BREP_UV_DIST_FUZZ);

    switch (status) {

	case 1: /* 1 Vertical tangent */
	    x = ON_Curve_Get_Vertical_Tangent(curve, t[0], t[1], TOL2);
	    list.push_back(x);
	    return true;

	case 2: /* 1 Horizontal tangent */
	    x = ON_Curve_Get_Horizontal_Tangent(curve, t[0], t[1], TOL2);
	    list.push_back(x);
	    return true;

	case 3: /* Horizontal and vertical tangents present - Simple midpoint split */
	    if (left.Length() > BREP_UV_DIST_FUZZ)
		getHVTangents(curve, left, list);
	    if (right.Length() > BREP_UV_DIST_FUZZ)
		getHVTangents(curve, right, list);
	    return true;

	default:
	    return false;

    }

    return false;  //Should never get here
}


BRNode*
CurveTree::curveBBox(const ON_Curve* curve, int trim_index, int adj_face_index, const ON_Interval& t, bool isLeaf, bool innerTrim, const ON_BoundingBox& bb) const
{
    BRNode* node;
    bool vdot = true;

    if (isLeaf) {
	TRACE("creating leaf: u(" << u.Min() << ", " << u.Max() << ") v(" << v.Min() << ", " << v.Max() << ")");
	node = new BRNode(curve, trim_index, adj_face_index, bb, m_face, t, vdot, innerTrim, false);
    } else {
	node = new BRNode(bb);
    }

    return node;

}


ON_BoundingBox
CurveTree::initialLoopBBox(const ON_BrepFace &face)
{
    ON_BoundingBox bb;
    face.SurfaceOf()->GetBBox(bb[0], bb[1]);

    for (int i = 0; i < face.LoopCount(); i++) {
	const ON_BrepLoop* loop = face.Loop(i);
	if (loop->m_type == ON_BrepLoop::outer) {
	    if (loop->GetBBox(bb[0], bb[1], 0)) {
		TRACE("BBox for Loop min<" << bb[0][0] << ", " << bb[0][1] ", " << bb[0][2] << ">");
		TRACE("BBox for Loop max<" << bb[1][0] << ", " << bb[1][1] ", " << bb[1][2] << ">");
	    }
	    break;
	}
    }

    return bb;
}


BRNode*
CurveTree::subdivideCurve(const ON_Curve* curve, int trim_index, int adj_face_index, double min, double max, bool innerTrim, int divDepth) const
{
    ON_3dPoint points[2];
    points[0] = curve->PointAt(min);
    points[1] = curve->PointAt(max);
    point_t minpt, maxpt;
    VSETALL(minpt, INFINITY);
    VSETALL(maxpt, -INFINITY);
    for (int i = 0; i < 2; i++)
	VMINMAX(minpt, maxpt, points[i]);
    points[0]=ON_3dPoint(minpt);
    points[1]=ON_3dPoint(maxpt);
    ON_BoundingBox bb(points[0], points[1]);

    ON_Interval t(min, max);
    if (isLinear(curve, min, max) || divDepth >= BREP_MAX_LN_DEPTH) {
	double delta = (max - min)/(BREP_BB_CRV_PNT_CNT-1);
	point_t pnts[BREP_BB_CRV_PNT_CNT];
	ON_3dPoint pnt;
	for (int i=0;i<BREP_BB_CRV_PNT_CNT-1;i++) {
	    pnt = curve->PointAt(min + delta*i);
	    VSET(pnts[i], pnt[0], pnt[1], pnt[2]);
	}
	pnt = curve->PointAt(max);
	VSET(pnts[BREP_BB_CRV_PNT_CNT-1], pnt[0], pnt[1], pnt[2]);

	VSETALL(minpt, MAX_FASTF);
	VSETALL(maxpt, -MAX_FASTF);
	for (int i = 0; i < BREP_BB_CRV_PNT_CNT; i++)
	    VMINMAX(minpt, maxpt, pnts[i]);

	VMOVE(pnt, minpt);
	bb.Set(pnt, false);
	VMOVE(pnt, maxpt);
	bb.Set(pnt, true);
	return curveBBox(curve, trim_index, adj_face_index, t, true, innerTrim, bb);
    }

    // else subdivide
    BRNode* parent = curveBBox(curve, trim_index, adj_face_index, t, false, innerTrim, bb);
    double mid = (max+min)/2.0;
    BRNode* l = subdivideCurve(curve, trim_index, adj_face_index, min, mid, innerTrim, divDepth+1);
    BRNode* r = subdivideCurve(curve, trim_index, adj_face_index, mid, max, innerTrim, divDepth+1);
    parent->addChild(l);
    parent->addChild(r);
    return parent;
}


/**
 * Determine whether a given curve segment is linear
 */
bool
CurveTree::isLinear(const ON_Curve* curve, double min, double max) const
{
    ON_3dVector tangent_start = curve->TangentAt(min);
    ON_3dVector tangent_end = curve->TangentAt(max);
    double vdot = tangent_start * tangent_end;
    if (vdot < BREP_CURVE_FLATNESS)
	return false;

    ON_3dPoint pmin = curve->PointAt(min);
    ON_3dPoint pmax = curve->PointAt(max);

    const ON_Surface* surf = m_face->SurfaceOf();
    ON_Interval u = surf->Domain(0);
    ON_Interval v = surf->Domain(1);
    point_t a, b;
    VSET(a, u[0], v[0], 0.0);
    VSET(b, u[1], v[1], 0.0);
    double dd = DIST_PNT_PNT(a, b);
    double cd = DIST_PNT_PNT(pmin, pmax);

    if (cd > BREP_TRIM_SUB_FACTOR*dd)
	return false;

    double delta = (max - min)/(BREP_BB_CRV_PNT_CNT-1);
    ON_3dPoint points[BREP_BB_CRV_PNT_CNT];
    for (int i=0;i<BREP_BB_CRV_PNT_CNT-1;i++) {
	points[i] = curve->PointAt(min + delta*i);
    }
    points[BREP_BB_CRV_PNT_CNT-1] = curve->PointAt(max);

    ON_3dVector A;
    ON_3dVector B;
    vdot = 1.0;
    A = points[BREP_BB_CRV_PNT_CNT-1] - points[0];
    A.Unitize();
    for (int i=1;i<BREP_BB_CRV_PNT_CNT-1;i++) {
	B = points[i] - points[0];
	B.Unitize();
	vdot = vdot * (A * B);
	if (vdot < BREP_CURVE_FLATNESS)
	    return false; //already failed
    }

    return vdot >= BREP_CURVE_FLATNESS;
}


//--------------------------------------------------------------------------------
// SurfaceTree
SurfaceTree::SurfaceTree(const ON_BrepFace* face, bool removeTrimmed, int depthLimit, double within_distance_tol) :
    m_ctree(NULL),
    m_removeTrimmed(removeTrimmed),
    m_face(face),
    m_root(NULL),
    m_f_queue(new std::queue<ON_Plane *>)
{
    // build the surface bounding volume hierarchy
    const ON_Surface* surf = face->SurfaceOf();
    if (!surf) {
	TRACE("ERROR: NULL surface encountered in SurfaceTree()");
	return;
    }

    // may be a smaller trimmed subset of surface so worth getting
    // face boundary
    bool bGrowBox = false;
    ON_3dPoint min = ON_3dPoint::UnsetPoint, max = ON_3dPoint::UnsetPoint;
    for (int li = 0; li < face->LoopCount(); li++) {
	for (int ti = 0; ti < face->Loop(li)->TrimCount(); ti++) {
	    const ON_BrepTrim *trim = face->Loop(li)->Trim(ti);
	    trim->GetBoundingBox(min, max, bGrowBox);
	    bGrowBox = true;
	}
    }
    if (!bGrowBox) {
	surf->GetBoundingBox(min, max);
	removeTrimmed = false;
    }

    // first, build the Curve Tree
    if (removeTrimmed)
	m_ctree = new CurveTree(m_face);
    else
	m_ctree = NULL;

    TRACE("Creating surface tree for: " << face->m_face_index);

    ON_Interval u = surf->Domain(0);
    ON_Interval v = surf->Domain(1);

#ifdef LOOSEN_UV
    min[0] -= within_distance_tol;
    max[0] += within_distance_tol;
    min[1] -= within_distance_tol;
    max[1] += within_distance_tol;
#endif
    if ((min != ON_3dPoint::UnsetPoint) && (max != ON_3dPoint::UnsetPoint)) {
	u.Set(min[0], max[0]);
	v.Set(min[1], max[1]);
    }

    double uq = u.Length()*0.25;
    double vq = v.Length()*0.25;

    ///////////////////////////////////////////////////////////////////////
    // Populate initial frames array for use in tree build
    ON_Plane frames[9];
    surf->FrameAt(u.Min(), v.Min(), frames[0]);
    surf->FrameAt(u.Max(), v.Min(), frames[1]);
    surf->FrameAt(u.Max(), v.Max(), frames[2]);
    surf->FrameAt(u.Min(), v.Max(), frames[3]);
    surf->FrameAt(u.Mid(), v.Mid(), frames[4]);
    surf->FrameAt(u.Mid() - uq, v.Mid() - vq, frames[5]);
    surf->FrameAt(u.Mid() - uq, v.Mid() + vq, frames[6]);
    surf->FrameAt(u.Mid() + uq, v.Mid() - vq, frames[7]);
    surf->FrameAt(u.Mid() + uq, v.Mid() + vq, frames[8]);

    m_root = subdivideSurface(surf, u, v, frames, 0, depthLimit, 1, within_distance_tol);

    if (m_root) {
	m_root->BuildBBox();
    }
    TRACE("u: [" << u[0] << ", " << u[1] << "]");
    TRACE("v: [" << v[0] << ", " << v[1] << "]");
    TRACE("m_root: " << m_root);
    while (!m_f_queue->empty()) {
	bu_free(m_f_queue->front(), "free subsurface frames array");
	m_f_queue->pop();
    }
}


SurfaceTree::~SurfaceTree()
{
    delete m_ctree;
    delete m_root;
    delete m_f_queue;
}


BBNode*
SurfaceTree::getRootNode() const
{
    return m_root;
}


int
SurfaceTree::depth() const
{
    return m_root->depth();
}


ON_2dPoint
SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt) const
{
    return m_root->getClosestPointEstimate(pt);
}


ON_2dPoint
SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) const
{
    return m_root->getClosestPointEstimate(pt, u, v);
}


void
SurfaceTree::getLeaves(std::list<const BBNode*>& out_leaves) const
{
    m_root->getLeaves(out_leaves);
}


const ON_Surface *
SurfaceTree::getSurface() const
{
    return m_face->SurfaceOf();
}


int
brep_getSurfacePoint(const ON_3dPoint& pt, ON_2dPoint& uv, const BBNode* node) {
    plane_ray pr;
    const ON_Surface *surf = node->get_face().SurfaceOf();
    double umin, umax;
    double vmin, vmax;
    surf->GetDomain(0, &umin, &umax);
    surf->GetDomain(1, &vmin, &vmax);

    ON_3dVector dir = node->m_normal;
    dir.Reverse();
    ON_Ray ray(const_cast<ON_3dPoint &>(pt), dir);
    if (brep_get_plane_ray(ray, pr) < 0)
	return -1;

    //know use this as guess to iterate to closer solution
    pt2d_t Rcurr = V2INIT_ZERO;
    pt2d_t new_uv = V2INIT_ZERO;
    ON_3dVector su = ON_3dVector::ZeroVector;
    ON_3dVector sv = ON_3dVector::ZeroVector;
    bool found=false;
    fastf_t Dlast = MAX_FASTF;
    pt2d_t nuv;
    nuv[0] = (node->m_u[1] + node->m_u[0])/2.0;
    nuv[1] = (node->m_v[1] + node->m_v[0])/2.0;
    ON_3dPoint newpt;
    for (int i = 0; i < BREP_MAX_ITERATIONS; i++) {
	brep_r(surf, pr, nuv, newpt, su, sv, Rcurr);
	fastf_t d = v2mag(Rcurr);

	if (d < BREP_INTERSECTION_ROOT_EPSILON) {
	    TRACE1("R:"<<ON_PRINT2(Rcurr));
	    found = true;
	    break;
	} else if (d < BREP_INTERSECTION_ROOT_SETTLE) {
	    found = true;
	}
	brep_newton_iterate(pr, Rcurr, su, sv, nuv, new_uv);

	//Check for closed surface wrap around
	if (surf->IsClosed(0)) {
	    CLAMP(new_uv[0], umin, umax);
	}
	if (surf->IsClosed(1)) {
	    CLAMP(new_uv[1], vmin, vmax);
	}

#ifdef HOOD
	//push answer back to within node bounds
	double ufluff = (node->m_u[1] - node->m_u[0])*0.01;
	double vfluff = (node->m_v[1] - node->m_v[0])*0.01;
#else
	//push answer back to within node bounds
	double ufluff = 0.0;
	double vfluff = 0.0;
#endif

	if (new_uv[0] < node->m_u[0] - ufluff)
	    new_uv[0] = node->m_u[0];
	else if (new_uv[0] > node->m_u[1] + ufluff)
	    new_uv[0] = node->m_u[1];

	if (new_uv[1] < node->m_v[0] - vfluff)
	    new_uv[1] = node->m_v[0];
	else if (new_uv[1] > node->m_v[1] + vfluff)
	    new_uv[1] = node->m_v[1];


	surface_EvNormal(surf, new_uv[0], new_uv[1], newpt, ray.m_dir);
	ray.m_dir.Reverse();
	brep_get_plane_ray(ray, pr);

	if (d < Dlast) {
	    move(nuv, new_uv);
	    Dlast = d;
	}
    }
    if (found) {
	uv.x = nuv[0];
	uv.y = nuv[1];
	return 1;
    }
    return -1;
}


int
SurfaceTree::getSurfacePoint(const ON_3dPoint& pt, ON_2dPoint& uv, const ON_3dPoint& from, double tolerance) const
{
    std::list<const BBNode*> nodes;
    (void)m_root->getLeavesBoundingPoint(from, nodes);

    double min_dist = MAX_FASTF;
    ON_2dPoint curr_uv(0.0, 0.0);
    bool found = false;

    std::list<const BBNode*>::const_iterator i;
    for (i = nodes.begin(); i != nodes.end(); i++) {
	const BBNode* node = (*i);
	if (brep_getSurfacePoint(pt, curr_uv, node)) {
	    ON_3dPoint fp = m_face->SurfaceOf()->PointAt(curr_uv.x, curr_uv.y);
	    double dist = fp.DistanceTo(pt);
	    if (NEAR_ZERO(dist, BREP_SAME_POINT_TOLERANCE)) {
		uv = curr_uv;
		return 1; //close enough to same point so no sense in looking for one closer
	    } else if (NEAR_ZERO(dist, tolerance)) {
		if (dist < min_dist) {
		    uv = curr_uv;
		    min_dist = dist;
		    found = true; //within tolerance but may be a point closer so keep looking
		}
	    }
	}
    }
    if (found) {
	return 1;
    }

    nodes.clear();
    (void)m_root->getLeavesBoundingPoint(pt, nodes);
    for (i = nodes.begin(); i != nodes.end(); i++) {
	const BBNode* node = (*i);
	if (brep_getSurfacePoint(pt, curr_uv, node)) {
	    ON_3dPoint fp = m_face->SurfaceOf()->PointAt(curr_uv.x, curr_uv.y);
	    double dist = fp.DistanceTo(pt);
	    if (NEAR_ZERO(dist, BREP_SAME_POINT_TOLERANCE)) {
		uv = curr_uv;
		return 1; //close enough to same point so no sense in looking for one closer
	    } else if (NEAR_ZERO(dist, tolerance)) {
		if (dist < min_dist) {
		    uv = curr_uv;
		    min_dist = dist;
		    found = true; //within tolerance but may be a point closer so keep looking
		}
	    }
	}
    }

    if (found) {
	return 1;
    }

    return -1;
}


//static int bb_cnt=0;
BBNode *
SurfaceTree::surfaceBBox(const ON_Surface *localsurf,
			 bool isLeaf,
			 const ON_Plane frames[9],
			 const ON_Interval& u,
			 const ON_Interval& v,
			 double within_distance_tol) const
{
    point_t min, max, buffer;
    ON_BoundingBox bbox = ON_BoundingBox::EmptyBoundingBox;
    if (!surface_GetBoundingBox(localsurf, u, v, bbox, false)) {
	return NULL;
    }

    VSETALL(buffer, within_distance_tol);

    //bu_log("in bb%d rpp %f %f %f %f %f %f\n", bb_cnt, min[0], max[0], min[1], max[1], min[2], max[2]);
    VMOVE(min, bbox.Min());
    VMOVE(max, bbox.Max());

    // calculate the estimate point on the surface: i.e. use the point
    // on the surface defined by (u.Mid(), v.Mid()) as a heuristic for
    // finding the uv domain bounding a portion of the surface close
    // to an arbitrary model-space point (see
    // getClosestPointEstimate())
    ON_3dPoint estimate;
    ON_3dVector normal;
    estimate = frames[4].origin;
    normal = frames[4].zaxis;

    BBNode* node;
    if (isLeaf) {
	/* vect_t delta; */

	VSUB2(min, min, buffer);
	VADD2(max, max, buffer);
	/*	VSUB2(delta, max, min);
		VSCALE(delta, delta, BBOX_GROW_3D);
		VSUB2(min, min, delta);
		VADD2(max, max, delta);
	*/
	TRACE("creating leaf: u(" << u.Min() << ", " << u.Max() <<
	      ") v(" << v.Min() << ", " << v.Max() << ")");
	node = new BBNode(m_ctree, ON_BoundingBox(ON_3dPoint(min), ON_3dPoint(max)), u, v, false, false);
	node->prepTrims();

    } else {
	node = new BBNode(ON_BoundingBox(ON_3dPoint(min), ON_3dPoint(max)), m_ctree);
    }

    node->m_estimate = estimate;
    node->m_normal = normal;
    node->m_u = u;
    node->m_v = v;
    return node;
}


HIDDEN BBNode*
initialBBox(const CurveTree* ctree, const ON_Surface* surf, const ON_Interval& u, const ON_Interval& v)
{
    ON_BoundingBox bb = ON_BoundingBox::EmptyBoundingBox;
    if (!surface_GetBoundingBox(surf, u, v, bb, false)) {
	return NULL;
    }
    BBNode* node = new BBNode(ctree, bb, u, v, false, false);
    ON_3dPoint estimate;
    ON_3dVector normal;
    if (!surface_EvNormal(surf, surf->Domain(0).Mid(), surf->Domain(1).Mid(), estimate, normal)) {
	bu_bomb("Could not evaluate estimate point on surface");
    }
    node->m_estimate = estimate;
    node->m_normal = normal;
    node->m_u = u;
    node->m_v = v;
    return node;
}


// Cache surface information as file static to ensure initialization once;
static const ON_Surface *prev_surf[MAX_PSW] = {NULL};
static ON_Interval dom[MAX_PSW][2];
static int span_cnt[MAX_PSW][2] = {{0, 0}};
static double *span[MAX_PSW][2] = {{NULL, NULL}};

bool
hasSplit(const ON_Surface *surf, const int dir, const ON_Interval& interval, double &split)
{
    int p = bu_parallel_id();
    if (surf == NULL) {
	// clean up statics and return
	prev_surf[p] = NULL;
	for(int i=0; i<2; i++) {
	    span_cnt[p][i] = 0;
	    if (span[p][i])
		bu_free(span[p][i], "surface span vector");
	    span[p][i] = NULL;
	    dom[p][i] = ON_Interval::EmptyInterval;
	}

	return false;
    }
    if (prev_surf[p] != surf) {
	// load new surf info
	for(int i=0; i<2; i++) {
	    dom[p][i] = surf->Domain(i);
	    span_cnt[p][i] = surf->SpanCount(i);
	    if (span[p][i])
		bu_free(span[p][i], "surface span vector");
	    span[p][i] = (double *)bu_malloc((unsigned)(span_cnt[p][i]+1) * sizeof(double), "surface span vector");
	    surf->GetSpanVector(i, span[p][i]);
	}

	prev_surf[p] = surf;
    }

    // find direction split based on setting of 'dir'

    // first, if closed in 'dir' check to see if it extends over seam and use that as split
    if (surf->IsClosed(dir)) {
	bool testOpen = true;
	if (interval.Includes(dom[p][dir].m_t[0], testOpen)) { //crosses lower boundary
	    split = dom[p][dir].m_t[0];
	    return true;
	} else if (interval.Includes(dom[p][dir].m_t[1], testOpen)) { //crosses upper boundary
	    split = dom[p][dir].m_t[1];
	    return true;
	}
    }

    // next lets see if we have a knots in interval, if so split on middle knot
    if (span_cnt[p][dir] > 1) {
	int sum = 0;
	int cnt = 0;
	for(int i=0; i<span_cnt[p][dir]+1; i++) {
	    bool testOpen = true;
	    if (interval.Includes((span[p][dir])[i], testOpen)) { //crosses lower boundary
		sum = sum + i;
		cnt++;
	    }
	}
	if (cnt > 0) {
	    split = (span[p][dir])[sum/cnt];
	    return true;
	}
    }

    return false;
}


BBNode*
SurfaceTree::subdivideSurface(const ON_Surface *localsurf,
			      const ON_Interval& u,
			      const ON_Interval& v,
			      ON_Plane frames[9],
			      int divDepth,
			      int depthLimit,
			      int prev_knot,
			      double within_distance_tol
    ) const
{
    BBNode* quads[4];
    BBNode* parent = NULL;
    double usplit = 0.0;
    double vsplit = 0.0;
    double width = 0.0;
    double height = 0.0;
    double ratio = 5.0;
    double uq = u.Length()*0.25;
    double vq = v.Length()*0.25;
    localsurf->FrameAt(u.Mid() - uq, v.Mid() - vq, frames[5]);
    localsurf->FrameAt(u.Mid() - uq, v.Mid() + vq, frames[6]);
    localsurf->FrameAt(u.Mid() + uq, v.Mid() - vq, frames[7]);
    localsurf->FrameAt(u.Mid() + uq, v.Mid() + vq, frames[8]);
    unsigned int do_u_split = 0;
    unsigned int do_v_split = 0;
    unsigned int do_both_splits = 0;

    usplit = u.Mid();
    vsplit = v.Mid();

    if (divDepth >= depthLimit) {
	return surfaceBBox(localsurf, true, frames, u, v, within_distance_tol);
    }

    // The non-knot case where all criteria are satisfied is the
    // terminating case for the recursion - handle that first
    if (!prev_knot) {
	width = u.Length();
	height = v.Length();
	if (((width/height < ratio) && (width/height > 1.0/ratio) && isFlat(frames) && isStraight(frames))
	    || (divDepth >= depthLimit)) { //BREP_MAX_FT_DEPTH
	    return surfaceBBox(localsurf, true, frames, u, v, within_distance_tol);
	}
    }

    // Knots
    if (prev_knot) {
	int dir = 0; // U direction
	if (hasSplit(localsurf, dir, u, usplit)) {
	    do_u_split = 1;
	}
	dir = 1; // V direction
	if (hasSplit(localsurf, dir, v, vsplit)) {
	    if (do_u_split) {
		do_both_splits = 1;
		do_u_split = 0;
	    } else {
		do_v_split = 1;
	    }
	}
	parent = initialBBox(m_ctree, localsurf, u, v);
    }
    // Flatness
    if (!prev_knot) {
	bool isUFlat = isFlatU(frames);
	bool isVFlat = isFlatV(frames);

	parent = (divDepth == 0) ? initialBBox(m_ctree, localsurf, u, v) : surfaceBBox(localsurf, false, frames, u, v, within_distance_tol);

	if ((!isVFlat || (width/height > ratio)) && (!isUFlat || (height/width > ratio))) {
	    do_both_splits = 1;
	}
	if ((!isUFlat || (width/height > ratio)) && !do_both_splits) {
	    do_u_split = 1;
	}
	if (!do_both_splits && ! do_u_split) {
	    do_v_split = 1;
	}
    }

    ///////////////////////////////////

    if (do_both_splits) {
	ON_Interval firstu(u.Min(), usplit);
	ON_Interval secondu(usplit, u.Max());
	ON_Interval firstv(v.Min(), vsplit);
	ON_Interval secondv(vsplit, v.Max());

	const ON_Surface *q0surf = localsurf;
	const ON_Surface *q1surf = localsurf;
	const ON_Surface *q2surf = localsurf;
	const ON_Surface *q3surf = localsurf;


	/*********************************************************************
	 * In order to avoid fairly expensive re-calculation of 3d points at
	 * uv coordinates, all values that are shared between children at
	 * the same depth of a surface subdivision are pre-computed and
	 * passed as parameters.
	 *
	 * The majority of these points are already evaluated in the process
	 * of testing whether a subdivision has produced a leaf node.  These
	 * values are in the normals and corners arrays and have index values
	 * corresponding to the values of the figure on the left below.  There
	 * are four other shared values that are precomputed in the sharedcorners
	 * and sharednormals arrays; their index values in those arrays are
	 * illustrated in the figure on the right:
	 *
	 *
	 *   3-------------------2      +---------2---------+
	 *   |                   |      |                   |
	 *   |    6         8    |      |                   |
	 *   |                   |      |                   |
	 *  V|         4         |      1                   3
	 *   |                   |      |                   |
	 *   |    5         7    |      |                   |
	 *   |                   |      |                   |
	 *   0-------------------1      +---------0---------+
	 *             U                          U
	 *
	 *   Values inherited from      Values pre-prepared in
	 *   parent subdivision         shared arrays
	 *
	 *
	 * When the four subdivisions are made, the parent parameters
	 * are passed to the children as follows (values from the
	 * shared arrays are prefaced with an s):
	 *
	 *    3--------------S2     S2--------------2
	 *    |               |     |               |
	 *    |               |     |               |
	 *  V |       6       |     |       8       |
	 *    |               |     |               |
	 *    |               |     |               |
	 *    S1--------------4     4--------------S3
	 *            U                     U
	 *
	 *        Quadrant 3            Quadrant 2
	 *
	 *    S1--------------4     4--------------S3
	 *    |               |     |               |
	 *    |               |     |               |
	 *  V |       5       |     |       7       |
	 *    |               |     |               |
	 *    |               |     |               |
	 *    0--------------S0     S0--------------1
	 *             U                         U
	 *
	 *        Quadrant 0            Quadrant 1
	 *
	 *
	 **********************************************************************/

	ON_Plane sharedframes[4];
	localsurf->FrameAt(usplit, v.Min(), sharedframes[0]);
	localsurf->FrameAt(u.Min(), vsplit, sharedframes[1]);
	localsurf->FrameAt(usplit, v.Max(), sharedframes[2]);
	localsurf->FrameAt(u.Max(), vsplit, sharedframes[3]);
	// When splitting via knots, we don't know what point frames[4] is until
	// the knot is selected
	if (prev_knot) localsurf->FrameAt(usplit, vsplit, frames[4]);

	ON_Plane *newframes;
	if (!m_f_queue->empty()) {
	    newframes = m_f_queue->front(); m_f_queue->pop();
	} else {
	    newframes = (ON_Plane *)bu_malloc(9*sizeof(ON_Plane), "new frames");
	}
	newframes[0] = frames[0];
	newframes[1] = sharedframes[0];
	newframes[2] = frames[4];
	newframes[3] = sharedframes[1];
	newframes[4] = frames[5];
	quads[0] = subdivideSurface(q0surf, firstu, firstv, newframes, divDepth+1, depthLimit, prev_knot, within_distance_tol);
	newframes[0] = sharedframes[0];
	newframes[1] = frames[1];
	newframes[2] = sharedframes[3];
	newframes[3] = frames[4];
	newframes[4] = frames[7];
	quads[1] = subdivideSurface(q1surf, secondu, firstv, newframes, divDepth+1, depthLimit, prev_knot, within_distance_tol);
	newframes[0] = frames[4];
	newframes[1] = sharedframes[3];
	newframes[2] = frames[2];
	newframes[3] = sharedframes[2];
	newframes[4] = frames[8];
	quads[2] = subdivideSurface(q2surf, secondu, secondv, newframes, divDepth+1, depthLimit, prev_knot, within_distance_tol);
	newframes[0] = sharedframes[1];
	newframes[1] = frames[4];
	newframes[2] = sharedframes[2];
	newframes[3] = frames[3];
	newframes[4] = frames[6];
	quads[3] = subdivideSurface(q3surf, firstu, secondv, newframes, divDepth+1, depthLimit, prev_knot, within_distance_tol);
	for (int i = 0; i < 9; i++) newframes[i] = ON_Plane();
	m_f_queue->push(newframes);

	parent->m_trimmed = true;
	parent->m_checkTrim = false;

	for (int i = 0; i < 4; i++) {
	    if (!quads[i]) {
		continue;
	    }
	    if (!(quads[i]->m_trimmed)) {
		parent->m_trimmed = false;
	    }
	    if (quads[i]->m_checkTrim) {
		parent->m_checkTrim = true;
	    }
	}
	if (m_removeTrimmed) {
	    for (int i = 0; i < 4; i++) {
		if (!quads[i]) {
		    continue;
		}
		if (!(quads[i]->m_trimmed)) {
		    parent->addChild(quads[i]);
		}
	    }
	} else {
	    for (int i = 0; i < 4; i++) {
		parent->addChild(quads[i]);
	    }
	}
	parent->BuildBBox();
	return parent;
    }
    //////////////////////////////////////
    if (do_u_split) {
	ON_Interval firstu(u.Min(), usplit);
	ON_Interval secondu(usplit, u.Max());

	const ON_Surface *east = localsurf;
	const ON_Surface *west = localsurf;

	//////////////////////////////////
	/*********************************************************************
	 * In order to avoid fairly expensive re-calculation of 3d points at
	 * uv coordinates, all values that are shared between children at
	 * the same depth of a surface subdivision are pre-computed and
	 * passed as parameters.
	 *
	 * The majority of these points are already evaluated in the process
	 * of testing whether a subdivision has produced a leaf node.  These
	 * values are in the normals and corners arrays and have index values
	 * corresponding to the values of the figure on the left below.  There
	 * are four other shared values that are precomputed in the sharedcorners
	 * and sharednormals arrays; their index values in those arrays are
	 * illustrated in the figure on the right:
	 *
	 *
	 *   3-------------------2      +---------2---------+
	 *   |                   |      |                   |
	 *   |    6         8    |      |                   |
	 *   |                   |      |                   |
	 *  V|         4         |      1                   3
	 *   |                   |      |                   |
	 *   |    5         7    |      |                   |
	 *   |                   |      |                   |
	 *   0-------------------1      +---------0---------+
	 *             U                          U
	 *
	 *   Values inherited from      Values pre-prepared in
	 *   parent subdivision         shared arrays
	 *
	 *
	 * When the four subdivisions are made, the parent parameters
	 * are passed to the children as follows (values from the
	 * shared arrays are prefaced with an s):
	 *
	 *    3--------------S1     S1--------------2
	 *    |               |     |               |
	 *    |               |     |               |
	 *  V |       4       |     |       5       |
	 *    |               |     |               |
	 *    |               |     |               |
	 *    0--------------S0     S0--------------1
	 *             U                         U
	 *
	 *        East                 West
	 *
	 *
	 **********************************************************************/
	/* When the four subdivisions are made, the parent parameters
	 * are passed to the children as follows (values from the
	 * shared arrays are prefaced with an s):
	 *
	 *    3--------------S2     S2--------------2
	 *    |               |     |               |
	 *    |               |     |               |
	 *  V |       6       |     |       8       |
	 *    |               |     |               |
	 *    |               |     |               |
	 *    S1--------------4     4--------------S3
	 *            U                     U
	 *
	 *        Quadrant 3            Quadrant 2
	 *
	 *    S1--------------4     4--------------S3
	 *    |               |     |               |
	 *    |               |     |               |
	 *  V |       5       |     |       7       |
	 *    |               |     |               |
	 *    |               |     |               |
	 *    0--------------S0     S0--------------1
	 *             U                         U
	 *
	 *        Quadrant 0            Quadrant 1
	 *
	 *
	 **********************************************************************/

	ON_Plane sharedframes[4];
	localsurf->FrameAt(usplit, v.Min(), sharedframes[0]);
	localsurf->FrameAt(usplit, v.Max(), sharedframes[1]);

	ON_Plane *newframes;
	if (!m_f_queue->empty()) {
	    newframes = m_f_queue->front(); m_f_queue->pop();
	} else {
	    newframes = (ON_Plane *)bu_malloc(9*sizeof(ON_Plane), "new frames");
	}
	newframes[0] = frames[0];
	newframes[1] = sharedframes[0];
	newframes[2] = sharedframes[1];
	newframes[3] = frames[3];

	if (prev_knot) {
	    localsurf->FrameAt(firstu.Mid(), v.Mid(), newframes[4]);
	    quads[0] = subdivideSurface(east, firstu, v, newframes, divDepth+1, depthLimit, prev_knot, within_distance_tol);
	} else {
	    ON_Interval first(0, 0.5);
	    localsurf->FrameAt(u.Mid() - uq, v.Mid(), newframes[4]);
	    quads[0] = subdivideSurface(east, u.ParameterAt(first), v, newframes, divDepth + 1, depthLimit, prev_knot, within_distance_tol);
	}

	newframes[0] = sharedframes[0];
	newframes[1] = frames[1];
	newframes[2] = frames[2];
	newframes[3] = sharedframes[1];

	if (prev_knot) {
	    localsurf->FrameAt(secondu.Mid(), v.Mid(), newframes[4]);
	    quads[1] = subdivideSurface(west, secondu, v, newframes, divDepth+1, depthLimit, prev_knot, within_distance_tol);
	} else {
	    ON_Interval second(0.5, 1.0);
	    localsurf->FrameAt(u.Mid() + uq, v.Mid(), newframes[4]);
	    quads[1] = subdivideSurface(west, u.ParameterAt(second), v, newframes, divDepth + 1, depthLimit, prev_knot, within_distance_tol);
	}

	for (int i = 0; i < 9; i++) newframes[i] = ON_Plane();
	m_f_queue->push(newframes);

	parent->m_trimmed = true;
	parent->m_checkTrim = false;

	for (int i = 0; i < 2; i++) {
	    if (!quads[i]) {
		continue;
	    }
	    if (!(quads[i]->m_trimmed)) {
		parent->m_trimmed = false;
	    }
	    if (quads[i]->m_checkTrim) {
		parent->m_checkTrim = true;
	    }
	}
	if (m_removeTrimmed) {
	    for (int i = 0; i < 2; i++) {
		if (!quads[i]) {
		    continue;
		}
		if (!(quads[i]->m_trimmed)) {
		    parent->addChild(quads[i]);
		}
	    }
	} else {
	    for (int i = 0; i < 2; i++) {
		parent->addChild(quads[i]);
	    }
	}
	parent->BuildBBox();
	return parent;
    }
    if (do_v_split || !prev_knot) {
	ON_Interval firstv(v.Min(), vsplit);
	ON_Interval secondv(vsplit, v.Max());

	const ON_Surface *north = localsurf;
	const ON_Surface *south = localsurf;
	//////////////////////////////////
	/*********************************************************************
	 * In order to avoid fairly expensive re-calculation of 3d points at
	 * uv coordinates, all values that are shared between children at
	 * the same depth of a surface subdivision are pre-computed and
	 * passed as parameters.
	 *
	 * The majority of these points are already evaluated in the process
	 * of testing whether a subdivision has produced a leaf node.  These
	 * values are in the normals and corners arrays and have index values
	 * corresponding to the values of the figure on the left below.  There
	 * are four other shared values that are precomputed in the sharedcorners
	 * and sharednormals arrays; their index values in those arrays are
	 * illustrated in the figure on the right:
	 *
	 *
	 *   3-------------------2      +---------2---------+
	 *   |                   |      |                   |
	 *   |    6         8    |      |                   |
	 *   |                   |      |                   |
	 *  V|         4         |      1                   3
	 *   |                   |      |                   |
	 *   |    5         7    |      |                   |
	 *   |                   |      |                   |
	 *   0-------------------1      +---------0---------+
	 *             U                          U
	 *
	 *   Values inherited from      Values pre-prepared in
	 *   parent subdivision         shared arrays
	 *
	 *
	 * When the four subdivisions are made, the parent parameters
	 * are passed to the children as follows (values from the
	 * shared arrays are prefaced with an s):
	 *
	 *    3--------------------2
	 *    |                    |
	 *    |                    |
	 *  V |         5          |
	 *    |                    |
	 *    |                    |
	 *    S0-------------------S1
	 *            U
	 *
	 *        North
	 *
	 *    S0-------------------S1
	 *    |                    |
	 *    |                    |
	 *  V |         4          |
	 *    |                    |
	 *    |                    |
	 *    0--------------------1
	 *             U
	 *
	 *        South
	 *
	 *
	 **********************************************************************/

	ON_Plane sharedframes[2];
	localsurf->FrameAt(u.Min(), vsplit, sharedframes[0]);
	localsurf->FrameAt(u.Max(), vsplit, sharedframes[1]);

	ON_Plane *newframes;
	if (!m_f_queue->empty()) {
	    newframes = m_f_queue->front(); m_f_queue->pop();
	} else {
	    newframes = (ON_Plane *)bu_malloc(9*sizeof(ON_Plane), "new frames");
	}
	newframes[0] = frames[0];
	newframes[1] = frames[1];
	newframes[2] = sharedframes[1];
	newframes[3] = sharedframes[0];
	if (prev_knot) {
	    localsurf->FrameAt(u.Mid(), firstv.Mid(), newframes[4]);
	    quads[0] = subdivideSurface(south, u, firstv, newframes, divDepth+1, depthLimit, prev_knot, within_distance_tol);
	} else {
	    ON_Interval first(0, 0.5);
	    localsurf->FrameAt(u.Mid(), v.Mid() - vq, newframes[4]);
	    quads[0] = subdivideSurface(south, u, v.ParameterAt(first), newframes, divDepth + 1, depthLimit, prev_knot, within_distance_tol);
	}

	newframes[0] = sharedframes[0];
	newframes[1] = sharedframes[1];
	newframes[2] = frames[2];
	newframes[3] = frames[3];

	if (prev_knot) {
	    localsurf->FrameAt(u.Mid(), secondv.Mid(), newframes[4]);
	    quads[1] = subdivideSurface(north, u, secondv, newframes, divDepth+1, depthLimit, prev_knot, within_distance_tol);
	} else {
	    ON_Interval second(0.5, 1.0);
	    localsurf->FrameAt(u.Mid(), v.Mid() + vq, newframes[4]);
	    quads[1] = subdivideSurface(north, u, v.ParameterAt(second), newframes, divDepth + 1, depthLimit, prev_knot, within_distance_tol);
	}

	for (int i = 0; i < 9; i++) newframes[i] = ON_Plane();
	m_f_queue->push(newframes);

	parent->m_trimmed = true;
	parent->m_checkTrim = false;

	for (int i = 0; i < 2; i++) {
	    if (!quads[i]) {
		continue;
	    }
	    if (!(quads[i]->m_trimmed)) {
		parent->m_trimmed = false;
	    }
	    if (quads[i]->m_checkTrim) {
		parent->m_checkTrim = true;
	    }
	}
	if (m_removeTrimmed) {
	    for (int i = 0; i < 2; i++) {
		if (!quads[i]) {
		    continue;
		}
		if (!(quads[i]->m_trimmed)) {
		    parent->addChild(quads[i]);
		}
	    }
	} else {
	    for (int i = 0; i < 2; i++) {
		parent->addChild(quads[i]);
	    }
	}

	parent->BuildBBox();
	return parent;
    }

    if (!do_both_splits && !do_u_split && !do_v_split) {
	(const_cast<ON_Surface *>(localsurf))->ClearBoundingBox();
	delete parent;
	return subdivideSurface(localsurf, u, v, frames, 0, depthLimit, 0, within_distance_tol);
    }

    // Should never get here
    return NULL;
}


bool
SurfaceTree::isFlat(const ON_Plane frames[9]) const
{
    return ON_Surface_IsFlat(frames, BREP_SURFACE_FLATNESS);
}


bool
SurfaceTree::isStraight(const ON_Plane frames[9]) const
{
    return ON_Surface_IsStraight(frames, BREP_SURFACE_FLATNESS);
}


bool
SurfaceTree::isFlatU(const ON_Plane frames[9]) const
{
    return ON_Surface_IsFlat_U(frames, BREP_SURFACE_FLATNESS);
}


bool
SurfaceTree::isFlatV(const ON_Plane frames[9]) const
{
    return ON_Surface_IsFlat_V(frames, BREP_SURFACE_FLATNESS);
}


//--------------------------------------------------------------------------------
// get_closest_point implementation

typedef struct _gcp_data {
    const ON_Surface* surf;
    ON_3dPoint pt;

    // for newton iteration
    ON_3dPoint S;
    ON_3dVector du;
    ON_3dVector dv;
    ON_3dVector duu;
    ON_3dVector dvv;
    ON_3dVector duv;
} GCPData;


bool
gcp_gradient(pt2d_t out_grad, GCPData& data, pt2d_t uv)
{
    bool evaluated = data.surf->Ev2Der(uv[0],
					    uv[1],
					    data.S,
					    data.du,
					    data.dv,
					    data.duu,
					    data.duv,
					    data.dvv); // calc S(u, v) dS/du dS/dv d2S/du2 d2S/dv2 d2S/dudv
    if (!evaluated) return false;
    out_grad[0] = 2 * (data.du * (data.S - data.pt));
    out_grad[1] = 2 * (data.dv * (data.S - data.pt));
    return true;
}


bool
gcp_newton_iteration(pt2d_t out_uv, const GCPData& data, pt2d_t grad, pt2d_t in_uv)
{
    ON_3dVector delta = data.S - data.pt;
    double g1du = 2 * (data.duu * delta) + 2 * (data.du * data.du);
    double g2du = 2 * (data.duv * delta) + 2 * (data.dv * data.du);
    double g1dv = g2du;
    double g2dv = 2 * (data.dvv * delta) + 2 * (data.dv * data.dv);
    mat2d_t jacob = { g1du, g1dv,
		      g2du, g2dv };
    mat2d_t inv_jacob;
    if (mat2d_inverse(inv_jacob, jacob)) {
	pt2d_t tmp;
	mat2d_pt2d_mul(tmp, inv_jacob, grad);
	pt2dsub(out_uv, in_uv, tmp);
	return true;
    } else {
	// XXX fix the error handling
	// std::cerr << "inverse failed!" << std::endl;
	return false;
    }
}


bool
get_closest_point(ON_2dPoint& outpt,
		  const ON_BrepFace& face,
		  const ON_3dPoint& point,
		  const SurfaceTree* tree,
		  double tolerance)
{
    int try_count = 0;
    bool delete_tree = false;
    bool found = false;
    double d_last = DBL_MAX;
    pt2d_t curr_grad = {0.0, 0.0};
    pt2d_t new_uv = {0.0, 0.0};
    GCPData data;
    data.surf = face.SurfaceOf();
    data.pt = point;

    TRACE("get_closest_point: " << PT(point));

    // get initial estimate
    const SurfaceTree* a_tree = tree;
    if (a_tree == NULL) {
	a_tree = new SurfaceTree(&face);
	delete_tree = true;
    }

    // If we don't have a workable tree, there's
    // no point in continuing
    if (!a_tree->Valid()) {
	if (delete_tree)
	    delete a_tree;
	return false;
    }

    ON_Interval u, v;
    ON_2dPoint est = a_tree->getClosestPointEstimate(point, u, v);
    pt2d_t uv = {est[0], est[1]};

    // do the newton iterations
    // terminates on 1 of 3 conditions:
    // 1. if the gradient falls below an epsilon (preferred :-)
    // 2. if the gradient diverges
    // 3. iterated MAX_FCP_ITERATIONS
try_again:
    int diverge_count = 0;
    for (int i = 0; i < BREP_MAX_FCP_ITERATIONS; i++) {
	assert(gcp_gradient(curr_grad, data, uv));

	ON_3dPoint p = data.surf->PointAt(uv[0], uv[1]);
	double d = p.DistanceTo(point);
	TRACE("dist: " << d);

	if (NEAR_EQUAL(d, d_last, tolerance)) {
	    found = true; break;
	} else if (d > d_last) {
	    TRACE("diverged!");
	    diverge_count++;
	}
	if (gcp_newton_iteration(new_uv, data, curr_grad, uv)) {
	    move(uv, new_uv);
	} else {
	    // iteration failed, nudge diagonally
	    uv[0] += VUNITIZE_TOL;
	    uv[1] += VUNITIZE_TOL;
	}
	d_last = d;
    }
    if (found) {
	// check to see if we've left the surface domain
	double l, h;
	data.surf->GetDomain(0, &l, &h);
	CLAMP(uv[0], l, h); // make sure in range

	data.surf->GetDomain(1, &l, &h);
	CLAMP(uv[1], l, h);

	outpt[0] = uv[0];
	outpt[1] = uv[1];
    } else {
	TRACE("FAILED TO FIND CLOSEST POINT!");
	// XXX: try the mid point of the domain -- HACK! but it seems to work!?
	if (try_count == 0) {
	    uv[0] = u.Mid();
	    uv[1] = v.Mid();
	    ++try_count;
	    goto try_again;
	}
    }

    if (delete_tree)
	delete a_tree;
    return found;
}


bool
sortX(const BRNode* first, const BRNode* second)
{
    point_t first_min, second_min;
    point_t first_max, second_max;

    first->GetBBox(first_min, first_max);
    second->GetBBox(second_min, second_max);

    if (first_min[X] < second_min[X])
	return true;
    else
	return false;
}


bool
sortY(const BRNode* first, const BRNode* second)
{
    point_t first_min, second_min;
    point_t first_max, second_max;

    first->GetBBox(first_min, first_max);
    second->GetBBox(second_min, second_max);

    if (first_min[Y] < second_min[Y])
	return true;
    else
	return false;
}


} /* end namespace */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
