/*                     B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @addtogroup g_ */
/** @{ */
/** @file brep.cpp
 *
 * Implementation of a generalized Boundary Representation (BREP)
 * primitive using the openNURBS library.
 *
 */

#include "common.h"

#include <vector>
#include <list>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

#include "assert.h"

#include "vmath.h"

#include "brep.h"
#include "dvec.h"

#include "raytrace.h"
#include "rtgeom.h"

#include "brep_debug.h"


#define BN_VMATH_PREFIX_INDICES 1
#define ROOT_TOL 1.E-7

/* uncomment to enable debug plotting */
/* #define PLOTTING 1 */


#ifdef __cplusplus
extern "C" {
#endif
    int rt_brep_prep(struct soltab *stp, struct rt_db_internal* ip, struct rt_i* rtip);
    void rt_brep_print(register const struct soltab *stp);
    int rt_brep_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead);
    void rt_brep_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp);
    void rt_brep_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp);
    int rt_brep_class(const struct soltab *stp, const fastf_t *min, const fastf_t *max, const struct bn_tol *tol);
    void rt_brep_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp);
    void rt_brep_free(register struct soltab *stp);
    int rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);
    int rt_brep_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);
    int rt_brep_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip);
    int rt_brep_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip);
    void rt_brep_ifree(struct rt_db_internal *ip);
    int rt_brep_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local);
    int rt_brep_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr);
    int rt_brep_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, const char **argv);
    int rt_brep_params(struct pc_pc_set *, const struct rt_db_internal *ip);
#ifdef __cplusplus
}
#endif


/* FIXME: fugly */
static int hit_count = 0;


/********************************************************************************
 * Auxiliary functions
 ********************************************************************************/


using namespace brlcad;

ON_Ray toXRay(struct xray* rp) {
    ON_3dPoint pt(rp->r_pt);
    ON_3dVector dir(rp->r_dir);
    return ON_Ray(pt, dir);
}


//--------------------------------------------------------------------------------
// specific
struct brep_specific*
brep_specific_new()
{
    return (struct brep_specific*)bu_calloc(1, sizeof(struct brep_specific), "brep_specific_new");
}


void
brep_specific_delete(struct brep_specific* bs)
{
    if (bs != NULL) {
	delete bs->bvh;
	bu_free(bs, "brep_specific_delete");
    }
}


//--------------------------------------------------------------------------------
// prep

bool
brep_pt_trimmed(pt2d_t pt, const ON_BrepFace& face) {
    bool retVal = false;
    TRACE1("brep_pt_trimmed: " << PT2(pt));
    // for each loop
    const ON_Surface* surf = face.SurfaceOf();
    double umin, umax;
    ON_2dPoint from, to;
    from.x = pt[0];
    from.y = to.y = pt[1];
    surf->GetDomain(0, &umin, &umax);
    to.x = umax + 1;
    ON_Line ray(from, to);
    int intersections = 0;
    // for (int i = 0; i < face.LoopCount(); i++) {
    // ON_BrepLoop* loop = face.Loop(i);
    // for each trim
    // for (int j = 0; j < loop->m_ti.Count(); j++) {
    // ON_BrepTrim& trim = face.Brep()->m_T[loop->m_ti[j]];
    // const ON_Curve* trimCurve = trim.TrimCurveOf();
    // intersections += brep_count_intersections(ray, trimCurve);
    // ray.IntersectCurve(trimCurve, intersections, 0.0001);
    // intersections += trimCurve->NumIntersectionsWith(ray);
    // }
    // }

    /* If we base trimming on the number of intersections with, rhino
     * generated curves won't raytrace.  In fact, we need to ignore
     * trimming for the time being, just return false.
     *
     * FIXME: figure out what this code does, and fix it for rhino
     * generated geometries. djg 4/16/08
     */

    // the point is trimmed if the # of intersections is even and non-zero
    retVal= (intersections > 0 && (intersections % 2) == 0);

    return retVal;
}


double
getVerticalTangent(const ON_Curve *curve, double min, double max) {
    double mid;
    ON_3dVector tangent;
    bool tanmin;

    tangent = curve->TangentAt(min);
    tanmin = (tangent[X] < 0.0);
    while ((max-min) > 0.00001) {
	mid = (max + min)/2.0;
	tangent = curve->TangentAt(mid);
	if (NEAR_ZERO(tangent[X], 0.00001)) {
	    return mid;
	}
	if ((tangent[X] < 0.0) == tanmin) {
	    min = mid;
	} else {
	    max = mid;
	}
    }
    return min;
}


double
getHorizontalTangent(const ON_Curve *curve, double min, double max) {
    double mid;
    ON_3dVector tangent;
    bool tanmin;

    tangent = curve->TangentAt(min);
    tanmin = (tangent[Y] < 0.0);
    while ((max-min) > 0.00001) {
	mid = (max + min)/2.0;
	tangent = curve->TangentAt(mid);
	if (NEAR_ZERO(tangent[Y], 0.00001)) {
	    return mid;
	}
	if ((tangent[Y] < 0.0) == tanmin) {
	    min = mid;
	} else {
	    max = mid;
	}
    }
    return min;
}


bool
split_trims_hv_tangent(const ON_Curve* curve, ON_Interval& t, std::list<double>& list) {
    bool tanx1, tanx2, tanx_changed;
    bool tany1, tany2, tany_changed;
    bool tan_changed;
    ON_3dVector tangent1, tangent2;
    ON_3dPoint p1, p2;

    tangent1 = curve->TangentAt(t[0]);
    tangent2 = curve->TangentAt(t[1]);

    tanx1 = (tangent1[X] < 0.0);
    tanx2 = (tangent2[X] < 0.0);
    tany1 = (tangent1[Y] < 0.0);
    tany2 = (tangent2[Y] < 0.0);

    tanx_changed =(tanx1 != tanx2);
    tany_changed =(tany1 != tany2);

    tan_changed = tanx_changed || tany_changed;

    if (tan_changed) {
	if (tanx_changed && tany_changed) {//horz & vert simply split
	    double midpoint = (t[1]+t[0])/2.0;
	    ON_Interval left(t[0], midpoint);
	    ON_Interval right(midpoint, t[1]);
	    split_trims_hv_tangent(curve, left, list);
	    split_trims_hv_tangent(curve, right, list);
	    return true;
	} else if (tanx_changed) {//find horz
	    double x = getVerticalTangent(curve, t[0], t[1]);
	    list.push_back(x);
	    M_COLOR_PLOT(DARKORANGE);
	} else { //find vert
	    double x = getHorizontalTangent(curve, t[0], t[1]);
	    list.push_back(x);
	    M_COLOR_PLOT(MAGENTA);
	}
    } else { // check point slope for change
	bool slopex, slopex_changed;
	bool slopey, slopey_changed;
	bool slope_changed;

	p1 = curve->PointAt(t[0]);
	p2 = curve->PointAt(t[1]);

	slopex = ((p2[X] - p1[X]) < 0.0);
	slopey = ((p2[Y] - p1[Y]) < 0.0);

	slopex_changed = (slopex != tanx1);
	slopey_changed = (slopey != tany1);

	slope_changed = slopex_changed || slopey_changed;

	if (slope_changed) {  //2 horz or 2 vert changes simply split
	    double midpoint = (t[1]+t[0])/2.0;
	    ON_Interval left(t[0], midpoint);
	    ON_Interval right(midpoint, t[1]);
	    split_trims_hv_tangent(curve, left, list);
	    split_trims_hv_tangent(curve, right, list);
	    return true;
	} else {
	    M_COLOR_PLOT(DARKGREEN);
	}
    }
    //plot color coded segment
    plottrim(*curve, t[0], t[1]);

    return true;
}


int
brep_build_bvh(struct brep_specific* bs)
{
    // First, run the openNURBS validity check on the brep in question
    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	bu_log("NULL Brep");
	return -1;
    } else {
	if (!brep->IsValid(&tl)) bu_log("brep is NOT valid\n");
    }

    /* May want to do something about setting orientation?  not used,
     * commented out for now CY 2009:
     */
    // int orientation = brep->SolidOrientation();

    /* Initialize the top level Bounding Box node for the entire
     * surface tree.  The purpose of this node is to provide a parent
     * node for the trees to be built on each BREP component surface.
     */
    bs->bvh = new BBNode(brep->BoundingBox());

    /* For each face in the brep, build its surface tree and add the root
     * node of that tree as a child of the bvh master node defined above.
     * A possible future refinement of this approach would be to build
     * a tree structure on top of the collection of surface trees based
     * on their 3D bounding volumes, as opposed to the current approach
     * of simply having all surfaces be child nodes of the master node.
     * This would allow a ray intersection to avoid checking every surface
     * tree bounding box, but should probably be undertaken only if this
     * step proves to be a bottleneck for raytracing.
     */
    ON_BrepFaceArray& faces = brep->m_F;
    for (int i = 0; i < faces.Count(); i++) {
	ON_BrepFace& face = faces[i];
	/*
	 * bu_log("Prepping Face: %d of %d\n", i+1, faces.Count());
	 */
	SurfaceTree* st = new SurfaceTree(&face);
	face.m_face_user.p = st;
	bs->bvh->addChild(st->getRootNode());
    }
    bs->bvh->BuildBBox();
    return 0;
}


/********************************************************************************
 * BRL-CAD Primitive interface
 ********************************************************************************/

/**
 * R T _ B R E P _ P R E P
 *
 * Given a pointer of a GED database record, and a transformation
 * matrix, determine if this is a valid NURB, and if so, prepare the
 * surface so the intersections will work.
 */
int
rt_brep_prep(struct soltab *stp, struct rt_db_internal* ip, struct rt_i* rtip)
{
    TRACE1("rt_brep_prep");
    /* This prepares the NURBS specific data structures to be used
     * during intersection... i.e. acceleration data structures and
     * whatever else is needed.
     */
    struct rt_brep_internal* bi;
    struct brep_specific* bs;
    const struct bn_tol *tol = &rtip->rti_tol;

    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    if ((bs = (struct brep_specific*)stp->st_specific) == NULL) {
	bs = (struct brep_specific*)bu_malloc(sizeof(struct brep_specific), "brep_specific");
	bs->brep = bi->brep;
	bi->brep = NULL;
	stp->st_specific = (genptr_t)bs;
    }

    /* The workhorse routines of BREP prep are called by brep_build_bvh
     */
    if (brep_build_bvh(bs) < 0) {
	return -1;
    }

    /* Once a proper SurfaceTree is built, finalize the bounding
     * volumes */
    bs->bvh->GetBBox(stp->st_min, stp->st_max);

    // expand outer bounding box just a little bit
    point_t adjust;
    VSETALL(adjust, tol->dist < SMALL_FASTF ? SMALL_FASTF : tol->dist);
    VSUB2(stp->st_min, stp->st_min, adjust);
    VADD2(stp->st_max, stp->st_max, adjust);

    VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
    vect_t work;
    VSUB2SCALE(work, stp->st_max, stp->st_min, 0.5);
    fastf_t f = work[X];
    V_MAX(f, work[Y]);
    V_MAX(f, work[Z]);
    stp->st_aradius = f;
    stp->st_bradius = MAGNITUDE(work);

    return 0;
}


/**
 * R T _ B R E P _ P R I N T
 */
void
rt_brep_print(const struct soltab *stp)
{
    struct brep_specific* bs;

    if (!stp)
	return;
    RT_CK_SOLTAB(stp);
    bs = (struct brep_specific*)stp->st_specific;
    if (!bs)
	return;
}


//================================================================================
// shot support

class plane_ray {
public:
    vect_t n1;
    fastf_t d1;

    vect_t n2;
    fastf_t d2;
};


void
brep_get_plane_ray(ON_Ray& r, plane_ray& pr)
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
    v1[index] += 1; // alter the smallest component
    VCROSS(pr.n1, v1, r.m_dir); // n1 is perpendicular to v1
    VUNITIZE(pr.n1);
    VCROSS(pr.n2, pr.n1, r.m_dir);       // n2 is perpendicular to v1 and n1
    VUNITIZE(pr.n2);
    pr.d1 = VDOT(pr.n1, r.m_origin);
    pr.d2 = VDOT(pr.n2, r.m_origin);
    TRACE1("n1:" << ON_PRINT3(pr.n1) << " n2:" << ON_PRINT3(pr.n2) << " d1:" << pr.d1 << " d2:" << pr.d2);
}


class brep_hit {
public:

    enum hit_type {
	CLEAN_HIT,
	CLEAN_MISS,
	NEAR_HIT,
	NEAR_MISS,
	CRACK_HIT //applied to first point of two near_miss points with same normal direction, second point removed
    };
    enum hit_direction {
	ENTERING,
	LEAVING
    };

    const ON_BrepFace& face;
    point_t origin;
    point_t point;
    vect_t normal;
    pt2d_t uv;
    bool trimmed;
    bool closeToEdge;
    bool oob;
    enum hit_type hit;
    enum hit_direction direction;
    int m_adj_face_index;
    // XXX - calculate the dot of the dir with the normal here!
    BBNode const * sbv;

    brep_hit(const ON_BrepFace& f, const point_t orig, const point_t p, const vect_t n, const pt2d_t _uv)
	: face(f), trimmed(false), closeToEdge(false), oob(false), sbv(NULL)
    {
	VMOVE(origin, orig);
	VMOVE(point, p);
	VMOVE(normal, n);
	move(uv, _uv);
    }

    brep_hit(const brep_hit& h)
	: face(h.face), trimmed(h.trimmed), closeToEdge(h.closeToEdge), oob(h.oob), sbv(h.sbv)
    {
	VMOVE(origin, h.origin);
	VMOVE(point, h.point);
	VMOVE(normal, h.normal);
	move(uv, h.uv);
	trimmed = h.trimmed;
	closeToEdge = h.closeToEdge;
	oob = h.oob;
	sbv = h.sbv;
	hit = h.hit;
	direction = h.direction;
    }

    brep_hit& operator=(const brep_hit& h)
    {
	const_cast<ON_BrepFace&>(face) = h.face;
	VMOVE(origin, h.origin);
	VMOVE(point, h.point);
	VMOVE(normal, h.normal);
	move(uv, h.uv);
	trimmed = h.trimmed;
	closeToEdge = h.closeToEdge;
	oob = h.oob;
	sbv = h.sbv;
	hit = h.hit;
	direction = h.direction;

	return *this;
    }

    bool operator==(const brep_hit& h)
    {
	return NEAR_ZERO(DIST_PT_PT(point, h.point), BREP_SAME_POINT_TOLERANCE);
    }

    bool operator<(const brep_hit& h)
    {
	return DIST_PT_PT(point, origin) < DIST_PT_PT(h.point, origin);
    }
};


typedef std::list<brep_hit> HitList;


void
brep_r(const ON_Surface* surf, plane_ray& pr, pt2d_t uv, ON_3dPoint& pt, ON_3dVector& su, ON_3dVector& sv, pt2d_t R)
{
    assert(surf->Ev1Der(uv[0], uv[1], pt, su, sv));
    R[0] = VDOT(pr.n1, ((fastf_t*)pt)) - pr.d1;
    R[1] = VDOT(pr.n2, ((fastf_t*)pt)) - pr.d2;
}


void
brep_newton_iterate(plane_ray& pr, pt2d_t R, ON_3dVector& su, ON_3dVector& sv, pt2d_t uv, pt2d_t out_uv)
{
    mat2d_t jacob = { VDOT(pr.n1, ((fastf_t*)su)), VDOT(pr.n1, ((fastf_t*)sv)),
		      VDOT(pr.n2, ((fastf_t*)su)), VDOT(pr.n2, ((fastf_t*)sv)) };
    mat2d_t inv_jacob;
    if (mat2d_inverse(inv_jacob, jacob)) {
	// check inverse validity
	pt2d_t tmp;
	mat2d_pt2d_mul(tmp, inv_jacob, R);
	pt2dsub(out_uv, uv, tmp);
    } else {
	TRACE2("inverse failed"); // XXX how to handle this?
	move(out_uv, uv);
    }
}


int
brep_getSurfacePoint(const ON_3dPoint& pt, ON_2dPoint& uv , BBNode* node) {
    plane_ray pr;
    const ON_Surface *surf = node->m_face->SurfaceOf();
    double umin, umax;
    double vmin, vmax;
    surf->GetDomain(0, &umin, &umax);
    surf->GetDomain(1, &vmin, &vmax);

    ON_3dVector dir = node->m_normal;
    dir.Reverse();
    ON_Ray ray((ON_3dPoint&)pt, dir);
    brep_get_plane_ray(ray, pr);

    //know use this as guess to iterate to closer solution
    pt2d_t Rcurr;
    pt2d_t new_uv;
    ON_3dVector su, sv;
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
	    if (new_uv[0] < umin) {
		new_uv[0] = umin;
	    } else if (new_uv[0] > umax) {
		new_uv[0] = umax;
	    }
	}
	if (surf->IsClosed(1)) {
	    if (new_uv[1] < vmin) {
		new_uv[1] = vmin;
	    } else if (new_uv[1] > vmax) {
		new_uv[1] = vmax;
	    }
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


	surf->EvNormal(new_uv[0], new_uv[1], newpt, ray.m_dir);
	ray.m_dir.Reverse();
	brep_get_plane_ray(ray, pr);

	if (d <  Dlast) {
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


typedef enum {
    BREP_INTERSECT_RIGHT_OF_EDGE = -5,
    BREP_INTERSECT_MISSED_EDGE = -4,
    BREP_INTERSECT_ROOT_ITERATION_LIMIT = -3,
    BREP_INTERSECT_ROOT_DIVERGED = -2,
    BREP_INTERSECT_OOB = -1,
    BREP_INTERSECT_TRIMMED = 0,
    BREP_INTERSECT_FOUND = 1
}


brep_intersect_reason_t;


HIDDEN const char*
BREP_INTERSECT_REASON(brep_intersect_reason_t index)
{
    static const char *reason[] = {
	"grazed to the right of the edge",
	"missed the edge altogether (outside tolerance)",
	"hit root iteration limit",
	"root diverged",
	"out of subsurface bounds",
	"trimmed",
	"found",
	"UNKNOWN"
    };

    return reason[index+5];
}


void
utah_F(const ON_3dPoint &S, const ON_3dVector &p1, const double p1d, const ON_3dVector &p2, const double p2d, double &f1, double &f2)
{
    f1 = (S * p1) + p1d;
    f2 = (S * p2) + p2d;
}


void
utah_Fu(const ON_3dVector &Su, const ON_3dVector &p1, const ON_3dVector &p2, double &d0, double &d1)
{
    d0 = Su * p1;
    d1 = Su * p2;
}


void
utah_Fv(const ON_3dVector &Sv, const ON_3dVector &p1, const ON_3dVector &p2, double &d0, double &d1)
{
    d0 = Sv * p1;
    d1 = Sv * p2;
}


void
utah_ray_planes(const ON_Ray &r, ON_3dVector &p1, double &p1d, ON_3dVector &p2, double &p2d)
{
    ON_3dPoint ro(r.m_origin);
    ON_3dVector rdir(r.m_dir);
    double rdx, rdy, rdz;
    double rdxmag, rdymag, rdzmag;

    rdx = rdir.x;
    rdy = rdir.y;
    rdz = rdir.z;

    rdxmag = fabs(rdx);
    rdymag = fabs(rdy);
    rdzmag = fabs(rdz);

    if (rdxmag > rdymag && rdxmag > rdzmag)
	p1 = ON_3dVector(rdy, -rdx, 0);
    else
	p1 = ON_3dVector(0, rdz, -rdy);
    // keith test
    p1.Unitize();
    //keith
    p2 = ON_CrossProduct(p1, rdir);

    p1d = -(p1 * ro);
    p2d = -(p2 * ro);
}


double
utah_calc_t(const ON_Ray &r, ON_3dPoint &S)
{
    ON_3dVector d(r.m_dir);
    ON_3dVector oS(S - r.m_origin);

    return (d * oS) / (d * d);
}


void
utah_pushBack(const ON_Surface* surf, ON_2dPoint &uv)
{
    double t0, t1;

    surf->GetDomain(0, &t0, &t1);
    if (uv.x < t0) {
	uv.x = t0;
    } else if (uv.x >= t1) {
	uv.x = t1 - ROOT_TOL;
    }

    surf->GetDomain(1, &t0, &t1);
    if (uv.y < t0) {
	uv.y = t0;
    } else if (uv.y >= t1) {
	uv.y = t1 - ROOT_TOL;
    }
}


int
utah_newton_solver_test(const BBNode* sbv, const ON_Surface* surf, const ON_Ray& r, ON_2dPoint* ouv, double* t, ON_3dVector* N, bool& converged, ON_2dPoint* suv, int count, int iu, int iv)
{
    int i;
    int intersects = 0;
    double j11, j12, j21, j22;
    double f, g;
    double rootdist, oldrootdist;
    double J, invdetJ;
    double du, dv;

    ON_3dVector p1, p2;
    double p1d = 0, p2d = 0;
    int errantcount = 0;
    utah_ray_planes(r, p1, p1d, p2, p2d);

    ON_3dPoint S;
    ON_3dVector Su, Sv;

    ON_2dPoint uv;

    uv.x = suv->x;
    uv.y = suv->y;

    ON_2dPoint uv0(uv);
    surf->Ev1Der(uv.x, uv.y, S, Su, Sv);

    utah_F(S, p1, p1d, p2, p2d, f, g);
    rootdist = fabs(f) + fabs(g);

    for (i = 0; i < BREP_MAX_ITERATIONS; i++) {
	utah_Fu(Su, p1, p2, j11, j21);
	utah_Fv(Sv, p1, p2, j12, j22);

	J = (j11 * j22 - j12 * j21);

	if (NEAR_ZERO(J, BREP_INTERSECTION_ROOT_EPSILON)) {
	    // perform jittered perturbation in parametric domain....
	    uv.x = uv.x + .1 * drand48() * (uv0.x - uv.x);
	    uv.y = uv.y + .1 * drand48() * (uv0.y - uv.y);
	    continue;
	}

	invdetJ = 1. / J;

	if ((iu != -1) && (iv != -1)) {
	    du = -invdetJ * (j22 * f - j12 * g);
	    dv = -invdetJ * (j11 * g - j21 * f);

	    if (i == 0) {
		if (((iu == 0) && (du < 0.0)) || ((iu==1) && (du > 0.0))) return intersects; //head out of U bounds
		if (((iv == 0) && (dv < 0.0)) || ((iv==1) && (dv > 0.0))) return intersects; //head out of V bounds
	    }
	}

	du = invdetJ * (j22 * f - j12 * g);
	dv = invdetJ * (j11 * g - j21 * f);
	uv.x -= du;
	uv.y -= dv;

	utah_pushBack(surf, uv);

	surf->Ev1Der(uv.x, uv.y, S, Su, Sv);
	utah_F(S, p1, p1d, p2, p2d, f, g);
	oldrootdist = rootdist;
	rootdist = fabs(f) + fabs(g);
	if (oldrootdist < rootdist) {
	    du *= 0.5;
	    dv *= 0.5;
	    uv.x += du;
	    uv.y += dv;

	    utah_pushBack(surf, uv);

	    surf->Ev1Der(uv.x, uv.y, S, Su, Sv);
	    utah_F(S, p1, p1d, p2, p2d, f, g);
	    oldrootdist = rootdist;
	    rootdist = fabs(f) + fabs(g);
	}

	if (oldrootdist < rootdist) {
	    if (errantcount > 3) {
		return intersects;
	    } else {
		errantcount++;
	    }
	}

	if (rootdist < ROOT_TOL) {
	    if (sbv->m_u.Includes(uv.x) && sbv->m_v.Includes(uv.y)) {
		bool new_point = true;
		for (int j=0;j<count;j++) {
		    if (NEAR_EQUAL(uv.x, ouv[j].x, 0.0001) && NEAR_EQUAL(uv.y, ouv[j].y, 0.0001)) {
			new_point = false;
		    }
		}
		if (new_point) {
		    //bu_log("New Hit Point:(%f %f %f) uv(%f, %f)\n", S.x, S.y, S.z, uv.x, uv.y);
		    t[count] = utah_calc_t(r, S);
		    N[count] = ON_CrossProduct(Su, Sv);
		    N[count].Unitize();
		    ouv[count].x = uv.x;
		    ouv[count].y = uv.y;
		    intersects++;
		    converged = true;
		}
	    }
	    return intersects;
	}
    }
    return intersects;
}


int
utah_newton_4corner_solver(const BBNode* sbv, const ON_Surface* surf, const ON_Ray& r, ON_2dPoint* ouv, double* t, ON_3dVector* N, bool& converged, int docorners)
{
    int intersects = 0;
    converged = false;
    if (docorners) {
	for (int iu = 0; iu < 2; iu++) {
	    for (int iv = 0; iv < 2; iv++) {
		ON_2dPoint uv;
		uv.x = sbv->m_u[iu];
		uv.y = sbv->m_v[iv];
		intersects += utah_newton_solver_test(sbv, surf, r, ouv, t, N, converged, &uv, intersects, iu, iv);
	    }
	}
    }

    ON_2dPoint uv;
    uv.x = sbv->m_u.Mid();
    uv.y = sbv->m_v.Mid();
    intersects += utah_newton_solver_test(sbv, surf, r, ouv, t, N, converged, &uv, intersects, -1, -1);
    return intersects;
}


void
utah_newton_solver(const ON_Surface* surf, const ON_Ray& r, ON_2dPoint &uv, double& t, ON_3dVector &N, bool& converged)
{
    int i;
    double j11, j12, j21, j22;
    double f, g;
    double rootdist, oldrootdist;
    double J, invdetJ;

    ON_3dVector p1, p2;
    double p1d = 0, p2d = 0;
    converged = false;
    utah_ray_planes(r, p1, p1d, p2, p2d);

    ON_3dPoint S;
    ON_3dVector Su, Sv;
    ON_2dPoint uv0(uv);

    surf->Ev1Der(uv.x, uv.y, S, Su, Sv);

    utah_F(S, p1, p1d, p2, p2d, f, g);
    rootdist = fabs(f) + fabs(g);

    for (i = 0; i < BREP_MAX_ITERATIONS; i++) {
	utah_Fu(Su, p1, p2, j11, j21);
	utah_Fv(Sv, p1, p2, j12, j22);

	J = (j11 * j22 - j12 * j21);

	if (NEAR_ZERO(J, BREP_INTERSECTION_ROOT_EPSILON)) {
	    // perform jittered perturbation in parametric domain....
	    uv.x = uv.x + .1 * drand48() * (uv0.x - uv.x);
	    uv.y = uv.y + .1 * drand48() * (uv0.y - uv.y);
	    continue;
	}

	invdetJ = 1. / J;

	uv.x -= invdetJ * (j22 * f - j12 * g);
	uv.y -= invdetJ * (j11 * g - j21 * f);

	utah_pushBack(surf, uv);

	surf->Ev1Der(uv.x, uv.y, S, Su, Sv);
	utah_F(S, p1, p1d, p2, p2d, f, g);
	oldrootdist = rootdist;
	rootdist = fabs(f) + fabs(g);

	if (oldrootdist < rootdist) return;

	if (rootdist < ROOT_TOL) {
	    t = utah_calc_t(r, S);
	    converged = true;
	    N = ON_CrossProduct(Su, Sv);
	    N.Unitize();

	    return;
	}
    }
}


bool
lines_intersect(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4)
{
    double tol = 1e-8;
    double A1 = y2-y1;
    double B1 = x1-x2;
    double C1 = A1*x1+B1*y1;

    double A2 = y4-y3;
    double B2 = x3-x4;
    double C2 = A2*x3+B2*y3;

    double det = A1*B2 - A2*B1;

    if (NEAR_ZERO(det, tol)) {
	return false;
    } else {
	double x = (B2*C1 - B1*C2)/det;
	double y = (A1*C2 - A2*C1)/det;

	if ((x >= std::min(x1, x2)) && (x <= std::max(x1, x2)) && (x >= std::min(x3, x4)) && (x <= std::max(x3, x4)) && (y >= std::min(y1, y2)) && (y <= std::max(y1, y2)) && (y >= std::min(y3, y4)) && (y <= std::max(y3, y4))) {
	    return true;
	}

	if (NEAR_EQUAL(x, x1, tol) && NEAR_EQUAL(y, y1, tol)) {
	    return true;
	}

	if (NEAR_EQUAL(x, x2, tol) && NEAR_EQUAL(y, y2, tol)) {
	    return true;
	}

	if (NEAR_EQUAL(x, x3, tol) && NEAR_EQUAL(y, y3, tol)) {
	    return true;
	}

	if (NEAR_EQUAL(x, x4, tol) && NEAR_EQUAL(y, y4, tol)) {
	    return true;
	}
    }

    return false;
}


bool
utah_isTrimmed(ON_2dPoint uv, const ON_BrepFace *face) {
    static bool approximationsInit = false;
    static const int MAX_CURVES = 10000;
    static const int MAX_NUMBEROFPOINTS = 1000;

    static bool curveApproximated[MAX_CURVES];
    static ON_3dPoint curveApproximations[MAX_CURVES][MAX_NUMBEROFPOINTS];

    ON_wString curveinfo;
    ON_TextLog log(curveinfo);

    if (!approximationsInit) {
	approximationsInit = true;
	for (int i = 0; i < MAX_CURVES; i++) {
	    curveApproximated[i] = false;
	}
    }

    if (face == NULL) {
	return false;
    }
    const ON_Surface* surf = face->SurfaceOf();
    if (surf == NULL) {
	return false;
    }
    TRACE1("utah_isTrimmed: " << uv);
    // for each loop
    for (int li = 0; li < face->LoopCount(); li++) {
	ON_BrepLoop* loop = face->Loop(li);
	if (loop == 0) {
	    continue;
	}
	// for each trim
	ON_3dPoint closestPoint;
	ON_3dVector tangent, kappa;
	double currentDistance = -10000.0;;
	ON_3dPoint hitPoint(uv.x, uv.y, 0.0);
	for (int lti = 0; lti < loop->TrimCount(); lti++) {
	    const ON_BrepTrim* trim = loop->Trim(lti);
	    if (0 == trim)
		continue;
	    const ON_Curve* trimCurve = face->Brep()->m_C2[trim->m_c2i];
	    if (trimCurve == 0) {
		continue;
	    }

	    // Uncomment the following to get a look at the summary report
	    // of a given trimming curve
	    /* trimCurve->Dump(log);
	       ON_String cinfo = ON_String(curveinfo);
	       const char *info = cinfo.Array();
	       bu_log("%s\n", info);
	    */

	    double closestT;
	    bool gotClosest = trimCurve->GetClosestPoint(hitPoint, &closestT);
	    if (!gotClosest) {
		// Someone needs to work on GetClosestPoint not to fail
		// It is failing on nurbs curves that aren't rational
		// For now if it fails we will use the approx. approach
		double shortestDistance;
		double t;
		ON_Interval domain = trimCurve->Domain();
		double step = (domain.m_t[1] - domain.m_t[0]) / (double) MAX_NUMBEROFPOINTS;
		if (!curveApproximated[trim->m_c2i]) {
		    curveApproximated[trim->m_c2i] = true;
		    t = domain.m_t[0];
		    for (int i = 0; i < MAX_NUMBEROFPOINTS; i++) {
			curveApproximations[trim->m_c2i][i] = trimCurve->PointAt(t);
			t += step;
		    }
		}
		closestT = t = domain.m_t[0];
		closestPoint = curveApproximations[trim->m_c2i][0];
		currentDistance = shortestDistance = closestPoint.DistanceTo(hitPoint);
		for (int i = 0; i < MAX_NUMBEROFPOINTS; i++) {
		    closestPoint = curveApproximations[trim->m_c2i][i];
		    currentDistance = closestPoint.DistanceTo(hitPoint);
		    if (currentDistance < shortestDistance) {
			closestT = t;
			shortestDistance = currentDistance;
		    }
		    t += step;
		}
	    }
	    ON_3dPoint testClosestPoint;
	    ON_3dVector testTangent, testKappa;
	    double testDistance;
	    trimCurve->EvCurvature(closestT, testClosestPoint, testTangent, testKappa);
	    testDistance = testClosestPoint.DistanceTo(hitPoint);
	    if ((currentDistance < 0.0) || (testDistance < currentDistance)) {
		closestPoint = testClosestPoint;
		tangent = testTangent;
		kappa = testKappa;
		currentDistance = testDistance;
	    }
	}
	if (currentDistance >= 0.0) {
	    ON_3dVector hitDirection(hitPoint.x-closestPoint.x, hitPoint.y-closestPoint.y, hitPoint.z-closestPoint.z);
	    double dot = (hitDirection * kappa);
	    //printf("closestT=%lf dot=%lf closestPoint=(%lf, %lf, %lf) hitPoint=(%lf, %lf, %lf) tangent=(%lf, %lf, %lf) kappa=(%lf, %lf, %lf) normal=(%lf, %lf, %lf) hitDirection=(%lf, %lf, %lf)\n", closestT, dot, closestPoint.x, closestPoint.y, closestPoint.z, hitPoint.x, hitPoint.y, hitPoint.z, tangent.x, tangent.y, tangent.z, kappa.x, kappa.y, kappa.z, normal.x, normal.y, normal.z, hitDirection.x, hitDirection.y, hitDirection.z);
	    if (((li == 0) && (dot < 0.0)) ||
		((li > 0) && (dot > 0.0))) {
		return true;
	    }
	}
    }
    return false;
}

#define MAX_BREP_SUBDIVISION_INTERSECTS 5
int
utah_brep_intersect_test(const BBNode* sbv, const ON_BrepFace* face, const ON_Surface* surf, pt2d_t& uv, ON_Ray& ray, HitList& hits)
{
    ON_3dVector N[MAX_BREP_SUBDIVISION_INTERSECTS];
    double t[MAX_BREP_SUBDIVISION_INTERSECTS];
    ON_2dPoint ouv[MAX_BREP_SUBDIVISION_INTERSECTS];
    int found = BREP_INTERSECT_ROOT_DIVERGED;
    bool converged = false;
    int numhits;

    double grazing_float = sbv->m_normal * ray.m_dir;

    if (fabs(grazing_float) < 0.2) {
	numhits = utah_newton_4corner_solver(sbv, surf, ray, ouv, t, N, converged, 1);
    } else {
	numhits = utah_newton_4corner_solver(sbv, surf, ray, ouv, t, N, converged, 0);
    }

    for (int i=0;i < numhits;i++) {
	fastf_t closesttrim;
	BRNode* trimBR = NULL;
	int trim_status = ((BBNode*)sbv)->isTrimmed(ouv[i], trimBR, closesttrim);
	if (converged && (t[i] > 1.e-2)) {
	    if (trim_status != 1) {
		ON_3dPoint _pt;
		ON_3dVector _norm(N[i]);
		_pt = ray.m_origin + (ray.m_dir*t[i]);
		if (face->m_bRev) {
		    //bu_log("Reversing normal for Face:%d\n", face->m_face_index);
		    _norm.Reverse();
		}
		hit_count += 1;
		uv[0] = ouv[i].x;
		uv[1] = ouv[i].y;
		brep_hit bh(*face, (const fastf_t*)ray.m_origin, (const fastf_t*)_pt, (const fastf_t*)_norm, uv);
		bh.trimmed = false;
		if (trimBR != NULL) {
		    bh.m_adj_face_index = trimBR->m_adj_face_index;
		} else {
		    bh.m_adj_face_index = -99;
		}
		if (fabs(closesttrim) < BREP_EDGE_MISS_TOLERANCE) {
		    bh.closeToEdge = true;
		    bh.hit = brep_hit::NEAR_HIT;
		} else {
		    bh.closeToEdge = false;
		    bh.hit = brep_hit::CLEAN_HIT;
		}
		if (VDOT(ray.m_dir, _norm) < 0.0)
		    bh.direction = brep_hit::ENTERING;
		else
		    bh.direction = brep_hit::LEAVING;
		bh.sbv = sbv;
		hits.push_back(bh);
		found = BREP_INTERSECT_FOUND;
	    }
#if 1
	    else if (fabs(closesttrim) < BREP_EDGE_MISS_TOLERANCE) {
		ON_3dPoint _pt;
		ON_3dVector _norm(N[i]);
		_pt = ray.m_origin + (ray.m_dir*t[i]);
		if (face->m_bRev) {
		    //bu_log("Reversing normal for Face:%d\n", face->m_face_index);
		    _norm.Reverse();
		}
		hit_count += 1;
		uv[0] = ouv[i].x;
		uv[1] = ouv[i].y;
		brep_hit bh(*face, (const fastf_t*)ray.m_origin, (const fastf_t*)_pt, (const fastf_t*)_norm, uv);
		bh.trimmed = true;
		bh.closeToEdge = true;
		if (trimBR != NULL) {
		    bh.m_adj_face_index = trimBR->m_adj_face_index;
		} else {
		    bh.m_adj_face_index = -99;
		}
		bh.hit = brep_hit::NEAR_MISS;
		if (VDOT(ray.m_dir, _norm) < 0.0)
		    bh.direction = brep_hit::ENTERING;
		else
		    bh.direction = brep_hit::LEAVING;
		bh.sbv = sbv;
		hits.push_back(bh);
		found = BREP_INTERSECT_FOUND;
	    }
#endif
	}
    }
    return found;
}


int
utah_brep_intersect(const BBNode* sbv, const ON_BrepFace* face, const ON_Surface* surf, pt2d_t uv, ON_Ray& ray, HitList& hits)
{
    ON_3dVector N;
    bool hit = false;
    double t;
    ON_2dPoint ouv(uv[0], uv[1]);
    int found = BREP_INTERSECT_ROOT_DIVERGED;
    bool converged = false;
    fastf_t closesttrim;

    utah_newton_solver(surf, ray, ouv, t, N, converged);
    /*
     * DDR.  The utah people are using this t_min which represents the
     * last point hit along the ray to ensure we are looking at points
     * futher down the ray.  I haven't implemented this I'm not sure
     * we need it
     *
     * if (converged && (t > 1.e-2) && (t < t_min) && (!utah_isTrimmed(ouv, face))) hit = true;
     *
     */
    //if (converged && (t > 1.e-2) && (!utah_isTrimmed(ouv, face))) hit = true;
    //if (converged && (t > 1.e-2) && (!((BBNode*)sbv)->isTrimmed(ouv))) hit = true;

    if ((sbv->m_u[0] < ouv[0]) && (sbv->m_u[1] > ouv[0]) &&
	(sbv->m_v[0] < ouv[1]) && (sbv->m_v[1] > ouv[1])) {
	BRNode* trimBR = NULL;
	int trim_status = ((BBNode*)sbv)->isTrimmed(ouv, trimBR, closesttrim);
	if (converged && (t > 1.e-2)) {
	    if (trim_status != 1) {
		hit = true;
//#define KHITPLOT
#ifdef KHITPLOT
		double min[3], max[3];
		COLOR_PLOT(255, 200, 200);
		VSET(min, ouv[0]-0.01, ouv[1]-0.01, 0.0);
		VSET(max, ouv[0]+0.01, ouv[1]+0.01, 0.0);
		BB_PLOT(min, max);
	    } else {
		double min[3], max[3];
		COLOR_PLOT(200, 255, 200);
		VSET(min, ouv[0]-0.01, ouv[1]-0.01, 0.0);
		VSET(max, ouv[0]+0.01, ouv[1]+0.01, 0.0);
		BB_PLOT(min, max);
#endif
	    }
	}
    }

    uv[0] = ouv.x;
    uv[1] = ouv.y;

    if (hit) {
	ON_3dPoint _pt;
	ON_3dVector _norm(N);
	_pt = ray.m_origin + (ray.m_dir*t);
	if (face->m_bRev) _norm.Reverse();
	hit_count += 1;
	hits.push_back(brep_hit(*face, (const fastf_t*)ray.m_origin, (const fastf_t*)_pt, (const fastf_t*)_norm, uv));
	hits.back().sbv = sbv;
	found = BREP_INTERSECT_FOUND;
    }

    return found;
}


int
brep_intersect(const BBNode* sbv, const ON_BrepFace* face, const ON_Surface* surf, pt2d_t uv, ON_Ray& ray, HitList& hits)
{
    int found = BREP_INTERSECT_ROOT_ITERATION_LIMIT;
    fastf_t Dlast = MAX_FASTF;
    int diverge_iter = 0;
    pt2d_t Rcurr;
    pt2d_t new_uv;
    ON_3dPoint pt;
    ON_3dVector su;
    ON_3dVector sv;
    plane_ray pr;
    fastf_t closesttrim;

    brep_get_plane_ray(ray, pr);

    for (int i = 0; i < BREP_MAX_ITERATIONS; i++) {
	brep_r(surf, pr, uv, pt, su, sv, Rcurr);

	fastf_t d = v2mag(Rcurr);
	if (d < BREP_INTERSECTION_ROOT_EPSILON) {
	    TRACE1("R:"<<ON_PRINT2(Rcurr));
	    found = BREP_INTERSECT_FOUND; break;
	} else if (d > Dlast) {
	    found = BREP_INTERSECT_ROOT_DIVERGED; //break;
	    diverge_iter++;
	    if (diverge_iter > 10)
		break;
	}

	brep_newton_iterate(pr, Rcurr, su, sv, uv, new_uv);
	move(uv, new_uv);
	Dlast = d;
    }

    BRNode* trimBR = NULL;
    int trim_status = ((BBNode*)sbv)->isTrimmed(uv, trimBR, closesttrim);
    if ((found > 0) &&  (trim_status != 1)) {
	ON_3dPoint _pt;
	ON_3dVector _norm;
	surf->EvNormal(uv[0], uv[1], _pt, _norm);
	if (face->m_bRev) _norm.Reverse();
	hits.push_back(brep_hit(*face, (const fastf_t*)ray.m_origin, (const fastf_t*)_pt, (const fastf_t*)_norm, uv));
	hits.back().sbv = sbv;

	if (!sbv->m_u.Includes(uv[0]) || !sbv->m_v.Includes(uv[1])) {
	    hits.back().oob = true;
	    return BREP_INTERSECT_OOB;
	}
    }

    return found;
}


typedef std::pair<int, int> ip_t;
typedef std::list<ip_t> MissList;

HIDDEN int
sign(double val)
{
    return (val >= 0.0) ? 1 : -1;
}


bool
containsNearMiss(HitList *hits)
{
    for (HitList::iterator i = hits->begin(); i != hits->end(); ++i) {
	brep_hit&out = *i;
	if (out.hit == brep_hit::NEAR_MISS) {
	    return true;
	}
    }
    return false;
}


bool
containsNearHit(HitList *hits)
{
    for (HitList::iterator i = hits->begin(); i != hits->end(); ++i) {
	brep_hit&out = *i;
	if (out.hit == brep_hit::NEAR_HIT) {
	    return true;
	}
    }
    return false;
}


/**
 * R T _ B R E P _ S H O T
 *
 * Intersect a ray with a brep.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_brep_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct brep_specific* bs;

    if (!stp)
	return 0;
    RT_CK_SOLTAB(stp);
    bs = (struct brep_specific*)stp->st_specific;
    if (!bs)
	return 0;

    /* First, test for intersections between the Surface Tree
     * hierarchy and the ray - if one or more leaf nodes are
     * intersected, there is potentially a hit and more evaluation is
     * needed.  Otherwise, return a miss.
     */
    std::list<BBNode*> inters;
    ON_Ray r = toXRay(rp);
    bs->bvh->intersectsHierarchy(r, inters);
    if (inters.size() == 0) return 0; // MISS

    // find all the hits (XXX very inefficient right now!)
    HitList all_hits; // record all hits
    MissList misses;
    int s = 0;
    hit_count = 0;
    for (std::list<BBNode*>::iterator i = inters.begin(); i != inters.end(); i++) {
	const BBNode* sbv = (*i);
	const ON_BrepFace* f = sbv->m_face;
	const ON_Surface* surf = f->SurfaceOf();
	pt2d_t uv = {sbv->m_u.Mid(), sbv->m_v.Mid()};
	utah_brep_intersect_test(sbv, f, surf, uv, r, all_hits);
	s++;
    }

#ifdef KDEBUGMISS
    //(void)fclose(_plot_file());
    //	plot = NULL;
#endif
    HitList hits = all_hits;

    // sort the hits
    hits.sort();
    HitList orig = hits;
////////////////////////
    if ((hits.size() > 1) && containsNearMiss(&hits)) { //&& ((hits.size() % 2) != 0)) {
/*
  bu_log("**** Before Pass1 Hits: %d\n", hits.size());

  for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
  point_t prev;

  brep_hit &out = *i;

  if (i != hits.begin()) {
  bu_log("<%g>", DIST_PT_PT(out.point, prev));
  }
  bu_log("(");
  if (out.hit == brep_hit::CLEAN_HIT) bu_log("-CH-(%d)", out.face.m_face_index);
  if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
  if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
  if (out.direction == brep_hit::ENTERING) bu_log("+");
  if (out.direction == brep_hit::LEAVING) bu_log("-");
  VMOVE(prev, out.point);
  bu_log(")");
  }
*/
	HitList::iterator prev;
	HitList::iterator next;
	HitList::iterator curr = hits.begin();
	while (curr != hits.end()) {
	    brep_hit &curr_hit = *curr;
	    if (curr_hit.hit == brep_hit::NEAR_MISS) {
		if (curr != hits.begin()) {
		    prev = curr;
		    prev--;
		    brep_hit &prev_hit = (*prev);
		    if ((prev_hit.hit != brep_hit::NEAR_MISS) && (prev_hit.direction == curr_hit.direction)) {
			//remove current miss
			curr=hits.erase(curr);
			curr=hits.begin(); //rewind and start again
			continue;
		    }
		}
		next=curr;
		next++;
		if (next != hits.end()) {
		    brep_hit &next_hit = (*next);
		    if ((next_hit.hit != brep_hit::NEAR_MISS) && (next_hit.direction == curr_hit.direction)) {
			//remove current miss
			curr=hits.erase(curr);
			curr=hits.begin(); //rewind and start again
			continue;
		    }
		}
	    }
	    curr++;
	}
#if 0
	bu_log("**** After Pass1 Hits: %d\n", hits.size());

	for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
	    point_t prev;

	    brep_hit &out = *i;

	    if (i != hits.begin()) {
		bu_log("<%g>", DIST_PT_PT(out.point, prev));
	    }
	    bu_log("(");
	    if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
	    if (out.direction == brep_hit::ENTERING) bu_log("+");
	    if (out.direction == brep_hit::LEAVING) bu_log("-");
	    VMOVE(prev, out.point);
	    bu_log(")");
	}
#endif
	// check for crack hits between adjacent faces
	curr = hits.begin();
	while (curr != hits.end()) {
	    brep_hit &curr_hit = *curr;
	    if (curr_hit.hit == brep_hit::NEAR_MISS) {
		if (curr != hits.begin()) {
		    prev = curr;
		    prev--;
		    brep_hit &prev_hit = (*prev);
		    if ((prev_hit.hit == brep_hit::NEAR_MISS) && (prev_hit.direction == curr_hit.direction)) {
			//remove current miss
			prev_hit.hit = brep_hit::CRACK_HIT;
			curr=hits.erase(curr);
			continue;
		    } else if ((prev_hit.hit == brep_hit::NEAR_MISS) && (prev_hit.direction != curr_hit.direction)) {
			//remove edge near miss
			prev_hit.hit = brep_hit::CRACK_HIT;
			(void)hits.erase(prev);
			curr=hits.erase(curr);
			continue;
		    }
		}
	    }
	    curr++;
	}
	// check for CH double enter or double leave between adjacent faces(represents overlapping faces)
	curr = hits.begin();
	while (curr != hits.end()) {
	    brep_hit &curr_hit = *curr;
	    if (curr_hit.hit == brep_hit::CLEAN_HIT) {
		if (curr != hits.begin()) {
		    prev = curr;
		    prev--;
		    brep_hit &prev_hit = (*prev);
		    if ((prev_hit.hit == brep_hit::CLEAN_HIT) &&
			(prev_hit.direction == curr_hit.direction) &&
			(prev_hit.face.m_face_index == curr_hit.m_adj_face_index)) {
			// if "entering" remove first hit if "existing" remove second hit
			// until we get good solids with known normal directions assume
			// first hit direction is "entering" todo check solid status and normals
			HitList::iterator first = hits.begin();
			brep_hit &first_hit = *first;
			if (first_hit.direction == curr_hit.direction) { // assume "entering"
			    curr=hits.erase(prev);
			} else { // assume "exiting"
			    curr=hits.erase(curr);
			}
			continue;
		    }
		}
	    }
	    curr++;
	}
#if 0
	//debugging print
	bu_log("**** After Pass2 Hits: %d\n", hits.size());

	for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
	    point_t prev;

	    brep_hit &out = *i;

	    if (i != hits.begin()) {
		bu_log("<%g>", DIST_PT_PT(out.point, prev));
	    }
	    bu_log("(");
	    if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
	    if (out.direction == brep_hit::ENTERING) bu_log("+");
	    if (out.direction == brep_hit::LEAVING) bu_log("-");
	    VMOVE(prev, out.point);
	    bu_log(")");
	}

	bu_log("\n**********************\n");
#endif
	if ((hits.size() > 0) && ((hits.size() % 2) != 0)) {
	    brep_hit &curr_hit = hits.back();
	    if (curr_hit.hit == brep_hit::NEAR_MISS) {
		hits.pop_back();
	    }
	}

	if ((hits.size() > 0) && ((hits.size() % 2) != 0)) {
	    brep_hit &curr_hit = hits.front();
	    if (curr_hit.hit == brep_hit::NEAR_MISS) {
		hits.pop_front();
	    }
	}
/*
  bu_log("**** After Pass3 Hits: %d\n", hits.size());

  for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
  point_t prev;

  brep_hit &out = *i;

  if (i != hits.begin()) {
  bu_log("<%g>", DIST_PT_PT(out.point, prev));
  }
  bu_log("(");
  if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
  if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
  if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
  if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
  if (out.direction == brep_hit::ENTERING) bu_log("+");
  if (out.direction == brep_hit::LEAVING) bu_log("-");
  VMOVE(prev, out.point);
  bu_log(")");
  }

  bu_log("\n**********************\n");
*/
    }
    ///////////// handle near hit
    if ((hits.size() > 1) && containsNearHit(&hits)) { //&& ((hits.size() % 2) != 0)) {
	HitList::iterator prev;
	HitList::iterator next;
	HitList::iterator curr = hits.begin();
	while (curr != hits.end()) {
	    brep_hit &curr_hit = *curr;
	    if (curr_hit.hit == brep_hit::NEAR_HIT) {
		if (curr != hits.begin()) {
		    prev = curr;
		    prev--;
		    brep_hit &prev_hit = (*prev);
		    if ((prev_hit.hit != brep_hit::NEAR_HIT) && (prev_hit.direction == curr_hit.direction)) {
			//remove current miss
			curr=hits.erase(curr);
			continue;
		    }
		}
		next=curr;
		next++;
		if (next != hits.end()) {
		    brep_hit &next_hit = (*next);
		    if ((next_hit.hit != brep_hit::NEAR_HIT) && (next_hit.direction == curr_hit.direction)) {
			//remove current miss
			curr=hits.erase(curr);
			continue;
		    }
		}
	    }
	    curr++;
	}
	curr = hits.begin();
	while (curr != hits.end()) {
	    brep_hit &curr_hit = *curr;
	    if (curr_hit.hit == brep_hit::NEAR_HIT) {
		if (curr != hits.begin()) {
		    prev = curr;
		    prev--;
		    brep_hit &prev_hit = (*prev);
		    if ((prev_hit.hit == brep_hit::NEAR_HIT) && (prev_hit.direction == curr_hit.direction)) {
			//remove current near hit
			prev_hit.hit = brep_hit::CRACK_HIT;
			curr=hits.erase(curr);
			continue;
		    }
		}
	    }
	    curr++;
	}
	//    bu_log("**** After Pass2 Hits: %d\n", hits.size());

	if ((hits.size() > 0) && ((hits.size() % 2) != 0)) {
	    brep_hit &curr_hit = hits.back();
	    if (curr_hit.hit == brep_hit::NEAR_HIT) {
		hits.pop_back();
	    }
	}
    }
    ///////////// near hit end
    if (false) { //((hits.size() % 2) != 0) {
	bu_log("**** After Pass3 Hits: %zu\n", hits.size());

	for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
	    point_t prev;

	    brep_hit &out = *i;

	    if (i != hits.begin()) {
		bu_log("<%g>", DIST_PT_PT(out.point, prev));
	    }
	    bu_log("(");
	    if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
	    if (out.direction == brep_hit::ENTERING) bu_log("+");
	    if (out.direction == brep_hit::LEAVING) bu_log("-");
	    VMOVE(prev, out.point);
	    bu_log(")");
	}
	bu_log("\n**** Orig Hits: %zu\n", orig.size());

	for (HitList::iterator i = orig.begin(); i != orig.end(); ++i) {
	    point_t prev;

	    brep_hit &out = *i;

	    if (i != orig.begin()) {
		bu_log("<%g>", DIST_PT_PT(out.point, prev));
	    }
	    bu_log("(");
	    if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
	    if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
	    if (out.direction == brep_hit::ENTERING) bu_log("+");
	    if (out.direction == brep_hit::LEAVING) bu_log("-");
	    VMOVE(prev, out.point);
	    bu_log(")");
	}

	bu_log("\n**********************\n");

    }

    all_hits.clear();
    all_hits = hits;

    ////////////////////////
    TRACE("---");
    int num = 0;
    for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
	if ((i->trimmed && !i->closeToEdge) || i->oob || NEAR_ZERO(VDOT(i->normal, rp->r_dir), RT_DOT_TOL)) {
	    // remove what we were removing earlier
	    if (i->oob) {
		TRACE("\toob u: " << i->uv[0] << ", " << IVAL(i->sbv->m_u));
		TRACE("\toob v: " << i->uv[1] << ", " << IVAL(i->sbv->m_v));
	    }
	    i = hits.erase(i);

	    if (i != hits.begin())
		--i;

	    continue;
	}
	TRACE("hit " << num << ": " << PT(i->point) << " [" << VDOT(i->normal, rp->r_dir) << "]");
	++num;
    }

    if (hits.size() > 0) {
	// we should have "valid" points now, remove duplicates or grazes
	HitList::iterator last = hits.begin();
	HitList::iterator i = hits.begin();
	++i;
	while (i != hits.end()) {
	    if ((*i) == (*last)) {
		double lastDot = VDOT(last->normal, rp->r_dir);
		double iDot = VDOT(i->normal, rp->r_dir);

		if (sign(lastDot) != sign(iDot)) {
		    // delete them both
		    i = hits.erase(last);
		    i = hits.erase(i);
		    last = i;

		    if (i != hits.end())
			++i;
		} else {
		    // just delete the second
		    i = hits.erase(i);
		}
	    } else {
		last = i;
		++i;
	    }
	}
    }
    // remove multiple "INs" in a row assume last "IN" is the actual entering hit, for
    // multiple "OUTs" in a row assume first "OUT" is the actual exiting hit, remove unused
    // "INs/OUTs" from hit list.
    //if ((hits.size() > 0) && ((hits.size() % 2) != 0)) {
    if (hits.size() > 0) {
	// we should have "valid" points now, remove duplicates or grazes
	HitList::iterator last = hits.begin();
	HitList::iterator i = hits.begin();
	++i;
	int entering=1;
	while (i != hits.end()) {
	    double lastDot = VDOT(last->normal, rp->r_dir);
	    double iDot = VDOT(i->normal, rp->r_dir);

	    if (i == hits.begin()) {
		//take this as the entering sign for now, should be checking solid for
		// inward or outward facing normals and make determination there but to
		// much unsolid geom right now.
		entering = sign(iDot);
	    }
	    if (sign(lastDot) == sign(iDot)) {
		if (sign(iDot) == entering) {
		    i = hits.erase(last);
		    last = i;
		    if (i != hits.end())
			++i;
		} else { //exiting
		    i = hits.erase(i);
		}

	    } else {
		last = i;
		++i;
	    }
	}
    }

    if ((hits.size() > 1) && ((hits.size() % 2) != 0)) {
	brep_hit &first_hit = hits.front();
	brep_hit &last_hit = hits.back();
	double firstDot = VDOT(first_hit.normal, rp->r_dir);
	double lastDot = VDOT(last_hit.normal, rp->r_dir);
	if (sign(firstDot) == sign(lastDot)) {
	    hits.pop_back();
	}
    }

#if 0
    if (false) {
	if (hits.size() > 1 && (hits.size() % 2) != 0) {
	    bu_log("**** ERROR odd number of hits: %d\n", hits.size());
	    for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
		brep_hit&out = *i;
		bu_log("(");
		if (out.hit == brep_hit::CLEAN_HIT) bu_log("H");
		if ((out.hit == brep_hit::NEAR_HIT) || (out.hit == brep_hit::NEAR_MISS)) bu_log("c");
		if (out.direction == brep_hit::ENTERING) bu_log("+");
		if (out.direction == brep_hit::LEAVING) bu_log("-");
		bu_log(")");
	    }
	    bu_log("\n");

	    bu_log("xyz %g %g %g \n", rp->r_pt[0], rp->r_pt[1], rp->r_pt[2]);
	    bu_log("dir %g %g %g \n", rp->r_dir[0], rp->r_dir[1], rp->r_dir[2]);
	    if ((hits.size() % 2) != 0) {
		bu_log("**** Current Hits: %d\n", hits.size());

		for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
		    point_t prev;

		    brep_hit &out = *i;

		    if (i != hits.begin()) {
			bu_log("<%g>", DIST_PT_PT(out.point, prev));
		    }
		    bu_log("(");
		    if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
		    if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
		    if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
		    if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
		    if (out.direction == brep_hit::ENTERING) bu_log("+");
		    if (out.direction == brep_hit::LEAVING) bu_log("-");
		    VMOVE(prev, out.point);
		    bu_log(")");
		}
		bu_log("\n**** Orig Hits: %d\n", orig.size());

		for (HitList::iterator i = orig.begin(); i != orig.end(); ++i) {
		    point_t prev;

		    brep_hit &out = *i;

		    if (i != orig.begin()) {
			bu_log("<%g>", DIST_PT_PT(out.point, prev));
		    }
		    bu_log("(");
		    if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
		    if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
		    if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
		    if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
		    if (out.direction == brep_hit::ENTERING) bu_log("+");
		    if (out.direction == brep_hit::LEAVING) bu_log("-");
		    //bu_log("<%d>", out.sbv->m_face->m_bRev);
		    VMOVE(prev, out.point);
		    bu_log(")");
		}

		bu_log("\n**********************\n");

	    }
	}
	point_t last_point;
	int hitCount = 0;
	for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
	    if (hitCount == 0) {
		TRACE2("point: " << i->point[0] << ", " << i->point[1] << ", " << i->point[2] << " dist_to_ray: " << DIST_PT_PT(i->point, rp->r_pt));
	    } else {
		TRACE2("point: " << i->point[0] << ", " << i->point[1] << ", " << i->point[2] << " dist_to_ray: " << DIST_PT_PT(i->point, rp->r_pt) << " dist_to_last_point: " << DIST_PT_PT(i->point, last_point));
	    }
	    VMOVE(last_point, i->point);
	    hitCount += 1;
	}
#ifdef PLOTTING
	pcount++;
	if (pcount > -1) {
	    point_t ray;
	    point_t vscaled;
	    VSCALE(vscaled, rp->r_dir, 100);
	    VADD2(ray, rp->r_pt, vscaled);
	    COLOR_PLOT(200, 200, 200);
	    LINE_PLOT(rp->r_pt, ray);
	}
#endif
	all_hits.clear();
	all_hits = hits;

	num = 0;
	MissList::iterator m = misses.begin();
	for (HitList::iterator i = all_hits.begin(); i != all_hits.end(); ++i) {

#ifdef PLOTTING
	    if (pcount > -1) {
		// set the color of point and normal
		if (i->trimmed && i->closeToEdge) {
		    COLOR_PLOT(0, 0, 255); // blue is trimmed but close to edge
		} else if (i->trimmed) {
		    COLOR_PLOT(255, 255, 0); // yellow trimmed
		} else if (i->oob) {
		    COLOR_PLOT(255, 0, 0); // red is oob
		} else if (NEAR_ZERO(VDOT(i->normal, rp->r_dir), RT_DOT_TOL)) {
		    COLOR_PLOT(0, 255, 255); // purple is grazing
		} else {
		    COLOR_PLOT(0, 255, 0); // green is regular surface
		}

		// draw normal
		point_t v;
		VADD2(v, i->point, i->normal);
		LINE_PLOT(i->point, v);

		// draw intersection
		PT_PLOT(i->point);

		// draw bounding box
		BB_PLOT(i->sbv->m_node.m_min, i->sbv->m_node.m_max);
	    }
#endif

	    TRACE("hit " << num << ": " << ON_PRINT3(i->point) << " [" << dot << "]");
	    while ((m != misses.end()) && (m->first == num)) {
		static int reasons = 0;
		if (reasons < 100) {
		    reasons++;
		    TRACE("miss " << num << ": " << BREP_INTERSECT_REASON(m->second));
		} else {
		    static int quelled = 0;
		    if (!quelled) {
			TRACE("Too many reasons.  Suppressing further output." << std::endl);
			quelled = 1;
		    }
		}
		++m;
	    }
	    num++;
	}
	while (m != misses.end()) {
	    static int reasons = 0;
	    if (reasons < 100) {
		reasons++;
		TRACE("miss " << BREP_INTERSECT_REASON(m->second));
	    } else {
		static int quelled = 0;
		if (!quelled) {
		    TRACE("Too many reasons.  Suppressing further output." << std::endl);
		    quelled = 1;
		}
	    }
	    ++m;
	}
    }
#endif

    bool hit = false;
    if (hits.size() > 1) {

#if 0
	if (false) {
	    //TRACE2("screen xy: " << ap->a_x << ", " << ap->a_y);
	    bu_log("**** ERROR odd number of hits: %d\n", hits.size());
	    bu_log("xyz %g %g %g \n", rp->r_pt[0], rp->r_pt[1], rp->r_pt[2]);
	    bu_log("dir %g %g %g \n", rp->r_dir[0], rp->r_dir[1], rp->r_dir[2]);
	    bu_log("**** Current Hits: %d\n", hits.size());

	    for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
		point_t prev;

		brep_hit &out = *i;

		if (i != hits.begin()) {
		    bu_log("<%g>", DIST_PT_PT(out.point, prev));
		}
		bu_log("(");
		if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
		if (out.direction == brep_hit::ENTERING) bu_log("+");
		if (out.direction == brep_hit::LEAVING) bu_log("-");
		VMOVE(prev, out.point);
		bu_log(")");
	    }
	    bu_log("\n**** Orig Hits: %d\n", orig.size());

	    for (HitList::iterator i = orig.begin(); i != orig.end(); ++i) {
		point_t prev;

		brep_hit &out = *i;

		if (i != orig.begin()) {
		    bu_log("<%g>", DIST_PT_PT(out.point, prev));
		}
		bu_log("(");
		if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
		if (out.direction == brep_hit::ENTERING) bu_log("+");
		if (out.direction == brep_hit::LEAVING) bu_log("-");
		bu_log("<%d>", out.sbv->m_face->m_bRev);
		VMOVE(prev, out.point);
		bu_log(")");
	    }

	    bu_log("\n**********************\n");
	}
#endif

//#define KODDHIT
#ifdef KODDHIT //ugly debugging hack to raytrace single surface and not worry about odd hits
	static fastf_t diststep = 0.0;
	bool hit_it = hits.size() > 0;
#else
	bool hit_it = hits.size() % 2 == 0;
#endif
	if (hit_it) {
	    // take each pair as a segment
	    for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
		brep_hit& in = *i;
#ifndef KODDHIT  //ugly debugging hack to raytrace single surface and not worry about odd hits
		i++;
#endif
		brep_hit& out = *i;

		register struct seg* segp;
		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;

		VMOVE(segp->seg_in.hit_point, in.point);
		VMOVE(segp->seg_in.hit_normal, in.normal);
#ifdef KODDHIT //ugly debugging hack to raytrace single surface and not worry about odd hits
		segp->seg_in.hit_dist = diststep + 1.0;
#else
		segp->seg_in.hit_dist = DIST_PT_PT(rp->r_pt, in.point);
#endif
		segp->seg_in.hit_surfno = in.face.m_face_index;
		VSET(segp->seg_in.hit_vpriv, in.uv[0], in.uv[1], 0.0);

		VMOVE(segp->seg_out.hit_point, out.point);
		VMOVE(segp->seg_out.hit_normal, out.normal);
		segp->seg_out.hit_dist = DIST_PT_PT(rp->r_pt, out.point);
		segp->seg_out.hit_surfno = out.face.m_face_index;
		VSET(segp->seg_out.hit_vpriv, out.uv[0], out.uv[1], 0.0);

		BU_LIST_INSERT(&(seghead->l), &(segp->l));
	    }
	    hit = true;
	} else {
	    //TRACE2("screen xy: " << ap->a_x << ", " << ap->a_y);
	    bu_log("**** ERROR odd number of hits: %zu\n", hits.size());
	    bu_log("xyz %g %g %g \n", rp->r_pt[0], rp->r_pt[1], rp->r_pt[2]);
	    bu_log("dir %g %g %g \n", rp->r_dir[0], rp->r_dir[1], rp->r_dir[2]);
	    bu_log("**** Current Hits: %zu\n", hits.size());

	    for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
		point_t prev;

		brep_hit &out = *i;
		VMOVE(prev, out.point);

		if (i != hits.begin()) {
		    bu_log("<%g>", DIST_PT_PT(out.point, prev));
		}
		bu_log("(");
		if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
		if (out.direction == brep_hit::ENTERING) bu_log("+");
		if (out.direction == brep_hit::LEAVING) bu_log("-");

		bu_log(")");
	    }
	    bu_log("\n**** Orig Hits: %zu\n", orig.size());

	    for (HitList::iterator i = orig.begin(); i != orig.end(); ++i) {
		point_t prev;

		brep_hit &out = *i;
		VMOVE(prev, out.point);

		if (i != orig.begin()) {
		    bu_log("<%g>", DIST_PT_PT(out.point, prev));
		}
		bu_log("(");
		if (out.hit == brep_hit::CRACK_HIT) bu_log("_CRACK_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::CLEAN_HIT) bu_log("_CH_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::NEAR_HIT) bu_log("_NH_(%d)", out.face.m_face_index);
		if (out.hit == brep_hit::NEAR_MISS) bu_log("_NM_(%d)", out.face.m_face_index);
		if (out.direction == brep_hit::ENTERING) bu_log("+");
		if (out.direction == brep_hit::LEAVING) bu_log("-");
		bu_log("<%d>", out.sbv->m_face->m_bRev);

		bu_log(")");
	    }

	    bu_log("\n**********************\n");
	}
    }

    return (hit) ? (int)hits.size() : 0; // MISS
}


/**
 * R T _ B R E P _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_brep_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct brep_specific* bs;

    if (!hitp || !stp || !rp)
	return;
    RT_CK_SOLTAB(stp);
    bs = (struct brep_specific*)stp->st_specific;
    if (!bs)
	return;

    /* XXX todo */
}


/**
 * R T _ B R E P _ C U R V E
 *
 * Return the curvature of the nurb.
 */
void
rt_brep_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    struct brep_specific* bs;

    if (!cvp || !hitp || !stp)
	return;
    RT_CK_SOLTAB(stp);
    bs = (struct brep_specific*)stp->st_specific;
    if (!bs)
	return;

    /* XXX todo */
}


int
rt_brep_class(const struct soltab *stp, const fastf_t *min, const fastf_t *max, const struct bn_tol *tol)
{
    if (stp) RT_CK_SOLTAB(stp);
    if (tol) BN_CK_TOL(tol);
    if (!min) return 0;
    if (!max) return 0;

    return 0;
}


/**
 * R T _ B R E P _ U V
 *
 * For a hit on the surface of an nurb, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 * u = azimuth
 * v = elevation
 */
void
rt_brep_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
    struct brep_specific* bs;

    if (ap) RT_CK_APPLICATION(ap);
    if (!stp)
	return;
    RT_CK_SOLTAB(stp);
    bs = (struct brep_specific*)stp->st_specific;
    if (!bs)
	return;

    uvp->uv_u = hitp->hit_vpriv[0];
    uvp->uv_v = hitp->hit_vpriv[1];
}


/**
 * R T _ B R E P _ F R E E
 */
void
rt_brep_free(register struct soltab *stp)
{
    TRACE1("rt_brep_free");

    struct brep_specific* bs;

    if (!stp)
	return;
    RT_CK_SOLTAB(stp);
    bs = (struct brep_specific*)stp->st_specific;
    if (!bs)
	return;

    brep_specific_delete(bs);
}


/**
 * Given surface tree bounding box information, plot the bounding box
 * as a wireframe in mged.
 */
void
plot_bbnode(BBNode* node, struct bu_list* vhead, int depth, int start, int limit)
{
    BU_CK_LIST_HEAD(vhead);

    ON_3dPoint min = node->m_node.m_min;
    ON_3dPoint max = node->m_node.m_max;
    point_t verts[] = {{min[0], min[1], min[2]},
		       {min[0], max[1], min[2]},
		       {min[0], max[1], max[2]},
		       {min[0], min[1], max[2]},
		       {max[0], min[1], min[2]},
		       {max[0], max[1], min[2]},
		       {max[0], max[1], max[2]},
		       {max[0], min[1], max[2]}};

    if (depth >= start && depth<=limit) {
	for (int i = 0; i <= 4; i++) {
	    RT_ADD_VLIST(vhead, verts[i%4], (i == 0) ? BN_VLIST_LINE_MOVE : BN_VLIST_LINE_DRAW);
	}
	for (int i = 0; i <= 4; i++) {
	    RT_ADD_VLIST(vhead, verts[(i%4)+4], (i == 0) ? BN_VLIST_LINE_MOVE : BN_VLIST_LINE_DRAW);
	}
	for (int i = 0; i < 4; i++) {
	    RT_ADD_VLIST(vhead, verts[i], BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, verts[i+4], BN_VLIST_LINE_DRAW);
	}

    }

    for (size_t i = 0; i < node->m_children.size(); i++) {
	if (i < 1)
	    plot_bbnode(node->m_children[i], vhead, depth+1, start, limit);
    }
}


double
find_next_point(const ON_Curve* crv, double startdomval, double increment, double tolerance, int stepcount)
{
    double inc = increment;
    if (startdomval + increment > 1.0) inc = 1.0 - startdomval;
    ON_Interval dom = crv->Domain();
    ON_3dPoint prev_pt = crv->PointAt(dom.ParameterAt(startdomval));
    ON_3dPoint next_pt = crv->PointAt(dom.ParameterAt(startdomval+inc));
    if (prev_pt.DistanceTo(next_pt) > tolerance) {
	stepcount++;
	inc = inc / 2;
	return find_next_point(crv, startdomval, inc, tolerance, stepcount);
    } else {
	if (stepcount > 5) return 0.0;
	return startdomval + inc;
    }
}


double
find_next_trimming_point(const ON_Curve* crv, const ON_Surface* s, double startdomval, double increment, double tolerance, int stepcount)
{
    double inc = increment;
    if (startdomval + increment > 1.0) inc = 1.0 - startdomval;
    ON_Interval dom = crv->Domain();
    ON_3dPoint prev_pt = crv->PointAt(dom.ParameterAt(startdomval));
    ON_3dPoint next_pt = crv->PointAt(dom.ParameterAt(startdomval+inc));
    ON_3dPoint prev_3d_pt, next_3d_pt;
    s->EvPoint(prev_pt[0], prev_pt[1], prev_3d_pt, 0, 0);
    s->EvPoint(next_pt[0], next_pt[1], next_3d_pt, 0, 0);
    if (prev_3d_pt.DistanceTo(next_3d_pt) > tolerance) {
	stepcount++;
	inc = inc / 2;
	return find_next_trimming_point(crv, s, startdomval, inc, tolerance, stepcount);
    } else {
	if (stepcount > 5) return 0.0;
	return startdomval + inc;
    }
}


/* a binary predicate for std:list implemented as a function */
bool near_equal(double first, double second) {
    /* FIXME: arbitrary nearness tolerance */
      return NEAR_EQUAL(first, second, 1e-6);
}


void plot_sum_surface(struct bu_list *vhead, const ON_Surface *surf,
		int isocurveres, int gridres) {
	double pt1[3], pt2[3];
	ON_2dPoint from, to;

	ON_Interval udom = surf->Domain(0);
	ON_Interval vdom = surf->Domain(1);

	for (int u = 0; u <= gridres; u++) {
		for (int v = 1; v <= isocurveres; v++) {
			ON_3dPoint p = surf->PointAt(udom.ParameterAt((double) u
					/ (double) gridres), vdom.ParameterAt((double) (v - 1)
					/ (double) isocurveres));
			VMOVE(pt1, p);
			p = surf->PointAt(udom.ParameterAt((double) u / (double) gridres),
					vdom.ParameterAt((double) v / (double) isocurveres));
			VMOVE(pt2, p);
			RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
			RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	}

	for (int v = 0; v <= gridres; v++) {
		for (int u = 1; u <= isocurveres; u++) {
			ON_3dPoint p = surf->PointAt(udom.ParameterAt((double) (u - 1)
					/ (double) isocurveres), vdom.ParameterAt((double) v
					/ (double) gridres));
			VMOVE(pt1, p);
			p = surf->PointAt(udom.ParameterAt((double) u
					/ (double) isocurveres), vdom.ParameterAt((double) v
					/ (double) gridres));
			VMOVE(pt2, p);
			RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
			RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	}
	return;
}
void plotisoUCheckForTrim(struct bu_list *vhead, SurfaceTree* st, fastf_t from,
		fastf_t to, fastf_t v) {
	double pt1[3], pt2[3];
	std::list<BRNode*> m_trims_right;
	std::list<double> trim_hits;

	const ON_Surface *surf = st->getSurface();
	CurveTree *ctree = st->ctree;
	double umin, umax;
	surf->GetDomain(0, &umin, &umax);

	m_trims_right.clear();

	fastf_t tol = 0.001;
	ON_2dPoint pt;
	pt.x = umin;
	pt.y = v;

	if (ctree != NULL) {
		m_trims_right.clear();
		ctree->getLeavesRight(m_trims_right, pt, tol);
	}

	int cnt = 1;
	//bu_log("V - %f\n",pt.x);
	trim_hits.clear();
	for (std::list<BRNode*>::iterator i = m_trims_right.begin(); i
			!= m_trims_right.end(); i++, cnt++) {
		BRNode* br = dynamic_cast<BRNode*> (*i);

		point_t bmin, bmax;
		if (!br->m_Horizontal) {
			br->GetBBox(bmin, bmax);
			if (((bmin[Y] - tol) <= pt[Y]) && (pt[Y] <= (bmax[Y] + tol))) { //if check trim and in BBox
				fastf_t u = br->getCurveEstimateOfU(pt[Y], tol);
				trim_hits.push_back(u);
				//bu_log("%d U %d - %f pt %f,%f bmin %f,%f  bmax %f,%f\n",br->m_face->m_face_index,cnt,u,pt.x,pt.y,bmin[X],bmin[Y],bmax[X],bmax[Y]);
			}
		}
	}
	trim_hits.sort();
	trim_hits.unique(near_equal);

	int hit_cnt = trim_hits.size();
	cnt = 1;
	//bu_log("\tplotisoUCheckForTrim: hit_cnt %d from center  %f %f 0.0 to center %f %f 0.0\n",hit_cnt,from,v ,to,v);

	if ((hit_cnt > 0) && ((hit_cnt % 2) == 0)) {
		while (!trim_hits.empty()) {
			double start = trim_hits.front();
			if (start < from) {
				start = from;
			}
			trim_hits.pop_front();
			double end = trim_hits.front();
			if (end > to) {
				end = to;
			}
			trim_hits.pop_front();
			//bu_log("\tfrom - %f, to - %f\n",from,to);
			fastf_t deltax = (end - start) / 50.0;
			if (deltax > 0.001) {
				for (fastf_t x = start; x < end; x = x + deltax) {
					ON_3dPoint p = surf->PointAt(x, pt.y);
					VMOVE(pt1, p);
					if (x + deltax > end) {
						p = surf->PointAt(end, pt.y);
					} else {
						p = surf->PointAt(x + deltax, pt.y);
					}
					VMOVE(pt2, p);

					//				bu_log(
					//						"\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
					//						cnt++, x, v, x + deltax, v);

					RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
					RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
				}
			}
		}
	}

	return;
}

void plotisoVCheckForTrim(struct bu_list *vhead, SurfaceTree* st, fastf_t from,
		fastf_t to, fastf_t u) {
	double pt1[3], pt2[3];
	std::list<BRNode*> m_trims_above;
	std::list<double> trim_hits;

	const ON_Surface *surf = st->getSurface();
	CurveTree *ctree = st->ctree;
	double vmin, vmax;
	surf->GetDomain(1, &vmin, &vmax);

	m_trims_above.clear();

	fastf_t tol = 0.001;
	ON_2dPoint pt;
	pt.x = u;
	pt.y = vmin;

	if (ctree != NULL) {
		m_trims_above.clear();
		ctree->getLeavesAbove(m_trims_above, pt, tol);
	}

	int cnt = 1;
	trim_hits.clear();
	for (std::list<BRNode*>::iterator i = m_trims_above.begin(); i
			!= m_trims_above.end(); i++, cnt++) {
		BRNode* br = dynamic_cast<BRNode*> (*i);

		point_t bmin, bmax;
		if (!br->m_Vertical) {
			br->GetBBox(bmin, bmax);

			if (((bmin[X] - tol) <= pt[X]) && (pt[X] <= (bmax[X] + tol))) { //if check trim and in BBox
				fastf_t v = br->getCurveEstimateOfV(pt[X], tol);
				trim_hits.push_back(v);
				//bu_log("%d V %d - %f pt %f,%f bmin %f,%f  bmax %f,%f\n",br->m_face->m_face_index,cnt,v,pt.x,pt.y,bmin[X],bmin[Y],bmax[X],bmax[Y]);
			}
		}
	}
	trim_hits.sort();
	trim_hits.unique(near_equal);

	size_t hit_cnt = trim_hits.size();
	cnt = 1;

	//bu_log("\tplotisoVCheckForTrim: hit_cnt %d from center  %f %f 0.0 to center %f %f 0.0\n",hit_cnt,u,from,u,to);

	if ((hit_cnt > 0) && ((hit_cnt % 2) == 0)) {
		while (!trim_hits.empty()) {
			double start = trim_hits.front();
			trim_hits.pop_front();
			if (start < from) {
				start = from;
			}
			double end = trim_hits.front();
			trim_hits.pop_front();
			if (end > to) {
				end = to;
			}
			//bu_log("\tfrom - %f, to - %f\n",from,to);
			fastf_t deltay = (end - start) / 50.0;
			if (deltay > 0.001) {
				for (fastf_t y = start; y < end; y = y + deltay) {
					ON_3dPoint p = surf->PointAt(pt.x, y);
					VMOVE(pt1, p);
					if (y + deltay > end) {
						p = surf->PointAt(pt.x, end);
					} else {
						p = surf->PointAt(pt.x, y + deltay);
					}
					VMOVE(pt2, p);

					//bu_log("\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
					//		cnt++, u, y, u, y + deltay);

					RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
					RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
				}
			}
		}
	}
	return;
}

void plotisoU(struct bu_list *vhead, SurfaceTree* st, fastf_t from, fastf_t to,
		fastf_t v, int curveres) {
	double pt1[3], pt2[3];
	fastf_t deltau = (to - from) / curveres;
	const ON_Surface *surf = st->getSurface();

	for (fastf_t u = from; u < to; u = u + deltau) {
		ON_3dPoint p = surf->PointAt(u, v);
		//bu_log("p1 2d - %f,%f 3d - %f,%f,%f\n",pt.x, y,p.x,p.y,p.z);
		VMOVE(pt1, p);
		if (u + deltau > to) {
			p = surf->PointAt(to, v);
		} else {
			p = surf->PointAt(u + deltau, v);
		}
		//bu_log("p1 2d - %f,%f 3d - %f,%f,%f\n",pt.x,y+deltay,p.x,p.y,p.z);
		VMOVE(pt2, p);
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}
}

void plotisoV(struct bu_list *vhead, SurfaceTree* st, fastf_t from, fastf_t to,
		fastf_t u, int curveres) {
	double pt1[3], pt2[3];
	fastf_t deltav = (to - from) / curveres;
	const ON_Surface *surf = st->getSurface();

	for (fastf_t v = from; v < to; v = v + deltav) {
		ON_3dPoint p = surf->PointAt(u, v);
		//bu_log("p1 2d - %f,%f 3d - %f,%f,%f\n",pt.x, y,p.x,p.y,p.z);
		VMOVE(pt1, p);
		if (v + deltav > to) {
			p = surf->PointAt(u, to);
		} else {
			p = surf->PointAt(u, v + deltav);
		}
		//bu_log("p1 2d - %f,%f 3d - %f,%f,%f\n",pt.x,y+deltay,p.x,p.y,p.z);
		VMOVE(pt2, p);
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}
}

void plot_BBNode(struct bu_list *vhead, SurfaceTree* st, BBNode * node, int isocurveres, int gridres) {
	if (node->isLeaf()) {
		//draw leaf
		if (node->m_trimmed) {
			return; // nothing to do node is trimmed
		} else if (node->m_checkTrim) { // node may contain trim check all corners
			fastf_t u = node->m_u[0];
			fastf_t v = node->m_v[0];
			fastf_t from = u;
			fastf_t to = node->m_u[1];
			//bu_log("drawBBNode: node %x uvmin center %f %f 0.0, uvmax center %f %f 0.0\n",node,node->m_u[0],node->m_v[0],node->m_u[1],node->m_v[1]);

			plotisoUCheckForTrim(vhead, st, from, to, v); //bottom
			v = node->m_v[1];
			plotisoUCheckForTrim(vhead, st, from, to, v); //top
			from = node->m_v[0];
			to = node->m_v[1];
			plotisoVCheckForTrim(vhead, st, from, to, u); //left
			u = node->m_u[1];
			plotisoVCheckForTrim(vhead, st, from, to, u); //right
			return;
		} else { // fully untrimmed just draw bottom and right edges
			fastf_t u = node->m_u[0];
			fastf_t v = node->m_v[0];
			fastf_t from = u;
			fastf_t to = node->m_u[1];
			plotisoU(vhead, st, from, to, v, isocurveres); //bottom

			from = v;
			to = node->m_v[1];
			plotisoV(vhead, st, from, to, u, isocurveres); //right
			return;
		}
	} else {
		if (node->m_children.size() > 0) {
			for (std::vector<BBNode*>::iterator childnode =
					node->m_children.begin(); childnode
					!= node->m_children.end(); childnode++) {
				plot_BBNode(vhead, st, *childnode,isocurveres,gridres);
			}
		}
	}
}

void plot_face_from_surface_tree(struct bu_list *vhead, SurfaceTree* st,
		int isocurveres, int gridres) {
	BBNode *root = st->getRootNode();
	plot_BBNode(vhead, st, root, isocurveres, gridres);
}
///////////////////////////
void plot_face_trim(struct bu_list *vhead, ON_BrepFace &face, int plotres,
		bool dim3d) {
	const ON_Surface* surf = face.SurfaceOf();
	double umin, umax;
	double pt1[3], pt2[3];
	ON_2dPoint from, to;

	ON_TextLog tl(stderr);

	surf->GetDomain(0, &umin, &umax);
	for (int i = 0; i < face.LoopCount(); i++) {
		ON_BrepLoop* loop = face.Loop(i);
		// for each trim
		for (int j = 0; j < loop->m_ti.Count(); j++) {
			ON_BrepTrim& trim = face.Brep()->m_T[loop->m_ti[j]];
			const ON_Curve* trimCurve = trim.TrimCurveOf();
			//trimCurve->Dump(tl);

			ON_Interval dom = trimCurve->Domain();
			// XXX todo: dynamically sample the curve
			for (int k = 1; k <= plotres; k++) {
				ON_3dPoint p = trimCurve->PointAt(dom.ParameterAt((double) (k
						- 1) / (double) plotres));
				if (dim3d)
					p = surf->PointAt(p.x, p.y);
				VMOVE(pt1, p);
				p = trimCurve->PointAt(dom.ParameterAt((double) k
						/ (double) plotres));
				if (dim3d)
					p = surf->PointAt(p.x, p.y);
				VMOVE(pt2, p);
				RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
				RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
			}
		}
	}

	return;
}

/**
 * R T _ B R E P _ P L O T
 *
 * There are several ways to visualize NURBS surfaces, depending on
 * the purpose.  For "normal" wireframe viewing, the ideal approach is
 * to do a tesselation of the NURBS surface and show that wireframe.
 * The quicker and simpler approach is to visualize the edges,
 * although that can sometimes generate less than ideal/useful results
 * (for example, a revolved edge that forms a sphere will have a
 * wireframe consisting of a 2D arc in MGED when only edges are used.)
 * A third approach is to walk the uv space for each surface, find
 * 3space points at uv intervals, and draw lines between the results -
 * this is slightly more comprehensive when it comes to showing where
 * surfaces are in 3space but looks blocky and crude.  For now,
 * edge-only wireframes are the default.
 *
 */
int
rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *tol)
{
    TRACE1("rt_brep_plot");
    struct rt_brep_internal* bi;
    int i;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    ON_Brep* brep = bi->brep;
	int gridres = 10;
	int isocurveres = 100;

	for (int index = 0; index < brep->m_F.Count(); index++) {
		ON_BrepFace& face = brep->m_F[index];
		const ON_Surface *surf = face.SurfaceOf();

		ON_RevSurface *revsurf;
		ON_SumSurface *sumsurf;
		if ((surf->IsClosed(0) || surf->IsClosed(1)) && (sumsurf = const_cast<ON_SumSurface *> (ON_SumSurface::Cast(surf)))) {
			SurfaceTree* st = new SurfaceTree(&face, true, 2);

			plot_face_from_surface_tree(vhead, st, isocurveres, gridres);

			delete st;
		} else if (surf->IsClosed(0) || surf->IsClosed(1) || (revsurf
				= const_cast<ON_RevSurface *> (ON_RevSurface::Cast(surf)))) {

			SurfaceTree* st = new SurfaceTree(&face, true, 0);

			plot_face_from_surface_tree(vhead, st, isocurveres, gridres);

			delete st;
		}
	}

	{

		point_t pt1, pt2;

		for (i = 0; i < bi->brep->m_E.Count(); i++) {
			ON_BrepEdge& e = brep->m_E[i];
			const ON_Curve* crv = e.EdgeCurveOf();

			if (crv->IsLinear()) {
				ON_BrepVertex& v1 = brep->m_V[e.m_vi[0]];
				ON_BrepVertex& v2 = brep->m_V[e.m_vi[1]];
				VMOVE(pt1, v1.Point());
				VMOVE(pt2, v2.Point());
				RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
				RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
			} else {
				ON_Interval dom = crv->Domain();

				double domainval = 0.0;
				double olddomainval = 1.0;
				int crudestep = 0;
				// Insert first point.
				ON_3dPoint p = crv->PointAt(dom.ParameterAt(domainval));
				VMOVE(pt1, p);
				RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);

				/* Dynamic sampling approach - start with an initial guess
				 * for the next point of one tenth of the domain length
				 * further down the domain from the previous value.  Set a
				 * maximum physical distance between points of 100 times
				 * the model tolerance.  Reduce the increment until the
				 * tolerance is satisfied, then add the point and use it
				 * as the starting point for the next calculation until
				 * the whole domain is finished.  Perhaps it would be more
				 * ideal to base the tolerance on some fraction of the
				 * curve bounding box dimensions?
				 */

				while (domainval < 1.0 && crudestep <= 100) {
					olddomainval = domainval;
					if (crudestep == 0)
						domainval = find_next_point(crv, domainval, 0.1,
								tol->dist * 100, 0);
					if (crudestep >= 1 || ZERO(domainval)) {
						crudestep++;
						domainval = olddomainval + (1.0 - olddomainval)
								/ 100 * crudestep;
					}
					p = crv->PointAt(dom.ParameterAt(domainval));
					VMOVE(pt1, p);
					RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_DRAW);
				}
			}
		}
	}

    return 0;
}


/**
 * R T _ B R E P _ T E S S
 */
int
rt_brep_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    struct rt_brep_internal *bi;

    if (!r || !m || !ip || !ttol || !tol)
	return -1;

    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal *)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    /* XXX - implement me */
    return -1;
}


/**
 * XXX In order to facilitate exporting the ON_Brep object without a
 * whole lot of effort, we're going to (for now) extend the
 * ON_BinaryArchive to support an "in-memory" representation of a
 * binary archive. Currently, the openNURBS library only supports
 * file-based archiving operations. This implies the
 */
class RT_MemoryArchive : public ON_BinaryArchive
{
public:
    RT_MemoryArchive();
    RT_MemoryArchive(genptr_t memory, size_t len);
    virtual ~RT_MemoryArchive();

    // ON_BinaryArchive overrides
    size_t CurrentPosition() const;
    bool SeekFromCurrentPosition(int);
    bool SeekFromStart(size_t);
    bool AtEnd() const;

    size_t Size() const;
    /**
     * Generate a byte-array copy of this memory archive.  Allocates
     * memory using bu_malloc, so must be freed with bu_free
     */
    uint8_t* CreateCopy() const;

protected:
    size_t Read(size_t, void*);
    size_t Write(size_t, const void*);
    bool Flush();

private:
    size_t pos;
    std::vector<char> m_buffer;
};


RT_MemoryArchive::RT_MemoryArchive()
    : ON_BinaryArchive(ON::write3dm), pos(0)
{
}


RT_MemoryArchive::RT_MemoryArchive(genptr_t memory, size_t len)
    : ON_BinaryArchive(ON::read3dm), pos(0)
{
    m_buffer.reserve(len);
    for (size_t i = 0; i < len; i++) {
	m_buffer.push_back(((char*)memory)[i]);
    }
}


RT_MemoryArchive::~RT_MemoryArchive()
{
}


size_t
RT_MemoryArchive::CurrentPosition() const
{
    return pos;
}


bool
RT_MemoryArchive::SeekFromCurrentPosition(int seek_to)
{
    if (pos + seek_to > m_buffer.size()) return false;
    pos += seek_to;
    return true;
}


bool
RT_MemoryArchive::SeekFromStart(size_t seek_to)
{
    if (seek_to > m_buffer.size()) return false;
    pos = seek_to;
    return true;
}


bool
RT_MemoryArchive::AtEnd() const
{
    return pos >= m_buffer.size();
}


size_t
RT_MemoryArchive::Size() const
{
    return m_buffer.size();
}


uint8_t*
RT_MemoryArchive::CreateCopy() const
{
    uint8_t *memory = (uint8_t *)bu_malloc(m_buffer.size()*sizeof(uint8_t), "rt_memoryarchive createcopy");
    const size_t size = m_buffer.size();
    for (size_t i = 0; i < size; i++) {
	memory[i] = m_buffer[i];
    }
    return memory;
}


size_t
RT_MemoryArchive::Read(size_t amount, void* buf)
{
    const size_t read_amount = (pos + amount > m_buffer.size()) ? m_buffer.size()-pos : amount;
    const size_t start = pos;
    for (; pos < (start+read_amount); pos++) {
	((char*)buf)[pos-start] = m_buffer[pos];
    }
    return read_amount;
}


size_t
RT_MemoryArchive::Write(const size_t amount, const void* buf)
{
    // the write can come in at any position!
    const size_t start = pos;
    // resize if needed to support new data
    if (m_buffer.size() < (start+amount)) {
	m_buffer.resize(start+amount);
    }
    for (; pos < (start+amount); pos++) {
	m_buffer[pos] = ((char*)buf)[pos-start];
    }
    return amount;
}


bool
RT_MemoryArchive::Flush()
{
    return true;
}


/**
 * R T _ B R E P _ E X P O R T 5
 */
int
rt_brep_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    TRACE1("rt_brep_export5");
    struct rt_brep_internal* bi;

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BREP) return -1;
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    BU_EXTERNAL_INIT(ep);

    RT_MemoryArchive archive;
    /* XXX what to do about the version */
    ONX_Model model;

    {
	ON_Layer default_layer;
	default_layer.SetLayerIndex(0);
	default_layer.SetLayerName("Default");
	model.m_layer_table.Reserve(1);
	model.m_layer_table.Append(default_layer);
    }

    ONX_Model_Object& mo = model.m_object_table.AppendNew();
    mo.m_object = bi->brep;
    mo.m_attributes.m_layer_index = 0;
    mo.m_attributes.m_name = "brep";
    mo.m_attributes.m_uuid = ON_opennurbs4_id;

    model.m_properties.m_RevisionHistory.NewRevision();
    model.m_properties.m_Application.m_application_name = "BRL-CAD B-Rep primitive";

    model.Polish();
    ON_TextLog err(stderr);
    bool ok = model.Write(archive, 4, "export5", &err);
    if (ok) {
	ep->ext_nbytes = (long)archive.Size();
	ep->ext_buf = archive.CreateCopy();
	return 0;
    } else {
	return -1;
    }
}


/**
 * R T _ B R E P _ I M P O R T 5
 */
int
rt_brep_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    ON::Begin();
    TRACE1("rt_brep_import5");

    struct rt_brep_internal* bi;
    if (dbip) RT_CK_DBI(dbip);
    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BREP;
    ip->idb_meth = &rt_functab[ID_BREP];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_brep_internal), "rt_brep_internal");

    bi = (struct rt_brep_internal*)ip->idb_ptr;
    bi->magic = RT_BREP_INTERNAL_MAGIC;

    RT_MemoryArchive archive(ep->ext_buf, ep->ext_nbytes);
    ONX_Model model;
    ON_TextLog dump(stderr);
    //archive.Dump3dmChunk(dump);
    model.Read(archive, &dump);

    if (model.IsValid(&dump)) {
	ONX_Model_Object mo = model.m_object_table[0];
	// XXX does openNURBS force us to copy? it seems the answer is
	// YES due to the const-ness
	bi->brep = ON_Brep::New(*ON_Brep::Cast(mo.m_object));
	if (mat) {
	    ON_Xform xform(mat);

	    if (!xform.IsIdentity()) {
		bu_log("Applying transformation matrix....\n");
		for (int row=0;row<4;row++) {
		    bu_log("%d - ", row);
		    for (int col=0;col<4;col++) {
			bu_log(" %5f", xform.m_xform[row][col]);
		    }
		    bu_log("\n");
		}
		bi->brep->Transform(xform);
	    }
	}
	return 0;
    } else {
	return -1;
    }
}


/**
 * R T _ B R E P _ I F R E E
 */
void
rt_brep_ifree(struct rt_db_internal *ip)
{
    struct rt_brep_internal* bi;
    RT_CK_DB_INTERNAL(ip);

    TRACE1("rt_brep_ifree");

    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    if (bi->brep != NULL)
	delete bi->brep;
    bu_free(bi, "rt_brep_internal free");
    ip->idb_ptr = GENPTR_NULL;
}


/**
 * R T _ B R E P _ D E S C R I B E
 */
int
rt_brep_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double UNUSED(mm2local))
{
    BU_CK_VLS(str);
    RT_CK_DB_INTERNAL(ip);

    ON_wString wonstr;
    ON_TextLog log(wonstr);

    struct rt_brep_internal* bi;
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    if (bi->brep != NULL)
	bi->brep->Dump(log);

    ON_String onstr = ON_String(wonstr);
    bu_vls_strcat(str, "Boundary Representation (BREP) object\n");

    if (!verbose)
	return 0;

    const char *description = onstr.Array();
    // skip the first "ON_Brep:" line
    while (description && description[0] && description[0] != '\n') {
	description++;
    }
    if (description && description[0] && description[0] == '\n') {
	description++;
    }
    bu_vls_strcat(str, description);

    return 0;
}


/**
 * R T _ B R E P _ T C L G E T
 */
int
rt_brep_tclget(Tcl_Interp *, const struct rt_db_internal *, const char *)
{
    return 0;
}


/**
 * R T _ B R E P _ T C L A D J U S T
 */
int
rt_brep_tcladjust(Tcl_Interp *, struct rt_db_internal *, int, const char **)
{
    return 0;
}


/**
 * R T _ B R E P _ P A R A M S
 */
int
rt_brep_params(struct pc_pc_set *, const struct rt_db_internal *)
{
    return 0;
}


/** @} */

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
