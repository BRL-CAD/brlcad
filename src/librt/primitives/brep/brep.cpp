/*                     B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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
#include <map>
#include <stack>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

#include "poly2tri/poly2tri.h"

#include "assert.h"

#include "vmath.h"

#include "brep.h"
#include "dvec.h"

#include "raytrace.h"
#include "rtgeom.h"

#include "./brep_local.h"
#include "./brep_debug.h"

/* undefine "min" and "max" macros, if they exist, to prevent
 * name conflicts with functions "std::min" and "std::max".
 */
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#define BN_VMATH_PREFIX_INDICES 1
#define ROOT_TOL 1.E-7

/* uncomment to enable debug plotting */
/* #define PLOTTING 1 */

#ifdef __cplusplus
extern "C" {
#endif
    int rt_brep_bbox(struct rt_db_internal* ip, point_t *min, point_t *max);
    int rt_brep_prep(struct soltab *stp, struct rt_db_internal* ip, struct rt_i* rtip);
    void rt_brep_print(register const struct soltab *stp);
    int rt_brep_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead);
    void rt_brep_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp);
    void rt_brep_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp);
    int rt_brep_class(const struct soltab *stp, const fastf_t *min, const fastf_t *max, const struct bn_tol *tol);
    void rt_brep_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp);
    void rt_brep_free(register struct soltab *stp);
    int rt_brep_adaptive_plot(struct rt_db_internal *ip, const struct rt_view_info *info);
    int rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol, const struct rt_view_info *UNUSED(info));
    int rt_brep_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);
    int rt_brep_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip);
    int rt_brep_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip);
    void rt_brep_ifree(struct rt_db_internal *ip);
    int rt_brep_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local);
    int rt_brep_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr);
    int rt_brep_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, const char **argv);
    int rt_brep_params(struct pc_pc_set *, const struct rt_db_internal *ip);
    RT_EXPORT extern int rt_brep_boolean(struct rt_db_internal *out, const struct rt_db_internal *ip1, const struct rt_db_internal *ip2, const char* operation);
    struct rt_selection_set *rt_brep_find_selections(const struct rt_db_internal *ip, const struct rt_selection_query *query);
    int rt_brep_process_selection(struct rt_db_internal *ip, const struct rt_selection *selection, const struct rt_selection_operation *op);
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
    ON_2dPoint from(0.0, 0.0);
    ON_2dPoint to(0.0, 0.0);

    from.x = pt[0];
    from.y = to.y = pt[1];
    surf->GetDomain(0, &umin, &umax);
    to.x = umax + 1;
    ON_Line ray(from, to);

    // int intersections = 0;
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
    // retVal= (intersections > 0 && (intersections % 2) == 0);

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
    ON_3dVector tangent1(0.0, 0.0, 0.0);
    ON_3dVector tangent2(0.0, 0.0, 0.0);
    ON_3dPoint p1(0.0, 0.0, 0.0);
    ON_3dPoint p2(0.0, 0.0, 0.0);

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
 * Calculate a bounding RPP around a BREP.  Unlike the prep
 * routine, which makes use of the full bounding volume hierarchy,
 * this routine just calls the openNURBS function.
 */
int
rt_brep_bbox(struct rt_db_internal *ip, point_t *min, point_t *max) {
    struct rt_brep_internal* bi;
    ON_3dPoint dmin(0.0, 0.0, 0.0);
    ON_3dPoint dmax(0.0, 0.0, 0.0);

    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    bi->brep->GetBBox(dmin, dmax);
    VMOVE(*min, dmin);
    VMOVE(*max, dmax);

    return 0;
}


/**
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
	BU_ALLOC(bs, struct brep_specific);
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
    fastf_t dist;
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

    brep_hit(const ON_BrepFace& f, const ON_Ray& ray, const point_t p, const vect_t n, const pt2d_t _uv)
	: face(f), trimmed(false), closeToEdge(false), oob(false), hit(CLEAN_HIT), direction(ENTERING), m_adj_face_index(0), sbv(NULL)
    {
	vect_t dir;
	VMOVE(origin, ray.m_origin);
	VMOVE(point, p);
	VMOVE(normal, n);
	VSUB2(dir, point, origin);
	dist = VDOT(ray.m_dir, dir);
	move(uv, _uv);
    }

    brep_hit(const ON_BrepFace& f, fastf_t d, const ON_Ray& ray, const point_t p, const vect_t n, const pt2d_t _uv)
	: face(f), dist(d), trimmed(false), closeToEdge(false), oob(false), hit(CLEAN_HIT), direction(ENTERING), m_adj_face_index(0), sbv(NULL)
    {
	VMOVE(origin, ray.m_origin);
	VMOVE(point, p);
	VMOVE(normal, n);
	move(uv, _uv);
    }

    brep_hit(const brep_hit& h)
	: face(h.face), dist(h.dist), trimmed(h.trimmed), closeToEdge(h.closeToEdge), oob(h.oob), hit(h.hit), direction(h.direction), m_adj_face_index(h.m_adj_face_index), sbv(h.sbv)
    {
	VMOVE(origin, h.origin);
	VMOVE(point, h.point);
	VMOVE(normal, h.normal);
	move(uv, h.uv);

    }

    brep_hit& operator=(const brep_hit& h)
    {
	const_cast<ON_BrepFace&>(face) = h.face;
	dist = h.dist;
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
	m_adj_face_index = h.m_adj_face_index;

	return *this;
    }

    bool operator==(const brep_hit& h) const
    {
	return NEAR_ZERO(dist - h.dist, BREP_SAME_POINT_TOLERANCE);
    }

    bool operator<(const brep_hit& h) const
    {
	return dist < h.dist;
    }
};


typedef std::list<brep_hit> HitList;


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
    if (t1 < t0) {
	double tmp = t0;
	t0 = t1;
	t1 = tmp;
    }

    if (uv.x < t0) {
	uv.x = t0;
    } else if (uv.x > t1) {
	uv.x = t1;
    }

    surf->GetDomain(1, &t0, &t1);
    if (t1 < t0) {
	double tmp = t0;
	t0 = t1;
	t1 = tmp;
    }
    if (uv.y < t0) {
	uv.y = t0;
    } else if (uv.y > t1) {
	uv.y = t1;
    }
}


void
utah_pushBack(const BBNode* sbv, ON_2dPoint &uv)
{
    double t0, t1;
    int i = sbv->m_u.m_t[0] < sbv->m_u.m_t[1] ? 0 : 1;

    t0 = sbv->m_u.m_t[i];
    t1 = sbv->m_u.m_t[1-i];
    if (uv.x < t0) {
	uv.x = t0;
    } else if (uv.x > t1) {
	uv.x = t1;
    }
    i = sbv->m_v.m_t[0] < sbv->m_v.m_t[1] ? 0 : 1;
    t0 = sbv->m_v.m_t[i];
    t1 = sbv->m_v.m_t[1-i];
    if (uv.y < t0 ) {
	uv.y = t0;
    } else if (uv.y > t1) {
	uv.y = t1;
    }
}


int
utah_newton_solver(const BBNode* sbv, const ON_Surface* surf, const ON_Ray& r, ON_2dPoint* ouv, double* t, ON_3dVector* N, bool& converged, ON_2dPoint* suv, const int count, const int iu, const int iv)
{
    int i = 0;
    int intersects = 0;
    double j11 = 0.0;
    double j12 = 0.0;
    double j21 = 0.0;
    double j22 = 0.0;
    double f = 0.0;
    double g = 0.0;
    double rootdist = 0.0;
    double oldrootdist = 0.0;
    double J = 0.0;
    double invdetJ = 0.0;
    double du = 0.0;
    double dv = 0.0;
    double cdu = 0.0;
    double cdv = 0.0;

    ON_3dVector p1, p2;
    double p1d = 0.0, p2d = 0.0;
    int errantcount = 0;
    utah_ray_planes(r, p1, p1d, p2, p2d);

    ON_3dPoint S(0.0, 0.0, 0.0);
    ON_3dVector Su(0.0, 0.0, 0.0);
    ON_3dVector Sv(0.0, 0.0, 0.0);
    //ON_3dVector Suu, Suv, Svv;

    ON_2dPoint uv(0.0, 0.0);
    ON_2dPoint puv(0.0, 0.0);

    uv.x = suv->x;
    uv.y = suv->y;

    ON_2dPoint uv0(uv);
    surf->Ev1Der(uv.x, uv.y, S, Su, Sv);
    //surf->Ev2Der( uv.x, uv.y, S, Su, Sv, Suu, Suv, Svv);

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

	invdetJ = 1.0 / J;

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


	if (i==0) {
	    cdu = du;
	    cdv = dv;
	} else {
	    int sgnd = (du > 0) - (du < 0);
	    int sgncd = (cdu > 0) - (cdu < 0);
	    if ((sgnd != sgncd) && (fabs(du) > fabs(cdu))) {
		du = sgnd * 0.75 * fabs(cdu);
	    }
	    sgnd = (dv > 0) - (dv < 0);
	    sgncd = (cdv > 0) - (cdv < 0);
	    if ((sgnd != sgncd) && (fabs(dv) > fabs(cdv))) {
		dv = sgnd * 0.75 * fabs(cdv);
	    }
	    cdu = du;
	    cdv = dv;
	}
	puv.x = uv.x;
	puv.y = uv.y;

	uv.x -= du;
	uv.y -= dv;

	utah_pushBack(sbv, uv);

	surf->Ev1Der(uv.x, uv.y, S, Su, Sv);
	utah_F(S, p1, p1d, p2, p2d, f, g);
	oldrootdist = rootdist;
	rootdist = fabs(f) + fabs(g);
	int halve_count = 0;

	/* FIXME: all constants should be documented, why this
	 * value?  what's the sensitivity/impact?
	 */
	while ((halve_count++ < 3) && (oldrootdist < rootdist)) {
	    // divide current UV step
	    uv.x = (puv.x + uv.x)/2.0;
	    uv.y = (puv.y + uv.y)/2.0;

	    utah_pushBack(sbv, uv);

	    surf->Ev1Der(uv.x, uv.y, S, Su, Sv);
	    utah_F(S, p1, p1d, p2, p2d, f, g);
	    rootdist = fabs(f) + fabs(g);
	}

	if (oldrootdist <= rootdist) {

	    /* FIXME: all constants should be documented. why this
	     * value? must it coincide with the constant in the
	     * preceding loop?
	     */
	    if (errantcount > 3) {
		return intersects;
	    } else {
		errantcount++;
	    }
	}

	if (rootdist < ROOT_TOL) {
	    int ulow = (sbv->m_u.m_t[0] <= sbv->m_u.m_t[1]) ? 0 : 1;
	    int vlow = (sbv->m_v.m_t[0] <= sbv->m_v.m_t[1]) ? 0 : 1;
	    if ((sbv->m_u.m_t[ulow]-VUNITIZE_TOL < uv.x && uv.x < sbv->m_u.m_t[1-ulow]+VUNITIZE_TOL) &&
		    (sbv->m_v.m_t[vlow]-VUNITIZE_TOL < uv.y && uv.y < sbv->m_v.m_t[1-vlow]+VUNITIZE_TOL)) {
		bool new_point = true;
		for (int j=0;j<count;j++) {
		    if (NEAR_EQUAL(uv.x, ouv[j].x, VUNITIZE_TOL) && NEAR_EQUAL(uv.y, ouv[j].y, VUNITIZE_TOL)) {
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
		intersects += utah_newton_solver(sbv, surf, r, ouv, t, N, converged, &uv, intersects, iu, iv);
	    }
	}
    }

    ON_2dPoint uv;
    uv.x = sbv->m_u.Mid();
    uv.y = sbv->m_v.Mid();
    intersects += utah_newton_solver(sbv, surf, r, ouv, t, N, converged, &uv, intersects, -1, -1);
    return intersects;
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
	ON_3dPoint closestPoint(0.0, 0.0, 0.0);
	ON_3dVector tangent(0.0, 0.0, 0.0);
	ON_3dVector kappa(0.0, 0.0, 0.0);
	double currentDistance = -10000.0;
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
utah_brep_intersect(const BBNode* sbv, const ON_BrepFace* face, const ON_Surface* surf, pt2d_t& uv, ON_Ray& ray, HitList& hits)
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

    if (converged) {
	for (int i = 0; i < numhits; i++) {
	    double closesttrim;
	    BRNode* trimBR = NULL;
	    int trim_status = sbv->isTrimmed(ouv[i], &trimBR, closesttrim);
	    if (trim_status != 1) {
		ON_3dPoint _pt;
		ON_3dVector _norm(N[i]);
		vect_t vpt;
		vect_t vnorm;
		_pt = ray.m_origin + (ray.m_dir * t[i]);
		VMOVE(vpt, _pt);
		if (face->m_bRev) {
		    //bu_log("Reversing normal for Face:%d\n", face->m_face_index);
		    _norm.Reverse();
		}
		VMOVE(vnorm, _norm);
		hit_count += 1;
		uv[0] = ouv[i].x;
		uv[1] = ouv[i].y;
		brep_hit bh(*face, t[i], ray, vpt, vnorm, uv);
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
		if (VDOT(ray.m_dir, vnorm) < 0.0)
		    bh.direction = brep_hit::ENTERING;
		else
		    bh.direction = brep_hit::LEAVING;
		bh.sbv = sbv;
		hits.push_back(bh);
		found = BREP_INTERSECT_FOUND;
	    } else if (fabs(closesttrim) < BREP_EDGE_MISS_TOLERANCE) {
		ON_3dPoint _pt;
		ON_3dVector _norm(N[i]);
		vect_t vpt;
		vect_t vnorm;
		_pt = ray.m_origin + (ray.m_dir * t[i]);
		VMOVE(vpt, _pt);
		if (face->m_bRev) {
		    //bu_log("Reversing normal for Face:%d\n", face->m_face_index);
		    _norm.Reverse();
		}
		VMOVE(vnorm, _norm);
		hit_count += 1;
		uv[0] = ouv[i].x;
		uv[1] = ouv[i].y;
		brep_hit bh(*face, t[i], ray, vpt, vnorm, uv);
		bh.trimmed = true;
		bh.closeToEdge = true;
		if (trimBR != NULL) {
		    bh.m_adj_face_index = trimBR->m_adj_face_index;
		} else {
		    bh.m_adj_face_index = -99;
		}
		bh.hit = brep_hit::NEAR_MISS;
		if (VDOT(ray.m_dir, vnorm) < 0.0)
		    bh.direction = brep_hit::ENTERING;
		else
		    bh.direction = brep_hit::LEAVING;
		bh.sbv = sbv;
		hits.push_back(bh);
		found = BREP_INTERSECT_FOUND;
	    }
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
	utah_brep_intersect(sbv, f, surf, uv, r, all_hits);
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
    }

    all_hits.clear();
    all_hits = hits;


    if (hits.size() > 0) {
	// remove grazing hits with with normal to ray dot less than BREP_GRAZING_DOT_TOL (>= 89.999 degrees obliq)
	TRACE("-- Remove grazing hits --");
	int num = 0;
	for (HitList::iterator i = hits.begin(); i != hits.end(); ++i) {
	    brep_hit &curr_hit = *i;
	    if ((curr_hit.trimmed && !curr_hit.closeToEdge) || curr_hit.oob || NEAR_ZERO(VDOT(curr_hit.normal, rp->r_dir), BREP_GRAZING_DOT_TOL)) {
		// remove what we were removing earlier
		if (curr_hit.oob) {
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
    }


    if (hits.size() > 0) {
	// we should have "valid" points now, remove duplicates or grazes(same point with in/out sign change)
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

    bool hit = false;
    if (hits.size() > 1) {

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
		segp->seg_in.hit_dist = in.dist;
#endif
		segp->seg_in.hit_surfno = in.face.m_face_index;
		VSET(segp->seg_in.hit_vpriv, in.uv[0], in.uv[1], 0.0);

		VMOVE(segp->seg_out.hit_point, out.point);
		VMOVE(segp->seg_out.hit_normal, out.normal);
		segp->seg_out.hit_dist = out.dist;
		segp->seg_out.hit_surfno = out.face.m_face_index;
		VSET(segp->seg_out.hit_vpriv, out.uv[0], out.uv[1], 0.0);

		BU_LIST_INSERT(&(seghead->l), &(segp->l));
	    }
	    hit = true;
	} else {
	    //TRACE2("screen xy: " << ap->a_x << ", " << ap->a_y);
	    bu_log("**** ERROR odd number of hits: %lu\n", static_cast<unsigned long>(hits.size()));
	    bu_log("xyz %g %g %g \n", rp->r_pt[0], rp->r_pt[1], rp->r_pt[2]);
	    bu_log("dir %g %g %g \n", rp->r_dir[0], rp->r_dir[1], rp->r_dir[2]);
	    bu_log("**** Current Hits: %lu\n", static_cast<unsigned long>(hits.size()));

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
	    bu_log("\n**** Orig Hits: %lu\n", static_cast<unsigned long>(orig.size()));

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
 * For a hit on the surface of an nurb, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1
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
bool
near_equal(double first, double second)
{
    /* FIXME: arbitrary nearness tolerance */
    return NEAR_EQUAL(first, second, 1e-6);
}


void
plot_sum_surface(struct bu_list *vhead, const ON_Surface *surf, int isocurveres, int gridres)
{
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;

    ON_2dPoint from(0.0, 0.0);
    ON_2dPoint to(0.0, 0.0);

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


void
plotisoUCheckForTrim(struct bu_list *vhead, SurfaceTree* st, fastf_t from, fastf_t to, fastf_t v)
{
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
    std::list<BRNode*> m_trims_right;
    std::list<double> trim_hits;

    const ON_Surface *surf = st->getSurface();
    CurveTree *ctree = st->ctree;
    double umin, umax;

    surf->GetDomain(0, &umin, &umax);
    m_trims_right.clear();

    fastf_t tol = 0.001;
    ON_2dPoint pt(0.0, 0.0);

    pt.x = umin;
    pt.y = v;

    if (ctree != NULL) {
	m_trims_right.clear();
	ctree->getLeavesRight(m_trims_right, pt, tol);
    }

    int cnt = 1;

    //bu_log("V - %f\n", pt.x);
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
		//bu_log("%d U %d - %f pt %f, %f bmin %f, %f bmax %f, %f\n", br->m_face->m_face_index, cnt, u, pt.x, pt.y, bmin[X], bmin[Y], bmax[X], bmax[Y]);
	    }
	}
    }

    trim_hits.sort();
    trim_hits.unique(near_equal);

    int hit_cnt = trim_hits.size();

    cnt = 1;
    //bu_log("\tplotisoUCheckForTrim: hit_cnt %d from center  %f %f 0.0 to center %f %f 0.0\n", hit_cnt, from, v , to, v);

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

	    //bu_log("\tfrom - %f, to - %f\n", from, to);
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


void
plotisoVCheckForTrim(struct bu_list *vhead, SurfaceTree* st, fastf_t from, fastf_t to, fastf_t u)
{
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
    std::list<BRNode*> m_trims_above;
    std::list<double> trim_hits;

    const ON_Surface *surf = st->getSurface();
    CurveTree *ctree = st->ctree;
    double vmin, vmax;
    surf->GetDomain(1, &vmin, &vmax);

    m_trims_above.clear();

    fastf_t tol = 0.001;
    ON_2dPoint pt(0.0, 0.0);

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
		//bu_log("%d V %d - %f pt %f, %f bmin %f, %f bmax %f, %f\n", br->m_face->m_face_index, cnt, v, pt.x, pt.y, bmin[X], bmin[Y], bmax[X], bmax[Y]);
	    }
	}
    }
    trim_hits.sort();
    trim_hits.unique(near_equal);

    size_t hit_cnt = trim_hits.size();
    cnt = 1;

    //bu_log("\tplotisoVCheckForTrim: hit_cnt %d from center  %f %f 0.0 to center %f %f 0.0\n", hit_cnt, u, from, u, to);

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
	    //bu_log("\tfrom - %f, to - %f\n", from, to);
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


void
plotisoU(struct bu_list *vhead, SurfaceTree* st, fastf_t from, fastf_t to, fastf_t v, int curveres)
{
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
    fastf_t deltau = (to - from) / curveres;
    const ON_Surface *surf = st->getSurface();

    for (fastf_t u = from; u < to; u = u + deltau) {
	ON_3dPoint p = surf->PointAt(u, v);
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
	VMOVE(pt1, p);
	if (u + deltau > to) {
	    p = surf->PointAt(to, v);
	} else {
	    p = surf->PointAt(u + deltau, v);
	}
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
	VMOVE(pt2, p);
	RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
    }
}


void
plotisoV(struct bu_list *vhead, SurfaceTree* st, fastf_t from, fastf_t to, fastf_t u, int curveres)
{
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
    fastf_t deltav = (to - from) / curveres;
    const ON_Surface *surf = st->getSurface();

    for (fastf_t v = from; v < to; v = v + deltav) {
	ON_3dPoint p = surf->PointAt(u, v);
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
	VMOVE(pt1, p);
	if (v + deltav > to) {
	    p = surf->PointAt(u, to);
	} else {
	    p = surf->PointAt(u, v + deltav);
	}
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
	VMOVE(pt2, p);
	RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
    }
}


void
plot_BBNode(struct bu_list *vhead, SurfaceTree* st, BBNode * node, int isocurveres, int gridres)
{
    if (node->isLeaf()) {
	//draw leaf
	if (node->m_trimmed) {
	    return; // nothing to do node is trimmed
	} else if (node->m_checkTrim) { // node may contain trim check all corners
	    fastf_t u = node->m_u[0];
	    fastf_t v = node->m_v[0];
	    fastf_t from = u;
	    fastf_t to = node->m_u[1];
	    //bu_log("drawBBNode: node %x uvmin center %f %f 0.0, uvmax center %f %f 0.0\n", node, node->m_u[0], node->m_v[0], node->m_u[1], node->m_v[1]);

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
		plot_BBNode(vhead, st, *childnode, isocurveres, gridres);
	    }
	}
    }
}


void
plot_face_from_surface_tree(struct bu_list *vhead, SurfaceTree* st, int isocurveres, int gridres)
{
    BBNode *root = st->getRootNode();
    plot_BBNode(vhead, st, root, isocurveres, gridres);
}


void getEdgePoints(const ON_BrepEdge &edge, fastf_t t1, ON_3dPoint &start_3d, ON_3dVector &start_tang, fastf_t t2, ON_3dPoint &end_3d, ON_3dVector &end_tang, fastf_t min_dist, fastf_t max_dist, fastf_t within_dist, fastf_t cos_within_ang, std::map<double,ON_3dPoint *> &param_points)
{
    ON_Interval range = edge.Domain();
    ON_3dPoint mid_3d;
    ON_3dVector mid_tang;
    fastf_t t = (t1 + t2) / 2.0;

    if ( edge.EvTangent(t,mid_3d,mid_tang) ) {
	ON_Line line3d(start_3d, end_3d);
	double dist3d;
	if ((line3d.Length() > max_dist)
		|| ((dist3d=mid_3d.DistanceTo(line3d.ClosestPointTo(mid_3d))) > within_dist + ON_ZERO_TOLERANCE)
		|| (((start_tang * end_tang) < cos_within_ang - ON_ZERO_TOLERANCE) && (dist3d > min_dist + ON_ZERO_TOLERANCE))) {
	    getEdgePoints(edge, t1, start_3d, start_tang, t, mid_3d, mid_tang, min_dist, max_dist, within_dist, cos_within_ang, param_points);
	    param_points[(t - range.m_t[0])/(range.m_t[1] - range.m_t[0])] = new ON_3dPoint(mid_3d);
	    getEdgePoints(edge, t, mid_3d, mid_tang, t2, end_3d, end_tang, min_dist, max_dist, within_dist, cos_within_ang, param_points);
	}
    }

}

std::map<double, ON_3dPoint *> *getEdgePoints(ON_BrepEdge &edge,
        fastf_t max_dist, const struct rt_tess_tol *ttol,
        const struct bn_tol *tol, const struct rt_view_info *UNUSED(info))
{
    std::map<double,ON_3dPoint *> *param_points = NULL;
    fastf_t min_dist, within_dist, cos_within_ang;

    double dist = 1000.0;
#if 1
    for(int eti = 0; eti < edge.TrimCount(); eti++) {
	ON_BrepTrim *trim = edge.Trim(eti);

	    ON_3dPoint min,max;

	    if (eti == 0) {
#ifdef BY_ADJACENT_SURFACES
		const ON_Surface *s = trim->SurfaceOf();
		ON_BrepFace *face = trim->Face();
		bool bGrowBox = false;
		for (int ti = 0; ti < face->Loop(0)->TrimCount(); ti++) {
		    ON_BrepTrim *trim = face->Loop(0)->Trim(ti);
		    trim->GetBoundingBox(min, max, bGrowBox);
		    bGrowBox = true;
		}
		ON_Interval x_interval(min.x,max.x);
		ON_Interval y_interval(min.y,max.y);
		ON_Surface *s2 = s->Duplicate();
		if ((s2->Trim(0,x_interval)) && (s2->Trim(1,y_interval))) {
		    if (s2->GetBoundingBox(min,max,false)) {
			dist = DIST_PT_PT(min,max);
		    }
		}
#elif defined BY_FACE_SIZE
		ON_BrepFace *face = trim->Face();
		if (face->GetBoundingBox(min,max,false)) {
		    dist = DIST_PT_PT(min,max);
		}
#elif defined BY_BREP_SIZE
		ON_Brep *brep = trim->Brep();
		ON_BoundingBox tight_bbox;
		if (brep->GetTightBoundingBox(tight_bbox)) {
		    dist = DIST_PT_PT(tight_bbox.m_min,tight_bbox.m_max);
		}
#elif defined BY_LOOP_SIZE
		bool bGrowBox = false;
		ON_BrepFace *face = trim->Face();
		for (int ti = 0; ti < face->Loop(0)->TrimCount(); ti++) {
		    ON_BrepTrim *trim = face->Loop(0)->Trim(ti);
		    trim->GetBoundingBox(min, max, bGrowBox);
		    bGrowBox = true;
		}
		dist = DIST_PT_PT(min,max);
#elif !defined BY_TRIM_SIZE
		if (trim->GetBoundingBox(min,max,false)) {
		    dist = DIST_PT_PT(min,max);
		}
#endif
	    } else {
#ifdef BY_ADJACENT_SURFACES
		bool bGrowBox = false;
		const ON_Surface *s = trim->SurfaceOf();
		ON_BrepFace *face = trim->Face();
		for (int ti = 0; ti < face->Loop(0)->TrimCount(); ti++) {
		    ON_BrepTrim *trim = face->Loop(0)->Trim(ti);
		    trim->GetBoundingBox(min, max, bGrowBox);
		    bGrowBox = true;
		}
		ON_Interval x_interval(min.x,max.x);
		ON_Interval y_interval(min.y,max.y);
		ON_Surface *s2 = s->Duplicate();
		if ((s2->Trim(0,x_interval)) && (s2->Trim(1,y_interval))) {
		    if (s2->GetBoundingBox(min,max,false)) {
			double surfacesize = DIST_PT_PT(min,max);
			V_MIN(dist,surfacesize);
		    }
		}
#elif defined BY_FACE_SIZE
		ON_BrepFace *face = trim->Face();
		if (face->GetBoundingBox(min,max,false)) {
		    double facesize = DIST_PT_PT(min,max);
		    V_MIN(dist,facesize);
		}
#elif defined BY_BREP_SIZE
		ON_Brep *brep = trim->Brep();
		if (brep->GetTightBoundingBox(tight_bbox)) {
		    double brepsize = DIST_PT_PT(min,max);
		    V_MIN(dist,brepsize);
		}
#elif defined BY_LOOP_SIZE
		bool bGrowBox = false;
		ON_BrepFace *face = trim->Face();
		for (int ti = 0; ti < face->Loop(0)->TrimCount(); ti++) {
		    ON_BrepTrim *trim = face->Loop(0)->Trim(ti);
		    trim->GetBoundingBox(min, max, bGrowBox);
		    bGrowBox = true;
		}
		V_MIN(dist,DIST_PT_PT(min,max));
#elif !defined BY_TRIM_SIZE
		if (trim->GetBoundingBox(min,max,false)) {
		    double trimsize = DIST_PT_PT(min,max);
		    V_MIN(dist,trimsize);
		}
#endif
	    }
    }
#else
    for(int ti = 0; ti < edge.TrimCount(); ti++) {
	ON_BrepTrim *trim = edge.Trim(ti);
	const ON_Surface *s = trim->SurfaceOf();
	double width,height,diag;
	if (s->GetSurfaceSize(&width, &height)) {
	    ON_BrepFace *face = trim->Face();
	    face->SetDomain(0, 0.0, width);
	    face->SetDomain(1, 0.0, height);
	    ON_Interval range;
	    double trim_width, trim_height;
	    bool bGrowBox = false;
	    trim->GetBBox(&trim_width, &trim_height, bGrowBox);
	    diag = sqrt(trim_width*trim_width + trim_height*trim_height);
	    V_MIN(dist, diag);
	}
    }
#endif

    if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	min_dist = tol->dist;
    } else {
	min_dist = ttol->abs;
    }

    double rel = 0.0;
    if (ttol->rel > 0.0 + ON_ZERO_TOLERANCE) {
	rel = ttol->rel*dist;
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
	//if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	//    min_dist = within_dist;
	//}
    } else if (ttol->abs > 0.0 + ON_ZERO_TOLERANCE)  {
	within_dist = min_dist;
    } else {
	within_dist = 0.01 * dist; // default to 1% minimum surface distance
    }

    if (ttol->norm > 0.0 + ON_ZERO_TOLERANCE) {
	cos_within_ang = cos(ttol->norm);
    } else {
	cos_within_ang = cos(ON_PI/2.0);
    }

    if (edge.m_edge_user.p == NULL) {
	param_points = new std::map<double,ON_3dPoint *>();
	edge.m_edge_user.p = (void *)param_points;
	ON_Interval range = edge.Domain();

#ifdef BYKNOTS
	int knotcnt = edge.SpanCount();
	double *knots = new double[knotcnt + 1];
	edge.GetSpanVector(knots);

	//(*param_points)[0.0] = new ON_3dPoint(edge.PointAt(knots[0]));
	for(int ki = 1; ki < knotcnt+1; ki++) {
	    (*param_points)[(knots[ki-1]-range.m_t[0])/(range.m_t[1] - range.m_t[0])] = new ON_3dPoint(edge.PointAt(knots[ki-1]));
	    getEdgePoints(edge, knots[ki-1], knots[ki], min_dist, within_dist, cos_within_ang, *param_points);
	}
	(*param_points)[1.0] = new ON_3dPoint(edge.PointAt(knots[knotcnt]));
	delete [] knots;
#else
	if (edge.IsClosed()) {
	    ON_3dPoint start_3d, mid_3d, end_3d;
	    ON_3dVector start_tang, mid_tang, end_tang;
	    double mid_range = (range.m_t[0] + range.m_t[1])/2.0;

	    if ( edge.EvTangent(range.m_t[0],start_3d,start_tang)
		    && edge.EvTangent(mid_range,mid_3d,mid_tang)
		    && edge.EvTangent(range.m_t[1],end_3d,end_tang) ) {
		(*param_points)[0.0] = new ON_3dPoint(edge.PointAt(range.m_t[0]));
		getEdgePoints(edge, range.m_t[0], start_3d, start_tang, mid_range, mid_3d, mid_tang, min_dist, max_dist, within_dist, cos_within_ang, *param_points);
		(*param_points)[0.5] = new ON_3dPoint(edge.PointAt(mid_range));
		getEdgePoints(edge, mid_range, mid_3d, mid_tang, range.m_t[1], end_3d, end_tang, min_dist, max_dist, within_dist, cos_within_ang, *param_points);
		(*param_points)[1.0] = new ON_3dPoint(edge.PointAt(range.m_t[1]));
	    }
	} else {
	    ON_3dPoint start_3d,end_3d;
	    ON_3dVector start_tang,end_tang;
	    if ( edge.EvTangent(range.m_t[0],start_3d,start_tang)
	    	    && edge.EvTangent(range.m_t[1],end_3d,end_tang)) {
		(*param_points)[0.0] = new ON_3dPoint(start_3d);
		getEdgePoints(edge, range.m_t[0], start_3d, start_tang, range.m_t[1], end_3d, end_tang, min_dist, max_dist, within_dist, cos_within_ang, *param_points);
		(*param_points)[1.0] = new ON_3dPoint(end_3d);
	    }
	}
#endif
    } else {
	param_points = (std::map<double,ON_3dPoint *> *)edge.m_edge_user.p;
    }

    return param_points;
}


void
getEdgePoints(const ON_BrepTrim &trim,
	      fastf_t t1,
	      ON_3dPoint &start_2d,
	      ON_3dVector &start_tang,
	      ON_3dPoint &start_3d,
	      ON_3dVector &start_norm,
	      fastf_t t2,
	      ON_3dPoint &end_2d,
	      ON_3dVector &end_tang,
	      ON_3dPoint &end_3d,
	      ON_3dVector &end_norm,
	      fastf_t min_dist,
	      fastf_t max_dist,
	      fastf_t within_dist,
	      fastf_t cos_within_ang,
	      std::map<double, ON_3dPoint *> &param_points)
{
    const ON_Surface *s = trim.SurfaceOf();
    ON_Interval range = trim.Domain();
    ON_3dPoint mid_2d(0.0, 0.0, 0.0);
    ON_3dPoint mid_3d(0.0, 0.0, 0.0);
    ON_3dVector mid_norm(0.0, 0.0, 0.0);
    ON_3dVector mid_tang(0.0, 0.0, 0.0);
    fastf_t t = (t1 + t2) / 2.0;

    if (trim.EvTangent(t, mid_2d, mid_tang)
	    && s->EvNormal(mid_2d.x, mid_2d.y, mid_3d, mid_norm)) {
	ON_Line line3d(start_3d, end_3d);
	double dist3d;

	if ((line3d.Length() > max_dist)
		|| ((dist3d = mid_3d.DistanceTo(line3d.ClosestPointTo(mid_3d)))
			> within_dist + ON_ZERO_TOLERANCE)
		|| ((((start_tang * end_tang)
			< cos_within_ang - ON_ZERO_TOLERANCE)
			|| ((start_norm * end_norm)
				< cos_within_ang - ON_ZERO_TOLERANCE))
			&& (dist3d > min_dist + ON_ZERO_TOLERANCE))) {
	    getEdgePoints(trim, t1, start_2d, start_tang, start_3d, start_norm,
		    t, mid_2d, mid_tang, mid_3d, mid_norm, min_dist, max_dist,
		    within_dist, cos_within_ang, param_points);
	    param_points[(t - range.m_t[0]) / (range.m_t[1] - range.m_t[0])] =
		    new ON_3dPoint(mid_3d);
	    getEdgePoints(trim, t, mid_2d, mid_tang, mid_3d, mid_norm, t2,
		    end_2d, end_tang, end_3d, end_norm, min_dist, max_dist,
		    within_dist, cos_within_ang, param_points);
	}
    }
}

std::map<double, ON_3dPoint *> *
getEdgePoints(ON_BrepTrim &trim,
	      fastf_t max_dist,
	      const struct rt_tess_tol *ttol,
	      const struct bn_tol *tol,
	      const struct rt_view_info *UNUSED(info))
{
    std::map<double, ON_3dPoint *> *param_points = NULL;
    fastf_t min_dist, within_dist, cos_within_ang;

    double dist = 1000.0;

    const ON_Surface *s = trim.SurfaceOf();

    bool bGrowBox = false;
    ON_3dPoint min, max;
    if (trim.GetBoundingBox(min, max, bGrowBox)) {
	dist = DIST_PT_PT(min,max);
    }

    if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	min_dist = tol->dist;
    } else {
	min_dist = ttol->abs;
    }

    double rel = 0.0;
    if (ttol->rel > 0.0 + ON_ZERO_TOLERANCE) {
	rel = ttol->rel * dist;
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else if (ttol->abs > 0.0 + ON_ZERO_TOLERANCE) {
	within_dist = min_dist;
    } else {
	within_dist = 0.01 * dist; // default to 1% minimum surface distance
    }

    if (ttol->norm > 0.0 + ON_ZERO_TOLERANCE) {
	cos_within_ang = cos(ttol->norm);
    } else {
	cos_within_ang = cos(ON_PI / 2.0);
    }

    if (trim.m_trim_user.p == NULL) {
	param_points = new std::map<double, ON_3dPoint *>();
	trim.m_trim_user.p = (void *) param_points;
	ON_Interval range = trim.Domain();

	if (trim.IsClosed()) {
	    double mid_range = (range.m_t[0] + range.m_t[1]) / 2.0;
	    ON_3dPoint start_2d(0.0, 0.0, 0.0);
	    ON_3dPoint start_3d(0.0, 0.0, 0.0);
	    ON_3dVector start_tang(0.0, 0.0, 0.0);
	    ON_3dVector start_norm(0.0, 0.0, 0.0);
	    ON_3dPoint mid_2d(0.0, 0.0, 0.0);
	    ON_3dPoint mid_3d(0.0, 0.0, 0.0);
	    ON_3dVector mid_tang(0.0, 0.0, 0.0);
	    ON_3dVector mid_norm(0.0, 0.0, 0.0);
	    ON_3dPoint end_2d(0.0, 0.0, 0.0);
	    ON_3dPoint end_3d(0.0, 0.0, 0.0);
	    ON_3dVector end_tang(0.0, 0.0, 0.0);
	    ON_3dVector end_norm(0.0, 0.0, 0.0);

	    if (trim.EvTangent(range.m_t[0], start_2d, start_tang)
		    && trim.EvTangent(mid_range, mid_2d, mid_tang)
		    && trim.EvTangent(range.m_t[1], end_2d, end_tang)
		    && s->EvNormal(mid_2d.x, mid_2d.y, mid_3d, mid_norm)
		    && s->EvNormal(start_2d.x, start_2d.y, start_3d, start_norm)
		    && s->EvNormal(end_2d.x, end_2d.y, end_3d, end_norm)) {
		(*param_points)[0.0] = new ON_3dPoint(
			s->PointAt(trim.PointAt(range.m_t[0]).x,
				trim.PointAt(range.m_t[0]).y));
		getEdgePoints(trim, range.m_t[0], start_2d, start_tang,
			start_3d, start_norm, mid_range, mid_2d, mid_tang,
			mid_3d, mid_norm, min_dist, max_dist, within_dist,
			cos_within_ang, *param_points);
		(*param_points)[0.5] = new ON_3dPoint(
			s->PointAt(trim.PointAt(mid_range).x,
				trim.PointAt(mid_range).y));
		getEdgePoints(trim, mid_range, mid_2d, mid_tang, mid_3d,
			mid_norm, range.m_t[1], end_2d, end_tang, end_3d,
			end_norm, min_dist, max_dist, within_dist,
			cos_within_ang, *param_points);
		(*param_points)[1.0] = new ON_3dPoint(
			s->PointAt(trim.PointAt(range.m_t[1]).x,
				trim.PointAt(range.m_t[1]).y));
	    }
	} else {
	    ON_3dPoint start_2d(0.0, 0.0, 0.0);
	    ON_3dPoint start_3d(0.0, 0.0, 0.0);
	    ON_3dVector start_tang(0.0, 0.0, 0.0);
	    ON_3dVector start_norm(0.0, 0.0, 0.0);
	    ON_3dPoint end_2d(0.0, 0.0, 0.0);
	    ON_3dPoint end_3d(0.0, 0.0, 0.0);
	    ON_3dVector end_tang(0.0, 0.0, 0.0);
	    ON_3dVector end_norm(0.0, 0.0, 0.0);

	    if (trim.EvTangent(range.m_t[0], start_2d, start_tang)
		    && trim.EvTangent(range.m_t[1], end_2d, end_tang)
		    && s->EvNormal(start_2d.x, start_2d.y, start_3d, start_norm)
		    && s->EvNormal(end_2d.x, end_2d.y, end_3d, end_norm)) {
		(*param_points)[0.0] = new ON_3dPoint(start_3d);
		getEdgePoints(trim, range.m_t[0], start_2d, start_tang,
			start_3d, start_norm, range.m_t[1], end_2d, end_tang,
			end_3d, end_norm, min_dist, max_dist, within_dist,
			cos_within_ang, *param_points);
		(*param_points)[1.0] = new ON_3dPoint(end_3d);
	    }
	}
    } else {
	param_points = (std::map<double, ON_3dPoint *> *) trim.m_trim_user.p;
    }

    return param_points;
}

void
getSurfacePoints(const ON_Surface *s,
		 fastf_t u1,
		 fastf_t u2,
		 fastf_t v1,
		 fastf_t v2,
		 fastf_t min_dist,
		 fastf_t within_dist,
		 fastf_t cos_within_ang,
		 ON_2dPointArray &on_surf_points,
		 bool left,
		 bool below)
{
    double ldfactor = 2.0;
    ON_2dPoint p2d(0.0, 0.0);
    ON_3dPoint p[4] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
    ON_3dVector norm[4] = {ON_3dVector(), ON_3dVector(), ON_3dVector(), ON_3dVector()};
    ON_3dPoint mid(0.0, 0.0, 0.0);
    ON_3dVector norm_mid(0.0, 0.0, 0.0);
    fastf_t u = (u1 + u2) / 2.0;
    fastf_t v = (v1 + v2) / 2.0;
    fastf_t udist = u2 - u1;
    fastf_t vdist = v2 - v1;

    if ((udist < min_dist + ON_ZERO_TOLERANCE)
	    || (vdist < min_dist + ON_ZERO_TOLERANCE)) {
	return;
    }

    if (udist > ldfactor * vdist) {
	int isteps = (int) (udist / vdist);
	isteps = (int) (udist / vdist / ldfactor * 2.0);
	fastf_t step = udist / (fastf_t) isteps;

	fastf_t step_u;
	for (int i = 1; i <= isteps; i++) {
	    step_u = u1 + i * step;
	    if ((below) && (i < isteps)) {
		p2d.Set(step_u, v1);
		on_surf_points.Append(p2d);
	    }
	    if (i == 1) {
		getSurfacePoints(s, u1, u1 + step, v1, v2, min_dist,
			within_dist, cos_within_ang, on_surf_points, left,
			below);
	    } else if (i == isteps) {
		getSurfacePoints(s, u2 - step, u2, v1, v2, min_dist,
			within_dist, cos_within_ang, on_surf_points, left,
			below);
	    } else {
		getSurfacePoints(s, step_u - step, step_u, v1, v2, min_dist, within_dist,
			cos_within_ang, on_surf_points, left, below);
	    }
	    left = false;

	    if (i < isteps) {
		//top
		p2d.Set(step_u, v2);
		on_surf_points.Append(p2d);
	    }
	}
    } else if (vdist > ldfactor * udist) {
	int isteps = (int) (vdist / udist);
	isteps = (int) (vdist / udist / ldfactor * 2.0);
	fastf_t step = vdist / (fastf_t) isteps;
	fastf_t step_v;
	for (int i = 1; i <= isteps; i++) {
	    step_v = v1 + i * step;
	    if ((left) && (i < isteps)) {
		p2d.Set(u1, step_v);
		on_surf_points.Append(p2d);
	    }

	    if (i == 1) {
		getSurfacePoints(s, u1, u2, v1, v1 + step, min_dist,
			within_dist, cos_within_ang, on_surf_points, left,
			below);
	    } else if (i == isteps) {
		getSurfacePoints(s, u1, u2, v2 - step, v2, min_dist,
			within_dist, cos_within_ang, on_surf_points, left,
			below);
	    } else {
		getSurfacePoints(s, u1, u2, step_v - step, step_v, min_dist, within_dist,
			cos_within_ang, on_surf_points, left, below);
	    }

	    below = false;

	    if (i < isteps) {
		//right
		p2d.Set(u2, step_v);
		on_surf_points.Append(p2d);
	    }
	}
    } else if ((s->EvNormal(u1, v1, p[0], norm[0]))
	    && (s->EvNormal(u2, v1, p[1], norm[1])) // for u
	    && (s->EvNormal(u2, v2, p[2], norm[2]))
	    && (s->EvNormal(u1, v2, p[3], norm[3]))
	    && (s->EvNormal(u, v, mid, norm_mid))) {
	double udot;
	double vdot;
	ON_Line line1(p[0], p[2]);
	ON_Line line2(p[1], p[3]);
	double dist = mid.DistanceTo(line1.ClosestPointTo(mid));
	V_MAX(dist, mid.DistanceTo(line2.ClosestPointTo(mid)));

	if (dist < min_dist + ON_ZERO_TOLERANCE) {
	    return;
	}

	if (VNEAR_EQUAL(norm[0],norm[1],ON_ZERO_TOLERANCE)) {
	    udot = 1.0;
	} else {
	    udot = norm[0] * norm[1];
	}
	if (VNEAR_EQUAL(norm[0],norm[3],ON_ZERO_TOLERANCE)) {
	    vdot = 1.0;
	} else {
	    vdot = norm[0] * norm[3];
	}
	if ((udot < cos_within_ang - ON_ZERO_TOLERANCE)
		&& (vdot < cos_within_ang - ON_ZERO_TOLERANCE)) {
	    if (left) {
		p2d.Set(u1, v);
		on_surf_points.Append(p2d);
	    }
	    if (below) {
		p2d.Set(u, v1);
		on_surf_points.Append(p2d);
	    }
	    //center
	    p2d.Set(u, v);
	    on_surf_points.Append(p2d);
	    //right
	    p2d.Set(u2, v);
	    on_surf_points.Append(p2d);
	    //top
	    p2d.Set(u, v2);
	    on_surf_points.Append(p2d);

	    getSurfacePoints(s, u1, u, v1, v, min_dist, within_dist,
		    cos_within_ang, on_surf_points, left, below);
	    getSurfacePoints(s, u1, u, v, v2, min_dist, within_dist,
		    cos_within_ang, on_surf_points, left, false);
	    getSurfacePoints(s, u, u2, v1, v, min_dist, within_dist,
		    cos_within_ang, on_surf_points, false, below);
	    getSurfacePoints(s, u, u2, v, v2, min_dist, within_dist,
		    cos_within_ang, on_surf_points, false, false);
	} else if (udot < cos_within_ang - ON_ZERO_TOLERANCE) {
	    if (below) {
		p2d.Set(u, v1);
		on_surf_points.Append(p2d);
	    }
	    //top
	    p2d.Set(u, v2);
	    on_surf_points.Append(p2d);
	    getSurfacePoints(s, u1, u, v1, v2, min_dist, within_dist,
		    cos_within_ang, on_surf_points, left, below);
	    getSurfacePoints(s, u, u2, v1, v2, min_dist, within_dist,
		    cos_within_ang, on_surf_points, false, below);
	} else if (vdot < cos_within_ang - ON_ZERO_TOLERANCE) {
	    if (left) {
		p2d.Set(u1, v);
		on_surf_points.Append(p2d);
	    }
	    //right
	    p2d.Set(u2, v);
	    on_surf_points.Append(p2d);

	    getSurfacePoints(s, u1, u2, v1, v, min_dist, within_dist,
		    cos_within_ang, on_surf_points, left, below);
	    getSurfacePoints(s, u1, u2, v, v2, min_dist, within_dist,
		    cos_within_ang, on_surf_points, left, false);
	} else {
	    if (dist > within_dist + ON_ZERO_TOLERANCE) {
		if (left) {
		    p2d.Set(u1, v);
		    on_surf_points.Append(p2d);
		}
		if (below) {
		    p2d.Set(u, v1);
		    on_surf_points.Append(p2d);
		}
		//center
		p2d.Set(u, v);
		on_surf_points.Append(p2d);
		//right
		p2d.Set(u2, v);
		on_surf_points.Append(p2d);
		//top
		p2d.Set(u, v2);
		on_surf_points.Append(p2d);

		getSurfacePoints(s, u1, u, v1, v, min_dist, within_dist,
			cos_within_ang, on_surf_points, left, below);
		getSurfacePoints(s, u1, u, v, v2, min_dist, within_dist,
			cos_within_ang, on_surf_points, left, false);
		getSurfacePoints(s, u, u2, v1, v, min_dist, within_dist,
			cos_within_ang, on_surf_points, false, below);
		getSurfacePoints(s, u, u2, v, v2, min_dist, within_dist,
			cos_within_ang, on_surf_points, false, false);
	    }
	}
    }
}


void
getSurfacePoints(ON_BrepFace &face,
		 const struct rt_tess_tol *ttol,
		 const struct bn_tol *tol,
		 const struct rt_view_info *UNUSED(info),
		 ON_2dPointArray &on_surf_points)
{
    double surface_width, surface_height;
    const ON_Surface *s = face.SurfaceOf();
    ON_Brep *brep = face.Brep();

    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	double dist = 0.0;
	double min_dist = 0.0;
	double within_dist = 0.0;
	double  cos_within_ang = 0.0;

	if ((surface_width < tol->dist) || (surface_height < tol->dist)) {
	    return;
	}

	// may be a smaller trimmed subset of surface so worth getting
	// face boundary
	bool bGrowBox = false;
	ON_3dPoint min, max;
	for (int ti = 0; ti < face.Loop(0)->TrimCount(); ti++) {
	    ON_BrepTrim *trim = face.Loop(0)->Trim(ti);
	    trim->GetBoundingBox(min, max, bGrowBox);
	    bGrowBox = true;
	}

	ON_BoundingBox tight_bbox;
	if (brep->GetTightBoundingBox(tight_bbox)) {
	    dist = DIST_PT_PT(tight_bbox.m_min,tight_bbox.m_max);
	}

	if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	    min_dist = tol->dist;
	} else {
	    min_dist = ttol->abs;
	}

	double rel = 0.0;
	if (ttol->rel > 0.0 + ON_ZERO_TOLERANCE) {
	    rel = ttol->rel * dist;
	    within_dist = rel < min_dist ? min_dist : rel;
	    //if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	    //    min_dist = within_dist;
	    //}
	} else if ((ttol->abs > 0.0 + ON_ZERO_TOLERANCE)
		&& (ttol->norm < 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = min_dist;
	} else if ((ttol->abs > 0.0 + ON_ZERO_TOLERANCE)
		|| (ttol->norm > 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = dist;
	} else {
	    within_dist = 0.01 * dist; // default to 1% minimum surface distance
	}

	if (ttol->norm > 0.0 + ON_ZERO_TOLERANCE) {
	    cos_within_ang = cos(ttol->norm);
	} else {
	    cos_within_ang = cos(ON_PI / 2.0);
	}
	ON_BOOL32 uclosed = s->IsClosed(0);
	ON_BOOL32 vclosed = s->IsClosed(1);
	if (uclosed && vclosed) {
	    ON_2dPoint p(0.0, 0.0);
	    double midx = (min.x + max.x) / 2.0;
	    double midy = (min.y + max.y) / 2.0;

	    //bottom left
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //midy left
	    p.Set(min.x, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, midx, min.y, midy, min_dist, within_dist,
		    cos_within_ang, on_surf_points, true, true);

	    //bottom midx
	    p.Set(midx, min.y);
	    on_surf_points.Append(p);

	    //midx midy
	    p.Set(midx, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, midx, max.x, min.y, midy, min_dist, within_dist,
		    cos_within_ang, on_surf_points, false, true);

	    //bottom right
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //right  midy
	    p.Set(max.x, midy);
	    on_surf_points.Append(p);

	    //top left
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, midx, midy, max.y, min_dist, within_dist,
		    cos_within_ang, on_surf_points, true, false);

	    //top midx
	    p.Set(midx, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, midx, max.x, midy, max.y, min_dist, within_dist,
		    cos_within_ang, on_surf_points, false, false);

	    //top left
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	} else if (uclosed) {
	    ON_2dPoint p(0.0, 0.0);
	    double midx = (min.x + max.x) / 2.0;

	    //bottom left
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //top left
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, midx, min.y, max.y, min_dist,
		    within_dist, cos_within_ang, on_surf_points, true, true);

	    //bottom midx
	    p.Set(midx, min.y);
	    on_surf_points.Append(p);

	    //top midx
	    p.Set(midx, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, midx, max.x, min.y, max.y, min_dist,
		    within_dist, cos_within_ang, on_surf_points, false, true);

	    //bottom right
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //top right
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	} else if (vclosed) {
	    ON_2dPoint p(0.0, 0.0);
	    double midy = (min.y + max.y) / 2.0;

	    //bottom left
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //left midy
	    p.Set(min.x, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, max.x, min.y, midy, min_dist,
		    within_dist, cos_within_ang, on_surf_points, true, true);

	    //bottom right
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //right midy
	    p.Set(max.x, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, max.x, midy, max.y, min_dist,
		    within_dist, cos_within_ang, on_surf_points, true, false);

	    // top left
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

	    //top right
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	} else {
	    ON_2dPoint p(0.0, 0.0);

	    //bottom left
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //top left
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, max.x, min.y, max.y, min_dist,
		    within_dist, cos_within_ang, on_surf_points, true, true);

	    //bottom right
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //top right
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	}
    }
}


double surface_GetClosestPoint3dFirstOrderByRangeOld(
	const ON_Surface *surf,
        const ON_3dPoint& p,
        const ON_Interval& u_range,
        const ON_Interval& v_range,
        ON_2dPoint& p2d,
        ON_3dPoint& p3d,
        double tol
        )
{
    ON_3dPoint p0;
    ON_2dPoint p2d0;
    ON_3dVector ds, dt, dss, dst, dtt;
    ON_3dVector T, K;
    bool rc = false;
    ON_Interval current_u_range = u_range;
    ON_Interval current_v_range = v_range;

    p2d0.x = (u_range.m_t[0] + u_range.m_t[1]) / 2.0;
    p2d0.y = (v_range.m_t[0] + v_range.m_t[1]) / 2.0;

    int prec = std::cerr.precision();
    std::cerr.precision(15);
    bool notdone = true;
    double current_distance = DBL_MAX;
    double previous_distance = DBL_MAX;
    double distance;
    ON_3dPoint working_p3d;
    ON_3dPoint working_p2d;
    int errcnt = 0;
    while (notdone && (surf->Ev2Der(p2d0.x, p2d0.y, p0, ds, dt, dss, dst, dtt))) {
	if ((distance = p0.DistanceTo(p)) >= previous_distance) {
	    if (++errcnt <= 10) {
		p2d0 = (p2d0 + working_p2d) / 2.0;
		continue;
	    } else {
		break;
	    }
	    /* if (++errcnt > 10) break; */
	} else {
	    previous_distance = distance;
	    working_p3d = p0;
	    working_p2d = p2d0;
	    errcnt = 0;
	}
	ON_EvCurvature(ds, dt, T, K);
	ON_3dVector N = ON_CrossProduct(ds, dt);
	N.Unitize();
	ON_Plane plane(p0, N);
	ON_3dPoint q = plane.ClosestPointTo(p);
	ON_2dVector pullback;
	ON_3dVector vector = q - p0;
	double vlength = vector.Length();

	if (vlength > tol) {
	    rc = true;

	    if (ON_Pullback3dVector(vector, 0.0, ds, dt, dss, dst, dtt,
		    pullback)) {
		int ix = (current_u_range.m_t[0] <= current_u_range.m_t[1]) ? 0 : 1;
		int iy = (current_v_range.m_t[0] <= current_v_range.m_t[1]) ? 0 : 1;
/*
		if (pullback.x > 0.0)
		    current_u_range.m_t[ix] =  (current_u_range.m_t[ix] + p2d0.x)/2.0;
		else
		    current_u_range.m_t[1-ix] = (current_u_range.m_t[1 - ix] + p2d0.x)/2.0;

		if (pullback.y > 0.0)
		    current_v_range.m_t[iy] = (current_v_range.m_t[iy] + p2d0.y)/2.0;
		else
		    current_v_range.m_t[1-iy] = (current_v_range.m_t[1 - iy] + p2d0.y)/2.0;
*/
		ON_2dPoint prev2d = p2d0;
		ON_2dVector prevPB = pullback;
		p2d0 = p2d0 + pullback;
		while ((!current_u_range.Includes(p2d0.x, false)) || (!current_v_range.Includes(p2d0.y, false))) {
		    if (!current_u_range.Includes(p2d0.x, false)) {
			double scalex = 1.0;
			if (p2d0.x < current_u_range.m_t[ix]) {
			    scalex = 1.0 - (p2d0.x - current_u_range.m_t[ix]) / prevPB.x;
			    p2d0.x = current_u_range.m_t[ix];
			    if (scalex > tol)
				p2d0.y = prev2d.y + prevPB.y * scalex;
			} else {
			    scalex = 1.0 - (p2d0.x - current_u_range.m_t[1-ix]) / prevPB.x;
			    p2d0.x = current_u_range.m_t[1-ix];
			    if (scalex > tol)
				p2d0.y = prev2d.y + prevPB.y * scalex;
			}
			if (scalex > tol)
			    prevPB = prevPB * scalex;
		    }
		    if (!current_v_range.Includes(p2d0.y, false)) {
			double scaley = 1.0;
			if (p2d0.y < current_v_range.m_t[iy]) {
			    scaley = 1.0 - (p2d0.y - current_v_range.m_t[iy]) / prevPB.y;
			    p2d0.y = current_v_range.m_t[iy];
			    if (scaley > tol)
				p2d0.x = prev2d.x + prevPB.x * scaley;
			} else {
			    scaley = 1.0 - (p2d0.y - current_v_range.m_t[1-iy]) / prevPB.y;
			    p2d0.y = current_v_range.m_t[1-iy];
			    if (scaley > tol)
				p2d0.x = prev2d.x + prevPB.x * scaley;
			}
			if (scaley > tol)
			    prevPB = prevPB * scaley;
		    }
		}


#if 1
		if (!current_u_range.Includes(p2d0.x, false)) {
		    int i = (current_u_range.m_t[0] <= current_u_range.m_t[1]) ? 0 : 1;
#if 0
		    p2d0.x = (current_u_range.m_t[i] + current_u_range.m_t[1 - i])/2.0;
#else
		    p2d0.x = (p2d0.x < current_u_range.m_t[i]) ?
				    (current_u_range.m_t[i] + prev2d.x)/2.0 : (current_u_range.m_t[1 - i] + prev2d.x)/2.0;
#endif
		}
		if (!current_v_range.Includes(p2d0.y, false)) {
		    int i = (current_v_range.m_t[0] <= current_v_range.m_t[1]) ? 0 : 1;
#if 0
		    p2d0.y = ( current_v_range.m_t[i] + current_v_range.m_t[1 - i] ) / 2.0;
#else
		    p2d0.y = (p2d0.y < current_v_range.m_t[i]) ?
				    (current_v_range.m_t[i] + prev2d.y)/2.0 : (current_v_range.m_t[1 - i] + prev2d.y)/2.0;
#endif
		}
#else
		if ((!current_u_range.Includes(p2d0.x, false)) || (!current_v_range.Includes(p2d0.y, false))) {
		    p2d0.x = (current_u_range.m_t[0] + current_u_range.m_t[1])/2.0;
		    p2d0.y = ( current_v_range.m_t[0] + current_v_range.m_t[1] ) / 2.0;
		}
#endif

	    } else {
		// pullback fails so need to subdivide and try again
		ON_Interval u_interval;
		ON_Interval v_interval;
		ON_2dPoint p2d1;
		ON_3dPoint p3d1;
		for (int iu = 0; iu < 2; iu++) {
		    u_interval = current_u_range;
		    u_interval.m_t[iu] = p2d0.x;
		    for (int iv = 0; iv < 2; iv++) {
			v_interval = current_v_range;
			v_interval.m_t[iv] = p2d0.y;


			//if (!u_interval.IsInterval()
			//        && !v_interval.IsInterval()) {
			if ((u_interval.Length() < tol)
			        && (v_interval.Length() < tol)) {
			    //walked down to nothing
			    p2d1.x = (u_interval.m_t[0]+u_interval.m_t[1])/2.0;
			    p2d1.y = (v_interval.m_t[0]+v_interval.m_t[1])/2.0;
			    p3d1 = surf->PointAt(p2d1.x, p2d1.y);
			    distance = p.DistanceTo(p3d1);
			} else {
			    distance =
				    surface_GetClosestPoint3dFirstOrderByRangeOld(
				            surf, p, u_interval, v_interval,
				            p2d1, p3d1, tol);
			}
			if (distance < tol) {
			    p3d = working_p3d;
			    p2d = working_p2d;
			    return distance;
			} else if (distance < previous_distance) {
			    previous_distance = distance;
			    working_p3d = p3d1;
			    working_p2d = p2d1;
			    errcnt = 0;
			}
		    }
		}
		notdone = false;
	    }
	} else {
	    // can't get any closer
	    notdone = false;
	}
    }
    if (previous_distance < current_distance) {
	current_distance = previous_distance;
	p3d = working_p3d;
	p2d = working_p2d;
    }
    std::cerr.precision(prec);
    return current_distance;
}


ON_BOOL32
surface_GetBoundingBox(
	const ON_Surface *surf,
	const ON_Interval &u_interval,
	const ON_Interval &v_interval,
        ON_BoundingBox& bbox,
	ON_BOOL32 bGrowBox
        )
{
    /* defined static for first time thru initialization, will
     * have to do something else here for multiple threads
     */
    static ON_NurbsSurface *localcopy = ON_NurbsSurface::New();
    ON_NurbsSurface *nurbssurf = NULL;

    if ((nurbssurf = dynamic_cast<ON_NurbsSurface * >(const_cast<ON_Surface *>(surf))) != NULL) {
	*localcopy = *nurbssurf;
	if (localcopy->Trim(0, u_interval) && localcopy->Trim(1, v_interval)) {
	    return localcopy->GetBoundingBox(bbox, bGrowBox);
	}
    } if (surf->GetNurbForm(*localcopy)) {
	if (localcopy->Trim(0, u_interval) && localcopy->Trim(1, v_interval)) {
	    return localcopy->GetBoundingBox(bbox, bGrowBox);
	}
    }

    return false;
}


ON_BOOL32
surface_GetIntervalMinMaxDistance(
	const ON_Surface *surf,
        const ON_3dPoint& p,
	const ON_Interval &u_interval,
	const ON_Interval &v_interval,
        double &min_distance,
        double &max_distance
        )
{
    ON_BoundingBox bbox;

    if (surface_GetBoundingBox(surf,u_interval,v_interval,bbox, false)) {
	min_distance = bbox.MinimumDistanceTo(p);

	max_distance = bbox.MaximumDistanceTo(p);
	/*
	ON_BoundingBox face_bbox;
	double max;
	face_bbox = bbox;
	// try closest maximum distance to each BB face
	// X
	face_bbox.m_min.x = face_bbox.m_max.x = bbox.m_min.x;
	max = face_bbox.MaximumDistanceTo(p);
	if (max < max_distance) {
	    max_distance = max;
	}
	face_bbox.m_min.x = face_bbox.m_max.x = bbox.m_max.x;
	max = face_bbox.MaximumDistanceTo(p);
	if (max < max_distance) {
	    max_distance = max;
	}
	face_bbox.m_min.x = bbox.m_min.x;
	// Y
	face_bbox.m_min.y = face_bbox.m_max.y = bbox.m_min.y;
	max = face_bbox.MaximumDistanceTo(p);
	if (max < max_distance) {
	    max_distance = max;
	}
	face_bbox.m_min.y = face_bbox.m_max.y = bbox.m_max.y;
	max = face_bbox.MaximumDistanceTo(p);
	if (max < max_distance) {
	    max_distance = max;
	}
	face_bbox.m_min.y = bbox.m_min.y;
	// Z
	face_bbox.m_min.z = face_bbox.m_max.z = bbox.m_min.z;
	max = face_bbox.MaximumDistanceTo(p);
	if (max < max_distance) {
	    max_distance = max;
	}
	face_bbox.m_min.z = face_bbox.m_max.z = bbox.m_max.z;
	max = face_bbox.MaximumDistanceTo(p);
	if (max < max_distance) {
	    max_distance = max;
	}
	*/
	return true;
    }
    return false;
}

#if 0
double surface_GetClosestPointUsingSubdivisionNR(const ON_Surface *surf,
        ON_Surface *tmpsurf, const ON_3dPoint& p, ON_Interval &u_interval,
        ON_Interval &v_interval, double &current_closest_dist, ON_2dPoint& p2d,
        ON_3dPoint& p3d, double tol)
{
    int level = 1;
    double u[2] = { u_interval.m_t[0], u_interval.m_t[1]};
    double v[2] = { v_interval.m_t[0], v_interval.m_t[1]};
    double nu[2];
    double nv[2];

    bool done = false;
    bool ll = true;
    bool lr = false;
    bool ul = false;
    bool ur = false;
    while (not done) {
	for (int iv = 0; iv < 2; iv++) {
	    nv[iv] = v[iv];
	    nv[1 - iv] = (v[1] + v[0]) / 2.0;
	    for (int iu = 0; iu < 2; iu++) {
		nu[iu] = u[iu];
		nu[1 - iu] = (u[1] + u[0]) / 2.0;
		if (((nv[1] - nv[0]) > tol) || ((nu[1] - nu[0]) > tol)) {
		    v[1-iv] = nv[1 - iv];
		    u[1-iu] = nu[1 - iu];
		    level++;
		    iu = iv = 2;
		    break;
		} else {
		    if (level == 1)
			done=true;
		    if (ll) {
			ll = false;
			lr = true;
		    } else if (lr) {
			lr = false;
			ul = true;
		    } else if (ul) {
			ul = false;
			ur = true;
		    } else {
			ur = false;
			ll = true;
		    }
		}
	    }
	}
	if (lr) {
	    double ul = u[1] - u[0];
	    double vl = v[1] - v[0];
	    u[0] = u[1];
	    u[1] = u[1] + ul;
	    v[0] = v[1];
	    v[1] = v[1] + vl;
	    level--;
	}
    }
}
#endif


int depthtest = 30;
double
surface_GetClosestPointUsingSubdivisionUlength(
	const ON_Surface *surf,
        const ON_3dPoint& p,
	ON_Interval &u_interval,
	ON_Interval &v_interval,
        double current_closest_dist,
        ON_2dPoint& p2d,
        ON_3dPoint& p3d,
        double tol,
        int level
        )
{
    double u_length = u_interval.Length();
    double v_length = v_interval.Length();
    double min_distance, max_distance;
#if 1
    if (++level > depthtest) {
	if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
	    if (max_distance < current_closest_dist) {
		p2d.x = u_interval.Mid();
		p2d.y = v_interval.Mid();
		ON_3dPoint curr_pnt = surf->PointAt(p2d.x,p2d.y);
		current_closest_dist = curr_pnt.DistanceTo(p);
	    }
	}
	return current_closest_dist;
    }
#endif
    if ((u_length > tol) || (v_length > tol)) {
	ON_Interval new_u_interval = u_interval;
	ON_Interval new_v_interval = v_interval;

	for (int iu = 0; iu < 2; iu++) {
	    if (u_length > tol) {
		new_u_interval.m_t[iu] = u_interval.m_t[iu];
		new_u_interval.m_t[1-iu] = (u_interval.m_t[1] + u_interval.m_t[0])/2.0;
	    }
	    for (int iv = 0; iv < 2; iv++) {
		if (v_length > tol) {
		    new_v_interval.m_t[iv] = v_interval.m_t[iv];
		    new_v_interval.m_t[1-iv] = (v_interval.m_t[1] + v_interval.m_t[0])/2.0;
		}
		if (surface_GetIntervalMinMaxDistance(surf,p,new_u_interval,new_v_interval,min_distance,max_distance)) {
		    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
			double distance = surface_GetClosestPointUsingSubdivisionUlength(surf,p,new_u_interval,new_v_interval,current_closest_dist,p2d,p3d,tol,level);
			if (distance < current_closest_dist-tol) {
			    current_closest_dist = distance;
			    if (current_closest_dist < tol)
				return current_closest_dist;
			}
		    }
		}
	    }
	}
    } else {
	if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
	    if (max_distance <= current_closest_dist) {
		p2d.x = u_interval.Mid();
		p2d.y = v_interval.Mid();
		ON_3dPoint curr_pnt = surf->PointAt(p2d.x,p2d.y);
		current_closest_dist = curr_pnt.DistanceTo(p);
	    }
	}
    }
    return current_closest_dist;
}
double surface_GetClosestPointUsingSubdivision(const ON_Surface *surf,
        const ON_3dPoint& p, ON_Interval &u_interval, ON_Interval &v_interval,
        double current_closest_dist, ON_2dPoint& p2d, ON_3dPoint& p3d,
        double tol, int level)
{
    double min_distance, max_distance;
    ON_Interval new_u_interval = u_interval;
    ON_Interval new_v_interval = v_interval;

    for (int iu = 0; iu < 2; iu++) {
	new_u_interval.m_t[iu] = u_interval.m_t[iu];
	new_u_interval.m_t[1 - iu] = (u_interval.m_t[1] + u_interval.m_t[0]) / 2.0;
	for (int iv = 0; iv < 2; iv++) {
	    new_v_interval.m_t[iv] = v_interval.m_t[iv];
	    new_v_interval.m_t[1 - iv] = (v_interval.m_t[1] + v_interval.m_t[0]) / 2.0;
	    if (surface_GetIntervalMinMaxDistance(surf, p, new_u_interval,new_v_interval, min_distance, max_distance)) {
		if (min_distance < current_closest_dist) {
		    if (NEAR_EQUAL(min_distance,max_distance,tol*tol)) {
			if (max_distance < current_closest_dist) {
			    p2d.x = new_u_interval.Mid();
			    p2d.y = new_v_interval.Mid();
			    p3d = surf->PointAt(p2d.x,p2d.y);
			    current_closest_dist = p3d.DistanceTo(p);
			}
			return current_closest_dist;
		    } else if (NEAR_ZERO(min_distance,tol)) { // (min_distance < current_closest_dist) { // (NEAR_ZERO(min_distance,tol)) { //
			double distance = surface_GetClosestPointUsingSubdivision(surf, p, new_u_interval, new_v_interval,current_closest_dist, p2d, p3d, tol, level);
			if (distance < current_closest_dist) {
			    current_closest_dist = distance;
			    if (current_closest_dist < tol)
				return current_closest_dist;
			}
		    }
		}
	    }
	}
    }
    return current_closest_dist;
}


double
surface_GetOptimalNormalUSplit(const ON_Surface *surf, const ON_Interval &u_interval, const ON_Interval &v_interval,double tol)
{
    ON_3dVector normal[4];
    double u = u_interval.Mid();

    if ((normal[0] = surf->NormalAt(u_interval.m_t[0],v_interval.m_t[0])) &&
	(normal[2] = surf->NormalAt(u_interval.m_t[0],v_interval.m_t[1]))) {
	double step = u_interval.Length()/2.0;
	double stepdir = 1.0;
	u = u_interval.m_t[0] + stepdir * step;

	while (step > tol) {
	    if ((normal[1] = surf->NormalAt(u,v_interval.m_t[0])) &&
		(normal[3] = surf->NormalAt(u,v_interval.m_t[1]))) {
		    double udot_1 = normal[0] * normal[1];
		    double udot_2 = normal[2] * normal[3];
		    if ((udot_1 < 0.0) || (udot_2 < 0.0)) {
			stepdir = -1.0;
		    } else {
			stepdir = 1.0;
		    }
		    step = step / 2.0;
		    u = u + stepdir * step;
	    }
	}
    }
    return u;
}


double
surface_GetOptimalNormalVSplit(const ON_Surface *surf, const ON_Interval &u_interval, const ON_Interval &v_interval,double tol)
{
    ON_3dVector normal[4];
    double v = v_interval.Mid();

    if ((normal[0] = surf->NormalAt(u_interval.m_t[0],v_interval.m_t[0])) &&
	(normal[1] = surf->NormalAt(u_interval.m_t[1],v_interval.m_t[0]))) {
	double step = v_interval.Length()/2.0;
	double stepdir = 1.0;
	v = v_interval.m_t[0] + stepdir * step;

	while (step > tol) {
	    if ((normal[2] = surf->NormalAt(u_interval.m_t[0],v)) &&
		(normal[3] = surf->NormalAt(u_interval.m_t[1],v))) {
		double vdot_1 = normal[0] * normal[2];
		double vdot_2 = normal[1] * normal[3];
		if ((vdot_1 < 0.0) || (vdot_2 < 0.0)) {
		    stepdir = -1.0;
		} else {
		    stepdir = 1.0;
		}
		step = step / 2.0;
		v = v + stepdir * step;
	    }
	}
    }
    return v;
}


//forward for cyclic
double surface_GetClosestPoint3dFirstOrderByRange(const ON_Surface *surf,const ON_3dPoint& p,const ON_Interval& u_range,
        const ON_Interval& v_range,double current_closest_dist,ON_2dPoint& p2d,ON_3dPoint& p3d,double tol,int level);

double surface_GetClosestPoint3dFirstOrderSubdivision(const ON_Surface *surf,
        const ON_3dPoint& p, const ON_Interval &u_interval, double u, const ON_Interval &v_interval, double v,
        double current_closest_dist, ON_2dPoint& p2d, ON_3dPoint& p3d,
        double tol, int level)
{
    double min_distance, max_distance;
    ON_Interval new_u_interval = u_interval;
    ON_Interval new_v_interval = v_interval;

    for (int iu = 0; iu < 2; iu++) {
	new_u_interval.m_t[iu] = u_interval.m_t[iu];
	new_u_interval.m_t[1 - iu] = u;
	for (int iv = 0; iv < 2; iv++) {
	    new_v_interval.m_t[iv] = v_interval.m_t[iv];
	    new_v_interval.m_t[1 - iv] = v;
	    if (surface_GetIntervalMinMaxDistance(surf, p, new_u_interval,new_v_interval, min_distance, max_distance)) {
		double distance = DBL_MAX;
		if (NEAR_ZERO(min_distance,tol)) { // (min_distance < current_closest_dist) {
		    /////////////////////////////////////////
		    // Could check normals and CV angles here
		    /////////////////////////////////////////
		    ON_3dVector normal[4];
		    if ((normal[0] = surf->NormalAt(new_u_interval.m_t[0],new_v_interval.m_t[0])) &&
			(normal[1] = surf->NormalAt(new_u_interval.m_t[1],new_v_interval.m_t[0])) &&
			(normal[2] = surf->NormalAt(new_u_interval.m_t[0],new_v_interval.m_t[1])) &&
			(normal[3] = surf->NormalAt(new_u_interval.m_t[1],new_v_interval.m_t[1]))) {

			    double udot_1 = normal[0] * normal[1];
			    double udot_2 = normal[2] * normal[3];
			    double vdot_1 = normal[0] * normal[2];
			    double vdot_2 = normal[1] * normal[3];

			    if ((udot_1 < 0.0) || (udot_2 < 0.0) || (vdot_1 < 0.0) || (vdot_2 < 0.0)) {
				double u_split,v_split;
				if ((udot_1 < 0.0) || (udot_2 < 0.0)) {
				    //get optimal U split
				    u_split = surface_GetOptimalNormalUSplit(surf,new_u_interval,new_v_interval,tol);
				} else {
				    u_split = new_u_interval.Mid();
				}
				if ((vdot_1 < 0.0) || (vdot_2 < 0.0)) {
				    //get optimal V split
				    v_split = surface_GetOptimalNormalVSplit(surf,new_u_interval,new_v_interval,tol);
				} else {
				    v_split = new_v_interval.Mid();
				}
				distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,new_u_interval,u_split,new_v_interval,v_split,current_closest_dist,p2d,p3d,tol,level++);
				if (distance < current_closest_dist) {
				    current_closest_dist = distance;
				    if (current_closest_dist < tol)
					return current_closest_dist;
				}
			    } else {
				distance = surface_GetClosestPoint3dFirstOrderByRange(surf,p,new_u_interval,new_v_interval,current_closest_dist,p2d,p3d,tol,level++);
				if (distance < current_closest_dist) {
				    current_closest_dist = distance;
				    if (current_closest_dist < tol)
					return current_closest_dist;
				}
			    }
		    }
		}
	    }
	}
    }
    return current_closest_dist;
}


double
surface_GetClosestPoint3dFirstOrderByRange(
	const ON_Surface *surf,
        const ON_3dPoint& p,
        const ON_Interval& u_range,
        const ON_Interval& v_range,
        double current_closest_dist,
        ON_2dPoint& p2d,
        ON_3dPoint& p3d,
        double tol,
        int level
        )
{
    ON_3dPoint p0;
    ON_2dPoint p2d0;
    ON_3dVector ds, dt, dss, dst, dtt;
    ON_2dPoint working_p2d;
    ON_3dPoint working_p3d;
    ON_3dPoint P;
    ON_3dVector Ds, Dt, Dss, Dst, Dtt;
    bool notdone = true;
    double previous_distance = DBL_MAX;
    double distance;
    int errcnt = 0;

    p2d0.x = u_range.Mid();
    p2d0.y = v_range.Mid();

    while (notdone && (surf->Ev2Der(p2d0.x, p2d0.y, p0, ds, dt, dss, dst, dtt))) {
	if ((distance = p0.DistanceTo(p)) >= previous_distance) {
	    if (++errcnt <= 10) {
		p2d0 = (p2d0 + working_p2d) / 2.0;
		continue;
	    } else {
		///////////////////////////
		// Don't Subdivide just not getting any closer
		///////////////////////////
		/*
		double distance =
		        surface_GetClosestPoint3dFirstOrderSubdivision(surf, p,
		                u_range, u_range.Mid(), v_range, v_range.Mid(),
		                current_closest_dist, p2d, p3d, tol, level++);
		if (distance < current_closest_dist) {
		    current_closest_dist = distance;
		    if (current_closest_dist < tol)
			return current_closest_dist;
		}
		*/
		break;
	    }
	} else {
	    if (distance < current_closest_dist) {
		current_closest_dist = distance;
		p3d = p0;
		p2d = p2d0;
		if (current_closest_dist < tol)
		    return current_closest_dist;
	    }
	    previous_distance = distance;
	    working_p3d = p0;
	    working_p2d = p2d0;
	    errcnt = 0;
	}
	ON_3dVector N = ON_CrossProduct(ds, dt);
	N.Unitize();
	ON_Plane plane(p0, N);
	ON_3dPoint q = plane.ClosestPointTo(p);
	ON_2dVector pullback;
	ON_3dVector vector = q - p0;
	double vlength = vector.Length();

	if (vlength > 0.0) {
	    if (ON_Pullback3dVector(vector, 0.0, ds, dt, dss, dst, dtt,
		    pullback)) {
		p2d0 = p2d0 + pullback;
		if (!u_range.Includes(p2d0.x, false)) {
		    int i = (u_range.m_t[0] <= u_range.m_t[1]) ? 0 : 1;
		    p2d0.x =
			    (p2d0.x < u_range.m_t[i]) ?
			            u_range.m_t[i] : u_range.m_t[1 - i];
		}
		if (!v_range.Includes(p2d0.y, false)) {
		    int i = (v_range.m_t[0] <= v_range.m_t[1]) ? 0 : 1;
		    p2d0.y =
			    (p2d0.y < v_range.m_t[i]) ?
			            v_range.m_t[i] : v_range.m_t[1 - i];
		}
	    } else {
		///////////////////////////
		// Subdivide and go again
		///////////////////////////
		notdone = false;
		distance =
		        surface_GetClosestPoint3dFirstOrderSubdivision(surf, p,
		                u_range, u_range.Mid(), v_range, v_range.Mid(),
		                current_closest_dist, p2d, p3d, tol, level++);
		if (distance < current_closest_dist) {
		    current_closest_dist = distance;
		    if (current_closest_dist < tol)
			return current_closest_dist;
		}
		break;
	    }
	} else {
	    // can't get any closer
	    notdone = false;
	    break;
	}
    }
    if (previous_distance < current_closest_dist) {
	current_closest_dist = previous_distance;
	p3d = working_p3d;
	p2d = working_p2d;
    }

    return current_closest_dist;
}


bool surface_GetClosestPoint3dFirstOrder(
	const ON_Surface *surf,
        const ON_3dPoint& p,
        ON_2dPoint& p2d,
        ON_3dPoint& p3d,
        int quadrant = 0,	// optional - determines which quadrant to evaluate from
				//         0 = default
				//         1 from NE quadrant
				//         2 from NW quadrant
				//         3 from SW quadrant
				//         4 from SE quadrant
        double tol = 0.0
        )
{
    ON_3dPoint p0;
    ON_2dPoint p2d0;
    ON_3dVector ds, dt, dss, dst, dtt;
    ON_3dVector T, K;
    bool rc = false;

    int prec = std::cerr.precision();
    std::cerr.precision(15);

    int u_spancnt = surf->SpanCount(0);
    int v_spancnt = surf->SpanCount(1);
    double *uspan = new double[u_spancnt + 2];// adding 2 here because going to divide at midpoint
    double *vspan = new double[v_spancnt + 2];
    double current_distance = DBL_MAX;
    //ON_2dPoint working_p2d;
    //ON_3dPoint working_p3d;
    if (surf->GetSpanVector(0, uspan) && surf->GetSpanVector(1, vspan)) {
	ON_3dPoint P;
	ON_3dVector normal[4], Ds, Dt, Dss, Dst, Dtt;

	double umid = surf->Domain(0).Mid();
	int umid_index = u_spancnt/2;
	for (int u_span_index = 0; u_span_index < u_spancnt + 1;u_span_index++) {
	    if (NEAR_EQUAL(uspan[u_span_index],umid,tol)) {
		umid_index = u_span_index;
		break;
	    } else if (uspan[u_span_index] > umid) {
		for (u_span_index = u_spancnt + 1; u_span_index > 0;u_span_index--) {
		    if (uspan[u_span_index-1] < umid) {
			uspan[u_span_index] = umid;
			umid_index = u_span_index;
			u_spancnt++;
			u_span_index = u_spancnt+1;
			break;
		    } else {
			uspan[u_span_index] = uspan[u_span_index-1];
		    }
		}
	    }
	}
	double vmid = surf->Domain(1).Mid();
	int vmid_index = v_spancnt/2;
	for (int v_span_index = 0; v_span_index < v_spancnt + 1;v_span_index++) {
	    if (NEAR_EQUAL(vspan[v_span_index],vmid,tol)) {
		vmid_index = v_span_index;
		break;
	    } else if (vspan[v_span_index] > vmid) {
		for (v_span_index = v_spancnt + 1; v_span_index > 0;v_span_index--) {
		    if (vspan[v_span_index-1] < vmid) {
			vspan[v_span_index] = vmid;
			vmid_index = v_span_index;
			v_spancnt++;
			v_span_index = v_spancnt+1;
			break;
		    } else {
			vspan[v_span_index] = vspan[v_span_index-1];
		    }
		}
	    }
	}
	if (quadrant == 0) {
	    for (int u_span_index = 1; u_span_index < u_spancnt + 1;
		    u_span_index++) {
		for (int v_span_index = 1; v_span_index < v_spancnt + 1;
			v_span_index++) {
		    ON_Interval u_interval(uspan[u_span_index - 1],
			    uspan[u_span_index]);
		    ON_Interval v_interval(vspan[v_span_index - 1],
			    vspan[v_span_index]);
		    double min_distance,max_distance;

		    int level = 1;
		    if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
			    /////////////////////////////////////////
			    // Could check normals and CV angles here
			    /////////////////////////////////////////
			    double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
			    if (distance < current_distance) {
				current_distance = distance;
				if (current_distance < tol) {
				    rc = true;
				    goto cleanup;
				}
			    }
			}
		    }
		}
	    }
	} else if (quadrant == 1) {
	    if (surf->IsClosed(0)) { //NE,SE,NW.SW
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { //NE,NW,SW,SE
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	} else if (quadrant == 2) {
	    if (surf->IsClosed(0)) { // NW,SW,NE,SE
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { // NW,NE,SW,SE
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	} else if (quadrant == 3) {
	    if (surf->IsClosed(0)) { // SW,NW,SE,NE
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { // SW,SE,NW,NE
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	} else if (quadrant == 4) {
	    if (surf->IsClosed(0)) { // SE,NE,SW,NW
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { // SE,SW,NE,NW
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(surf,p,u_interval,v_interval,min_distance,max_distance)) {
			    if (NEAR_ZERO(min_distance,tol)) { //(min_distance < current_closest_dist-tol) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	}

    }
cleanup:
    delete[] uspan;
    delete[] vspan;

    std::cerr.precision(prec);
    return rc;
}


bool trim_GetClosestPoint3dFirstOrder(
	const ON_BrepTrim& trim,
        const ON_3dPoint& p,
        ON_2dPoint& p2d,
        double& t,
        double tol,
        const ON_Interval* interval,
        bool print
        )
{
    bool rc = false;
    const ON_Surface *surf = trim.SurfaceOf();

    double t0 = interval->Mid();
    ON_3dPoint p3d;
    ON_3dPoint p0;
    ON_3dVector ds,dt,dss,dst,dtt;
    ON_3dVector T,K;
    int prec = std::cerr.precision();
    ON_BoundingBox tight_bbox;
    std::vector<ON_BoundingBox> bbox;
    ON_2dPoint origp2d = p2d;
    std::cerr.precision(15);

    ON_Curve *c = trim.Brep()->m_C2[trim.m_c2i];
    ON_NurbsCurve N;
    if ( 0 == c->GetNurbForm(N) )
      return false;
    if ( N.m_order < 2 || N.m_cv_count < N.m_order )
      return false;

    p2d = trim.PointAt(t);
    int quadrant = 0;     // optional - determines which quadrant to evaluate from
                          //         0 = default
                          //         1 from NE quadrant
                          //         2 from NW quadrant
                          //         3 from SW quadrant
                          //         4 from SE quadrant
    ON_Interval u_interval = surf->Domain(0);
    ON_Interval v_interval = surf->Domain(1);
    if (p2d.y > v_interval.Mid()) {
	// North quadrants -> 1 or 2;
	if (p2d.x > u_interval.Mid()) {
	    quadrant = 1; // NE
	} else {
	    quadrant = 2; //NW
	}
    } else {
	// South quadrants -> 3 or 4;
	if (p2d.x > u_interval.Mid()) {
	    quadrant = 4; // SE
	} else {
	    quadrant = 3; //SW
	}
    }
    if (surface_GetClosestPoint3dFirstOrder(surf,p,p2d,p3d,quadrant,tol)) {
#if 0
	    bool special = true;
	    if (special) return special;
#endif
#if 0
	    double test2d_dist = p2d.DistanceTo(origp2d);
	    if ((!surf->IsAtSingularity(p2d.x,p2d.y,false)) && ((test2d_dist > u_interval.Length()/2.0) || (test2d_dist > v_interval.Length()/2.0))) { // !NEAR_ZERO(test2d_dist,tol)) {
		std::cerr << "2d First Order failed:" << std::endl;
		std::cerr << "     test2d_dist: " << test2d_dist << std::endl;
		std::cerr << "     p2d: < " << p2d.x << ", " << p2d.y << ", 0 >" << std::endl;
		std::cerr << "     origp2d: < " << origp2d.x << ", " << origp2d.y << ", 0 >" << std::endl;
		std::cerr << "     p: < " << p.x << ", " << p.y << ", " << p.z << " >" << std::endl;
		std::cerr << "     p3d: < " << p3d.x << ", " << p3d.y << ", " << p3d.z << " >" << std::endl;
	    }
#endif
#if 0
			    double test3d_dist = p.DistanceTo(p3d);
			    if (!NEAR_ZERO(test3d_dist,tol)) {
				////////////////////
				int prec = std::cerr.precision();
				std::cerr.precision(15);
				////////////////////
				std::cerr << "First Order failed for Surf - " << std::endl;
				std::cerr << "     test_dist: " << test3d_dist << std::endl;
				std::cerr << "     p: < " << p.x << ", " << p.y << ", " << p.z << " >" << std::endl;
				std::cerr << "     p3d: < " << p3d.x << ", " << p3d.y << ", " << p3d.z << " >" << std::endl;
				std::cerr.precision(prec);
			    }
#endif
	ON_3dPoint test2dp = surf->PointAt(p2d.x,p2d.y);
	ON_BezierCurve B;
	bool bGrowBox = false;
	ON_3dVector d1,d2;
	double max_dist_to_closest_pt = DBL_MAX;
	ON_Interval *span_interval = new ON_Interval[N.m_cv_count - N.m_order + 1];
	double *min_distance = new double[N.m_cv_count - N.m_order + 1];
	double *max_distance = new double[N.m_cv_count - N.m_order + 1];
	bool *skip = new bool[N.m_cv_count - N.m_order + 1];
	bbox.resize(N.m_cv_count - N.m_order + 1);
	for ( int span_index = 0; span_index <= N.m_cv_count - N.m_order; span_index++ )
	{
	    skip[span_index] = true;
	  if ( !(N.m_knot[span_index + N.m_order-2] < N.m_knot[span_index + N.m_order-1]) )
	    continue;

	  // check for span out of interval
	  int i = (interval->m_t[0] <= interval->m_t[1]) ? 0 : 1;
	  if ( N.m_knot[span_index + N.m_order-2] > interval->m_t[1-i] )
	    continue;
	  if ( N.m_knot[span_index + N.m_order-1] < interval->m_t[i] )
	    continue;

	  if ( !N.ConvertSpanToBezier( span_index, B ) )
	    continue;
	  ON_Interval bi = B.Domain();
	  if ( !B.GetTightBoundingBox(tight_bbox,bGrowBox,NULL) )
	    continue;
	  bbox[span_index] = tight_bbox;
	  d1 = tight_bbox.m_min - p2d;
	  d2 = tight_bbox.m_max - p2d;
	  min_distance[span_index] = tight_bbox.MinimumDistanceTo(p2d);

	  if (print == true) {
		std::cout << "in bbox_" << span_index << " rpp " << bbox[span_index].m_min.x << " " << bbox[span_index].m_max.x << " " << bbox[span_index].m_min.y << " " << bbox[span_index].m_max.y << " -0.001 0.001" << std::endl;
	  }
	  if (min_distance[span_index] > max_dist_to_closest_pt) {
	      max_distance[span_index] = DBL_MAX;
	      continue;
	  }
	  skip[span_index] = false;
	  span_interval[span_index].m_t[0] = ((N.m_knot[span_index + N.m_order-2]) < interval->m_t[i]) ? interval->m_t[i] : N.m_knot[span_index + N.m_order-2];
	  span_interval[span_index].m_t[1] = ((N.m_knot[span_index + N.m_order-1]) > interval->m_t[1 -i]) ? interval->m_t[1 -i] : (N.m_knot[span_index + N.m_order-1]);
	  ON_3dPoint d1sq(d1.x*d1.x,d1.y*d1.y,0.0),d2sq(d2.x*d2.x,d2.y*d2.y,0.0);
	  double distancesq;
	  if(d1sq.x < d2sq.x) {
	    if (d1sq.y < d2sq.y) {
		if ((d1sq.x + d2sq.y) < (d2sq.x + d1sq.y)) {
		    distancesq = d1sq.x + d2sq.y;
		} else {
		    distancesq = d2sq.x + d1sq.y;
		}
	    } else {
		if ((d1sq.x + d1sq.y) < (d2sq.x + d2sq.y)) {
		    distancesq = d1sq.x + d1sq.y;
		} else {
		    distancesq = d2sq.x + d2sq.y;
		}
	    }
	  } else {
	    if (d1sq.y < d2sq.y) {
		if ((d1sq.x + d1sq.y) < (d2sq.x + d2sq.y)) {
		    distancesq = d1sq.x + d1sq.y;
		} else {
		    distancesq = d2sq.x + d2sq.y;
		}
	    } else {
		if ((d1sq.x + d2sq.y) < (d2sq.x + d1sq.y)) {
		    distancesq = d1sq.x + d2sq.y;
		} else {
		    distancesq = d2sq.x + d1sq.y;
		}
	    }
	  }
	  max_distance[span_index] = sqrt(distancesq);
	  if (max_distance[span_index] < max_dist_to_closest_pt) {
	      max_dist_to_closest_pt = max_distance[span_index];
	  }
	  if (max_distance[span_index] < min_distance[span_index]) {
	      // should only be here for near equal fuzz
	      min_distance[span_index] = max_distance[span_index];
	  }
	}
	for ( int span_index = 0; span_index <= N.m_cv_count - N.m_order; span_index++ )
	{

	  if ( skip[span_index] )
	    continue;

	  if (min_distance[span_index] > max_dist_to_closest_pt) {
	      skip[span_index] = true;
	      continue;
	  }

	}

	ON_3dPoint q;
	ON_3dPoint point;
	double closest_distance = DBL_MAX;
	double closestT = DBL_MAX;
	for ( int span_index = 0; span_index <= N.m_cv_count - N.m_order; span_index++ )
	{
	    if (skip[span_index]) {
		continue;
	    }
	    t0 = span_interval[span_index].Mid();
	    bool closestfound = false;
	    bool notdone = true;
	    double distance, previous_distance = DBL_MAX;
	    ON_3dVector firstDervative, secondDervative;
	    while (notdone
		    && trim.Ev2Der(t0, point, firstDervative, secondDervative)
		    && ON_EvCurvature(firstDervative, secondDervative, T, K)) {
		ON_Line line(point, point + 100.0 * T);
		q = line.ClosestPointTo(p2d);
		double delta_t = (firstDervative * (q - point))
		        / (firstDervative * firstDervative);
		double new_t0 = t0 + delta_t;
		if (!span_interval[span_index].Includes(new_t0, false)) {
		    // limit to interval
		    int i = (span_interval[span_index].m_t[0] <= span_interval[span_index].m_t[1]) ? 0 : 1;
		    new_t0 =
			    (new_t0 < span_interval[span_index].m_t[i]) ?
				    span_interval[span_index].m_t[i] : span_interval[span_index].m_t[1 - i];
		}
		delta_t = new_t0 - t0;
		t0 = new_t0;
		point = trim.PointAt(t0);
		distance = point.DistanceTo(p2d);
		if (distance < previous_distance) {
		    closestfound = true;
		    closestT = t0;
		    previous_distance = distance;
		    if (fabs(delta_t) < tol) {
			notdone = false;
		    }
		} else {
		    notdone = false;
		}
	    }
	    if (closestfound && (distance < closest_distance)) {
		closest_distance = distance;
		rc = true;
		t = closestT;
	    }
	}
	delete [] span_interval;
	delete [] min_distance;
	delete [] max_distance;
	delete [] skip;

    }
    std::cerr.precision(prec);

    return rc;
}
#if 1
static int lfi = -1;
static int hfi = 10000;
static int dl_li = -1;
static int dh_li = 10000;
static int dl_lti = -1;
static int dh_lti = 10000;
#endif
void poly2tri_CDT(struct bu_list *vhead, ON_BrepFace &face,
	const struct rt_tess_tol *ttol, const struct bn_tol *tol,
	const struct rt_view_info *info, bool watertight = false, int plottype =
		0, int num_points = -1.0)
{
	int fi = face.m_face_index;
    ON_RTree rt_trims, rt_points;
    ON_2dPointArray on_surf_points;
    const ON_Surface *s = face.SurfaceOf();
    double surface_width, surface_height;
    std::vector<ON_3dPoint *> singularity_points;
    fastf_t max_dist = 0.0;
    p2t::CDT* cdt = NULL;
    ON_BoundingBox loop_bb;

#if 1
    if ((fi > hfi) || (fi < lfi))
	return;
#endif
    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	if ((surface_width < tol->dist) || (surface_height < tol->dist)) {
	    return;
	}
	max_dist = sqrt(
		surface_width * surface_width + surface_height * surface_height)
		/ 10.0;
    }

    std::map<p2t::Point *, ON_3dPoint *> *pointmap = new std::map<p2t::Point *, ON_3dPoint *>();

    for (int li = 0; li < face.LoopCount(); li++) {
	ON_2dPointArray on_loop_points;
	std::vector<p2t::Point*> polyline;
	ON_BrepLoop *loop = face.Loop(li);
#if 1
	if ((li > dh_li) || (li < dl_li))
	    continue;
#endif

	for (int lti = 0; lti < loop->TrimCount(); lti++) {
	    ON_BrepTrim *trim = loop->Trim(lti);
	    ON_Interval d = trim->Domain();
	    ON_BrepEdge *edge = trim->Edge();
#if 1
		if ((lti > dh_lti) || (lti < dl_lti))
		continue;
#endif

	    if (trim->m_type == ON_BrepTrim::singular) {
		ON_BrepVertex& v1 = face.Brep()->m_V[trim->m_vi[0]];
		ON_3dPoint *p3d = new ON_3dPoint(v1.Point());
		p2t::Point *p = new p2t::Point(trim->PointAtStart().x,
			trim->PointAtStart().y);
		polyline.push_back(p);
		on_loop_points.Append(trim->PointAtStart());
		(*pointmap)[p] = p3d;
		// vector just used for freeing 3d point of singularity
		singularity_points.push_back(p3d);
		continue;
	    }

	    std::map<double, ON_3dPoint *> *param_points3d = NULL;
	    if (watertight) {
		if (!edge->m_edge_user.p) {
		    (void) getEdgePoints(*edge, max_dist, ttol, tol, info);
		}
		if (edge->m_edge_user.p) {
		    param_points3d = (std::map<double, ON_3dPoint *> *) edge->m_edge_user.p;
		}
	    } else {
		if (!trim->m_trim_user.p) {
		    (void) getEdgePoints(*trim, max_dist, ttol, tol, info);
		}
		if (trim->m_trim_user.p) {
		    param_points3d = (std::map<double, ON_3dPoint *> *) trim->m_trim_user.p;
		}
	    }
	    if (param_points3d != NULL) {
		ON_3dPoint boxmin;
		ON_3dPoint boxmax;

		if (trim->GetBoundingBox(boxmin, boxmax, false)) {
		    if (!trim->m_bRev3d || !watertight) {
			double t0, t1;
			trim->GetDomain(&t0, &t1);
			ON_2dPoint p2d = trim->PointAtStart();
			std::map<double, ON_3dPoint*>::const_iterator i;
			int count=0;
			for (i = param_points3d->begin();
				i != param_points3d->end();) {
			    double t = (*i).first;
			    ON_3dPoint *p3d = (*i).second;
			    p2d = trim->PointAt(d.ParameterAt(t));
			    if (++i == param_points3d->end())
				continue;
#define PULLBACK_TESTING
#ifdef PULLBACK_TESTING
			    double tpercent = t;
			    t = d.ParameterAt(t);
			    ON_Interval interval = trim->Domain();
#if 0
			    if (trim->Edge()->IsClosed()) {
				/*
				if (NEAR_EQUAL(t, 0.0, tol->dist_sq)) {
				    t = interval.m_t[0];
				    //interval.m_t[1] = interval.Mid();
				} else if (NEAR_EQUAL(t, 1.0, tol->dist_sq)) {
				    t = interval.m_t[1];
				    //interval.m_t[0] = interval.Mid();
				} else {
				    t = interval.Mid();
				}
				*/
				if (t < interval.Mid()) {
				    t = interval.m_t[0];
				} else {
				    t = interval.m_t[1];
				}
			    } else {
				//t = interval.Mid();
				if (t < interval.Mid()) {
				    t = (interval.m_t[0] + interval.Mid())/2.0;
				} else {
				    t = (interval.Mid() + interval.m_t[1])/2.0;
				}
			    }
#endif
			    bool print = false;
			    ON_2dPoint p2dorig = p2d;
			    if (trim_GetClosestPoint3dFirstOrder(*trim, *p3d, p2d,
				    t, tol->dist, &interval, print)) {
#if 0
				bool special = true;
				count++;
				if (special && (count > 3)) return;
#endif
				ON_2dPoint tp2d = trim->PointAt(t);
				ON_3dPoint tp3d = s->PointAt(tp2d.x, tp2d.y);
				ON_3dPoint tp3d2 = s->PointAt(p2d.x, p2d.y);
				ON_3dPoint tp3d3 = s->PointAt(p2dorig.x, p2dorig.y);
				/*
				 if (!NEAR_EQUAL(t,(t0 + (t1 - t0) * tpercent),tol->dist_sq)) {
				 std::cerr << "First Order failed for Face - " << fi << std::endl;
				 std::cerr << "     tpercent: " << tpercent << " (li=" << li << ")" << " (lti=" << lti << ")"<< std::endl;
				 std::cerr << "     found: " << t << " should be: " << (t0 + (t1 - t0) * tpercent) << ")" << std::endl;
				 std::cerr << "     p3d: < " << p3d->x << ", " << p3d->y << ", " << p3d->z << " >" << std::endl;
				 std::cerr << "     tp3d: < " << tp3d.x << ", " << tp3d.y << ", " << tp3d.z << " >" << std::endl;
				 std::cerr << "     p2d: < " << p2d.x << ", " << p2d.y << ", 0 >" << std::endl;
				 std::cerr << "     tp2d: < " << tp2d.x << ", " << tp2d.y << ", 0 >" << std::endl;
				 }
				 */
#if 0
				 if (!NEAR_ZERO(tp2d.DistanceTo(p2d),tol->dist)) {
				 std::cerr << "2d First Order failed for Face - " << fi << std::endl;
				 std::cerr << "     tp2d.DistanceTo(p2d): " << tp2d.DistanceTo(p2d) << std::endl;
				 std::cerr << "     p2d: < " << p2d.x << ", " << p2d.y << ", 0 >" << std::endl;
				 std::cerr << "     tp2d: < " << tp2d.x << ", " << tp2d.y << ", 0 >" << std::endl;
				 }
#endif
#if 0
				 double test_dist = p3d->DistanceTo(tp3d);
				double test_dist2 = p3d->DistanceTo(tp3d2);
				double test_dist3 = p3d->DistanceTo(tp3d3);
				if (!NEAR_ZERO(test_dist,tol->dist)) {
				    ////////////////////
				    int prec = std::cerr.precision();
				    std::cerr.precision(15);
				    ////////////////////
				    std::cerr << "First Order failed for Face - "
					    << fi << std::endl;
				    std::cerr << "     test_dist: " << test_dist << std::endl;
				    std::cerr << "     tpercent: " << tpercent
					    << " (li=" << li << ")" << " (lti="
					    << lti << ")" << std::endl;
				    std::cerr << "     found: " << t
					    << " should be: "
					    << (t0 + (t1 - t0) * tpercent) << ")"
					    << std::endl;
				    std::cerr << "     p3d: < " << p3d->x << ", "
					    << p3d->y << ", " << p3d->z << " >"
					    << std::endl;
				    std::cerr << "     tp3d: < " << tp3d.x << ", "
					    << tp3d.y << ", " << tp3d.z << " >"
					    << std::endl;
				    std::cerr << "     p2d: < " << p2d.x << ", "
					    << p2d.y << ", 0 >" << std::endl;
				    std::cerr << "     tp2d: < " << tp2d.x << ", "
					    << tp2d.y << ", 0 >" << std::endl;
				    std::cerr.precision(prec);
				}
#endif
			    } else {
				p2d = trim->PointAt(t);
			    }
#else
			    p2d = trim->PointAt(t0 + (t1 - t0) * t);
#endif

			    // map point to last entry to 3d point
			    p2t::Point *p = new p2t::Point(p2d.x, p2d.y);
			    polyline.push_back(p);
			    on_loop_points.Append(p2d);
			    (*pointmap)[p] = p3d;
			}
		    } else {
			double t0, t1;
			trim->GetDomain(&t0, &t1);
			ON_2dPoint p2d = trim->PointAtStart();
			std::map<double, ON_3dPoint*>::const_reverse_iterator i;
			int count=0;
			for (i = param_points3d->rbegin();
				i != param_points3d->rend();) {
			    double t = 1.0 - (*i).first;
			    ON_3dPoint *p3d = (*i).second;
			    p2d = trim->PointAt(d.ParameterAt(t));
			    if (++i == param_points3d->rend())
				continue;
#define PULLBACK_TESTING
#ifdef PULLBACK_TESTING
			    double tpercent = t;
			    t = d.ParameterAt(t);
			    ON_Interval interval = trim->Domain();
#if 0
			    if (trim->Edge()->IsClosed()) {
				/*
				if (NEAR_EQUAL(t, 0.0, tol->dist_sq)) {
				    t = interval.m_t[0];
				    //interval.m_t[1] = interval.Mid();
				} else if (NEAR_EQUAL(t, 1.0, tol->dist_sq)) {
				    t = interval.m_t[1];
				    //interval.m_t[0] = interval.Mid();
				} else {
				    t = interval.Mid();
				}
				*/
				if (t < interval.Mid()) {
				    t = interval.m_t[0];
				} else {
				    t = interval.m_t[1];
				}
			    } else {
				//t = interval.Mid();
				if (t < interval.Mid()) {
				    t = (interval.m_t[0] + interval.Mid())/2.0;
				} else {
				    t = (interval.Mid() + interval.m_t[1])/2.0;
				}
			    }
#endif
			    bool print = false;
			    ON_2dPoint p2dorig = p2d;
			    if (trim_GetClosestPoint3dFirstOrder(*trim, *p3d, p2d,
				    t, tol->dist, &interval, print)) {
#if 0
				bool special = true;
				count++;
				if (special && (count > 3)) return;
#endif
				ON_2dPoint tp2d = trim->PointAt(t);
				ON_3dPoint tp3d = s->PointAt(tp2d.x, tp2d.y);
				ON_3dPoint tp3d2 = s->PointAt(p2d.x, p2d.y);
				ON_3dPoint tp3d3 = s->PointAt(p2dorig.x, p2dorig.y);
				/*
				 if (!NEAR_EQUAL(t,(t0 + (t1 - t0) * tpercent),tol->dist_sq)) {
				 std::cerr << "First Order failed for Face - " << fi << std::endl;
				 std::cerr << "     tpercent: " << tpercent << " (li=" << li << ")" << " (lti=" << lti << ")"<< std::endl;
				 std::cerr << "     found: " << t << " should be: " << (t0 + (t1 - t0) * tpercent) << ")" << std::endl;
				 std::cerr << "     p3d: < " << p3d->x << ", " << p3d->y << ", " << p3d->z << " >" << std::endl;
				 std::cerr << "     tp3d: < " << tp3d.x << ", " << tp3d.y << ", " << tp3d.z << " >" << std::endl;
				 std::cerr << "     p2d: < " << p2d.x << ", " << p2d.y << ", 0 >" << std::endl;
				 std::cerr << "     tp2d: < " << tp2d.x << ", " << tp2d.y << ", 0 >" << std::endl;
				 }
				 */
#if 0
				 if (!NEAR_ZERO(tp2d.DistanceTo(p2d),tol->dist)) {
				 std::cerr << "2d First Order failed for Face - " << fi << std::endl;
				 std::cerr << "     tp2d.DistanceTo(p2d): " << tp2d.DistanceTo(p2d) << std::endl;
				 std::cerr << "     p2d: < " << p2d.x << ", " << p2d.y << ", 0 >" << std::endl;
				 std::cerr << "     tp2d: < " << tp2d.x << ", " << tp2d.y << ", 0 >" << std::endl;
				 }
#endif
#if 0
				 double test_dist = p3d->DistanceTo(tp3d);
				double test_dist2 = p3d->DistanceTo(tp3d2);
				double test_dist3 = p3d->DistanceTo(tp3d3);
				if (!NEAR_ZERO(test_dist,tol->dist)) {
				    ////////////////////
				    int prec = std::cerr.precision();
				    std::cerr.precision(15);
				    ////////////////////
				    std::cerr << "First Order failed for Face - "
					    << fi << std::endl;
				    std::cerr << "     test_dist: " << test_dist << std::endl;
				    std::cerr << "     tpercent: " << tpercent
					    << " (li=" << li << ")" << " (lti="
					    << lti << ")" << std::endl;
				    std::cerr << "     found: " << t
					    << " should be: "
					    << (t0 + (t1 - t0) * tpercent) << ")"
					    << std::endl;
				    std::cerr << "     p3d: < " << p3d->x << ", "
					    << p3d->y << ", " << p3d->z << " >"
					    << std::endl;
				    std::cerr << "     tp3d: < " << tp3d.x << ", "
					    << tp3d.y << ", " << tp3d.z << " >"
					    << std::endl;
				    std::cerr << "     p2d: < " << p2d.x << ", "
					    << p2d.y << ", 0 >" << std::endl;
				    std::cerr << "     tp2d: < " << tp2d.x << ", "
					    << tp2d.y << ", 0 >" << std::endl;
				    std::cerr.precision(prec);
				}
#endif
			    } else {
				p2d = trim->PointAt(t);
			    }
#else
			    p2d = trim->PointAt(t0 + (t1 - t0) * t);
#endif

			    // map point to last entry to 3d point
			    p2t::Point *p = new p2t::Point(p2d.x, p2d.y);
			    polyline.push_back(p);
			    on_loop_points.Append(p2d);
			    (*pointmap)[p] = p3d;
			}
		    }
		}
	    }
	}

	if (on_loop_points.Count() > 2) {
	    for (int i = 1; i <= on_loop_points.Count(); i++) {
		ON_2dPoint *start = NULL;
		ON_2dPoint *end = NULL;
		if (i == on_loop_points.Count()) {
		    start = on_loop_points.At(i - 1);
		    end = on_loop_points.At(0);
		} else {
		    start = on_loop_points.At(i - 1);
		    end = on_loop_points.At(i);
		}
		ON_Line *line = new ON_Line(*start, *end);
		ON_BoundingBox bb = line->BoundingBox();
		bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
		bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
		bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
		bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
		bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
		bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

		rt_trims.Insert2d(bb.Min(), bb.Max(), line);
	    }
	} else {
	    return;
	}
#ifdef DEBUG_MY
	std::vector<p2t::Point*>::const_iterator i;
	int count = 0;
	for (i = polyline.begin(); i != polyline.end(); i++) {
	    p2t::Point* cp = *i;
	    std::cerr << "in p" << count++ << " sph " <<  cp->x << " " << cp->y << " " << 0.0 << " 1.5" << std::endl;
	}
	return;
#endif
	if (li == 0) {
	    cdt = new p2t::CDT(polyline);
	} else {
	    cdt->AddHole(polyline);
	}

    }

    getSurfacePoints(face, ttol, tol, info, on_surf_points);

    for (int i = 0; i < on_surf_points.Count(); i++) {
	ON_SimpleArray<void*> results;
	ON_2dPoint *p = on_surf_points.At(i);

	rt_trims.Search2d((const double *) p, (const double *) p, results);

	if (results.Count() > 0) {
	    bool on_edge = false;
	    for (int ri = 0; ri < results.Count(); ri++) {
		double dist;
		ON_Line *l = (ON_Line *) *results.At(ri);
		dist = l->MinimumDistanceTo(*p);
		if (NEAR_ZERO(dist,tol->dist)) {
		    on_edge = true;
		    break;
		}
	    }
	    if (!on_edge) {
		cdt->AddPoint(new p2t::Point(p->x, p->y));
	    }
	} else {
	    cdt->AddPoint(new p2t::Point(p->x, p->y));
	}
    }
    ON_SimpleArray<void*> results;
    ON_BoundingBox bb = rt_trims.BoundingBox();
    rt_trims.Search2d((const double *) bb.m_min, (const double *) bb.m_max,
	    results);

    if (results.Count() > 0) {
	for (int ri = 0; ri < results.Count(); ri++) {
	    ON_Line *l = (ON_Line *) *results.At(ri);
	    delete l;
	}
    }
    rt_trims.RemoveAll();

    if ((plottype < 3)) {
	cdt->Triangulate(true, num_points);
    } else {
	cdt->Triangulate(false, num_points);
    }

    if (plottype < 3) {
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	if (plottype == 0) { // shaded tris 3d
	    ON_3dPoint pnt[3] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
	    ON_3dVector norm[3] = {ON_3dVector(), ON_3dVector(), ON_3dVector()};
	    point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
	    vect_t nv[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};

	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    if (s->EvNormal(p->x, p->y, pnt[j], norm[j])) {
			if (watertight) {
			    std::map<p2t::Point *, ON_3dPoint *>::iterator ii =
				    pointmap->find(p);
			    if (ii != pointmap->end()) {
				pnt[j] = *((*ii).second);
			    }
			}
			if (face.m_bRev) {
			    norm[j] = norm[j] * -1.0;
			}
			VMOVE(pt[j], pnt[j]);
			VMOVE(nv[j], norm[j]);
		    }
		}
		//tri one
		RT_ADD_VLIST(vhead, nv[0], BN_VLIST_TRI_START);
		RT_ADD_VLIST(vhead, nv[0], BN_VLIST_TRI_VERTNORM);
		RT_ADD_VLIST(vhead, pt[0], BN_VLIST_TRI_MOVE);
		RT_ADD_VLIST(vhead, nv[1], BN_VLIST_TRI_VERTNORM);
		RT_ADD_VLIST(vhead, pt[1], BN_VLIST_TRI_DRAW);
		RT_ADD_VLIST(vhead, nv[2], BN_VLIST_TRI_VERTNORM);
		RT_ADD_VLIST(vhead, pt[2], BN_VLIST_TRI_DRAW);
		RT_ADD_VLIST(vhead, pt[0], BN_VLIST_TRI_END);
	    }
	} else if (plottype == 1) { // tris 3d wire
	    ON_3dPoint pnt[3] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};;
	    ON_3dVector norm[3] = {ON_3dVector(), ON_3dVector(), ON_3dVector()};;
	    point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};

	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    if (s->EvNormal(p->x, p->y, pnt[j], norm[j])) {
			if (watertight) {
			    std::map<p2t::Point *, ON_3dPoint *>::iterator ii =
				    pointmap->find(p);
			    if (ii != pointmap->end()) {
				pnt[j] = *((*ii).second);
			    }
			}
			if (face.m_bRev) {
			    norm[j] = norm[j] * -1.0;
			}
			VMOVE(pt[j], pnt[j]);
		    }
		}
		//tri one
		RT_ADD_VLIST(vhead, pt[0], BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt[1], BN_VLIST_LINE_DRAW);
		RT_ADD_VLIST(vhead, pt[2], BN_VLIST_LINE_DRAW);
		RT_ADD_VLIST(vhead, pt[0], BN_VLIST_LINE_DRAW);

	    }
	} else if (plottype == 2) { // tris 2d
	    point_t pt1 = VINIT_ZERO;
	    point_t pt2 = VINIT_ZERO;

	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;

		for (size_t j = 0; j < 3; j++) {
		    if (j == 0) {
			p = t->GetPoint(2);
		    } else {
			p = t->GetPoint(j - 1);
		    }
		    pt1[0] = p->x;
		    pt1[1] = p->y;
		    pt1[2] = 0.0;
		    p = t->GetPoint(j);
		    pt2[0] = p->x;
		    pt2[1] = p->y;
		    pt2[2] = 0.0;
		    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	    }
	}
    } else if (plottype == 3) {
	std::list<p2t::Triangle*> tris = cdt->GetMap();
	std::list<p2t::Triangle*>::iterator it;
	point_t pt1 = VINIT_ZERO;
	point_t pt2 = VINIT_ZERO;

	for (it = tris.begin(); it != tris.end(); it++) {
	    p2t::Triangle* t = *it;
	    p2t::Point *p = NULL;
	    for (size_t j = 0; j < 3; j++) {
		if (j == 0) {
		    p = t->GetPoint(2);
		} else {
		    p = t->GetPoint(j - 1);
		}
		pt1[0] = p->x;
		pt1[1] = p->y;
		pt1[2] = 0.0;
		p = t->GetPoint(j);
		pt2[0] = p->x;
		pt2[1] = p->y;
		pt2[2] = 0.0;
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	    }
	}
    } else if (plottype == 4) {
	std::vector<p2t::Point*>& points = cdt->GetPoints();
	point_t pt = VINIT_ZERO;

	for (size_t i = 0; i < points.size(); i++) {
	    p2t::Point *p = NULL;
	    p = (p2t::Point *) points[i];
	    pt[0] = p->x;
	    pt[1] = p->y;
	    pt[2] = 0.0;
	    RT_ADD_VLIST(vhead, pt, BN_VLIST_POINT_DRAW);
	}
    }

    std::map<p2t::Point *, ON_3dPoint *>::iterator ii;
    for (ii = pointmap->begin(); ii != pointmap->end(); pointmap->erase(ii++))
	;
    while (!singularity_points.empty()) {
	delete singularity_points.back();
	singularity_points.pop_back();
    }
    delete pointmap;
#ifdef VALGRIND_CHECK
    // free edge/trim points currently stored in edge->m_edge_user.p/trim->m_trim_user.p for valgrind check
    for (int li = 0; li < face.LoopCount(); li++) {
	ON_BrepLoop *loop = face.Loop(li);

	for (int lti = 0; lti < loop->TrimCount(); lti++) {
	    ON_BrepTrim *trim = loop->Trim(lti);
	    ON_BrepEdge *edge = trim->Edge();

	    if (trim->m_type != ON_BrepTrim::singular) {
		// temporarily free param_points3d for valgrind check
		std::map<double, ON_3dPoint *> *points = NULL;
		if (watertight) {
		    if (edge->m_edge_user.p) {
			points = (std::map<double, ON_3dPoint *> *)edge->m_edge_user.p;
			edge->m_edge_user.p = NULL;
		    }
		} else {
		    if (trim->m_trim_user.p) {
			points = (std::map<double, ON_3dPoint *> *) trim->m_trim_user.p;
			trim->m_trim_user.p = NULL;
		    }
		}
		if (points != NULL) {
		    std::map<double, ON_3dPoint*>::const_iterator i;
		    for (i = points->begin();
			    i != points->end();i++) {
			ON_3dPoint *p3d = (*i).second;
			delete p3d;
		    }
		    points->clear();
		    delete points;
		}
	    }
	}
    }
#endif
    if (cdt != NULL) {
	std::vector<p2t::Point*> v = cdt->GetPoints();
	while (!v.empty()) {
	    delete v.back();
	    v.pop_back();
	}
	if (plottype < 4)
	    delete cdt;
    }
    return;
}


void
plot_face_trim(struct bu_list *vhead, ON_BrepFace &face, int plotres, bool dim3d)
{
    const ON_Surface* surf = face.SurfaceOf();
    double umin, umax;
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
    ON_2dPoint from(0.0, 0.0);
    ON_2dPoint to(0.0, 0.0);

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


int
rt_brep_adaptive_plot(struct rt_db_internal *ip, const struct rt_view_info *info)
{
    TRACE1("rt_brep_adaptive_plot");
    struct rt_brep_internal* bi;
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;

    BU_CK_LIST_HEAD(info->vhead);
    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    ON_Brep* brep = bi->brep;
    int gridres = 10;
    int isocurveres = 100;

    for (int index = 0; index < brep->m_F.Count(); index++) {
	ON_BrepFace& face = brep->m_F[index];
	const ON_Surface *surf = face.SurfaceOf();

	if (surf->IsClosed(0) || surf->IsClosed(1)) {
	    ON_SumSurface *sumsurf = const_cast<ON_SumSurface *> (ON_SumSurface::Cast(surf));
	    if (sumsurf != NULL) {
		SurfaceTree* st = new SurfaceTree(&face, true, 2);

		plot_face_from_surface_tree(info->vhead, st, isocurveres, gridres);

		delete st;
	    } else {
		ON_RevSurface *revsurf = const_cast<ON_RevSurface *> (ON_RevSurface::Cast(surf));

		if (revsurf != NULL) {
		    SurfaceTree* st = new SurfaceTree(&face, true, 0);

		    plot_face_from_surface_tree(info->vhead, st, isocurveres, gridres);

		    delete st;
		}
	    }
	}
    }

    for (int index = 0; index < bi->brep->m_E.Count(); index++) {
	ON_BrepEdge& e = brep->m_E[index];
	const ON_Curve* crv = e.EdgeCurveOf();

	if (crv->IsLinear()) {
	    ON_BrepVertex& v1 = brep->m_V[e.m_vi[0]];
	    ON_BrepVertex& v2 = brep->m_V[e.m_vi[1]];
	    VMOVE(pt1, v1.Point());
	    VMOVE(pt2, v2.Point());
	    RT_ADD_VLIST(info->vhead, pt1, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(info->vhead, pt2, BN_VLIST_LINE_DRAW);
	} else {
	    point_t endpt;
	    ON_Interval dom = crv->Domain();

	    ON_3dPoint p = crv->PointAt(dom.ParameterAt(1.0));
	    VMOVE(endpt, p);

	    int min_linear_seg_count = crv->Degree() + 1;
	    double max_domain_step = 1.0 / min_linear_seg_count;

	    // specify first tentative segment t1 to t2
	    double t2 = max_domain_step;
	    double t1 = 0.0;
	    p = crv->PointAt(dom.ParameterAt(t1));
	    VMOVE(pt1, p);
	    RT_ADD_VLIST(info->vhead, pt1, BN_VLIST_LINE_MOVE);

	    // add segments until the minimum segment count is
	    // achieved and the distance between the end of the last
	    // segment and the endpoint is within point spacing
	    for (int nsegs = 0; (nsegs < min_linear_seg_count) ||
		(DIST_PT_PT(pt1, endpt) > info->point_spacing); ++nsegs)
	    {
		p = crv->PointAt(dom.ParameterAt(t2));
		VMOVE(pt2, p);

		// bring t2 increasingly closer to t1 until target
		// point spacing is achieved
		double step = t2 - t1;
		while (DIST_PT_PT(pt1, pt2) > info->point_spacing) {
		    step /= 2.0;
		    t2 = t1 + step;
		    p = crv->PointAt(dom.ParameterAt(t2));
		    VMOVE(pt2, p);
		}
		RT_ADD_VLIST(info->vhead, pt2, BN_VLIST_LINE_DRAW);

		// advance to next segment
		t1 = t2;
		VMOVE(pt1, pt2);

		t2 += max_domain_step;
		if (t2 > 1.0) {
		    t2 = 1.0;
		}
	    }
	    RT_ADD_VLIST(info->vhead, endpt, BN_VLIST_LINE_DRAW);
	}
    }

    return 0;
}


/**
 * There are several ways to visualize NURBS surfaces, depending on
 * the purpose.  For "normal" wireframe viewing, the ideal approach is
 * to do a tessellation of the NURBS surface and show that wireframe.
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
rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *tol, const struct rt_view_info *UNUSED(info))
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

	if (surf->IsClosed(0) || surf->IsClosed(1)) {
	    ON_SumSurface *sumsurf = const_cast<ON_SumSurface *> (ON_SumSurface::Cast(surf));
	    if (sumsurf != NULL) {
		SurfaceTree* st = new SurfaceTree(&face, true, 2);

		plot_face_from_surface_tree(vhead, st, isocurveres, gridres);

		delete st;
	    } else {
		ON_RevSurface *revsurf = const_cast<ON_RevSurface *> (ON_RevSurface::Cast(surf));

		if (revsurf != NULL) {
		    SurfaceTree* st = new SurfaceTree(&face, true, 0);

		    plot_face_from_surface_tree(vhead, st, isocurveres, gridres);

		    delete st;
		}
	    }
	}
    }

    {

	point_t pt1 = VINIT_ZERO;
	point_t pt2 = VINIT_ZERO;

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

int rt_brep_plot_poly(struct bu_list *vhead, const struct db_full_path *pathp, struct rt_db_internal *ip,
		const struct rt_tess_tol *ttol, const struct bn_tol *tol,
		const struct rt_view_info *info) {
	TRACE1("rt_brep_plot");
	struct rt_brep_internal* bi;
	const char *solid_name =  DB_FULL_PATH_CUR_DIR(pathp)->d_namep;
	ON_wString wstr;
	ON_TextLog tl(wstr);

	BU_CK_LIST_HEAD(vhead);
	RT_CK_DB_INTERNAL(ip);
	bi = (struct rt_brep_internal*) ip->idb_ptr;
	RT_BREP_CK_MAGIC(bi);

	ON_Brep* brep = bi->brep;
	if (brep == NULL || !brep->IsValid(&tl)) {
	    if (wstr.Length() > 0) {
		ON_String onstr = ON_String(wstr);
		const char *isvalidinfo = onstr.Array();
		bu_log("brep (%s) is NOT valid: %s\n",solid_name,isvalidinfo);
	    } else {
		bu_log("brep (%s) is NOT valid.\n",solid_name);
	    }
	    //return 0; let's just try it for now, need to improve the not valid checks
	}

#ifndef TESTIT
#ifndef WATER_TIGHT
#ifdef DRAW_FACE
	fastf_t  max_dist = 0;
#endif
	for (int index = 0; index < brep->m_F.Count(); index++) {
		ON_BrepFace *face = brep->Face(index);
		const ON_Surface *s = face->SurfaceOf();
		double surface_width,surface_height;
		if (s->GetSurfaceSize(&surface_width,&surface_height)) {
		    // reparameterization of the face's surface and transforms the "u"
		    // and "v" coordinates of all the face's parameter space trimming
		    // curves to minimize distortion in the map from parameter space to 3d..
		    face->SetDomain(0, 0.0, surface_width);
		    face->SetDomain(1, 0.0, surface_height);
#ifdef DRAW_FACE
		    max_dist = sqrt(surface_width*surface_width + surface_height*surface_height) / 10.0;
#endif
		}
	}
#ifdef DRAW_FACE
	for (int index = 0; index < brep->m_E.Count(); index++) {
		ON_BrepEdge& edge = brep->m_E[index];
		if (edge.m_edge_user.p == NULL) {
		    std::map<double,ON_3dPoint *> *points = getEdgePoints(edge, max_dist, ttol, tol, info);
		}
	}
#endif
#endif
	bool watertight = true;
	int plottype = 0;
	int numpoints = -1;
	for (int index = 0; index < brep->m_F.Count(); index++) {
		ON_BrepFace& face = brep->m_F[index];

#ifdef DRAW_FACE
		draw_face_CDT(vhead, face, ttol, tol, info, watertight, plottype, numpoints);
#else
		poly2tri_CDT(vhead, face, ttol, tol, info, watertight, plottype, numpoints);
#endif
	}
#else
	for (int index = 0; index < brep->m_F.Count(); index++) {
		ON_BrepFace& face = brep->m_F[index];
		SurfaceTree* st = new SurfaceTree(&face, true, 10);

		plot_poly_from_surface_tree(vhead, st, face.m_bRev);

		delete st;
	}
#endif
	for (int index = 0; index < brep->m_E.Count(); index++) {
		ON_BrepEdge& edge = brep->m_E[index];
		if (edge.m_edge_user.p != NULL) {
		    std::map<double,ON_3dPoint *> *points = (std::map<double,ON_3dPoint *> *)edge.m_edge_user.p;
		    std::map<double,ON_3dPoint *>::iterator i;
		    for(i=points->begin(); i != points->end(); i++) {
			ON_3dPoint *p = (*i).second;
			delete p;
		    }
		    points->clear();
		    delete points;
		    edge.m_edge_user.p = NULL;
		}
	}

	return 0;
}

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
    ip->idb_meth = &OBJ[ID_BREP];
    BU_ALLOC(ip->idb_ptr, struct rt_brep_internal);

    bi = (struct rt_brep_internal*)ip->idb_ptr;
    bi->magic = RT_BREP_INTERNAL_MAGIC;

    RT_MemoryArchive archive(ep->ext_buf, ep->ext_nbytes);
    ONX_Model model;
    ON_TextLog dump(stderr);
    //archive.Dump3dmChunk(dump);
    model.Read(archive, &dump);

    //if (model.IsValid(&dump)) {
	ONX_Model_Object mo = model.m_object_table[0];
	// XXX does openNURBS force us to copy? it seems the answer is
	// YES due to the const-ness
	bi->brep = ON_Brep::New(*ON_Brep::Cast(mo.m_object));
	if (mat) {
	    ON_Xform xform(mat);

	    if (!xform.IsIdentity()) {
		bi->brep->Transform(xform);
	    }
	}
	return 0;
    //} else {
    //	return -1;
    //}
}


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


int
rt_brep_tclget(Tcl_Interp *, const struct rt_db_internal *, const char *)
{
    return 0;
}


int
rt_brep_tcladjust(Tcl_Interp *, struct rt_db_internal *, int, const char **)
{
    return 0;
}


int
rt_brep_params(struct pc_pc_set *, const struct rt_db_internal *)
{
    return 0;
}


int
rt_brep_boolean(struct rt_db_internal *out, const struct rt_db_internal *ip1, const struct rt_db_internal *ip2, const char* operation)
{
    RT_CK_DB_INTERNAL(ip1);
    RT_CK_DB_INTERNAL(ip2);
    struct rt_brep_internal *bip1, *bip2;
    bip1 = (struct rt_brep_internal *)ip1->idb_ptr;
    bip2 = (struct rt_brep_internal *)ip2->idb_ptr;
    RT_BREP_CK_MAGIC(bip1);
    RT_BREP_CK_MAGIC(bip2);

    ON_Brep *brep1, *brep2, *brep_out;
    brep1 = bip1->brep;
    brep2 = bip2->brep;
    brep_out = ON_Brep::New();

    int ret;
    op_type operation_type;
    if (BU_STR_EQUAL(operation, "u"))
	operation_type = BOOLEAN_UNION;
    else if (BU_STR_EQUAL(operation, "i"))
	operation_type = BOOLEAN_INTERSECT;
    else if (BU_STR_EQUAL(operation, "-"))
	operation_type = BOOLEAN_DIFF;
    else if (BU_STR_EQUAL(operation, "x"))
	operation_type = BOOLEAN_XOR;
    else
	return -1;

    if ((ret = ON_Boolean(brep_out, brep1, brep2, operation_type)) < 0)
	return ret;

    // make the final rt_db_internal
    struct rt_brep_internal *bip_out;
    BU_ALLOC(bip_out, struct rt_brep_internal);
    bip_out->magic = RT_BREP_INTERNAL_MAGIC;
    bip_out->brep = brep_out;
    RT_DB_INTERNAL_INIT(out);
    out->idb_ptr = (genptr_t)bip_out;
    out->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    out->idb_meth = &OBJ[ID_BREP];
    out->idb_minor_type = ID_BREP;

    return 0;
}

struct brep_selectable_cv {
    int face_index;
    int i;
    int j;
    double sqdist_to_start;
    double sqdist_to_line;
};

struct brep_cv {
    int face_index;
    int i;
    int j;
};

struct brep_selection {
    std::list<brep_cv *> *control_vertexes; /**< brep_cv_list */
};

bool
cmp_cv_startdist(brep_selectable_cv *c1, brep_selectable_cv *c2)
{
    if (c1->sqdist_to_start < c2->sqdist_to_start) {
	return true;
    }

    return false;
}

static void
brep_free_selection(struct rt_selection *s)
{
    struct brep_selection *bs = (struct brep_selection *)s->obj;
    std::list<brep_cv *> *cvs = bs->control_vertexes;

    std::list<brep_cv *>::iterator cv;
    for (cv = cvs->begin(); cv != cvs->end(); ++cv) {
	delete *cv;
    }
    cvs->clear();

    delete cvs;
    delete bs;
    BU_FREE(s, struct rt_selection);
}

struct rt_selection *
new_cv_selection(brep_selectable_cv *s)
{
    // make new brep selection w/ cv list
    brep_selection *bs = new brep_selection;
    bs->control_vertexes = new std::list<brep_cv *>();

    // add referenced cv to cv list
    brep_cv *cvitem = new brep_cv;
    cvitem->face_index = s->face_index;
    cvitem->i = s->i;
    cvitem->j = s->j;
    bs->control_vertexes->push_back(cvitem);

    // wrap and return
    struct rt_selection *selection;
    BU_ALLOC(selection, struct rt_selection);
    selection->obj = (void *)bs;

    return selection;
}

struct rt_selection_set *
rt_brep_find_selections(const struct rt_db_internal *ip, const struct rt_selection_query *query)
{
    struct rt_brep_internal *bip;
    ON_Brep *brep;

    RT_CK_DB_INTERNAL(ip);
    bip = (struct rt_brep_internal *)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bip);
    brep = bip->brep;

    int num_faces = brep->m_F.Count();
    if (num_faces == 0) {
	return NULL;
    }

    // get a list of all the selectable control vertexes and
    // simultaneously find the distance from the closest vertex to the
    // query line
    std::list<brep_selectable_cv *> selectable;
    double min_distsq = INFINITY;
    for (int face_index = 0; face_index < num_faces; ++face_index) {
	ON_BrepFace *face = brep->Face(face_index);
	const ON_Surface *surface = face->SurfaceOf();
	const ON_NurbsSurface *nurbs_surface = dynamic_cast<const ON_NurbsSurface *>(surface);

	if (!nurbs_surface) {
	    continue;
	}

	// TODO: should only consider vertices in untrimmed regions
	int num_rows = nurbs_surface->m_cv_count[0];
	int num_cols = nurbs_surface->m_cv_count[1];
	for (int i = 0; i < num_rows; ++i) {
	    for (int j = 0; j < num_cols; ++j) {
		double *cv = nurbs_surface->CV(i, j);

		brep_selectable_cv *scv = new brep_selectable_cv;
		scv->face_index = face_index;
		scv->i = i;
		scv->j = j;
		scv->sqdist_to_start = DIST_PT_PT_SQ(query->start, cv);
		scv->sqdist_to_line =
		    bn_distsq_line3_pt3(query->start, query->dir, cv);

		selectable.push_back(scv);

		if (scv->sqdist_to_line < min_distsq) {
		    min_distsq = scv->sqdist_to_line;
		}
	    }
	}
    }

    // narrow down the list to just the control vertexes closest to
    // the query line, and sort them by proximity to the query start
    std::list<brep_selectable_cv *>::iterator s, tmp_s;
    for (s = selectable.begin(); s != selectable.end();) {
	tmp_s = s++;
	if ((*tmp_s)->sqdist_to_line > min_distsq) {
	    delete *tmp_s;
	    selectable.erase(tmp_s);
	}
    }
    selectable.sort(cmp_cv_startdist);

    // build and return list of selections
    struct rt_selection_set *selection_set;
    BU_ALLOC(selection_set, struct rt_selection_set);
    BU_PTBL_INIT(&selection_set->selections);

    for (s = selectable.begin(); s != selectable.end(); ++s) {
	bu_ptbl_ins(&selection_set->selections, (long *)new_cv_selection(*s));
	delete *s;
    }
    selectable.clear();
    selection_set->free_selection = brep_free_selection;

    return selection_set;
}

int
rt_brep_process_selection(
	struct rt_db_internal *ip,
	const struct rt_selection *selection,
	const struct rt_selection_operation *op)
{
    if (op->type == RT_SELECTION_NOP) {
	return 0;
    }

    if (op->type != RT_SELECTION_TRANSLATION) {
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    struct rt_brep_internal *bip = (struct rt_brep_internal *)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bip);
    ON_Brep *brep = bip->brep;

    brep_selection *bs = (brep_selection *)selection->obj;
    if (!brep || !bs || bs->control_vertexes->empty()) {
	return -1;
    }

    fastf_t dx = op->parameters.tran.dx;
    fastf_t dy = op->parameters.tran.dy;
    fastf_t dz = op->parameters.tran.dz;

    std::list<brep_cv *>::iterator cv = bs->control_vertexes->begin();
    for (; cv != bs->control_vertexes->end(); ++cv) {
	// TODO: if another face references the same surface, the
	// surface needs to be duplicated
	int face_index = (*cv)->face_index;
	if (face_index < 0 || face_index >= brep->m_F.Count()) {
	    bu_log("%d is not a valid face index\n",face_index);
	    return -1;
	}
	int surface_index = brep->m_F[face_index].m_si;
	int ret = brep_translate_scv(brep, surface_index, (*cv)->i, (*cv)->j, dx, dy, dz);
	if (ret < 0) {
	    return ret;
	}
    }

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
