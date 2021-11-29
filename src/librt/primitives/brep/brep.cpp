/*                     B R E P . C P P
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
/** @addtogroup librt */
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

#include "bu/cv.h"
#include "bu/opt.h"
#include "bu/time.h"
#include "brep.h"
#include "bn/dvec.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "../../librt_private.h"

#include "./brep_local.h"
#include "./brep_debug.h"


/* define to enable output of debug hit information */
/* #define RT_DEBUG_HITS 1 */


#ifdef __cplusplus
extern "C" {
#endif
    int rt_brep_bbox(struct rt_db_internal* ip, point_t *min, point_t *max, const struct bn_tol *tol);
    int rt_brep_prep(struct soltab *stp, struct rt_db_internal* ip, struct rt_i* rtip);
    void rt_brep_print(const struct soltab *stp);
    int rt_brep_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead);
    void rt_brep_norm(struct hit *hitp, struct soltab *stp, struct xray *rp);
    void rt_brep_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp);
    void rt_brep_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp);
    void rt_brep_free(struct soltab *stp);
    int rt_brep_adaptive_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bn_tol *tol, const struct bview *v, fastf_t s_size);
    int rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol, const struct bview *UNUSED(info));
    int rt_brep_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);
    int rt_brep_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr);
    int rt_brep_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv);
    int rt_brep_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip);
    int rt_brep_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip);
    void rt_brep_ifree(struct rt_db_internal *ip);
    int rt_brep_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local);
    int rt_brep_params(struct pc_pc_set *, const struct rt_db_internal *ip);
    RT_EXPORT extern int rt_brep_boolean(struct rt_db_internal *out, const struct rt_db_internal *ip1, const struct rt_db_internal *ip2, db_op_t operation);
    struct rt_selection_set *rt_brep_find_selections(const struct rt_db_internal *ip, const struct rt_selection_query *query);
    int rt_brep_process_selection(struct rt_db_internal *ip, struct db_i *dbip, const struct rt_selection *selection, const struct rt_selection_operation *op);
    int rt_brep_valid(struct bu_vls *log, struct rt_db_internal *ip, int flags);
    int rt_brep_plate_mode(const struct rt_db_internal *ip);
    void rt_brep_plate_mode_getvals(double *pthickness, int *nocos, const struct rt_db_internal *ip);
    int rt_brep_prep_serialize(struct soltab *stp, const struct rt_db_internal *ip, struct bu_external *external, size_t *version);
#ifdef __cplusplus
}
#endif


/********************************************************************************
 * Auxiliary functions
 ********************************************************************************/


using namespace brlcad;

int
brep_debug(const char *objname)
{
    static int debug_output = 0; // TODO - understand how we can/can't
				 // use static vars for this...

    /* If we've got debugging set in the environment, grab the
     * value
     */
    if (getenv("LIBRT_BREP_DEBUG")) {
	// TODO - cache previous value of env var in a static buffer
	// so we can skip doing anything if things haven't changed
	char *envstr = getenv("LIBRT_BREP_DEBUG");
	if (bu_opt_int(NULL, 1, (const char **)&envstr, (void *)&debug_output) == -1) {
	    /* If we don't have a number, check if the value matches
	     * the objname.  If it does, enable all possible debug
	     * output only when shooting this object.
	     *
	     * TODO - add support for specifying name and verbosity
	     * levels
	     *
	     * TODO - add support for specifying a specific ray or
	     * range of rays via the environment variable as well.
	     *
	     * TODO - can we set/clear static variables in brep_debug
	     * so we don't have to do string ops every time? */
	    if (BU_STR_EQUAL(objname, envstr))
		return INT_MAX;
	    return 0;
	}
    } else {
	debug_output = 0;
    }
    return debug_output;
}


class brep_hit
{
public:

    enum hit_type {
	CLEAN_HIT,
	CLEAN_MISS,
	NEAR_HIT,
	NEAR_MISS,
	CRACK_HIT //applied to first point of two near_miss points
		  //with same normal direction, second point removed
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
    const BBNode *sbv;
    int active;

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


#ifdef RT_DEBUG_HITS


static const char *
brep_hit_type_str(int hit)
{
    static const char *terr  = "!!ERROR!!";
    static const char *clean_hit  = "_CH_";
    static const char *clean_miss = "_MISS_";
    static const char *near_hit   = "_NH_";
    static const char *near_miss  = "_NM_";
    static const char *crack_hit  = "_CRACK_";
    if (hit == brep_hit::CLEAN_HIT)
	return clean_hit;
    if (hit == brep_hit::CLEAN_MISS)
	return clean_miss;
    if (hit == brep_hit::CRACK_HIT)
	return crack_hit;
    if (hit == brep_hit::NEAR_HIT)
	return near_hit;
    if (hit == brep_hit::NEAR_MISS)
	return near_miss;
    return terr;
}


static void
log_key(struct bu_vls *logstr)
{
    bu_vls_printf(logstr, "\nKey: _CRACK_ = CRACK_HIT; _CH_ = CLEAN_HIT; _NH_ = NEAR_HIT; _NM_ = NEAR_MISS\n");
    bu_vls_printf(logstr,  "      {...} = data for 1 hit pnt; + = ENTERING; - = LEAVING; (#) = m_face_index\n");
    bu_vls_printf(logstr,  "      [1] = face reversed (m_bRev true); [0] = face not reversed (m_bRev false)\n");
    bu_vls_printf(logstr,  "      <#> = distance from previous point to next hit point\n\n");
}


static void
log_hits(std::list<brep_hit> &hits, int UNUSED(verbosity))
{
    struct bu_vls logstr = BU_VLS_INIT_ZERO;
    log_key(&logstr);
    for (std::list<brep_hit>::iterator i = hits.begin(); i != hits.end(); ++i) {
	point_t prev = VINIT_ZERO;

	const brep_hit &out = *i;

	if (i != hits.begin()) {
	    bu_vls_printf(&logstr, "<%g>", DIST_PNT_PNT(out.point, prev));
	}
	bu_vls_printf(&logstr, "{");
	bu_vls_printf(&logstr, "%s(%d)", brep_hit_type_str((int)out.hit), out.face.m_face_index);
	if (out.direction == brep_hit::ENTERING) bu_vls_printf(&logstr, "+");
	if (out.direction == brep_hit::LEAVING) bu_vls_printf(&logstr, "-");
	bu_vls_printf(&logstr, "[%d]", out.sbv->get_face().m_bRev);
	bu_vls_printf(&logstr, "}");
	VMOVE(prev, out.point);
    }
    bu_log("%s\n", bu_vls_addr(&logstr));
    bu_vls_free(&logstr);
}


static void
log_subset(std::vector<brep_hit*> &hits, size_t min, size_t max, brep_hit *pprev)
{
    struct bu_vls logstr = BU_VLS_INIT_ZERO;
    brep_hit *prev = pprev;
    for (size_t i = min; i < max; i++) {
	if (!hits[i]->active) continue;
	if (prev) {
	    bu_vls_printf(&logstr, "<%g>", DIST_PNT_PNT(hits[i]->point, prev->point));
	}
	bu_vls_printf(&logstr, "{");
	bu_vls_printf(&logstr, "%s(%d)", brep_hit_type_str((int)hits[i]->hit), hits[i]->face.m_face_index);
	if (hits[i]->direction == brep_hit::ENTERING) bu_vls_printf(&logstr, "+");
	if (hits[i]->direction == brep_hit::LEAVING) bu_vls_printf(&logstr, "-");
	bu_vls_printf(&logstr, "[%d]", hits[i]->sbv->get_face().m_bRev);
	bu_vls_printf(&logstr, "}");
	prev = hits[i];
    }
    if (bu_vls_strlen(&logstr) > 0) {
	bu_log("%s\n", bu_vls_addr(&logstr));
    }
    bu_vls_free(&logstr);
}


#endif

static ON_Ray
toXRay(const struct xray* rp)
{
    ON_3dPoint pt(rp->r_pt);
    ON_3dVector dir(rp->r_dir);
    return ON_Ray(pt, dir);
}


//--------------------------------------------------------------------------------
// specific
static struct brep_specific*
brep_specific_new()
{
    return (struct brep_specific*)bu_calloc(1, sizeof(struct brep_specific), "brep_specific_new");
}


static void
brep_specific_delete(struct brep_specific* bs)
{
    if (bs != NULL) {
	delete bs->brep;
	delete bs->bvh;
	bu_free(bs, "brep_specific_delete");
    }
}


//--------------------------------------------------------------------------------
// prep

struct brep_build_bvh_parallel {
    struct brep_specific *bs;
    SurfaceTree**faces;
};


static void
brep_build_bvh_surface_tree(int cpu, void *data)
{
    struct brep_build_bvh_parallel *bbbp = (struct brep_build_bvh_parallel *)data;
    int index;
    const ON_BrepFaceArray& faces = bbbp->bs->brep->m_F;
    size_t faceCount = faces.Count();

    do {
	index = -1;

	/* figure out which face to work on next */
	bu_semaphore_acquire(BU_SEM_GENERAL);
	for (size_t i = 0; i < faceCount; i++) {
	    if (bbbp->faces[i] == NULL) {
		index = i;
		bbbp->faces[i] = (SurfaceTree*)(intptr_t)(cpu+1); /* claim this one */
		break;
	    }
	}
	bu_semaphore_release(BU_SEM_GENERAL);

	if (index != -1) {
	    /* bu_log("thread %d: preparing face %d of %d\n", cpu, index+1, faceCount); */
	    SurfaceTree* st = new SurfaceTree(&faces[index], true, 8);
	    bbbp->faces[index] = st;
	}

	/* iterate until there is no more work left */
    } while (index != -1);
}


static int
brep_build_bvh(struct brep_specific* bs)
{
    // First, run the openNURBS validity check on the brep in question
    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    //int64_t start;

    if (brep == NULL) {
	bu_log("NULL Brep");
	return -1;
    }

    /* Initialize the top level Bounding Box node for the entire
     * surface tree.  The purpose of this node is to provide a parent
     * node for the trees to be built on each BREP component surface.
     * This takes no time.
     */
    bs->bvh = new BBNode(brep->BoundingBox());

    ON_BrepFaceArray& faces = brep->m_F;
    size_t faceCount = faces.Count();
    if (faceCount == 0) {
	bu_log("Empty Brep");
	return -1;
    }

    struct brep_build_bvh_parallel bbbp;
    bbbp.bs = bs;
    bbbp.faces = (SurfaceTree**)bu_calloc(faceCount, sizeof(SurfaceTree*), "alloc face array");

    /* For each face in the brep, build its surface tree and add the
     * root node of that tree as a child of the bvh master node
     * defined above.  We do this in parallel in order to divy up work
     * for objects comprised of many faces.
     *
     * A possible future refinement of this approach would be to build
     * a tree structure on top of the collection of surface trees
     * based on their 3D bounding volumes, as opposed to the current
     * approach of simply having all surfaces be child nodes of the
     * master node.  This would allow a ray intersection to avoid
     * checking every surface tree bounding box, but should probably
     * be undertaken only if this step proves to be a bottleneck for
     * raytracing.
     */

    //start = bu_gettime();
    bu_parallel(brep_build_bvh_surface_tree, 0, &bbbp);

    for (int i = 0; (size_t)i < faceCount; i++) {
	ON_BrepFace& face = faces[i];
	face.m_face_user.p = bbbp.faces[i];
	bs->bvh->addChild(bbbp.faces[i]->getRootNode());
    }
    //bu_log("!!! PREP FACES: %.2f sec\n", (bu_gettime() - start) / 1000000.0);

    // note: the SurfaceTrees in bbbp.faces are never destroyed/freed
    bu_free(bbbp.faces, "free face array");

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
rt_brep_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol))
{
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
    int plate_mode;
    //int64_t start;

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

    bs = (struct brep_specific*)stp->st_specific;
    if (bs == NULL) {
	bs = brep_specific_new();
	bs->brep = bi->brep;
	plate_mode = rt_brep_plate_mode(ip);
	bi->brep = NULL;
	stp->st_specific = (void *)bs;
    } else {
	bi->brep = bs->brep;
	plate_mode = rt_brep_plate_mode(ip);
	bi->brep = NULL;
    }

    if (plate_mode) {
	bs->plate_mode = 1;
	rt_brep_plate_mode_getvals(&bs->plate_mode_thickness, &bs->plate_mode_nocos, ip);
    }

    ON_TextLog err(stderr);
    if (!bs->brep->IsValid(&err)) {
	//bu_log("brep is NOT valid\n");
    } else {
	bs->is_solid = bs->brep->IsSolid();
	//bu_log("brep %s solid\n", (bs->is_solid) ? "is" : "is NOT");
    }

    //start = bu_gettime();
    /* do the majority of real work here */
    if (brep_build_bvh(bs) < 0) {
	return -1;
    }
    //bu_log("!!! BUILD BVH: %.2f sec\n", (bu_gettime() - start) / 1000000.0);

    /* Once a proper SurfaceTree is built, finalize the bounding
     * volumes.  This takes no time. */
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

typedef enum {
    BREP_INTERSECT_RIGHT_OF_EDGE = -5,
    BREP_INTERSECT_MISSED_EDGE = -4,
    BREP_INTERSECT_ROOT_ITERATION_LIMIT = -3,
    BREP_INTERSECT_ROOT_DIVERGED = -2,
    BREP_INTERSECT_OOB = -1,
    BREP_INTERSECT_TRIMMED = 0,
    BREP_INTERSECT_FOUND = 1
} brep_intersect_reason_t;


static void
utah_F(const ON_3dPoint &S, const ON_3dVector &p1, const double p1d, const ON_3dVector &p2, const double p2d, double &f1, double &f2)
{
    f1 = (S * p1) + p1d;
    f2 = (S * p2) + p2d;
}


static void
utah_Fu(const ON_3dVector &Su, const ON_3dVector &p1, const ON_3dVector &p2, double &d0, double &d1)
{
    d0 = Su * p1;
    d1 = Su * p2;
}


static void
utah_Fv(const ON_3dVector &Sv, const ON_3dVector &p1, const ON_3dVector &p2, double &d0, double &d1)
{
    d0 = Sv * p1;
    d1 = Sv * p2;
}


static double
utah_calc_t(const ON_Ray &r, const ON_3dPoint &S)
{
    ON_3dVector d(r.m_dir);
    ON_3dVector oS(S - r.m_origin);

    return (d * oS) / (d * d);
}


static void
utah_pushBack(const BBNode* sbv, ON_2dPoint &uv)
{
    double t0, t1;
    int i = sbv->m_u.m_t[0] < sbv->m_u.m_t[1] ? 0 : 1;

    t0 = sbv->m_u.m_t[i];
    t1 = sbv->m_u.m_t[1 - i];
    if (uv.x < t0) {
	uv.x = t0;
    } else if (uv.x > t1) {
	uv.x = t1;
    }
    i = sbv->m_v.m_t[0] < sbv->m_v.m_t[1] ? 0 : 1;
    t0 = sbv->m_v.m_t[i];
    t1 = sbv->m_v.m_t[1 - i];
    if (uv.y < t0) {
	uv.y = t0;
    } else if (uv.y > t1) {
	uv.y = t1;
    }
}


static int
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
    //surf->Ev2Der(uv.x, uv.y, S, Su, Sv, Suu, Suv, Svv);

    utah_F(S, p1, p1d, p2, p2d, f, g);
    rootdist = fabs(f) + fabs(g);

    for (i = 0; i < BREP_MAX_ITERATIONS; i++) {
	utah_Fu(Su, p1, p2, j11, j21);
	utah_Fv(Sv, p1, p2, j12, j22);

	J = (j11 * j22 - j12 * j21);

	if (NEAR_ZERO(J, BREP_INTERSECTION_ROOT_EPSILON)) {
	    // perform jittered perturbation in parametric domain....
	    // FIXME: drand48 call should be replaced by a faster
	    // libbn random mechanism - common.h's definition of
	    // drand48 on Windows showed hot in profiling.
	    uv.x = uv.x + .1 * drand48() * (uv0.x - uv.x);
	    uv.y = uv.y + .1 * drand48() * (uv0.y - uv.y);
	    continue;
	}

	invdetJ = 1.0 / J;

	if ((iu != -1) && (iv != -1)) {
	    du = -invdetJ * (j22 * f - j12 * g);
	    dv = -invdetJ * (j11 * g - j21 * f);

	    if (i == 0) {
		if (((iu == 0) && (du < 0.0)) || ((iu == 1) && (du > 0.0)))
		    return intersects; //head out of U bounds
		if (((iv == 0) && (dv < 0.0)) || ((iv == 1) && (dv > 0.0)))
		    return intersects; //head out of V bounds
	    }
	}

	du = invdetJ * (j22 * f - j12 * g);
	dv = invdetJ * (j11 * g - j21 * f);


	if (i == 0) {
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

	/* iterate at most 3 times just because. might be worth trying
	 * additional depths or refining adaptively.
	 */
	while ((halve_count++ < 3) && (oldrootdist < rootdist)) {
	    // divide current UV step
	    uv.x = (puv.x + uv.x) / 2.0;
	    uv.y = (puv.y + uv.y) / 2.0;

	    utah_pushBack(sbv, uv);

	    surf->Ev1Der(uv.x, uv.y, S, Su, Sv);
	    utah_F(S, p1, p1d, p2, p2d, f, g);
	    rootdist = fabs(f) + fabs(g);
	}

	if (oldrootdist <= rootdist) {

	    /* if we're not getting any better after 3 tries, give up
	     * and return what was found.  no particular reason for 3.
	     */
	    if (errantcount > 3) {
		return intersects;
	    } else {
		errantcount++;
	    }
	}

/* if we get this close to a root, good enough.  No particular
 * significance other than it's below our typical distance tol and
 * above double precision tol.
 */
#define ROOT_TOL 1.E-7

	if (rootdist < ROOT_TOL) {
	    int ulow = (sbv->m_u.m_t[0] <= sbv->m_u.m_t[1]) ? 0 : 1;
	    int vlow = (sbv->m_v.m_t[0] <= sbv->m_v.m_t[1]) ? 0 : 1;
	    if ((sbv->m_u.m_t[ulow] - VUNITIZE_TOL < uv.x && uv.x < sbv->m_u.m_t[1 - ulow] + VUNITIZE_TOL) &&
		(sbv->m_v.m_t[vlow] - VUNITIZE_TOL < uv.y && uv.y < sbv->m_v.m_t[1 - vlow] + VUNITIZE_TOL)) {
		bool new_point = true;
		for (int j = 0; j < count; j++) {
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


static int
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


static int
utah_brep_intersect(const BBNode* sbv, const ON_BrepFace* face, const ON_Surface* surf, pt2d_t& uv, const ON_Ray& ray, std::list<brep_hit>& hits)
{
#define MAX_BREP_SUBDIVISION_INTERSECTS 5
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
	    const BRNode* trimBR = NULL;
	    int trim_status = sbv->isTrimmed(ouv[i], &trimBR, closesttrim, BREP_EDGE_MISS_TOLERANCE);
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

static int
sign(double val)
{
    return (val >= 0.0) ? 1 : -1;
}


static bool
containsNearMiss(const std::list<brep_hit> *hits)
{
    for (std::list<brep_hit>::const_iterator i = hits->begin(); i != hits->end(); ++i) {
	const brep_hit&out = *i;
	if (out.hit == brep_hit::NEAR_MISS) {
	    return true;
	}
    }
    return false;
}


static bool
containsNearHit(const std::list<brep_hit> *hits)
{
    for (std::list<brep_hit>::const_iterator i = hits->begin(); i != hits->end(); ++i) {
	const brep_hit&out = *i;
	if (out.hit == brep_hit::NEAR_HIT) {
	    return true;
	}
    }
    return false;
}


static double
brep_platemode_thickness(const struct xray& ray, const brep_hit& hit, const struct brep_specific& bs)
{
    double los = bs.plate_mode_thickness;
    if (bs.plate_mode_nocos) {
	return los;
    }

    double dot = fabs(VDOT(hit.normal, ray.r_dir));
    los = los / dot;

    point_t hp;
    VJOIN1(hp, ray.r_pt, hit.dist + los, ray.r_dir);
    ON_3dPoint los_pnt(V3ARGS(hp));

    /* FIXME: default behavior matches what BoT does, but results in
     * undesirable los values on high obliquity angles.
     */

/*#define WORK_IN_PROGRESS 1 */
#ifdef WORK_IN_PROGRESS

    /* try to make sure we don't extend more than plate-mode thickness
     * beyond the surface by calculating the proposed exit point's
     * distance to the surface.
     */
    const ON_Surface* surf = hit.face.SurfaceOf();
    const ON_BrepFace& face = hit.face;

#if 0
    SurfaceTree* tree = NULL;
    ON_2dPoint uvpt;
    get_closest_point(uvpt, face, los_pnt, tree);
    ON_3dPoint p = surf->PointAt(uvpt[0], uvpt[1]);
    double dist_to_surf = p.DistanceTo(los_pnt);
#else
#endif

    const int MAX_ITERATIONS = 100;
    const double MIN_STEPSIZE = bs.plate_mode_thickness * 0.1;

    int iterations = 0;
    while (!NEAR_EQUAL(dist_to_surf, bs.plate_mode_thickness, MIN_STEPSIZE) && iterations++ < MAX_ITERATIONS) {

	if (dist_to_surf > bs.plate_mode_thickness)
	    los -= dist_to_surf / 2.0;
	else if (dist_to_surf < bs.plate_mode_thickness)
	    los += dist_to_surf / 3.0;

#if 0
	/* calculate a new exit point distance to surface */
	VJOIN1(hp, ray.r_pt, hit.dist + los, ray.r_dir);
	los_pnt = ON_3dPoint(V3ARGS(hp));
	get_closest_point(uvpt, face, los_pnt, tree);
	p = surf->PointAt(uvpt[0], uvpt[1]);
	dist_to_surf = p.DistanceTo(los_pnt);
#else
#endif
    }

#endif


    return los;
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
rt_brep_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
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
    std::list<const BBNode*> inters;
    ON_Ray r = toXRay(rp);
    bs->bvh->intersectsHierarchy(r, inters);
    if (inters.empty())
	return 0; // MISS

    // find all the hits (XXX very inefficient right now!)
    std::list<brep_hit> hits;
    MissList misses;
    for (std::list<const BBNode*>::const_iterator i = inters.begin(); i != inters.end(); i++) {
	const BBNode* sbv = (*i);
	const ON_BrepFace* f = &sbv->get_face();
	const ON_Surface* surf = f->SurfaceOf();
	pt2d_t uv = {sbv->m_u.Mid(), sbv->m_v.Mid()};
	utah_brep_intersect(sbv, f, surf, uv, r, hits);
    }

    // sort the hits
    hits.sort();

#ifdef RT_DEBUG_HITS
    std::list<brep_hit> orig = hits;
#endif

    ////////////////////////
    if ((hits.size() > 1) && containsNearMiss(&hits)) { //&& ((hits.size() % 2) != 0)) {

	std::list<brep_hit>::iterator prev;
	std::list<brep_hit>::const_iterator next;
	std::list<brep_hit>::iterator curr = hits.begin();

	while (curr != hits.end()) {
	    const brep_hit &curr_hit = *curr;
	    if (curr_hit.hit == brep_hit::NEAR_MISS) {
		if (curr != hits.begin()) {
		    prev = curr;
		    prev--;
		    const brep_hit &prev_hit = (*prev);
		    if ((prev_hit.hit != brep_hit::NEAR_MISS) && (prev_hit.direction == curr_hit.direction)) {
			//remove current miss
			curr = hits.erase(curr);
			curr = hits.begin(); //rewind and start again
			continue;
		    }
		}
		next = curr;
		next++;
		if (next != hits.end()) {
		    const brep_hit &next_hit = (*next);
		    if ((next_hit.hit != brep_hit::NEAR_MISS) && (next_hit.direction == curr_hit.direction)) {
			//remove current miss
			curr = hits.erase(curr);
			curr = hits.begin(); //rewind and start again
			continue;
		    }
		}
	    }
	    curr++;
	}

	// check for crack hits between adjacent faces
	curr = hits.begin();
	while (curr != hits.end()) {
	    const brep_hit &curr_hit = *curr;
	    if (curr != hits.begin()) {
		if (curr_hit.hit == brep_hit::NEAR_MISS) {
		    prev = curr;
		    prev--;
		    brep_hit &prev_hit = (*prev);
		    if (prev_hit.hit == brep_hit::NEAR_MISS) { // two near misses in a row
			if (prev_hit.m_adj_face_index == curr_hit.face.m_face_index) {
			    if (prev_hit.direction == curr_hit.direction) {
				//remove current miss
				prev_hit.hit = brep_hit::CRACK_HIT;
				curr = hits.erase(curr);
				continue;
			    } else {
				//remove both edge near misses
				(void)hits.erase(prev);
				curr = hits.erase(curr);
				continue;
			    }
			} else {
			    // not adjacent faces so remove first miss
			    (void)hits.erase(prev);
			}
		    }
		} else {
		    prev = curr;
		    prev--;
		    brep_hit &prev_hit = (*prev);
		    if ((curr_hit.hit == brep_hit::CLEAN_HIT || curr_hit.hit == brep_hit::NEAR_HIT) && prev_hit.hit == brep_hit::NEAR_MISS) {
			if (curr_hit.direction == brep_hit::ENTERING) {
			    (void)hits.erase(prev);
			} else {
			    prev_hit.hit = brep_hit::CRACK_HIT;
			}
		    }
		}
	    }
	    curr++;
	}

	// check for CH double enter or double leave between adjacent
	// faces(represents overlapping faces)
	curr = hits.begin();
	while (curr != hits.end()) {
	    const brep_hit &curr_hit = *curr;
	    if (curr_hit.hit == brep_hit::CLEAN_HIT) {
		if (curr != hits.begin()) {
		    prev = curr;
		    prev--;
		    const brep_hit &prev_hit = (*prev);
		    if ((prev_hit.hit == brep_hit::CLEAN_HIT) &&
			(prev_hit.direction == curr_hit.direction) &&
			(prev_hit.face.m_face_index == curr_hit.m_adj_face_index)) {
			// if "entering" remove first hit if
			// "existing" remove second hit until we get
			// good solids with known normal directions
			// assume first hit direction is "entering"
			// todo check solid status and normals
			std::list<brep_hit>::const_iterator first = hits.begin();
			const brep_hit &first_hit = *first;
			if (first_hit.direction == curr_hit.direction) { // assume "entering"
			    curr = hits.erase(prev);
			} else { // assume "exiting"
			    curr = hits.erase(curr);
			}
			continue;
		    }
		}
	    }
	    curr++;
	}

	if (!hits.empty() && ((hits.size() % 2) != 0)) {
	    const brep_hit &curr_hit = hits.back();
	    if (curr_hit.hit == brep_hit::NEAR_MISS) {
		hits.pop_back();
	    }
	}

	if (!hits.empty() && ((hits.size() % 2) != 0)) {
	    const brep_hit &curr_hit = hits.front();
	    if (curr_hit.hit == brep_hit::NEAR_MISS) {
		hits.pop_front();
	    }
	}

    }

    ///////////// handle near hit
    if ((hits.size() > 1) && containsNearHit(&hits)) { //&& ((hits.size() % 2) != 0)) {
	std::list<brep_hit>::iterator prev;
	std::list<brep_hit>::const_iterator next;
	std::list<brep_hit>::iterator curr = hits.begin();
	while (curr != hits.end()) {
	    const brep_hit &curr_hit = *curr;
	    if (curr_hit.hit == brep_hit::NEAR_HIT) {
		if (curr != hits.begin()) {
		    prev = curr;
		    prev--;
		    const brep_hit &prev_hit = (*prev);
		    if ((prev_hit.hit != brep_hit::NEAR_HIT) && (prev_hit.direction == curr_hit.direction)) {
			//remove current miss
			curr = hits.erase(curr);
			continue;
		    }
		}
		next = curr;
		next++;
		if (next != hits.end()) {
		    const brep_hit &next_hit = (*next);
		    if ((next_hit.hit != brep_hit::NEAR_HIT) && (next_hit.direction == curr_hit.direction)) {
			//remove current miss
			curr = hits.erase(curr);
			continue;
		    }
		}
	    }
	    curr++;
	}
	curr = hits.begin();
	while (curr != hits.end()) {
	    const brep_hit &curr_hit = *curr;
	    if (curr_hit.hit == brep_hit::NEAR_HIT) {
		if (curr != hits.begin()) {
		    prev = curr;
		    prev--;
		    brep_hit &prev_hit = (*prev);
		    if ((prev_hit.hit == brep_hit::NEAR_HIT) && (prev_hit.direction == curr_hit.direction)) {
			//remove current near hit
			prev_hit.hit = brep_hit::CRACK_HIT;
			curr = hits.erase(curr);
			continue;
		    }
		}
	    }
	    curr++;
	}
    }

    if (!hits.empty()) {
	// remove grazing hits with with normal to ray dot less than
	// BREP_GRAZING_DOT_TOL (>= 89.999 degrees obliq)
	TRACE("-- Remove grazing hits --");
	int num = 0;
	for (std::list<brep_hit>::iterator i = hits.begin(); i != hits.end(); ++i) {
	    const brep_hit &curr_hit = *i;
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

    if (!hits.empty()) {
	// we should have "valid" points now, remove duplicates or
	// grazes(same point with in/out sign change)
	std::list<brep_hit>::iterator last = hits.begin();
	std::list<brep_hit>::iterator i = hits.begin();
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

    // remove multiple "INs" in a row assume last "IN" is the actual
    // entering hit, for multiple "OUTs" in a row assume first "OUT"
    // is the actual exiting hit, remove unused "INs/OUTs" from hit
    // list.

    //if (!hits.empty() && ((hits.size() % 2) != 0)) {
    if (!hits.empty()) {
	// we should have "valid" points now, remove duplicates or grazes
	std::list<brep_hit>::iterator last = hits.begin();
	std::list<brep_hit>::iterator i = hits.begin();
	++i;
	int entering = 1;
	while (i != hits.end()) {
	    double lastDot = VDOT(last->normal, rp->r_dir);
	    double iDot = VDOT(i->normal, rp->r_dir);

	    if (i == hits.begin()) {
		// take this as the entering sign for now, should be
		// checking solid for inward or outward facing normals
		// and make determination there but to much unsolid
		// geom right now.
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
	const brep_hit &first_hit = hits.front();
	const brep_hit &last_hit = hits.back();
	double firstDot = VDOT(first_hit.normal, rp->r_dir);
	double lastDot = VDOT(last_hit.normal, rp->r_dir);
	if (sign(firstDot) == sign(lastDot)) {
	    hits.pop_back();
	}
    }

    if (bs->plate_mode) {

	/* Newer plate mode enabled version of logic, causing problems
	 * with NIST3 (see regress/nurbs test)
	 */

	size_t nhits = hits.size();
	if (nhits > 0) {
	    /* PLATE MODE case */

	    /* iterate over all hit points assuming a plate-mode shell */
	    for (std::list<brep_hit>::const_iterator i = hits.begin(); i != hits.end(); ++i) {
		const brep_hit& in = *i;
		const brep_hit& out = *i;

		double los = brep_platemode_thickness(*rp, in, *bs);

		struct seg* segp;
		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;

		/* set in hit */
		segp->seg_in.hit_dist = in.dist - (los*0.5);
		// segment is centered on the hit point
		segp->seg_in.hit_surfno = in.face.m_face_index;
		VSET(segp->seg_in.hit_vpriv, in.uv[0], in.uv[1], 0.0);
		VMOVE(segp->seg_in.hit_normal, in.normal);
		VJOIN1(segp->seg_in.hit_point, rp->r_pt, segp->seg_in.hit_dist, rp->r_dir);
		segp->seg_in.hit_rayp = &ap->a_ray;

		VMOVE(segp->seg_out.hit_point, out.point);
		VMOVE(segp->seg_out.hit_normal, out.normal);
		segp->seg_out.hit_dist = out.dist;

		/* set out hit */
		segp->seg_out.hit_dist = out.dist + (los*0.5); // centered
		segp->seg_out.hit_surfno = out.face.m_face_index;
		VSET(segp->seg_out.hit_vpriv, out.uv[0], out.uv[1], 0.0);
		VREVERSE(segp->seg_out.hit_normal, out.normal);
		segp->seg_out.hit_rayp = &ap->a_ray;
		VJOIN1(segp->seg_out.hit_point, rp->r_pt, segp->seg_out.hit_dist, rp->r_dir);

		BU_LIST_INSERT(&(seghead->l), &(segp->l));
	    }

#ifdef RT_DEBUG_HITS
	    //TRACE2("screen xy: " << ap->a_x << ", " << ap->a_y);
	    bu_log("**** ERROR odd number of hits: %lu\n", static_cast<unsigned long>(hits.size()));
	    bu_log("xyz %g %g %g \n", rp->r_pt[0], rp->r_pt[1], rp->r_pt[2]);
	    bu_log("dir %g %g %g \n", rp->r_dir[0], rp->r_dir[1], rp->r_dir[2]);
	    bu_log("**** Current Hits: %lu\n", static_cast<unsigned long>(hits.size()));

	    log_hits(hits, debug_output);

	    bu_log("\n**** Orig Hits: %lu\n", static_cast<unsigned long>(orig.size()));

	    log_hits(orig, debug_output);

	    bu_log("\n**********************\n");
#endif
	}

	return nhits;

    } else {

	/* SOLID case */

	bool hit = false;
	if (hits.size() > 1) {

	    bool hit_it = hits.size() % 2 == 0;
	    if (hit_it) {
		// take each pair as a segment
		for (std::list<brep_hit>::const_iterator i = hits.begin(); i != hits.end(); ++i) {
		    const brep_hit& in = *i;
		    i++;
		    const brep_hit& out = *i;

		    struct seg* segp;
		    RT_GET_SEG(segp, ap->a_resource);
		    segp->seg_stp = stp;

		    VMOVE(segp->seg_in.hit_point, in.point);
		    VMOVE(segp->seg_in.hit_normal, in.normal);
		    segp->seg_in.hit_dist = in.dist;
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
	    }
	}

	return (hit) ? (int)hits.size() : 0; // MISS
    }

    return 0;
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_brep_norm(struct hit *UNUSED(hitp), struct soltab *UNUSED(stp), struct xray *UNUSED(rp))
{
    /* normal was computed during shot, resides in hitp->hit_normal */
    return;
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


/**
 * For a hit on the surface of an nurb, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1
 * u = azimuth
 * v = elevation
 */
void
rt_brep_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
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
rt_brep_free(struct soltab *stp)
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


/* a binary predicate for std:list implemented as a function */
static bool
near_equal(double first, double second)
{
    /* FIXME: arbitrary nearness tolerance */
    return NEAR_EQUAL(first, second, 1e-6);
}


static void
plotisoUCheckForTrim(struct bu_list *vlfree, struct bu_list *vhead, const SurfaceTree* st, fastf_t from, fastf_t to, fastf_t v)
{
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
    std::list<const BRNode*> m_trims_right;
    std::list<double> trim_hits;

    const ON_Surface *surf = st->getSurface();
    const CurveTree *ctree = st->m_ctree;
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
    std::list<const BRNode *>::const_iterator i;
    for (i = m_trims_right.begin(); i != m_trims_right.end(); i++, cnt++) {
	const BRNode* br = *i;

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

    //cnt = 1;
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

		    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
		}
	    }
	}
    }

    return;
}


static void
plotisoVCheckForTrim(struct bu_list *vlfree, struct bu_list *vhead, const SurfaceTree* st, fastf_t from, fastf_t to, fastf_t u)
{
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
    std::list<const BRNode*> m_trims_above;
    std::list<double> trim_hits;

    const ON_Surface *surf = st->getSurface();
    const CurveTree *ctree = st->m_ctree;
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
    for (std::list<const BRNode*>::const_iterator i = m_trims_above.begin(); i
	     != m_trims_above.end(); i++, cnt++) {
	const BRNode* br = *i;

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
    //cnt = 1;

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

		    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
		}
	    }
	}
    }
    return;
}


static void
plotisoU(struct bu_list *vlfree, struct bu_list *vhead, SurfaceTree* st, fastf_t from, fastf_t to, fastf_t v, int curveres)
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
	BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
    }
}


static void
plotisoV(struct bu_list *vlfree, struct bu_list *vhead, SurfaceTree* st, fastf_t from, fastf_t to, fastf_t u, int curveres)
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
	BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
    }
}


static void
plot_BBNode(struct bu_list *vlfree, struct bu_list *vhead, SurfaceTree* st, const BBNode * node, int isocurveres, int gridres)
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

	    plotisoUCheckForTrim(vlfree, vhead, st, from, to, v); //bottom
	    v = node->m_v[1];
	    plotisoUCheckForTrim(vlfree, vhead, st, from, to, v); //top
	    from = node->m_v[0];
	    to = node->m_v[1];
	    plotisoVCheckForTrim(vlfree, vhead, st, from, to, u); //left
	    u = node->m_u[1];
	    plotisoVCheckForTrim(vlfree, vhead, st, from, to, u); //right
	    return;
	} else { // fully untrimmed just draw bottom and right edges
	    fastf_t u = node->m_u[0];
	    fastf_t v = node->m_v[0];
	    fastf_t from = u;
	    fastf_t to = node->m_u[1];
	    plotisoU(vlfree, vhead, st, from, to, v, isocurveres); //bottom

	    from = v;
	    to = node->m_v[1];
	    plotisoV(vlfree, vhead, st, from, to, u, isocurveres); //right
	    return;
	}
    } else {
	for (std::vector<BBNode*>::const_iterator childnode = node->get_children().begin(); childnode != node->get_children().end(); ++childnode) {
	    plot_BBNode(vlfree, vhead, st, *childnode, isocurveres, gridres);
	}
    }
}


static void
plot_face_from_surface_tree(struct bu_list *vlfree, struct bu_list *vhead, SurfaceTree* st, int isocurveres, int gridres)
{
    const BBNode *root = st->getRootNode();
    plot_BBNode(vlfree, vhead, st, root, isocurveres, gridres);
}

static fastf_t
brep_avg_curve_bbox_diagonal_len(ON_Brep *brep)
{
    fastf_t avg_curve_len = 0.0;
    int i, num_curves = 0;

    for (i = 0; i < brep->m_E.Count(); ++i) {
	ON_BrepEdge &e = brep->m_E[i];
	const ON_Curve *crv = e.EdgeCurveOf();

	if (!crv->IsLinear()) {
	    ++num_curves;

	    ON_BoundingBox bbox;
	    if (crv->GetTightBoundingBox(bbox)) {
		avg_curve_len += bbox.Diagonal().Length();
	    } else {
		ON_3dVector linear_approx =
		    crv->PointAtEnd() - crv->PointAtStart();
		avg_curve_len += linear_approx.Length();
	    }
	}
    }
    avg_curve_len /= num_curves;

    return avg_curve_len;
}

static fastf_t
brep_est_avg_curve_len(struct rt_brep_internal *bi)
{
    return brep_avg_curve_bbox_diagonal_len(bi->brep) * 2.0;
}

int
rt_brep_adaptive_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol), const struct bview *v, fastf_t UNUSED(s_size))
{
    TRACE1("rt_brep_adaptive_plot");
    struct rt_brep_internal* bi;
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    fastf_t point_spacing = solid_point_spacing(v, brep_est_avg_curve_len(bi) * M_2_PI * 2.0);

    ON_Brep* brep = bi->brep;
    int gridres = 10;
    int isocurveres = 100;

    for (int index = 0; index < brep->m_F.Count(); index++) {
	const ON_BrepFace& face = brep->m_F[index];
	const ON_Surface *surf = face.SurfaceOf();

	if (surf->IsClosed(0) || surf->IsClosed(1)) {
	    ON_SumSurface *sumsurf = const_cast<ON_SumSurface *>(ON_SumSurface::Cast(surf));
	    if (sumsurf != NULL) {
		SurfaceTree st(&face, true, 2);
		plot_face_from_surface_tree(vlfree, vhead, &st, isocurveres, gridres);
	    } else {
		ON_RevSurface *revsurf = const_cast<ON_RevSurface *>(ON_RevSurface::Cast(surf));

		if (revsurf != NULL) {
		    SurfaceTree st(&face, true, 0);
		    plot_face_from_surface_tree(vlfree, vhead, &st, isocurveres, gridres);
		}
	    }
	}
    }

    for (int index = 0; index < bi->brep->m_E.Count(); index++) {
	const ON_BrepEdge& e = brep->m_E[index];
	const ON_Curve* crv = e.EdgeCurveOf();

	if (crv->IsLinear()) {
	    const ON_BrepVertex& v1 = brep->m_V[e.m_vi[0]];
	    const ON_BrepVertex& v2 = brep->m_V[e.m_vi[1]];
	    VMOVE(pt1, v1.Point());
	    VMOVE(pt2, v2.Point());
	    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
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
	    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);

	    // add segments until the minimum segment count is
	    // achieved and the distance between the end of the last
	    // segment and the endpoint is within point spacing
	    for (int nsegs = 0; (nsegs < min_linear_seg_count) ||
		     (DIST_PNT_PNT(pt1, endpt) > point_spacing); ++nsegs) {
		p = crv->PointAt(dom.ParameterAt(t2));
		VMOVE(pt2, p);

		// bring t2 increasingly closer to t1 until target
		// point spacing is achieved
		double step = t2 - t1;
		while (DIST_PNT_PNT(pt1, pt2) > point_spacing) {
		    step /= 2.0;
		    t2 = t1 + step;
		    p = crv->PointAt(dom.ParameterAt(t2));
		    VMOVE(pt2, p);
		}
		BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);

		// advance to next segment
		t1 = t2;
		VMOVE(pt1, pt2);

		t2 += max_domain_step;
		if (t2 > 1.0) {
		    t2 = 1.0;
		}
	    }
	    BV_ADD_VLIST(vlfree, vhead, endpt, BV_VLIST_LINE_DRAW);
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
rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *tol, const struct bview *UNUSED(info))
{
    TRACE1("rt_brep_plot");
    struct rt_brep_internal* bi;
    int i;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    ON_Brep* brep = bi->brep;
    int gridres = 10;
    int isocurveres = 100;

    for (int index = 0; index < brep->m_F.Count(); index++) {
	const ON_BrepFace& face = brep->m_F[index];
	const ON_Surface *surf = face.SurfaceOf();

	if (surf != NULL) {
	    if (surf->IsClosed(0) || surf->IsClosed(1)) {
		ON_SumSurface *sumsurf = const_cast<ON_SumSurface *>(ON_SumSurface::Cast(surf));
		if (sumsurf != NULL) {
		    SurfaceTree st(&face, true, 2);
		    plot_face_from_surface_tree(vlfree, vhead, &st, isocurveres, gridres);
		} else {
		    ON_RevSurface *revsurf = const_cast<ON_RevSurface *>(ON_RevSurface::Cast(surf));

		    if (revsurf != NULL) {
			SurfaceTree st(&face, true, 0);
			plot_face_from_surface_tree(vlfree, vhead, &st, isocurveres, gridres);
		    }
		}
	    }
	} else {
	    bu_log("Surface index %d not defined.\n", index);
	}
    }

    {
	for (i = 0; i < bi->brep->m_E.Count(); i++) {
	    int j = 0;
	    int pnt_cnt = 0;
	    ON_3dPoint p;
	    point_t pt1 = VINIT_ZERO;
	    ON_Polyline poly;
	    const ON_BrepEdge& e = brep->m_E[i];
	    const ON_Curve* crv = e.EdgeCurveOf();
	    pnt_cnt = ON_Curve_PolyLine_Approx(&poly, crv, tol->dist);
	    if (pnt_cnt > 1) {
		p = poly[0];
		VMOVE(pt1, p);
		BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		for (j = 1; j < pnt_cnt; j++) {
		    p = poly[j];
		    VMOVE(pt1, p);
		    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_DRAW);
		}
	    }
	}
    }

    return 0;
}


int
rt_brep_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    int ret = 0;
    struct rt_brep_internal *bi = NULL;

    if (!r || !m || !ip || !ttol || !tol)
	return -1;

    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal *)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    int fcnt=0, fncnt=0, ncnt=0, vcnt=0;
    int *faces = NULL;
    fastf_t *vertices = NULL;
    int *face_normals = NULL;
    fastf_t *normals = NULL;

    struct bg_tess_tol cdttol = BG_TESS_TOL_INIT_ZERO;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;
    ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)bi->brep, NULL);
    ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);
    if (ON_Brep_CDT_Tessellate(s_cdt, 0, NULL)) {
	// Couldn't get a solid mesh, we're done
	ON_Brep_CDT_Destroy(s_cdt);
	return -1;
    }
    ON_Brep_CDT_Mesh(&faces, &fcnt, &vertices, &vcnt, &face_normals, &fncnt, &normals, &ncnt, s_cdt, 0, NULL);
    ON_Brep_CDT_Destroy(s_cdt);

    struct rt_bot_internal *bot;
    BU_GET(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = 0;
    bot->num_vertices = vcnt;
    bot->num_faces = fcnt;
    bot->vertices = vertices;
    bot->faces = faces;
    bot->thickness = NULL;
    bot->face_mode = (struct bu_bitv *)NULL;
    bot->num_normals = ncnt;
    bot->num_face_normals = fncnt;
    bot->normals = normals;
    bot->face_normals = face_normals;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_ptr = (void *)bot;
    intern.idb_meth = &OBJ[intern.idb_type];

    ret = rt_bot_tess(r, m, &intern, ttol, tol);

    BU_PUT(bot, struct rt_bot_internal);
    bu_free(faces, "faces");
    bu_free(vertices, "vertices");
    bu_free(face_normals, "face_normals");
    bu_free(normals, "normals");

    return ret;
}


/**
 * XXX In order to facilitate exporting the ON_Brep object without a
 * whole lot of effort, we're going to (for now) extend the
 * ON_BinaryArchive to support an "in-memory" representation of a
 * binary archive. Currently, the openNURBS library only supports
 * file-based archiving operations.
 */
class RT_MemoryArchive : public ON_BinaryArchive
{
public:
    RT_MemoryArchive();
    RT_MemoryArchive(const void *memory, size_t len);
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
    : ON_BinaryArchive(ON::archive_mode::write3dm), pos(0), m_buffer()
{
}


RT_MemoryArchive::RT_MemoryArchive(const void *memory, size_t len)
    : ON_BinaryArchive(ON::archive_mode::read3dm), pos(0),
      m_buffer((char *)memory, (char *)memory + len)
{
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
    if (pos + seek_to > m_buffer.size())
	return false;
    pos += seek_to;
    return true;
}


bool
RT_MemoryArchive::SeekFromStart(size_t seek_to)
{
    if (seek_to > m_buffer.size())
	return false;
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
    uint8_t *memory = (uint8_t *)bu_malloc(m_buffer.size() * sizeof(uint8_t), "rt_memoryarchive createcopy");
    std::copy(m_buffer.begin(), m_buffer.end(), memory);
    return memory;
}


size_t
RT_MemoryArchive::Read(size_t amount, void* buf)
{
    const size_t read_amount = (pos + amount > m_buffer.size()) ? m_buffer.size() - pos : amount;
    std::copy(m_buffer.begin() + pos, m_buffer.begin() + pos + read_amount, (char *)buf);
    pos += read_amount;
    return read_amount;
}


size_t
RT_MemoryArchive::Write(const size_t amount, const void* buf)
{
    // the write can come in at any position!
    const size_t start = pos;
    // resize if needed to support new data
    if (m_buffer.size() < (start + amount)) {
	m_buffer.resize(start + amount);
    }

    std::copy((char *)buf, (char *)buf + amount, m_buffer.begin() + pos);
    pos += amount;
    return amount;
}


bool
RT_MemoryArchive::Flush()
{
    return true;
}


static void
brep_dbi2on(const struct rt_db_internal *intern, ONX_Model& model)
{
    struct rt_brep_internal *bi = (struct rt_brep_internal *)intern->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    ON_Layer default_layer;
    default_layer.Default();
    default_layer.SetLayerIndex(0);
    default_layer.SetLayerName("Default");
    model.m_layer_table.Reserve(1);
    model.m_layer_table.Append(default_layer);

    ON_DimStyle default_style;
    default_style.SetDefaults();
    model.m_dimstyle_table.Reserve(1);
    model.m_dimstyle_table.Append(default_style);

    model.m_object_table.SetCapacity(1);

    ONX_Model_Object& mo = model.m_object_table.AppendNew();
    mo.m_object = bi->brep;

    /* XXX what to do about the version */
    mo.m_attributes.m_layer_index = 0;
    mo.m_attributes.m_name = "brep";
    mo.m_attributes.m_uuid = ON_opennurbs4_id;

    model.m_properties.m_RevisionHistory.NewRevision();
    model.m_properties.m_Application.m_application_name = "BRL-CAD B-Rep primitive";
    model.Polish();
}


int
rt_brep_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    if (attr == (char *)NULL) {
	bu_vls_sprintf(logstr, "brep");

	ONX_Model model;
	brep_dbi2on(intern, model);

	/* Create a serialized version for base-64 encoding */
	RT_MemoryArchive archive;
	ON_TextLog err(stderr);
	bool ok = model.Write(archive, 4, "export5", &err);
	if (ok) {
	    void *archive_cp = archive.CreateCopy();
	    signed char *brep64 = bu_b64_encode_block((const signed char *)archive_cp, archive.Size());
	    bu_vls_printf(logstr, " \"%s\"", brep64);
	    bu_free(archive_cp, "free archive copy");
	    bu_free(brep64, "free encoded brep string");
	    return 0;
	}
    }
    return -1;
}


extern "C" int
rt_brep_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_brep_internal *bi = (struct rt_brep_internal *)intern->idb_ptr;
    signed char *decoded;
    ONX_Model model;
    if (argc == 1 && argv[0]) {
	int decoded_size = bu_b64_decode(&decoded, (const signed char *)argv[0]);
	RT_MemoryArchive archive(decoded, decoded_size);
	ON_wString wonstr;
	ON_TextLog log(wonstr);

	RT_BREP_CK_MAGIC(bi);
	model.Read(archive, &log);
	bu_vls_printf(logstr, "%s", ON_String(wonstr).Array());
	ONX_Model_Object mo = model.m_object_table[0];
	bi->brep = ON_Brep::New(*ON_Brep::Cast(mo.m_object));
	return 0;
    }
    return -1;
}


int
rt_brep_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    TRACE1("rt_brep_export5");

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BREP)
	return -1;
    if (dbip)
	RT_CK_DBI(dbip);

    BU_EXTERNAL_INIT(ep);

    ONX_Model model;
    brep_dbi2on(ip, model);

    RT_MemoryArchive archive;
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
    ON_TextLog err(stderr);
    //archive.Dump3dmChunk(err);
    model.Read(archive, &err);

    ONX_Model_Object mo = model.m_object_table[0];
    bi->brep = ON_Brep::New(*ON_Brep::Cast(mo.m_object));
    if (mat) {
	ON_Xform xform(mat);

	if (!xform.IsIdentity()) {
	    bi->brep->Transform(xform);
	}
    }

    return 0;
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
    ip->idb_ptr = ((void *)0);
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
rt_brep_params(struct pc_pc_set *, const struct rt_db_internal *)
{
    return 0;
}


int
rt_brep_boolean(struct rt_db_internal *out, const struct rt_db_internal *ip1, const struct rt_db_internal *ip2, db_op_t operation)
{
    RT_CK_DB_INTERNAL(ip1);
    RT_CK_DB_INTERNAL(ip2);
    struct rt_brep_internal *bip1, *bip2;
    bip1 = (struct rt_brep_internal *)ip1->idb_ptr;
    bip2 = (struct rt_brep_internal *)ip2->idb_ptr;
    RT_BREP_CK_MAGIC(bip1);
    RT_BREP_CK_MAGIC(bip2);

    const ON_Brep *brep1, *brep2;
    ON_Brep *brep_out;
    brep1 = bip1->brep;
    brep2 = bip2->brep;

    op_type operation_type;
    switch (operation) {
	case DB_OP_UNION:
	    operation_type = BOOLEAN_UNION;
	    break;
	case DB_OP_SUBTRACT:
	    operation_type = BOOLEAN_DIFF;
	    break;
	case DB_OP_INTERSECT:
	    operation_type = BOOLEAN_INTERSECT;
	    break;
	default:
	    return -1;
    }

    brep_out = ON_Brep::New();

    int ret;
    if ((ret = ON_Boolean(brep_out, brep1, brep2, operation_type)) < 0)
	return ret;

    // make the final rt_db_internal
    struct rt_brep_internal *bip_out;
    BU_ALLOC(bip_out, struct rt_brep_internal);
    bip_out->magic = RT_BREP_INTERNAL_MAGIC;
    bip_out->brep = brep_out;
    RT_DB_INTERNAL_INIT(out);
    out->idb_ptr = (void *)bip_out;
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


static bool
cmp_cv_startdist(const brep_selectable_cv *c1, const brep_selectable_cv *c2)
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

    std::list<brep_cv *>::const_iterator cv;
    for (cv = cvs->begin(); cv != cvs->end(); ++cv) {
	delete *cv;
    }
    cvs->clear();

    delete cvs;
    delete bs;
    BU_FREE(s, struct rt_selection);
}


static struct rt_selection *
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
		scv->sqdist_to_start = DIST_PNT_PNT_SQ(query->start, cv);
		scv->sqdist_to_line =
		    bg_distsq_line3_pnt3(query->start, query->dir, cv);

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
rt_brep_process_selection(struct rt_db_internal *ip, struct db_i *UNUSED(dbip), const struct rt_selection *selection, const struct rt_selection_operation *op)
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

    const brep_selection *bs = (brep_selection *)selection->obj;
    if (!brep || !bs || bs->control_vertexes->empty()) {
	return -1;
    }

    fastf_t dx = op->parameters.tran.dx;
    fastf_t dy = op->parameters.tran.dy;
    fastf_t dz = op->parameters.tran.dz;

    std::list<brep_cv *>::const_iterator cv = bs->control_vertexes->begin();
    for (; cv != bs->control_vertexes->end(); ++cv) {
	// TODO: if another face references the same surface, the
	// surface needs to be duplicated
	int face_index = (*cv)->face_index;
	if (face_index < 0 || face_index >= brep->m_F.Count()) {
	    bu_log("%d is not a valid face index\n", face_index);
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


static void
brep_log(struct bu_vls *log, const char *fmt, ...)
{
    va_list ap;
    if (log) {
	BU_CK_VLS(log);
	va_start(ap, fmt);
	bu_vls_vprintf(log, fmt, ap);
    }
    va_end(ap);
}


int
rt_brep_valid(struct bu_vls *log, struct rt_db_internal *ip, int flags)
{
    int ret = 1; /* Start assuming it is valid - we need to prove it isn't */
    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BREP) {
	brep_log(log, "Object is not a brep.\n");
	return 0;
    }
    struct rt_brep_internal *bi = (struct rt_brep_internal *)ip->idb_ptr;
    ON_Brep *brep = NULL;
    if (bi == NULL || bi->brep == NULL) {
	brep_log(log, "Error: No ON_Brep object present.\n");
	return 0;
    }
    brep = bi->brep;

    /* OpenNURBS IsValid test */
    if (!flags || flags & RT_BREP_OPENNURBS) {
	ON_TextLog text(stderr);
	if (!brep->IsValid(&text)) {
	    brep_log(log, "brep NOT valid\n");
	    ret = 0;
	    goto brep_valid_done;
	}
    }

#if 0
    /* UV domain sanity checks - this doesn't trigger on bad face of test case, so
     * apparently not issue??? or are we having the issue lots of places due
     * to the fixed edge tol and just not seeing it much due to the NM/NH logic? */
    if (!flags || flags & RT_BREP_UV_PARAM) {
	double delta_threshold = BREP_EDGE_MISS_TOLERANCE * 100;
	for (int index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace *face = brep->Face(index);
	    const ON_Surface *s = face->SurfaceOf();
	    if (s) {
		double umin, umax, vmin, vmax;
		s->GetDomain(0, &umin, &umax);
		s->GetDomain(1, &vmin, &vmax);
		if (fabs(umax - umin) < delta_threshold) {
		    brep_log(log, "Face %d: small delta in U parameterization domain: %g -> %g (%g < %g)\n", index, umin, umax, fabs(umax - umin), delta_threshold);
		    ret = 0;
		}
		if (fabs(vmax - vmin) < delta_threshold) {
		    brep_log(log, "Face %d: small delta in V parameterization domain: %g -> %g (%g)\n", index, vmin, vmax, fabs(vmax - vmin));
		    ret = 0;
		}
	    }
	}
	if (!ret) {
	    goto brep_valid_done;
	}
    }
#endif

brep_valid_done:
    if (log && ret)
	bu_vls_printf(log, "\nbrep is valid\n");
    return ret;
}

void
rt_brep_plate_mode_getvals(double *pthickness, int *nocos, const struct rt_db_internal *ip)
{
    if (!pthickness || !nocos || !ip) return;

    (*pthickness) = 0.0;
    (*nocos) = 0;

    // Check for a thickness and nocos setting
    if (ip->idb_avs.magic == BU_AVS_MAGIC) {
	const char *pval = bu_avs_get(&ip->idb_avs, "_plate_mode_thickness");
	if (pval != NULL) {
	    char *endptr = NULL;
	    errno = 0;
	    (*pthickness) = fabs(strtod(pval, &endptr));
	    if ((endptr != NULL && strlen(endptr) > 0) || (errno == ERANGE)) {
		(*pthickness) = 0.0;
	    }
	    //bu_log("plate mode thickness: %f\n", pthickness);
	}
	const char *pcos = bu_avs_get(&ip->idb_avs, "_plate_mode_nocos");
	if (BU_STR_EQUAL(pcos, "1")) {
	    (*nocos) = 1;
	}
    }
}

int
rt_brep_plate_mode(const struct rt_db_internal *ip)
{
    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BREP) {
	return 0;
    }
    struct rt_brep_internal *bi = (struct rt_brep_internal *)ip->idb_ptr;
    if (bi == NULL || bi->brep == NULL) {
	return 0;
    }

    if (!bi->brep->IsValid(NULL)) {
	// Not valid, not plate mode
	return 0;
    }

    if (bi->brep->IsSolid()) {
	// Is solid, not plate mode
	return 0;
    }

    // Valid and not solid - plate mode
    return 1;
}


int
rt_brep_prep_serialize(struct soltab *stp, const struct rt_db_internal *ip, struct bu_external *external, size_t *version)
{
    RT_CK_SOLTAB(stp);
    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(external);

    const size_t current_version = 0;

    RT_CK_SOLTAB(stp);
    BU_CK_EXTERNAL(external);

    if (stp->st_specific) {
	/* export to external */

	const brep_specific &specific = *static_cast<brep_specific *>(stp->st_specific);

	Serializer serializer;
	serializer.write_uint32(specific.bvh->get_children().size());

	for (std::vector<BBNode *>::const_iterator it = specific.bvh->get_children().begin(); it != specific.bvh->get_children().end(); ++it) {
	    (*it)->m_ctree->serialize(serializer);
	    (*it)->serialize(serializer);
	}

	*version = current_version;
	*external = serializer.take();
	return 0;
    } else {
	/* load from external */

	if (*version != current_version)
	    return 1;

	brep_specific * const specific = brep_specific_new();
	stp->st_specific = specific;
	specific->plate_mode = rt_brep_plate_mode(ip);
	std::swap(specific->brep, static_cast<rt_brep_internal *>(ip->idb_ptr)->brep);
	if (specific->plate_mode) {
	    rt_brep_plate_mode_getvals(&specific->plate_mode_thickness, &specific->plate_mode_nocos, ip);
	}
	specific->bvh = new BBNode(specific->brep->BoundingBox());
	specific->is_solid = specific->brep->IsSolid(); // recompute solidity

	Deserializer deserializer(*external);
	const uint32_t num_children = deserializer.read_uint32();

	for (uint32_t i = 0; i < num_children; ++i) {
	    const CurveTree * const ctree = new CurveTree(deserializer, *specific->brep->m_F.At(i));
	    specific->bvh->addChild(new BBNode(deserializer, *ctree));
	}

	specific->bvh->BuildBBox();

	{
	    /* Once a proper SurfaceTree is built, finalize the bounding
	     * volumes.  This takes no time. */
	    specific->bvh->GetBBox(stp->st_min, stp->st_max);

	    // expand outer bounding box just a little bit
	    const struct bn_tol *tol = &stp->st_rtip->rti_tol;
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
	}

	return 0;
    }
}

int rt_brep_plot_poly(struct bu_list *vhead, const struct db_full_path *pathp, struct rt_db_internal *ip,
		      const struct bg_tess_tol *ttol, const struct bn_tol *tol,
		      const struct bview *UNUSED(info))
{
    TRACE1("rt_brep_plot");

    if (!vhead || !pathp || pathp->fp_len <= 0 || !ip || !ttol || !tol)
	return -1;

    struct rt_brep_internal* bi;
    const char *solid_name =  DB_FULL_PATH_CUR_DIR(pathp)->d_namep;
    ON_wString wstr;
    ON_TextLog tl(wstr);

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal*) ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    ON_Brep* brep = bi->brep;

    if (brep_facecdt_plot(NULL, solid_name, ttol, tol, brep, vhead, NULL, &RTG.rtg_vlfree, -1, 0, -1)) {
	return -1;
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
