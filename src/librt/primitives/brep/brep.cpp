/*                     B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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
#include <cmath>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <set>

#include "assert.h"

#include "vmath.h"

#include "bu/cv.h"
#include "bu/opt.h"
#include "bu/datetime.h"
#include "brep.h"
#include "bn/mat.h"
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
    void rt_brep_vshot(struct soltab *stp[], struct xray *rp[], struct seg *segp, int n, struct application *ap);
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
    int rt_brep_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip);
    int rt_brep_mirror(struct rt_db_internal *ip, const plane_t plane);
    int rt_brep_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip);
    void rt_brep_ifree(struct rt_db_internal *ip);
    int rt_brep_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local);
    int rt_brep_make(const struct rt_functab *ftp, struct rt_db_internal *intern, const char *variant, const point_t origin, double scale);
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

    const ON_BrepFace *face;
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

    /* Fixed workspace slots are assigned in full before use. */
    brep_hit() = default;

    brep_hit(const ON_BrepFace& f, const ON_Ray& ray, const point_t p, const vect_t n, const pt2d_t _uv)
	: face(&f), trimmed(false), closeToEdge(false), oob(false), hit(CLEAN_HIT), direction(ENTERING), m_adj_face_index(0), sbv(NULL)
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
	: face(&f), dist(d), trimmed(false), closeToEdge(false), oob(false), hit(CLEAN_HIT), direction(ENTERING), m_adj_face_index(0), sbv(NULL)
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
	face = h.face;
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


class brep_hit_workspace
{
public:
    brep_hit_workspace() : m_count(0), m_total(0), m_overflow(false) {}

    void push_back(const brep_hit &hit)
    {
	++m_total;
	if (m_count == RT_BREP_MAX_HITS) {
	    m_overflow = true;
	    return;
	}
	m_hits[m_count++] = hit;
    }

    void sort()
    {
	/* Stable insertion sort matches std::list::sort's ordering of hits with
	 * equal ray distances and keeps this bounded workspace self-contained. */
	for (size_t i = 1; i < m_count; ++i) {
	    brep_hit value = m_hits[i];
	    size_t j = i;
	    while (j > 0 && value < m_hits[j - 1]) {
		m_hits[j] = m_hits[j - 1];
		--j;
	    }
	    m_hits[j] = value;
	}
    }

    brep_hit &operator[](size_t index) { return m_hits[index]; }
    const brep_hit &operator[](size_t index) const { return m_hits[index]; }
    brep_hit &front() { return m_hits[0]; }
    const brep_hit &front() const { return m_hits[0]; }
    brep_hit &back() { return m_hits[m_count - 1]; }
    const brep_hit &back() const { return m_hits[m_count - 1]; }
    void erase(size_t index)
    {
	for (size_t i = index + 1; i < m_count; ++i)
	    m_hits[i - 1] = m_hits[i];
	if (m_count)
	    --m_count;
    }
    void pop_back()
    {
	if (m_count)
	    --m_count;
    }
    void pop_front()
    {
	if (m_count)
	    erase(0);
    }
    bool empty() const { return m_count == 0; }
    size_t size() const { return m_count; }
    size_t total() const { return m_total; }
    bool overflow() const { return m_overflow; }

private:
    brep_hit m_hits[RT_BREP_MAX_HITS];
    size_t m_count;
    size_t m_total;
    bool m_overflow;
};


static bool
brep_hits_identical(const brep_hit &first, const brep_hit &second)
{
    return first.face == second.face &&
	std::memcmp(&first.dist, &second.dist, sizeof(first.dist)) == 0 &&
	std::memcmp(first.origin, second.origin, sizeof(first.origin)) == 0 &&
	std::memcmp(first.point, second.point, sizeof(first.point)) == 0 &&
	std::memcmp(first.normal, second.normal, sizeof(first.normal)) == 0 &&
	std::memcmp(first.uv, second.uv, sizeof(first.uv)) == 0 &&
	first.trimmed == second.trimmed &&
	first.closeToEdge == second.closeToEdge &&
	first.oob == second.oob &&
	first.hit == second.hit &&
	first.direction == second.direction &&
	first.m_adj_face_index == second.m_adj_face_index &&
	first.sbv == second.sbv;
}


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
	bu_vls_printf(&logstr, "%s(%d)", brep_hit_type_str((int)out.hit), out.face->m_face_index);
	if (out.direction == brep_hit::ENTERING) bu_vls_printf(&logstr, "+");
	if (out.direction == brep_hit::LEAVING) bu_vls_printf(&logstr, "-");
	bu_vls_printf(&logstr, "[%d]", out.sbv->get_face().m_bRev);
	bu_vls_printf(&logstr, "}");
	VMOVE(prev, out.point);
    }
    bu_log("%s\n", bu_vls_addr(&logstr));
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
    return new brep_specific();
}


static void
brep_specific_delete(struct brep_specific* bs)
{
    if (bs != NULL) {
	delete bs->bvh;
	for (std::vector<const CurveTree *>::const_iterator i = bs->ctrees.begin(); i != bs->ctrees.end(); ++i)
	    delete *i;
	delete bs->brep;
	delete bs;
    }
}


//--------------------------------------------------------------------------------
// prep

struct brep_build_bvh_parallel {
    struct brep_specific *bs;
    SurfaceTree**faces;
    int depth_limit;
};


static bool
brep_bezier_control_bbox(const ON_BezierCurve &curve, ON_BoundingBox &bbox)
{
    if (curve.Dimension() != 3 || curve.CVCount() < 1)
	return false;
    bool grow = false;
    for (int i = 0; i < curve.CVCount(); ++i) {
	ON_4dPoint cv;
	if (!curve.GetCV(i, cv) || !cv.IsValid() ||
		!(cv.w > 0.0) || !std::isfinite(cv.w))
	    return false;
	const ON_3dPoint point(cv.x / cv.w, cv.y / cv.w, cv.z / cv.w);
	if (!point.IsValid() || !bbox.Set(point, grow))
	    return false;
	grow = true;
    }
    return bbox.IsValid();
}


static bool
brep_bezier_control_bbox(const ON_BezierSurface &surface,
    ON_BoundingBox &bbox)
{
    if (surface.Dimension() != 3 || surface.Order(0) < 1 ||
	    surface.Order(1) < 1)
	return false;
    bool grow = false;
    for (int i = 0; i < surface.Order(0); ++i) {
	for (int j = 0; j < surface.Order(1); ++j) {
	    ON_4dPoint cv;
	    if (!surface.GetCV(i, j, cv) || !cv.IsValid() ||
		    !(cv.w > 0.0) || !std::isfinite(cv.w))
		return false;
	    const ON_3dPoint point(cv.x / cv.w, cv.y / cv.w,
		cv.z / cv.w);
	    if (!point.IsValid() || !bbox.Set(point, grow))
		return false;
	    grow = true;
	}
    }
    return bbox.IsValid();
}


static const int BREP_DIRECT_BEZIER_MAX_ORDER = 16;
static const size_t BREP_DIRECT_BEZIER_MAX_CVS =
    BREP_DIRECT_BEZIER_MAX_ORDER * BREP_DIRECT_BEZIER_MAX_ORDER;
static const size_t BREP_DIRECT_SUBDIVISION_CAPACITY = 64;
static const int BREP_DIRECT_SUBDIVISION_MAX_DEPTH = 24;
static const int BREP_DIRECT_SUBDIVISION_MIN_ADAPTIVE_DEPTH = 4;
static const int BREP_DIRECT_EXACT_REFINEMENT_LEVELS = 8;
static const size_t BREP_DIRECT_EXACT_REFINEMENT_BUDGET = 64;
static const double BREP_DIRECT_CLIP_MINIMUM_FRACTION = 1.0 / 16.0;
static const double BREP_DIRECT_CLIP_MINIMUM_RETAINED_FRACTION = 0.5;
static const double BREP_DIRECT_ROOT_RELATIVE_TOLERANCE = 5.0e-9;
static const double BREP_DIRECT_EVALUATION_ULPS = 32.0;
static const double BREP_SEAM_BOUND_RELATIVE_TOLERANCE = 0.01;
static const size_t BREP_SEAM_BOUND_CELL_BUDGET = 4096;
static const size_t BREP_SEAM_CORRESPONDENCE_CELL_BUDGET = 4096;
static_assert(RT_BREP_DEFAULT_SURFACE_TREE_DEPTH == BREP_MAX_FT_DEPTH,
    "librt and libbrep SurfaceTree depth defaults must agree");


static void
brep_build_surface_data(struct brep_specific *bs)
{
    bs->face_records.clear();
    bs->surface_spans.clear();
    if (!bs->brep)
	return;

    const ON_Brep &brep = *bs->brep;
    bs->face_records.reserve(brep.m_F.Count());
    for (int face_index = 0; face_index < brep.m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep.m_F[face_index];
	const ON_Surface *surface = face.SurfaceOf();
	brep_face_record record;
	record.face_index = face_index;
	record.span_begin = bs->surface_spans.size();
	if (!surface) {
	    bs->face_records.push_back(record);
	    continue;
	}

	ON_NurbsSurface nurbs;
	/* Status 2 preserves the exact locus but requires an explicit
	 * point-parameter map before trim or normal evaluation.  Its prepared
	 * eligibility is restricted to regular interior events below; mapped
	 * seam, edge, and vertex boxes need separate interval certificates. */
	record.nurb_form_status = surface->GetNurbForm(nurbs);
	if (record.nurb_form_status < 1 || record.nurb_form_status > 2 ||
		nurbs.m_order[0] < 2 ||
		nurbs.m_order[1] < 2 ||
		nurbs.m_order[0] > BREP_DIRECT_BEZIER_MAX_ORDER ||
		nurbs.m_order[1] > BREP_DIRECT_BEZIER_MAX_ORDER ||
		nurbs.m_cv_count[0] < nurbs.m_order[0] ||
		nurbs.m_cv_count[1] < nurbs.m_order[1]) {
	    bs->face_records.push_back(record);
	    continue;
	}

	bool complete = true;
	for (int u_span = 0;
		u_span <= nurbs.m_cv_count[0] - nurbs.m_order[0]; ++u_span) {
	    const double u_lower =
		nurbs.m_knot[0][u_span + nurbs.m_order[0] - 2];
	    const double u_upper =
		nurbs.m_knot[0][u_span + nurbs.m_order[0] - 1];
	    if (!(u_lower < u_upper))
		continue;
	    for (int v_span = 0;
		    v_span <= nurbs.m_cv_count[1] - nurbs.m_order[1];
		    ++v_span) {
		const double v_lower =
		    nurbs.m_knot[1][v_span + nurbs.m_order[1] - 2];
		const double v_upper =
		    nurbs.m_knot[1][v_span + nurbs.m_order[1] - 1];
		if (!(v_lower < v_upper))
		    continue;
		brep_surface_span span;
		if (!nurbs.ConvertSpanToBezier(u_span, v_span, span.surface) ||
			!brep_bezier_control_bbox(span.surface, span.bbox)) {
		    complete = false;
		    break;
		}
		span.surface_domain[0] = ON_Interval(u_lower, u_upper);
		span.surface_domain[1] = ON_Interval(v_lower, v_upper);
		span.face_index = face_index;
		span.span_index = (int)bs->surface_spans.size();
		bs->surface_spans.push_back(span);
	    }
	    if (!complete)
		break;
	}
	if (!complete || bs->surface_spans.size() == record.span_begin) {
	    bs->surface_spans.resize(record.span_begin);
	} else {
	    record.span_count = bs->surface_spans.size() - record.span_begin;
	    record.supported = true;
	}
	bs->face_records.push_back(record);
    }
}


static const brep_face_record *
brep_face_surface_record(const struct brep_specific *bs, int face_index)
{
    if (!bs || face_index < 0)
	return NULL;
    for (std::vector<brep_face_record>::const_iterator record =
	    bs->face_records.begin(); record != bs->face_records.end(); ++record)
	if (record->face_index == face_index)
	    return &*record;
    return NULL;
}


static bool
brep_surface_parameter_from_nurbs(const struct brep_specific *bs,
    int face_index, const double nurbs_uv[2], ON_2dPoint &surface_uv)
{
    if (!bs || !bs->brep || !nurbs_uv || face_index < 0 ||
	    face_index >= bs->brep->m_F.Count() ||
	    !std::isfinite(nurbs_uv[0]) || !std::isfinite(nurbs_uv[1]))
	return false;
    const brep_face_record *record = brep_face_surface_record(bs,
	face_index);
    const ON_Surface *surface = bs->brep->m_F[face_index].SurfaceOf();
    if (!record || !record->supported || !surface)
	return false;
    if (record->nurb_form_status == 1) {
	surface_uv.Set(nurbs_uv[0], nurbs_uv[1]);
	return surface_uv.IsValid();
    }
    if (record->nurb_form_status != 2 ||
	    !surface->GetSurfaceParameterFromNurbFormParameter(nurbs_uv[0],
		nurbs_uv[1], &surface_uv.x, &surface_uv.y) ||
	    !surface_uv.IsValid())
	return false;

    double round_trip[2] = {ON_UNSET_VALUE, ON_UNSET_VALUE};
    if (!surface->GetNurbFormParameterFromSurfaceParameter(surface_uv.x,
	    surface_uv.y, &round_trip[0], &round_trip[1]) ||
	    !std::isfinite(round_trip[0]) || !std::isfinite(round_trip[1]))
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	const double scale = std::max(1.0, std::max(fabs(nurbs_uv[direction]),
	    fabs(round_trip[direction])));
	if (fabs(round_trip[direction] - nurbs_uv[direction]) >
		4096.0 * DBL_EPSILON * scale)
	    return false;
    }
    return true;
}


static bool
brep_trim_lift_at(const ON_BrepTrim &trim, double parameter,
    ON_3dPoint &lift, ON_3dVector *derivative)
{
    const ON_BrepFace *face = trim.Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!surface)
	return false;
    if (!derivative) {
	const ON_3dPoint uv = trim.PointAt(parameter);
	lift = surface->PointAt(uv.x, uv.y);
	return uv.IsValid() && lift.IsValid();
    }
    ON_3dPoint uv;
    ON_3dVector trim_derivative;
    if (!trim.Ev1Der(parameter, uv, trim_derivative) || !uv.IsValid() ||
	    !trim_derivative.IsValid())
	return false;
    ON_3dVector surface_u;
    ON_3dVector surface_v;
    if (!surface->Ev1Der(uv.x, uv.y, lift, surface_u, surface_v) ||
	    !lift.IsValid() || !surface_u.IsValid() || !surface_v.IsValid())
	return false;
    if (derivative) {
	*derivative = trim_derivative.x * surface_u +
	    trim_derivative.y * surface_v;
	if (!derivative->IsValid())
	    return false;
    }
    return true;
}


static bool
brep_edge_trim_parameter_in_domain(const ON_BrepEdge &edge,
    const ON_BrepTrim &trim, double edge_parameter,
    const ON_Interval &search_domain, int sample_count,
    double &trim_parameter)
{
    const ON_Interval edge_domain = edge.Domain();
    const ON_Interval trim_domain = trim.Domain();
    if (!edge_domain.IsIncreasing() || !trim_domain.IsIncreasing() ||
	    !search_domain.IsIncreasing() || sample_count < 2 ||
	    trim.m_ei != edge.m_edge_index)
	return false;
    const double domain_tolerance = 256.0 * DBL_EPSILON *
	std::max(1.0, std::max(fabs(trim_domain.Min()),
	    fabs(trim_domain.Max())));
    if (search_domain.Min() < trim_domain.Min() - domain_tolerance ||
	    search_domain.Max() > trim_domain.Max() + domain_tolerance)
	return false;
    const ON_3dPoint edge_point = edge.PointAt(edge_parameter);
    if (!edge_point.IsValid())
	return false;

    double edge_fraction = edge_domain.NormalizedParameterAt(edge_parameter);
    if (!std::isfinite(edge_fraction) ||
	    edge_fraction < -ON_ZERO_TOLERANCE ||
	    edge_fraction > 1.0 + ON_ZERO_TOLERANCE)
	return false;
    edge_fraction = std::max(0.0, std::min(1.0, edge_fraction));
    if (trim.m_bRev3d)
	edge_fraction = 1.0 - edge_fraction;

    double best_parameter = trim_domain.ParameterAt(edge_fraction);
    best_parameter = std::max(search_domain.Min(),
	std::min(search_domain.Max(), best_parameter));
    ON_3dPoint initial_lift;
    if (!brep_trim_lift_at(trim, best_parameter, initial_lift, NULL))
	return false;
    double best_distance = initial_lift.DistanceTo(edge_point);
    if (!std::isfinite(best_distance))
	return false;
    for (int sample = 0; sample < sample_count; ++sample) {
	const double parameter = search_domain.ParameterAt((double)sample /
	    (double)(sample_count - 1));
	ON_3dPoint lift;
	if (!brep_trim_lift_at(trim, parameter, lift, NULL))
	    return false;
	const double distance = lift.DistanceTo(edge_point);
	if (!std::isfinite(distance))
	    return false;
	if (distance < best_distance) {
	    best_distance = distance;
	    best_parameter = parameter;
	}
    }

    for (int iteration = 0; iteration < 24; ++iteration) {
	ON_3dPoint lift;
	ON_3dVector derivative;
	if (!brep_trim_lift_at(trim, best_parameter, lift, &derivative))
	    return false;
	const double denominator = derivative * derivative;
	if (!(denominator > DBL_MIN) || !std::isfinite(denominator))
	    break;
	const ON_3dVector residual = lift - edge_point;
	const double step = (residual * derivative) / denominator;
	if (!std::isfinite(step))
	    return false;
	if (fabs(step) <= 64.0 * DBL_EPSILON *
		std::max(1.0, search_domain.Length()))
	    break;

	bool improved = false;
	double fraction = 1.0;
	for (int line_search = 0; line_search < 8; ++line_search) {
	    const double candidate = std::max(search_domain.Min(),
		std::min(search_domain.Max(), best_parameter - fraction * step));
	    ON_3dPoint candidate_lift;
	    if (!brep_trim_lift_at(trim, candidate, candidate_lift, NULL)) {
		fraction *= 0.5;
		continue;
	    }
	    const double distance = candidate_lift.DistanceTo(edge_point);
	    if (std::isfinite(distance) && distance < best_distance) {
		best_parameter = candidate;
		best_distance = distance;
		improved = true;
		break;
	    }
	    fraction *= 0.5;
	}
	if (!improved)
	    break;
    }

    trim_parameter = best_parameter;
    return std::isfinite(trim_parameter) && std::isfinite(best_distance);
}


static bool
brep_edge_trim_parameter(const ON_BrepEdge &edge, const ON_BrepTrim &trim,
    double edge_parameter, double &trim_parameter)
{
    return brep_edge_trim_parameter_in_domain(edge, trim, edge_parameter,
	trim.Domain(), 33, trim_parameter);
}


static bool
brep_curve_closest_parameter(const ON_Curve &curve, const ON_3dPoint &point,
    double &parameter)
{
    const ON_Interval domain = curve.Domain();
    if (!domain.IsIncreasing() || !point.IsValid())
	return false;
    double best_distance = DBL_MAX;
    double best_parameter = domain.Min();
    for (int sample = 0; sample <= 32; ++sample) {
	const double candidate = domain.ParameterAt((double)sample / 32.0);
	const ON_3dPoint curve_point = curve.PointAt(candidate);
	if (!curve_point.IsValid())
	    return false;
	const double distance = curve_point.DistanceTo(point);
	if (!std::isfinite(distance))
	    return false;
	if (distance < best_distance) {
	    best_distance = distance;
	    best_parameter = candidate;
	}
    }

    for (int iteration = 0; iteration < 24; ++iteration) {
	ON_3dPoint curve_point;
	ON_3dVector first_derivative;
	ON_3dVector second_derivative;
	if (!curve.Ev2Der(best_parameter, curve_point, first_derivative,
		second_derivative) || !curve_point.IsValid() ||
		!first_derivative.IsValid() || !second_derivative.IsValid())
	    return false;
	const ON_3dVector residual = curve_point - point;
	const double numerator = residual * first_derivative;
	const double denominator = first_derivative * first_derivative +
	    residual * second_derivative;
	const double denominator_scale = std::max(1.0,
	    first_derivative * first_derivative +
	    fabs(residual * second_derivative));
	if (!std::isfinite(numerator) || !std::isfinite(denominator))
	    return false;
	if (fabs(denominator) <= 128.0 * DBL_EPSILON * denominator_scale)
	    break;
	const double step = numerator / denominator;
	if (!std::isfinite(step))
	    return false;
	if (fabs(step) <= 64.0 * DBL_EPSILON *
		std::max(1.0, domain.Length()))
	    break;

	bool improved = false;
	double fraction = 1.0;
	for (int line_search = 0; line_search < 8; ++line_search) {
	    const double candidate = std::max(domain.Min(),
		std::min(domain.Max(), best_parameter - fraction * step));
	    const ON_3dPoint curve_point_candidate = curve.PointAt(candidate);
	    if (!curve_point_candidate.IsValid())
		return false;
	    const double distance = curve_point_candidate.DistanceTo(point);
	    if (!std::isfinite(distance))
		return false;
	    if (distance < best_distance) {
		best_distance = distance;
		best_parameter = candidate;
		improved = true;
		break;
	    }
	    fraction *= 0.5;
	}
	if (!improved)
	    break;
    }
    parameter = best_parameter;
    return std::isfinite(parameter) && std::isfinite(best_distance);
}


/* Retain the former bounded sampler as diagnostic evidence only.  Production
 * edge/seam eligibility is assigned later by the global proof. */
static bool
brep_edge_trim_correspondence_regular(const ON_BrepEdge &edge,
    const ON_BrepTrim &trim)
{
    const ON_BrepFace *face = trim.Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!surface || trim.m_ei != edge.m_edge_index)
	return false;
    ON_NurbsCurve nurbs;
    if (!trim.GetNurbForm(nurbs) || nurbs.m_order < 2 ||
	    nurbs.m_cv_count < nurbs.m_order)
	return false;
    const ON_Interval edge_domain = edge.Domain();
    const ON_3dPoint edge_start = edge.PointAtStart();
    const ON_3dPoint edge_end = edge.PointAtEnd();
    if (!edge_domain.IsIncreasing() || !edge_start.IsValid() ||
	    !edge_end.IsValid())
	return false;
    const double edge_coordinate_scale = std::max(1.0,
	std::max(fabs(edge_start.x), std::max(fabs(edge_start.y),
	std::max(fabs(edge_start.z), std::max(fabs(edge_end.x),
	std::max(fabs(edge_end.y), fabs(edge_end.z)))))));
    if (edge_start.DistanceTo(edge_end) <=
	    1024.0 * DBL_EPSILON * edge_coordinate_scale)
	return false;

    size_t samples = 0;
    bool have_previous_parameter = false;
    double previous_edge_parameter = 0.0;
    const double edge_parameter_tolerance = 1024.0 * DBL_EPSILON *
	std::max(1.0, std::max(fabs(edge_domain.Min()),
	    fabs(edge_domain.Max())));
    for (int span_index = 0;
	    span_index <= nurbs.m_cv_count - nurbs.m_order; ++span_index) {
	const double lower = nurbs.m_knot[span_index + nurbs.m_order - 2];
	const double upper = nurbs.m_knot[span_index + nurbs.m_order - 1];
	if (!(lower < upper))
	    continue;
	for (int sample = 1; sample <= 4; ++sample) {
	    if (++samples > 256)
		return false;
	    const double trim_parameter = lower +
		((double)sample / 5.0) * (upper - lower);
	    ON_3dPoint uv;
	    ON_3dVector trim_derivative;
	    if (!nurbs.Ev1Der(trim_parameter, uv, trim_derivative) ||
		    !uv.IsValid() || !trim_derivative.IsValid())
		return false;
	    ON_3dPoint lift;
	    ON_3dVector surface_u;
	    ON_3dVector surface_v;
	    if (!surface->Ev1Der(uv.x, uv.y, lift, surface_u, surface_v) ||
		    !lift.IsValid() || !surface_u.IsValid() ||
		    !surface_v.IsValid())
		return false;
	    ON_3dVector lift_tangent = trim_derivative.x * surface_u +
		trim_derivative.y * surface_v;
	    double edge_parameter = 0.0;
	    if (!lift_tangent.Unitize() ||
		    !brep_curve_closest_parameter(edge, lift, edge_parameter))
		return false;
	    ON_3dPoint edge_point;
	    ON_3dVector edge_tangent;
	    if (!edge.Ev1Der(edge_parameter, edge_point, edge_tangent) ||
		    !edge_point.IsValid() || !edge_tangent.Unitize())
		return false;
	    if (have_previous_parameter) {
		const double directed_change = trim.m_bRev3d ?
		    previous_edge_parameter - edge_parameter :
		    edge_parameter - previous_edge_parameter;
		if (directed_change < -edge_parameter_tolerance)
		    return false;
	    }
	    previous_edge_parameter = edge_parameter;
	    have_previous_parameter = true;
	    const double directed_tangent = (trim.m_bRev3d ? -1.0 : 1.0) *
		(lift_tangent * edge_tangent);
	    if (!std::isfinite(directed_tangent) ||
		    directed_tangent <= 1.0e-8)
		return false;
	}
    }
    return samples > 0;
}


static bool
brep_edge_discrepancy(const ON_Brep &brep, const ON_BrepEdge &edge,
    double &maximum)
{
    const ON_Interval edge_domain = edge.Domain();
    if (edge.m_ti.Count() != 2 || !edge_domain.IsIncreasing())
	return false;
    maximum = 0.0;
    for (int sample = 0; sample <= 32; ++sample) {
	double edge_fraction = (double)sample / 32.0;
	const ON_3dPoint edge_point = edge.PointAt(
	    edge_domain.ParameterAt(edge_fraction));
	if (!edge_point.IsValid())
	    return false;
	for (int side = 0; side < 2; ++side) {
	    const int trim_index = edge.m_ti[side];
	    if (trim_index < 0 || trim_index >= brep.m_T.Count())
		return false;
	    const ON_BrepTrim &trim = brep.m_T[trim_index];
	    double trim_parameter = 0.0;
	    if (!brep_edge_trim_parameter(edge, trim,
		    edge_domain.ParameterAt(edge_fraction), trim_parameter))
		return false;
	    ON_3dPoint lift;
	    if (!brep_trim_lift_at(trim, trim_parameter, lift, NULL))
		return false;
	    maximum = std::max(maximum, edge_point.DistanceTo(lift));
	}
    }
    return std::isfinite(maximum);
}


static void
brep_certify_edge_correspondences(struct brep_specific *bs);


static void
brep_build_edge_data(struct brep_specific *bs, const struct bn_tol *tol)
{
    bs->edge_records.clear();
    bs->edge_spans.clear();
    if (!bs->brep)
	return;

    const ON_Brep &brep = *bs->brep;
    size_t maximum_spans = 0;
    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep.m_E[edge_index];
	if (edge.m_ti.Count() == 2)
	    maximum_spans += std::max(0, edge.SpanCount());
    }
    bs->edge_records.reserve(brep.m_E.Count());
    bs->edge_spans.reserve(maximum_spans);

    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep.m_E[edge_index];
	if (edge.m_ti.Count() != 2)
	    continue;
	const int first_trim = edge.m_ti[0];
	const int second_trim = edge.m_ti[1];
	if (first_trim < 0 || first_trim >= brep.m_T.Count() ||
		second_trim < 0 || second_trim >= brep.m_T.Count())
	    continue;

	brep_edge_record record;
	record.edge_index = edge_index;
	record.face_index[0] = brep.m_T[first_trim].FaceIndexOf();
	record.face_index[1] = brep.m_T[second_trim].FaceIndexOf();
	record.model_tolerance = tol && tol->dist >= 0.0 ? tol->dist : 0.0;
	record.declared_tolerance = edge.m_tolerance;
	record.tolerance = record.model_tolerance;
	const bool declared = ON_IsValid(record.declared_tolerance) &&
	    record.declared_tolerance >= 0.0;
	if (declared)
	    record.tolerance = std::max(record.tolerance,
		record.declared_tolerance);
	else
	    record.tolerance_inferred = true;
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const int vertex_index = edge.m_vi[endpoint];
	    if (vertex_index < 0 || vertex_index >= brep.m_V.Count())
		continue;
	    const double vertex_tolerance =
		brep.m_V[vertex_index].m_tolerance;
	    if (ON_IsValid(vertex_tolerance) && vertex_tolerance >= 0.0)
		record.tolerance = std::max(record.tolerance,
		    vertex_tolerance);
	}
	record.discrepancy_measured = brep_edge_discrepancy(brep, edge,
	    record.measured_discrepancy);
	record.correspondence_screened =
	    brep_edge_trim_correspondence_regular(edge,
		brep.m_T[first_trim]) &&
	    brep_edge_trim_correspondence_regular(edge,
		brep.m_T[second_trim]);
	if (record.tolerance_inferred && record.discrepancy_measured)
	    record.tolerance = std::max(record.tolerance,
		record.measured_discrepancy);
	const double coordinate_scale = std::max(1.0,
	    std::max(fabs(edge.PointAtStart().x),
	    std::max(fabs(edge.PointAtStart().y),
	    fabs(edge.PointAtStart().z))));
	const double roundoff = std::max(ON_ZERO_TOLERANCE,
	    128.0 * DBL_EPSILON * coordinate_scale);
	record.discrepancy_sample_authorized = record.discrepancy_measured &&
	    record.measured_discrepancy <= record.tolerance + roundoff;
	record.discrepancy_authorized = false;
	record.span_begin = bs->edge_spans.size();
	if (record.face_index[0] < 0 || record.face_index[1] < 0) {
	    bs->edge_records.push_back(record);
	    continue;
	}

	ON_NurbsCurve nurbs;
	/* A return value of two preserves the locus but not the edge's
	 * parameterization.  Local topology work needs both, so leave such
	 * curves on the explicit unsupported/fallback path. */
	if (edge.GetNurbForm(nurbs) != 1 || nurbs.m_order < 2 ||
		nurbs.m_cv_count < nurbs.m_order) {
	    bs->edge_records.push_back(record);
	    continue;
	}

	bool complete = true;
	for (int span_index = 0;
		span_index <= nurbs.m_cv_count - nurbs.m_order;
		++span_index) {
	    const double lower = nurbs.m_knot[span_index + nurbs.m_order - 2];
	    const double upper = nurbs.m_knot[span_index + nurbs.m_order - 1];
	    if (!(lower < upper))
		continue;
	    brep_edge_span span;
	    if (!nurbs.ConvertSpanToBezier(span_index, span.curve) ||
		    !brep_bezier_control_bbox(span.curve, span.bbox)) {
		complete = false;
		break;
	    }
	    span.edge_domain = ON_Interval(lower, upper);
	    span.edge_index = edge_index;
	    bs->edge_spans.push_back(span);
	}
	if (!complete || bs->edge_spans.size() == record.span_begin) {
	    bs->edge_spans.resize(record.span_begin);
	} else {
	    record.span_count = bs->edge_spans.size() - record.span_begin;
	    record.supported = true;
	}
	bs->edge_records.push_back(record);
    }
}


/* Prep-time certificate for the geometric seam gap.  For each
 * incident face it bounds the symmetric Hausdorff separation between the
 * shared 3D edge locus and the surface-lifted trim locus.  Positive rational
 * Bezier control hulls bound both loci relative to endpoint chords.  A
 * complete strict INSIDE result authorizes the edge corridor only after the
 * global parameter correspondence and both topological endpoint contracts
 * are also certified; unavailable, ambiguous, outside, and exhausted cases
 * remain on fallback. */
struct brep_discrepancy_cell {
    ON_BezierCurve edge_curve;
    ON_Interval edge_domain;
    double trim_parameter[2] = {0.0, 0.0};
    size_t edge_span = 0;
    size_t depth = 0;
};


static ON_4dPoint
brep_lerp_homogeneous(const ON_4dPoint &first, const ON_4dPoint &second,
    double fraction)
{
    const double complement = 1.0 - fraction;
    return ON_4dPoint(complement * first.x + fraction * second.x,
	complement * first.y + fraction * second.y,
	complement * first.z + fraction * second.z,
	complement * first.w + fraction * second.w);
}


static bool
brep_split_homogeneous(const ON_4dPoint *input, int order, double parameter,
    ON_4dPoint *left, ON_4dPoint *right)
{
    if (!input || !left || !right || order < 2 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER || !(parameter >= 0.0) ||
	    !(parameter <= 1.0))
	return false;
    ON_4dPoint work[BREP_DIRECT_BEZIER_MAX_ORDER] = {};
    for (int i = 0; i < order; ++i)
	work[i] = input[i];
    left[0] = work[0];
    right[order - 1] = work[order - 1];
    for (int level = 1; level < order; ++level) {
	for (int i = 0; i < order - level; ++i)
	    work[i] = brep_lerp_homogeneous(work[i], work[i + 1],
		parameter);
	left[level] = work[0];
	right[order - level - 1] = work[order - level - 1];
    }
    return true;
}


static bool
brep_restrict_homogeneous(const ON_4dPoint *input, int order,
    const ON_Interval &interval, ON_4dPoint *output)
{
    if (!input || !output || !interval.IsIncreasing() ||
	    interval.Min() < 0.0 || interval.Max() > 1.0)
	return false;
    if (interval.Min() <= 0.0 && interval.Max() >= 1.0) {
	for (int i = 0; i < order; ++i)
	    output[i] = input[i];
	return true;
    }
    ON_4dPoint left[BREP_DIRECT_BEZIER_MAX_ORDER];
    ON_4dPoint unused[BREP_DIRECT_BEZIER_MAX_ORDER];
    if (!brep_split_homogeneous(input, order, interval.Max(), left,
	    unused))
	return false;
    if (interval.Min() <= 0.0) {
	for (int i = 0; i < order; ++i)
	    output[i] = left[i];
	return true;
    }
    const double local_minimum = interval.Min() / interval.Max();
    return brep_split_homogeneous(left, order, local_minimum, unused,
	output);
}


static double
brep_point_segment_distance(const ON_3dPoint &point,
    const ON_3dPoint &start, const ON_3dPoint &end)
{
    const ON_3dVector chord = end - start;
    const double length_squared = chord * chord;
    if (!(length_squared > DBL_MIN) || !std::isfinite(length_squared))
	return point.DistanceTo(start);
    double fraction = ((point - start) * chord) / length_squared;
    fraction = std::max(0.0, std::min(1.0, fraction));
    return point.DistanceTo(start + fraction * chord);
}


static double
brep_segment_hausdorff_bound(const ON_3dPoint &first_start,
    const ON_3dPoint &first_end, const ON_3dPoint &second_start,
    const ON_3dPoint &second_end)
{
    return std::max(
	std::max(brep_point_segment_distance(first_start, second_start,
	    second_end),
	    brep_point_segment_distance(first_end, second_start, second_end)),
	std::max(brep_point_segment_distance(second_start, first_start,
	    first_end),
	    brep_point_segment_distance(second_end, first_start, first_end)));
}


static bool
brep_bezier_chord_deviation(const ON_BezierCurve &curve,
    const ON_3dPoint &start, const ON_3dPoint &end, double &deviation)
{
    if (curve.Dimension() != 3 || curve.CVCount() < 2)
	return false;
    deviation = 0.0;
    const int denominator = curve.CVCount() - 1;
    for (int cv_index = 0; cv_index < curve.CVCount(); ++cv_index) {
	ON_4dPoint cv;
	if (!curve.GetCV(cv_index, cv) || !cv.IsValid() ||
		!(cv.w > 0.0) || !std::isfinite(cv.w))
	    return false;
	const ON_3dPoint point(cv.x / cv.w, cv.y / cv.w, cv.z / cv.w);
	const double fraction = (double)cv_index / (double)denominator;
	const ON_3dPoint chord_point = (1.0 - fraction) * start +
	    fraction * end;
	deviation = std::max(deviation, point.DistanceTo(chord_point));
    }
    return std::isfinite(deviation);
}


static bool
brep_prepare_trim_spans(const ON_BrepTrim &trim,
    std::vector<brep_trim_span> &spans)
{
    spans.clear();
    ON_NurbsCurve nurbs;
    if (trim.GetNurbForm(nurbs) != 1 || nurbs.Dimension() != 2 ||
	    nurbs.m_order < 2 || nurbs.m_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    nurbs.m_cv_count < nurbs.m_order)
	return false;
    spans.reserve(nurbs.m_cv_count - nurbs.m_order + 1);
    for (int span_index = 0;
	    span_index <= nurbs.m_cv_count - nurbs.m_order; ++span_index) {
	const double lower =
	    nurbs.m_knot[span_index + nurbs.m_order - 2];
	const double upper =
	    nurbs.m_knot[span_index + nurbs.m_order - 1];
	if (!(lower < upper))
	    continue;
	brep_trim_span span;
	if (!nurbs.ConvertSpanToBezier(span_index, span.curve))
	    return false;
	for (int cv_index = 0; cv_index < span.curve.CVCount(); ++cv_index) {
	    ON_4dPoint cv;
	    if (!span.curve.GetCV(cv_index, cv) || !cv.IsValid() ||
		    !(cv.w > 0.0) || !std::isfinite(cv.w))
		return false;
	}
	span.trim_domain = ON_Interval(lower, upper);
	spans.push_back(span);
    }
    return !spans.empty();
}


static bool
brep_trim_spans_cover(const std::vector<brep_trim_span> &spans,
    size_t span_begin, size_t span_count, const ON_Interval &domain)
{
    if (!domain.IsIncreasing() || span_begin > spans.size() ||
	span_count > spans.size() - span_begin || !span_count)
	return false;
    const double domain_length = domain.Length();
    if (!(domain_length > 0.0) || !std::isfinite(domain_length))
	return false;
    for (size_t span_offset = 0; span_offset < span_count; ++span_offset) {
	const ON_Interval &span_domain =
	    spans[span_begin + span_offset].trim_domain;
	if (!span_domain.IsIncreasing() ||
		!std::isfinite(span_domain.Min()) ||
		!std::isfinite(span_domain.Max()))
	    return false;
    }
    /* Grow the union prefix by the farthest reachable endpoint.  Summed
     * lengths are not a coverage proof because overlap can conceal a gap.
     * Span boundaries originate in the same retained knot vector, so require
     * an exact closed union rather than turning parameter roundoff into
     * geometric authority. */
    double coverage_end = domain.Min();
    for (size_t step = 0;
	    step < span_count && coverage_end < domain.Max(); ++step) {
	double next_end = coverage_end;
	for (size_t span_offset = 0; span_offset < span_count; ++span_offset) {
	    const ON_Interval &span_domain =
		spans[span_begin + span_offset].trim_domain;
	    const double lower = std::max(domain.Min(), span_domain.Min());
	    const double upper = std::min(domain.Max(), span_domain.Max());
	    if (lower <= coverage_end && upper > next_end)
		next_end = upper;
	}
	if (!(next_end > coverage_end))
	    return false;
	coverage_end = next_end;
    }
    return coverage_end >= domain.Max();
}


static bool
brep_trim_segment_uv_bbox(const std::vector<brep_trim_span> &spans,
    const ON_Interval &trim_interval, ON_BoundingBox &bbox)
{
    if (!brep_trim_spans_cover(spans, 0, spans.size(), trim_interval))
	return false;
    bool grow = false;
    for (std::vector<brep_trim_span>::const_iterator span_it = spans.begin();
	    span_it != spans.end(); ++span_it) {
	const double lower = std::max(trim_interval.Min(),
	    span_it->trim_domain.Min());
	const double upper = std::min(trim_interval.Max(),
	    span_it->trim_domain.Max());
	if (!(lower < upper))
	    continue;
	const ON_Interval normalized(
	    span_it->trim_domain.NormalizedParameterAt(lower),
	    span_it->trim_domain.NormalizedParameterAt(upper));
	const int order = span_it->curve.CVCount();
	ON_4dPoint input[BREP_DIRECT_BEZIER_MAX_ORDER];
	ON_4dPoint restricted[BREP_DIRECT_BEZIER_MAX_ORDER];
	if (!normalized.IsIncreasing() || order < 2 ||
		order > BREP_DIRECT_BEZIER_MAX_ORDER)
	    return false;
	for (int cv_index = 0; cv_index < order; ++cv_index) {
	    if (!span_it->curve.GetCV(cv_index, input[cv_index]))
		return false;
	}
	if (!brep_restrict_homogeneous(input, order, normalized, restricted))
	    return false;
	for (int cv_index = 0; cv_index < order; ++cv_index) {
	    const ON_4dPoint &cv = restricted[cv_index];
	    if (!cv.IsValid() ||
		    !(cv.w > 0.0) || !std::isfinite(cv.w))
		return false;
	    const ON_3dPoint uv(cv.x / cv.w, cv.y / cv.w, 0.0);
	    if (!uv.IsValid() || !bbox.Set(uv, grow))
		return false;
	    grow = true;
	}
    }
    return grow && bbox.IsValid();
}


static const brep_face_record *
brep_prepared_face(const struct brep_specific *bs, int face_index)
{
    if (!bs)
	return NULL;
    for (std::vector<brep_face_record>::const_iterator face_it =
	    bs->face_records.begin(); face_it != bs->face_records.end();
	    ++face_it) {
	if (face_it->face_index == face_index)
	    return &*face_it;
    }
    return NULL;
}


static bool
brep_lifted_trim_chord_deviation(const struct brep_specific *bs,
    int face_index, const ON_BoundingBox &uv_bbox,
    const ON_3dPoint &start, const ON_3dPoint &end, double &deviation)
{
    const brep_face_record *face_record = brep_prepared_face(bs, face_index);
    if (!bs || !bs->brep || !face_record || !face_record->supported ||
	    face_index < 0 || face_index >= bs->brep->m_F.Count())
	return false;
    const ON_Surface *surface = bs->brep->m_F[face_index].SurfaceOf();
    if (!surface || !uv_bbox.IsValid())
	return false;

    double parameter_min[2] = {uv_bbox.m_min.x, uv_bbox.m_min.y};
    double parameter_max[2] = {uv_bbox.m_max.x, uv_bbox.m_max.y};
    for (int direction = 0; direction < 2; ++direction) {
	const ON_Interval domain = surface->Domain(direction);
	if (!domain.IsIncreasing())
	    return false;
	const double coordinate_scale = std::max(1.0,
	    std::max(fabs(domain.Min()), std::max(fabs(domain.Max()),
	    std::max(fabs(parameter_min[direction]),
		fabs(parameter_max[direction])))));
	const double padding = 256.0 * DBL_EPSILON * coordinate_scale;
	if (parameter_min[direction] < domain.Min() - padding ||
		parameter_max[direction] > domain.Max() + padding)
	    return false;
	parameter_min[direction] = std::max(domain.Min(),
	    parameter_min[direction] - padding);
	parameter_max[direction] = std::min(domain.Max(),
	    parameter_max[direction] + padding);
	if (!(parameter_min[direction] < parameter_max[direction])) {
	    if (parameter_min[direction] <= domain.Min())
		parameter_max[direction] = std::min(domain.Max(),
		    domain.Min() + padding);
	    else
		parameter_min[direction] = std::max(domain.Min(),
		    domain.Max() - padding);
	}
	if (!(parameter_min[direction] < parameter_max[direction]))
	    return false;
    }

    deviation = 0.0;
    bool bounded = false;
    for (size_t span_index = face_record->span_begin;
	    span_index < face_record->span_begin + face_record->span_count;
	    ++span_index) {
	const brep_surface_span &span = bs->surface_spans[span_index];
	double overlap_min[2];
	double overlap_max[2];
	bool overlaps = true;
	for (int direction = 0; direction < 2; ++direction) {
	    overlap_min[direction] = std::max(parameter_min[direction],
		span.surface_domain[direction].Min());
	    overlap_max[direction] = std::min(parameter_max[direction],
		span.surface_domain[direction].Max());
	    overlaps = overlaps &&
		overlap_min[direction] < overlap_max[direction];
	}
	if (!overlaps)
	    continue;
	ON_Interval normalized[2];
	for (int direction = 0; direction < 2; ++direction) {
	    normalized[direction] = ON_Interval(
		span.surface_domain[direction].NormalizedParameterAt(
		    overlap_min[direction]),
		span.surface_domain[direction].NormalizedParameterAt(
		    overlap_max[direction]));
	    if (!normalized[direction].IsIncreasing())
		return false;
	}
	const int u_order = span.surface.Order(0);
	const int v_order = span.surface.Order(1);
	if (u_order < 2 || v_order < 2 ||
		u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
		v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	    return false;
	ON_4dPoint input[BREP_DIRECT_BEZIER_MAX_CVS];
	ON_4dPoint u_restricted[BREP_DIRECT_BEZIER_MAX_CVS];
	ON_4dPoint restricted[BREP_DIRECT_BEZIER_MAX_CVS];
	for (int i = 0; i < u_order; ++i) {
	    for (int j = 0; j < v_order; ++j) {
		if (!span.surface.GetCV(i, j, input[i * v_order + j]))
		    return false;
	    }
	}
	for (int j = 0; j < v_order; ++j) {
	    ON_4dPoint curve[BREP_DIRECT_BEZIER_MAX_ORDER];
	    ON_4dPoint result[BREP_DIRECT_BEZIER_MAX_ORDER];
	    for (int i = 0; i < u_order; ++i)
		curve[i] = input[i * v_order + j];
	    if (!brep_restrict_homogeneous(curve, u_order, normalized[0],
		    result))
		return false;
	    for (int i = 0; i < u_order; ++i)
		u_restricted[i * v_order + j] = result[i];
	}
	for (int i = 0; i < u_order; ++i) {
	    if (!brep_restrict_homogeneous(&u_restricted[i * v_order],
		    v_order, normalized[1], &restricted[i * v_order]))
		return false;
	}
	for (int i = 0; i < u_order; ++i) {
	    for (int j = 0; j < v_order; ++j) {
		const ON_4dPoint &cv = restricted[i * v_order + j];
		if (!cv.IsValid() || !(cv.w > 0.0) || !std::isfinite(cv.w))
		    return false;
		const ON_3dPoint point(cv.x / cv.w, cv.y / cv.w,
		    cv.z / cv.w);
		deviation = std::max(deviation,
		    brep_point_segment_distance(point, start, end));
		bounded = true;
	    }
	}
    }
    return bounded && std::isfinite(deviation);
}


static bool
brep_discrepancy_cell_bounds(const struct brep_specific *bs,
    const ON_BrepEdge &edge, const ON_BrepTrim &trim,
    const std::vector<brep_trim_span> &trim_spans,
    const brep_discrepancy_cell &cell, double &lower, double &upper)
{
    const ON_3dPoint edge_start = cell.edge_curve.PointAt(0.0);
    const ON_3dPoint edge_end = cell.edge_curve.PointAt(1.0);
    ON_3dPoint lift_start;
    ON_3dPoint lift_end;
    if (!edge_start.IsValid() || !edge_end.IsValid() ||
	    !brep_trim_lift_at(trim, cell.trim_parameter[0], lift_start, NULL) ||
	    !brep_trim_lift_at(trim, cell.trim_parameter[1], lift_end, NULL))
	return false;

    double edge_deviation = 0.0;
    if (!brep_bezier_chord_deviation(cell.edge_curve, edge_start, edge_end,
	    edge_deviation))
	return false;
    const ON_Interval trim_interval(
	std::min(cell.trim_parameter[0], cell.trim_parameter[1]),
	std::max(cell.trim_parameter[0], cell.trim_parameter[1]));
    ON_BoundingBox uv_bbox;
    if (!brep_trim_segment_uv_bbox(trim_spans, trim_interval, uv_bbox))
	return false;
    double lift_deviation = 0.0;
    if (!brep_lifted_trim_chord_deviation(bs, trim.FaceIndexOf(), uv_bbox,
	    lift_start, lift_end, lift_deviation))
	return false;

    const double chord_distance = brep_segment_hausdorff_bound(edge_start,
	edge_end, lift_start, lift_end);
    const double deviation = edge_deviation + lift_deviation;
    lower = std::max(0.0, chord_distance - deviation);
    upper = chord_distance + deviation;
    return std::isfinite(lower) && std::isfinite(upper) && lower <= upper &&
	cell.edge_domain.IsIncreasing() && edge.m_edge_index == trim.m_ei;
}


/* Nearest-point correspondence is not required to map a tolerance-displaced
 * lifted trim endpoint back to the edge-domain endpoint.  Establish those two
 * boundary pairs from BREP topology instead: orientation must select the same
 * vertex, and both the 3-D edge endpoint and surface lift must satisfy that
 * specific vertex's declared/model tolerance. */
static bool
brep_certify_edge_trim_endpoints(const struct brep_specific *bs,
    const brep_edge_record &record, const ON_BrepEdge &edge,
    const ON_BrepTrim &trim, double trim_parameter[2])
{
    if (!bs || !bs->brep || !trim_parameter ||
	    edge.m_edge_index != record.edge_index ||
	    trim.m_ei != edge.m_edge_index || !edge.Domain().IsIncreasing() ||
	    !trim.Domain().IsIncreasing())
	return false;
    const ON_Brep &brep = *bs->brep;
    const ON_Interval edge_domain = edge.Domain();
    const ON_Interval trim_domain = trim.Domain();
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	const int trim_endpoint = trim.m_bRev3d ? 1 - endpoint : endpoint;
	const int vertex_index = edge.m_vi[endpoint];
	if (vertex_index < 0 || vertex_index >= brep.m_V.Count() ||
		trim.m_vi[trim_endpoint] != vertex_index)
	    return false;
	const ON_BrepVertex &vertex = brep.m_V[vertex_index];
	const double vertex_tolerance =
	    ON_IsValid(vertex.m_tolerance) && vertex.m_tolerance >= 0.0 ?
	    std::max(record.model_tolerance, vertex.m_tolerance) :
	    record.model_tolerance;
	if (!ON_IsValid(vertex_tolerance) || vertex_tolerance < 0.0)
	    return false;
	trim_parameter[endpoint] = trim_endpoint ? trim_domain.Max() :
	    trim_domain.Min();
	const ON_3dPoint edge_point = edge.PointAt(endpoint ?
	    edge_domain.Max() : edge_domain.Min());
	ON_3dPoint trim_lift;
	if (!vertex.point.IsValid() || !edge_point.IsValid() ||
		!brep_trim_lift_at(trim, trim_parameter[endpoint], trim_lift,
		    NULL))
	    return false;
	const double coordinate_scale = std::max(1.0,
	    std::max(fabs(vertex.point.x), std::max(fabs(vertex.point.y),
	    std::max(fabs(vertex.point.z), std::max(fabs(edge_point.x),
	    std::max(fabs(edge_point.y), std::max(fabs(edge_point.z),
	    std::max(fabs(trim_lift.x), std::max(fabs(trim_lift.y),
		fabs(trim_lift.z))))))))));
	const double roundoff = std::max(ON_ZERO_TOLERANCE,
	    512.0 * DBL_EPSILON * coordinate_scale);
	if (vertex.point.DistanceTo(edge_point) >
		vertex_tolerance + roundoff ||
	    vertex.point.DistanceTo(trim_lift) >
		vertex_tolerance + roundoff)
	    return false;
    }
    return true;
}


static bool
brep_bound_edge_trim_discrepancy(const struct brep_specific *bs,
    const brep_edge_record &record, const ON_BrepEdge &edge,
    const ON_BrepTrim &trim, const double certified_endpoint[2],
    double envelope, double target_width,
    size_t cell_budget, double &lower, double &upper, int &proof_class,
    size_t &cell_count, size_t &maximum_depth, bool &exhausted,
    std::vector<brep_edge_trim_cell> &bounded_cells,
    std::vector<brep_trim_span> &bounded_spans)
{
    const size_t maximum_subdivision_depth = 24;
    exhausted = false;
    proof_class = RT_BREP_SEAM_GAP_UNAVAILABLE;
    bounded_spans.clear();
    if (!brep_prepare_trim_spans(trim, bounded_spans))
	return false;
    std::vector<brep_discrepancy_cell> stack;
    stack.reserve(128);
    const ON_Interval edge_domain = edge.Domain();
    const ON_Interval trim_domain = trim.Domain();
    if (!edge_domain.IsIncreasing() || !trim_domain.IsIncreasing())
	return false;
    const double edge_parameter_tolerance = 512.0 * DBL_EPSILON *
	std::max(1.0, std::max(fabs(edge_domain.Min()),
	    fabs(edge_domain.Max())));
    const double parameter_tolerance = 512.0 * DBL_EPSILON *
	std::max(1.0, std::max(fabs(trim_domain.Min()),
	    fabs(trim_domain.Max())));
    if (!certified_endpoint || !std::isfinite(certified_endpoint[0]) ||
	    !std::isfinite(certified_endpoint[1]))
	return false;
    const double expected_start = certified_endpoint[0];
    const double expected_end = certified_endpoint[1];
    double previous_end = expected_start;
    for (size_t span_index = record.span_begin;
	    span_index < record.span_begin + record.span_count; ++span_index) {
	const brep_edge_span &span = bs->edge_spans[span_index];
	brep_discrepancy_cell cell;
	cell.edge_curve = span.curve;
	cell.edge_domain = span.edge_domain;
	cell.edge_span = span_index;
	cell.trim_parameter[0] = previous_end;
	const bool final_span = span_index + 1 ==
	    record.span_begin + record.span_count;
	if (final_span) {
	    cell.trim_parameter[1] = expected_end;
	} else {
	    const ON_Interval remaining_trim(
		std::min(previous_end, expected_end),
		std::max(previous_end, expected_end));
	    if (!brep_edge_trim_parameter_in_domain(edge, trim,
		    cell.edge_domain.Max(), remaining_trim, 33,
		    cell.trim_parameter[1]))
		return false;
	}
	const double directed_span = trim.m_bRev3d ?
	    cell.trim_parameter[0] - cell.trim_parameter[1] :
	    cell.trim_parameter[1] - cell.trim_parameter[0];
	if (!(directed_span > parameter_tolerance))
	    return false;
	previous_end = cell.trim_parameter[1];
	stack.push_back(cell);
    }
    if (stack.empty() ||
	    fabs(stack.front().edge_domain.Min() - edge_domain.Min()) >
		edge_parameter_tolerance ||
	    fabs(stack.back().edge_domain.Max() - edge_domain.Max()) >
		edge_parameter_tolerance ||
	    fabs(stack.front().trim_parameter[0] -
	    expected_start) > parameter_tolerance ||
	    fabs(previous_end - expected_end) > parameter_tolerance)
	return false;

    lower = 0.0;
    upper = 0.0;
    cell_count = 0;
    maximum_depth = 0;
    bool ambiguous = false;
    bool outside = false;
    const auto retain_cell = [&](const brep_discrepancy_cell &cell) {
	brep_edge_trim_cell retained;
	retained.edge_domain = cell.edge_domain;
	retained.trim_domain = ON_Interval(
	    std::min(cell.trim_parameter[0], cell.trim_parameter[1]),
	    std::max(cell.trim_parameter[0], cell.trim_parameter[1]));
	retained.edge_span = cell.edge_span;
	retained.trim_span_count = bounded_spans.size();
	retained.trim_index = trim.m_trim_index;
	bounded_cells.push_back(retained);
    };
    while (!stack.empty()) {
	if (cell_count >= cell_budget) {
	    exhausted = true;
	    return false;
	}
	brep_discrepancy_cell cell = stack.back();
	stack.pop_back();
	cell_count++;
	maximum_depth = std::max(maximum_depth, cell.depth);
	double cell_lower = 0.0;
	double cell_upper = 0.0;
	if (!brep_discrepancy_cell_bounds(bs, edge, trim, bounded_spans, cell,
		cell_lower, cell_upper))
	    return false;
	/* Once one cell is strictly outside, its lower bound is enough to
	 * reject seam repair.  Bound the remaining partition cells without
	 * further refinement so the reported aggregate stays conservative. */
	if (outside) {
	    retain_cell(cell);
	    lower = std::max(lower, cell_lower);
	    upper = std::max(upper, cell_upper);
	    continue;
	}
	if (cell_lower > envelope) {
	    retain_cell(cell);
	    lower = std::max(lower, cell_lower);
	    upper = std::max(upper, cell_upper);
	    outside = true;
	    continue;
	}
	if (cell_upper < envelope) {
	    retain_cell(cell);
	    lower = std::max(lower, cell_lower);
	    upper = std::max(upper, cell_upper);
	    continue;
	}
	/* Equality is deliberately not acceptance.  If a straddling interval
	 * has reached the advertised resolution, retain it as an explicit
	 * ambiguous result. */
	if (cell_upper - cell_lower <= target_width) {
	    retain_cell(cell);
	    lower = std::max(lower, cell_lower);
	    upper = std::max(upper, cell_upper);
	    ambiguous = true;
	    continue;
	}
	if (cell.depth >= maximum_subdivision_depth) {
	    retain_cell(cell);
	    lower = std::max(lower, cell_lower);
	    upper = std::max(upper, cell_upper);
	    ambiguous = true;
	    continue;
	}

	const double edge_midpoint = cell.edge_domain.Mid();
	double trim_midpoint = 0.0;
	const double parameter_min = std::min(cell.trim_parameter[0],
	    cell.trim_parameter[1]);
	const double parameter_max = std::max(cell.trim_parameter[0],
	    cell.trim_parameter[1]);
	const ON_Interval parameter_domain(parameter_min, parameter_max);
	if (!brep_edge_trim_parameter_in_domain(edge, trim, edge_midpoint,
		parameter_domain, 9, trim_midpoint)) {
	    retain_cell(cell);
	    lower = std::max(lower, cell_lower);
	    upper = std::max(upper, cell_upper);
	    ambiguous = true;
	    continue;
	}
	const double midpoint_tolerance = 256.0 * DBL_EPSILON *
	    std::max(1.0, std::max(fabs(parameter_min),
	    fabs(parameter_max)));
	if (trim_midpoint < parameter_min - midpoint_tolerance ||
		trim_midpoint > parameter_max + midpoint_tolerance) {
	    retain_cell(cell);
	    lower = std::max(lower, cell_lower);
	    upper = std::max(upper, cell_upper);
	    ambiguous = true;
	    continue;
	}
	trim_midpoint = std::max(parameter_min,
	    std::min(parameter_max, trim_midpoint));
	ON_BezierCurve left;
	ON_BezierCurve right;
	if (!cell.edge_curve.Split(0.5, left, right)) {
	    retain_cell(cell);
	    lower = std::max(lower, cell_lower);
	    upper = std::max(upper, cell_upper);
	    ambiguous = true;
	    continue;
	}
	brep_discrepancy_cell children[2];
	children[0].edge_curve = left;
	children[0].edge_domain = ON_Interval(cell.edge_domain.Min(),
	    edge_midpoint);
	children[0].trim_parameter[0] = cell.trim_parameter[0];
	children[0].trim_parameter[1] = trim_midpoint;
	children[1].edge_curve = right;
	children[1].edge_domain = ON_Interval(edge_midpoint,
	    cell.edge_domain.Max());
	children[1].trim_parameter[0] = trim_midpoint;
	children[1].trim_parameter[1] = cell.trim_parameter[1];
	children[0].edge_span = children[1].edge_span = cell.edge_span;
	children[0].depth = children[1].depth = cell.depth + 1;
	stack.push_back(children[1]);
	stack.push_back(children[0]);
    }
    if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper)
	return false;
    proof_class = outside ? RT_BREP_SEAM_GAP_OUTSIDE :
	(ambiguous ? RT_BREP_SEAM_GAP_AMBIGUOUS :
	RT_BREP_SEAM_GAP_INSIDE);
    return true;
}


static void
brep_bound_edge_discrepancies(struct brep_specific *bs)
{
    if (!bs || !bs->brep)
	return;
    bs->edge_trim_cells.clear();
    bs->edge_trim_spans.clear();
    const ON_Brep &brep = *bs->brep;
    size_t remaining_cells = BREP_SEAM_BOUND_CELL_BUDGET;
    for (std::vector<brep_edge_record>::iterator record_it =
	    bs->edge_records.begin(); record_it != bs->edge_records.end();
	    ++record_it) {
	brep_edge_record &record = *record_it;
	record.trim_cell_begin[0] = record.trim_cell_begin[1] =
	    bs->edge_trim_cells.size();
	record.trim_cell_count[0] = record.trim_cell_count[1] = 0;
	if (!record.supported || !record.correspondence_supported ||
		record.edge_index < 0 ||
		record.edge_index >= brep.m_E.Count())
	    continue;
	const ON_BrepEdge &edge = brep.m_E[record.edge_index];
	if (edge.m_ti.Count() != 2)
	    continue;
	const ON_3dPoint edge_start = edge.PointAtStart();
	const ON_3dPoint edge_end = edge.PointAtEnd();
	const double coordinate_scale = std::max(1.0,
	    std::max(fabs(edge_start.x), std::max(fabs(edge_start.y),
	    std::max(fabs(edge_start.z), std::max(fabs(edge_end.x),
	    std::max(fabs(edge_end.y), fabs(edge_end.z)))))));
	record.discrepancy_bound_tolerance = std::max(
	    4096.0 * DBL_EPSILON * coordinate_scale,
	    BREP_SEAM_BOUND_RELATIVE_TOLERANCE *
	    std::max(0.0, record.tolerance));
	if (!ON_IsValid(record.tolerance) || record.tolerance < 0.0)
	    continue;
	/* Eligible records not visited because an earlier record consumed the
	 * solid-wide budget are explicit capacity fallbacks as well. */
	if (!remaining_cells) {
	    record.discrepancy_bound_exhausted = true;
	    continue;
	}
	double total_lower = 0.0;
	double total_upper = 0.0;
	size_t total_cells = 0;
	size_t total_depth = 0;
	bool complete = true;
	size_t endpoint_certified_sides = 0;
	std::vector<brep_edge_trim_cell> retained_side_cells[2];
	std::vector<brep_trim_span> retained_side_spans[2];
	int total_class = RT_BREP_SEAM_GAP_INSIDE;
	for (int side = 0; side < 2; ++side) {
	    const int trim_index = edge.m_ti[side];
	    if (trim_index < 0 || trim_index >= brep.m_T.Count()) {
		complete = false;
		break;
	    }
	    double side_lower = 0.0;
	    double side_upper = 0.0;
	    size_t side_cells = 0;
	    size_t side_depth = 0;
	    int side_class = RT_BREP_SEAM_GAP_UNAVAILABLE;
	    bool side_exhausted = false;
	    double endpoint_parameter[2] = {0.0, 0.0};
	    const bool side_endpoints_certified =
		brep_certify_edge_trim_endpoints(bs, record, edge,
		    brep.m_T[trim_index], endpoint_parameter);
	    if (!side_endpoints_certified) {
		complete = false;
		break;
	    }
	    endpoint_certified_sides++;
	    if (!brep_bound_edge_trim_discrepancy(bs, record, edge,
		    brep.m_T[trim_index], endpoint_parameter, record.tolerance,
		    record.discrepancy_bound_tolerance, remaining_cells,
		    side_lower, side_upper, side_class, side_cells, side_depth,
		    side_exhausted, retained_side_cells[side],
		    retained_side_spans[side])) {
		total_cells += side_cells;
		total_depth = std::max(total_depth, side_depth);
		remaining_cells -= std::min(remaining_cells, side_cells);
		record.discrepancy_bound_exhausted = side_exhausted;
		complete = false;
		break;
	    }
	    total_lower = std::max(total_lower, side_lower);
	    total_upper = std::max(total_upper, side_upper);
	    total_cells += side_cells;
	    total_depth = std::max(total_depth, side_depth);
	    remaining_cells -= std::min(remaining_cells, side_cells);
	    if (side_class == RT_BREP_SEAM_GAP_OUTSIDE)
		total_class = RT_BREP_SEAM_GAP_OUTSIDE;
	    else if (side_class == RT_BREP_SEAM_GAP_AMBIGUOUS &&
		    total_class != RT_BREP_SEAM_GAP_OUTSIDE)
		total_class = RT_BREP_SEAM_GAP_AMBIGUOUS;
	}
	record.discrepancy_bound_cells = total_cells;
	record.discrepancy_bound_depth = total_depth;
	record.discrepancy_endpoints_certified =
	    endpoint_certified_sides == 2;
	if (complete) {
	    record.discrepancy_lower_bound = total_lower;
	    record.discrepancy_upper_bound = total_upper;
	    record.discrepancy_bounded = true;
	    record.discrepancy_proof_class = total_class;
	    record.discrepancy_authorized =
		total_class == RT_BREP_SEAM_GAP_INSIDE;
	    if (record.discrepancy_authorized &&
		    record.frame_interval_supported) {
		for (int side = 0; side < 2; ++side) {
		    const size_t trim_span_begin =
			bs->edge_trim_spans.size();
		    std::sort(retained_side_cells[side].begin(),
			retained_side_cells[side].end(),
			[](const brep_edge_trim_cell &first,
			    const brep_edge_trim_cell &second) {
			    return first.edge_domain.Min() <
				second.edge_domain.Min();
			});
		    for (std::vector<brep_edge_trim_cell>::iterator cell_it =
			    retained_side_cells[side].begin();
			    cell_it != retained_side_cells[side].end(); ++cell_it) {
			cell_it->trim_span_begin += trim_span_begin;
		    }
		    bs->edge_trim_spans.insert(bs->edge_trim_spans.end(),
			retained_side_spans[side].begin(),
			retained_side_spans[side].end());
		    record.trim_cell_begin[side] = bs->edge_trim_cells.size();
		    record.trim_cell_count[side] =
			retained_side_cells[side].size();
		    bs->edge_trim_cells.insert(bs->edge_trim_cells.end(),
			retained_side_cells[side].begin(),
			retained_side_cells[side].end());
		}
	    }
	}
    }
}


static const brep_edge_record *
brep_vertex_edge_record(const struct brep_specific *bs, int edge_index)
{
    if (!bs)
	return NULL;
    for (std::vector<brep_edge_record>::const_iterator record_it =
	    bs->edge_records.begin(); record_it != bs->edge_records.end();
	    ++record_it) {
	if (record_it->edge_index == edge_index)
	    return &*record_it;
    }
    return NULL;
}


static bool
brep_vertex_outgoing_tangent(const ON_Brep &brep, int vertex_index,
    int edge_index, double tolerance, ON_3dVector &outgoing)
{
    if (vertex_index < 0 || vertex_index >= brep.m_V.Count() ||
	    edge_index < 0 || edge_index >= brep.m_E.Count())
	return false;
    const ON_BrepVertex &vertex = brep.m_V[vertex_index];
    const ON_BrepEdge &edge = brep.m_E[edge_index];
    int endpoint = -1;
    if (edge.m_vi[0] == vertex_index && edge.m_vi[1] != vertex_index)
	endpoint = 0;
    else if (edge.m_vi[1] == vertex_index && edge.m_vi[0] != vertex_index)
	endpoint = 1;
    if (endpoint < 0 || !edge.Domain().IsIncreasing())
	return false;

    ON_3dPoint point;
    ON_3dVector tangent;
    const double parameter = endpoint ? edge.Domain().Max() :
	edge.Domain().Min();
    if (!edge.Ev1Der(parameter, point, tangent) || !point.IsValid() ||
	    !tangent.IsValid())
	return false;
    if (endpoint)
	tangent = -tangent;
    if (!tangent.Unitize())
	return false;
    const double coordinate_scale = std::max(1.0,
	std::max(fabs(vertex.point.x), std::max(fabs(vertex.point.y),
	fabs(vertex.point.z))));
    const double roundoff = 512.0 * DBL_EPSILON * coordinate_scale;
    if (point.DistanceTo(vertex.point) > tolerance + roundoff)
	return false;
    outgoing = tangent;
    return true;
}


/* Build the oriented spherical link of each supported closed-manifold planar
 * vertex.  Each incident face directs its arc so the local material is on the
 * left.  Requiring the directed arcs to form one cycle rejects boundary,
 * nonmanifold, disconnected, and inconsistently oriented fans at prep time.
 * Curved incident faces intentionally remain unsupported until their local
 * differential bounds are certified. */
static void
brep_build_vertex_data(struct brep_specific *bs)
{
    if (!bs)
	return;
    bs->vertex_records.clear();
    if (!bs->brep)
	return;

    const ON_Brep &brep = *bs->brep;
    bs->vertex_records.reserve(brep.m_V.Count());
    for (int vertex_index = 0; vertex_index < brep.m_V.Count();
	    ++vertex_index) {
	const ON_BrepVertex &vertex = brep.m_V[vertex_index];
	brep_vertex_record record;
	record.vertex_index = vertex_index;
	record.point = vertex.point;
	const int valence = vertex.m_ei.Count();
	if (!record.point.IsValid() || valence < 3 ||
		valence > RT_BREP_TRACE_MAX_LOCAL_ROOTS) {
	    bs->vertex_records.push_back(record);
	    continue;
	}

	std::vector<int> edge_indices(valence, -1);
	std::vector<ON_3dVector> outgoing(valence);
	bool complete = true;
	for (int incident = 0; incident < valence; ++incident) {
	    const int edge_index = vertex.m_ei[incident];
	    for (int previous = 0; previous < incident; ++previous)
		if (edge_indices[previous] == edge_index)
		    complete = false;
	    edge_indices[incident] = edge_index;
	    const brep_edge_record *edge_record =
		brep_vertex_edge_record(bs, edge_index);
	    if (!complete || !edge_record || !edge_record->supported ||
		    !edge_record->correspondence_screened ||
		    !edge_record->correspondence_supported ||
		    edge_record->correspondence_exhausted ||
		    !edge_record->discrepancy_bounded ||
		    edge_record->discrepancy_bound_exhausted ||
		    !edge_record->discrepancy_authorized ||
		    edge_record->face_index[0] < 0 ||
		    edge_record->face_index[1] < 0 ||
		    edge_record->face_index[0] == edge_record->face_index[1] ||
		    !brep_vertex_outgoing_tangent(brep, vertex_index,
			edge_index, edge_record->tolerance,
			outgoing[incident])) {
		complete = false;
		break;
	    }
	}
	if (!complete) {
	    bs->vertex_records.push_back(record);
	    continue;
	}

	struct vertex_face_link {
	    ON_3dVector outward_normal;
	    int face_index = -1;
	    int incident[2] = {-1, -1};
	    int count = 0;
	};
	std::vector<vertex_face_link> links;
	links.reserve(valence);
	for (int incident = 0; complete && incident < valence; ++incident) {
	    const brep_edge_record *edge_record =
		brep_vertex_edge_record(bs, edge_indices[incident]);
	    for (int side = 0; complete && side < 2; ++side) {
		const int face_index = edge_record->face_index[side];
		size_t link_index = 0;
		while (link_index < links.size() &&
			links[link_index].face_index != face_index)
		    ++link_index;
		if (link_index == links.size()) {
		    if (face_index < 0 || face_index >= brep.m_F.Count()) {
			complete = false;
			break;
		    }
		    const ON_BrepFace &face = brep.m_F[face_index];
		    const ON_Surface *surface = face.SurfaceOf();
		    ON_Plane plane;
		    if (!surface) {
			complete = false;
			break;
		    }
		    const ON_BoundingBox surface_bbox = surface->BoundingBox();
		    if (!surface_bbox.IsValid()) {
			complete = false;
			break;
		    }
		    const double coordinate_scale = std::max(1.0,
			std::max(fabs(surface_bbox.m_min.x),
			std::max(fabs(surface_bbox.m_min.y),
			std::max(fabs(surface_bbox.m_min.z),
			std::max(fabs(surface_bbox.m_max.x),
			std::max(fabs(surface_bbox.m_max.y),
			fabs(surface_bbox.m_max.z)))))));
		    const double planar_roundoff =
			4096.0 * DBL_EPSILON * coordinate_scale;
		    if (!surface->IsPlanar(&plane, planar_roundoff)) {
			complete = false;
			break;
		    }
		    vertex_face_link link;
		    link.face_index = face_index;
		    link.outward_normal = plane.Normal();
		    if (face.m_bRev)
			link.outward_normal = -link.outward_normal;
		    if (!link.outward_normal.Unitize()) {
			complete = false;
			break;
		    }
		    links.push_back(link);
		}
		vertex_face_link &link = links[link_index];
		if (link.count >= 2 ||
			(link.count && link.incident[0] == incident)) {
		    complete = false;
		    break;
		}
		link.incident[link.count++] = incident;
	    }
	}
	if (!complete || links.size() != (size_t)valence) {
	    bs->vertex_records.push_back(record);
	    continue;
	}

	std::vector<int> next(valence, -1);
	std::vector<int> arc_face(valence, -1);
	std::vector<ON_3dVector> arc_normal(valence);
	std::vector<double> arc_sweep(valence, 0.0);
	std::vector<int> incoming(valence, 0);
	for (size_t link_index = 0; complete && link_index < links.size();
		++link_index) {
	    const vertex_face_link &link = links[link_index];
	    if (link.count != 2) {
		complete = false;
		break;
	    }
	    const int first = link.incident[0];
	    const int second = link.incident[1];
	    if (fabs(link.outward_normal * outgoing[first]) > 1.0e-8 ||
		    fabs(link.outward_normal * outgoing[second]) > 1.0e-8) {
		complete = false;
		break;
	    }
	    int loop_incoming = -1;
	    int loop_outgoing = -1;
	    const int face_incident[2] = {first, second};
	    for (int side = 0; side < 2; ++side) {
		const int incident = face_incident[side];
		const ON_BrepEdge &edge = brep.m_E[edge_indices[incident]];
		const ON_BrepTrim *face_trim = NULL;
		for (int trim_side = 0; trim_side < edge.m_ti.Count();
			++trim_side) {
		    const int trim_index = edge.m_ti[trim_side];
		    if (trim_index < 0 || trim_index >= brep.m_T.Count() ||
			    brep.m_T[trim_index].FaceIndexOf() !=
			    link.face_index)
			continue;
		    if (face_trim) {
			complete = false;
			break;
		    }
		    face_trim = &brep.m_T[trim_index];
		}
		if (!complete || !face_trim ||
			face_trim->m_vi[0] == face_trim->m_vi[1]) {
		    complete = false;
		    break;
		}
		if (face_trim->m_vi[1] == vertex_index)
		    loop_incoming = incident;
		else if (face_trim->m_vi[0] == vertex_index)
		    loop_outgoing = incident;
		else
		    complete = false;
	    }
	    if (!complete || loop_incoming < 0 || loop_outgoing < 0 ||
		    loop_incoming == loop_outgoing) {
		complete = false;
		break;
	    }
	    int from = loop_incoming;
	    int to = loop_outgoing;
	    if (brep.m_F[link.face_index].m_bRev)
		std::swap(from, to);
	    const double sine = link.outward_normal *
		ON_CrossProduct(outgoing[from], outgoing[to]);
	    const double cosine = outgoing[from] * outgoing[to];
	    if (!std::isfinite(sine) || !std::isfinite(cosine) ||
		    fabs(sine) <= 1.0e-8) {
		complete = false;
		break;
	    }
	    double sweep = -atan2(sine, cosine);
	    if (sweep <= 0.0)
		sweep += 2.0 * ON_PI;
	    if (!std::isfinite(sweep) || sweep <= 1.0e-8 ||
		    sweep >= 2.0 * ON_PI - 1.0e-8) {
		complete = false;
		break;
	    }
	    if (next[from] >= 0 || ++incoming[to] != 1) {
		complete = false;
		break;
	    }
	    next[from] = to;
	    arc_face[from] = link.face_index;
	    arc_normal[from] = link.outward_normal;
	    arc_sweep[from] = sweep;
	}
	for (int incident = 0; complete && incident < valence; ++incident)
	    if (next[incident] < 0 || incoming[incident] != 1)
		complete = false;
	if (!complete) {
	    bs->vertex_records.push_back(record);
	    continue;
	}

	std::vector<bool> visited(valence, false);
	int current = 0;
	for (int arc_index = 0; arc_index < valence; ++arc_index) {
	    if (current < 0 || current >= valence || visited[current]) {
		complete = false;
		break;
	    }
	    visited[current] = true;
	    brep_vertex_arc arc;
	    arc.outgoing = outgoing[current];
	    arc.outward_normal = arc_normal[current];
	    arc.clockwise_sweep = arc_sweep[current];
	    arc.edge_index = edge_indices[current];
	    arc.face_index = arc_face[current];
	    record.arcs.push_back(arc);
	    current = next[current];
	}
	if (!complete || current != 0 ||
		record.arcs.size() != (size_t)valence) {
	    record.arcs.clear();
	    bs->vertex_records.push_back(record);
	    continue;
	}
	record.planar = true;
	record.supported = true;
	bs->vertex_records.push_back(record);
    }
}


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
	    SurfaceTree* st = new SurfaceTree(&faces[index], true,
		bbbp->depth_limit);
	    bbbp->faces[index] = st;
	}

	/* iterate until there is no more work left */
    } while (index != -1);
}


static void
brep_accumulate_surface_tree_stats(struct brep_specific *bs,
    const BBNode *node, int depth)
{
    if (!bs || !node)
	return;
    bs->surface_tree_nodes++;
    bs->surface_tree_maximum_depth = std::max(
	bs->surface_tree_maximum_depth, depth);
    const std::vector<BBNode *> &children = node->get_children();
    if (children.empty()) {
	bs->surface_tree_leaves++;
	return;
    }
    for (std::vector<BBNode *>::const_iterator child = children.begin();
	    child != children.end(); ++child)
	brep_accumulate_surface_tree_stats(bs, *child, depth + 1);
}


static void
brep_collect_bvh_stats(struct brep_specific *bs)
{
    if (!bs)
	return;
    bs->surface_tree_maximum_depth = 0;
    bs->surface_tree_nodes = 0;
    bs->surface_tree_leaves = 0;
    bs->curve_tree_leaves = 0;
    if (bs->bvh) {
	const std::vector<BBNode *> &roots = bs->bvh->get_children();
	for (std::vector<BBNode *>::const_iterator root = roots.begin();
		root != roots.end(); ++root)
	    brep_accumulate_surface_tree_stats(bs, *root, 0);
    }
    for (std::vector<const CurveTree *>::const_iterator tree =
	    bs->ctrees.begin(); tree != bs->ctrees.end(); ++tree) {
	if (!*tree)
	    continue;
	std::list<const BRNode *> leaves;
	(*tree)->getLeaves(leaves);
	bs->curve_tree_leaves += leaves.size();
    }
}


static int
brep_build_bvh(struct brep_specific* bs, int depth_limit)
{
    // First, run the openNURBS validity check on the brep in question
    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    //int64_t start;

    if (brep == NULL) {
	bu_log("NULL Brep");
	return -1;
    }
    if (depth_limit < 0 || depth_limit > BREP_MAX_FT_DEPTH)
	return -1;
    const int64_t build_start = bu_gettime();
    bs->surface_tree_depth_limit = depth_limit;

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
    bbbp.depth_limit = depth_limit;

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

    bool complete = true;
    for (size_t i = 0; i < faceCount; ++i) {
	SurfaceTree *st = bbbp.faces[i];
	if (!st || !st->Valid())
	    complete = false;
    }
    if (!complete) {
	for (size_t i = 0; i < faceCount; ++i)
	    delete bbbp.faces[i];
	bu_free(bbbp.faces, "free incomplete face array");
	return -1;
    }

    bs->ctrees.reserve(faceCount);
    for (int i = 0; (size_t)i < faceCount; i++) {
	SurfaceTree *st = bbbp.faces[i];
	bs->ctrees.push_back(st->releaseCurveTree());
	bs->bvh->addChild(st->releaseRootNode());
	delete st;
    }
    //bu_log("!!! PREP FACES: %.2f sec\n", (bu_gettime() - start) / 1000000.0);

    bu_free(bbbp.faces, "free face array");

    bs->bvh->BuildBBox();
    brep_collect_bvh_stats(bs);
    const int64_t elapsed = bu_gettime() - build_start;
    bs->surface_tree_build_microseconds = elapsed > 0 ?
	(size_t)elapsed : 0;
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
_rt_brep_set_surface_tree_depth(struct rt_i *rtip, int depth_limit)
{
    if (!rtip || !rtip->i || !rtip->needprep || depth_limit < 0 ||
	    depth_limit > BREP_MAX_FT_DEPTH)
	return 0;
    rtip->i->rti_brep_surface_tree_depth = depth_limit;
    return 1;
}


int
_rt_brep_prep_stats(const struct soltab *stp,
    struct rt_brep_prep_stats *result)
{
    if (!stp || !result || stp->st_id != ID_BREP || !stp->st_specific)
	return 0;
    const struct brep_specific *bs =
	(const struct brep_specific *)stp->st_specific;
    result->surface_tree_depth_limit = bs->surface_tree_depth_limit;
    result->surface_tree_maximum_depth = bs->surface_tree_maximum_depth;
    result->surface_tree_nodes = bs->surface_tree_nodes;
    result->surface_tree_leaves = bs->surface_tree_leaves;
    result->curve_trees = bs->ctrees.size();
    result->curve_tree_leaves = bs->curve_tree_leaves;
    result->surface_tree_build_microseconds =
	bs->surface_tree_build_microseconds;
    return 1;
}


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

    brep_build_edge_data(bs, tol);
    brep_build_surface_data(bs);
    brep_certify_edge_correspondences(bs);
    brep_bound_edge_discrepancies(bs);
    brep_build_vertex_data(bs);

    //start = bu_gettime();
    /* do the majority of real work here */
    const int depth_limit = rtip && rtip->i ?
	rtip->i->rti_brep_surface_tree_depth :
	RT_BREP_DEFAULT_SURFACE_TREE_DEPTH;
    if (brep_build_bvh(bs, depth_limit) < 0) {
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


enum brep_solver_status_t {
    BREP_SOLVER_NO_ROOT = 0,
    BREP_SOLVER_CONVERGED_REGULAR,
    BREP_SOLVER_CONVERGED_SINGULAR,
    BREP_SOLVER_DUPLICATE_ROOT,
    BREP_SOLVER_OUTSIDE_DOMAIN,
    BREP_SOLVER_JACOBIAN_SINGULAR,
    BREP_SOLVER_STALLED,
    BREP_SOLVER_ITERATION_LIMIT,
    BREP_SOLVER_EVALUATION_FAILED,
    BREP_SOLVER_NONFINITE,
    BREP_SOLVER_CAPACITY_EXHAUSTED
};

static_assert(RT_BREP_TRACE_SOLVER_STATUS_COUNT ==
    BREP_SOLVER_CAPACITY_EXHAUSTED + 1,
    "BREP trace solver status count is stale");


struct brep_solver_result {
    int intersections;
    brep_solver_status_t status;

    brep_solver_result(int count = 0, brep_solver_status_t reason = BREP_SOLVER_NO_ROOT) :
	intersections(count), status(reason)
    {
    }

    bool converged() const
    {
	return status == BREP_SOLVER_CONVERGED_REGULAR ||
	    status == BREP_SOLVER_CONVERGED_SINGULAR ||
	    status == BREP_SOLVER_DUPLICATE_ROOT;
    }
};


static bool
utah_finite(double value)
{
    return std::isfinite(value);
}


static bool
utah_finite(const ON_2dPoint &point)
{
    return utah_finite(point.x) && utah_finite(point.y);
}


static bool
utah_finite(const ON_3dPoint &point)
{
    return utah_finite(point.x) && utah_finite(point.y) && utah_finite(point.z);
}


static bool
utah_finite(const ON_3dVector &vector)
{
    return utah_finite(vector.x) && utah_finite(vector.y) && utah_finite(vector.z);
}


/* This remains tighter than the default librt model tolerance.  It is only a
 * numerical root test; trim and solid acceptance use separate logic. */
static const double BREP_ROOT_TOL = 1.0e-7;
static const double BREP_ROOT_RESIDUAL_FLOOR = 1.0e-13;


static void
utah_F(const ON_3dPoint &S, const ON_3dPoint &ray_origin,
    const ON_3dVector &p1, const ON_3dVector &p2, double &f1, double &f2)
{
    const ON_3dVector relative_point(S - ray_origin);
    f1 = relative_point * p1;
    f2 = relative_point * p2;
}


static bool
utah_root_normal(const ON_Surface *surf, const ON_2dPoint &uv,
    const ON_3dPoint &S, const ON_3dVector &Su, const ON_3dVector &Sv,
    ON_3dVector &normal, bool &regular_normal)
{
    normal = ON_CrossProduct(Su, Sv);
    regular_normal = utah_finite(normal) && normal.Unitize();
    if (regular_normal)
	return true;

    ON_3dPoint normal_point;
    if (!surface_EvNormal(surf, uv.x, uv.y, normal_point, normal) ||
	    !utah_finite(normal_point) || !utah_finite(normal) ||
	    !normal.Unitize())
	return false;

    /* A one-sided normal may evaluate at a nearby point.  S is retained as
     * the positional root; the normal is used only for conditioning and hit
     * orientation. */
    (void)S;
    return true;
}


static bool
utah_root_converged(const ON_Surface *surf, const ON_Ray &r,
    const ON_2dPoint &uv, const ON_3dPoint &S, const ON_3dVector &Su,
    const ON_3dVector &Sv, double residual)
{
    if (!utah_finite(residual) || residual > BREP_ROOT_TOL)
	return false;

    ON_3dVector normal;
    bool regular_normal = false;
    if (!utah_root_normal(surf, uv, S, Su, Sv, normal, regular_normal))
	return false;

    const double ray_length = r.m_dir.Length();
    if (!utah_finite(ray_length) || ray_length <= ON_ZERO_TOLERANCE)
	return false;
    const double normal_dot = fabs(normal * r.m_dir) / ray_length;
    if (!utah_finite(normal_dot))
	return false;

    /* The perpendicular residual alone badly overstates convergence near a
     * tangent.  Dividing it by |N.D| is the first-order error estimate along
     * the ray, so require that estimate to meet the ordinary root tolerance.
     * The floor allows an exact/double contact to reach the later contact
     * cleanup without demanding a literal floating-point zero. */
    return residual <= std::max(BREP_ROOT_RESIDUAL_FLOOR,
	BREP_ROOT_TOL * normal_dot);
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


static brep_solver_result
utah_store_root(const BBNode *sbv, const ON_Surface *surf, const ON_Ray &r,
	ON_2dPoint *ouv, double *t, ON_3dVector *N, const int capacity,
	const int count, const ON_2dPoint &uv, const ON_3dPoint &S,
	const ON_3dVector &Su, const ON_3dVector &Sv)
{
    int ulow = (sbv->m_u.m_t[0] <= sbv->m_u.m_t[1]) ? 0 : 1;
    int vlow = (sbv->m_v.m_t[0] <= sbv->m_v.m_t[1]) ? 0 : 1;
    if (!((sbv->m_u.m_t[ulow] - VUNITIZE_TOL < uv.x && uv.x < sbv->m_u.m_t[1 - ulow] + VUNITIZE_TOL) &&
	    (sbv->m_v.m_t[vlow] - VUNITIZE_TOL < uv.y && uv.y < sbv->m_v.m_t[1 - vlow] + VUNITIZE_TOL)))
	return brep_solver_result(0, BREP_SOLVER_OUTSIDE_DOMAIN);

    for (int j = 0; j < count; j++) {
	if (NEAR_EQUAL(uv.x, ouv[j].x, VUNITIZE_TOL) && NEAR_EQUAL(uv.y, ouv[j].y, VUNITIZE_TOL))
	    return brep_solver_result(0, BREP_SOLVER_DUPLICATE_ROOT);
    }

    if (count >= capacity)
	return brep_solver_result(0, BREP_SOLVER_CAPACITY_EXHAUSTED);

    double root_t = utah_calc_t(r, S);
    if (!utah_finite(root_t))
	return brep_solver_result(0, BREP_SOLVER_NONFINITE);

    ON_3dVector normal;
    bool regular_normal = false;
    if (!utah_root_normal(surf, uv, S, Su, Sv, normal, regular_normal))
	return brep_solver_result(0, BREP_SOLVER_EVALUATION_FAILED);

    t[count] = root_t;
    N[count] = normal;
    ouv[count] = uv;
    return brep_solver_result(1, regular_normal ? BREP_SOLVER_CONVERGED_REGULAR : BREP_SOLVER_CONVERGED_SINGULAR);
}


static brep_solver_result
utah_newton_solver(const BBNode* sbv, const ON_Surface* surf, const ON_Ray& r,
	ON_2dPoint* ouv, double* t, ON_3dVector* N, const int capacity,
	ON_2dPoint* suv, const int count, const int iu, const int iv)
{
    int i = 0;
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

    if (!surf->EvPoint(uv.x, uv.y, S) || !utah_finite(uv) || !utah_finite(S))
	return brep_solver_result(0, BREP_SOLVER_EVALUATION_FAILED);
    utah_F(S, r.m_origin, p1, p2, f, g);
    rootdist = hypot(f, g);
    if (!utah_finite(rootdist))
	return brep_solver_result(0, BREP_SOLVER_NONFINITE);

    /* Position is authoritative for convergence.  In particular, an exact
     * root at a pole can have a singular derivative/Jacobian. */
    if (rootdist < BREP_ROOT_TOL) {
	ON_3dPoint derivative_point;
	ON_3dVector derivative_u;
	ON_3dVector derivative_v;
	if (surf->Ev1Der(uv.x, uv.y, derivative_point, derivative_u, derivative_v) &&
		utah_finite(derivative_point) && utah_finite(derivative_u) &&
		utah_finite(derivative_v) && utah_root_converged(surf, r, uv,
		    derivative_point, derivative_u, derivative_v, rootdist))
	    return utah_store_root(sbv, surf, r, ouv, t, N, capacity, count, uv,
		derivative_point, derivative_u, derivative_v);
	if (utah_root_converged(surf, r, uv, S, Su, Sv, rootdist))
	    return utah_store_root(sbv, surf, r, ouv, t, N, capacity, count,
		uv, S, Su, Sv);
    }

    if (!surf->Ev1Der(uv.x, uv.y, S, Su, Sv) ||
	    !utah_finite(S) || !utah_finite(Su) || !utah_finite(Sv))
	return brep_solver_result(0, BREP_SOLVER_EVALUATION_FAILED);

    for (i = 0; i < BREP_MAX_ITERATIONS; i++) {
	utah_Fu(Su, p1, p2, j11, j21);
	utah_Fv(Sv, p1, p2, j12, j22);

	J = (j11 * j22 - j12 * j21);

	double ucol = hypot(j11, j21);
	double vcol = hypot(j12, j22);
	if (!utah_finite(J) || !utah_finite(ucol) || !utah_finite(vcol))
	    return brep_solver_result(0, BREP_SOLVER_NONFINITE);
	if (ucol <= ON_ZERO_TOLERANCE || vcol <= ON_ZERO_TOLERANCE ||
		fabs(J) <= BREP_INTERSECTION_ROOT_EPSILON * ucol * vcol)
	    return brep_solver_result(0, BREP_SOLVER_JACOBIAN_SINGULAR);

	invdetJ = 1.0 / J;

	if ((iu != -1) && (iv != -1)) {
	    du = -invdetJ * (j22 * f - j12 * g);
	    dv = -invdetJ * (j11 * g - j21 * f);

	    if (i == 0) {
		if (((iu == 0) && (du < 0.0)) || ((iu == 1) && (du > 0.0)))
		    return brep_solver_result(0, BREP_SOLVER_OUTSIDE_DOMAIN);
		if (((iv == 0) && (dv < 0.0)) || ((iv == 1) && (dv > 0.0)))
		    return brep_solver_result(0, BREP_SOLVER_OUTSIDE_DOMAIN);
	    }
	}

	du = invdetJ * (j22 * f - j12 * g);
	dv = invdetJ * (j11 * g - j21 * f);
	if (!utah_finite(du) || !utah_finite(dv))
	    return brep_solver_result(0, BREP_SOLVER_NONFINITE);


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

	if (!surf->Ev1Der(uv.x, uv.y, S, Su, Sv) ||
		!utah_finite(S) || !utah_finite(Su) || !utah_finite(Sv))
	    return brep_solver_result(0, BREP_SOLVER_EVALUATION_FAILED);
	utah_F(S, r.m_origin, p1, p2, f, g);
	oldrootdist = rootdist;
	rootdist = hypot(f, g);
	if (!utah_finite(rootdist))
	    return brep_solver_result(0, BREP_SOLVER_NONFINITE);
	int halve_count = 0;

	/* iterate at most 3 times just because. might be worth trying
	 * additional depths or refining adaptively.
	 */
	while ((halve_count++ < 3) && (oldrootdist < rootdist)) {
	    // divide current UV step
	    uv.x = (puv.x + uv.x) / 2.0;
	    uv.y = (puv.y + uv.y) / 2.0;

	    utah_pushBack(sbv, uv);

	    if (!surf->Ev1Der(uv.x, uv.y, S, Su, Sv) ||
		    !utah_finite(S) || !utah_finite(Su) || !utah_finite(Sv))
		return brep_solver_result(0, BREP_SOLVER_EVALUATION_FAILED);
	    utah_F(S, r.m_origin, p1, p2, f, g);
	    rootdist = hypot(f, g);
	    if (!utah_finite(rootdist))
		return brep_solver_result(0, BREP_SOLVER_NONFINITE);
	}

	if (oldrootdist <= rootdist) {

	    /* if we're not getting any better after 3 tries, give up
	     * and return what was found.  no particular reason for 3.
	     */
	    if (errantcount > 3) {
		return brep_solver_result(0, BREP_SOLVER_STALLED);
	    } else {
		errantcount++;
	    }
	}

	if (utah_root_converged(surf, r, uv, S, Su, Sv, rootdist))
	    return utah_store_root(sbv, surf, r, ouv, t, N, capacity, count, uv, S, Su, Sv);
    }
    return brep_solver_result(0, BREP_SOLVER_ITERATION_LIMIT);
}


static brep_solver_result
utah_newton_4corner_solver(const BBNode* sbv, const ON_Surface* surf,
	const ON_Ray& r, ON_2dPoint* ouv, double* t, ON_3dVector* N,
	const int capacity, int docorners)
{
    int intersects = 0;
    brep_solver_status_t status = BREP_SOLVER_NO_ROOT;
    if (docorners) {
	for (int iu = 0; iu < 2; iu++) {
	    for (int iv = 0; iv < 2; iv++) {
		ON_2dPoint uv;
		uv.x = sbv->m_u[iu];
		uv.y = sbv->m_v[iv];
		brep_solver_result result = utah_newton_solver(sbv, surf, r, ouv, t, N,
		    capacity, &uv, intersects, iu, iv);
		intersects += result.intersections;
		if (result.converged() || status == BREP_SOLVER_NO_ROOT)
		    status = result.status;
	    }
	}
    }

    ON_2dPoint uv;
    uv.x = sbv->m_u.Mid();
    uv.y = sbv->m_v.Mid();
    brep_solver_result result = utah_newton_solver(sbv, surf, r, ouv, t, N,
	capacity, &uv, intersects, -1, -1);
    intersects += result.intersections;
    if (result.converged() || status == BREP_SOLVER_NO_ROOT)
	status = result.status;
    return brep_solver_result(intersects, status);
}


static int
brep_initial_hit_class(int trim_status, double trim_distance)
{
    if (trim_status != 1)
	return fabs(trim_distance) < BREP_EDGE_MISS_TOLERANCE ?
	    brep_hit::NEAR_HIT : brep_hit::CLEAN_HIT;
    return fabs(trim_distance) < BREP_EDGE_MISS_TOLERANCE ?
	brep_hit::NEAR_MISS : brep_hit::CLEAN_MISS;
}


static void
brep_trace_root(struct rt_brep_shot_trace *trace, const ON_BrepFace *face,
    double dist, const ON_2dPoint &uv, const ON_3dVector &surface_normal,
    const ON_Ray &ray, int trim_status, double trim_distance,
    const BRNode *trim_node, int hit_class)
{
    if (!trace)
	return;
    trace->candidate_roots++;
    if (trace->stored_roots >= RT_BREP_TRACE_MAX_ROOTS) {
	trace->root_overflow++;
	return;
    }
    struct rt_brep_trace_root &root = trace->roots[trace->stored_roots++];
    ON_3dVector normal(surface_normal);
    if (face->m_bRev)
	normal.Reverse();
    root.dist = dist;
    root.uv[0] = uv.x;
    root.uv[1] = uv.y;
    root.normal_dot = normal * ray.m_dir;
    root.trim_distance = trim_distance;
    root.face_index = face->m_face_index;
    root.adjacent_face_index = trim_node ? trim_node->m_adj_face_index : -99;
    root.trim_status = trim_status;
    root.hit_class = hit_class;
    root.direction = root.normal_dot < 0.0 ? brep_hit::ENTERING :
	brep_hit::LEAVING;
}


template <typename CurveType>
static bool
brep_line_curve_distance_at(const CurveType &curve, const ON_Ray &ray,
    double parameter, double &distance, double &ray_dist)
{
    const double direction_squared = ray.m_dir * ray.m_dir;
    if (!(direction_squared > DBL_MIN) || !std::isfinite(direction_squared))
	return false;
    const ON_3dPoint point = curve.PointAt(parameter);
    if (!point.IsValid())
	return false;
    ON_3dVector offset = point - ray.m_origin;
    ray_dist = (offset * ray.m_dir) / direction_squared;
    offset -= ray_dist * ray.m_dir;
    distance = offset.Length();
    return std::isfinite(distance) && std::isfinite(ray_dist);
}


template <typename CurveType>
static bool
brep_line_curve_distance(const CurveType &curve, const ON_Ray &ray,
    double &parameter, double &distance, double &ray_dist)
{
    const ON_Interval domain = curve.Domain();
    if (!domain.IsIncreasing())
	return false;

    /* Each input is one prepared Bezier span.  This bounded minimization is
     * diagnostic only; a production event resolver must subdivide every
     * ambiguous span conservatively rather than assuming it is unimodal. */
    const int sample_count = 17;
    int best_sample = -1;
    distance = DBL_MAX;
    for (int sample = 0; sample < sample_count; ++sample) {
	const double fraction = (double)sample / (double)(sample_count - 1);
	const double candidate_parameter = domain.ParameterAt(fraction);
	double candidate_distance;
	double candidate_ray_dist;
	if (!brep_line_curve_distance_at(curve, ray, candidate_parameter,
		candidate_distance, candidate_ray_dist))
	    continue;
	if (candidate_distance < distance) {
	    best_sample = sample;
	    parameter = candidate_parameter;
	    distance = candidate_distance;
	    ray_dist = candidate_ray_dist;
	}
    }
    if (best_sample < 0)
	return false;

    const int lower_sample = std::max(0, best_sample - 1);
    const int upper_sample = std::min(sample_count - 1, best_sample + 1);
    double lower = domain.ParameterAt((double)lower_sample /
	(double)(sample_count - 1));
    double upper = domain.ParameterAt((double)upper_sample /
	(double)(sample_count - 1));
    const double golden = 0.6180339887498948482;
    double left = upper - golden * (upper - lower);
    double right = lower + golden * (upper - lower);
    double left_distance = DBL_MAX;
    double right_distance = DBL_MAX;
    double left_ray_dist = 0.0;
    double right_ray_dist = 0.0;
    (void)brep_line_curve_distance_at(curve, ray, left, left_distance,
	left_ray_dist);
    (void)brep_line_curve_distance_at(curve, ray, right, right_distance,
	right_ray_dist);
    for (int iteration = 0; iteration < 32; ++iteration) {
	if (left_distance <= right_distance) {
	    upper = right;
	    right = left;
	    right_distance = left_distance;
	    right_ray_dist = left_ray_dist;
	    left = upper - golden * (upper - lower);
	    left_distance = DBL_MAX;
	    (void)brep_line_curve_distance_at(curve, ray, left, left_distance,
		left_ray_dist);
	} else {
	    lower = left;
	    left = right;
	    left_distance = right_distance;
	    left_ray_dist = right_ray_dist;
	    right = lower + golden * (upper - lower);
	    right_distance = DBL_MAX;
	    (void)brep_line_curve_distance_at(curve, ray, right, right_distance,
		right_ray_dist);
	}
    }
    if (left_distance < distance) {
	parameter = left;
	distance = left_distance;
	ray_dist = left_ray_dist;
    }
    if (right_distance < distance) {
	parameter = right;
	distance = right_distance;
	ray_dist = right_ray_dist;
    }
    return std::isfinite(distance) && std::isfinite(ray_dist);
}


static bool
brep_line_intersects_box(const ON_Ray &ray, const ON_BoundingBox &bbox,
    double expansion)
{
    if (!bbox.IsValid() || !(expansion >= 0.0) || !std::isfinite(expansion))
	return false;
    double minimum_t = -DBL_MAX;
    double maximum_t = DBL_MAX;
    for (int axis = 0; axis < 3; ++axis) {
	const double lower = bbox.m_min[axis] - expansion;
	const double upper = bbox.m_max[axis] + expansion;
	if (fabs(ray.m_dir[axis]) <= DBL_MIN) {
	    if (ray.m_origin[axis] < lower || ray.m_origin[axis] > upper)
		return false;
	    continue;
	}
	double first = (lower - ray.m_origin[axis]) / ray.m_dir[axis];
	double second = (upper - ray.m_origin[axis]) / ray.m_dir[axis];
	if (first > second)
	    std::swap(first, second);
	minimum_t = std::max(minimum_t, first);
	maximum_t = std::min(maximum_t, second);
	if (minimum_t > maximum_t)
	    return false;
    }
    return true;
}


static bool
brep_ray_plane_frame(const ON_Ray &ray, ON_3dVector &first,
    ON_3dVector &second)
{
    ON_3dVector direction = ray.m_dir;
    if (!direction.Unitize())
	return false;
    const ON_3dVector axis = fabs(direction.x) <= fabs(direction.y) &&
	fabs(direction.x) <= fabs(direction.z) ? ON_3dVector(1.0, 0.0, 0.0) :
	(fabs(direction.y) <= fabs(direction.z) ?
	ON_3dVector(0.0, 1.0, 0.0) : ON_3dVector(0.0, 0.0, 1.0));
    first = ON_CrossProduct(direction, axis);
    if (!first.Unitize())
	return false;
    second = ON_CrossProduct(direction, first);
    return second.Unitize();
}


enum brep_corrector_status_t {
    BREP_CORRECTOR_INVALID_INPUT = RT_BREP_TRACE_CORRECTOR_INVALID_INPUT,
    BREP_CORRECTOR_EVALUATION_FAILED =
	RT_BREP_TRACE_CORRECTOR_EVALUATION_FAILED,
    BREP_CORRECTOR_NONFINITE = RT_BREP_TRACE_CORRECTOR_NONFINITE,
    BREP_CORRECTOR_DEGENERATE_NORMAL =
	RT_BREP_TRACE_CORRECTOR_DEGENERATE_NORMAL,
    BREP_CORRECTOR_JACOBIAN_SINGULAR =
	RT_BREP_TRACE_CORRECTOR_JACOBIAN_SINGULAR,
    BREP_CORRECTOR_NO_IMPROVEMENT =
	RT_BREP_TRACE_CORRECTOR_NO_IMPROVEMENT,
    BREP_CORRECTOR_ITERATION_LIMIT =
	RT_BREP_TRACE_CORRECTOR_ITERATION_LIMIT,
    BREP_CORRECTOR_CONVERGED = RT_BREP_TRACE_CORRECTOR_CONVERGED
};

static_assert(RT_BREP_TRACE_CORRECTOR_STATUS_COUNT ==
    BREP_CORRECTOR_CONVERGED + 1,
    "BREP trace corrector status count is stale");


struct brep_continuation_result {
    ON_2dPoint uv;
    ON_3dPoint point;
    ON_3dVector normal;
    double dist = 0.0;
    double residual = DBL_MAX;
    double acceptance_limit = 0.0;
    size_t iterations = 0;
    brep_corrector_status_t status = BREP_CORRECTOR_INVALID_INPUT;
    bool converged = false;
};


static brep_continuation_result
brep_continuation_newton(const brep_surface_span &span, const ON_Ray &ray,
    const ON_2dPoint &seed, const double minimum[2],
    const double maximum[2]);


struct brep_interval {
    double minimum;
    double maximum;
};


/* Fixed expansions preserve exact signs of binary64 coefficient expressions.
 * The capacity is a work limit; failure leaves the correspondence unsupported. */
struct brep_expansion {
    double component[RT_BREP_EXPANSION_CAPACITY];
    size_t count;
};


struct brep_expansion_interval {
    brep_expansion minimum;
    brep_expansion maximum;
};


static brep_interval brep_interval_expanded(double minimum, double maximum);
static brep_interval brep_interval_add(const brep_interval &first,
    const brep_interval &second);
static brep_interval brep_interval_scale(double scale,
    const brep_interval &value);
static brep_interval brep_interval_multiply(const brep_interval &first,
    const brep_interval &second);
static bool brep_interval_divide(const brep_interval &numerator,
    const brep_interval &denominator, brep_interval &result);
static bool brep_interval_divide_nonzero(const brep_interval &numerator,
    const brep_interval &denominator, brep_interval &result);
static bool brep_expansion_set(brep_expansion &result, double value,
    size_t &high_water);
static bool brep_expansion_add(const brep_expansion &first,
    const brep_expansion &second, brep_expansion &result,
    size_t &high_water);
static bool brep_expansion_scale(const brep_expansion &input, double scale,
    brep_expansion &result, size_t &high_water);
static bool brep_expansion_bounds(const brep_expansion &value,
    brep_interval &bounds);
static bool brep_single_coefficient_intervals(const double cv[4],
    const double origin[3], const double direction[3],
    const double planes[2][3], const brep_interval &direction_squared,
    brep_interval function[2], brep_interval &ray_coefficient);
static bool brep_single_coefficient_expansion_intervals(const double cv[4],
    const double origin[3], const double planes[2][3],
    brep_interval function[2], size_t &high_water);


static bool
brep_interval_common_error(double center, const brep_interval &interval,
    double &error)
{
    if (!std::isfinite(center) || !std::isfinite(interval.minimum) ||
	    !std::isfinite(interval.maximum) ||
	    interval.minimum > interval.maximum)
	return false;
    const double lower_error = std::nextafter(center - interval.minimum,
	INFINITY);
    const double upper_error = std::nextafter(interval.maximum - center,
	INFINITY);
    error = std::max(error, std::max(lower_error, upper_error));
    return std::isfinite(error) && error >= 0.0;
}


struct brep_surface_coefficients {
    double value[2][BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval value_interval[2][BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval value_expansion_interval[2][BREP_DIRECT_BEZIER_MAX_CVS];
    double ray_numerator[BREP_DIRECT_BEZIER_MAX_CVS];
    double weight[BREP_DIRECT_BEZIER_MAX_CVS];
    double error[2] = {0.0, 0.0};
    double ray_numerator_error = 0.0;
    double weight_error = 0.0;
    int order[2] = {0, 0};
    size_t expansion_high_water = 0;
    bool expansion_available = false;
};


static bool
brep_surface_coefficients_init(brep_surface_coefficients &coefficients,
    const brep_surface_span &span, const ON_Ray &ray,
    const ON_3dVector &first, const ON_3dVector &second)
{
    coefficients.order[0] = span.surface.Order(0);
    coefficients.order[1] = span.surface.Order(1);
    if (coefficients.order[0] < 2 || coefficients.order[1] < 2 ||
	    coefficients.order[0] > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    coefficients.order[1] > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;

    const ON_3dVector planes[2] = {first, second};
    const double direction_squared = ray.m_dir * ray.m_dir;
    if (!(direction_squared > DBL_MIN) || !std::isfinite(direction_squared))
	return false;
    brep_interval direction_squared_interval = {0.0, 0.0};
    for (int component = 0; component < 3; ++component) {
	const brep_interval direction = {ray.m_dir[component],
	    ray.m_dir[component]};
	direction_squared_interval = brep_interval_add(
	    direction_squared_interval,
	    brep_interval_multiply(direction, direction));
    }
    if (!(direction_squared_interval.minimum > 0.0) ||
	    !std::isfinite(direction_squared_interval.maximum))
	return false;
    coefficients.expansion_available = true;
    for (int i = 0; i < coefficients.order[0]; ++i) {
	for (int j = 0; j < coefficients.order[1]; ++j) {
	    ON_4dPoint cv;
	    if (!span.surface.GetCV(i, j, cv) || !cv.IsValid() ||
		    !(cv.w > 0.0) || !std::isfinite(cv.w))
		return false;
	    const ON_3dVector numerator(
		std::fma(-ray.m_origin.x, cv.w, cv.x),
		std::fma(-ray.m_origin.y, cv.w, cv.y),
		std::fma(-ray.m_origin.z, cv.w, cv.z));
	    const double cv_value[4] = {cv.x, cv.y, cv.z, cv.w};
	    const double origin[3] = {ray.m_origin.x, ray.m_origin.y,
		ray.m_origin.z};
	    const double direction[3] = {ray.m_dir.x, ray.m_dir.y,
		ray.m_dir.z};
	    const double plane_value[2][3] = {
		{first.x, first.y, first.z},
		{second.x, second.y, second.z}
	    };
	    brep_interval function_interval[2];
	    brep_interval ray_coefficient;
	    if (!brep_single_coefficient_intervals(cv_value, origin, direction,
		    plane_value, direction_squared_interval, function_interval,
		    ray_coefficient))
		return false;
	    brep_interval expansion_function_interval[2];
	    bool expansion_available =
		brep_single_coefficient_expansion_intervals(cv_value, origin,
		    plane_value, expansion_function_interval,
		    coefficients.expansion_high_water);
	    for (int equation = 0; expansion_available && equation < 2;
		    ++equation) {
		expansion_function_interval[equation].minimum = std::max(
		    expansion_function_interval[equation].minimum,
		    function_interval[equation].minimum);
		expansion_function_interval[equation].maximum = std::min(
		    expansion_function_interval[equation].maximum,
		    function_interval[equation].maximum);
		if (expansion_function_interval[equation].minimum >
			expansion_function_interval[equation].maximum)
		    expansion_available = false;
	    }
	    if (!expansion_available)
		coefficients.expansion_available = false;
	    const size_t index = (size_t)i * coefficients.order[1] + j;
	    double ray_dot = 0.0;
	    for (int component = 0; component < 3; ++component)
		ray_dot = std::fma(numerator[component], ray.m_dir[component],
		    ray_dot);
	    coefficients.ray_numerator[index] = ray_dot / direction_squared;
	    coefficients.weight[index] = cv.w;
	    if (!std::isfinite(coefficients.ray_numerator[index]))
		return false;
	    if (!brep_interval_common_error(
			coefficients.ray_numerator[index], ray_coefficient,
			coefficients.ray_numerator_error))
		return false;
	    for (int equation = 0; equation < 2; ++equation) {
		double value = 0.0;
		for (int component = 0; component < 3; ++component)
		    value = std::fma(numerator[component],
			planes[equation][component], value);
		if (!std::isfinite(value))
		    return false;
		coefficients.value[equation][index] = value;
		coefficients.value_interval[equation][index] =
		    function_interval[equation];
		coefficients.value_expansion_interval[equation][index] =
		    expansion_available ?
		    expansion_function_interval[equation] :
		    function_interval[equation];
		if (!brep_interval_common_error(value,
			function_interval[equation],
			coefficients.error[equation]))
		    return false;
	    }
	}
    }
    coefficients.weight_error = 0.0;
    return true;
}


static bool
brep_coefficient_hull_excluded(const double *values, size_t count,
    double error)
{
    double minimum = DBL_MAX;
    double maximum = -DBL_MAX;
    for (size_t i = 0; i < count; ++i) {
	minimum = std::min(minimum, values[i]);
	maximum = std::max(maximum, values[i]);
    }
    return minimum > error || maximum < -error;
}


static bool
brep_surface_coefficients_excluded(
    const brep_surface_coefficients &coefficients)
{
    const size_t count = (size_t)coefficients.order[0] *
	coefficients.order[1];
    for (int equation = 0; equation < 2; ++equation) {
	if (brep_coefficient_hull_excluded(coefficients.value[equation],
		count, coefficients.error[equation]))
	    return true;
    }
    return false;
}


static bool
brep_surface_span_excluded(const brep_surface_span &span,
    const ON_Ray &ray, const ON_3dVector &first,
    const ON_3dVector &second)
{
    brep_surface_coefficients coefficients;
    if (!brep_surface_coefficients_init(coefficients, span, ray, first,
	    second))
	return false;
    return brep_surface_coefficients_excluded(coefficients);
}


static void
brep_scalar_bezier_split(const double *input, int order, double parameter,
    double *left, double *right)
{
    if (!input || !left || !right || order < 1 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return;
    double work[BREP_DIRECT_BEZIER_MAX_ORDER] = {0.0};
    for (int i = 0; i < order; ++i)
	work[i] = input[i];
    left[0] = work[0];
    right[order - 1] = work[order - 1];
    for (int level = 1; level < order; ++level) {
	for (int i = 0; i < order - level; ++i)
	    work[i] = (1.0 - parameter) * work[i] +
		parameter * work[i + 1];
	left[level] = work[0];
	right[order - level - 1] = work[order - level - 1];
    }
}


static bool
brep_scalar_bezier_restrict(const double *input, int order, double minimum,
    double maximum, double *output)
{
    if (order < 2 || order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    minimum < 0.0 || maximum > 1.0 || !(minimum < maximum))
	return false;
    double first[BREP_DIRECT_BEZIER_MAX_ORDER];
    double second[BREP_DIRECT_BEZIER_MAX_ORDER];
    const double *current = input;
    double local_minimum = minimum;
    if (maximum < 1.0) {
	brep_scalar_bezier_split(input, order, maximum, first, second);
	current = first;
	local_minimum = minimum / maximum;
    }
    if (local_minimum > 0.0) {
	brep_scalar_bezier_split(current, order, local_minimum, first, second);
	for (int i = 0; i < order; ++i)
	    output[i] = second[i];
    } else {
	for (int i = 0; i < order; ++i)
	    output[i] = current[i];
    }
    return true;
}


static bool
brep_scalar_surface_restrict(const double *input, int u_order, int v_order,
    double u_minimum, double u_maximum, double v_minimum, double v_maximum,
    double *output)
{
    double u_restricted[BREP_DIRECT_BEZIER_MAX_CVS];
    double source[BREP_DIRECT_BEZIER_MAX_ORDER];
    double result[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int j = 0; j < v_order; ++j) {
	for (int i = 0; i < u_order; ++i)
	    source[i] = input[(size_t)i * v_order + j];
	if (!brep_scalar_bezier_restrict(source, u_order, u_minimum,
		u_maximum, result))
	    return false;
	for (int i = 0; i < u_order; ++i)
	    u_restricted[(size_t)i * v_order + j] = result[i];
    }
    for (int i = 0; i < u_order; ++i) {
	for (int j = 0; j < v_order; ++j)
	    source[j] = u_restricted[(size_t)i * v_order + j];
	if (!brep_scalar_bezier_restrict(source, v_order, v_minimum,
		v_maximum, result))
	    return false;
	for (int j = 0; j < v_order; ++j)
	    output[(size_t)i * v_order + j] = result[j];
    }
    return true;
}


static brep_interval
brep_interval_expanded(double minimum, double maximum)
{
    return {std::nextafter(minimum, -INFINITY),
	std::nextafter(maximum, INFINITY)};
}


static brep_interval
brep_interval_add(const brep_interval &first, const brep_interval &second)
{
    return brep_interval_expanded(first.minimum + second.minimum,
	first.maximum + second.maximum);
}


static brep_interval
brep_interval_scale(double scale, const brep_interval &value)
{
    const double first = scale * value.minimum;
    const double second = scale * value.maximum;
    return brep_interval_expanded(std::min(first, second),
	std::max(first, second));
}


static brep_interval
brep_interval_multiply(const brep_interval &first,
	const brep_interval &second)
{
    const double product[4] = {
	first.minimum * second.minimum, first.minimum * second.maximum,
	first.maximum * second.minimum, first.maximum * second.maximum
    };
    double minimum = product[0];
    double maximum = product[0];
    for (size_t i = 1; i < 4; ++i) {
	minimum = std::min(minimum, product[i]);
	maximum = std::max(maximum, product[i]);
    }
    return brep_interval_expanded(minimum, maximum);
}


static bool
brep_interval_divide_nonzero(const brep_interval &numerator,
	const brep_interval &denominator, brep_interval &result)
{
    if (!std::isfinite(numerator.minimum) ||
	    !std::isfinite(numerator.maximum) ||
	    !std::isfinite(denominator.minimum) ||
	    !std::isfinite(denominator.maximum) ||
	    numerator.minimum > numerator.maximum ||
	    denominator.minimum > denominator.maximum ||
	    (denominator.minimum <= 0.0 && denominator.maximum >= 0.0))
	return false;
    const double quotient[4] = {
	numerator.minimum / denominator.minimum,
	numerator.minimum / denominator.maximum,
	numerator.maximum / denominator.minimum,
	numerator.maximum / denominator.maximum
    };
    double minimum = quotient[0];
    double maximum = quotient[0];
    for (size_t i = 1; i < 4; ++i) {
	minimum = std::min(minimum, quotient[i]);
	maximum = std::max(maximum, quotient[i]);
    }
    result = brep_interval_expanded(minimum, maximum);
    return std::isfinite(result.minimum) && std::isfinite(result.maximum);
}


static bool
brep_interval_divide(const brep_interval &numerator,
	const brep_interval &denominator, brep_interval &result)
{
    return denominator.minimum > 0.0 &&
	brep_interval_divide_nonzero(numerator, denominator, result);
}


static bool
brep_linear_coefficient_hulls(
    const double values[2][BREP_DIRECT_BEZIER_MAX_CVS], size_t count,
    const double coefficient_error[2], const double transform[2][2],
    brep_interval hull[2])
{
    if (!values || !coefficient_error || !transform || !hull || !count ||
	    count > BREP_DIRECT_BEZIER_MAX_CVS ||
	    !std::isfinite(coefficient_error[0]) ||
	    !std::isfinite(coefficient_error[1]) ||
	    coefficient_error[0] < 0.0 || coefficient_error[1] < 0.0)
	return false;
    for (int row = 0; row < 2; ++row) {
	if (!std::isfinite(transform[row][0]) ||
		!std::isfinite(transform[row][1]))
	    return false;
	hull[row].minimum = DBL_MAX;
	hull[row].maximum = -DBL_MAX;
	for (size_t i = 0; i < count; ++i) {
	    brep_interval combination = {0.0, 0.0};
	    for (int source = 0; source < 2; ++source) {
		if (!std::isfinite(values[source][i]))
		    return false;
		const brep_interval coefficient = brep_interval_expanded(
		    values[source][i] - coefficient_error[source],
		    values[source][i] + coefficient_error[source]);
		combination = brep_interval_add(combination,
		    brep_interval_scale(transform[row][source], coefficient));
	    }
	    hull[row].minimum = std::min(hull[row].minimum,
		combination.minimum);
	    hull[row].maximum = std::max(hull[row].maximum,
		combination.maximum);
	}
	if (!std::isfinite(hull[row].minimum) ||
		!std::isfinite(hull[row].maximum) ||
		hull[row].minimum > hull[row].maximum)
	    return false;
    }
    return true;
}


extern "C" int
_rt_brep_linear_hull_test(const fastf_t *first_coefficients,
    const fastf_t *second_coefficients, size_t count,
    const fastf_t coefficient_error[2], const fastf_t transform[2][2],
    struct rt_brep_linear_hull_test_result *result)
{
    if (!first_coefficients || !second_coefficients || !coefficient_error ||
	    !transform || !result || !count ||
	    count > BREP_DIRECT_BEZIER_MAX_CVS)
	return 0;
    double values[2][BREP_DIRECT_BEZIER_MAX_CVS] = {};
    for (size_t i = 0; i < count; ++i) {
	values[0][i] = first_coefficients[i];
	values[1][i] = second_coefficients[i];
    }
    const double errors[2] = {
	coefficient_error[0], coefficient_error[1]
    };
    const double matrix[2][2] = {
	{transform[0][0], transform[0][1]},
	{transform[1][0], transform[1][1]}
    };
    brep_interval hull[2];
    if (!brep_linear_coefficient_hulls(values, count, errors, matrix, hull))
	return 0;
    result->excluded = 0;
    for (int row = 0; row < 2; ++row) {
	result->minimum[row] = hull[row].minimum;
	result->maximum[row] = hull[row].maximum;
	if (hull[row].minimum > 0.0 || hull[row].maximum < 0.0)
	    result->excluded = 1;
    }
    return 1;
}


static uint64_t
brep_binomial_coefficient(int degree, int index)
{
    if (index < 0 || index > degree)
	return 0;
    index = std::min(index, degree - index);
    uint64_t result = 1;
    for (int i = 1; i <= index; ++i)
	result = result * (uint64_t)(degree - index + i) / (uint64_t)i;
    return result;
}


static bool
brep_interval_surface_derivative_coefficients(const brep_interval *input,
    int u_order, int v_order, int direction, brep_interval *output,
    int output_order[2])
{
    if (!input || !output || !output_order ||
	    (direction != 0 && direction != 1) ||
	    u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    output_order[0] = u_order - (direction == 0 ? 1 : 0);
    output_order[1] = v_order - (direction == 1 ? 1 : 0);
    const int degree = direction == 0 ? u_order - 1 : v_order - 1;
    for (int i = 0; i < output_order[0]; ++i) {
	for (int j = 0; j < output_order[1]; ++j) {
	    const size_t previous = (size_t)i * v_order + j;
	    const size_t next = direction == 0 ?
		(size_t)(i + 1) * v_order + j :
		(size_t)i * v_order + j + 1;
	    const brep_interval delta = brep_interval_add(input[next],
		brep_interval_scale(-1.0, input[previous]));
	    const brep_interval derivative = brep_interval_scale(degree,
		delta);
	    if (!std::isfinite(derivative.minimum) ||
		    !std::isfinite(derivative.maximum) ||
		    derivative.minimum > derivative.maximum)
		return false;
	    output[(size_t)i * output_order[1] + j] = derivative;
	}
    }
    return true;
}


static bool
brep_interval_surface_product_coefficients(const brep_interval *first,
    const int first_order[2], const brep_interval *second,
    const int second_order[2], brep_interval *output, int output_order[2])
{
    if (!first || !first_order || !second || !second_order || !output ||
	    !output_order || first_order[0] < 1 || first_order[1] < 1 ||
	    second_order[0] < 1 || second_order[1] < 1)
	return false;
    output_order[0] = first_order[0] + second_order[0] - 1;
    output_order[1] = first_order[1] + second_order[1] - 1;
    if (output_order[0] > RT_BREP_DETERMINANT_TEST_MAX_ORDER ||
	    output_order[1] > RT_BREP_DETERMINANT_TEST_MAX_ORDER)
	return false;

    const int first_degree[2] = {
	first_order[0] - 1, first_order[1] - 1
    };
    const int second_degree[2] = {
	second_order[0] - 1, second_order[1] - 1
    };
    for (int k = 0; k < output_order[0]; ++k) {
	const uint64_t u_denominator = brep_binomial_coefficient(
	    first_degree[0] + second_degree[0], k);
	const int first_u_minimum = std::max(0, k - second_degree[0]);
	const int first_u_maximum = std::min(first_degree[0], k);
	for (int l = 0; l < output_order[1]; ++l) {
	    const uint64_t v_denominator = brep_binomial_coefficient(
		first_degree[1] + second_degree[1], l);
	    const int first_v_minimum = std::max(0,
		l - second_degree[1]);
	    const int first_v_maximum = std::min(first_degree[1], l);
	    if (!u_denominator || !v_denominator)
		return false;
	    brep_interval coefficient = {0.0, 0.0};
	    for (int i = first_u_minimum; i <= first_u_maximum; ++i) {
		const int second_i = k - i;
		const uint64_t u_numerator =
		    brep_binomial_coefficient(first_degree[0], i) *
		    brep_binomial_coefficient(second_degree[0], second_i);
		brep_interval u_weight;
		if (!brep_interval_divide_nonzero(
			{(double)u_numerator, (double)u_numerator},
			{(double)u_denominator, (double)u_denominator},
			u_weight))
		    return false;
		for (int j = first_v_minimum; j <= first_v_maximum; ++j) {
		    const int second_j = l - j;
		    const uint64_t v_numerator =
			brep_binomial_coefficient(first_degree[1], j) *
			brep_binomial_coefficient(second_degree[1], second_j);
		    brep_interval v_weight;
		    if (!brep_interval_divide_nonzero(
			    {(double)v_numerator, (double)v_numerator},
			    {(double)v_denominator, (double)v_denominator},
			    v_weight))
			return false;
		    const brep_interval value = brep_interval_multiply(
			first[(size_t)i * first_order[1] + j],
			second[(size_t)second_i * second_order[1] + second_j]);
		    coefficient = brep_interval_add(coefficient,
			brep_interval_multiply(value,
			    brep_interval_multiply(u_weight, v_weight)));
		}
	    }
	    if (!std::isfinite(coefficient.minimum) ||
		    !std::isfinite(coefficient.maximum) ||
		    coefficient.minimum > coefficient.maximum)
		return false;
	    output[(size_t)k * output_order[1] + l] = coefficient;
	}
    }
    return true;
}


static bool
brep_interval_surface_determinant_coefficients(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    int u_order, int v_order, brep_interval *determinant,
    int determinant_order[2])
{
    if (!values || !determinant || !determinant_order)
	return false;
    brep_interval derivative[2][2][BREP_DIRECT_BEZIER_MAX_CVS];
    int derivative_order[2][2][2];
    for (int equation = 0; equation < 2; ++equation) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (!brep_interval_surface_derivative_coefficients(
		    values[equation], u_order, v_order, direction,
		    derivative[equation][direction],
		    derivative_order[equation][direction]))
		return false;
	}
    }

    brep_interval cross[RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
    int first_order[2];
    int second_order[2];
    if (!brep_interval_surface_product_coefficients(derivative[0][0],
	    derivative_order[0][0], derivative[1][1],
	    derivative_order[1][1], determinant, first_order) ||
	    !brep_interval_surface_product_coefficients(derivative[0][1],
		derivative_order[0][1], derivative[1][0],
		derivative_order[1][0], cross, second_order) ||
	    first_order[0] != second_order[0] ||
	    first_order[1] != second_order[1])
	return false;
    determinant_order[0] = first_order[0];
    determinant_order[1] = first_order[1];
    const size_t count = (size_t)first_order[0] * first_order[1];
    for (size_t i = 0; i < count; ++i) {
	determinant[i] = brep_interval_add(determinant[i],
	    brep_interval_scale(-1.0, cross[i]));
	if (!std::isfinite(determinant[i].minimum) ||
		!std::isfinite(determinant[i].maximum) ||
		determinant[i].minimum > determinant[i].maximum)
	    return false;
    }
    return true;
}


extern "C" int
_rt_brep_determinant_test(const fastf_t *first_coefficients,
    const fastf_t *first_error, const fastf_t *second_coefficients,
    const fastf_t *second_error, int u_order, int v_order,
    struct rt_brep_determinant_test_result *result)
{
    if (!first_coefficients || !first_error || !second_coefficients ||
	    !second_error || !result || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const fastf_t *coefficient[2] = {
	first_coefficients, second_coefficients
    };
    const fastf_t *error[2] = {first_error, second_error};
    const size_t count = (size_t)u_order * v_order;
    for (int equation = 0; equation < 2; ++equation) {
	for (size_t i = 0; i < count; ++i) {
	    if (!std::isfinite(coefficient[equation][i]) ||
		    !std::isfinite(error[equation][i]) ||
		    error[equation][i] < 0.0)
		return 0;
	    values[equation][i] = brep_interval_expanded(
		coefficient[equation][i] - error[equation][i],
		coefficient[equation][i] + error[equation][i]);
	}
    }
    brep_interval determinant[RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
    int determinant_order[2];
    if (!brep_interval_surface_determinant_coefficients(values, u_order,
	    v_order, determinant, determinant_order))
	return 0;
    result->u_order = determinant_order[0];
    result->v_order = determinant_order[1];
    const size_t determinant_count =
	(size_t)determinant_order[0] * determinant_order[1];
    for (size_t i = 0; i < determinant_count; ++i) {
	result->minimum[i] = determinant[i].minimum;
	result->maximum[i] = determinant[i].maximum;
    }
    return 1;
}


static bool
brep_interval_surface_evaluate_coefficients(const brep_interval *input,
    int u_order, int v_order, const double parameter[2],
    brep_interval &value)
{
    if (!input || !parameter || u_order < 1 || v_order < 1 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    parameter[0] < 0.0 || parameter[0] > 1.0 ||
	    parameter[1] < 0.0 || parameter[1] > 1.0)
	return false;
    brep_interval v_control[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int j = 0; j < v_order; ++j) {
	brep_interval work[BREP_DIRECT_BEZIER_MAX_ORDER];
	for (int i = 0; i < u_order; ++i)
	    work[i] = input[(size_t)i * v_order + j];
	for (int level = 1; level < u_order; ++level) {
	    for (int i = 0; i < u_order - level; ++i) {
		work[i] = brep_interval_add(
		    brep_interval_scale(1.0 - parameter[0], work[i]),
		    brep_interval_scale(parameter[0], work[i + 1]));
	    }
	}
	v_control[j] = work[0];
    }
    for (int level = 1; level < v_order; ++level) {
	for (int j = 0; j < v_order - level; ++j) {
	    v_control[j] = brep_interval_add(
		brep_interval_scale(1.0 - parameter[1], v_control[j]),
		brep_interval_scale(parameter[1], v_control[j + 1]));
	}
    }
    value = v_control[0];
    return std::isfinite(value.minimum) && std::isfinite(value.maximum) &&
	value.minimum <= value.maximum;
}


static bool
brep_interval_surface_derivative_hull(const brep_interval *input,
    int u_order, int v_order, int direction, brep_interval &hull)
{
    brep_interval derivative[BREP_DIRECT_BEZIER_MAX_CVS];
    int derivative_order[2];
    if (!brep_interval_surface_derivative_coefficients(input, u_order,
	    v_order, direction, derivative, derivative_order))
	return false;
    hull.minimum = DBL_MAX;
    hull.maximum = -DBL_MAX;
    const size_t count =
	(size_t)derivative_order[0] * derivative_order[1];
    for (size_t i = 0; i < count; ++i) {
	hull.minimum = std::min(hull.minimum, derivative[i].minimum);
	hull.maximum = std::max(hull.maximum, derivative[i].maximum);
    }
    return std::isfinite(hull.minimum) && std::isfinite(hull.maximum) &&
	hull.minimum <= hull.maximum;
}


static bool
brep_interval_krawczyk_from_bounds(const brep_interval function[2],
    const brep_interval jacobian[2][2], const double root[2],
    struct rt_brep_krawczyk_test_result &result)
{
    if (!function || !jacobian || !root ||
	    root[0] < 0.0 || root[0] > 1.0 ||
	    root[1] < 0.0 || root[1] > 1.0)
	return false;
    double midpoint[2][2];
    for (int row = 0; row < 2; ++row) {
	for (int column = 0; column < 2; ++column) {
	    midpoint[row][column] =
		0.5 * jacobian[row][column].minimum +
		0.5 * jacobian[row][column].maximum;
	}
    }
    const double determinant = midpoint[0][0] * midpoint[1][1] -
	midpoint[0][1] * midpoint[1][0];
    const double first_scale = hypot(midpoint[0][0], midpoint[1][0]);
    const double second_scale = hypot(midpoint[0][1], midpoint[1][1]);
    if (!std::isfinite(determinant) || !(first_scale > DBL_MIN) ||
	    !(second_scale > DBL_MIN))
	return false;
    result.determinant_ratio =
	fabs(determinant / first_scale) / second_scale;
    if (!(fabs(determinant) > 0.0) ||
	    !std::isfinite(result.determinant_ratio))
	return false;
    const double inverse[2][2] = {
	{midpoint[1][1] / determinant, -midpoint[0][1] / determinant},
	{-midpoint[1][0] / determinant, midpoint[0][0] / determinant}
    };
    for (int row = 0; row < 2; ++row) {
	for (int column = 0; column < 2; ++column) {
	    if (!std::isfinite(inverse[row][column]))
		return false;
	}
    }
    result.available = 1;

    brep_interval center[2];
    for (int row = 0; row < 2; ++row) {
	brep_interval correction = {0.0, 0.0};
	for (int equation = 0; equation < 2; ++equation) {
	    correction = brep_interval_add(correction,
		brep_interval_scale(inverse[row][equation],
		    function[equation]));
	}
	center[row] = brep_interval_add({root[row], root[row]},
	    brep_interval_scale(-1.0, correction));
    }

    brep_interval remainder[2][2];
    for (int row = 0; row < 2; ++row) {
	for (int column = 0; column < 2; ++column) {
	    brep_interval product = {0.0, 0.0};
	    for (int equation = 0; equation < 2; ++equation) {
		product = brep_interval_add(product,
		    brep_interval_scale(inverse[row][equation],
			jacobian[equation][column]));
	    }
	    remainder[row][column] = brep_interval_scale(-1.0, product);
	    if (row == column) {
		remainder[row][column] = brep_interval_add({1.0, 1.0},
		    remainder[row][column]);
	    }
	}
    }

    const brep_interval offset[2] = {
	brep_interval_expanded(-root[0], 1.0 - root[0]),
	brep_interval_expanded(-root[1], 1.0 - root[1])
    };
    result.certified = 1;
    const double inclusion_margin = 512.0 * DBL_EPSILON;
    for (int row = 0; row < 2; ++row) {
	brep_interval image = center[row];
	for (int column = 0; column < 2; ++column) {
	    image = brep_interval_add(image, brep_interval_multiply(
		remainder[row][column], offset[column]));
	}
	result.image_minimum[row] = image.minimum;
	result.image_maximum[row] = image.maximum;
	if (!(image.minimum > inclusion_margin) ||
		!(image.maximum < 1.0 - inclusion_margin))
	    result.certified = 0;
    }
    return true;
}


static bool
brep_interval_surface_krawczyk(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    int u_order, int v_order, const double root[2],
    struct rt_brep_krawczyk_test_result &result)
{
    if (!values || !root || root[0] < 0.0 || root[0] > 1.0 ||
	    root[1] < 0.0 || root[1] > 1.0)
	return false;
    brep_interval function[2];
    brep_interval jacobian[2][2];
    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_interval_surface_evaluate_coefficients(values[equation],
		u_order, v_order, root, function[equation]))
	    return false;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!brep_interval_surface_derivative_hull(values[equation],
		    u_order, v_order, direction,
		    jacobian[equation][direction]))
		return false;
	}
    }
    return brep_interval_krawczyk_from_bounds(function, jacobian, root,
	result);
}


extern "C" int
_rt_brep_krawczyk_test(const fastf_t *first_coefficients,
    const fastf_t *first_error, const fastf_t *second_coefficients,
    const fastf_t *second_error, int u_order, int v_order,
    const fastf_t root[2], struct rt_brep_krawczyk_test_result *result)
{
    if (!first_coefficients || !first_error || !second_coefficients ||
	    !second_error || !root || !result || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    *result = {};
    brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const fastf_t *coefficient[2] = {
	first_coefficients, second_coefficients
    };
    const fastf_t *error[2] = {first_error, second_error};
    const size_t count = (size_t)u_order * v_order;
    for (int equation = 0; equation < 2; ++equation) {
	for (size_t i = 0; i < count; ++i) {
	    if (!std::isfinite(coefficient[equation][i]) ||
		    !std::isfinite(error[equation][i]) ||
		    error[equation][i] < 0.0)
		return 0;
	    values[equation][i] = brep_interval_expanded(
		coefficient[equation][i] - error[equation][i],
		coefficient[equation][i] + error[equation][i]);
	}
    }
    (void)brep_interval_surface_krawczyk(values, u_order, v_order, root,
	*result);
    return 1;
}


static bool
brep_interval_coefficients_strict_sign(const brep_interval *coefficients,
    size_t count, int &sign)
{
    if (!coefficients || !count)
	return false;
    sign = 0;
    for (size_t i = 0; i < count; ++i) {
	const int coefficient_sign = coefficients[i].minimum > 0.0 ? 1 :
	    (coefficients[i].maximum < 0.0 ? -1 : 0);
	if (!coefficient_sign || (sign && coefficient_sign != sign)) {
	    sign = 0;
	    break;
	}
	sign = coefficient_sign;
    }
    return true;
}


static bool
brep_interval_determinant_sign_from_coefficients(
    const brep_interval *determinant, const int determinant_order[2],
    int &determinant_sign)
{
    if (!determinant || !determinant_order || determinant_order[0] < 1 ||
	    determinant_order[1] < 1 ||
	    determinant_order[0] > RT_BREP_DETERMINANT_TEST_MAX_ORDER ||
	    determinant_order[1] > RT_BREP_DETERMINANT_TEST_MAX_ORDER)
	return false;
    const size_t determinant_count =
	(size_t)determinant_order[0] * determinant_order[1];
    return brep_interval_coefficients_strict_sign(determinant,
	determinant_count, determinant_sign);
}


static bool
brep_interval_surface_determinant_sign(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    int u_order, int v_order, int &determinant_sign)
{
    brep_interval determinant[RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
    int determinant_order[2];
    if (!brep_interval_surface_determinant_coefficients(values, u_order,
	    v_order, determinant, determinant_order))
	return false;
    return brep_interval_determinant_sign_from_coefficients(determinant,
	determinant_order, determinant_sign);
}


static bool
brep_interval_surface_regular_graph(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    int u_order, int v_order, int regular_direction,
    struct rt_brep_corridor_test_result &result)
{
    if (!values || (regular_direction != 0 && regular_direction != 1) ||
	    u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    brep_interval regular_derivative;
    if (!brep_interval_surface_derivative_hull(values[0], u_order,
	    v_order, regular_direction, regular_derivative))
	return false;
    result.regular_derivative_signed =
	regular_derivative.minimum > 0.0 || regular_derivative.maximum < 0.0;

    brep_interval boundary[2] = {
	{DBL_MAX, -DBL_MAX}, {DBL_MAX, -DBL_MAX}
    };
    const int weak_count = regular_direction == 0 ? v_order : u_order;
    for (int i = 0; i < weak_count; ++i) {
	const size_t first_index = regular_direction == 0 ? (size_t)i :
	    (size_t)i * v_order;
	const size_t second_index = regular_direction == 0 ?
	    (size_t)(u_order - 1) * v_order + i :
	    (size_t)i * v_order + v_order - 1;
	boundary[0].minimum = std::min(boundary[0].minimum,
	    values[0][first_index].minimum);
	boundary[0].maximum = std::max(boundary[0].maximum,
	    values[0][first_index].maximum);
	boundary[1].minimum = std::min(boundary[1].minimum,
	    values[0][second_index].minimum);
	boundary[1].maximum = std::max(boundary[1].maximum,
	    values[0][second_index].maximum);
    }
    result.regular_boundaries_opposed =
	(boundary[0].maximum < 0.0 && boundary[1].minimum > 0.0) ||
	(boundary[1].maximum < 0.0 && boundary[0].minimum > 0.0);

    return true;
}


static bool
brep_interval_surface_corridor(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    int u_order, int v_order, int regular_direction,
    struct rt_brep_corridor_test_result &result)
{
    result = {};
    if (!brep_interval_surface_regular_graph(values, u_order, v_order,
	    regular_direction, result))
	return false;

    int determinant_sign = 0;
    if (!brep_interval_surface_determinant_sign(values, u_order, v_order,
	    determinant_sign))
	return false;
    result.determinant_signed = determinant_sign != 0;
    result.determinant_sign = determinant_sign;
    result.available = 1;
    result.unique = result.regular_derivative_signed &&
	result.regular_boundaries_opposed && result.determinant_signed;
    return true;
}


extern "C" int
_rt_brep_corridor_test(const fastf_t *first_coefficients,
    const fastf_t *first_error, const fastf_t *second_coefficients,
    const fastf_t *second_error, int u_order, int v_order,
    int regular_direction, struct rt_brep_corridor_test_result *result)
{
    if (!first_coefficients || !first_error || !second_coefficients ||
	    !second_error || !result ||
	    (regular_direction != 0 && regular_direction != 1) ||
	    u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    *result = {};
    brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const fastf_t *coefficient[2] = {
	first_coefficients, second_coefficients
    };
    const fastf_t *error[2] = {first_error, second_error};
    const size_t count = (size_t)u_order * v_order;
    for (int equation = 0; equation < 2; ++equation) {
	for (size_t i = 0; i < count; ++i) {
	    if (!std::isfinite(coefficient[equation][i]) ||
		    !std::isfinite(error[equation][i]) ||
		    error[equation][i] < 0.0)
		return 0;
	    values[equation][i] = brep_interval_expanded(
		coefficient[equation][i] - error[equation][i],
		coefficient[equation][i] + error[equation][i]);
	}
    }
    (void)brep_interval_surface_corridor(values, u_order, v_order,
	regular_direction, *result);
    return 1;
}


static bool
brep_single_coefficient_intervals(const double cv[4],
    const double origin[3], const double direction[3],
    const double planes[2][3], const brep_interval &direction_squared,
    brep_interval function[2], brep_interval &ray_coefficient)
{
    brep_interval numerator[3];
    for (int component = 0; component < 3; ++component) {
	if (!std::isfinite(cv[component]) || !std::isfinite(origin[component]) ||
		!std::isfinite(direction[component]) ||
		!std::isfinite(planes[0][component]) ||
		!std::isfinite(planes[1][component]) || !std::isfinite(cv[3]))
	    return false;
	const double centered = std::fma(-origin[component], cv[3],
	    cv[component]);
	if (!std::isfinite(centered))
	    return false;
	numerator[component] = brep_interval_expanded(centered, centered);
    }
    brep_interval ray_dot = {0.0, 0.0};
    for (int component = 0; component < 3; ++component) {
	ray_dot = brep_interval_add(ray_dot,
	    brep_interval_scale(direction[component], numerator[component]));
    }
    if (!brep_interval_divide(ray_dot, direction_squared, ray_coefficient))
	return false;
    for (int equation = 0; equation < 2; ++equation) {
	function[equation] = {0.0, 0.0};
	for (int component = 0; component < 3; ++component) {
	    function[equation] = brep_interval_add(function[equation],
		brep_interval_scale(planes[equation][component],
		    numerator[component]));
	}
    }
    return true;
}


static void
brep_interval_bezier_split(const brep_interval *input, int order,
    const brep_interval &parameter, brep_interval *left,
    brep_interval *right)
{
    brep_interval work[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
    for (int i = 0; i < order; ++i)
	work[i] = input[i];
    left[0] = work[0];
    right[order - 1] = work[order - 1];
    const brep_interval complement = brep_interval_add({1.0, 1.0},
	brep_interval_scale(-1.0, parameter));
    for (int level = 1; level < order; ++level) {
	for (int i = 0; i < order - level; ++i) {
	    work[i] = brep_interval_add(
		brep_interval_multiply(complement, work[i]),
		brep_interval_multiply(parameter, work[i + 1]));
	}
	left[level] = work[0];
	right[order - level - 1] = work[order - level - 1];
    }
}


static bool
brep_interval_bezier_restrict(const brep_interval *input, int order,
    double minimum, double maximum, brep_interval *output)
{
    if (!input || !output || order < 2 ||
	    order > RT_BREP_DETERMINANT_TEST_MAX_ORDER || minimum < 0.0 ||
	    maximum > 1.0 || !(minimum < maximum))
	return false;
    brep_interval first[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
    brep_interval second[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
    const brep_interval *current = input;
    brep_interval local_minimum = {minimum, minimum};
    if (maximum < 1.0) {
	brep_interval_bezier_split(input, order, {maximum, maximum}, first,
	    second);
	current = first;
	if (!brep_interval_divide({minimum, minimum}, {maximum, maximum},
		local_minimum))
	    return false;
    }
    if (minimum > 0.0) {
	brep_interval_bezier_split(current, order, local_minimum, first,
	    second);
	for (int i = 0; i < order; ++i)
	    output[i] = second[i];
    } else {
	for (int i = 0; i < order; ++i)
	    output[i] = current[i];
    }
    return true;
}


static bool
brep_interval_surface_restrict(const brep_interval *input, int u_order,
    int v_order, double u_minimum, double u_maximum, double v_minimum,
    double v_maximum, brep_interval *output)
{
    brep_interval u_restricted[BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval source[BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_interval result[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int j = 0; j < v_order; ++j) {
	for (int i = 0; i < u_order; ++i)
	    source[i] = input[(size_t)i * v_order + j];
	if (!brep_interval_bezier_restrict(source, u_order, u_minimum,
		u_maximum, result))
	    return false;
	for (int i = 0; i < u_order; ++i)
	    u_restricted[(size_t)i * v_order + j] = result[i];
    }
    for (int i = 0; i < u_order; ++i) {
	for (int j = 0; j < v_order; ++j)
	    source[j] = u_restricted[(size_t)i * v_order + j];
	if (!brep_interval_bezier_restrict(source, v_order, v_minimum,
		v_maximum, result))
	    return false;
	for (int j = 0; j < v_order; ++j)
	    output[(size_t)i * v_order + j] = result[j];
    }
    return true;
}


static bool
brep_interval_coefficient_hull(const brep_interval *coefficients,
    size_t count, brep_interval &hull)
{
    if (!coefficients || !count)
	return false;
    hull = {DBL_MAX, -DBL_MAX};
    for (size_t i = 0; i < count; ++i) {
	if (!std::isfinite(coefficients[i].minimum) ||
		!std::isfinite(coefficients[i].maximum) ||
		coefficients[i].minimum > coefficients[i].maximum)
	    return false;
	hull.minimum = std::min(hull.minimum, coefficients[i].minimum);
	hull.maximum = std::max(hull.maximum, coefficients[i].maximum);
    }
    return std::isfinite(hull.minimum) && std::isfinite(hull.maximum) &&
	hull.minimum <= hull.maximum;
}


static bool
brep_interval_rational_curve_derivative_hull(
    const brep_interval *numerator, const brep_interval *weight, int order,
    brep_interval &derivative)
{
    if (!numerator || !weight || order < 2 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    brep_interval weight_hull;

    if (!brep_interval_coefficient_hull(weight, order, weight_hull) ||
	    !(weight_hull.minimum > 0.0))
	return false;

    brep_interval numerator_derivative[BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_interval weight_derivative[BREP_DIRECT_BEZIER_MAX_ORDER];
    const double degree = order - 1;
    for (int i = 0; i < order - 1; ++i) {
	numerator_derivative[i] = brep_interval_scale(degree,
	    brep_interval_add(numerator[i + 1],
		brep_interval_scale(-1.0, numerator[i])));
	weight_derivative[i] = brep_interval_scale(degree,
	    brep_interval_add(weight[i + 1],
		brep_interval_scale(-1.0, weight[i])));
    }
    const int derivative_order[2] = {order - 1, 1};
    const int value_order[2] = {order, 1};
    /* Form X'W-XW' as Bernstein product coefficients before taking a hull.
     * Independent hull products lose the shared-control cancellation and the
     * error is amplified when a small restricted domain is rescaled. */
    brep_interval first_product[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
    brep_interval second_product[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
    int first_order[2];
    int second_order[2];
    if (!brep_interval_surface_product_coefficients(numerator_derivative,
	    derivative_order, weight, value_order, first_product,
	    first_order) ||
	    !brep_interval_surface_product_coefficients(weight_derivative,
		derivative_order, numerator, value_order, second_product,
		second_order) ||
	    first_order[0] != second_order[0] || first_order[1] != 1 ||
	    second_order[1] != 1)
	return false;
    brep_interval derivative_numerator = {DBL_MAX, -DBL_MAX};
    for (int i = 0; i < first_order[0]; ++i) {
	const brep_interval coefficient = brep_interval_add(first_product[i],
	    brep_interval_scale(-1.0, second_product[i]));
	derivative_numerator.minimum = std::min(
	    derivative_numerator.minimum, coefficient.minimum);
	derivative_numerator.maximum = std::max(
	    derivative_numerator.maximum, coefficient.maximum);
    }
    const brep_interval denominator = brep_interval_multiply(weight_hull,
	weight_hull);
    return brep_interval_divide(derivative_numerator, denominator,
	derivative);
}


static bool
brep_interval_rational_surface_derivative_hull(
    const brep_interval *numerator, const brep_interval *weight,
    int u_order, int v_order, int direction, double parameter_scale,
    brep_interval &derivative)
{
    if (!numerator || !weight ||
	    (direction != 0 && direction != 1) ||
	    !(parameter_scale > 0.0) || !std::isfinite(parameter_scale))
	return false;
    const size_t count = (size_t)u_order * v_order;
    brep_interval weight_hull;
    if (!brep_interval_coefficient_hull(weight, count, weight_hull) ||
	    !(weight_hull.minimum > 0.0))
	return false;

    brep_interval numerator_derivative[BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval weight_derivative[BREP_DIRECT_BEZIER_MAX_CVS];
    int numerator_derivative_order[2];
    int weight_derivative_order[2];
    if (!brep_interval_surface_derivative_coefficients(numerator, u_order,
	    v_order, direction, numerator_derivative,
	    numerator_derivative_order) ||
	    !brep_interval_surface_derivative_coefficients(weight, u_order,
		v_order, direction, weight_derivative,
		weight_derivative_order))
	return false;
    const int value_order[2] = {u_order, v_order};
    /* Preserve the rational quotient-rule cancellation coefficient-wise.
     * This is both tighter and still outward: every product and subtraction
     * is interval arithmetic, followed only by a Bernstein convex hull. */
    brep_interval first_product[RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
    brep_interval second_product[RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
    int first_order[2];
    int second_order[2];
    if (!brep_interval_surface_product_coefficients(numerator_derivative,
	    numerator_derivative_order, weight, value_order, first_product,
	    first_order) ||
	    !brep_interval_surface_product_coefficients(weight_derivative,
		weight_derivative_order, numerator, value_order, second_product,
		second_order) ||
	    first_order[0] != second_order[0] ||
	    first_order[1] != second_order[1])
	return false;
    brep_interval derivative_numerator = {DBL_MAX, -DBL_MAX};
    const size_t product_count = (size_t)first_order[0] * first_order[1];
    for (size_t i = 0; i < product_count; ++i) {
	const brep_interval coefficient = brep_interval_add(first_product[i],
	    brep_interval_scale(-1.0, second_product[i]));
	derivative_numerator.minimum = std::min(
	    derivative_numerator.minimum, coefficient.minimum);
	derivative_numerator.maximum = std::max(
	    derivative_numerator.maximum, coefficient.maximum);
    }
    const brep_interval denominator = brep_interval_multiply(weight_hull,
	weight_hull);
    brep_interval normalized_derivative;
    if (!brep_interval_divide(derivative_numerator, denominator,
	    normalized_derivative))
	return false;
    derivative = brep_interval_scale(1.0 / parameter_scale,
	normalized_derivative);
    return std::isfinite(derivative.minimum) &&
	std::isfinite(derivative.maximum) &&
	derivative.minimum <= derivative.maximum;
}


static bool
brep_interval_trim_curve_data(const ON_BezierCurve &curve,
    const ON_2dPoint &reference, double minimum, double maximum,
    brep_interval numerator[2][BREP_DIRECT_BEZIER_MAX_ORDER],
    brep_interval *weight)
{
    const int order = curve.CVCount();
    if (!weight || curve.Dimension() != 2 || order < 2 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    brep_interval source[2][BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_interval source_weight[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int i = 0; i < order; ++i) {
	ON_4dPoint cv;
	if (!curve.GetCV(i, cv) || !cv.IsValid() || !(cv.w > 0.0) ||
		!std::isfinite(cv.w))
	    return false;
	source_weight[i] = {cv.w, cv.w};
	for (int direction = 0; direction < 2; ++direction) {
	    source[direction][i] = brep_interval_add(
		{cv[direction], cv[direction]},
		brep_interval_scale(-reference[direction],
		    source_weight[i]));
	}
    }
    for (int direction = 0; direction < 2; ++direction) {
	if (!brep_interval_bezier_restrict(source[direction], order, minimum,
		maximum, numerator[direction]))
	    return false;
    }
    return brep_interval_bezier_restrict(source_weight, order, minimum,
	maximum, weight);
}


static bool
brep_interval_trim_uv_hull(
    const brep_interval numerator[2][BREP_DIRECT_BEZIER_MAX_ORDER],
    const brep_interval *weight, int order, const ON_2dPoint &reference,
    brep_interval uv[2])
{
    if (!weight || order < 2 || order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	uv[direction] = {DBL_MAX, -DBL_MAX};
	for (int i = 0; i < order; ++i) {
	    brep_interval value;
	    if (!brep_interval_divide(numerator[direction][i], weight[i],
		    value))
		return false;
	    value = brep_interval_add(value,
		{reference[direction], reference[direction]});
	    uv[direction].minimum = std::min(uv[direction].minimum,
		value.minimum);
	    uv[direction].maximum = std::max(uv[direction].maximum,
		value.maximum);
	}
	if (!std::isfinite(uv[direction].minimum) ||
		!std::isfinite(uv[direction].maximum) ||
		uv[direction].minimum > uv[direction].maximum)
	    return false;
    }
    return true;
}


static bool
brep_interval_projected_surface_derivatives(const brep_surface_span &span,
    const ON_3dVector &axis, const ON_3dPoint &reference,
    const brep_interval uv[2], brep_interval derivative[2])
{
    const int order[2] = {
	span.surface.Order(0), span.surface.Order(1)
    };
    if (order[0] < 2 || order[1] < 2 ||
	    order[0] > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    order[1] > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    double physical_minimum[2];
    double physical_maximum[2];
    double normalized_minimum[2];
    double normalized_maximum[2];
    for (int direction = 0; direction < 2; ++direction) {
	const ON_Interval &domain = span.surface_domain[direction];
	if (!domain.IsIncreasing())
	    return false;
	const double scale = std::max(1.0,
	    std::max(fabs(domain.Min()), std::max(fabs(domain.Max()),
		std::max(fabs(uv[direction].minimum),
		    fabs(uv[direction].maximum)))));
	const double padding = 512.0 * DBL_EPSILON * scale;
	physical_minimum[direction] = std::max(domain.Min(),
	    uv[direction].minimum - padding);
	physical_maximum[direction] = std::min(domain.Max(),
	    uv[direction].maximum + padding);
	if (!(physical_minimum[direction] < physical_maximum[direction])) {
	    if (physical_minimum[direction] <= domain.Min())
		physical_maximum[direction] = std::min(domain.Max(),
		    domain.Min() + padding);
	    else
		physical_minimum[direction] = std::max(domain.Min(),
		    domain.Max() - padding);
	}
	if (!(physical_minimum[direction] < physical_maximum[direction]))
	    return false;
	normalized_minimum[direction] = domain.NormalizedParameterAt(
	    physical_minimum[direction]);
	normalized_maximum[direction] = domain.NormalizedParameterAt(
	    physical_maximum[direction]);
	if (!(normalized_minimum[direction] <
		normalized_maximum[direction]))
	    return false;
    }

    brep_interval projected[BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval weight[BREP_DIRECT_BEZIER_MAX_CVS];
    for (int i = 0; i < order[0]; ++i) {
	for (int j = 0; j < order[1]; ++j) {
	    ON_4dPoint cv;
	    if (!span.surface.GetCV(i, j, cv) || !cv.IsValid() ||
		    !(cv.w > 0.0) || !std::isfinite(cv.w))
		return false;
	    const size_t index = (size_t)i * order[1] + j;
	    weight[index] = {cv.w, cv.w};
	    projected[index] = {0.0, 0.0};
	    for (int component = 0; component < 3; ++component) {
		const brep_interval centered = brep_interval_add(
		    {cv[component], cv[component]},
		    brep_interval_scale(-reference[component],
			weight[index]));
		projected[index] = brep_interval_add(projected[index],
		    brep_interval_scale(axis[component], centered));
	    }
	}
    }
    brep_interval restricted_projected[BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval restricted_weight[BREP_DIRECT_BEZIER_MAX_CVS];
    if (!brep_interval_surface_restrict(projected, order[0], order[1],
	    normalized_minimum[0], normalized_maximum[0],
	    normalized_minimum[1], normalized_maximum[1],
	    restricted_projected) ||
	    !brep_interval_surface_restrict(weight, order[0], order[1],
		normalized_minimum[0], normalized_maximum[0],
		normalized_minimum[1], normalized_maximum[1],
		restricted_weight))
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	if (!brep_interval_rational_surface_derivative_hull(
		restricted_projected, restricted_weight, order[0], order[1],
		direction, span.surface_domain[direction].Length(),
		derivative[direction]))
	    return false;
    }
    return true;
}


struct brep_correspondence_cell {
    size_t span_index = 0;
    double minimum = 0.0;
    double maximum = 1.0;
    size_t depth = 0;
};


static bool
brep_projected_curve_control(const ON_BezierCurve &curve, int control_index,
    const ON_3dVector &axis, brep_expansion &projected, double &weight,
    size_t &high_water)
{
    const int order = curve.CVCount();
    if (curve.Dimension() != 3 || order < 2 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER || control_index < 0 ||
	    control_index >= order)
	return false;
    ON_4dPoint cv;
    if (!curve.GetCV(control_index, cv) || !cv.IsValid() ||
	    !(cv.w > 0.0) || !std::isfinite(cv.w) ||
	    !brep_expansion_set(projected, 0.0, high_water))
	return false;
    weight = cv.w;
    for (int component = 0; component < 3; ++component) {
	brep_expansion value = {};
	brep_expansion term = {};
	brep_expansion sum = {};
	if (!brep_expansion_set(value, cv[component], high_water) ||
		!brep_expansion_scale(value, axis[component], term,
		    high_water) ||
		!brep_expansion_add(projected, term, sum, high_water))
	    return false;
	projected = sum;
    }
    return true;
}


static bool
brep_projected_curve_control_monotone(const ON_BezierCurve &curve,
    const ON_3dVector &axis)
{
    const int order = curve.CVCount();
    if (curve.Dimension() != 3 || order < 2 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    brep_expansion projected[BREP_DIRECT_BEZIER_MAX_ORDER];
    double weight[BREP_DIRECT_BEZIER_MAX_ORDER];
    size_t high_water = 0;
    for (int i = 0; i < order; ++i) {
	if (!brep_projected_curve_control(curve, i, axis, projected[i],
		weight[i], high_water))
	    return false;
    }

    bool strict = false;
    for (int i = 0; i < order - 1; ++i) {
	brep_expansion next = {};
	brep_expansion previous = {};
	brep_expansion difference = {};
	brep_interval bounds;
	if (!brep_expansion_scale(projected[i + 1], weight[i], next,
		high_water) ||
		!brep_expansion_scale(projected[i], -weight[i + 1],
		    previous, high_water) ||
		!brep_expansion_add(next, previous, difference, high_water) ||
		!brep_expansion_bounds(difference, bounds) ||
		bounds.minimum < 0.0)
	    return false;
	if (bounds.minimum > 0.0)
	    strict = true;
    }
    return strict;
}


static bool
brep_expansion_rational_control_difference(const brep_expansion &first,
    double first_weight, const brep_expansion &second,
    double second_weight, brep_interval &difference, size_t &high_water)
{
    brep_expansion next = {};
    brep_expansion previous = {};
    brep_expansion exact_difference = {};
    return brep_expansion_scale(second, first_weight, next, high_water) &&
	brep_expansion_scale(first, -second_weight, previous, high_water) &&
	brep_expansion_add(next, previous, exact_difference, high_water) &&
	brep_expansion_bounds(exact_difference, difference);
}


static bool
brep_scalar_curve_control_sign(const ON_BezierCurve &curve, int component,
    int &sign)
{
    const int order = curve.CVCount();
    sign = 0;
    if (curve.Dimension() != 2 || (component != 0 && component != 1) ||
	    order < 2 || order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    brep_expansion numerator[BREP_DIRECT_BEZIER_MAX_ORDER];
    double weight[BREP_DIRECT_BEZIER_MAX_ORDER];
    size_t high_water = 0;
    for (int i = 0; i < order; ++i) {
	ON_4dPoint cv;
	if (!curve.GetCV(i, cv) || !cv.IsValid() || !(cv.w > 0.0) ||
		!std::isfinite(cv.w) ||
		!brep_expansion_set(numerator[i], cv[component], high_water))
	    return false;
	weight[i] = cv.w;
    }
    bool strict = false;
    for (int i = 0; i < order - 1; ++i) {
	brep_interval difference;
	if (!brep_expansion_rational_control_difference(numerator[i],
		weight[i], numerator[i + 1], weight[i + 1], difference,
		high_water))
	    return false;
	if (std::fpclassify(difference.minimum) == FP_ZERO &&
		std::fpclassify(difference.maximum) == FP_ZERO)
	    continue;
	const int coefficient_sign = !(difference.minimum < 0.0) ? 1 :
	    (!(difference.maximum > 0.0) ? -1 : 0);
	if (!coefficient_sign || (sign && coefficient_sign != sign))
	    return false;
	sign = coefficient_sign;
	strict = strict || (coefficient_sign > 0 ?
	    difference.minimum > 0.0 : difference.maximum < 0.0);
    }
    return sign != 0 && strict;
}


static bool
brep_trim_control_constant(const ON_BezierCurve &curve, int component,
    double value)
{
    const int order = curve.CVCount();
    if (curve.Dimension() != 2 || (component != 0 && component != 1) ||
	    !std::isfinite(value) || order < 2 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    size_t high_water = 0;
    for (int i = 0; i < order; ++i) {
	ON_4dPoint cv;
	brep_expansion coordinate = {};
	brep_expansion weighted_value = {};
	brep_expansion negative_value = {};
	brep_expansion difference = {};
	brep_interval bounds;
	if (!curve.GetCV(i, cv) || !cv.IsValid() || !(cv.w > 0.0) ||
		!brep_expansion_set(coordinate, cv[component], high_water) ||
		!brep_expansion_set(weighted_value, value, high_water) ||
		!brep_expansion_scale(weighted_value, -cv.w, negative_value,
		    high_water) ||
		!brep_expansion_add(coordinate, negative_value, difference,
		    high_water) ||
		!brep_expansion_bounds(difference, bounds) ||
		std::fpclassify(bounds.minimum) != FP_ZERO ||
		std::fpclassify(bounds.maximum) != FP_ZERO)
	    return false;
    }
    return true;
}


static bool
brep_projected_surface_boundary_control_sign(
    const brep_surface_span &span, int fixed_direction, bool maximum_side,
    const ON_3dVector &axis, int &sign)
{
    sign = 0;
    if (fixed_direction != 0 && fixed_direction != 1)
	return false;
    const int order[2] = {
	span.surface.Order(0), span.surface.Order(1)
    };
    const int varying_direction = 1 - fixed_direction;
    const int varying_order = order[varying_direction];
    if (order[0] < 2 || order[1] < 2 ||
	    order[0] > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    order[1] > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    brep_expansion projected[BREP_DIRECT_BEZIER_MAX_ORDER];
    double weight[BREP_DIRECT_BEZIER_MAX_ORDER];
    size_t high_water = 0;
    const int fixed_index = maximum_side ? order[fixed_direction] - 1 : 0;
    for (int varying_index = 0; varying_index < varying_order;
	    ++varying_index) {
	const int u = fixed_direction == 0 ? fixed_index : varying_index;
	const int v = fixed_direction == 0 ? varying_index : fixed_index;
	ON_4dPoint cv;
	if (!span.surface.GetCV(u, v, cv) || !cv.IsValid() ||
		!(cv.w > 0.0) ||
		!brep_expansion_set(projected[varying_index], 0.0,
		    high_water))
	    return false;
	weight[varying_index] = cv.w;
	for (int component = 0; component < 3; ++component) {
	    brep_expansion coordinate = {};
	    brep_expansion term = {};
	    brep_expansion sum = {};
	    if (!brep_expansion_set(coordinate, cv[component], high_water) ||
		    !brep_expansion_scale(coordinate, axis[component], term,
			high_water) ||
		    !brep_expansion_add(projected[varying_index], term, sum,
			high_water))
		return false;
	    projected[varying_index] = sum;
	}
    }
    bool strict = false;
    for (int i = 0; i < varying_order - 1; ++i) {
	brep_interval difference;
	if (!brep_expansion_rational_control_difference(projected[i],
		weight[i], projected[i + 1], weight[i + 1], difference,
		high_water))
	    return false;
	if (std::fpclassify(difference.minimum) == FP_ZERO &&
		std::fpclassify(difference.maximum) == FP_ZERO)
	    continue;
	const int coefficient_sign = !(difference.minimum < 0.0) ? 1 :
	    (!(difference.maximum > 0.0) ? -1 : 0);
	if (!coefficient_sign || (sign && coefficient_sign != sign))
	    return false;
	sign = coefficient_sign;
	strict = strict || (coefficient_sign > 0 ?
	    difference.minimum > 0.0 : difference.maximum < 0.0);
    }
    return sign != 0 && strict;
}


static bool
brep_certify_isoparametric_trim(const struct brep_specific *bs,
    const ON_BrepTrim &trim, const ON_3dVector &axis, int orientation,
    size_t &remaining_cells, size_t &visited_cells, bool &exhausted)
{
    if (!bs || !bs->brep || (orientation != -1 && orientation != 1))
	return false;
    int fixed_direction = -1;
    bool maximum_side = false;
    switch (trim.m_iso) {
	case ON_Surface::W_iso:
	    fixed_direction = 0;
	    break;
	case ON_Surface::E_iso:
	    fixed_direction = 0;
	    maximum_side = true;
	    break;
	case ON_Surface::S_iso:
	    fixed_direction = 1;
	    break;
	case ON_Surface::N_iso:
	    fixed_direction = 1;
	    maximum_side = true;
	    break;
	default:
	    return false;
    }
    const int face_index = trim.FaceIndexOf();
    const brep_face_record *face_record = brep_prepared_face(bs,
	face_index);
    const ON_BrepFace *face = trim.Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!face_record || !face_record->supported || !surface)
	return false;
    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
    if (!fixed_domain.IsIncreasing())
	return false;
    const double fixed_value = maximum_side ? fixed_domain.Max() :
	fixed_domain.Min();
    const int varying_direction = 1 - fixed_direction;

    std::vector<brep_trim_span> trim_spans;
    if (!brep_prepare_trim_spans(trim, trim_spans))
	return false;
    int trim_sign = 0;
    for (std::vector<brep_trim_span>::const_iterator span_it =
	    trim_spans.begin(); span_it != trim_spans.end(); ++span_it) {
	if (!remaining_cells) {
	    exhausted = true;
	    return false;
	}
	remaining_cells--;
	visited_cells++;
	int span_sign = 0;
	if (!brep_trim_control_constant(span_it->curve, fixed_direction,
		fixed_value) ||
		!brep_scalar_curve_control_sign(span_it->curve,
		    varying_direction, span_sign) ||
		(trim_sign && span_sign != trim_sign))
	    return false;
	trim_sign = span_sign;
    }

    int surface_sign = 0;
    size_t surface_boundary_spans = 0;
    for (size_t span_index = face_record->span_begin;
	    span_index < face_record->span_begin + face_record->span_count;
	    ++span_index) {
	const brep_surface_span &span = bs->surface_spans[span_index];
	const double span_fixed_value = maximum_side ?
	    span.surface_domain[fixed_direction].Max() :
	    span.surface_domain[fixed_direction].Min();
	if (std::memcmp(&span_fixed_value, &fixed_value,
		sizeof(fixed_value)) != 0)
	    continue;
	if (!remaining_cells) {
	    exhausted = true;
	    return false;
	}
	remaining_cells--;
	visited_cells++;
	int span_sign = 0;
	if (!brep_projected_surface_boundary_control_sign(span,
		fixed_direction, maximum_side, axis, span_sign) ||
		(surface_sign && span_sign != surface_sign))
	    return false;
	surface_sign = span_sign;
	surface_boundary_spans++;
    }
    return trim_sign && surface_sign && surface_boundary_spans &&
	orientation * trim_sign * surface_sign > 0;
}


static bool
brep_certify_projected_edge(const struct brep_specific *bs,
    const brep_edge_record &record, const ON_3dVector &axis,
    size_t &remaining_cells, size_t &visited_cells, bool &exhausted)
{
    if (!bs || !record.span_count)
	return false;
    brep_expansion previous_endpoint = {};
    double previous_weight = 0.0;
    double previous_domain_end = 0.0;
    bool have_previous = false;
    size_t high_water = 0;
    for (size_t span_index = record.span_begin;
	    span_index < record.span_begin + record.span_count; ++span_index) {
	if (!remaining_cells) {
	    exhausted = true;
	    return false;
	}
	remaining_cells--;
	visited_cells++;
	const brep_edge_span &span = bs->edge_spans[span_index];
	brep_expansion start = {};
	brep_expansion end = {};
	double start_weight = 0.0;
	double end_weight = 0.0;
	if (!brep_projected_curve_control_monotone(span.curve, axis) ||
		!brep_projected_curve_control(span.curve, 0, axis, start,
		    start_weight, high_water) ||
		!brep_projected_curve_control(span.curve,
		    span.curve.CVCount() - 1, axis, end, end_weight,
		    high_water))
	    return false;
	if (have_previous) {
	    brep_interval difference;
	    const double current_domain_start = span.edge_domain.Min();
	    if (std::memcmp(&current_domain_start, &previous_domain_end,
		    sizeof(previous_domain_end)) != 0 ||
		    !brep_expansion_rational_control_difference(
			previous_endpoint, previous_weight, start, start_weight,
			difference, high_water) ||
		    std::fpclassify(difference.minimum) != FP_ZERO ||
		    std::fpclassify(difference.maximum) != FP_ZERO)
		return false;
	}
	previous_endpoint = end;
	previous_weight = end_weight;
	previous_domain_end = span.edge_domain.Max();
	have_previous = true;
    }
    return have_previous;
}


static int
brep_projected_trim_cell(const struct brep_specific *bs,
    const std::vector<brep_trim_span> &trim_spans, int face_index,
    int orientation, const ON_3dVector &axis, const ON_3dPoint &reference,
    const ON_2dPoint &uv_reference, const brep_correspondence_cell &cell)
{
    if (!bs || !bs->brep || cell.span_index >= trim_spans.size() ||
	    face_index < 0 || face_index >= bs->brep->m_F.Count() ||
	    (orientation != -1 && orientation != 1))
	return -1;
    const brep_trim_span &trim_span = trim_spans[cell.span_index];
    const int order = trim_span.curve.CVCount();
    brep_interval trim_numerator[2][BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_interval trim_weight[BREP_DIRECT_BEZIER_MAX_ORDER];
    if (!brep_interval_trim_curve_data(trim_span.curve, uv_reference,
	    cell.minimum, cell.maximum, trim_numerator, trim_weight))
	return -1;
    brep_interval trim_derivative[2];
    for (int direction = 0; direction < 2; ++direction) {
	if (!brep_interval_rational_curve_derivative_hull(
		trim_numerator[direction], trim_weight, order,
		trim_derivative[direction]))
	    return -1;
    }
    brep_interval uv[2];
    if (!brep_interval_trim_uv_hull(trim_numerator, trim_weight, order,
	    uv_reference, uv))
	return -1;

    const brep_face_record *face_record = brep_prepared_face(bs,
	face_index);
    if (!face_record || !face_record->supported)
	return -1;
    bool covered = false;
    brep_interval lift_derivative = {DBL_MAX, -DBL_MAX};
    for (size_t span_index = face_record->span_begin;
	    span_index < face_record->span_begin + face_record->span_count;
	    ++span_index) {
	const brep_surface_span &surface_span = bs->surface_spans[span_index];
	bool overlap = true;
	for (int direction = 0; direction < 2; ++direction) {
	    const ON_Interval &domain = surface_span.surface_domain[direction];
	    const double scale = std::max(1.0,
		std::max(fabs(domain.Min()), fabs(domain.Max())));
	    const double padding = 1024.0 * DBL_EPSILON * scale;
	    overlap = overlap && uv[direction].maximum >=
		domain.Min() - padding && uv[direction].minimum <=
		domain.Max() + padding;
	}
	if (!overlap)
	    continue;
	brep_interval surface_derivative[2];
	if (!brep_interval_projected_surface_derivatives(surface_span, axis,
		reference, uv, surface_derivative))
	    return -1;
	brep_interval cell_derivative = brep_interval_add(
	    brep_interval_multiply(surface_derivative[0],
		trim_derivative[0]),
	    brep_interval_multiply(surface_derivative[1],
		trim_derivative[1]));
	cell_derivative = brep_interval_scale(orientation, cell_derivative);
	lift_derivative.minimum = std::min(lift_derivative.minimum,
	    cell_derivative.minimum);
	lift_derivative.maximum = std::max(lift_derivative.maximum,
	    cell_derivative.maximum);
	covered = true;
    }
    if (!covered || !std::isfinite(lift_derivative.minimum) ||
	    !std::isfinite(lift_derivative.maximum))
	return -1;
    if (lift_derivative.minimum > 0.0)
	return 1;
    if (lift_derivative.maximum <= 0.0)
	return -1;
    return 0;
}


template <typename CellTest>
static bool
brep_certify_correspondence_cells(size_t span_count, CellTest cell_test,
    size_t &remaining_cells, size_t &visited_cells, size_t &maximum_depth,
    bool &exhausted)
{
    const size_t maximum_subdivision_depth = 24;
    std::vector<brep_correspondence_cell> stack;
    stack.reserve(std::min((size_t)128, span_count));
    for (size_t span_index = span_count; span_index > 0; --span_index) {
	brep_correspondence_cell cell;
	cell.span_index = span_index - 1;
	stack.push_back(cell);
    }
    while (!stack.empty()) {
	if (!remaining_cells) {
	    exhausted = true;
	    return false;
	}
	const brep_correspondence_cell cell = stack.back();
	stack.pop_back();
	remaining_cells--;
	visited_cells++;
	maximum_depth = std::max(maximum_depth, cell.depth);
	const int status = cell_test(cell);
	if (status > 0)
	    continue;
	if (status < 0)
	    return false;
	const double midpoint = 0.5 * (cell.minimum + cell.maximum);
	if (cell.depth >= maximum_subdivision_depth ||
		!(cell.minimum < midpoint) || !(midpoint < cell.maximum)) {
	    exhausted = true;
	    return false;
	}
	brep_correspondence_cell right = cell;
	brep_correspondence_cell left = cell;
	left.maximum = midpoint;
	right.minimum = midpoint;
	left.depth = right.depth = cell.depth + 1;
	stack.push_back(right);
	stack.push_back(left);
    }
    return true;
}


/* A shared strictly monotone projection is a sufficient global injectivity
 * and orientation theorem.  Exact rational control signs cover edge curves
 * and isoparametric trim/surface boundaries, including zero endpoint
 * derivatives at poles.  Other regular trims use outward derivative bounds
 * with conservative subdivision.  Failure or exhaustion grants no edge
 * authority. */
static void
brep_certify_edge_correspondences(struct brep_specific *bs)
{
    if (!bs || !bs->brep)
	return;
    const ON_Brep &brep = *bs->brep;
    size_t remaining_cells = BREP_SEAM_CORRESPONDENCE_CELL_BUDGET;
    for (std::vector<brep_edge_record>::iterator record_it =
	    bs->edge_records.begin(); record_it != bs->edge_records.end();
	    ++record_it) {
	brep_edge_record &record = *record_it;
	if (!record.supported || record.edge_index < 0 ||
		record.edge_index >= brep.m_E.Count())
	    continue;
	if (!remaining_cells) {
	    record.correspondence_exhausted = true;
	    continue;
	}
	const ON_BrepEdge &edge = brep.m_E[record.edge_index];
	if (edge.m_ti.Count() != 2)
	    continue;
	const ON_3dPoint reference = edge.PointAtStart();
	ON_3dVector axis = edge.PointAtEnd() - reference;
	if (!reference.IsValid() || !axis.Unitize())
	    continue;

	bool exhausted = false;
	size_t visited = 0;
	size_t depth = 0;
	const bool edge_certified = brep_certify_projected_edge(bs, record,
	    axis, remaining_cells, visited, exhausted);
	bool trim_certified[2] = {false, false};
	for (int side = 0; edge_certified && side < 2; ++side) {
	    const int trim_index = edge.m_ti[side];
	    if (trim_index < 0 || trim_index >= brep.m_T.Count())
		break;
	    const ON_BrepTrim &trim = brep.m_T[trim_index];
	    std::vector<brep_trim_span> trim_spans;
	    if (!brep_prepare_trim_spans(trim, trim_spans))
		break;
	    const ON_3dPoint uv_start = trim.PointAtStart();
	    if (!uv_start.IsValid())
		break;
	    const ON_2dPoint uv_reference(uv_start.x, uv_start.y);
	    const int orientation = trim.m_bRev3d ? -1 : 1;
	    trim_certified[side] = brep_certify_isoparametric_trim(bs, trim,
		axis, orientation, remaining_cells, visited, exhausted);
	    if (!trim_certified[side] && !exhausted) {
		trim_certified[side] = brep_certify_correspondence_cells(
		    trim_spans.size(),
		    [&](const brep_correspondence_cell &cell) {
			return brep_projected_trim_cell(bs, trim_spans,
			    trim.FaceIndexOf(), orientation, axis, reference,
			    uv_reference, cell);
		    }, remaining_cells, visited, depth, exhausted);
	    }
	}
	record.correspondence_cells = visited;
	record.correspondence_depth = depth;
	record.correspondence_exhausted = exhausted;
	record.correspondence_supported = edge_certified &&
	    trim_certified[0] && trim_certified[1] && !exhausted;
	record.frame_interval_supported = record.correspondence_supported;
    }
}


static bool
brep_interval_determinant_surface_restrict(const brep_interval *input,
    int u_order, int v_order, double u_minimum, double u_maximum,
    double v_minimum, double v_maximum, brep_interval *output)
{
    if (!input || !output || u_order < 2 || v_order < 2 ||
	    u_order > RT_BREP_DETERMINANT_TEST_MAX_ORDER ||
	    v_order > RT_BREP_DETERMINANT_TEST_MAX_ORDER)
	return false;
    brep_interval u_restricted[RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
    brep_interval source[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
    brep_interval restricted[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
    for (int j = 0; j < v_order; ++j) {
	for (int i = 0; i < u_order; ++i)
	    source[i] = input[(size_t)i * v_order + j];
	if (!brep_interval_bezier_restrict(source, u_order, u_minimum,
		u_maximum, restricted))
	    return false;
	for (int i = 0; i < u_order; ++i)
	    u_restricted[(size_t)i * v_order + j] = restricted[i];
    }
    for (int i = 0; i < u_order; ++i) {
	for (int j = 0; j < v_order; ++j)
	    source[j] = u_restricted[(size_t)i * v_order + j];
	if (!brep_interval_bezier_restrict(source, v_order, v_minimum,
		v_maximum, restricted))
	    return false;
	for (int j = 0; j < v_order; ++j)
	    output[(size_t)i * v_order + j] = restricted[j];
    }
    return true;
}


extern "C" int
_rt_brep_determinant_restrict_test(const fastf_t *input_minimum,
    const fastf_t *input_maximum, int u_order, int v_order,
    const fastf_t minimum[2], const fastf_t maximum[2],
    fastf_t *output_minimum, fastf_t *output_maximum)
{
    if (!input_minimum || !input_maximum || !minimum || !maximum ||
	    !output_minimum || !output_maximum || u_order < 2 || v_order < 2 ||
	    u_order > RT_BREP_DETERMINANT_TEST_MAX_ORDER ||
	    v_order > RT_BREP_DETERMINANT_TEST_MAX_ORDER)
	return 0;
    const size_t count = (size_t)u_order * v_order;
    brep_interval input[RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
    brep_interval output[RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
    for (size_t i = 0; i < count; ++i) {
	if (!std::isfinite(input_minimum[i]) ||
		!std::isfinite(input_maximum[i]) ||
		input_minimum[i] > input_maximum[i])
	    return 0;
	input[i] = {input_minimum[i], input_maximum[i]};
    }
    if (!brep_interval_determinant_surface_restrict(input, u_order,
	    v_order, minimum[0], maximum[0], minimum[1], maximum[1], output))
	return 0;
    for (size_t i = 0; i < count; ++i) {
	output_minimum[i] = output[i].minimum;
	output_maximum[i] = output[i].maximum;
    }
    return 1;
}


static bool
brep_two_sum(double first, double second, double &sum, double &error)
{
    sum = first + second;
    if (!std::isfinite(sum))
	return false;
    const double second_virtual = sum - first;
    const double first_virtual = sum - second_virtual;
    const double second_roundoff = second - second_virtual;
    const double first_roundoff = first - first_virtual;
    error = first_roundoff + second_roundoff;
    return std::isfinite(error);
}


static bool
brep_two_product(double first, double second, double &product, double &error)
{
    if (!std::isfinite(first) || !std::isfinite(second))
	return false;
    if (std::fpclassify(first) == FP_ZERO ||
	    std::fpclassify(second) == FP_ZERO) {
	product = first * second;
	error = 0.0;
	return true;
    }

    /*
     * An expansion cannot represent a nonzero bit below the least binary64
     * subnormal.  Reject any product whose least possible exact significand
     * bit could cross that boundary.  This is deliberately conservative for
     * operands with trailing zero bits.
     */
    int first_exponent = 0;
    int second_exponent = 0;
    (void)std::frexp(first, &first_exponent);
    (void)std::frexp(second, &second_exponent);
    const int least_product_exponent = first_exponent + second_exponent -
	2 * DBL_MANT_DIG;
    const int least_binary64_exponent = DBL_MIN_EXP - DBL_MANT_DIG;
    if (least_product_exponent < least_binary64_exponent)
	return false;

    product = first * second;
    if (!std::isfinite(product))
	return false;
    error = std::fma(first, second, -product);
    return std::isfinite(error);
}


static bool
brep_expansion_set(brep_expansion &result, double value,
    size_t &high_water)
{
    if (!std::isfinite(value))
	return false;
    result.count = 1;
    result.component[0] = value;
    high_water = std::max(high_water, result.count);
    return true;
}


static bool
brep_expansion_append(brep_expansion &result, double value)
{
    if (std::fpclassify(value) == FP_ZERO)
	return true;
    if (!std::isfinite(value) ||
	    result.count >= RT_BREP_EXPANSION_CAPACITY)
	return false;
    result.component[result.count++] = value;
    return true;
}


static bool
brep_expansion_grow(const brep_expansion &input, double value,
    brep_expansion &result, size_t &high_water)
{
    if (!input.count || input.count > RT_BREP_EXPANSION_CAPACITY ||
	    !std::isfinite(value))
	return false;
    result.count = 0;
    double accumulator = value;
    for (size_t i = 0; i < input.count; ++i) {
	double sum = 0.0;
	double error = 0.0;
	if (!brep_two_sum(accumulator, input.component[i], sum, error) ||
		!brep_expansion_append(result, error))
	    return false;
	accumulator = sum;
    }
    if (!brep_expansion_append(result, accumulator))
	return false;
    if (!result.count && !brep_expansion_set(result, 0.0, high_water))
	return false;
    high_water = std::max(high_water, result.count);
    return true;
}


static bool
brep_expansion_add(const brep_expansion &first,
    const brep_expansion &second, brep_expansion &result,
    size_t &high_water)
{
    if (!first.count || !second.count ||
	    first.count > RT_BREP_EXPANSION_CAPACITY ||
	    second.count > RT_BREP_EXPANSION_CAPACITY)
	return false;
    brep_expansion current = first;
    brep_expansion next = {};
    for (size_t i = 0; i < second.count; ++i) {
	if (!brep_expansion_grow(current, second.component[i], next,
		high_water))
	    return false;
	current = next;
    }
    result = current;
    return true;
}


static bool
brep_expansion_scale(const brep_expansion &input, double scale,
    brep_expansion &result, size_t &high_water)
{
    if (!input.count || input.count > RT_BREP_EXPANSION_CAPACITY ||
	    !std::isfinite(scale))
	return false;
    if (std::fpclassify(scale) == FP_ZERO)
	return brep_expansion_set(result, 0.0, high_water);
    brep_expansion current = {};
    if (!brep_expansion_set(current, 0.0, high_water))
	return false;
    for (size_t i = 0; i < input.count; ++i) {
	double product = 0.0;
	double error = 0.0;
	if (!brep_two_product(input.component[i], scale, product, error))
	    return false;
	brep_expansion term = {};
	term.count = 0;
	if (!brep_expansion_append(term, error) ||
		!brep_expansion_append(term, product))
	    return false;
	if (!term.count && !brep_expansion_set(term, 0.0, high_water))
	    return false;
	brep_expansion next = {};
	if (!brep_expansion_add(current, term, next, high_water))
	    return false;
	current = next;
    }
    result = current;
    return true;
}


static bool
brep_expansion_lerp(const brep_expansion &first,
    const brep_expansion &second, double parameter, brep_expansion &result,
    size_t &high_water)
{
    if (parameter < 0.0 || parameter > 1.0 ||
	    !std::isfinite(parameter))
	return false;
    brep_expansion negative_first = {};
    brep_expansion difference = {};
    brep_expansion scaled = {};
    if (!brep_expansion_scale(first, -1.0, negative_first, high_water) ||
	    !brep_expansion_add(second, negative_first, difference,
		high_water) ||
	    !brep_expansion_scale(difference, parameter, scaled,
		high_water) ||
	    !brep_expansion_add(first, scaled, result, high_water))
	return false;
    return true;
}


static bool
brep_expansion_interval_lerp(const brep_expansion_interval &first,
    const brep_expansion_interval &second, double parameter,
    brep_expansion_interval &result, size_t &high_water)
{
    if (!brep_expansion_lerp(first.minimum, second.minimum, parameter,
	    result.minimum, high_water) ||
	    !brep_expansion_lerp(first.maximum, second.maximum, parameter,
		result.maximum, high_water))
	return false;
    return true;
}


static bool
brep_expansion_bounds(const brep_expansion &value, brep_interval &bounds)
{
    if (!value.count || value.count > RT_BREP_EXPANSION_CAPACITY)
	return false;
    size_t center_index = 0;
    for (size_t i = 0; i < value.count; ++i) {
	if (!std::isfinite(value.component[i]))
	    return false;
	if (fabs(value.component[i]) > fabs(value.component[center_index]))
	    center_index = i;
    }
    double radius = 0.0;
    for (size_t i = 0; i < value.count; ++i) {
	if (i == center_index)
	    continue;
	if (std::fpclassify(value.component[i]) == FP_ZERO)
	    continue;
	radius = std::nextafter(radius + fabs(value.component[i]), INFINITY);
	if (!std::isfinite(radius))
	    return false;
    }
    const double center = value.component[center_index];
    if (std::fpclassify(radius) == FP_ZERO) {
	bounds.minimum = center;
	bounds.maximum = center;
	return true;
    }
    bounds.minimum = std::nextafter(center - radius, -INFINITY);
    bounds.maximum = std::nextafter(center + radius, INFINITY);
    return std::isfinite(bounds.minimum) && std::isfinite(bounds.maximum) &&
	bounds.minimum <= bounds.maximum;
}


static bool
brep_expansion_exact_ldexp(double input, int exponent, double &output)
{
    if (!std::isfinite(input))
	return false;
    output = std::ldexp(input, exponent);
    if (!std::isfinite(output))
	return false;
    const double restored = std::ldexp(output, -exponent);
    return std::isfinite(restored) &&
	std::memcmp(&restored, &input, sizeof(input)) == 0;
}


static bool
brep_expansion_outward_ldexp(double input, int exponent, bool lower,
    double &output)
{
    if (!std::isfinite(input))
	return false;
    output = std::ldexp(input, exponent);
    if (!std::isfinite(output))
	return false;
    const double restored = std::ldexp(output, -exponent);
    if (!std::isfinite(restored) ||
	    std::memcmp(&restored, &input, sizeof(input)) != 0) {
	output = std::nextafter(output, lower ? -INFINITY : INFINITY);
    }
    return std::isfinite(output);
}


static bool
brep_single_coefficient_expansion_intervals(const double cv[4],
    const double origin[3], const double planes[2][3],
    brep_interval function[2], size_t &high_water)
{
    if (!cv || !origin || !planes)
	return false;
    brep_expansion numerator[3];
    for (int component = 0; component < 3; ++component) {
	if (!std::isfinite(cv[component]) ||
		!std::isfinite(origin[component]) ||
		!std::isfinite(cv[3]) ||
		!std::isfinite(planes[0][component]) ||
		!std::isfinite(planes[1][component]))
	    return false;
	brep_expansion cv_value = {};
	brep_expansion origin_value = {};
	brep_expansion weighted_origin = {};
	brep_expansion negative_origin = {};
	if (!brep_expansion_set(cv_value, cv[component], high_water) ||
		!brep_expansion_set(origin_value, origin[component], high_water) ||
		!brep_expansion_scale(origin_value, cv[3], weighted_origin,
		    high_water) ||
		!brep_expansion_scale(weighted_origin, -1.0, negative_origin,
		    high_water) ||
		!brep_expansion_add(cv_value, negative_origin,
		    numerator[component], high_water))
	    return false;
    }

    for (int equation = 0; equation < 2; ++equation) {
	brep_expansion dot = {};
	if (!brep_expansion_set(dot, 0.0, high_water))
	    return false;
	for (int component = 0; component < 3; ++component) {
	    brep_expansion term = {};
	    brep_expansion next = {};
	    if (!brep_expansion_scale(numerator[component],
		    planes[equation][component], term, high_water) ||
		    !brep_expansion_add(dot, term, next, high_water))
		return false;
	    dot = next;
	}
	if (!brep_expansion_bounds(dot, function[equation]))
	    return false;
    }
    return true;
}


static bool
brep_expansion_interval_from_interval(const brep_interval &input,
    brep_expansion_interval &result, size_t &high_water)
{
    if (!std::isfinite(input.minimum) || !std::isfinite(input.maximum) ||
	    input.minimum > input.maximum)
	return false;
    return brep_expansion_set(result.minimum, input.minimum, high_water) &&
	brep_expansion_set(result.maximum, input.maximum, high_water);
}


/*
 * Q_i on [minimum, maximum] is the blossom with degree-i copies of the
 * minimum followed by i copies of the maximum.  This avoids the a/b division
 * in the usual two-split restriction and keeps every binary64 parameter an
 * exact dyadic expansion operand.
 */
static bool
brep_expansion_bezier_blossom(const brep_expansion_interval *input,
    int order, double minimum, double maximum, int output_index,
    brep_expansion_interval &output, size_t &high_water)
{
    if (!input || order < 2 || order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    minimum < 0.0 || maximum > 1.0 || !(minimum < maximum) ||
	    output_index < 0 || output_index >= order)
	return false;
    brep_expansion_interval work[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int i = 0; i < order; ++i)
	work[i] = input[i];
    const int degree = order - 1;
    const int minimum_count = degree - output_index;
    for (int level = 1; level <= degree; ++level) {
	const double parameter = level <= minimum_count ? minimum : maximum;
	for (int i = 0; i <= degree - level; ++i) {
	    brep_expansion_interval next = {};
	    if (!brep_expansion_interval_lerp(work[i], work[i + 1],
		    parameter, next, high_water))
		return false;
	    work[i] = next;
	}
    }
    output = work[0];
    return true;
}


static bool
brep_expansion_surface_restrict(const brep_interval *input, int u_order,
    int v_order, double u_minimum, double u_maximum, double v_minimum,
    double v_maximum, bool normalized_output, brep_interval *output,
    size_t &high_water)
{
    if (!input || !output || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    u_minimum < 0.0 || u_maximum > 1.0 ||
	    v_minimum < 0.0 || v_maximum > 1.0 ||
	    !(u_minimum < u_maximum) || !(v_minimum < v_maximum))
	return false;

    double maximum_magnitude = 0.0;
    const size_t count = (size_t)u_order * v_order;
    for (size_t i = 0; i < count; ++i) {
	if (!std::isfinite(input[i].minimum) ||
		!std::isfinite(input[i].maximum) ||
		input[i].minimum > input[i].maximum)
	    return false;
	maximum_magnitude = std::max(maximum_magnitude,
	    std::max(fabs(input[i].minimum), fabs(input[i].maximum)));
    }
    int equation_exponent = 0;
    if (std::fpclassify(maximum_magnitude) != FP_ZERO)
	(void)std::frexp(maximum_magnitude, &equation_exponent);

    brep_interval normalized_input[BREP_DIRECT_BEZIER_MAX_CVS];
    for (size_t i = 0; i < count; ++i) {
	if (!brep_expansion_exact_ldexp(input[i].minimum,
		-equation_exponent, normalized_input[i].minimum) ||
		!brep_expansion_exact_ldexp(input[i].maximum,
		    -equation_exponent, normalized_input[i].maximum))
	    return false;
    }
    brep_interval ordinary[BREP_DIRECT_BEZIER_MAX_CVS];
    if (!brep_interval_surface_restrict(normalized_input, u_order, v_order,
	    u_minimum, u_maximum, v_minimum, v_maximum, ordinary))
	return false;

    brep_expansion_interval source[BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_expansion_interval u_control[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int output_u = 0; output_u < u_order; ++output_u) {
	for (int j = 0; j < v_order; ++j) {
	    for (int i = 0; i < u_order; ++i) {
		if (!brep_expansion_interval_from_interval(
			normalized_input[(size_t)i * v_order + j], source[i],
			high_water))
		    return false;
	    }
	    if (!brep_expansion_bezier_blossom(source, u_order, u_minimum,
		    u_maximum, output_u, u_control[j], high_water))
		return false;
	}
	for (int output_v = 0; output_v < v_order; ++output_v) {
	    brep_expansion_interval restricted = {};
	    if (!brep_expansion_bezier_blossom(u_control, v_order,
		    v_minimum, v_maximum, output_v, restricted, high_water))
		return false;
	    brep_interval lower_bounds;
	    brep_interval upper_bounds;
	    if (!brep_expansion_bounds(restricted.minimum, lower_bounds) ||
		    !brep_expansion_bounds(restricted.maximum, upper_bounds) ||
		    lower_bounds.minimum > upper_bounds.maximum)
		return false;
	    brep_interval result = {
		lower_bounds.minimum, upper_bounds.maximum
	    };
	    const size_t output_index =
		(size_t)output_u * v_order + output_v;
	    result.minimum = std::max(result.minimum,
		ordinary[output_index].minimum);
	    result.maximum = std::min(result.maximum,
		ordinary[output_index].maximum);
	    if (result.minimum > result.maximum)
		return false;
	    if (!normalized_output) {
		if (!brep_expansion_outward_ldexp(result.minimum,
			equation_exponent, true, result.minimum) ||
			!brep_expansion_outward_ldexp(result.maximum,
			    equation_exponent, false, result.maximum))
		    return false;
	    }
	    output[output_index] = result;
	}
    }
    return true;
}


static bool
brep_expansion_bezier_evaluate(const brep_expansion_interval *input,
    int order, double parameter, brep_expansion_interval &output,
    size_t &high_water)
{
    if (!input || order < 1 || order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    parameter < 0.0 || parameter > 1.0)
	return false;
    brep_expansion_interval work[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int i = 0; i < order; ++i)
	work[i] = input[i];
    for (int level = 1; level < order; ++level) {
	for (int i = 0; i < order - level; ++i) {
	    brep_expansion_interval next = {};
	    if (!brep_expansion_interval_lerp(work[i], work[i + 1],
		    parameter, next, high_water))
		return false;
	    work[i] = next;
	}
    }
    output = work[0];
    return true;
}


static bool
brep_expansion_surface_evaluate_coefficients(const brep_interval *input,
    int u_order, int v_order, const double parameter[2],
    brep_interval &value, size_t &high_water)
{
    if (!input || !parameter || u_order < 1 || v_order < 1 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    parameter[0] < 0.0 || parameter[0] > 1.0 ||
	    parameter[1] < 0.0 || parameter[1] > 1.0)
	return false;
    brep_expansion_interval source[BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_expansion_interval v_control[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int j = 0; j < v_order; ++j) {
	for (int i = 0; i < u_order; ++i) {
	    if (!brep_expansion_interval_from_interval(
		    input[(size_t)i * v_order + j], source[i], high_water))
		return false;
	}
	if (!brep_expansion_bezier_evaluate(source, u_order, parameter[0],
		v_control[j], high_water))
	    return false;
    }
    brep_expansion_interval evaluated = {};
    if (!brep_expansion_bezier_evaluate(v_control, v_order, parameter[1],
	    evaluated, high_water))
	return false;
    brep_interval lower;
    brep_interval upper;
    brep_interval ordinary;
    if (!brep_expansion_bounds(evaluated.minimum, lower) ||
	    !brep_expansion_bounds(evaluated.maximum, upper) ||
	    !brep_interval_surface_evaluate_coefficients(input, u_order,
		v_order, parameter, ordinary))
	return false;
    value.minimum = std::max(lower.minimum, ordinary.minimum);
    value.maximum = std::min(upper.maximum, ordinary.maximum);
    return std::isfinite(value.minimum) && std::isfinite(value.maximum) &&
	value.minimum <= value.maximum;
}


/*
 * Enclose a tensor-product Bernstein surface after fixing one parameter.
 * The remaining Bernstein coefficients form a univariate convex hull, so
 * their interval hull encloses every value on the corresponding parameter
 * line.  This is the interval analogue of evaluating only one stage of a
 * tensor-product de Casteljau scheme.
 */
static bool
brep_expansion_surface_fixed_parameter_hull(const brep_interval *input,
    int u_order, int v_order, int direction, double parameter,
    brep_interval &hull, size_t &high_water)
{
    if (!input || (direction != 0 && direction != 1) ||
	    u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    parameter < 0.0 || parameter > 1.0)
	return false;
    const int direction_order = direction == 0 ? u_order : v_order;
    const int other_order = direction == 0 ? v_order : u_order;
    hull.minimum = DBL_MAX;
    hull.maximum = -DBL_MAX;
    for (int other = 0; other < other_order; ++other) {
	brep_expansion_interval source[BREP_DIRECT_BEZIER_MAX_ORDER];
	for (int i = 0; i < direction_order; ++i) {
	    const int u = direction == 0 ? i : other;
	    const int v = direction == 0 ? other : i;
	    if (!brep_expansion_interval_from_interval(
		    input[(size_t)u * v_order + v], source[i], high_water))
		return false;
	}
	brep_expansion_interval evaluated = {};
	brep_interval lower;
	brep_interval upper;
	if (!brep_expansion_bezier_evaluate(source, direction_order,
		parameter, evaluated, high_water) ||
		!brep_expansion_bounds(evaluated.minimum, lower) ||
		!brep_expansion_bounds(evaluated.maximum, upper) ||
		lower.minimum > upper.maximum)
	    return false;
	hull.minimum = std::min(hull.minimum, lower.minimum);
	hull.maximum = std::max(hull.maximum, upper.maximum);
    }
    return std::isfinite(hull.minimum) && std::isfinite(hull.maximum) &&
	hull.minimum <= hull.maximum;
}


static bool
brep_expansion_difference_scaled(const brep_interval &next,
    const brep_interval &previous, double scale, brep_interval &result,
    size_t &high_water)
{
    brep_expansion next_value = {};
    brep_expansion previous_value = {};
    brep_expansion negative_previous = {};
    brep_expansion difference = {};
    brep_expansion scaled = {};
    brep_interval lower;
    if (!brep_expansion_set(next_value, next.minimum, high_water) ||
	    !brep_expansion_set(previous_value, previous.maximum, high_water) ||
	    !brep_expansion_scale(previous_value, -1.0, negative_previous,
		high_water) ||
	    !brep_expansion_add(next_value, negative_previous, difference,
		high_water) ||
	    !brep_expansion_scale(difference, scale, scaled, high_water) ||
	    !brep_expansion_bounds(scaled, lower))
	return false;

    if (!brep_expansion_set(next_value, next.maximum, high_water) ||
	    !brep_expansion_set(previous_value, previous.minimum, high_water) ||
	    !brep_expansion_scale(previous_value, -1.0, negative_previous,
		high_water) ||
	    !brep_expansion_add(next_value, negative_previous, difference,
		high_water) ||
	    !brep_expansion_scale(difference, scale, scaled, high_water))
	return false;
    brep_interval upper;
    if (!brep_expansion_bounds(scaled, upper))
	return false;
    result = {lower.minimum, upper.maximum};
    return std::isfinite(result.minimum) && std::isfinite(result.maximum) &&
	result.minimum <= result.maximum;
}


static bool
brep_expansion_surface_derivative_hull(const brep_interval *input,
    int u_order, int v_order, int direction, brep_interval &hull,
    size_t &high_water)
{
    if (!input || (direction != 0 && direction != 1) ||
	    u_order < 2 || v_order < 2)
	return false;
    brep_interval ordinary[BREP_DIRECT_BEZIER_MAX_CVS];
    int ordinary_order[2];
    if (!brep_interval_surface_derivative_coefficients(input, u_order,
	    v_order, direction, ordinary, ordinary_order))
	return false;
    const int degree = direction == 0 ? u_order - 1 : v_order - 1;
    hull.minimum = DBL_MAX;
    hull.maximum = -DBL_MAX;
    for (int i = 0; i < ordinary_order[0]; ++i) {
	for (int j = 0; j < ordinary_order[1]; ++j) {
	    const size_t previous = (size_t)i * v_order + j;
	    const size_t next = direction == 0 ?
		(size_t)(i + 1) * v_order + j :
		(size_t)i * v_order + j + 1;
	    brep_interval derivative;
	    if (!brep_expansion_difference_scaled(input[next],
		    input[previous], degree, derivative, high_water))
		return false;
	    const size_t index = (size_t)i * ordinary_order[1] + j;
	    derivative.minimum = std::max(derivative.minimum,
		ordinary[index].minimum);
	    derivative.maximum = std::min(derivative.maximum,
		ordinary[index].maximum);
	    if (derivative.minimum > derivative.maximum)
		return false;
	    hull.minimum = std::min(hull.minimum, derivative.minimum);
	    hull.maximum = std::max(hull.maximum, derivative.maximum);
	}
    }
    return std::isfinite(hull.minimum) && std::isfinite(hull.maximum) &&
	hull.minimum <= hull.maximum;
}


static bool
brep_expansion_interval_add_tight(const brep_interval &first,
    const brep_interval &second, brep_interval &result, size_t &high_water)
{
    const brep_interval ordinary = brep_interval_add(first, second);
    brep_expansion first_value = {};
    brep_expansion second_value = {};
    brep_expansion sum = {};
    brep_interval lower;
    if (!brep_expansion_set(first_value, first.minimum, high_water) ||
	    !brep_expansion_set(second_value, second.minimum, high_water) ||
	    !brep_expansion_add(first_value, second_value, sum, high_water) ||
	    !brep_expansion_bounds(sum, lower))
	return false;
    brep_interval upper;
    if (!brep_expansion_set(first_value, first.maximum, high_water) ||
	    !brep_expansion_set(second_value, second.maximum, high_water) ||
	    !brep_expansion_add(first_value, second_value, sum, high_water) ||
	    !brep_expansion_bounds(sum, upper))
	return false;
    result.minimum = std::max(lower.minimum, ordinary.minimum);
    result.maximum = std::min(upper.maximum, ordinary.maximum);
    return result.minimum <= result.maximum;
}


static bool
brep_expansion_interval_scale_tight(double scale,
    const brep_interval &value, brep_interval &result, size_t &high_water)
{
    const brep_interval ordinary = brep_interval_scale(scale, value);
    const bool negative = std::signbit(scale);
    const double lower_input = negative ? value.maximum : value.minimum;
    const double upper_input = negative ? value.minimum : value.maximum;
    brep_expansion input_value = {};
    brep_expansion scaled = {};
    brep_interval lower;
    if (!brep_expansion_set(input_value, lower_input, high_water) ||
	    !brep_expansion_scale(input_value, scale, scaled, high_water) ||
	    !brep_expansion_bounds(scaled, lower))
	return false;
    brep_interval upper;
    if (!brep_expansion_set(input_value, upper_input, high_water) ||
	    !brep_expansion_scale(input_value, scale, scaled, high_water) ||
	    !brep_expansion_bounds(scaled, upper))
	return false;
    result.minimum = std::max(lower.minimum, ordinary.minimum);
    result.maximum = std::min(upper.maximum, ordinary.maximum);
    return result.minimum <= result.maximum;
}


static bool
brep_expansion_interval_multiply_tight(const brep_interval &first,
    const brep_interval &second, brep_interval &result, size_t &high_water)
{
    const brep_interval ordinary = brep_interval_multiply(first, second);
    const double first_endpoint[2] = {first.minimum, first.maximum};
    const double second_endpoint[2] = {second.minimum, second.maximum};
    double minimum = DBL_MAX;
    double maximum = -DBL_MAX;
    for (int i = 0; i < 2; ++i) {
	for (int j = 0; j < 2; ++j) {
	    brep_expansion input_value = {};
	    brep_expansion product = {};
	    brep_interval bounds;
	    if (!brep_expansion_set(input_value, first_endpoint[i],
		    high_water) ||
		    !brep_expansion_scale(input_value, second_endpoint[j],
			product, high_water) ||
		    !brep_expansion_bounds(product, bounds))
		return false;
	    minimum = std::min(minimum, bounds.minimum);
	    maximum = std::max(maximum, bounds.maximum);
	}
    }
    result.minimum = std::max(minimum, ordinary.minimum);
    result.maximum = std::min(maximum, ordinary.maximum);
    return result.minimum <= result.maximum;
}


static bool
brep_expansion_linear_coefficients(
    const brep_interval input[2][BREP_DIRECT_BEZIER_MAX_CVS], size_t count,
    const double transform[2][2],
    brep_interval output[2][BREP_DIRECT_BEZIER_MAX_CVS],
    size_t &high_water)
{
    if (!input || !transform || !output ||
	    count > BREP_DIRECT_BEZIER_MAX_CVS)
	return false;
    for (int row = 0; row < 2; ++row) {
	for (int equation = 0; equation < 2; ++equation) {
	    if (!std::isfinite(transform[row][equation]))
		return false;
	}
	for (size_t i = 0; i < count; ++i) {
	    brep_interval first;
	    brep_interval second;
	    if (!brep_expansion_interval_scale_tight(transform[row][0],
		    input[0][i], first, high_water) ||
		    !brep_expansion_interval_scale_tight(transform[row][1],
			input[1][i], second, high_water) ||
		    !brep_expansion_interval_add_tight(first, second,
			output[row][i], high_water))
		return false;
	}
    }
    return true;
}


static bool
brep_interval_coefficient_hull_excluded(const brep_interval *values,
    size_t count)
{
    if (!values || !count)
	return false;
    double minimum = DBL_MAX;
    double maximum = -DBL_MAX;
    for (size_t i = 0; i < count; ++i) {
	if (!std::isfinite(values[i].minimum) ||
		!std::isfinite(values[i].maximum) ||
		values[i].minimum > values[i].maximum)
	    return false;
	minimum = std::min(minimum, values[i].minimum);
	maximum = std::max(maximum, values[i].maximum);
    }
    return minimum > 0.0 || maximum < 0.0;
}


static bool
brep_interval_coefficients_power_two_normalize(brep_interval *values,
    size_t count)
{
    if (!values || !count || count > BREP_DIRECT_BEZIER_MAX_CVS)
	return false;
    double maximum_magnitude = 0.0;
    for (size_t i = 0; i < count; ++i) {
	if (!std::isfinite(values[i].minimum) ||
		!std::isfinite(values[i].maximum) ||
		values[i].minimum > values[i].maximum)
	    return false;
	maximum_magnitude = std::max(maximum_magnitude,
	    std::max(fabs(values[i].minimum), fabs(values[i].maximum)));
    }
    if (std::fpclassify(maximum_magnitude) == FP_ZERO)
	return true;
    int exponent = 0;
    (void)std::frexp(maximum_magnitude, &exponent);
    brep_interval normalized[BREP_DIRECT_BEZIER_MAX_CVS];
    for (size_t i = 0; i < count; ++i) {
	if (!brep_expansion_outward_ldexp(values[i].minimum, -exponent,
		true, normalized[i].minimum) ||
		!brep_expansion_outward_ldexp(values[i].maximum, -exponent,
		    false, normalized[i].maximum) ||
		normalized[i].minimum > normalized[i].maximum)
	    return false;
    }
    for (size_t i = 0; i < count; ++i)
	values[i] = normalized[i];
    return true;
}


static bool
brep_expansion_krawczyk_from_bounds(const brep_interval function[2],
    const brep_interval jacobian[2][2], const double root[2],
    struct rt_brep_krawczyk_test_result &result, size_t &high_water)
{
    if (!function || !jacobian || !root ||
	    root[0] < 0.0 || root[0] > 1.0 ||
	    root[1] < 0.0 || root[1] > 1.0)
	return false;
    double midpoint[2][2];
    for (int row = 0; row < 2; ++row) {
	for (int column = 0; column < 2; ++column) {
	    midpoint[row][column] =
		0.5 * jacobian[row][column].minimum +
		0.5 * jacobian[row][column].maximum;
	}
    }
    const double determinant = midpoint[0][0] * midpoint[1][1] -
	midpoint[0][1] * midpoint[1][0];
    const double first_scale = hypot(midpoint[0][0], midpoint[1][0]);
    const double second_scale = hypot(midpoint[0][1], midpoint[1][1]);
    if (!std::isfinite(determinant) || !(first_scale > DBL_MIN) ||
	    !(second_scale > DBL_MIN))
	return false;
    result.determinant_ratio =
	fabs(determinant / first_scale) / second_scale;
    if (!(fabs(determinant) > 0.0) ||
	    !std::isfinite(result.determinant_ratio))
	return false;
    const double inverse[2][2] = {
	{midpoint[1][1] / determinant, -midpoint[0][1] / determinant},
	{-midpoint[1][0] / determinant, midpoint[0][0] / determinant}
    };
    for (int row = 0; row < 2; ++row) {
	for (int column = 0; column < 2; ++column) {
	    if (!std::isfinite(inverse[row][column]))
		return false;
	}
    }
    result.available = 1;

    brep_interval center[2];
    for (int row = 0; row < 2; ++row) {
	brep_interval correction = {0.0, 0.0};
	for (int equation = 0; equation < 2; ++equation) {
	    brep_interval term;
	    brep_interval next;
	    if (!brep_expansion_interval_scale_tight(
		    inverse[row][equation], function[equation], term,
		    high_water) ||
		    !brep_expansion_interval_add_tight(correction, term, next,
			high_water))
		return false;
	    correction = next;
	}
	brep_interval negative_correction;
	if (!brep_expansion_interval_scale_tight(-1.0, correction,
		negative_correction, high_water) ||
	    !brep_expansion_interval_add_tight({root[row], root[row]},
		negative_correction, center[row], high_water))
	    return false;
    }

    brep_interval remainder[2][2];
    for (int row = 0; row < 2; ++row) {
	for (int column = 0; column < 2; ++column) {
	    brep_interval product = {0.0, 0.0};
	    for (int equation = 0; equation < 2; ++equation) {
		brep_interval term;
		brep_interval next;
		if (!brep_expansion_interval_scale_tight(
			inverse[row][equation],
			jacobian[equation][column], term, high_water) ||
			!brep_expansion_interval_add_tight(product, term, next,
			    high_water))
		    return false;
		product = next;
	    }
	    if (!brep_expansion_interval_scale_tight(-1.0, product,
		    remainder[row][column], high_water))
		return false;
	    if (row == column) {
		brep_interval next;
		if (!brep_expansion_interval_add_tight({1.0, 1.0},
			remainder[row][column], next, high_water))
		    return false;
		remainder[row][column] = next;
	    }
	}
    }

    const brep_interval offset[2] = {
	brep_interval_expanded(-root[0], 1.0 - root[0]),
	brep_interval_expanded(-root[1], 1.0 - root[1])
    };
    result.certified = 1;
    const double inclusion_margin = 512.0 * DBL_EPSILON;
    for (int row = 0; row < 2; ++row) {
	brep_interval image = center[row];
	for (int column = 0; column < 2; ++column) {
	    brep_interval product;
	    brep_interval next;
	    if (!brep_expansion_interval_multiply_tight(
		    remainder[row][column], offset[column], product,
		    high_water) ||
		    !brep_expansion_interval_add_tight(image, product, next,
			high_water))
		return false;
	    image = next;
	}
	result.image_minimum[row] = image.minimum;
	result.image_maximum[row] = image.maximum;
	if (!(image.minimum > inclusion_margin) ||
		!(image.maximum < 1.0 - inclusion_margin))
	    result.certified = 0;
    }
    return true;
}


static bool
brep_expansion_surface_krawczyk(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    int u_order, int v_order, const double root[2],
    struct rt_brep_krawczyk_test_result &result, size_t &high_water)
{
    if (!values || !root)
	return false;
    brep_interval function[2];
    brep_interval jacobian[2][2];
    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_expansion_surface_evaluate_coefficients(values[equation],
		u_order, v_order, root, function[equation], high_water))
	    return false;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!brep_expansion_surface_derivative_hull(values[equation],
		    u_order, v_order, direction,
		    jacobian[equation][direction], high_water))
		return false;
	}
    }
    return brep_expansion_krawczyk_from_bounds(function, jacobian, root,
	result, high_water);
}


extern "C" int
_rt_brep_expansion_interval_test(const fastf_t *first_coefficients,
    const fastf_t *second_coefficients, int u_order, int v_order,
    const fastf_t coefficient_error[2], const fastf_t root[2],
    struct rt_brep_interval_test_result *result,
    size_t *expansion_high_water)
{
    if (!first_coefficients || !second_coefficients || !coefficient_error ||
	    !root || !result || !expansion_high_water || u_order < 2 ||
	    v_order < 2 || u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    *result = {};
    brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const fastf_t *coefficient[2] = {
	first_coefficients, second_coefficients
    };
    const size_t count = (size_t)u_order * v_order;
    for (int equation = 0; equation < 2; ++equation) {
	if (!std::isfinite(coefficient_error[equation]) ||
		coefficient_error[equation] < 0.0)
	    return 0;
	for (size_t i = 0; i < count; ++i) {
	    if (!std::isfinite(coefficient[equation][i]))
		return 0;
	    values[equation][i] =
		std::fpclassify(coefficient_error[equation]) == FP_ZERO ?
		brep_interval{coefficient[equation][i],
		    coefficient[equation][i]} :
		brep_interval_expanded(coefficient[equation][i] -
		    coefficient_error[equation], coefficient[equation][i] +
		    coefficient_error[equation]);
	}
    }
    size_t high_water = 0;
    for (int equation = 0; equation < 2; ++equation) {
	brep_interval function;
	if (!brep_expansion_surface_evaluate_coefficients(values[equation],
		u_order, v_order, root, function, high_water))
	    return 0;
	result->function_minimum[equation] = function.minimum;
	result->function_maximum[equation] = function.maximum;
	for (int direction = 0; direction < 2; ++direction) {
	    brep_interval derivative;
	    if (!brep_expansion_surface_derivative_hull(values[equation],
		    u_order, v_order, direction, derivative, high_water))
		return 0;
	    result->jacobian_minimum[equation][direction] =
		derivative.minimum;
	    result->jacobian_maximum[equation][direction] =
		derivative.maximum;
	}
    }
    *expansion_high_water = high_water;
    return 1;
}


extern "C" int
_rt_brep_expansion_krawczyk_test(const fastf_t *first_coefficients,
    const fastf_t *first_error, const fastf_t *second_coefficients,
    const fastf_t *second_error, int u_order, int v_order,
    const fastf_t root[2], struct rt_brep_krawczyk_test_result *result,
    size_t *expansion_high_water)
{
    if (!first_coefficients || !first_error || !second_coefficients ||
	    !second_error || !root || !result || !expansion_high_water ||
	    u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    *result = {};
    brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const fastf_t *coefficient[2] = {
	first_coefficients, second_coefficients
    };
    const fastf_t *error[2] = {first_error, second_error};
    const size_t count = (size_t)u_order * v_order;
    for (int equation = 0; equation < 2; ++equation) {
	for (size_t i = 0; i < count; ++i) {
	    if (!std::isfinite(coefficient[equation][i]) ||
		    !std::isfinite(error[equation][i]) ||
		    error[equation][i] < 0.0)
		return 0;
	    values[equation][i] =
		std::fpclassify(error[equation][i]) == FP_ZERO ?
		brep_interval{coefficient[equation][i],
		    coefficient[equation][i]} :
		brep_interval_expanded(coefficient[equation][i] -
		    error[equation][i], coefficient[equation][i] +
		    error[equation][i]);
	}
    }
    size_t high_water = 0;
    if (!brep_expansion_surface_krawczyk(values, u_order, v_order, root,
	    *result, high_water))
	return 0;
    *expansion_high_water = high_water;
    return 1;
}


static bool
brep_expansion_surface_taylor_term(
    const brep_interval *input, int u_order, int v_order,
    int u_derivatives, int v_derivatives, const double root[2],
    brep_interval &term, size_t &high_water)
{
    if (!input || !root || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER || u_derivatives < 0 ||
	    v_derivatives < 0 || u_derivatives >= u_order ||
	    v_derivatives >= v_order || root[0] < 0.0 || root[0] > 1.0 ||
	    root[1] < 0.0 || root[1] > 1.0)
	return false;

    brep_interval first[BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval second[BREP_DIRECT_BEZIER_MAX_CVS];
    const size_t count = (size_t)u_order * v_order;
    for (size_t i = 0; i < count; ++i)
	first[i] = input[i];
    brep_interval *current = first;
    brep_interval *next = second;
    int current_u_order = u_order;
    int current_v_order = v_order;
    for (int derivative = 0; derivative < u_derivatives; ++derivative) {
	const double scale = current_u_order - 1;
	for (int i = 0; i < current_u_order - 1; ++i) {
	    for (int j = 0; j < current_v_order; ++j) {
		const size_t output = (size_t)i * current_v_order + j;
		const size_t previous = (size_t)i * current_v_order + j;
		const size_t following =
		    (size_t)(i + 1) * current_v_order + j;
		if (!brep_expansion_difference_scaled(current[following],
			current[previous], scale, next[output], high_water))
		    return false;
	    }
	}
	current_u_order--;
	std::swap(current, next);
    }
    for (int derivative = 0; derivative < v_derivatives; ++derivative) {
	const double scale = current_v_order - 1;
	for (int i = 0; i < current_u_order; ++i) {
	    for (int j = 0; j < current_v_order - 1; ++j) {
		const size_t output =
		    (size_t)i * (current_v_order - 1) + j;
		const size_t previous = (size_t)i * current_v_order + j;
		const size_t following = previous + 1;
		if (!brep_expansion_difference_scaled(current[following],
			current[previous], scale, next[output], high_water))
		    return false;
	    }
	}
	current_v_order--;
	std::swap(current, next);
    }

    brep_interval derivative_value;
    if (!brep_expansion_surface_evaluate_coefficients(current,
	    current_u_order, current_v_order, root, derivative_value,
	    high_water))
	return false;
    term = derivative_value;
    for (int i = 2; i <= u_derivatives; ++i) {
	if (std::fpclassify(term.minimum) == FP_ZERO &&
		std::fpclassify(term.maximum) == FP_ZERO)
	    continue;
	brep_interval quotient;
	if (!brep_interval_divide_nonzero(term, {(double)i, (double)i},
		quotient))
	    return false;
	term = quotient;
    }
    for (int i = 2; i <= v_derivatives; ++i) {
	if (std::fpclassify(term.minimum) == FP_ZERO &&
		std::fpclassify(term.maximum) == FP_ZERO)
	    continue;
	brep_interval quotient;
	if (!brep_interval_divide_nonzero(term, {(double)i, (double)i},
		quotient))
	    return false;
	term = quotient;
    }
    return true;
}


static double
brep_interval_absolute_upper(const brep_interval &value)
{
    return std::max(fabs(value.minimum), fabs(value.maximum));
}


static bool
brep_local_root_monomial(double radius, int u_power, int v_power,
    brep_interval &monomial)
{
    if (!(radius > 0.0) || !std::isfinite(radius) || u_power < 0 ||
	    v_power < 0)
	return false;
    const int degree = u_power + v_power;
    if (!degree) {
	monomial = {1.0, 1.0};
	return true;
    }
    double magnitude = 1.0;
    for (int i = 0; i < degree; ++i) {
	magnitude = std::nextafter(magnitude * radius, INFINITY);
	if (!std::isfinite(magnitude))
	    return false;
    }
    if (!(u_power % 2) && !(v_power % 2))
	monomial = {0.0, magnitude};
    else
	monomial = {-magnitude, magnitude};
    return true;
}


/* Certify one regular root in a small infinity-norm box around an existing
 * numerical solution.  The bivariate Bernstein system is converted to an
 * exact finite Taylor expansion at that solution.  A fixed binary64 inverse
 * supplies a contraction map, while fixed expansions make every function,
 * image, and derivative bound outward.  The Taylor box may straddle a span
 * boundary; callers must separately authorize and bound any such analytic
 * extension in model space. */
static bool
brep_expansion_local_root_mode(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    int u_order, int v_order, const double root[2], double maximum_radius,
    bool largest_certificate,
    struct rt_brep_local_root_test_result &result)
{
    result = {};
    if (!values || !root || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER || root[0] < 0.0 ||
	    root[0] > 1.0 || root[1] < 0.0 || root[1] > 1.0 ||
	    !(maximum_radius > 0.0) || !std::isfinite(maximum_radius))
	return false;
    result.normalized_root[0] = root[0];
    result.normalized_root[1] = root[1];

    brep_interval taylor[2][BREP_DIRECT_BEZIER_MAX_ORDER]
	[BREP_DIRECT_BEZIER_MAX_ORDER] = {};
    size_t high_water = 0;
    for (int equation = 0; equation < 2; ++equation) {
	for (int u_power = 0; u_power < u_order; ++u_power) {
	    for (int v_power = 0; v_power < v_order; ++v_power) {
		if (!brep_expansion_surface_taylor_term(values[equation],
			u_order, v_order, u_power, v_power, root,
			taylor[equation][u_power][v_power], high_water))
		    return false;
	    }
	}
    }

    double midpoint[2][2];
    for (int equation = 0; equation < 2; ++equation) {
	const brep_interval derivative[2] = {
	    taylor[equation][1][0], taylor[equation][0][1]
	};
	for (int direction = 0; direction < 2; ++direction) {
	    midpoint[equation][direction] =
		0.5 * derivative[direction].minimum +
		0.5 * derivative[direction].maximum;
	}
    }
    const double determinant = std::fma(midpoint[0][0], midpoint[1][1],
	-midpoint[0][1] * midpoint[1][0]);
    const double first_scale = hypot(midpoint[0][0], midpoint[1][0]);
    const double second_scale = hypot(midpoint[0][1], midpoint[1][1]);
    if (!std::isfinite(determinant) || !(fabs(determinant) > 0.0) ||
	    !(first_scale > DBL_MIN) || !(second_scale > DBL_MIN)) {
	result.expansion_high_water = high_water;
	return true;
    }
    const double inverse[2][2] = {
	{midpoint[1][1] / determinant, -midpoint[0][1] / determinant},
	{-midpoint[1][0] / determinant, midpoint[0][0] / determinant}
    };
    for (int row = 0; row < 2; ++row)
	for (int equation = 0; equation < 2; ++equation)
	    if (!std::isfinite(inverse[row][equation]))
		return false;
    brep_interval inverse_first;
    brep_interval inverse_second;
    brep_interval inverse_negative_second;
    brep_interval inverse_determinant;
    if (!brep_expansion_interval_multiply_tight(
	    {inverse[0][0], inverse[0][0]},
	    {inverse[1][1], inverse[1][1]}, inverse_first, high_water) ||
	!brep_expansion_interval_multiply_tight(
	    {inverse[0][1], inverse[0][1]},
	    {inverse[1][0], inverse[1][0]}, inverse_second, high_water) ||
	!brep_expansion_interval_scale_tight(-1.0, inverse_second,
	    inverse_negative_second, high_water) ||
	!brep_expansion_interval_add_tight(inverse_first,
	    inverse_negative_second, inverse_determinant, high_water))
	return false;
    if (inverse_determinant.minimum <= 0.0 &&
	    inverse_determinant.maximum >= 0.0) {
	result.expansion_high_water = high_water;
	return true;
    }

    brep_interval correction[2];
    double correction_bound = 0.0;
    for (int row = 0; row < 2; ++row) {
	brep_interval sum = {0.0, 0.0};
	for (int equation = 0; equation < 2; ++equation) {
	    brep_interval term;
	    brep_interval next;
	    if (!brep_expansion_interval_scale_tight(
		    -inverse[row][equation], taylor[equation][0][0], term,
		    high_water) ||
		    !brep_expansion_interval_add_tight(sum, term, next,
			high_water))
		return false;
	    sum = next;
	}
	correction[row] = sum;
	correction_bound = std::max(correction_bound,
	    brep_interval_absolute_upper(sum));
    }
    if (!std::isfinite(correction_bound))
	return false;

    brep_interval linear[2][2];
    for (int row = 0; row < 2; ++row) {
	for (int direction = 0; direction < 2; ++direction) {
	    brep_interval value = {
		row == direction ? 1.0 : 0.0,
		row == direction ? 1.0 : 0.0
	    };
	    for (int equation = 0; equation < 2; ++equation) {
		const brep_interval &derivative = direction ?
		    taylor[equation][0][1] : taylor[equation][1][0];
		brep_interval product;
		brep_interval next;
		if (!brep_expansion_interval_scale_tight(
			-inverse[row][equation], derivative, product,
			high_water) ||
		    !brep_expansion_interval_add_tight(value, product, next,
			high_water))
		    return false;
		value = next;
	    }
	    linear[row][direction] = value;
	}
    }

    result.available = 1;
    result.determinant_ratio =
	fabs(determinant / first_scale) / second_scale;
    result.correction_bound = correction_bound;
    const double root_scale = std::max(1.0,
	std::max(fabs(root[0]), fabs(root[1])));
    double radius = std::max(
	std::nextafter(4.0 * correction_bound, INFINITY),
	1024.0 * DBL_EPSILON * root_scale);
    struct rt_brep_local_root_test_result best = {};
    bool have_best = false;
    for (size_t attempt = 0; attempt < 48 &&
	    radius <= maximum_radius; ++attempt) {
	result.attempts++;
	brep_interval image[2] = {correction[0], correction[1]};
	brep_interval contraction[2][2];
	for (int row = 0; row < 2; ++row) {
	    for (int direction = 0; direction < 2; ++direction) {
		brep_interval delta = {-radius, radius};
		brep_interval product;
		brep_interval next;
		if (!brep_expansion_interval_multiply_tight(
			linear[row][direction], delta, product, high_water) ||
		    !brep_expansion_interval_add_tight(image[row], product,
			next, high_water))
		    return false;
		image[row] = next;
	    }
	    for (int direction = 0; direction < 2; ++direction) {
		brep_interval value = {
		    row == direction ? 1.0 : 0.0,
		    row == direction ? 1.0 : 0.0
		};
		for (int equation = 0; equation < 2; ++equation) {
		    brep_interval derivative = {0.0, 0.0};
		    for (int u_power = 0; u_power < u_order; ++u_power) {
			for (int v_power = 0; v_power < v_order;
				++v_power) {
			    const int exponent = direction ? v_power : u_power;
			    if (!exponent)
				continue;
			    brep_interval coefficient;
			    brep_interval monomial;
			    brep_interval product;
			    brep_interval next;
			    if (!brep_expansion_interval_scale_tight(exponent,
				    taylor[equation][u_power][v_power],
				    coefficient, high_water) ||
				!brep_local_root_monomial(radius,
				    u_power - (direction ? 0 : 1),
				    v_power - (direction ? 1 : 0),
				    monomial) ||
				!brep_expansion_interval_multiply_tight(
				    coefficient, monomial, product, high_water) ||
				!brep_expansion_interval_add_tight(derivative,
				    product, next, high_water))
				return false;
			    derivative = next;
			}
		    }
		    brep_interval product;
		    brep_interval next;
		    if (!brep_expansion_interval_scale_tight(
			    -inverse[row][equation], derivative, product,
			    high_water) ||
			!brep_expansion_interval_add_tight(value, product, next,
			    high_water))
			return false;
		    value = next;
		}
		contraction[row][direction] = value;
	    }
	    for (int equation = 0; equation < 2; ++equation) {
		for (int u_power = 0; u_power < u_order; ++u_power) {
		    for (int v_power = 0; v_power < v_order; ++v_power) {
			if (u_power + v_power < 2)
			    continue;
			brep_interval monomial;
			brep_interval term;
			brep_interval transformed;
			brep_interval next;
			if (!brep_local_root_monomial(radius, u_power,
				v_power, monomial) ||
			    !brep_expansion_interval_multiply_tight(
				taylor[equation][u_power][v_power],
				monomial, term, high_water) ||
			    !brep_expansion_interval_scale_tight(
				-inverse[row][equation], term, transformed,
				high_water) ||
			    !brep_expansion_interval_add_tight(image[row],
				transformed, next, high_water))
			    return false;
			image[row] = next;
		    }
		}
	    }
	}

	double contraction_bound = 0.0;
	for (int row = 0; row < 2; ++row) {
	    double row_bound = 0.0;
	    for (int direction = 0; direction < 2; ++direction)
		row_bound = std::nextafter(row_bound +
		    brep_interval_absolute_upper(
			contraction[row][direction]), INFINITY);
	    contraction_bound = std::max(contraction_bound, row_bound);
	}
	result.radius = radius;
	result.contraction_bound = contraction_bound;
	for (int direction = 0; direction < 2; ++direction) {
	    result.image_minimum[direction] = image[direction].minimum;
	    result.image_maximum[direction] = image[direction].maximum;
	}
	const double inclusion_margin = 512.0 * DBL_EPSILON * radius;
	const bool included = image[0].minimum > -radius + inclusion_margin &&
	    image[0].maximum < radius - inclusion_margin &&
	    image[1].minimum > -radius + inclusion_margin &&
	    image[1].maximum < radius - inclusion_margin;
	if (included && contraction_bound < 1.0) {
	    result.certified = 1;
	    best = result;
	    have_best = true;
	    if (!largest_certificate || radius > 0.5 * maximum_radius)
		break;
	    radius *= 2.0;
	    continue;
	}
	if (have_best)
	    break;
	if (radius > 0.5 * maximum_radius)
	    break;
	radius *= 2.0;
    }
    if (have_best) {
	const size_t attempts = result.attempts;
	result = best;
	result.attempts = attempts;
    }
    result.expansion_high_water = high_water;
    return true;
}


static bool
brep_expansion_local_root(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    int u_order, int v_order, const double root[2], double maximum_radius,
    struct rt_brep_local_root_test_result &result)
{
    return brep_expansion_local_root_mode(values, u_order, v_order, root,
	maximum_radius, false, result);
}


static bool
brep_expansion_surface_local_box(const brep_interval *input, int u_order,
    int v_order, const double root[2], double radius, brep_interval &bound,
    size_t &high_water)
{
    if (!input || !root || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER || !(radius > 0.0) ||
	    !std::isfinite(radius))
	return false;
    bound = {0.0, 0.0};
    for (int u_power = 0; u_power < u_order; ++u_power) {
	for (int v_power = 0; v_power < v_order; ++v_power) {
	    brep_interval coefficient;
	    brep_interval monomial;
	    brep_interval term;
	    brep_interval next;
	    if (!brep_expansion_surface_taylor_term(input, u_order, v_order,
		    u_power, v_power, root, coefficient, high_water) ||
		!brep_local_root_monomial(radius, u_power, v_power,
		    monomial) ||
		!brep_expansion_interval_multiply_tight(coefficient, monomial,
		    term, high_water) ||
		!brep_expansion_interval_add_tight(bound, term, next,
		    high_water))
		return false;
	    bound = next;
	}
    }
    return std::isfinite(bound.minimum) && std::isfinite(bound.maximum) &&
	bound.minimum <= bound.maximum;
}


/* Bound the complete rational image of the local analytic root box relative
 * to the nominal surface point.  Homogeneous coordinate differences and the
 * weight polynomial are evaluated as exact finite Taylor enclosures, so the
 * bound remains valid when the box extends beyond a Bezier span boundary.
 * Strictly positive weight is required throughout that extended box. */
static bool
brep_surface_local_image_bound(const brep_surface_span &span,
    const double root[2], double radius,
    struct rt_brep_local_root_test_result &result)
{
    const int u_order = span.surface.Order(0);
    const int v_order = span.surface.Order(1);
    if (!root || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER || root[0] < 0.0 ||
	    root[0] > 1.0 || root[1] < 0.0 || root[1] > 1.0 ||
	    !(radius > 0.0) || !std::isfinite(radius))
	return false;
    const ON_3dPoint center = span.surface.PointAt(root[0], root[1]);
    if (!center.IsValid())
	return false;
    const double center_coordinate[3] = {center.x, center.y, center.z};
    brep_interval difference[3][BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval weight[BREP_DIRECT_BEZIER_MAX_CVS];
    size_t high_water = 0;
    for (int i = 0; i < u_order; ++i) {
	for (int j = 0; j < v_order; ++j) {
	    ON_4dPoint cv;
	    if (!span.surface.GetCV(i, j, cv) || !cv.IsValid() ||
		    !(cv.w > 0.0) || !std::isfinite(cv.w))
		return false;
	    const size_t index = (size_t)i * v_order + j;
	    const double coordinate[3] = {cv.x, cv.y, cv.z};
	    weight[index] = {cv.w, cv.w};
	    for (int component = 0; component < 3; ++component) {
		brep_interval centered_weight;
		if (!brep_expansion_interval_scale_tight(
			-center_coordinate[component], weight[index],
			centered_weight, high_water) ||
		    !brep_expansion_interval_add_tight(
			{coordinate[component], coordinate[component]},
			centered_weight, difference[component][index],
			high_water))
		    return false;
	    }
	}
    }

    brep_interval weight_bound;
    if (!brep_expansion_surface_local_box(weight, u_order, v_order, root,
	    radius, weight_bound, high_water) ||
	    !(weight_bound.minimum > 0.0))
	return false;
    brep_interval squared_displacement = {0.0, 0.0};
    for (int component = 0; component < 3; ++component) {
	brep_interval numerator;
	brep_interval quotient;
	if (!brep_expansion_surface_local_box(difference[component], u_order,
		v_order, root, radius, numerator, high_water) ||
	    !brep_interval_divide_nonzero(numerator, weight_bound, quotient))
	    return false;
	const double absolute = brep_interval_absolute_upper(quotient);
	const brep_interval square = brep_interval_multiply(
	    {absolute, absolute}, {absolute, absolute});
	squared_displacement = brep_interval_add(squared_displacement, square);
    }
    if (!(squared_displacement.maximum >= 0.0) ||
	    !std::isfinite(squared_displacement.maximum))
	return false;
    const double displacement = std::nextafter(
	sqrt(squared_displacement.maximum), INFINITY);
    if (!std::isfinite(displacement))
	return false;
    result.model_image_available = 1;
    result.model_expansion_high_water = high_water;
    result.model_image_displacement = displacement;
    result.weight_minimum = weight_bound.minimum;
    result.weight_maximum = weight_bound.maximum;
    return true;
}


static bool
brep_surface_local_root_certificate_mode(const brep_surface_span &span,
    const ON_Ray &ray, const double uv[2], double maximum_radius,
    bool largest_certificate,
    struct rt_brep_local_root_test_result &result)
{
    result = {};
    if (!uv || !span.surface_domain[0].IsIncreasing() ||
	    !span.surface_domain[1].IsIncreasing() ||
	    !(maximum_radius > 0.0) || !std::isfinite(maximum_radius))
	return false;
    const double root[2] = {
	span.surface_domain[0].NormalizedParameterAt(uv[0]),
	span.surface_domain[1].NormalizedParameterAt(uv[1])
    };
    if (!std::isfinite(root[0]) || !std::isfinite(root[1]) ||
	    root[0] < 0.0 || root[0] > 1.0 || root[1] < 0.0 ||
	    root[1] > 1.0)
	return false;
    ON_3dVector first;
    ON_3dVector second;
    brep_surface_coefficients coefficients;
    if (!brep_ray_plane_frame(ray, first, second) ||
	!brep_surface_coefficients_init(coefficients, span, ray, first,
	    second))
	return false;
    if (!coefficients.expansion_available)
	return true;
    if (!brep_expansion_local_root_mode(
	    coefficients.value_expansion_interval, coefficients.order[0],
	    coefficients.order[1], root, maximum_radius, largest_certificate,
	    result))
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	result.span_minimum[direction] =
	    span.surface_domain[direction].Min();
	result.span_maximum[direction] =
	    span.surface_domain[direction].Max();
    }
    if (result.certified)
	(void)brep_surface_local_image_bound(span, root, result.radius, result);
    return true;
}


static bool
brep_surface_local_root_certificate(const brep_surface_span &span,
    const ON_Ray &ray, const double uv[2], double maximum_radius,
    struct rt_brep_local_root_test_result &result)
{
    return brep_surface_local_root_certificate_mode(span, ray, uv,
	maximum_radius, false, result);
}


static bool
brep_local_root_tube_contained(double image_displacement,
    double edge_distance, double trim_distance, double tolerance,
    double roundoff, double upper_bound[2])
{
    if (!upper_bound || !std::isfinite(image_displacement) ||
	    !std::isfinite(edge_distance) || !std::isfinite(trim_distance) ||
	    !std::isfinite(tolerance) || !std::isfinite(roundoff) ||
	    image_displacement < 0.0 || edge_distance < 0.0 ||
	    trim_distance < 0.0 || tolerance < 0.0 || roundoff < 0.0)
	return false;
    upper_bound[0] = std::nextafter(edge_distance + image_displacement,
	INFINITY);
    upper_bound[1] = std::nextafter(trim_distance + image_displacement,
	INFINITY);
    const double allowed = brep_interval_add({tolerance, tolerance},
	{roundoff, roundoff}).minimum;
    return std::isfinite(upper_bound[0]) &&
	std::isfinite(upper_bound[1]) && std::isfinite(allowed) &&
	upper_bound[0] <= allowed && upper_bound[1] <= allowed;
}


extern "C" int
_rt_brep_local_root_tube_test(fastf_t image_displacement,
    fastf_t edge_distance, fastf_t trim_distance, fastf_t tolerance,
    fastf_t roundoff, fastf_t upper_bound[2])
{
    return brep_local_root_tube_contained(image_displacement, edge_distance,
	trim_distance, tolerance, roundoff, upper_bound) ? 1 : 0;
}


extern "C" int
_rt_brep_local_root_test(const fastf_t *first_minimum,
    const fastf_t *first_maximum, const fastf_t *second_minimum,
    const fastf_t *second_maximum, int u_order, int v_order,
    const fastf_t root[2], fastf_t maximum_radius,
    struct rt_brep_local_root_test_result *result)
{
    if (!first_minimum || !first_maximum || !second_minimum ||
	    !second_maximum || !root || !result || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const fastf_t *minimum[2] = {first_minimum, second_minimum};
    const fastf_t *maximum[2] = {first_maximum, second_maximum};
    const size_t count = (size_t)u_order * v_order;
    for (int equation = 0; equation < 2; ++equation) {
	for (size_t i = 0; i < count; ++i) {
	    if (!std::isfinite(minimum[equation][i]) ||
		    !std::isfinite(maximum[equation][i]) ||
		    minimum[equation][i] > maximum[equation][i])
		return 0;
	    values[equation][i] = {
		minimum[equation][i], maximum[equation][i]
	    };
	}
    }
    return brep_expansion_local_root(values, u_order, v_order, root,
	maximum_radius, *result) ? 1 : 0;
}


extern "C" int
_rt_brep_surface_local_root_test(const struct soltab *stp,
    const fastf_t ray_origin[3], const fastf_t ray_direction[3],
    int face_index, int span_index, const fastf_t uv[2],
    fastf_t maximum_radius, struct rt_brep_local_root_test_result *result)
{
    if (!stp || !stp->st_specific || !ray_origin || !ray_direction ||
	    !uv || !result || span_index < 0 || !(maximum_radius > 0.0) ||
	    !std::isfinite(maximum_radius))
	return 0;
    *result = {};
    const struct brep_specific *bs =
	(const struct brep_specific *)stp->st_specific;
    if (!bs->brep || (size_t)span_index >= bs->surface_spans.size())
	return 0;
    const brep_surface_span &span = bs->surface_spans[span_index];
    if (span.face_index != face_index)
	return 0;
    ON_3dPoint origin(ray_origin[0], ray_origin[1], ray_origin[2]);
    ON_3dVector ray_vector(ray_direction[0], ray_direction[1],
	ray_direction[2]);
    const ON_Ray ray(origin, ray_vector);
    return brep_surface_local_root_certificate(span, ray, uv,
	maximum_radius, *result) ? 1 : 0;
}


extern "C" int
_rt_brep_expansion_interval_product_test(const fastf_t first[2],
    const fastf_t second[2], fastf_t result[2],
    size_t *expansion_high_water)
{
    if (!first || !second || !result || !expansion_high_water ||
	    !std::isfinite(first[0]) || !std::isfinite(first[1]) ||
	    !std::isfinite(second[0]) || !std::isfinite(second[1]) ||
	    first[0] > first[1] || second[0] > second[1])
	return 0;
    size_t high_water = 0;
    brep_interval product;
    if (!brep_expansion_interval_multiply_tight(
	    {first[0], first[1]}, {second[0], second[1]}, product,
	    high_water))
	return 0;
    result[0] = product.minimum;
    result[1] = product.maximum;
    *expansion_high_water = high_water;
    return 1;
}


static bool
brep_scalar_surface_restrict_bounded(const double *input, int u_order,
    int v_order, double input_error, double u_minimum, double u_maximum,
    double v_minimum, double v_maximum, double *output,
    double &output_error)
{
    if (!input || !output || input_error < 0.0 ||
	    !std::isfinite(input_error) ||
	    !brep_scalar_surface_restrict(input, u_order, v_order, u_minimum,
		u_maximum, v_minimum, v_maximum, output))
	return false;
    const size_t count = (size_t)u_order * v_order;
    brep_interval source[BREP_DIRECT_BEZIER_MAX_CVS] = {};
    brep_interval restricted[BREP_DIRECT_BEZIER_MAX_CVS] = {};
    for (size_t i = 0; i < count; ++i)
	source[i] = brep_interval_expanded(input[i] - input_error,
	    input[i] + input_error);
    if (!brep_interval_surface_restrict(source, u_order, v_order, u_minimum,
	    u_maximum, v_minimum, v_maximum, restricted))
	return false;
    output_error = 0.0;
    for (size_t i = 0; i < count; ++i) {
	if (!brep_interval_common_error(output[i], restricted[i],
		output_error))
	    return false;
    }
    return true;
}


extern "C" int
_rt_brep_restrict_test(const fastf_t *input, int u_order, int v_order,
    fastf_t input_error, const fastf_t minimum[2],
    const fastf_t maximum[2], fastf_t *output, fastf_t *output_error)
{
    if (!minimum || !maximum || !output_error)
	return 0;
    double error = 0.0;
    if (!brep_scalar_surface_restrict_bounded(input, u_order, v_order,
	    input_error, minimum[0], maximum[0], minimum[1], maximum[1],
	    output, error))
	return 0;
    *output_error = error;
    return 1;
}


extern "C" int
_rt_brep_interval_restrict_test(const fastf_t *input,
    const fastf_t *input_error, int u_order, int v_order,
    const fastf_t minimum[2], const fastf_t maximum[2],
    fastf_t *output_minimum, fastf_t *output_maximum)
{
    if (!input || !input_error || !minimum || !maximum ||
	    !output_minimum || !output_maximum || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    const size_t count = (size_t)u_order * v_order;
    brep_interval source[BREP_DIRECT_BEZIER_MAX_CVS] = {};
    brep_interval restricted[BREP_DIRECT_BEZIER_MAX_CVS] = {};
    for (size_t i = 0; i < count; ++i) {
	if (!std::isfinite(input[i]) || !std::isfinite(input_error[i]) ||
		input_error[i] < 0.0)
	    return 0;
	source[i] = brep_interval_expanded(input[i] - input_error[i],
	    input[i] + input_error[i]);
    }
    if (!brep_interval_surface_restrict(source, u_order, v_order,
	    minimum[0], maximum[0], minimum[1], maximum[1], restricted))
	return 0;
    for (size_t i = 0; i < count; ++i) {
	if (!std::isfinite(restricted[i].minimum) ||
		!std::isfinite(restricted[i].maximum) ||
		restricted[i].minimum > restricted[i].maximum)
	    return 0;
	output_minimum[i] = restricted[i].minimum;
	output_maximum[i] = restricted[i].maximum;
    }
    return 1;
}


extern "C" int
_rt_brep_expansion_restrict_test(const fastf_t *input,
    const fastf_t *input_error, int u_order, int v_order,
    const fastf_t minimum[2], const fastf_t maximum[2],
    fastf_t *output_minimum, fastf_t *output_maximum,
    size_t *expansion_high_water)
{
    if (!input || !input_error || !minimum || !maximum ||
	    !output_minimum || !output_maximum || !expansion_high_water ||
	    u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    const size_t count = (size_t)u_order * v_order;
    brep_interval source[BREP_DIRECT_BEZIER_MAX_CVS] = {};
    brep_interval restricted[BREP_DIRECT_BEZIER_MAX_CVS] = {};
    for (size_t i = 0; i < count; ++i) {
	if (!std::isfinite(input[i]) || !std::isfinite(input_error[i]) ||
		input_error[i] < 0.0)
	    return 0;
	source[i] = brep_interval_expanded(input[i] - input_error[i],
	    input[i] + input_error[i]);
    }
    size_t high_water = 0;
    if (!brep_expansion_surface_restrict(source, u_order, v_order,
	    minimum[0], maximum[0], minimum[1], maximum[1], false,
	    restricted, high_water))
	return 0;
    for (size_t i = 0; i < count; ++i) {
	if (!std::isfinite(restricted[i].minimum) ||
		!std::isfinite(restricted[i].maximum) ||
		restricted[i].minimum > restricted[i].maximum)
	    return 0;
	output_minimum[i] = restricted[i].minimum;
	output_maximum[i] = restricted[i].maximum;
    }
    *expansion_high_water = high_water;
    return 1;
}


extern "C" int
_rt_brep_coefficient_test(const fastf_t cv[4], const fastf_t origin[3],
    const fastf_t direction[3], const fastf_t planes[2][3],
    struct rt_brep_coefficient_test_result *result)
{
    if (!cv || !origin || !direction || !planes || !result)
	return 0;
    brep_interval direction_squared = {0.0, 0.0};
    for (int component = 0; component < 3; ++component) {
	direction_squared = brep_interval_add(direction_squared,
	    brep_interval_multiply({direction[component],
		direction[component]}, {direction[component],
		direction[component]}));
    }
    brep_interval function[2];
    brep_interval ray_coefficient;
    if (!brep_single_coefficient_intervals(cv, origin, direction, planes,
	    direction_squared, function, ray_coefficient))
	return 0;
    brep_interval expansion_function[2];
    size_t expansion_high_water = 0;
    result->expansion_available =
	brep_single_coefficient_expansion_intervals(cv, origin, planes,
	    expansion_function, expansion_high_water) ? 1 : 0;
    for (int equation = 0; result->expansion_available && equation < 2;
	    ++equation) {
	expansion_function[equation].minimum = std::max(
	    expansion_function[equation].minimum, function[equation].minimum);
	expansion_function[equation].maximum = std::min(
	    expansion_function[equation].maximum, function[equation].maximum);
	if (expansion_function[equation].minimum >
		expansion_function[equation].maximum)
	    result->expansion_available = 0;
    }
    result->expansion_high_water = expansion_high_water;
    for (int equation = 0; equation < 2; ++equation) {
	result->function_minimum[equation] = function[equation].minimum;
	result->function_maximum[equation] = function[equation].maximum;
	if (result->expansion_available) {
	    result->expansion_function_minimum[equation] =
		expansion_function[equation].minimum;
	    result->expansion_function_maximum[equation] =
		expansion_function[equation].maximum;
	}
    }
    result->ray_minimum = ray_coefficient.minimum;
    result->ray_maximum = ray_coefficient.maximum;
    return 1;
}


static bool
brep_scalar_bezier_interval_evaluate(const double *input, int order,
	double coefficient_error, double parameter, brep_interval &value)
{
    if (!input || order < 1 || order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    coefficient_error < 0.0 || !std::isfinite(coefficient_error) ||
	    parameter < 0.0 || parameter > 1.0)
	return false;
    brep_interval work[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int i = 0; i < order; ++i)
	work[i] = brep_interval_expanded(input[i] - coefficient_error,
	    input[i] + coefficient_error);
    for (int level = 1; level < order; ++level) {
	for (int i = 0; i < order - level; ++i) {
	    const brep_interval first = brep_interval_scale(1.0 - parameter,
		work[i]);
	    const brep_interval second = brep_interval_scale(parameter,
		work[i + 1]);
	    work[i] = brep_interval_add(first, second);
	}
    }
    value = work[0];
    return std::isfinite(value.minimum) && std::isfinite(value.maximum);
}


static bool
brep_scalar_surface_interval_evaluate(const double *input, int u_order,
	int v_order, double coefficient_error, const double parameter[2],
	brep_interval &value)
{
    if (!input || u_order < 1 || v_order < 1 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    double source[BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_interval v_control[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int j = 0; j < v_order; ++j) {
	for (int i = 0; i < u_order; ++i)
	    source[i] = input[(size_t)i * v_order + j];
	if (!brep_scalar_bezier_interval_evaluate(source, u_order,
		coefficient_error, parameter[0], v_control[j]))
	    return false;
    }
    for (int level = 1; level < v_order; ++level) {
	for (int j = 0; j < v_order - level; ++j) {
	    const brep_interval first = brep_interval_scale(
		1.0 - parameter[1], v_control[j]);
	    const brep_interval second = brep_interval_scale(parameter[1],
		v_control[j + 1]);
	    v_control[j] = brep_interval_add(first, second);
	}
    }
    value = v_control[0];
    return std::isfinite(value.minimum) && std::isfinite(value.maximum);
}


static bool
brep_surface_derivative_interval(const double *values, int u_order,
	int v_order, int direction, double coefficient_error,
	brep_interval &result)
{
    if (!values || (direction != 0 && direction != 1) ||
	    u_order < 2 || v_order < 2)
	return false;
    double minimum = DBL_MAX;
    double maximum = -DBL_MAX;
    if (direction == 0) {
	for (int i = 0; i < u_order - 1; ++i) {
	    for (int j = 0; j < v_order; ++j) {
		const brep_interval next = brep_interval_expanded(
		    values[(size_t)(i + 1) * v_order + j] -
			coefficient_error,
		    values[(size_t)(i + 1) * v_order + j] +
			coefficient_error);
		const brep_interval previous = brep_interval_expanded(
		    values[(size_t)i * v_order + j] - coefficient_error,
		    values[(size_t)i * v_order + j] + coefficient_error);
		const brep_interval value = brep_interval_scale(u_order - 1,
		    brep_interval_add(next,
			brep_interval_scale(-1.0, previous)));
		minimum = std::min(minimum, value.minimum);
		maximum = std::max(maximum, value.maximum);
	    }
	}
    } else {
	for (int i = 0; i < u_order; ++i) {
	    for (int j = 0; j < v_order - 1; ++j) {
		const brep_interval next = brep_interval_expanded(
		    values[(size_t)i * v_order + j + 1] -
			coefficient_error,
		    values[(size_t)i * v_order + j + 1] +
			coefficient_error);
		const brep_interval previous = brep_interval_expanded(
		    values[(size_t)i * v_order + j] - coefficient_error,
		    values[(size_t)i * v_order + j] + coefficient_error);
		const brep_interval value = brep_interval_scale(v_order - 1,
		    brep_interval_add(next,
			brep_interval_scale(-1.0, previous)));
		minimum = std::min(minimum, value.minimum);
		maximum = std::max(maximum, value.maximum);
	    }
	}
    }
    result = brep_interval_expanded(minimum, maximum);
    return std::isfinite(result.minimum) && std::isfinite(result.maximum);
}


static bool
brep_surface_function_jacobian_intervals(
	const double values[2][BREP_DIRECT_BEZIER_MAX_CVS],
	int u_order, int v_order, const double coefficient_error[2],
	const double root[2], brep_interval function[2],
	brep_interval jacobian[2][2])
{
    if (root[0] < 0.0 || root[0] > 1.0 ||
	    root[1] < 0.0 || root[1] > 1.0)
	return false;

    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_scalar_surface_interval_evaluate(values[equation], u_order,
		v_order, coefficient_error[equation], root,
		function[equation]) ||
		!brep_surface_derivative_interval(values[equation], u_order,
		v_order, 0, coefficient_error[equation],
		jacobian[equation][0]) ||
		!brep_surface_derivative_interval(values[equation], u_order,
		v_order, 1, coefficient_error[equation],
		jacobian[equation][1]))
	    return false;
    }
    return true;
}


extern "C" int
_rt_brep_interval_test(const fastf_t *first_coefficients,
    const fastf_t *second_coefficients, int u_order, int v_order,
    const fastf_t coefficient_error[2], const fastf_t root[2],
    struct rt_brep_interval_test_result *result)
{
    if (!first_coefficients || !second_coefficients || !coefficient_error ||
	    !root || !result || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    double values[2][BREP_DIRECT_BEZIER_MAX_CVS] = {};
    const size_t count = (size_t)u_order * v_order;
    for (size_t i = 0; i < count; ++i) {
	values[0][i] = first_coefficients[i];
	values[1][i] = second_coefficients[i];
    }
    brep_interval function[2];
    brep_interval jacobian[2][2];
    if (!brep_surface_function_jacobian_intervals(values, u_order, v_order,
	    coefficient_error, root, function, jacobian))
	return 0;
    for (int equation = 0; equation < 2; ++equation) {
	result->function_minimum[equation] = function[equation].minimum;
	result->function_maximum[equation] = function[equation].maximum;
	for (int direction = 0; direction < 2; ++direction) {
	    result->jacobian_minimum[equation][direction] =
		jacobian[equation][direction].minimum;
	    result->jacobian_maximum[equation][direction] =
		jacobian[equation][direction].maximum;
	}
    }
    return 1;
}


extern "C" int
_rt_brep_interval_product_test(const fastf_t first[2],
    const fastf_t second[2], fastf_t result[2])
{
    if (!first || !second || !result || !std::isfinite(first[0]) ||
	    !std::isfinite(first[1]) || !std::isfinite(second[0]) ||
	    !std::isfinite(second[1]) || first[0] > first[1] ||
	    second[0] > second[1])
	return 0;
    const brep_interval product = brep_interval_multiply(
	{first[0], first[1]}, {second[0], second[1]});
    if (!std::isfinite(product.minimum) || !std::isfinite(product.maximum))
	return 0;
    result[0] = product.minimum;
    result[1] = product.maximum;
    return 1;
}


extern "C" int
_rt_brep_interval_divide_test(const fastf_t numerator[2],
    const fastf_t denominator[2], fastf_t result[2])
{
    if (!numerator || !denominator || !result ||
	    !std::isfinite(numerator[0]) ||
	    !std::isfinite(numerator[1]) ||
	    !std::isfinite(denominator[0]) ||
	    !std::isfinite(denominator[1]) ||
	    numerator[0] > numerator[1] ||
	    denominator[0] > denominator[1])
	return 0;
    brep_interval quotient;
    if (!brep_interval_divide({numerator[0], numerator[1]},
	    {denominator[0], denominator[1]}, quotient))
	return 0;
    result[0] = quotient.minimum;
    result[1] = quotient.maximum;
    return 1;
}


static bool
brep_surface_krawczyk_certified(
	const double values[2][BREP_DIRECT_BEZIER_MAX_CVS],
	int u_order, int v_order, const double coefficient_error[2],
	const double root[2])
{
    brep_interval function[2];
    brep_interval jacobian[2][2];
    if (!brep_surface_function_jacobian_intervals(values, u_order, v_order,
	    coefficient_error, root, function, jacobian))
	return false;

    double midpoint[2][2];
    for (int row = 0; row < 2; ++row) {
	for (int column = 0; column < 2; ++column)
	    midpoint[row][column] = 0.5 *
		(jacobian[row][column].minimum +
		 jacobian[row][column].maximum);
    }
    const double determinant = midpoint[0][0] * midpoint[1][1] -
	midpoint[0][1] * midpoint[1][0];
    const double first_scale = hypot(midpoint[0][0], midpoint[1][0]);
    const double second_scale = hypot(midpoint[0][1], midpoint[1][1]);
    if (!std::isfinite(determinant) ||
	    !(first_scale > DBL_MIN) || !(second_scale > DBL_MIN) ||
	    fabs(determinant) <= BREP_INTERSECTION_ROOT_EPSILON *
	    first_scale * second_scale)
	return false;
    const double inverse[2][2] = {
	{midpoint[1][1] / determinant, -midpoint[0][1] / determinant},
	{-midpoint[1][0] / determinant, midpoint[0][0] / determinant}
    };

    brep_interval center[2];
    for (int row = 0; row < 2; ++row) {
	brep_interval correction = {0.0, 0.0};
	for (int equation = 0; equation < 2; ++equation)
	    correction = brep_interval_add(correction,
		brep_interval_scale(inverse[row][equation],
		    function[equation]));
	center[row] = brep_interval_add({root[row], root[row]},
	    brep_interval_scale(-1.0, correction));
    }

    brep_interval remainder[2][2];
    for (int row = 0; row < 2; ++row) {
	for (int column = 0; column < 2; ++column) {
	    brep_interval product = {0.0, 0.0};
	    for (int equation = 0; equation < 2; ++equation)
		product = brep_interval_add(product,
		    brep_interval_scale(inverse[row][equation],
			jacobian[equation][column]));
	    remainder[row][column] = brep_interval_scale(-1.0, product);
	    if (row == column)
		remainder[row][column] = brep_interval_add(
		    {1.0, 1.0}, remainder[row][column]);
	}
    }

    const brep_interval offset[2] = {
	brep_interval_expanded(-root[0], 1.0 - root[0]),
	brep_interval_expanded(-root[1], 1.0 - root[1])
    };
    const double inclusion_margin = 512.0 * DBL_EPSILON;
    for (int row = 0; row < 2; ++row) {
	brep_interval image = center[row];
	for (int column = 0; column < 2; ++column)
	    image = brep_interval_add(image, brep_interval_multiply(
		remainder[row][column], offset[column]));
	if (!(image.minimum > inclusion_margin) ||
		!(image.maximum < 1.0 - inclusion_margin))
	    return false;
    }
    return true;
}


static bool
brep_scalar_bezier_reparameterize(const double *input, int order,
    double minimum, double maximum, double *output)
{
    if (!input || !output || order < 2 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    !std::isfinite(minimum) || !std::isfinite(maximum) ||
	    !(minimum < maximum))
	return false;
    if (fabs(minimum) <= DBL_MIN && fabs(maximum - 1.0) <= DBL_MIN) {
	for (int i = 0; i < order; ++i)
	    output[i] = input[i];
	return true;
    }

    double first[BREP_DIRECT_BEZIER_MAX_ORDER];
    double second[BREP_DIRECT_BEZIER_MAX_ORDER];
    double unused[BREP_DIRECT_BEZIER_MAX_ORDER];
    const double from_minimum = 1.0 - minimum;
    if (fabs(from_minimum) > DBL_MIN) {
	brep_scalar_bezier_split(input, order, minimum, unused, second);
	const double local_maximum = (maximum - minimum) / from_minimum;
	brep_scalar_bezier_split(second, order, local_maximum, first, unused);
	for (int i = 0; i < order; ++i)
	    output[i] = first[i];
	return true;
    }

    if (fabs(maximum) <= DBL_MIN)
	return false;
    brep_scalar_bezier_split(input, order, maximum, first, unused);
    const double local_minimum = minimum / maximum;
    brep_scalar_bezier_split(first, order, local_minimum, unused, second);
    for (int i = 0; i < order; ++i)
	output[i] = second[i];
    return true;
}


static bool
brep_scalar_surface_reparameterize(const double *input, int u_order,
    int v_order, double u_minimum, double u_maximum, double v_minimum,
    double v_maximum, double *output)
{
    double u_reparameterized[BREP_DIRECT_BEZIER_MAX_CVS];
    double source[BREP_DIRECT_BEZIER_MAX_ORDER];
    double result[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int j = 0; j < v_order; ++j) {
	for (int i = 0; i < u_order; ++i)
	    source[i] = input[(size_t)i * v_order + j];
	if (!brep_scalar_bezier_reparameterize(source, u_order, u_minimum,
		u_maximum, result))
	    return false;
	for (int i = 0; i < u_order; ++i)
	    u_reparameterized[(size_t)i * v_order + j] = result[i];
    }
    for (int i = 0; i < u_order; ++i) {
	for (int j = 0; j < v_order; ++j)
	    source[j] = u_reparameterized[(size_t)i * v_order + j];
	if (!brep_scalar_bezier_reparameterize(source, v_order, v_minimum,
		v_maximum, result))
	    return false;
	for (int j = 0; j < v_order; ++j)
	    output[(size_t)i * v_order + j] = result[j];
    }
    return true;
}


static bool
brep_interval_bezier_reparameterize(const brep_interval *input, int order,
    double minimum, double maximum, brep_interval *output)
{
    if (!input || !output || order < 2 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    !std::isfinite(minimum) || !std::isfinite(maximum) ||
	    !(minimum < maximum))
	return false;
    if (fabs(minimum) <= DBL_MIN && fabs(maximum - 1.0) <= DBL_MIN) {
	for (int i = 0; i < order; ++i)
	    output[i] = input[i];
	return true;
    }

    brep_interval first[BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_interval second[BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_interval unused[BREP_DIRECT_BEZIER_MAX_ORDER];
    const double from_minimum = 1.0 - minimum;
    if (fabs(from_minimum) > DBL_MIN) {
	brep_interval_bezier_split(input, order, {minimum, minimum}, unused,
	    second);
	const brep_interval numerator = brep_interval_add(
	    {maximum, maximum}, brep_interval_scale(-1.0,
		{minimum, minimum}));
	const brep_interval denominator = brep_interval_add({1.0, 1.0},
	    brep_interval_scale(-1.0, {minimum, minimum}));
	brep_interval local_maximum;
	if (!brep_interval_divide_nonzero(numerator, denominator,
		local_maximum))
	    return false;
	brep_interval_bezier_split(second, order, local_maximum, first,
	    unused);
	for (int i = 0; i < order; ++i)
	    output[i] = first[i];
	return true;
    }

    if (fabs(maximum) <= DBL_MIN)
	return false;
    brep_interval_bezier_split(input, order, {maximum, maximum}, first,
	unused);
    brep_interval local_minimum;
    if (!brep_interval_divide_nonzero({minimum, minimum},
	    {maximum, maximum}, local_minimum))
	return false;
    brep_interval_bezier_split(first, order, local_minimum, unused, second);
    for (int i = 0; i < order; ++i)
	output[i] = second[i];
    return true;
}


static bool
brep_interval_bezier_reparameterization_matrix(int order, double minimum,
    double maximum, brep_interval *matrix)
{
    if (!matrix || order < 2 || order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    for (int basis = 0; basis < order; ++basis) {
	brep_interval input[BREP_DIRECT_BEZIER_MAX_ORDER];
	brep_interval output[BREP_DIRECT_BEZIER_MAX_ORDER];
	for (int i = 0; i < order; ++i)
	    input[i] = {i == basis ? 1.0 : 0.0,
		i == basis ? 1.0 : 0.0};
	if (!brep_interval_bezier_reparameterize(input, order, minimum,
		maximum, output))
	    return false;
	for (int row = 0; row < order; ++row)
	    matrix[(size_t)row * order + basis] = output[row];
    }
    return true;
}


static bool
brep_interval_surface_apply_reparameterization(const brep_interval *input,
    int u_order, int v_order, const brep_interval *u_matrix,
    const brep_interval *v_matrix, brep_interval *output)
{
    if (!input || !u_matrix || !v_matrix || !output ||
	    u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    for (int i = 0; i < u_order; ++i) {
	for (int j = 0; j < v_order; ++j) {
	    brep_interval value = {0.0, 0.0};
	    for (int source_i = 0; source_i < u_order; ++source_i) {
		const brep_interval u_coefficient =
		    u_matrix[(size_t)i * u_order + source_i];
		for (int source_j = 0; source_j < v_order; ++source_j) {
		    const brep_interval coefficient = brep_interval_multiply(
			u_coefficient,
			v_matrix[(size_t)j * v_order + source_j]);
		    value = brep_interval_add(value,
			brep_interval_multiply(coefficient,
			    input[(size_t)source_i * v_order + source_j]));
		}
	    }
	    output[(size_t)i * v_order + j] = value;
	}
    }
    return true;
}


static bool
brep_scalar_surface_reparameterize_bounded_with_matrices(const double *input,
    int u_order, int v_order, double input_error, double u_minimum,
    double u_maximum, double v_minimum, double v_maximum,
    const brep_interval *u_matrix, const brep_interval *v_matrix,
    double *output, double &output_error)
{
    if (!input || !output || input_error < 0.0 ||
	    !std::isfinite(input_error) ||
	    !brep_scalar_surface_reparameterize(input, u_order, v_order,
		u_minimum, u_maximum, v_minimum, v_maximum, output))
	return false;
    const size_t count = (size_t)u_order * v_order;
    brep_interval source[BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval reparameterized[BREP_DIRECT_BEZIER_MAX_CVS];
    for (size_t i = 0; i < count; ++i)
	source[i] = brep_interval_expanded(input[i] - input_error,
	    input[i] + input_error);
    if (!brep_interval_surface_apply_reparameterization(source, u_order,
	    v_order, u_matrix, v_matrix, reparameterized))
	return false;
    output_error = 0.0;
    for (size_t i = 0; i < count; ++i) {
	if (!brep_interval_common_error(output[i], reparameterized[i],
		output_error))
	    return false;
    }
    return true;
}


static bool
brep_scalar_surface_reparameterize_bounded(const double *input, int u_order,
    int v_order, double input_error, double u_minimum, double u_maximum,
    double v_minimum, double v_maximum, double *output,
    double &output_error)
{
    brep_interval u_matrix[BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval v_matrix[BREP_DIRECT_BEZIER_MAX_CVS];
    if (!brep_interval_bezier_reparameterization_matrix(u_order, u_minimum,
	    u_maximum, u_matrix) ||
	    !brep_interval_bezier_reparameterization_matrix(v_order, v_minimum,
		v_maximum, v_matrix))
	return false;
    return brep_scalar_surface_reparameterize_bounded_with_matrices(input,
	u_order, v_order, input_error, u_minimum, u_maximum, v_minimum,
	v_maximum, u_matrix, v_matrix, output, output_error);
}


extern "C" int
_rt_brep_reparameterize_test(const fastf_t *input, int u_order, int v_order,
    fastf_t input_error, const fastf_t minimum[2],
    const fastf_t maximum[2], fastf_t *output, fastf_t *output_error)
{
    if (!minimum || !maximum || !output_error)
	return 0;
    double error = 0.0;
    if (!brep_scalar_surface_reparameterize_bounded(input, u_order, v_order,
	    input_error, minimum[0], maximum[0], minimum[1], maximum[1],
	    output, error))
	return 0;
    *output_error = error;
    return 1;
}


static bool
brep_surface_coefficients_reparameterize(
    const brep_surface_coefficients &source, const double minimum[2],
    const double maximum[2], brep_surface_coefficients &result)
{
    result.order[0] = source.order[0];
    result.order[1] = source.order[1];
    brep_interval u_matrix[BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval v_matrix[BREP_DIRECT_BEZIER_MAX_CVS];
    if (!brep_interval_bezier_reparameterization_matrix(source.order[0],
	    minimum[0], maximum[0], u_matrix) ||
	    !brep_interval_bezier_reparameterization_matrix(source.order[1],
		minimum[1], maximum[1], v_matrix))
	return false;

    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_scalar_surface_reparameterize_bounded_with_matrices(
		source.value[equation], source.order[0], source.order[1],
		source.error[equation], minimum[0], maximum[0], minimum[1],
		maximum[1], u_matrix, v_matrix, result.value[equation],
		result.error[equation]))
	    return false;
	if (!brep_interval_surface_apply_reparameterization(
		source.value_interval[equation], source.order[0],
		source.order[1], u_matrix, v_matrix,
		result.value_interval[equation]))
	    return false;
    }
    if (!brep_scalar_surface_reparameterize_bounded_with_matrices(
	    source.ray_numerator, source.order[0], source.order[1],
	    source.ray_numerator_error, minimum[0], maximum[0], minimum[1],
	    maximum[1], u_matrix, v_matrix, result.ray_numerator,
	    result.ray_numerator_error) ||
	    !brep_scalar_surface_reparameterize_bounded_with_matrices(
	    source.weight, source.order[0], source.order[1], source.weight_error,
	    minimum[0], maximum[0], minimum[1], maximum[1], u_matrix, v_matrix,
	    result.weight, result.weight_error))
	return false;

    const size_t count = (size_t)source.order[0] * source.order[1];
    for (size_t i = 0; i < count; ++i) {
	const brep_interval weight = brep_interval_expanded(
	    result.weight[i] - result.weight_error,
	    result.weight[i] + result.weight_error);
	if (!(weight.minimum > 0.0) || !std::isfinite(weight.maximum))
	    return false;
    }
    return true;
}


struct brep_subdivision_box {
    double minimum[2];
    double maximum[2];
    int depth;
    int exact_depth = 0;
};


enum brep_rotated_hull_status {
    BREP_ROTATED_HULL_INCONCLUSIVE = 0,
    BREP_ROTATED_HULL_RETAINED,
    BREP_ROTATED_HULL_EXCLUDED
};


struct brep_conditioned_surface_frame {
    double transform[2][2];
    int regular_direction;
};


static bool
brep_conditioned_surface_frame_init(
    const double values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], const double coefficient_error[2],
    brep_conditioned_surface_frame &frame)
{
    brep_interval derivative[2][2];
    double midpoint[2][2];
    for (int equation = 0; equation < 2; ++equation) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (!brep_surface_derivative_interval(values[equation], order[0],
		    order[1], direction, coefficient_error[equation],
		    derivative[equation][direction]))
		return false;
	    midpoint[equation][direction] =
		0.5 * derivative[equation][direction].minimum +
		0.5 * derivative[equation][direction].maximum;
	}
    }
    const double scale[2] = {
	hypot(midpoint[0][0], midpoint[1][0]),
	hypot(midpoint[0][1], midpoint[1][1])
    };
    frame.regular_direction = scale[0] >= scale[1] ? 0 : 1;
    const double regular_scale = scale[frame.regular_direction];
    if (!(regular_scale > DBL_MIN) || !std::isfinite(regular_scale))
	return false;
    frame.transform[0][0] =
	midpoint[0][frame.regular_direction] / regular_scale;
    frame.transform[0][1] =
	midpoint[1][frame.regular_direction] / regular_scale;
    frame.transform[1][0] = -frame.transform[0][1];
    frame.transform[1][1] = frame.transform[0][0];
    return std::isfinite(frame.transform[0][0]) &&
	std::isfinite(frame.transform[0][1]);
}


static brep_rotated_hull_status
brep_rotated_surface_hull_status(
    const double values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], const double coefficient_error[2])
{
    brep_conditioned_surface_frame frame;
    if (!brep_conditioned_surface_frame_init(values, order,
	    coefficient_error, frame))
	return BREP_ROTATED_HULL_INCONCLUSIVE;
    brep_interval hull[2];
    const size_t count = (size_t)order[0] * order[1];
    if (!brep_linear_coefficient_hulls(values, count, coefficient_error,
	    frame.transform, hull))
	return BREP_ROTATED_HULL_INCONCLUSIVE;
    for (int equation = 0; equation < 2; ++equation) {
	if (hull[equation].minimum > 0.0 ||
		hull[equation].maximum < 0.0)
	    return BREP_ROTATED_HULL_EXCLUDED;
    }
    return BREP_ROTATED_HULL_RETAINED;
}


static bool
brep_interval_conditioned_surface_frame_init(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], brep_conditioned_surface_frame &frame)
{
    brep_interval derivative[2][2];
    double midpoint[2][2];
    for (int equation = 0; equation < 2; ++equation) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (!brep_interval_surface_derivative_hull(values[equation],
		    order[0], order[1], direction,
		    derivative[equation][direction]))
		return false;
	    midpoint[equation][direction] =
		0.5 * derivative[equation][direction].minimum +
		0.5 * derivative[equation][direction].maximum;
	}
    }
    const double scale[2] = {
	hypot(midpoint[0][0], midpoint[1][0]),
	hypot(midpoint[0][1], midpoint[1][1])
    };
    frame.regular_direction = scale[0] >= scale[1] ? 0 : 1;
    const double regular_scale = scale[frame.regular_direction];
    if (!(regular_scale > DBL_MIN) || !std::isfinite(regular_scale))
	return false;
    frame.transform[0][0] =
	midpoint[0][frame.regular_direction] / regular_scale;
    frame.transform[0][1] =
	midpoint[1][frame.regular_direction] / regular_scale;
    frame.transform[1][0] = -frame.transform[0][1];
    frame.transform[1][1] = frame.transform[0][0];
    return std::isfinite(frame.transform[0][0]) &&
	std::isfinite(frame.transform[0][1]);
}


static brep_rotated_hull_status
brep_expansion_rotated_surface_hull_status(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], size_t &high_water)
{
    brep_conditioned_surface_frame frame;
    if (!brep_interval_conditioned_surface_frame_init(values, order, frame))
	return BREP_ROTATED_HULL_INCONCLUSIVE;
    brep_interval rotated[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const size_t count = (size_t)order[0] * order[1];
    if (!brep_expansion_linear_coefficients(values, count, frame.transform,
	    rotated, high_water))
	return BREP_ROTATED_HULL_INCONCLUSIVE;
    for (int equation = 0; equation < 2; ++equation) {
	if (brep_interval_coefficient_hull_excluded(rotated[equation], count))
	    return BREP_ROTATED_HULL_EXCLUDED;
    }
    return BREP_ROTATED_HULL_RETAINED;
}


static bool
brep_fold_scalar_surface_evaluate(const double *values, int u_order,
    int v_order, const double parameter[2], double &value)
{
    if (!values || !parameter || u_order < 1 || v_order < 1 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    parameter[0] < 0.0 || parameter[0] > 1.0 ||
	    parameter[1] < 0.0 || parameter[1] > 1.0)
	return false;
    double v_control[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int j = 0; j < v_order; ++j) {
	double work[BREP_DIRECT_BEZIER_MAX_ORDER];
	for (int i = 0; i < u_order; ++i)
	    work[i] = values[(size_t)i * v_order + j];
	for (int level = 1; level < u_order; ++level) {
	    for (int i = 0; i < u_order - level; ++i)
		work[i] = (1.0 - parameter[0]) * work[i] +
		    parameter[0] * work[i + 1];
	}
	v_control[j] = work[0];
    }
    for (int level = 1; level < v_order; ++level) {
	for (int j = 0; j < v_order - level; ++j)
	    v_control[j] = (1.0 - parameter[1]) * v_control[j] +
		parameter[1] * v_control[j + 1];
    }
    value = v_control[0];
    return std::isfinite(value);
}


static bool
brep_fold_scalar_surface_derivative_evaluate(const double *values,
    int u_order, int v_order, int direction, const double parameter[2],
    double &value)
{
    if (!values || (direction != 0 && direction != 1) ||
	    u_order < 2 || v_order < 2)
	return false;
    double derivative[BREP_DIRECT_BEZIER_MAX_CVS];
    const int derivative_u_order = u_order - (direction == 0 ? 1 : 0);
    const int derivative_v_order = v_order - (direction == 1 ? 1 : 0);
    const int degree = direction == 0 ? u_order - 1 : v_order - 1;
    for (int i = 0; i < derivative_u_order; ++i) {
	for (int j = 0; j < derivative_v_order; ++j) {
	    const size_t previous = (size_t)i * v_order + j;
	    const size_t next = direction == 0 ?
		(size_t)(i + 1) * v_order + j :
		(size_t)i * v_order + j + 1;
	    derivative[(size_t)i * derivative_v_order + j] =
		degree * (values[next] - values[previous]);
	}
    }
    return brep_fold_scalar_surface_evaluate(derivative,
	derivative_u_order, derivative_v_order, parameter, value);
}


/*
 * Construct only a conditioning hint from the nominal Jacobian at a
 * candidate.  All exclusions and certificates continue to use outward
 * coefficient intervals, so an inaccurate point or frame can make a proof
 * inconclusive but cannot discard a root.
 */
static bool
brep_conditioned_surface_frame_point_init(
    const double values[2][BREP_DIRECT_BEZIER_MAX_CVS], const int order[2],
    const double parameter[2], const double parameter_scale[2],
    int forced_regular_direction, brep_conditioned_surface_frame &frame)
{
    if (!values || !order || !parameter || !parameter_scale ||
	    (forced_regular_direction < -1 || forced_regular_direction > 1) ||
	    !(parameter_scale[0] > 0.0) || !(parameter_scale[1] > 0.0) ||
	    !std::isfinite(parameter_scale[0]) ||
	    !std::isfinite(parameter_scale[1]))
	return false;
    double jacobian[2][2];
    for (int equation = 0; equation < 2; ++equation) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (!brep_fold_scalar_surface_derivative_evaluate(
		    values[equation], order[0], order[1], direction,
		    parameter, jacobian[equation][direction]))
		return false;
	}
    }
    const double scale[2] = {
	parameter_scale[0] * hypot(jacobian[0][0], jacobian[1][0]),
	parameter_scale[1] * hypot(jacobian[0][1], jacobian[1][1])
    };
    frame.regular_direction = forced_regular_direction >= 0 ?
	forced_regular_direction : (scale[0] >= scale[1] ? 0 : 1);
    const double regular_scale = scale[frame.regular_direction];
    if (!(regular_scale > DBL_MIN) || !std::isfinite(regular_scale))
	return false;
    frame.transform[0][0] =
	parameter_scale[frame.regular_direction] *
	jacobian[0][frame.regular_direction] / regular_scale;
    frame.transform[0][1] =
	parameter_scale[frame.regular_direction] *
	jacobian[1][frame.regular_direction] / regular_scale;
    frame.transform[1][0] = -frame.transform[0][1];
    frame.transform[1][1] = frame.transform[0][0];
    return std::isfinite(frame.transform[0][0]) &&
	std::isfinite(frame.transform[0][1]);
}


static bool
brep_fold_refine_candidate(
    const double values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], double root[2])
{
    if (!values || !root)
	return false;
    for (int iteration = 0; iteration < 12; ++iteration) {
	double function[2];
	double jacobian[2][2];
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_fold_scalar_surface_evaluate(values[equation], order[0],
		    order[1], root, function[equation]))
		return false;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!brep_fold_scalar_surface_derivative_evaluate(
			values[equation], order[0], order[1], direction,
			root, jacobian[equation][direction]))
		    return false;
	    }
	}
	const double determinant = std::fma(jacobian[0][0],
	    jacobian[1][1], -jacobian[0][1] * jacobian[1][0]);
	if (!(fabs(determinant) > DBL_MIN) || !std::isfinite(determinant))
	    return false;
	const double step[2] = {
	    std::fma(function[0], jacobian[1][1],
		-function[1] * jacobian[0][1]) / determinant,
	    std::fma(jacobian[0][0], function[1],
		-jacobian[1][0] * function[0]) / determinant
	};
	const double next[2] = {root[0] - step[0], root[1] - step[1]};
	if (!std::isfinite(next[0]) || !std::isfinite(next[1]) ||
		next[0] < 0.0 || next[0] > 1.0 ||
		next[1] < 0.0 || next[1] > 1.0)
	    return false;
	if (std::memcmp(next, root, sizeof(next)) == 0)
	    return true;
	root[0] = next[0];
	root[1] = next[1];
    }
    return true;
}


static bool
brep_fold_transformed_evaluate(
    const double values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], const brep_conditioned_surface_frame &frame,
    const double parameter[2], double transformed[2])
{
    double source[2];
    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_fold_scalar_surface_evaluate(values[equation], order[0],
		order[1], parameter, source[equation]))
	    return false;
    }
    for (int row = 0; row < 2; ++row) {
	transformed[row] = frame.transform[row][0] * source[0] +
	    frame.transform[row][1] * source[1];
	if (!std::isfinite(transformed[row]))
	    return false;
    }
    return true;
}


static bool
brep_fold_regular_solve(
    const double values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], const brep_conditioned_surface_frame &frame,
    double weak_parameter, double root[2], double &weak_value,
    struct rt_brep_fold_test_result &result)
{
    const int regular = frame.regular_direction;
    const int weak = 1 - regular;
    double lower = 0.0;
    double upper = 1.0;
    double lower_parameter[2] = {0.0, 0.0};
    double upper_parameter[2] = {0.0, 0.0};
    lower_parameter[weak] = upper_parameter[weak] = weak_parameter;
    lower_parameter[regular] = lower;
    upper_parameter[regular] = upper;
    double lower_value[2];
    double upper_value[2];
    result.regular_solves++;
    if (!brep_fold_transformed_evaluate(values, order, frame,
	    lower_parameter, lower_value) ||
	    !brep_fold_transformed_evaluate(values, order, frame,
		upper_parameter, upper_value))
	return false;
    if (fabs(lower_value[0]) <= DBL_MIN) {
	root[0] = lower_parameter[0];
	root[1] = lower_parameter[1];
	weak_value = lower_value[1];
	return true;
    }
    if (fabs(upper_value[0]) <= DBL_MIN) {
	root[0] = upper_parameter[0];
	root[1] = upper_parameter[1];
	weak_value = upper_value[1];
	return true;
    }
    if (std::signbit(lower_value[0]) == std::signbit(upper_value[0]))
	return false;

    double middle_parameter[2] = {0.0, 0.0};
    middle_parameter[weak] = weak_parameter;
    double middle_value[2] = {0.0, 0.0};
    for (int iteration = 0; iteration < 56; ++iteration) {
	const double middle = 0.5 * lower + 0.5 * upper;
	if (!(middle > lower) || !(middle < upper))
	    break;
	middle_parameter[regular] = middle;
	if (!brep_fold_transformed_evaluate(values, order, frame,
		middle_parameter, middle_value))
	    return false;
	if (fabs(middle_value[0]) <= DBL_MIN) {
	    lower = upper = middle;
	    break;
	}
	if (std::signbit(lower_value[0]) ==
		std::signbit(middle_value[0])) {
	    lower = middle;
	    lower_value[0] = middle_value[0];
	} else {
	    upper = middle;
	    upper_value[0] = middle_value[0];
	}
    }
    middle_parameter[regular] = 0.5 * lower + 0.5 * upper;
    if (!brep_fold_transformed_evaluate(values, order, frame,
	    middle_parameter, middle_value))
	return false;
    root[0] = middle_parameter[0];
    root[1] = middle_parameter[1];
    weak_value = middle_value[1];
    return true;
}


static bool
brep_fold_store_candidate(
    const double values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], const brep_conditioned_surface_frame &frame,
    const double root[2], double bracket_width,
    struct rt_brep_fold_test_result &result)
{
    const double duplicate_tolerance = 1024.0 * DBL_EPSILON;
    for (size_t i = 0; i < result.candidate_count; ++i) {
	if (fabs(result.uv[i][0] - root[0]) <= duplicate_tolerance &&
		fabs(result.uv[i][1] - root[1]) <= duplicate_tolerance)
	    return true;
    }
    if (result.candidate_count >= RT_BREP_FOLD_TEST_MAX_CANDIDATES) {
	result.capacity_exhausted = 1;
	return false;
    }
    double transformed[2];
    if (!brep_fold_transformed_evaluate(values, order, frame, root,
	    transformed))
	return false;
    const size_t index = result.candidate_count++;
    result.uv[index][0] = root[0];
    result.uv[index][1] = root[1];
    result.residual[index] = hypot(transformed[0], transformed[1]);
    result.weak_bracket_width[index] = bracket_width;
    return true;
}


extern "C" int
_rt_brep_fold_test(const fastf_t *first_coefficients,
    const fastf_t *second_coefficients, int u_order, int v_order,
    const fastf_t coefficient_error[2],
    struct rt_brep_fold_test_result *result)
{
    if (!first_coefficients || !second_coefficients || !coefficient_error ||
	    !result || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    coefficient_error[0] < 0.0 || coefficient_error[1] < 0.0 ||
	    !std::isfinite(coefficient_error[0]) ||
	    !std::isfinite(coefficient_error[1]))
	return 0;
    *result = {};
    const int order[2] = {u_order, v_order};
    const size_t count = (size_t)u_order * v_order;
    double values[2][BREP_DIRECT_BEZIER_MAX_CVS] = {};
    for (size_t i = 0; i < count; ++i) {
	values[0][i] = first_coefficients[i];
	values[1][i] = second_coefficients[i];
    }
    const double error[2] = {
	coefficient_error[0], coefficient_error[1]
    };
    brep_conditioned_surface_frame frame;
    if (!brep_conditioned_surface_frame_init(values, order, error, frame))
	return 1;
    result->frame_available = 1;
    result->regular_direction = frame.regular_direction;
    bool previous_valid = false;
    double previous_weak_parameter = 0.0;
    double previous_weak_value = 0.0;
    for (int sample = 0; sample <= 16; ++sample) {
	const double weak_parameter = (double)sample / 16.0;
	double root[2];
	double weak_value = 0.0;
	result->samples++;
	if (!brep_fold_regular_solve(values, order, frame, weak_parameter,
		root, weak_value, *result)) {
	    previous_valid = false;
	    continue;
	}
	if (fabs(weak_value) <= DBL_MIN) {
	    result->brackets++;
	    if (!brep_fold_store_candidate(values, order, frame, root, 0.0,
		    *result))
		return 1;
	    previous_valid = false;
	    continue;
	}
	if (previous_valid && std::signbit(previous_weak_value) !=
		std::signbit(weak_value)) {
	    result->brackets++;
	    double lower = previous_weak_parameter;
	    double upper = weak_parameter;
	    double lower_value = previous_weak_value;
	    double bracket_root[2] = {root[0], root[1]};
	    for (int iteration = 0; iteration < 64; ++iteration) {
		const double middle = 0.5 * lower + 0.5 * upper;
		if (!(middle > lower) || !(middle < upper))
		    break;
		double middle_root[2];
		double middle_value = 0.0;
		if (!brep_fold_regular_solve(values, order, frame, middle,
			middle_root, middle_value, *result))
		    return 1;
		bracket_root[0] = middle_root[0];
		bracket_root[1] = middle_root[1];
		if (fabs(middle_value) <= DBL_MIN) {
		    lower = upper = middle;
		    break;
		}
		if (std::signbit(lower_value) == std::signbit(middle_value)) {
		    lower = middle;
		    lower_value = middle_value;
		} else {
		    upper = middle;
		}
	    }
	    const double final_parameter = 0.5 * lower + 0.5 * upper;
	    double final_value = 0.0;
	    if (!brep_fold_regular_solve(values, order, frame,
		    final_parameter, bracket_root, final_value, *result) ||
		    !brep_fold_store_candidate(values, order, frame,
			bracket_root, upper - lower, *result))
		return 1;
	}
	previous_valid = true;
	previous_weak_parameter = weak_parameter;
	previous_weak_value = weak_value;
    }
    return 1;
}


enum brep_clip_status {
    BREP_CLIP_INCONCLUSIVE = 0,
    BREP_CLIP_EMPTY,
    BREP_CLIP_RANGE
};


/* Return a conservative parameter range in which the convex hull of a
 * univariate Bernstein control polygon can satisfy the requested
 * halfspace.  Intersections of every control-point pair are considered;
 * this is no tighter than clipping the actual convex hull edges, but it is
 * deliberately conservative and avoids constructing a dynamic hull. */
static brep_clip_status
brep_bezier_halfspace_range(const double *control, int order,
    bool nonpositive, brep_interval &range)
{
    bool have_range = false;
    range.minimum = DBL_MAX;
    range.maximum = -DBL_MAX;
    for (int i = 0; i < order; ++i) {
	const double y = control[i];
	if (!std::isfinite(y))
	    return BREP_CLIP_INCONCLUSIVE;
	if ((nonpositive && y <= 0.0) || (!nonpositive && y >= 0.0)) {
	    const double x = (double)i / (order - 1);
	    const brep_interval x_interval = brep_interval_expanded(x, x);
	    range.minimum = std::min(range.minimum, x_interval.minimum);
	    range.maximum = std::max(range.maximum, x_interval.maximum);
	    have_range = true;
	}
    }

    for (int i = 0; i < order; ++i) {
	for (int j = i + 1; j < order; ++j) {
	    const double first_y = control[i];
	    const double second_y = control[j];
	    if (!((first_y < 0.0 && second_y > 0.0) ||
		    (first_y > 0.0 && second_y < 0.0)))
		continue;
	    const double first_x = (double)i / (order - 1);
	    const double second_x = (double)j / (order - 1);
	    const brep_interval first_x_interval =
		brep_interval_expanded(first_x, first_x);
	    const brep_interval second_x_interval =
		brep_interval_expanded(second_x, second_x);
	    const brep_interval first_y_interval =
		brep_interval_expanded(first_y, first_y);
	    const brep_interval second_y_interval =
		brep_interval_expanded(second_y, second_y);
	    const brep_interval delta_y = brep_interval_add(
		second_y_interval,
		brep_interval_scale(-1.0, first_y_interval));
	    brep_interval fraction;
	    if (!brep_interval_divide_nonzero(
		    brep_interval_scale(-1.0, first_y_interval), delta_y,
		    fraction))
		return BREP_CLIP_INCONCLUSIVE;
	    const brep_interval delta_x = brep_interval_add(
		second_x_interval,
		brep_interval_scale(-1.0, first_x_interval));
	    const brep_interval intersection = brep_interval_add(
		first_x_interval, brep_interval_multiply(fraction, delta_x));
	    if (!std::isfinite(intersection.minimum) ||
		    !std::isfinite(intersection.maximum))
		return BREP_CLIP_INCONCLUSIVE;
	    range.minimum = std::min(range.minimum, intersection.minimum);
	    range.maximum = std::max(range.maximum, intersection.maximum);
	    have_range = true;
	}
    }
    if (!have_range)
	return BREP_CLIP_EMPTY;
    range.minimum = std::max(0.0, range.minimum);
    range.maximum = std::min(1.0, range.maximum);
    return range.minimum <= range.maximum ? BREP_CLIP_RANGE :
	BREP_CLIP_EMPTY;
}


/* If either ray-plane equation is zero at a surface point, its row or
 * column reductions must lie between the lower and upper coefficient
 * envelopes.  Intersect the resulting convex-hull ranges for both
 * equations.  An empty range is currently only observed: it is not used to
 * discard a box until the clipping path has a larger independent corpus. */
static brep_clip_status
brep_surface_clip_range(
    const double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], const double error[2], int direction,
    brep_interval &range)
{
    range.minimum = 0.0;
    range.maximum = 1.0;
    const int direction_order = order[direction];
    const int other_order = order[1 - direction];
    for (int equation = 0; equation < 2; ++equation) {
	double lower[BREP_DIRECT_BEZIER_MAX_ORDER];
	double upper[BREP_DIRECT_BEZIER_MAX_ORDER];
	for (int i = 0; i < direction_order; ++i) {
	    lower[i] = DBL_MAX;
	    upper[i] = -DBL_MAX;
	    for (int j = 0; j < other_order; ++j) {
		const int u = direction == 0 ? i : j;
		const int v = direction == 0 ? j : i;
		const size_t index = (size_t)u * order[1] + v;
		const brep_interval coefficient = brep_interval_expanded(
		    restricted[equation][index] - error[equation],
		    restricted[equation][index] + error[equation]);
		if (!std::isfinite(coefficient.minimum) ||
			!std::isfinite(coefficient.maximum))
		    return BREP_CLIP_INCONCLUSIVE;
		lower[i] = std::min(lower[i], coefficient.minimum);
		upper[i] = std::max(upper[i], coefficient.maximum);
	    }
	}
	brep_interval lower_range;
	brep_interval upper_range;
	const brep_clip_status lower_status = brep_bezier_halfspace_range(
	    lower, direction_order, true, lower_range);
	const brep_clip_status upper_status = brep_bezier_halfspace_range(
	    upper, direction_order, false, upper_range);
	if (lower_status == BREP_CLIP_INCONCLUSIVE ||
		upper_status == BREP_CLIP_INCONCLUSIVE)
	    return BREP_CLIP_INCONCLUSIVE;
	if (lower_status == BREP_CLIP_EMPTY ||
		upper_status == BREP_CLIP_EMPTY)
	    return BREP_CLIP_EMPTY;
	range.minimum = std::max(range.minimum,
	    std::max(lower_range.minimum, upper_range.minimum));
	range.maximum = std::min(range.maximum,
	    std::min(lower_range.maximum, upper_range.maximum));
	if (range.minimum > range.maximum)
	    return BREP_CLIP_EMPTY;
    }
    return BREP_CLIP_RANGE;
}


static bool
brep_fold_regular_graph_contract(
    brep_interval current[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], int regular_direction, bool &excluded,
    brep_interval *regular_range,
    size_t &contractions, size_t &high_water)
{
    if (!current || !order ||
	    (regular_direction != 0 && regular_direction != 1))
	return false;
    excluded = false;
    if (regular_range)
	*regular_range = {0.0, 1.0};
    const size_t count = (size_t)order[0] * order[1];
    for (int iteration = 0; iteration < 4; ++iteration) {
	if (brep_interval_coefficient_hull_excluded(current[0], count)) {
	    excluded = true;
	    return true;
	}
	brep_interval center_value;
	brep_interval derivative;
	brep_interval quotient;
	if (!brep_expansion_surface_fixed_parameter_hull(current[0],
		order[0], order[1], regular_direction, 0.5, center_value,
		high_water) ||
		!brep_expansion_surface_derivative_hull(current[0], order[0],
		    order[1], regular_direction, derivative, high_water) ||
		!brep_interval_divide_nonzero(center_value, derivative,
		    quotient))
	    break;
	brep_interval range = brep_interval_add({0.5, 0.5},
	    brep_interval_scale(-1.0, quotient));
	if (range.maximum < 0.0 || range.minimum > 1.0) {
	    excluded = true;
	    return true;
	}
	range.minimum = std::max(0.0, range.minimum);
	range.maximum = std::min(1.0, range.maximum);
	if (!(range.minimum < range.maximum) ||
		(!(range.minimum > 0.0) && !(range.maximum < 1.0)))
	    break;
	double minimum[2] = {0.0, 0.0};
	double maximum[2] = {1.0, 1.0};
	minimum[regular_direction] = range.minimum;
	maximum[regular_direction] = range.maximum;
	brep_interval next[2][BREP_DIRECT_BEZIER_MAX_CVS];
	bool restriction_available = true;
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_expansion_surface_restrict(current[equation], order[0],
		    order[1], minimum[0], maximum[0], minimum[1], maximum[1],
		    true, next[equation], high_water)) {
		restriction_available = false;
		break;
	    }
	}
	if (!restriction_available)
	    break;
	brep_interval mapped_range = {0.0, 1.0};
	if (regular_range) {
	    const brep_interval previous = *regular_range;
	    const brep_interval width = brep_interval_add(
		{previous.maximum, previous.maximum},
		brep_interval_scale(-1.0,
		    {previous.minimum, previous.minimum}));
	    mapped_range = brep_interval_add(
		{previous.minimum, previous.minimum},
		brep_interval_multiply(width, range));
	    mapped_range.minimum = std::max(previous.minimum,
		std::max(0.0, mapped_range.minimum));
	    mapped_range.maximum = std::min(previous.maximum,
		std::min(1.0, mapped_range.maximum));
	    if (!(mapped_range.minimum < mapped_range.maximum))
		break;
	}
	for (int equation = 0; equation < 2; ++equation)
	    (void)brep_interval_coefficients_power_two_normalize(
		next[equation], count);
	contractions++;
	if (regular_range)
	    *regular_range = mapped_range;
	for (int equation = 0; equation < 2; ++equation) {
	    for (size_t i = 0; i < count; ++i)
		current[equation][i] = next[equation][i];
	}
    }
    return true;
}


static bool
brep_fold_strip_excluded(
    const brep_interval input[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], int regular_direction,
    struct rt_brep_shot_trace *trace, size_t &high_water)
{
    if (!input || !order || !trace ||
	    (regular_direction != 0 && regular_direction != 1))
	return false;
    struct rt_brep_corridor_test_result graph = {};
    if (!brep_interval_surface_regular_graph(input, order[0], order[1],
	    regular_direction, graph) || !graph.regular_derivative_signed ||
	    !graph.regular_boundaries_opposed)
	return false;

    struct brep_fold_strip_box {
	double minimum;
	double maximum;
	int depth;
    };
    static const size_t strip_stack_capacity = 16;
    static const size_t strip_box_capacity = 511;
    static const int strip_maximum_depth = 8;
    brep_fold_strip_box pending[strip_stack_capacity];
    size_t pending_count = 1;
    size_t visited = 0;
    pending[0] = {0.0, 1.0, 0};
    const size_t count = (size_t)order[0] * order[1];
    const int weak_direction = 1 - regular_direction;
    while (pending_count) {
	const brep_fold_strip_box box = pending[--pending_count];
	if (++visited > strip_box_capacity) {
	    trace->surface_fold_strip_workspace_exhausted++;
	    return false;
	}
	trace->surface_fold_strip_boxes++;
	double minimum[2] = {0.0, 0.0};
	double maximum[2] = {1.0, 1.0};
	minimum[weak_direction] = box.minimum;
	maximum[weak_direction] = box.maximum;
	brep_interval current[2][BREP_DIRECT_BEZIER_MAX_CVS];
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_expansion_surface_restrict(input[equation], order[0],
		    order[1], minimum[0], maximum[0], minimum[1], maximum[1],
		    true, current[equation], high_water)) {
		trace->surface_fold_strip_restriction_failures++;
		return false;
	    }
	}

	bool box_excluded = false;
	if (!brep_fold_regular_graph_contract(current, order,
		regular_direction, box_excluded, NULL,
		trace->surface_fold_strip_contractions, high_water)) {
	    trace->surface_fold_strip_arithmetic_failures++;
	    return false;
	}
	if (box_excluded ||
		brep_interval_coefficient_hull_excluded(current[0], count) ||
		brep_interval_coefficient_hull_excluded(current[1], count))
	    continue;
	if (box.depth >= strip_maximum_depth) {
	    trace->surface_fold_strip_depth_exhausted++;
	    return false;
	}
	if (pending_count + 2 > strip_stack_capacity) {
	    trace->surface_fold_strip_workspace_exhausted++;
	    return false;
	}
	const double middle = 0.5 * box.minimum + 0.5 * box.maximum;
	if (!(middle > box.minimum) || !(middle < box.maximum)) {
	    trace->surface_fold_strip_arithmetic_failures++;
	    return false;
	}
	pending[pending_count++] = {middle, box.maximum, box.depth + 1};
	pending[pending_count++] = {box.minimum, middle, box.depth + 1};
    }
    return true;
}


/*
 * A determinant need only be one-signed on the phi=0 graph, not throughout
 * the full two-dimensional corridor.  Enclose that graph with interval
 * Newton on bounded weak-coordinate slabs.  Form the Bernstein determinant
 * before regular-coordinate contraction, then restrict that polynomial onto
 * the contracted enclosure so reparameterization does not shrink its value
 * scale.  Every slab must be covered with one common strict sign; exhaustion
 * or uncertain arithmetic is inconclusive.
 */
static bool
brep_fold_graph_determinant_signed(
    const brep_interval input[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], int regular_direction,
    struct rt_brep_shot_trace *trace, int &determinant_sign,
    size_t &high_water)
{
    if (!input || !order || !trace ||
	    (regular_direction != 0 && regular_direction != 1))
	return false;
    struct rt_brep_corridor_test_result graph = {};
    if (!brep_interval_surface_regular_graph(input, order[0], order[1],
	    regular_direction, graph) || !graph.regular_derivative_signed ||
	    !graph.regular_boundaries_opposed)
	return false;

    struct brep_fold_graph_box {
	double minimum;
	double maximum;
	int depth;
    };
    static const size_t graph_stack_capacity = 16;
    static const size_t graph_box_capacity = 511;
    static const int graph_maximum_depth = 8;
    brep_fold_graph_box pending[graph_stack_capacity];
    size_t pending_count = 1;
    size_t visited = 0;
    pending[0] = {0.0, 1.0, 0};
    determinant_sign = 0;
    const int weak_direction = 1 - regular_direction;
    while (pending_count) {
	const brep_fold_graph_box box = pending[--pending_count];
	if (++visited > graph_box_capacity) {
	    trace->surface_fold_corridor_graph_workspace_exhausted++;
	    return false;
	}
	trace->surface_fold_corridor_graph_boxes++;
	double minimum[2] = {0.0, 0.0};
	double maximum[2] = {1.0, 1.0};
	minimum[weak_direction] = box.minimum;
	maximum[weak_direction] = box.maximum;
	brep_interval current[2][BREP_DIRECT_BEZIER_MAX_CVS];
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_expansion_surface_restrict(input[equation], order[0],
		    order[1], minimum[0], maximum[0], minimum[1], maximum[1],
		    true, current[equation], high_water)) {
		trace->surface_fold_corridor_graph_restriction_failures++;
		return false;
	    }
	}
	brep_interval determinant[RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
	int determinant_order[2];
	if (!brep_interval_surface_determinant_coefficients(current, order[0],
		order[1], determinant, determinant_order)) {
	    trace->surface_fold_corridor_graph_determinant_failures++;
	    return false;
	}
	int box_sign = 0;
	if (!brep_interval_determinant_sign_from_coefficients(determinant,
		determinant_order, box_sign)) {
	    trace->surface_fold_corridor_graph_determinant_failures++;
	    return false;
	}
	if (!box_sign) {
	    bool graph_excluded = false;
	    brep_interval regular_range;
	    if (!brep_fold_regular_graph_contract(current, order,
		    regular_direction, graph_excluded, &regular_range,
		    trace->surface_fold_corridor_graph_contractions,
		    high_water))
		return false;
	    if (graph_excluded)
		continue;
	    double contracted_minimum[2] = {0.0, 0.0};
	    double contracted_maximum[2] = {1.0, 1.0};
	    contracted_minimum[regular_direction] = regular_range.minimum;
	    contracted_maximum[regular_direction] = regular_range.maximum;
	    brep_interval restricted_determinant[
		RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
	    if (!brep_interval_determinant_surface_restrict(determinant,
		    determinant_order[0], determinant_order[1],
		    contracted_minimum[0], contracted_maximum[0],
		    contracted_minimum[1], contracted_maximum[1],
		    restricted_determinant)) {
		trace->surface_fold_corridor_graph_restriction_failures++;
		trace->surface_fold_corridor_graph_determinant_failures++;
		return false;
	    }
	    if (!brep_interval_determinant_sign_from_coefficients(
		    restricted_determinant, determinant_order, box_sign)) {
		trace->surface_fold_corridor_graph_determinant_failures++;
		return false;
	    }
	}
	if (box_sign) {
	    if (determinant_sign && determinant_sign != box_sign) {
		trace->surface_fold_corridor_graph_sign_conflicts++;
		return false;
	    }
	    determinant_sign = box_sign;
	    continue;
	}
	if (box.depth >= graph_maximum_depth) {
	    trace->surface_fold_corridor_graph_depth_exhausted++;
	    return false;
	}
	if (pending_count + 2 > graph_stack_capacity) {
	    trace->surface_fold_corridor_graph_workspace_exhausted++;
	    return false;
	}
	const double middle = 0.5 * box.minimum + 0.5 * box.maximum;
	if (!(middle > box.minimum) || !(middle < box.maximum))
	    return false;
	pending[pending_count++] = {middle, box.maximum, box.depth + 1};
	pending[pending_count++] = {box.minimum, middle, box.depth + 1};
    }
    return determinant_sign != 0;
}


static bool
brep_expansion_curve_restrict(const brep_interval *input, int order,
    double minimum, double maximum, brep_interval *output,
    size_t &high_water)
{
    if (!input || !output || order < 2 ||
	    order > BREP_DIRECT_BEZIER_MAX_ORDER || minimum < 0.0 ||
	    maximum > 1.0 || !(minimum < maximum))
	return false;
    brep_interval ordinary[BREP_DIRECT_BEZIER_MAX_ORDER];
    if (!brep_interval_bezier_restrict(input, order, minimum, maximum,
	    ordinary))
	return false;
    brep_expansion_interval source[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int i = 0; i < order; ++i) {
	if (!brep_expansion_interval_from_interval(input[i], source[i],
		high_water))
	    return false;
    }
    for (int i = 0; i < order; ++i) {
	brep_expansion_interval restricted = {};
	brep_interval lower;
	brep_interval upper;
	if (!brep_expansion_bezier_blossom(source, order, minimum, maximum,
		i, restricted, high_water) ||
		!brep_expansion_bounds(restricted.minimum, lower) ||
		!brep_expansion_bounds(restricted.maximum, upper) ||
		lower.minimum > upper.maximum)
	    return false;
	output[i].minimum = std::max(lower.minimum, ordinary[i].minimum);
	output[i].maximum = std::min(upper.maximum, ordinary[i].maximum);
	if (output[i].minimum > output[i].maximum)
	    return false;
    }
    return true;
}


static bool
brep_expansion_curve_evaluate(const brep_interval *input, int order,
    double parameter, brep_interval &value, size_t &high_water)
{
    if (!input || order < 2 || order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    parameter < 0.0 || parameter > 1.0)
	return false;
    brep_expansion_interval source[BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int i = 0; i < order; ++i) {
	if (!brep_expansion_interval_from_interval(input[i], source[i],
		high_water))
	    return false;
    }
    brep_expansion_interval evaluated = {};
    brep_interval lower;
    brep_interval upper;
    if (!brep_expansion_bezier_evaluate(source, order, parameter, evaluated,
	    high_water) ||
	    !brep_expansion_bounds(evaluated.minimum, lower) ||
	    !brep_expansion_bounds(evaluated.maximum, upper) ||
	    lower.minimum > upper.maximum)
	return false;
    value = {lower.minimum, upper.maximum};
    return true;
}


static bool
brep_expansion_curve_derivative_hull(const brep_interval *input, int order,
    brep_interval &hull, size_t &high_water)
{
    if (!input || order < 2 || order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    hull = {DBL_MAX, -DBL_MAX};
    for (int i = 0; i < order - 1; ++i) {
	brep_interval derivative;
	if (!brep_expansion_difference_scaled(input[i + 1], input[i],
		order - 1, derivative, high_water))
	    return false;
	hull.minimum = std::min(hull.minimum, derivative.minimum);
	hull.maximum = std::max(hull.maximum, derivative.maximum);
    }
    return std::isfinite(hull.minimum) && std::isfinite(hull.maximum) &&
	hull.minimum <= hull.maximum;
}


static bool
brep_fold_boundary_graph_sign(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], int regular_direction, int boundary, int &sign,
    size_t &contractions, size_t &high_water)
{
    if (!values || !order ||
	    (regular_direction != 0 && regular_direction != 1) ||
	    (boundary != 0 && boundary != 1))
	return false;
    const int regular_order = order[regular_direction];
    const int weak_direction = 1 - regular_direction;
    const int weak_index = boundary ? order[weak_direction] - 1 : 0;
    brep_interval current[2][BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int equation = 0; equation < 2; ++equation) {
	for (int i = 0; i < regular_order; ++i) {
	    const int u = regular_direction == 0 ? i : weak_index;
	    const int v = regular_direction == 0 ? weak_index : i;
	    current[equation][i] = values[equation][(size_t)u * order[1] + v];
	}
    }

    for (int iteration = 0; iteration < 8; ++iteration) {
	brep_interval center;
	brep_interval derivative;
	brep_interval quotient;
	if (!brep_expansion_curve_evaluate(current[0], regular_order, 0.5,
		center, high_water) ||
		!brep_expansion_curve_derivative_hull(current[0], regular_order,
		    derivative, high_water) ||
		!brep_interval_divide_nonzero(center, derivative, quotient))
	    break;
	brep_interval range = brep_interval_add({0.5, 0.5},
	    brep_interval_scale(-1.0, quotient));
	if (range.maximum < 0.0 || range.minimum > 1.0)
	    return false;
	range.minimum = std::max(0.0, range.minimum);
	range.maximum = std::min(1.0, range.maximum);
	if (!(range.minimum < range.maximum)) {
	    if (!std::isfinite(range.minimum) ||
		    !std::isfinite(range.maximum) ||
		    range.minimum > range.maximum)
		return false;
	    brep_interval value;
	    if (!brep_expansion_curve_evaluate(current[1], regular_order,
		    range.minimum, value, high_water))
		return false;
	    contractions++;
	    sign = value.minimum > 0.0 ? 1 :
		(value.maximum < 0.0 ? -1 : 0);
	    return true;
	}
	if (!(range.minimum > 0.0) && !(range.maximum < 1.0))
	    break;
	brep_interval next[2][BREP_DIRECT_BEZIER_MAX_ORDER];
	bool restricted = true;
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_expansion_curve_restrict(current[equation], regular_order,
		    range.minimum, range.maximum, next[equation], high_water)) {
		restricted = false;
		break;
	    }
	    (void)brep_interval_coefficients_power_two_normalize(
		next[equation], regular_order);
	}
	if (!restricted)
	    break;
	for (int equation = 0; equation < 2; ++equation) {
	    for (int i = 0; i < regular_order; ++i)
		current[equation][i] = next[equation][i];
	}
	contractions++;
    }

    sign = 0;
    for (int i = 0; i < regular_order; ++i) {
	const int coefficient_sign = current[1][i].minimum > 0.0 ? 1 :
	    (current[1][i].maximum < 0.0 ? -1 : 0);
	if (!coefficient_sign || (sign && sign != coefficient_sign)) {
	    sign = 0;
	    break;
	}
	sign = coefficient_sign;
    }
    return true;
}


/*
 * The regular-graph theorem supplies one phi=0 point on each weak boundary.
 * Contract the regular coordinate around that point and prove opposed strict
 * psi signs there.  Continuity supplies existence, while the determinant
 * theorem supplies uniqueness.
 */
static bool
brep_fold_graph_boundary_existence(
    const brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], int regular_direction, bool &available,
    size_t &contractions, size_t &high_water)
{
    if (!values || !order ||
	    (regular_direction != 0 && regular_direction != 1))
	return false;
    available = false;
    int sign[2] = {0, 0};
    for (int boundary = 0; boundary < 2; ++boundary) {
	if (!brep_fold_boundary_graph_sign(values, order, regular_direction,
		boundary, sign[boundary], contractions, high_water))
	    return false;
    }
    available = true;
    return sign[0] && sign[1] && sign[0] != sign[1];
}


extern "C" int
_rt_brep_fold_graph_test(const fastf_t *first_minimum,
    const fastf_t *first_maximum, const fastf_t *second_minimum,
    const fastf_t *second_maximum, int u_order, int v_order,
    int regular_direction, int test_determinant, int test_exclusion,
    struct rt_brep_fold_graph_test_result *result)
{
    if (!first_minimum || !first_maximum || !second_minimum ||
	    !second_maximum || !result ||
	    (regular_direction != 0 && regular_direction != 1) ||
	    (test_determinant != 0 && test_determinant != 1) ||
	    (test_exclusion != 0 && test_exclusion != 1) ||
	    u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return 0;
    *result = {};
    brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const fastf_t *minimum[2] = {first_minimum, second_minimum};
    const fastf_t *maximum[2] = {first_maximum, second_maximum};
    const size_t count = (size_t)u_order * v_order;
    for (int equation = 0; equation < 2; ++equation) {
	for (size_t i = 0; i < count; ++i) {
	    if (!std::isfinite(minimum[equation][i]) ||
		    !std::isfinite(maximum[equation][i]) ||
		    minimum[equation][i] > maximum[equation][i])
		return 0;
	    values[equation][i] = {
		minimum[equation][i], maximum[equation][i]
	    };
	}
    }

    struct rt_brep_corridor_test_result corridor = {};
    if (!brep_interval_surface_corridor(values, u_order, v_order,
	    regular_direction, corridor))
	return 0;
    result->available = corridor.available;
    result->regular_derivative_signed =
	corridor.regular_derivative_signed;
    result->regular_boundaries_opposed =
	corridor.regular_boundaries_opposed;
    result->whole_determinant_signed = corridor.determinant_signed;
    result->determinant_sign = corridor.determinant_sign;

    struct rt_brep_shot_trace trace = {};
    size_t high_water = 0;
    const int order[2] = {u_order, v_order};

    if (test_determinant && corridor.regular_derivative_signed &&
	    corridor.regular_boundaries_opposed) {
	int graph_sign = corridor.determinant_sign;
	if (brep_fold_graph_determinant_signed(values, order,
		regular_direction, &trace, graph_sign, high_water)) {
	    result->graph_determinant_signed = 1;
	    result->determinant_sign = graph_sign;
	}
    }
    if (corridor.regular_derivative_signed &&
	    corridor.regular_boundaries_opposed) {
	bool boundary_available = false;
	const bool boundary_certified = brep_fold_graph_boundary_existence(
	    values, order, regular_direction, boundary_available,
	    result->boundary_existence_contractions, high_water);
	result->boundary_existence_available = boundary_available ? 1 : 0;
	result->boundary_existence_certified = boundary_certified ? 1 : 0;
    }
    if (test_exclusion)
	result->system_excluded = brep_fold_strip_excluded(values, order,
	    regular_direction, &trace, high_water) ? 1 : 0;
    result->graph_boxes = trace.surface_fold_corridor_graph_boxes;
    result->graph_contractions =
	trace.surface_fold_corridor_graph_contractions;
    result->graph_restriction_failures =
	trace.surface_fold_corridor_graph_restriction_failures;
    result->graph_determinant_failures =
	trace.surface_fold_corridor_graph_determinant_failures;
    result->graph_sign_conflicts =
	trace.surface_fold_corridor_graph_sign_conflicts;
    result->graph_depth_exhausted =
	trace.surface_fold_corridor_graph_depth_exhausted;
    result->graph_workspace_exhausted =
	trace.surface_fold_corridor_graph_workspace_exhausted;
    result->strip_boxes = trace.surface_fold_strip_boxes;
    result->strip_contractions =
	trace.surface_fold_strip_contractions;
    result->strip_restriction_failures =
	trace.surface_fold_strip_restriction_failures;
    result->strip_arithmetic_failures =
	trace.surface_fold_strip_arithmetic_failures;
    result->strip_depth_exhausted =
	trace.surface_fold_strip_depth_exhausted;
    result->strip_workspace_exhausted =
	trace.surface_fold_strip_workspace_exhausted;
    result->expansion_high_water = high_water;
    return 1;
}


extern "C" int
_rt_brep_clip_test(const fastf_t *first_coefficients,
    const fastf_t *second_coefficients, int u_order, int v_order,
    const fastf_t coefficient_error[2], fastf_t parameter_range[4])
{
    if (!first_coefficients || !second_coefficients || !coefficient_error ||
	    !parameter_range || u_order < 2 || v_order < 2 ||
	    u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    v_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    coefficient_error[0] < 0.0 || coefficient_error[1] < 0.0 ||
	    !std::isfinite(coefficient_error[0]) ||
	    !std::isfinite(coefficient_error[1]))
	return 0;
    double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const size_t count = (size_t)u_order * v_order;
    for (size_t i = 0; i < count; ++i) {
	restricted[0][i] = first_coefficients[i];
	restricted[1][i] = second_coefficients[i];
    }
    const int order[2] = {u_order, v_order};
    const double error[2] = {coefficient_error[0], coefficient_error[1]};
    for (int direction = 0; direction < 2; ++direction) {
	brep_interval range;
	if (brep_surface_clip_range(restricted, order, error, direction,
		range) != BREP_CLIP_RANGE)
	    return 0;
	parameter_range[2 * direction] = range.minimum;
	parameter_range[2 * direction + 1] = range.maximum;
    }
    return 1;
}


static brep_clip_status
brep_surface_clip_box(
    const double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], const double error[2],
    const brep_subdivision_box &source, brep_subdivision_box &clipped,
    double removed[2])
{
    clipped = source;
    bool significant = false;
    for (int direction = 0; direction < 2; ++direction) {
	brep_interval local;
	const brep_clip_status status = brep_surface_clip_range(restricted,
	    order, error, direction, local);
	if (status != BREP_CLIP_RANGE)
	    return status;
	const double local_width = local.maximum - local.minimum;
	if (local_width < BREP_DIRECT_CLIP_MINIMUM_RETAINED_FRACTION) {
	    const double center = 0.5 * (local.minimum + local.maximum);
	    local.minimum = center -
		0.5 * BREP_DIRECT_CLIP_MINIMUM_RETAINED_FRACTION;
	    local.maximum = center +
		0.5 * BREP_DIRECT_CLIP_MINIMUM_RETAINED_FRACTION;
	    if (local.minimum < 0.0) {
		local.maximum -= local.minimum;
		local.minimum = 0.0;
	    }
	    if (local.maximum > 1.0) {
		local.minimum -= local.maximum - 1.0;
		local.maximum = 1.0;
	    }
	    local.minimum = std::max(0.0, local.minimum);
	}
	const double width = source.maximum[direction] -
	    source.minimum[direction];
	if (!(width > 0.0) || !std::isfinite(width))
	    return BREP_CLIP_INCONCLUSIVE;
	const brep_interval origin = {source.minimum[direction],
	    source.minimum[direction]};
	const brep_interval mapped_minimum = brep_interval_add(origin,
	    brep_interval_scale(width, {local.minimum, local.minimum}));
	const brep_interval mapped_maximum = brep_interval_add(origin,
	    brep_interval_scale(width, {local.maximum, local.maximum}));
	clipped.minimum[direction] = std::max(source.minimum[direction],
	    mapped_minimum.minimum);
	clipped.maximum[direction] = std::min(source.maximum[direction],
	    mapped_maximum.maximum);
	if (!(clipped.minimum[direction] < clipped.maximum[direction]))
	    return BREP_CLIP_EMPTY;
	removed[direction] = std::max(0.0, 1.0 -
	    (clipped.maximum[direction] - clipped.minimum[direction]) / width);
	significant = significant ||
	    removed[direction] >= BREP_DIRECT_CLIP_MINIMUM_FRACTION;
    }
    if (significant) {
	const double retained_area = (1.0 - removed[0]) *
	    (1.0 - removed[1]);
	/* Depth measures binary-subdivision work.  Charge two levels when a
	 * simultaneous U/V contraction retains less than half of the box area,
	 * so clipping cannot silently exceed the existing depth resolution. */
	clipped.depth = std::min(BREP_DIRECT_SUBDIVISION_MAX_DEPTH,
	    clipped.depth + (retained_area < 0.5 ? 2 : 1));
    }
    return significant ? BREP_CLIP_RANGE : BREP_CLIP_INCONCLUSIVE;
}


static bool
brep_surface_box_t_range(const brep_surface_coefficients &coefficients,
    const brep_subdivision_box &box, double &minimum_t, double &maximum_t)
{
    double numerator[BREP_DIRECT_BEZIER_MAX_CVS];
    double weight[BREP_DIRECT_BEZIER_MAX_CVS];

    double numerator_error = 0.0;
    double weight_error = 0.0;
    if (!brep_scalar_surface_restrict_bounded(coefficients.ray_numerator,
	    coefficients.order[0], coefficients.order[1],
	    coefficients.ray_numerator_error, box.minimum[0], box.maximum[0],
	    box.minimum[1], box.maximum[1], numerator, numerator_error) ||
	    !brep_scalar_surface_restrict_bounded(coefficients.weight,
	    coefficients.order[0], coefficients.order[1],
	    coefficients.weight_error, box.minimum[0], box.maximum[0],
	    box.minimum[1], box.maximum[1], weight, weight_error))
	return false;
    const size_t count = (size_t)coefficients.order[0] *
	coefficients.order[1];
    minimum_t = DBL_MAX;
    maximum_t = -DBL_MAX;
    for (size_t i = 0; i < count; ++i) {
	const brep_interval numerator_interval = brep_interval_expanded(
	    numerator[i] - numerator_error, numerator[i] + numerator_error);
	const brep_interval weight_interval = brep_interval_expanded(
	    weight[i] - weight_error, weight[i] + weight_error);
	brep_interval quotient;
	if (!brep_interval_divide(numerator_interval, weight_interval,
		quotient))
	    return false;
	minimum_t = std::min(minimum_t, quotient.minimum);
	maximum_t = std::max(maximum_t, quotient.maximum);
    }
    return std::isfinite(minimum_t) && std::isfinite(maximum_t) &&
	minimum_t <= maximum_t;
}


static bool
brep_surface_determinant_sign(
    const double coefficients[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const int order[2], const double error[2], int &determinant_sign)
{
    if (!coefficients || !order || !error || order[0] < 2 || order[1] < 2 ||
	    order[0] > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    order[1] > BREP_DIRECT_BEZIER_MAX_ORDER || error[0] < 0.0 ||
	    error[1] < 0.0 || !std::isfinite(error[0]) ||
	    !std::isfinite(error[1]))
	return false;
    brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const size_t count = (size_t)order[0] * order[1];
    for (int equation = 0; equation < 2; ++equation) {
	for (size_t i = 0; i < count; ++i) {
	    if (!std::isfinite(coefficients[equation][i]))
		return false;
	    values[equation][i] = brep_interval_expanded(
		coefficients[equation][i] - error[equation],
		coefficients[equation][i] + error[equation]);
	}
    }
    return brep_interval_surface_determinant_sign(values, order[0], order[1],
	determinant_sign);
}


static void
brep_trace_localize_fold_root(struct rt_brep_shot_trace *trace,
    const brep_surface_coefficients &coefficients,
    const brep_interval corridor_values[2][BREP_DIRECT_BEZIER_MAX_CVS],
    int regular_direction, const double corridor_minimum[2],
    const double corridor_maximum[2], const double parameter[2],
    const brep_subdivision_box &fallback, brep_subdivision_box &localized,
    size_t &high_water)
{
    localized = fallback;
    if (!trace || !corridor_values || !corridor_minimum ||
	    !corridor_maximum || !parameter ||
	    (regular_direction != 0 && regular_direction != 1)) {
	if (trace)
	    trace->surface_fold_localization_failures++;
	return;
    }
    const int weak_direction = 1 - regular_direction;
    const double weak_width = corridor_maximum[weak_direction] -
	corridor_minimum[weak_direction];
    if (!(weak_width > 0.0)) {
	trace->surface_fold_localization_failures++;
	return;
    }
    const double weak_candidate =
	(parameter[weak_direction] - corridor_minimum[weak_direction]) /
	weak_width;
    const double boundary_distance = std::min(weak_candidate,
	1.0 - weak_candidate);
    if (!(boundary_distance > 1024.0 * DBL_EPSILON)) {
	trace->surface_fold_localization_failures++;
	return;
    }
    const double initial_half_width = std::min(0.25,
	0.5 * boundary_distance);
    double best_t_width = DBL_MAX;
    bool have_localized = false;
    const int order[2] = {
	coefficients.order[0], coefficients.order[1]
    };
    for (int level = 0; level < 24; ++level) {
	const double half_width = initial_half_width * std::ldexp(1.0, -level);
	const double weak_minimum = weak_candidate - half_width;
	const double weak_maximum = weak_candidate + half_width;
	if (!(weak_minimum >= 0.0) || !(weak_maximum <= 1.0) ||
		!(weak_minimum < weak_maximum))
	    continue;
	double minimum[2] = {0.0, 0.0};
	double maximum[2] = {1.0, 1.0};
	minimum[weak_direction] = weak_minimum;
	maximum[weak_direction] = weak_maximum;
	brep_interval restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
	trace->surface_fold_localization_attempts++;
	bool restriction_available = true;
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_expansion_surface_restrict(corridor_values[equation],
		    order[0], order[1], minimum[0], maximum[0],
		    minimum[1], maximum[1], true, restricted[equation],
		    high_water)) {
		restriction_available = false;
		break;
	    }
	}
	struct rt_brep_corridor_test_result graph = {};
	if (!restriction_available ||
		!brep_interval_surface_regular_graph(restricted, order[0],
		    order[1], regular_direction, graph) ||
		!graph.regular_derivative_signed ||
		!graph.regular_boundaries_opposed) {
	    trace->surface_fold_localization_failures++;
	    continue;
	}
	bool boundary_available = false;
	size_t boundary_contractions = 0;
	if (!brep_fold_graph_boundary_existence(restricted, order,
		regular_direction, boundary_available, boundary_contractions,
		high_water))
	    continue;
	if (!boundary_available) {
	    trace->surface_fold_localization_failures++;
	    continue;
	}
	trace->surface_fold_localization_certified++;
	trace->surface_fold_localization_contractions +=
	    boundary_contractions;
	brep_interval contracted[2][BREP_DIRECT_BEZIER_MAX_CVS];
	const size_t coefficient_count = (size_t)order[0] * order[1];
	for (int equation = 0; equation < 2; ++equation) {
	    for (size_t i = 0; i < coefficient_count; ++i)
		contracted[equation][i] = restricted[equation][i];
	}
	bool excluded = false;
	brep_interval regular_range;
	size_t regular_contractions = 0;
	if (!brep_fold_regular_graph_contract(contracted, order,
		regular_direction, excluded, &regular_range,
		regular_contractions, high_water) || excluded) {
	    trace->surface_fold_localization_failures++;
	    continue;
	}
	trace->surface_fold_localization_contractions +=
	    regular_contractions;
	brep_subdivision_box candidate_box = {};
	for (int direction = 0; direction < 2; ++direction) {
	    const double corridor_width = corridor_maximum[direction] -
		corridor_minimum[direction];
	    const double local_minimum = direction == regular_direction ?
		regular_range.minimum : weak_minimum;
	    const double local_maximum = direction == regular_direction ?
		regular_range.maximum : weak_maximum;
	    candidate_box.minimum[direction] = corridor_minimum[direction] +
		local_minimum * corridor_width;
	    candidate_box.maximum[direction] = corridor_minimum[direction] +
		local_maximum * corridor_width;
	    if (!(corridor_width > 0.0) ||
		    !(candidate_box.minimum[direction] <
		    candidate_box.maximum[direction])) {
		restriction_available = false;
		break;
	    }
	}
	if (!restriction_available) {
	    trace->surface_fold_localization_failures++;
	    continue;
	}
	double t_minimum;
	double t_maximum;
	if (!brep_surface_box_t_range(coefficients, candidate_box, t_minimum,
		t_maximum)) {
	    trace->surface_fold_localization_failures++;
	    continue;
	}
	const double t_width = t_maximum - t_minimum;
	if (!(t_width >= 0.0) || !std::isfinite(t_width)) {
	    trace->surface_fold_localization_failures++;
	    continue;
	}
	if (!have_localized || t_width < best_t_width) {
	    localized = candidate_box;
	    best_t_width = t_width;
	    have_localized = true;
	}
    }
    if (!have_localized)
	trace->surface_fold_localization_failures++;
}


static void
brep_trace_store_fold_root(struct rt_brep_shot_trace *trace,
    const brep_surface_coefficients &coefficients,
    const brep_surface_span &span, const brep_subdivision_box &box,
    const double parameter[2], int determinant_sign)
{
    if (!trace || !parameter || !determinant_sign) {
	if (trace)
	    trace->surface_fold_root_failures++;
	return;
    }
    trace->surface_fold_roots++;
    for (int direction = 0; direction < 2; ++direction) {
	const double width = box.maximum[direction] - box.minimum[direction];
	if (!(width > 0.0) || !std::isfinite(parameter[direction]) ||
		parameter[direction] < box.minimum[direction] ||
		parameter[direction] > box.maximum[direction]) {
	    trace->surface_fold_root_failures++;
	    return;
	}
    }
    double t_minimum;
    double t_maximum;
    double numerator;
    double weight;
    if (!brep_surface_box_t_range(coefficients, box, t_minimum, t_maximum) ||
	    !brep_fold_scalar_surface_evaluate(coefficients.ray_numerator,
		coefficients.order[0], coefficients.order[1], parameter,
		numerator) ||
	    !brep_fold_scalar_surface_evaluate(coefficients.weight,
		coefficients.order[0], coefficients.order[1], parameter,
		weight) ||
	    !(weight > DBL_MIN)) {
	trace->surface_fold_root_failures++;
	return;
    }
    const double dist = numerator / weight;
    if (!std::isfinite(dist) || dist < t_minimum || dist > t_maximum) {
	trace->surface_fold_root_failures++;
	return;
    }
    if (trace->stored_surface_fold_roots >= RT_BREP_TRACE_MAX_FOLD_ROOTS) {
	trace->surface_fold_root_overflow++;
	return;
    }
    struct rt_brep_trace_fold_root &root =
	trace->surface_fold_roots_data[trace->stored_surface_fold_roots++];
    root = {};
    root.dist = dist;
    root.t_min = t_minimum;
    root.t_max = t_maximum;
    root.face_index = span.face_index;
    root.span_index = span.span_index;
    root.adjacent_face_index = -99;
    root.trim_status = 1;
    root.hit_class = brep_hit::CLEAN_MISS;
    root.trim_distance = -1.0;
    root.determinant_sign = determinant_sign;
    root.direction = -1;
    for (int direction = 0; direction < 2; ++direction) {
	root.uv[direction] =
	    span.surface_domain[direction].ParameterAt(parameter[direction]);
	root.uv_min[direction] =
	    span.surface_domain[direction].ParameterAt(box.minimum[direction]);
	root.uv_max[direction] =
	    span.surface_domain[direction].ParameterAt(box.maximum[direction]);
    }
}


static void
brep_trace_expansion_krawczyk_contractions(
    struct rt_brep_shot_trace *trace,
    const brep_surface_coefficients &coefficients,
    const double initial_minimum[2], const double initial_maximum[2],
    const double initial_root[2], bool &candidate_certified)
{
    if (!trace || !initial_minimum || !initial_maximum || !initial_root)
	return;
    static const int maximum_contractions = 4;
    double minimum[2] = {initial_minimum[0], initial_minimum[1]};
    double maximum[2] = {initial_maximum[0], initial_maximum[1]};
    double root[2] = {initial_root[0], initial_root[1]};
    size_t high_water = coefficients.expansion_high_water;
    for (int contraction = 0; contraction < maximum_contractions;
	    ++contraction) {
	brep_interval values[2][BREP_DIRECT_BEZIER_MAX_CVS];
	bool restricted = coefficients.expansion_available;
	trace->surface_fold_expansion_attempts++;
	for (int equation = 0; restricted && equation < 2; ++equation) {
	    if (!brep_expansion_surface_restrict(
		    coefficients.value_expansion_interval[equation],
		    coefficients.order[0], coefficients.order[1],
		    minimum[0], maximum[0], minimum[1], maximum[1], true,
		    values[equation], high_water)) {
		restricted = false;
		break;
	    }
	}
	trace->surface_fold_expansion_high_water = std::max(
	    trace->surface_fold_expansion_high_water, high_water);
	if (!restricted) {
	    trace->surface_fold_expansion_failures++;
	    return;
	}

	struct rt_brep_krawczyk_test_result result = {};
	if (!brep_expansion_surface_krawczyk(values, coefficients.order[0],
		coefficients.order[1], root, result, high_water)) {
	    trace->surface_fold_expansion_failures++;
	    return;
	}
	trace->surface_fold_expansion_high_water = std::max(
	    trace->surface_fold_expansion_high_water, high_water);
	const double inclusion_margin = 512.0 * DBL_EPSILON;
	double image_excess = 0.0;
	for (int direction = 0; direction < 2; ++direction) {
	    image_excess = std::max(image_excess,
		inclusion_margin - result.image_minimum[direction]);
	    image_excess = std::max(image_excess,
		result.image_maximum[direction] - (1.0 - inclusion_margin));
	}
	image_excess = std::max(0.0, image_excess);
	if (!trace->surface_fold_expansion_available ||
		image_excess < trace->surface_fold_expansion_best_image_excess)
	    trace->surface_fold_expansion_best_image_excess = image_excess;
	trace->surface_fold_expansion_available++;
	if (trace->surface_fold_expansion_available == 1) {
	    trace->surface_fold_expansion_min_determinant_ratio =
		result.determinant_ratio;
	} else {
	    trace->surface_fold_expansion_min_determinant_ratio = std::min(
		(double)trace->surface_fold_expansion_min_determinant_ratio,
		(double)result.determinant_ratio);
	}
	if (result.certified) {
	    if (!candidate_certified) {
		trace->surface_fold_expansion_certified++;
		candidate_certified = true;
	    }
	    return;
	}
	if (contraction + 1 >= maximum_contractions)
	    return;

	trace->surface_fold_expansion_contraction_attempts++;
	double next_minimum[2];
	double next_maximum[2];
	bool changed = false;
	for (int direction = 0; direction < 2; ++direction) {
	    const double image_minimum = std::max(0.0,
		result.image_minimum[direction]);
	    const double image_maximum = std::min(1.0,
		result.image_maximum[direction]);
	    if (!(image_minimum < image_maximum)) {
		trace->surface_fold_expansion_contraction_empty++;
		return;
	    }
	    const double width = maximum[direction] - minimum[direction];
	    if (!(width > 0.0) || !std::isfinite(width)) {
		trace->surface_fold_expansion_failures++;
		return;
	    }
	    const brep_interval origin = {
		minimum[direction], minimum[direction]
	    };
	    const brep_interval mapped_minimum = brep_interval_add(origin,
		brep_interval_scale(width,
		    {image_minimum, image_minimum}));
	    const brep_interval mapped_maximum = brep_interval_add(origin,
		brep_interval_scale(width,
		    {image_maximum, image_maximum}));
	    next_minimum[direction] = std::max(minimum[direction],
		mapped_minimum.minimum);
	    next_maximum[direction] = std::min(maximum[direction],
		mapped_maximum.maximum);
	    if (!(next_minimum[direction] < next_maximum[direction])) {
		trace->surface_fold_expansion_contraction_empty++;
		return;
	    }
	    changed = changed || next_minimum[direction] > minimum[direction] ||
		next_maximum[direction] < maximum[direction];
	}
	if (!changed) {
	    trace->surface_fold_expansion_contraction_unchanged++;
	    return;
	}
	trace->surface_fold_expansion_contracted++;
	for (int direction = 0; direction < 2; ++direction) {
	    minimum[direction] = next_minimum[direction];
	    maximum[direction] = next_maximum[direction];
	    root[direction] = 0.5;
	}
    }
}


static void
brep_trace_fold_certificates(struct rt_brep_shot_trace *trace,
    const brep_surface_coefficients &coefficients,
    const brep_surface_span &span,
    const double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS],
    const double restricted_error[2], const brep_subdivision_box &box)
{
    struct rt_brep_fold_test_result candidates = {};
    trace->surface_fold_attempts++;
    if (!_rt_brep_fold_test(restricted[0], restricted[1],
	    coefficients.order[0], coefficients.order[1], restricted_error,
	    &candidates) || !candidates.frame_available ||
	    candidates.capacity_exhausted)
	return;
    trace->surface_fold_candidates += candidates.candidate_count;
    size_t corridor_high_water = coefficients.expansion_high_water;
    const size_t coefficient_count = (size_t)coefficients.order[0] *
	coefficients.order[1];
    trace->surface_fold_corridor_high_water = std::max(
	trace->surface_fold_corridor_high_water, corridor_high_water);
    if (!coefficients.expansion_available)
	trace->surface_fold_corridor_failures++;
    const double box_width[2] = {
	box.maximum[0] - box.minimum[0],
	box.maximum[1] - box.minimum[1]
    };
    for (size_t candidate = 0; candidate < candidates.candidate_count;
	    ++candidate) {
	double refined_root[2] = {
	    candidates.uv[candidate][0], candidates.uv[candidate][1]
	};
	(void)brep_fold_refine_candidate(restricted, coefficients.order,
	    refined_root);
	const double boundary_distance[2] = {
	    std::min(refined_root[0], 1.0 - refined_root[0]),
	    std::min(refined_root[1], 1.0 - refined_root[1])
	};
	if (!(boundary_distance[0] > 1024.0 * DBL_EPSILON) ||
		!(boundary_distance[1] > 1024.0 * DBL_EPSILON))
	    continue;
	const double initial_half_width[2] = {
	    std::min(0.125, 0.5 * boundary_distance[0]),
	    std::min(0.125, 0.5 * boundary_distance[1])
	};
	bool expansion_candidate_certified = false;
	for (int level = 0; level < 8; ++level) {
	    double subbox_minimum[2];
	    double subbox_maximum[2];
	    double subbox_root[2];
	    bool valid = true;
	    for (int direction = 0; direction < 2; ++direction) {
		const double local_half_width = initial_half_width[direction] *
		    std::ldexp(1.0, -level);
		const double local_minimum =
		    refined_root[direction] - local_half_width;
		const double local_maximum =
		    refined_root[direction] + local_half_width;
		const double candidate_parameter = box.minimum[direction] +
		    refined_root[direction] * box_width[direction];
		subbox_minimum[direction] = box.minimum[direction] +
		    local_minimum * box_width[direction];
		subbox_maximum[direction] = box.minimum[direction] +
		    local_maximum * box_width[direction];
		if (!(subbox_minimum[direction] < candidate_parameter) ||
			!(candidate_parameter < subbox_maximum[direction])) {
		    valid = false;
		    break;
		}
		subbox_root[direction] =
		    (candidate_parameter - subbox_minimum[direction]) /
		    (subbox_maximum[direction] - subbox_minimum[direction]);
	    }
	    if (!valid)
		break;
	    brep_interval subbox_values[2][BREP_DIRECT_BEZIER_MAX_CVS];
	    bool restricted_available = true;
	    for (int equation = 0; equation < 2; ++equation) {
		if (!brep_interval_surface_restrict(
			coefficients.value_interval[equation],
			coefficients.order[0], coefficients.order[1],
			subbox_minimum[0], subbox_maximum[0],
			subbox_minimum[1], subbox_maximum[1],
			subbox_values[equation])) {
		    restricted_available = false;
		    break;
		}
	    }
	    const double inclusion_margin = 512.0 * DBL_EPSILON;
	    bool binary_certified = false;
	    if (!restricted_available) {
		trace->surface_fold_restriction_failures++;
	    } else {
		struct rt_brep_krawczyk_test_result result = {};
		trace->surface_fold_krawczyk_attempts++;
		if (brep_interval_surface_krawczyk(subbox_values,
			coefficients.order[0], coefficients.order[1],
			subbox_root, result)) {
		    double image_excess = 0.0;
		    for (int direction = 0; direction < 2; ++direction) {
			image_excess = std::max(image_excess,
			    inclusion_margin - result.image_minimum[direction]);
			image_excess = std::max(image_excess,
			    result.image_maximum[direction] -
				(1.0 - inclusion_margin));
		    }
		    image_excess = std::max(0.0, image_excess);
		    if (!trace->surface_fold_krawczyk_available ||
			    image_excess < trace->surface_fold_best_image_excess)
			trace->surface_fold_best_image_excess = image_excess;
		    trace->surface_fold_krawczyk_available++;
		    if (trace->surface_fold_krawczyk_available == 1) {
			trace->surface_fold_min_determinant_ratio =
			    result.determinant_ratio;
		    } else {
			trace->surface_fold_min_determinant_ratio = std::min(
			    (double)trace->surface_fold_min_determinant_ratio,
			    (double)result.determinant_ratio);
		    }
		    if (result.certified) {
			trace->surface_fold_krawczyk_certified++;
			binary_certified = true;
		    }
		}
	    }

	    brep_trace_expansion_krawczyk_contractions(trace, coefficients,
		subbox_minimum, subbox_maximum, subbox_root,
		expansion_candidate_certified);
	    if (binary_certified)
		break;
	}

	brep_conditioned_surface_frame candidate_frame;
	const bool candidate_frame_available = coefficients.expansion_available &&
	    brep_conditioned_surface_frame_init(restricted, coefficients.order,
		restricted_error, candidate_frame);
	if (!candidate_frame_available) {
	    trace->surface_fold_corridor_failures++;
	    continue;
	}
	const int weak_direction = 1 - candidate_frame.regular_direction;
	const bool lower_boundary = refined_root[weak_direction] <= 0.5;
	const double weak_boundary_distance = lower_boundary ?
	    refined_root[weak_direction] :
	    1.0 - refined_root[weak_direction];
	const double split_fractions[] = {
	    0.0625, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875
	};
	const size_t split_count = expansion_candidate_certified ?
	    sizeof(split_fractions) / sizeof(split_fractions[0]) : 1;
	for (size_t split_index = 0; split_index < split_count; split_index++) {
	    const double local_split = lower_boundary ?
		split_fractions[split_index] * weak_boundary_distance :
		1.0 - split_fractions[split_index] * weak_boundary_distance;
	    const double split = box.minimum[weak_direction] +
		local_split * box_width[weak_direction];
	    double corridor_minimum[2] = {
		box.minimum[0], box.minimum[1]
	    };
	    double corridor_maximum[2] = {
		box.maximum[0], box.maximum[1]
	    };
	    double strip_minimum[2] = {
		box.minimum[0], box.minimum[1]
	    };
	    double strip_maximum[2] = {
		box.maximum[0], box.maximum[1]
	    };
	    if (lower_boundary) {
		strip_maximum[weak_direction] = split;
		corridor_minimum[weak_direction] = split;
	    } else {
		corridor_maximum[weak_direction] = split;
		strip_minimum[weak_direction] = split;
	    }
	    brep_interval corridor_source[2][BREP_DIRECT_BEZIER_MAX_CVS];
	    brep_interval strip_source[2][BREP_DIRECT_BEZIER_MAX_CVS];
	    const double corridor_parameter_scale[2] = {
		(corridor_maximum[0] - corridor_minimum[0]) / box_width[0],
		(corridor_maximum[1] - corridor_minimum[1]) / box_width[1]
	    };
	    bool restriction_available = true;
	    for (int equation = 0; equation < 2; ++equation) {
		if (!brep_expansion_surface_restrict(
			    coefficients.value_expansion_interval[equation],
			    coefficients.order[0], coefficients.order[1],
			    corridor_minimum[0], corridor_maximum[0],
			    corridor_minimum[1], corridor_maximum[1], false,
			    corridor_source[equation], corridor_high_water) ||
			!brep_expansion_surface_restrict(
			    coefficients.value_expansion_interval[equation],
			    coefficients.order[0], coefficients.order[1],
			    strip_minimum[0], strip_maximum[0],
			    strip_minimum[1], strip_maximum[1], false,
			    strip_source[equation], corridor_high_water)) {
		    restriction_available = false;
		    break;
		}
	    }
	    trace->surface_fold_corridor_high_water = std::max(
		trace->surface_fold_corridor_high_water, corridor_high_water);
	    if (!restriction_available) {
		trace->surface_fold_corridor_failures++;
		continue;
	    }
	    trace->surface_fold_corridor_attempts++;
	    brep_conditioned_surface_frame local_candidate_frame;
	    brep_interval corridor_values[2][BREP_DIRECT_BEZIER_MAX_CVS];
	    brep_interval strip_values[2][BREP_DIRECT_BEZIER_MAX_CVS];
	    if (!brep_conditioned_surface_frame_point_init(restricted,
		    coefficients.order, refined_root, corridor_parameter_scale,
		    candidate_frame.regular_direction, local_candidate_frame) ||
		    !brep_expansion_linear_coefficients(corridor_source,
			coefficient_count, local_candidate_frame.transform,
			corridor_values, corridor_high_water) ||
		    !brep_expansion_linear_coefficients(strip_source,
			coefficient_count, local_candidate_frame.transform,
			strip_values, corridor_high_water)) {
		trace->surface_fold_corridor_failures++;
		continue;
	    }
	    trace->surface_fold_corridor_high_water = std::max(
		trace->surface_fold_corridor_high_water, corridor_high_water);
	    struct rt_brep_corridor_test_result corridor = {};
	    if (!brep_interval_surface_corridor(corridor_values,
		    coefficients.order[0], coefficients.order[1],
		    local_candidate_frame.regular_direction, corridor)) {
		trace->surface_fold_corridor_failures++;
		continue;
	    }
	    if (corridor.regular_derivative_signed &&
		    corridor.regular_boundaries_opposed &&
		    !corridor.determinant_signed) {
		trace->surface_fold_corridor_graph_attempts++;
		int graph_sign = 0;
		if (brep_fold_graph_determinant_signed(corridor_values,
			coefficients.order,
			local_candidate_frame.regular_direction, trace,
			graph_sign, corridor_high_water)) {
		    corridor.determinant_signed = 1;
		    corridor.determinant_sign = graph_sign;
		    corridor.unique = 1;
		    trace->surface_fold_corridor_graph_certified++;
		} else {
		    trace->surface_fold_corridor_graph_failures++;
		}
	    }
	    trace->surface_fold_corridor_available++;
	    trace->surface_fold_corridor_regular_signed +=
		corridor.regular_derivative_signed ? 1 : 0;
	    trace->surface_fold_corridor_boundaries_opposed +=
		corridor.regular_boundaries_opposed ? 1 : 0;
	    trace->surface_fold_corridor_determinant_signed +=
		corridor.determinant_signed ? 1 : 0;
	    trace->surface_fold_corridor_unique += corridor.unique ? 1 : 0;
	    bool boundary_existence = false;
	    if (corridor.unique) {
		bool boundary_available = false;
		trace->surface_fold_boundary_existence_attempts++;
		boundary_existence = brep_fold_graph_boundary_existence(
		    corridor_values, coefficients.order,
		    local_candidate_frame.regular_direction,
		    boundary_available,
		    trace->surface_fold_boundary_existence_contractions,
		    corridor_high_water);
		trace->surface_fold_corridor_high_water = std::max(
		    trace->surface_fold_corridor_high_water,
		    corridor_high_water);
		trace->surface_fold_boundary_existence_available +=
		    boundary_available ? 1 : 0;
		trace->surface_fold_boundary_existence_certified +=
		    boundary_existence ? 1 : 0;
		if (!boundary_available)
		    trace->surface_fold_boundary_existence_failures++;
	    }
	    bool strip_excluded = false;
	    if (corridor.unique) {
		strip_excluded = brep_fold_strip_excluded(strip_values,
		    coefficients.order, local_candidate_frame.regular_direction,
		    trace, corridor_high_water);
		trace->surface_fold_corridor_high_water = std::max(
		    trace->surface_fold_corridor_high_water,
		    corridor_high_water);
		trace->surface_fold_strip_excluded += strip_excluded ? 1 : 0;
	    }
	    if ((expansion_candidate_certified || boundary_existence) &&
		    corridor.unique &&
		    strip_excluded) {
		trace->surface_fold_complete++;
		double candidate_parameter[2];
		for (int direction = 0; direction < 2; ++direction) {
		    candidate_parameter[direction] = box.minimum[direction] +
			refined_root[direction] * box_width[direction];
		}
		brep_subdivision_box root_box;
		brep_trace_localize_fold_root(trace, coefficients,
		    corridor_values, local_candidate_frame.regular_direction,
		    corridor_minimum, corridor_maximum, candidate_parameter,
		    box, root_box, corridor_high_water);
		trace->surface_fold_corridor_high_water = std::max(
		    trace->surface_fold_corridor_high_water,
		    corridor_high_water);
		brep_trace_store_fold_root(trace, coefficients, span, root_box,
		    candidate_parameter, corridor.determinant_sign);
		break;
	    }
	}
    }
}


static void
brep_trace_surface_isolation(struct rt_brep_shot_trace *trace,
    const brep_surface_coefficients &coefficients,
    const brep_surface_span &span, const ON_Ray &ray,
    bool adaptive, bool clipping, double target_t_width,
    bool exact_refinement)
{
    brep_subdivision_box pending[BREP_DIRECT_SUBDIVISION_CAPACITY];
    size_t pending_count = 1;
    pending[0].minimum[0] = 0.0;
    pending[0].minimum[1] = 0.0;
    pending[0].maximum[0] = 1.0;
    pending[0].maximum[1] = 1.0;
    pending[0].depth = 0;
    pending[0].exact_depth = 0;
    trace->surface_workspace_high_water = std::max(
	trace->surface_workspace_high_water, pending_count);

    double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
    double restricted_error[2] = {0.0, 0.0};
    const size_t count = (size_t)coefficients.order[0] *
	coefficients.order[1];
    while (pending_count) {
	brep_subdivision_box box = pending[--pending_count];
	trace->surface_subdivision_boxes++;
	trace->surface_subdivision_max_depth = std::max(
	    trace->surface_subdivision_max_depth, (size_t)box.depth);
	bool excluded = false;
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_scalar_surface_restrict_bounded(
		    coefficients.value[equation],
		    coefficients.order[0], coefficients.order[1],
		    coefficients.error[equation], box.minimum[0],
		    box.maximum[0], box.minimum[1], box.maximum[1],
		    restricted[equation], restricted_error[equation])) {
		trace->surface_workspace_exhausted++;
		return;
	    }
	    if (brep_coefficient_hull_excluded(restricted[equation], count,
		    restricted_error[equation])) {
		excluded = true;
		break;
	    }
	}
	if (excluded)
	    continue;
	trace->surface_rotated_hull_attempts++;
	const brep_rotated_hull_status rotated_hull =
	    brep_rotated_surface_hull_status(restricted, coefficients.order,
		restricted_error);
	if (rotated_hull == BREP_ROTATED_HULL_EXCLUDED) {
	    trace->surface_rotated_hull_exclusions++;
	    continue;
	}
	if (rotated_hull == BREP_ROTATED_HULL_INCONCLUSIVE)
	    trace->surface_rotated_hull_inconclusive++;
	else
	    trace->surface_rotated_hull_retained++;

	brep_subdivision_box clipped_box;
	double removed[2] = {0.0, 0.0};
	const brep_clip_status clip_status = clipping ?
	    brep_surface_clip_box(restricted, coefficients.order,
		restricted_error, box, clipped_box, removed) :
	    BREP_CLIP_INCONCLUSIVE;
	if (clipping)
	    trace->surface_clip_attempts++;
	if (clipping && clip_status == BREP_CLIP_RANGE) {
	    double clipped_restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
	    double clipped_error[2] = {0.0, 0.0};
	    bool restriction_failed = false;
	    bool clipped_excluded = false;
	    for (int equation = 0; equation < 2; ++equation) {
		if (!brep_scalar_surface_restrict_bounded(
			coefficients.value[equation], coefficients.order[0],
			coefficients.order[1], coefficients.error[equation],
			clipped_box.minimum[0], clipped_box.maximum[0],
			clipped_box.minimum[1], clipped_box.maximum[1],
			clipped_restricted[equation],
			clipped_error[equation])) {
		    restriction_failed = true;
		    break;
		}
		if (brep_coefficient_hull_excluded(
			clipped_restricted[equation], count,
			clipped_error[equation])) {
		    clipped_excluded = true;
		    break;
		}
	    }
	    if (!restriction_failed && !clipped_excluded) {
		box = clipped_box;
		trace->surface_subdivision_max_depth = std::max(
		    trace->surface_subdivision_max_depth, (size_t)box.depth);
		for (int equation = 0; equation < 2; ++equation) {
		    restricted_error[equation] = clipped_error[equation];
		    for (size_t i = 0; i < count; ++i)
			restricted[equation][i] =
			    clipped_restricted[equation][i];
		}
		trace->surface_clip_contractions++;
		if (removed[0] > 0.0)
		    trace->surface_clip_u_contractions++;
		if (removed[1] > 0.0)
		    trace->surface_clip_v_contractions++;
		trace->surface_clip_max_fraction_removed = std::max(
		    (double)trace->surface_clip_max_fraction_removed,
		    std::max(removed[0], removed[1]));
	    } else if (restriction_failed) {
		trace->surface_clip_restriction_failures++;
	    } else {
		trace->surface_clip_inconclusive++;
	    }
	} else if (clipping) {
	    trace->surface_clip_inconclusive++;
	}
	double minimum_t = 0.0;
	double maximum_t = 0.0;
	bool have_t_range = false;
	bool krawczyk_terminated = false;
	int determinant_sign = 0;
	bool have_corrected_root = false;
	double corrected_local_root[2] = {0.0, 0.0};
	if (adaptive &&
		box.depth >= BREP_DIRECT_SUBDIVISION_MIN_ADAPTIVE_DEPTH) {
	    const ON_2dPoint seed(
		0.5 * (box.minimum[0] + box.maximum[0]),
		0.5 * (box.minimum[1] + box.maximum[1]));
	    trace->surface_corrector_attempts++;
	    const brep_continuation_result root = brep_continuation_newton(
		span, ray, seed, box.minimum, box.maximum);
	    if ((size_t)root.status < RT_BREP_TRACE_CORRECTOR_STATUS_COUNT)
		trace->surface_corrector_status[root.status]++;
	    if (root.converged) {
		trace->surface_corrector_converged++;
		corrected_local_root[0] =
		    (root.uv.x - box.minimum[0]) /
		    (box.maximum[0] - box.minimum[0]);
		corrected_local_root[1] =
		    (root.uv.y - box.minimum[1]) /
		    (box.maximum[1] - box.minimum[1]);
		have_corrected_root = true;
		krawczyk_terminated = brep_surface_krawczyk_certified(
		    restricted, coefficients.order[0], coefficients.order[1],
		    restricted_error, corrected_local_root);
	    }
	    if (krawczyk_terminated)
		have_t_range = brep_surface_box_t_range(coefficients, box,
		    minimum_t, maximum_t);
	}
	const double midpoint[2] = {
	    0.5 * (box.minimum[0] + box.maximum[0]),
	    0.5 * (box.minimum[1] + box.maximum[1])
	};
	const bool splittable[2] = {
	    midpoint[0] > box.minimum[0] && midpoint[0] < box.maximum[0],
	    midpoint[1] > box.minimum[1] && midpoint[1] < box.maximum[1]
	};
	bool terminal = box.depth >= BREP_DIRECT_SUBDIVISION_MAX_DEPTH ||
	    (!splittable[0] && !splittable[1]);
	bool exact_refine_box = false;
	brep_interval exact_values[2][BREP_DIRECT_BEZIER_MAX_CVS];
	bool have_exact_values = false;
	if (!krawczyk_terminated && adaptive && exact_refinement && terminal &&
		coefficients.expansion_available) {
	    trace->surface_terminal_expansion_attempts++;
	    have_exact_values = true;
	    size_t high_water = coefficients.expansion_high_water;
	    for (int equation = 0; have_exact_values && equation < 2;
		    ++equation) {
		have_exact_values = brep_expansion_surface_restrict(
		    coefficients.value_expansion_interval[equation],
		    coefficients.order[0], coefficients.order[1],
		    box.minimum[0], box.maximum[0], box.minimum[1],
		    box.maximum[1], true, exact_values[equation], high_water);
	    }
	    trace->surface_terminal_expansion_high_water = std::max(
		trace->surface_terminal_expansion_high_water, high_water);
	    if (!have_exact_values) {
		trace->surface_terminal_expansion_failures++;
	    } else {
		trace->surface_terminal_expansion_available++;
		bool exact_excluded = false;
		for (int equation = 0; equation < 2; ++equation) {
		    if (brep_interval_coefficient_hull_excluded(
			    exact_values[equation], count)) {
			exact_excluded = true;
			break;
		    }
		}
		const brep_rotated_hull_status exact_rotated_hull =
		    exact_excluded ? BREP_ROTATED_HULL_EXCLUDED :
		    brep_expansion_rotated_surface_hull_status(exact_values,
			coefficients.order, high_water);
		if (exact_rotated_hull == BREP_ROTATED_HULL_EXCLUDED)
		    exact_excluded = true;
		if (exact_excluded) {
		    trace->surface_terminal_expansion_exclusions++;
		    continue;
		}
		if (have_corrected_root) {
		    struct rt_brep_krawczyk_test_result result = {};
		    if (brep_expansion_surface_krawczyk(exact_values,
			    coefficients.order[0], coefficients.order[1],
			    corrected_local_root, result, high_water) &&
			    result.certified) {
			krawczyk_terminated = true;
			trace->surface_terminal_expansion_krawczyk++;
			have_t_range = brep_surface_box_t_range(coefficients,
			    box, minimum_t, maximum_t);
		    }
		    trace->surface_terminal_expansion_high_water = std::max(
			trace->surface_terminal_expansion_high_water,
			high_water);
		}
		if (!krawczyk_terminated &&
			exact_rotated_hull == BREP_ROTATED_HULL_RETAINED &&
			box.exact_depth < BREP_DIRECT_EXACT_REFINEMENT_LEVELS &&
			(splittable[0] || splittable[1])) {
		    if (trace->surface_terminal_expansion_refinements <
			    BREP_DIRECT_EXACT_REFINEMENT_BUDGET) {
			trace->surface_terminal_expansion_refinements++;
			terminal = false;
			exact_refine_box = true;
		    } else {
			trace->surface_terminal_expansion_budget_exhausted++;
		    }
		}
	    }
	}
	/* A uniqueness proof may succeed well before the ordinary depth limit.
	 * Keep locally refining a status-2 box whose certified ray interval is
	 * still too wide to separate model-tolerance-scale neighboring roots. */
	if (krawczyk_terminated && adaptive && exact_refinement &&
	    target_t_width > 0.0 && have_t_range &&
	    maximum_t - minimum_t > target_t_width &&
	    box.exact_depth < BREP_DIRECT_EXACT_REFINEMENT_LEVELS &&
	    (splittable[0] || splittable[1])) {
	    if (trace->surface_terminal_expansion_refinements <
		BREP_DIRECT_EXACT_REFINEMENT_BUDGET) {
		trace->surface_terminal_expansion_refinements++;
		krawczyk_terminated = false;
		have_t_range = false;
		terminal = false;
		exact_refine_box = true;
	    } else {
		trace->surface_terminal_expansion_budget_exhausted++;
	    }
	}
	if (krawczyk_terminated) {
	    trace->surface_regular_orientation_attempts++;
	    if (have_exact_values ?
		!brep_interval_surface_determinant_sign(exact_values,
		    coefficients.order[0], coefficients.order[1],
		    determinant_sign) :
		!brep_surface_determinant_sign(restricted,
		    coefficients.order, restricted_error, determinant_sign)) {
		trace->surface_regular_orientation_failures++;
	    } else if (determinant_sign) {
		trace->surface_regular_orientation_signed++;
	    } else {
		trace->surface_regular_orientation_uncertain++;
	    }
	}
	if (krawczyk_terminated || terminal) {
	    if (!krawczyk_terminated && adaptive &&
		    rotated_hull == BREP_ROTATED_HULL_RETAINED)
		brep_trace_fold_certificates(trace, coefficients, span, restricted,
		    restricted_error, box);
	    if (!have_t_range && !brep_surface_box_t_range(coefficients, box,
		    minimum_t, maximum_t)) {
		trace->surface_workspace_exhausted++;
		return;
	    }
	    trace->surface_isolated_boxes++;
	    const double t_width = maximum_t - minimum_t;
	    if (trace->surface_isolated_boxes == 1)
		trace->surface_isolated_min_t_width = t_width;
	    else
		trace->surface_isolated_min_t_width = std::min(
		    (double)trace->surface_isolated_min_t_width, t_width);
	    trace->surface_isolated_max_t_width = std::max(
		(double)trace->surface_isolated_max_t_width, t_width);
	    if (krawczyk_terminated) {
		trace->surface_krawczyk_boxes++;
		if (trace->surface_krawczyk_boxes == 1)
		    trace->surface_krawczyk_min_depth = box.depth;
		else
		    trace->surface_krawczyk_min_depth = std::min(
			trace->surface_krawczyk_min_depth, (size_t)box.depth);
		trace->surface_krawczyk_max_depth = std::max(
		    trace->surface_krawczyk_max_depth, (size_t)box.depth);
	    }
	    if (trace->stored_surface_boxes >=
		    RT_BREP_TRACE_MAX_SURFACE_BOXES) {
		trace->surface_box_overflow++;
	    } else {
		struct rt_brep_trace_surface_box &stored =
		    trace->surface_boxes[trace->stored_surface_boxes++];
		stored.uv_min[0] = span.surface_domain[0].ParameterAt(
		    box.minimum[0]);
		stored.uv_min[1] = span.surface_domain[1].ParameterAt(
		    box.minimum[1]);
		stored.uv_max[0] = span.surface_domain[0].ParameterAt(
		    box.maximum[0]);
		stored.uv_max[1] = span.surface_domain[1].ParameterAt(
		    box.maximum[1]);
		stored.t_min = minimum_t;
		stored.t_max = maximum_t;
		stored.face_index = span.face_index;
		stored.span_index = span.span_index;
		stored.depth = box.depth;
		stored.disposition = krawczyk_terminated ?
		    RT_BREP_TRACE_BOX_RESOLVED_REGULAR :
		    RT_BREP_TRACE_BOX_UNRESOLVED;
		stored.determinant_sign = determinant_sign;
	    }
	    continue;
	}

	int direction = box.maximum[0] - box.minimum[0] >=
	    box.maximum[1] - box.minimum[1] ? 0 : 1;
	if (!splittable[direction])
	    direction = 1 - direction;
	if (pending_count + 2 > BREP_DIRECT_SUBDIVISION_CAPACITY) {
	    trace->surface_workspace_exhausted++;
	    return;
	}
	brep_subdivision_box &second = pending[pending_count++];
	second = box;
	second.minimum[direction] = midpoint[direction];
	second.depth++;
	second.exact_depth += exact_refine_box ? 1 : 0;
	brep_subdivision_box &first = pending[pending_count++];
	first = box;
	first.maximum[direction] = midpoint[direction];
	first.depth++;
	first.exact_depth += exact_refine_box ? 1 : 0;
	trace->surface_workspace_high_water = std::max(
	    trace->surface_workspace_high_water, pending_count);
    }
}


static bool
brep_continuation_certificate(struct rt_brep_shot_trace *trace,
    const brep_surface_span &span, const ON_Ray &ray,
    const double minimum[2], const double maximum[2],
    const ON_2dPoint &root_uv, double root_dist, double existing_dist)
{
    ON_3dVector first;
    ON_3dVector second;
    brep_surface_coefficients source;
    brep_surface_coefficients extension;
    if (!brep_ray_plane_frame(ray, first, second) ||
	    !brep_surface_coefficients_init(source, span, ray, first, second) ||
	    !brep_surface_coefficients_reparameterize(source, minimum, maximum,
		extension)) {
	trace->continuation_certificate_exhausted++;
	return false;
    }

    brep_surface_span extension_span;
    extension_span.face_index = span.face_index;
    extension_span.span_index = span.span_index;
    for (int direction = 0; direction < 2; ++direction)
	extension_span.surface_domain[direction] = ON_Interval(
	    span.surface_domain[direction].ParameterAt(minimum[direction]),
	    span.surface_domain[direction].ParameterAt(maximum[direction]));
    struct rt_brep_shot_trace certificate = {};
    brep_trace_surface_isolation(&certificate, extension, extension_span,
	ray, false, false, 0.0, false);
    trace->continuation_certificate_boxes +=
	certificate.surface_subdivision_boxes;
    trace->continuation_certificate_isolated +=
	certificate.surface_isolated_boxes;
    trace->continuation_certificate_workspace = std::max(
	trace->continuation_certificate_workspace,
	certificate.surface_workspace_high_water);
    trace->continuation_certificate_exhausted +=
	certificate.surface_workspace_exhausted +
	certificate.surface_box_overflow;
    if (certificate.surface_workspace_exhausted ||
	    certificate.surface_box_overflow)
	return false;

    size_t root_boxes = 0;
    size_t existing_overlap = 0;
    double minimum_t = DBL_MAX;
    double maximum_t = -DBL_MAX;
    const double parameter_tolerance = 1.0e-12;
    const double ray_length = ray.m_dir.Length();
    const double span_scale = span.bbox.IsValid() ?
	span.bbox.Diagonal().Length() : 0.0;
    const double coordinate_scale = std::max(span_scale,
	std::max(fabs(ray.m_origin.x),
	std::max(fabs(ray.m_origin.y), fabs(ray.m_origin.z))));
    if (!(ray_length > DBL_MIN) || !std::isfinite(ray_length) ||
	    !(span_scale > DBL_MIN) || !std::isfinite(span_scale) ||
	    !std::isfinite(coordinate_scale)) {
	trace->continuation_certificate_exhausted++;
	return false;
    }
    const double distance_tolerance = std::max(
	BREP_DIRECT_ROOT_RELATIVE_TOLERANCE * span_scale,
	BREP_DIRECT_EVALUATION_ULPS * DBL_EPSILON * coordinate_scale) /
	ray_length + 128.0 * DBL_EPSILON * std::max(1.0,
	std::max(fabs(root_dist), fabs(existing_dist)));
    for (size_t i = 0; i < certificate.stored_surface_boxes; ++i) {
	const struct rt_brep_trace_surface_box &box =
	    certificate.surface_boxes[i];
	const bool contains_root = box.face_index == span.face_index &&
	    root_uv.x >= box.uv_min[0] - parameter_tolerance &&
	    root_uv.x <= box.uv_max[0] + parameter_tolerance &&
	    root_uv.y >= box.uv_min[1] - parameter_tolerance &&
	    root_uv.y <= box.uv_max[1] + parameter_tolerance &&
	    root_dist >= box.t_min - distance_tolerance &&
	    root_dist <= box.t_max + distance_tolerance;
	if (!contains_root)
	    continue;
	root_boxes++;
	minimum_t = std::min(minimum_t, box.t_min);
	maximum_t = std::max(maximum_t, box.t_max);
	if (existing_dist >= box.t_min - distance_tolerance &&
		existing_dist <= box.t_max + distance_tolerance)
	    existing_overlap++;
    }
    trace->continuation_certificate_root_boxes += root_boxes;
    trace->continuation_certificate_existing_overlap += existing_overlap;
    if (root_boxes) {
	trace->continuation_certificate_t_min = minimum_t;
	trace->continuation_certificate_t_max = maximum_t;
    }
    return root_boxes > 0 &&
	root_boxes == certificate.surface_isolated_boxes && !existing_overlap;
}


static void
brep_trace_surface_spans(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    if (!trace || !bs)
	return;
    trace->prepared_surface_spans = bs->surface_spans.size();
    ON_3dVector first;
    ON_3dVector second;
    const bool valid_frame = brep_ray_plane_frame(ray, first, second);
    const double ray_length = ray.m_dir.Length();
    const bool adaptive = tol && tol->dist > 0.0 &&
	std::isfinite(tol->dist) && ray_length > DBL_MIN &&
	std::isfinite(ray_length);
    const double target_t_width = adaptive ?
	0.01 * tol->dist / ray_length : 0.0;
    for (std::vector<brep_face_record>::const_iterator record_it =
	    bs->face_records.begin(); record_it != bs->face_records.end();
	    ++record_it) {
	const brep_face_record &record = *record_it;
	if (!record.supported || !valid_frame) {
	    trace->unsupported_surface_faces++;
	    continue;
	}
	trace->supported_surface_faces++;
	if (record.nurb_form_status == 2)
	    trace->reparameterized_surface_faces++;
	for (size_t span_index = record.span_begin;
		span_index < record.span_begin + record.span_count;
		++span_index) {
	    if (brep_surface_span_excluded(bs->surface_spans[span_index], ray,
		    first, second)) {
		trace->excluded_surface_spans++;
	    } else {
		trace->candidate_surface_spans++;
		brep_surface_coefficients coefficients;
		if (brep_surface_coefficients_init(coefficients,
			bs->surface_spans[span_index], ray, first, second))
		    brep_trace_surface_isolation(trace, coefficients,
			bs->surface_spans[span_index], ray, adaptive, true,
			target_t_width, record.nurb_form_status == 2);
		else
		    trace->surface_workspace_exhausted++;
	    }
	}
    }
}


static bool
brep_fold_span_pair_compatible(const brep_surface_span &first_span,
    const brep_surface_span &second_span)
{
    if (first_span.face_index != second_span.face_index ||
	    first_span.span_index == second_span.span_index)
	return false;
    const auto same_parameter = [](double first, double second) {
	return std::isfinite(first) && std::isfinite(second) &&
	    !(first < second) && !(second < first);
    };
    int shared_direction = -1;
    for (int direction = 0; direction < 2; ++direction) {
	const bool touches =
	    same_parameter(first_span.surface_domain[direction].Max(),
		second_span.surface_domain[direction].Min()) ||
	    same_parameter(second_span.surface_domain[direction].Max(),
		first_span.surface_domain[direction].Min());
	if (touches) {
	    if (shared_direction >= 0)
		return false;
	    shared_direction = direction;
	}
    }
    if (shared_direction < 0)
	return false;
    const int transverse_direction = 1 - shared_direction;
    return std::max(first_span.surface_domain[transverse_direction].Min(),
	second_span.surface_domain[transverse_direction].Min()) <=
	std::min(first_span.surface_domain[transverse_direction].Max(),
	    second_span.surface_domain[transverse_direction].Max());
}


static bool
brep_fold_minimum_t_interval(const ON_Ray &ray, const struct bn_tol *tol,
    brep_interval &minimum_t)
{
    minimum_t = {0.0, 0.0};
    if (!tol || !(tol->dist > 0.0) || !std::isfinite(tol->dist))
	return true;
    brep_interval squared = {0.0, 0.0};
    for (int component = 0; component < 3; ++component) {
	if (!std::isfinite(ray.m_dir[component]))
	    return false;
	const brep_interval value = {
	    ray.m_dir[component], ray.m_dir[component]
	};
	squared = brep_interval_add(squared,
	    brep_interval_multiply(value, value));
    }
    if (!(squared.minimum > 0.0) ||
	    !std::isfinite(squared.maximum))
	return false;
    const brep_interval length = {
	std::nextafter(sqrt(squared.minimum), -INFINITY),
	std::nextafter(sqrt(squared.maximum), INFINITY)
    };
    return length.minimum > 0.0 &&
	brep_interval_divide({tol->dist, tol->dist}, length, minimum_t);
}


static int
brep_fold_gap_classify(const brep_interval &lower,
    const brep_interval &upper, const brep_interval &minimum_t,
    brep_interval &gap)
{
    if (!std::isfinite(lower.minimum) || !std::isfinite(lower.maximum) ||
	    !std::isfinite(upper.minimum) || !std::isfinite(upper.maximum) ||
	    !std::isfinite(minimum_t.minimum) ||
	    !std::isfinite(minimum_t.maximum) ||
	    lower.minimum > lower.maximum || upper.minimum > upper.maximum ||
	    minimum_t.minimum < 0.0 ||
	    minimum_t.minimum > minimum_t.maximum)
	return 0;
    gap = brep_interval_add(upper,
	brep_interval_scale(-1.0, lower));
    if (!std::isfinite(gap.minimum) || !std::isfinite(gap.maximum))
	return 0;
    if (gap.minimum > minimum_t.maximum)
	return RT_BREP_FOLD_GAP_RESOLVED;
    if (gap.maximum < minimum_t.minimum)
	return RT_BREP_FOLD_GAP_SUBMINIMUM;
    return RT_BREP_FOLD_GAP_AMBIGUOUS;
}


extern "C" int
_rt_brep_fold_interval_test(const fastf_t lower[2],
    const fastf_t upper[2], const fastf_t direction[3],
    fastf_t model_tolerance,
    struct rt_brep_fold_interval_test_result *result)
{
    if (!lower || !upper || !direction || !result ||
	    !(model_tolerance >= 0.0) || !std::isfinite(model_tolerance))
	return 0;
    ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_3dVector ray_direction(direction[0], direction[1], direction[2]);
    ON_Ray ray(origin, ray_direction);
    struct bn_tol tol = {};
    tol.dist = model_tolerance;
    brep_interval minimum_t;
    if (!brep_fold_minimum_t_interval(ray, &tol, minimum_t))
	return 0;
    brep_interval gap;
    const int classification = brep_fold_gap_classify(
	{lower[0], lower[1]}, {upper[0], upper[1]}, minimum_t, gap);
    if (!classification)
	return 0;
    *result = {};
    result->gap_minimum = gap.minimum;
    result->gap_maximum = gap.maximum;
    result->minimum_t_minimum = minimum_t.minimum;
    result->minimum_t_maximum = minimum_t.maximum;
    result->classification = classification;
    return 1;
}


static void
brep_trace_fold_events(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    if (!trace || !bs || !bs->brep ||
	    trace->surface_fold_root_overflow ||
	    trace->surface_fold_root_failures)
	return;
    const size_t root_count = trace->stored_surface_fold_roots;
    if (!root_count)
	return;

    for (size_t i = 0; i < root_count; ++i) {
	struct rt_brep_trace_fold_root &root =
	    trace->surface_fold_roots_data[i];
	if (root.face_index < 0 ||
		root.face_index >= bs->brep->m_F.Count() ||
		root.span_index < 0 ||
		(size_t)root.span_index >= bs->surface_spans.size() ||
		!root.determinant_sign) {
	    trace->surface_fold_root_failures++;
	    return;
	}
	const ON_BrepFace &face = bs->brep->m_F[root.face_index];
	const brep_surface_span &span = bs->surface_spans[root.span_index];
	if (span.face_index != root.face_index) {
	    trace->surface_fold_root_failures++;
	    return;
	}
	const ON_Surface *surface = face.SurfaceOf();
	const double nurbs_uv[2] = {root.uv[0], root.uv[1]};
	ON_2dPoint surface_uv;
	ON_3dPoint point;
	ON_3dVector normal;
	const double local_u =
	    span.surface_domain[0].NormalizedParameterAt(root.uv[0]);
	const double local_v =
	    span.surface_domain[1].NormalizedParameterAt(root.uv[1]);
	const double parameter_epsilon = 4096.0 * DBL_EPSILON;
	const ON_3dPoint nurbs_point = span.surface.PointAt(local_u, local_v);
	if (!std::isfinite(local_u) || !std::isfinite(local_v) ||
		local_u < -parameter_epsilon ||
		local_u > 1.0 + parameter_epsilon ||
		local_v < -parameter_epsilon ||
		local_v > 1.0 + parameter_epsilon || !nurbs_point.IsValid() ||
		!brep_surface_parameter_from_nurbs(bs, root.face_index, nurbs_uv,
		surface_uv) || !surface ||
		!surface_EvNormal(surface, surface_uv.x, surface_uv.y, point,
		    normal) || !normal.Unitize()) {
	    trace->surface_fold_root_failures++;
	    return;
	}
	const double span_scale = span.bbox.IsValid() ?
	    span.bbox.Diagonal().Length() : 0.0;
	const double coordinate_scale = std::max(span_scale,
	    std::max(fabs(point.x), std::max(fabs(point.y),
	    std::max(fabs(point.z), std::max(fabs(nurbs_point.x),
	    std::max(fabs(nurbs_point.y), fabs(nurbs_point.z)))))));
	const double distance_tolerance = std::max(
	    BREP_DIRECT_ROOT_RELATIVE_TOLERANCE * span_scale,
	    BREP_DIRECT_EVALUATION_ULPS * DBL_EPSILON * coordinate_scale);
	if (!std::isfinite(distance_tolerance) ||
		point.DistanceTo(nurbs_point) > 4.0 * distance_tolerance) {
	    trace->surface_fold_root_failures++;
	    return;
	}
	if (face.m_bRev)
	    normal.Reverse();
	root.normal_dot = normal * ray.m_dir;
	int oriented_sign = root.determinant_sign;
	if (face.m_bRev)
	    oriented_sign = -oriented_sign;
	root.direction = oriented_sign < 0 ? brep_hit::ENTERING :
	    brep_hit::LEAVING;
	trace->surface_fold_direction_checks++;
	const int evaluated_direction = root.normal_dot < 0.0 ?
	    brep_hit::ENTERING : brep_hit::LEAVING;
	if (!std::isfinite(root.normal_dot) ||
		(evaluated_direction != root.direction &&
		 fabs(root.normal_dot) > BREP_GRAZING_DOT_TOL))
	    trace->surface_fold_direction_mismatches++;
	if ((size_t)root.face_index >= bs->ctrees.size() ||
		!bs->ctrees[root.face_index]) {
	    trace->surface_fold_trim_failures++;
	    continue;
	}
	const BRNode *trim_node = NULL;
	size_t trim_candidates = 0;
	trace->surface_fold_trim_queries++;
	root.trim_status = bs->ctrees[root.face_index]->isTrimmed(
	    surface_uv, &trim_node,
	    root.trim_distance, BREP_EDGE_MISS_TOLERANCE,
	    &trim_candidates);
	trace->surface_fold_trim_candidates += trim_candidates;
	root.adjacent_face_index = trim_node ?
	    trim_node->m_adj_face_index : -99;
	root.hit_class = brep_initial_hit_class(root.trim_status,
	    root.trim_distance);
    }

    brep_interval minimum_t;
    if (!brep_fold_minimum_t_interval(ray, tol, minimum_t)) {
	trace->surface_fold_root_failures++;
	return;
    }
    trace->surface_fold_minimum_t = minimum_t.maximum;

    size_t degree[RT_BREP_TRACE_MAX_FOLD_ROOTS] = {};
    int neighbor[RT_BREP_TRACE_MAX_FOLD_ROOTS];
    for (size_t i = 0; i < root_count; ++i)
	neighbor[i] = -1;
    for (size_t i = 0; i < root_count; ++i) {
	const struct rt_brep_trace_fold_root &first =
	    trace->surface_fold_roots_data[i];
	const brep_surface_span &first_span =
	    bs->surface_spans[first.span_index];
	for (size_t j = i + 1; j < root_count; ++j) {
	    const struct rt_brep_trace_fold_root &second =
		trace->surface_fold_roots_data[j];
	    const brep_surface_span &second_span =
		bs->surface_spans[second.span_index];
	    if (!brep_fold_span_pair_compatible(first_span, second_span))
		continue;
	    degree[i]++;
	    degree[j]++;
	    neighbor[i] = (int)j;
	    neighbor[j] = (int)i;
	}
    }

    bool matched[RT_BREP_TRACE_MAX_FOLD_ROOTS] = {};
    bool have_pair_gap = false;
    for (size_t i = 0; i < root_count; ++i) {
	if (matched[i] || degree[i] != 1 || neighbor[i] < 0)
	    continue;
	const size_t j = (size_t)neighbor[i];
	if (j >= root_count || matched[j] || degree[j] != 1 ||
		neighbor[j] != (int)i)
	    continue;
	matched[i] = true;
	matched[j] = true;
	trace->surface_fold_topology_pairs++;
	const struct rt_brep_trace_fold_root *lower =
	    &trace->surface_fold_roots_data[i];
	const struct rt_brep_trace_fold_root *upper =
	    &trace->surface_fold_roots_data[j];
	if (upper->dist < lower->dist)
	    std::swap(lower, upper);
	const brep_interval lower_interval = {lower->t_min, lower->t_max};
	const brep_interval upper_interval = {upper->t_min, upper->t_max};
	brep_interval gap;
	const int gap_class = brep_fold_gap_classify(lower_interval,
	    upper_interval, minimum_t, gap);
	if (!gap_class) {
	    trace->surface_fold_root_failures++;
	    continue;
	}
	if (!have_pair_gap) {
	    trace->surface_fold_pair_gap_min = gap.minimum;
	    trace->surface_fold_pair_gap_max = gap.maximum;
	    have_pair_gap = true;
	} else {
	    trace->surface_fold_pair_gap_min = std::min(
		(double)trace->surface_fold_pair_gap_min, gap.minimum);
	    trace->surface_fold_pair_gap_max = std::max(
		(double)trace->surface_fold_pair_gap_max, gap.maximum);
	}
	if (lower->direction == upper->direction) {
	    if (!(lower->t_max < upper->t_min) &&
		    !(upper->t_max < lower->t_min))
		trace->surface_fold_duplicate_events++;
	    else
		trace->surface_fold_unmatched_roots += 2;
	    continue;
	}
	const bool material = lower->direction == brep_hit::ENTERING &&
	    upper->direction == brep_hit::LEAVING;
	if (material)
	    trace->surface_fold_material_pairs++;
	else
	    trace->surface_fold_void_pairs++;
	if (gap_class == RT_BREP_FOLD_GAP_RESOLVED) {
	    trace->surface_fold_resolved_pairs++;
	    if (material && trace->surface_fold_material_pairs == 1) {
		trace->surface_fold_segment_in = lower->dist;
		trace->surface_fold_segment_out = upper->dist;
	    }
	} else if (gap_class == RT_BREP_FOLD_GAP_SUBMINIMUM) {
	    trace->surface_fold_subminimum_contacts++;
	} else {
	    trace->surface_fold_tolerance_ambiguous++;
	}
    }
    for (size_t i = 0; i < root_count; ++i) {
	if (!matched[i])
	    trace->surface_fold_unmatched_roots++;
    }
}


struct brep_edge_face_frame {
    ON_3dPoint lift;
    ON_3dVector outward_normal;
    ON_3dVector interior_conormal;
};


static bool
brep_edge_face_frame_at(const ON_Brep &brep, const ON_BrepEdge &edge,
    const ON_BrepTrim &trim, double edge_parameter,
    const ON_3dVector &edge_tangent, brep_edge_face_frame &frame)
{
    const ON_BrepFace *face = trim.Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!face || !surface || trim.m_ei != edge.m_edge_index)
	return false;

    double trim_parameter = 0.0;
    if (!brep_edge_trim_parameter(edge, trim, edge_parameter,
	    trim_parameter))
	return false;

    ON_3dPoint uv;
    ON_3dVector trim_derivative;
    if (!trim.Ev1Der(trim_parameter, uv, trim_derivative) ||
	    !uv.IsValid() || !trim_derivative.IsValid())
	return false;

    ON_3dVector surface_u;
    ON_3dVector surface_v;
    if (!surface->Ev1Der(uv.x, uv.y, frame.lift, surface_u, surface_v) ||
	    !frame.lift.IsValid() || !surface_u.IsValid() ||
	    !surface_v.IsValid())
	return false;

    ON_3dVector parameter_normal = ON_CrossProduct(surface_u, surface_v);
    ON_3dVector loop_tangent = trim_derivative.x * surface_u +
	trim_derivative.y * surface_v;
    if (!parameter_normal.Unitize() || !loop_tangent.Unitize())
	return false;
    frame.outward_normal = parameter_normal;
    if (face->m_bRev)
	frame.outward_normal.Reverse();
    frame.interior_conormal = ON_CrossProduct(parameter_normal, loop_tangent);

    /* A discrepant trim lift need not have exactly the prepared edge tangent.
     * Project its local directions into the edge cross-section before using
     * them to define a material sector. */
    frame.outward_normal -=
	(frame.outward_normal * edge_tangent) * edge_tangent;
    frame.interior_conormal -=
	(frame.interior_conormal * edge_tangent) * edge_tangent;
    return frame.outward_normal.Unitize() &&
	frame.interior_conormal.Unitize() && face->m_face_index >= 0 &&
	face->m_face_index < brep.m_F.Count();
}


static double
brep_directed_angle(const ON_3dVector &from, const ON_3dVector &to,
    const ON_3dVector &axis)
{
    double angle = atan2(axis * ON_CrossProduct(from, to), from * to);
    if (angle < 0.0)
	angle += 2.0 * M_PI;
    return angle;
}


static void
brep_classify_edge_sector(struct rt_brep_trace_edge &observation,
    const struct brep_specific *bs, const brep_edge_record &record,
    const ON_Ray &ray)
{
    observation.line_transition_direction = -1;
    if (!bs->brep || record.edge_index < 0 ||
	    record.edge_index >= bs->brep->m_E.Count())
	return;
    const ON_Brep &brep = *bs->brep;
    const ON_BrepEdge &edge = brep.m_E[record.edge_index];
    if (edge.m_ti.Count() != 2)
	return;

    ON_3dPoint edge_point;
    ON_3dVector edge_tangent;
    if (!edge.Ev1Der(observation.edge_parameter, edge_point, edge_tangent) ||
	    !edge_point.IsValid() || !edge_tangent.Unitize())
	return;
    const double ray_length = ray.m_dir.Length();
    if (!(ray_length > DBL_MIN) || !std::isfinite(ray_length))
	return;
    const ON_3dVector ray_direction = ray.m_dir / ray_length;
    observation.ray_edge_dot = ray_direction * edge_tangent;

    brep_edge_face_frame frames[2];
    for (int side = 0; side < 2; ++side) {
	const int trim_index = edge.m_ti[side];
	if (trim_index < 0 || trim_index >= brep.m_T.Count() ||
		!brep_edge_face_frame_at(brep, edge, brep.m_T[trim_index],
		    observation.edge_parameter, edge_tangent, frames[side]))
	    return;
	observation.lift_distance[side] =
	    frames[side].lift.DistanceTo(edge_point);
	observation.face_normal_dot[side] =
	    frames[side].outward_normal * ray_direction;
    }

    const double coordinate_scale = std::max(1.0,
	std::max(fabs(edge_point.x), std::max(fabs(edge_point.y),
	    fabs(edge_point.z))));
    const double model_roundoff = std::max(ON_ZERO_TOLERANCE,
	128.0 * DBL_EPSILON * coordinate_scale);
    if (!ON_IsValid(record.tolerance) || record.tolerance < 0.0 ||
	    observation.lift_distance[0] > record.tolerance + model_roundoff ||
	    observation.lift_distance[1] > record.tolerance + model_roundoff)
	return;

    ON_3dVector cross_ray = ray_direction -
	(ray_direction * edge_tangent) * edge_tangent;
    if (!cross_ray.Unitize())
	return;

    const ON_3dVector positive_turn = ON_CrossProduct(edge_tangent,
	frames[0].interior_conormal);
    const int orientation = positive_turn * frames[0].outward_normal < 0.0 ?
	1 : -1;
    const ON_3dVector oriented_axis = orientation * edge_tangent;
    const ON_3dVector arrival_turn = ON_CrossProduct(oriented_axis,
	frames[1].interior_conormal);
    const double sector_angle = brep_directed_angle(
	frames[0].interior_conormal, frames[1].interior_conormal,
	oriented_axis);
    const double orientation_epsilon = 1.0e-10;
    if (orientation * positive_turn * frames[0].outward_normal >=
	    -orientation_epsilon ||
	    arrival_turn * frames[1].outward_normal <= orientation_epsilon ||
	    sector_angle <= orientation_epsilon ||
	    sector_angle >= 2.0 * M_PI - orientation_epsilon)
	return;

    observation.sector_valid = 1;

    /* Classify the two infinitesimal ray directions about an exact edge
     * crossing.  This is independent of the closest-point offset used by the
     * near-edge seam observer below: -cross_ray is the state immediately
     * before the edge and +cross_ray is the state immediately after it. */
    const auto line_state = [&](const ON_3dVector &direction) {
	const double angle = brep_directed_angle(
	    frames[0].interior_conormal, direction, oriented_axis);
	if (angle <= orientation_epsilon ||
		2.0 * M_PI - angle <= orientation_epsilon ||
		fabs(angle - sector_angle) <= orientation_epsilon)
	    return 0;
	return angle < sector_angle ? 1 : -1;
    };
    observation.line_before_state = line_state(-cross_ray);
    observation.line_after_state = line_state(cross_ray);
    /* The wedge states are angular/topological.  Do not invalidate them with
     * the legacy fixed grazing cutoff evaluated at a discrepant trim lift.
     * Event publication separately requires non-grazing surface roots and a
     * complete root/box transaction; coupled roots additionally require the
     * certified local component, which is the relevant conditioning test. */
    if (observation.line_before_state && observation.line_after_state) {
	observation.line_state_valid = 1;
	if (observation.line_before_state < 0 &&
		observation.line_after_state > 0)
	    observation.line_transition_direction = brep_hit::ENTERING;
	else if (observation.line_before_state > 0 &&
		observation.line_after_state < 0)
	    observation.line_transition_direction = brep_hit::LEAVING;
    }

    const ON_3dPoint ray_point = ray.m_origin +
	observation.ray_dist * ray.m_dir;
    ON_3dVector offset = ray_point - edge_point;
    offset -= (offset * edge_tangent) * edge_tangent;
    const double offset_length = offset.Length();
    const double contact_tolerance = std::max(ON_ZERO_TOLERANCE,
	128.0 * DBL_EPSILON * coordinate_scale);
    if (offset_length <= contact_tolerance) {
	observation.closest_state = 0;
	return;
    }
    offset /= offset_length;
    const double offset_angle = brep_directed_angle(
	frames[0].interior_conormal, offset, oriented_axis);
    observation.closest_state = offset_angle < sector_angle ? 1 : -1;
}


/* Prove that a complete line segment stays strictly inside the oriented
 * material wedge at an already certified edge parameter.  The projected
 * direction of a segment that does not contain the wedge origin sweeps one
 * monotone angular interval shorter than pi.  Requiring that unwrapped
 * interval to stay strictly between both sector boundaries also covers a
 * reflex material sector, for which endpoint classification alone would not
 * be sufficient.  This does not turn proximity into an edge intersection. */
static bool
brep_edge_sector_segment_inside(const struct brep_specific *bs,
    const struct rt_brep_trace_edge &observation,
    const ON_3dPoint &segment_start, const ON_3dPoint &segment_end)
{
    if (!bs || !bs->brep || !observation.sector_valid ||
	    observation.edge_index < 0 ||
	    observation.edge_index >= bs->brep->m_E.Count() ||
	    !segment_start.IsValid() || !segment_end.IsValid())
	return false;
    const ON_Brep &brep = *bs->brep;
    const ON_BrepEdge &edge = brep.m_E[observation.edge_index];
    if (edge.m_ti.Count() != 2)
	return false;
    ON_3dPoint edge_point;
    ON_3dVector edge_tangent;
    if (!edge.Ev1Der(observation.edge_parameter, edge_point,
	    edge_tangent) || !edge_point.IsValid() || !edge_tangent.Unitize())
	return false;
    brep_edge_face_frame frames[2];
    for (int side = 0; side < 2; ++side) {
	const int trim_index = edge.m_ti[side];
	if (trim_index < 0 || trim_index >= brep.m_T.Count() ||
		!brep_edge_face_frame_at(brep, edge, brep.m_T[trim_index],
		    observation.edge_parameter, edge_tangent, frames[side]))
	    return false;
    }
    const ON_3dVector positive_turn = ON_CrossProduct(edge_tangent,
	frames[0].interior_conormal);
    const int orientation = positive_turn * frames[0].outward_normal < 0.0 ?
	1 : -1;
    const ON_3dVector oriented_axis = orientation * edge_tangent;
    const ON_3dVector arrival_turn = ON_CrossProduct(oriented_axis,
	frames[1].interior_conormal);
    const double sector_angle = brep_directed_angle(
	frames[0].interior_conormal, frames[1].interior_conormal,
	oriented_axis);
    const double orientation_epsilon = 1.0e-10;
    if (orientation * positive_turn * frames[0].outward_normal >=
	    -orientation_epsilon ||
	    arrival_turn * frames[1].outward_normal <= orientation_epsilon ||
	    sector_angle <= orientation_epsilon ||
	    sector_angle >= 2.0 * M_PI - orientation_epsilon)
	return false;
    ON_3dVector offsets[2] = {
	segment_start - edge_point, segment_end - edge_point
    };
    for (int endpoint = 0; endpoint < 2; ++endpoint)
	offsets[endpoint] -=
	    (offsets[endpoint] * edge_tangent) * edge_tangent;
    const double coordinate_scale = std::max(1.0,
	std::max(fabs(edge_point.x), std::max(fabs(edge_point.y),
	std::max(fabs(edge_point.z), std::max(fabs(segment_start.x),
	std::max(fabs(segment_start.y), std::max(fabs(segment_start.z),
	std::max(fabs(segment_end.x), std::max(fabs(segment_end.y),
	    fabs(segment_end.z))))))))));
    const double point_epsilon = std::max(ON_ZERO_TOLERANCE,
	128.0 * DBL_EPSILON * coordinate_scale);
    const ON_3dVector offset_delta = offsets[1] - offsets[0];
    const double delta_length_squared = offset_delta.LengthSquared();
    double closest_parameter = 0.0;
    if (delta_length_squared > DBL_MIN) {
	closest_parameter = -(offsets[0] * offset_delta) /
	    delta_length_squared;
	closest_parameter = std::max(0.0,
	    std::min(1.0, closest_parameter));
    }
    const ON_3dVector closest_offset =
	offsets[0] + closest_parameter * offset_delta;
    if (closest_offset.Length() <= point_epsilon)
	return false;

    double endpoint_angle[2] = {0.0, 0.0};
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	if (!offsets[endpoint].Unitize())
	    return false;
	endpoint_angle[endpoint] = brep_directed_angle(
	    frames[0].interior_conormal, offsets[endpoint], oriented_axis);
	if (endpoint_angle[endpoint] <= orientation_epsilon ||
		endpoint_angle[endpoint] >=
		    sector_angle - orientation_epsilon)
	    return false;
    }
    const double signed_turn = atan2(oriented_axis *
	ON_CrossProduct(offsets[0], offsets[1]), offsets[0] * offsets[1]);
    if (!std::isfinite(signed_turn))
	return false;
    const double unwrapped_end = endpoint_angle[0] + signed_turn;
    return std::min(endpoint_angle[0], unwrapped_end) >
	orientation_epsilon &&
	std::max(endpoint_angle[0], unwrapped_end) <
	sector_angle - orientation_epsilon;
}


/* Outward-rounded vector intervals used only by the allocation-free oblique
 * seam certificate.  They deliberately retain magnitudes: every production
 * predicate below is a homogeneous sign test, so normalization would add
 * division without strengthening the proof. */
struct brep_interval_vector {
    brep_interval component[3];
};


static brep_interval_vector
brep_interval_vector_scale(double scale, const brep_interval_vector &value)
{
    brep_interval_vector result;
    for (int component = 0; component < 3; ++component)
	result.component[component] = brep_interval_scale(scale,
	    value.component[component]);
    return result;
}


static brep_interval_vector
brep_interval_vector_scale(const brep_interval &scale,
    const brep_interval_vector &value)
{
    brep_interval_vector result;
    for (int component = 0; component < 3; ++component)
	result.component[component] = brep_interval_multiply(scale,
	    value.component[component]);
    return result;
}


static brep_interval_vector
brep_interval_vector_add(const brep_interval_vector &first,
    const brep_interval_vector &second)
{
    brep_interval_vector result;
    for (int component = 0; component < 3; ++component)
	result.component[component] = brep_interval_add(
	    first.component[component], second.component[component]);
    return result;
}


static brep_interval_vector
brep_interval_vector_cross(const brep_interval_vector &first,
    const brep_interval_vector &second)
{
    brep_interval_vector result;
    result.component[0] = brep_interval_add(
	brep_interval_multiply(first.component[1], second.component[2]),
	brep_interval_scale(-1.0,
	    brep_interval_multiply(first.component[2],
		second.component[1])));
    result.component[1] = brep_interval_add(
	brep_interval_multiply(first.component[2], second.component[0]),
	brep_interval_scale(-1.0,
	    brep_interval_multiply(first.component[0],
		second.component[2])));
    result.component[2] = brep_interval_add(
	brep_interval_multiply(first.component[0], second.component[1]),
	brep_interval_scale(-1.0,
	    brep_interval_multiply(first.component[1],
		second.component[0])));
    return result;
}


static brep_interval
brep_interval_vector_dot(const brep_interval_vector &first,
    const brep_interval_vector &second)
{
    brep_interval result = {0.0, 0.0};
    for (int component = 0; component < 3; ++component)
	result = brep_interval_add(result, brep_interval_multiply(
	    first.component[component], second.component[component]));
    return result;
}


static brep_interval_vector
brep_interval_vector_from_point(const ON_3dPoint &point)
{
    brep_interval_vector result;
    for (int component = 0; component < 3; ++component)
	result.component[component] = {point[component], point[component]};
    return result;
}


static bool
brep_interval_curve_geometry(const brep_edge_span &span,
    const ON_Interval &edge_domain, brep_interval_vector &point,
    brep_interval_vector &tangent)
{
    const int order = span.curve.CVCount();
    if (!span.edge_domain.IsIncreasing() || !edge_domain.IsIncreasing() ||
	    order < 2 || order > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    const double parameter_scale = std::max(1.0,
	std::max(fabs(span.edge_domain.Min()),
	    fabs(span.edge_domain.Max())));
    const double parameter_padding =
	512.0 * DBL_EPSILON * parameter_scale;
    if (edge_domain.Min() < span.edge_domain.Min() - parameter_padding ||
	    edge_domain.Max() > span.edge_domain.Max() + parameter_padding)
	return false;
    const double minimum = std::max(0.0,
	span.edge_domain.NormalizedParameterAt(edge_domain.Min()));
    const double maximum = std::min(1.0,
	span.edge_domain.NormalizedParameterAt(edge_domain.Max()));
    if (!(minimum < maximum))
	return false;

    brep_interval source[4][BREP_DIRECT_BEZIER_MAX_ORDER];
    brep_interval restricted[4][BREP_DIRECT_BEZIER_MAX_ORDER];
    for (int i = 0; i < order; ++i) {
	ON_4dPoint cv;
	if (!span.curve.GetCV(i, cv) || !cv.IsValid() ||
		!(cv.w > 0.0) || !std::isfinite(cv.w))
	    return false;
	for (int component = 0; component < 4; ++component)
	    source[component][i] = {cv[component], cv[component]};
    }
    for (int component = 0; component < 4; ++component) {
	if (!brep_interval_bezier_restrict(source[component], order,
		minimum, maximum, restricted[component]))
	    return false;
    }
    brep_interval weight_hull;
    if (!brep_interval_coefficient_hull(restricted[3], order,
	    weight_hull) || !(weight_hull.minimum > 0.0))
	return false;
    for (int component = 0; component < 3; ++component) {
	point.component[component] = {DBL_MAX, -DBL_MAX};
	for (int i = 0; i < order; ++i) {
	    brep_interval value;
	    if (!brep_interval_divide(restricted[component][i],
		    restricted[3][i], value))
		return false;
	    point.component[component].minimum = std::min(
		point.component[component].minimum, value.minimum);
	    point.component[component].maximum = std::max(
		point.component[component].maximum, value.maximum);
	}
	if (!brep_interval_rational_curve_derivative_hull(
		restricted[component], restricted[3], order,
		tangent.component[component]))
	    return false;
	tangent.component[component] = brep_interval_scale(
	    1.0 / (maximum - minimum), tangent.component[component]);
    }
    return true;
}


static bool
brep_interval_surface_derivatives(const brep_surface_span &span,
    const brep_interval uv[2], brep_interval_vector derivative[2])
{
    const int order[2] = {span.surface.Order(0), span.surface.Order(1)};
    if (order[0] < 2 || order[1] < 2 ||
	    order[0] > BREP_DIRECT_BEZIER_MAX_ORDER ||
	    order[1] > BREP_DIRECT_BEZIER_MAX_ORDER)
	return false;
    double minimum[2];
    double maximum[2];
    double physical_width[2];
    for (int direction = 0; direction < 2; ++direction) {
	const ON_Interval &domain = span.surface_domain[direction];
	if (!domain.IsIncreasing())
	    return false;
	const double scale = std::max(1.0,
	    std::max(fabs(domain.Min()), std::max(fabs(domain.Max()),
		std::max(fabs(uv[direction].minimum),
		    fabs(uv[direction].maximum)))));
	const double padding = 512.0 * DBL_EPSILON * scale;
	double physical_minimum = std::max(domain.Min(),
	    uv[direction].minimum - padding);
	double physical_maximum = std::min(domain.Max(),
	    uv[direction].maximum + padding);
	/* A nearly zero restriction width amplifies the last few ulps of the
	 * rational derivative numerator when it is rescaled to the surface
	 * domain.  Bound a containing parameter neighborhood wide enough for a
	 * stable outward derivative hull.  This widens the geometric proof; it
	 * never narrows the caller's UV interval. */
	const double stable_width = std::min(domain.Length(), std::max(
	    4.0 * sqrt(DBL_EPSILON) * domain.Length(),
	    4096.0 * DBL_EPSILON * scale));
	if (physical_maximum - physical_minimum < stable_width) {
	    const double center = 0.5 * (physical_minimum + physical_maximum);
	    physical_minimum = std::max(domain.Min(),
		center - 0.5 * stable_width);
	    physical_maximum = std::min(domain.Max(),
		physical_minimum + stable_width);
	    physical_minimum = std::max(domain.Min(),
		physical_maximum - stable_width);
	}
	if (!(physical_minimum < physical_maximum)) {
	    if (physical_minimum <= domain.Min())
		physical_maximum = std::min(domain.Max(),
		    domain.Min() + padding);
	    else
		physical_minimum = std::max(domain.Min(),
		    domain.Max() - padding);
	}
	if (!(physical_minimum < physical_maximum))
	    return false;
	minimum[direction] = domain.NormalizedParameterAt(physical_minimum);
	maximum[direction] = domain.NormalizedParameterAt(physical_maximum);
	physical_width[direction] = physical_maximum - physical_minimum;
	if (!(minimum[direction] < maximum[direction]))
	    return false;
    }

    brep_interval source[4][BREP_DIRECT_BEZIER_MAX_CVS];
    brep_interval restricted[4][BREP_DIRECT_BEZIER_MAX_CVS];
    for (int i = 0; i < order[0]; ++i) {
	for (int j = 0; j < order[1]; ++j) {
	    ON_4dPoint cv;
	    if (!span.surface.GetCV(i, j, cv) || !cv.IsValid() ||
		    !(cv.w > 0.0) || !std::isfinite(cv.w))
		return false;
	    const size_t index = (size_t)i * order[1] + j;
	    for (int component = 0; component < 4; ++component)
		source[component][index] = {cv[component], cv[component]};
	}
    }
    for (int component = 0; component < 4; ++component) {
	if (!brep_interval_surface_restrict(source[component], order[0],
		order[1], minimum[0], maximum[0], minimum[1], maximum[1],
		restricted[component]))
	    return false;
    }
    for (int direction = 0; direction < 2; ++direction) {
	for (int component = 0; component < 3; ++component) {
	    if (!brep_interval_rational_surface_derivative_hull(
		    restricted[component], restricted[3], order[0], order[1],
		    direction, physical_width[direction],
		    derivative[direction].component[component]))
		return false;
	}
    }
    return true;
}


/* Bound a complete retained trim interval directly from its positive-weight
 * rational Bezier spans.  Derivatives are taken in each restricted span's
 * increasing local parameter.  That differs from the original trim parameter
 * only by a positive scalar, which preserves every homogeneous frame sign
 * used by the seam theorem. */
static bool
brep_interval_trim_cell_geometry(const struct brep_specific *bs,
    const brep_edge_trim_cell &cell, brep_interval uv[2],
    brep_interval derivative[2])
{
    if (!bs || !uv || !derivative || !cell.trim_domain.IsIncreasing() ||
	cell.trim_span_begin > bs->edge_trim_spans.size() ||
	cell.trim_span_count >
	    bs->edge_trim_spans.size() - cell.trim_span_begin ||
	!cell.trim_span_count ||
	!brep_trim_spans_cover(bs->edge_trim_spans, cell.trim_span_begin,
	    cell.trim_span_count, cell.trim_domain))
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	uv[direction] = {DBL_MAX, -DBL_MAX};
	derivative[direction] = {DBL_MAX, -DBL_MAX};
    }
    const ON_2dPoint reference(0.0, 0.0);
    for (size_t span_offset = 0;
	    span_offset < cell.trim_span_count; ++span_offset) {
	const brep_trim_span &span = bs->edge_trim_spans[
	    cell.trim_span_begin + span_offset];
	const double lower = std::max(cell.trim_domain.Min(),
	    span.trim_domain.Min());
	const double upper = std::min(cell.trim_domain.Max(),
	    span.trim_domain.Max());
	if (!(lower < upper))
	    continue;
	const double minimum =
	    span.trim_domain.NormalizedParameterAt(lower);
	const double maximum =
	    span.trim_domain.NormalizedParameterAt(upper);
	const int order = span.curve.CVCount();
	if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
		!(minimum < maximum) || minimum < 0.0 || maximum > 1.0 ||
		order < 2 || order > BREP_DIRECT_BEZIER_MAX_ORDER)
	    return false;
	brep_interval numerator[2][BREP_DIRECT_BEZIER_MAX_ORDER];
	brep_interval weight[BREP_DIRECT_BEZIER_MAX_ORDER];
	brep_interval piece_uv[2];
	brep_interval piece_derivative[2];
	if (!brep_interval_trim_curve_data(span.curve, reference, minimum,
		maximum, numerator, weight) ||
		!brep_interval_trim_uv_hull(numerator, weight, order, reference,
		    piece_uv))
	    return false;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!brep_interval_rational_curve_derivative_hull(
		    numerator[direction], weight, order,
		    piece_derivative[direction]))
		return false;
	    uv[direction].minimum = std::min(uv[direction].minimum,
		piece_uv[direction].minimum);
	    uv[direction].maximum = std::max(uv[direction].maximum,
		piece_uv[direction].maximum);
	    derivative[direction].minimum = std::min(
		derivative[direction].minimum,
		piece_derivative[direction].minimum);
	    derivative[direction].maximum = std::max(
		derivative[direction].maximum,
		piece_derivative[direction].maximum);
	}
    }
    for (int direction = 0; direction < 2; ++direction) {
	if (!std::isfinite(uv[direction].minimum) ||
		!std::isfinite(uv[direction].maximum) ||
		!std::isfinite(derivative[direction].minimum) ||
		!std::isfinite(derivative[direction].maximum) ||
		uv[direction].minimum > uv[direction].maximum ||
		derivative[direction].minimum > derivative[direction].maximum)
	    return false;
    }
    return true;
}


extern "C" int
_rt_brep_trim_interval_test(
    const struct rt_brep_trim_interval_test_span *input_spans,
    size_t span_count, size_t cell_span_begin, size_t cell_span_count,
    const fastf_t cell_domain[2],
    struct rt_brep_trim_interval_test_result *result)
{
    if (!input_spans || !span_count ||
	span_count > RT_BREP_TRIM_INTERVAL_TEST_MAX_SPANS || !cell_domain ||
	!result || !std::isfinite(cell_domain[0]) ||
	!std::isfinite(cell_domain[1]) ||
	!(cell_domain[0] < cell_domain[1]))
	return 0;
    *result = {};

    brep_specific bs;
    bs.edge_trim_spans.reserve(span_count);
    for (size_t span_index = 0; span_index < span_count; ++span_index) {
	const struct rt_brep_trim_interval_test_span &input =
	    input_spans[span_index];
	if (input.order < 2 ||
		input.order > RT_BREP_TRIM_INTERVAL_TEST_MAX_ORDER ||
		!std::isfinite(input.domain_minimum) ||
		!std::isfinite(input.domain_maximum) ||
		!(input.domain_minimum < input.domain_maximum))
	    return 0;
	ON_BezierCurve curve(2, true, input.order);
	for (int cv_index = 0; cv_index < input.order; ++cv_index) {
	    for (int component = 0; component < 3; ++component) {
		if (!std::isfinite(input.control[cv_index][component]))
		    return 0;
	    }
	    if (!curve.SetCV(cv_index, ON::euclidean_rational,
		    input.control[cv_index]))
		return 1;
	}
	brep_trim_span span;
	span.curve = curve;
	span.trim_domain = ON_Interval(input.domain_minimum,
	    input.domain_maximum);
	bs.edge_trim_spans.push_back(span);
    }

    brep_edge_trim_cell cell;
    cell.trim_domain = ON_Interval(cell_domain[0], cell_domain[1]);
    cell.trim_span_begin = cell_span_begin;
    cell.trim_span_count = cell_span_count;
    brep_interval uv[2];
    brep_interval derivative[2];
    result->available = brep_interval_trim_cell_geometry(&bs, cell, uv,
	derivative);
    if (result->available) {
	for (int direction = 0; direction < 2; ++direction) {
	    result->uv_minimum[direction] = uv[direction].minimum;
	    result->uv_maximum[direction] = uv[direction].maximum;
	    result->derivative_minimum[direction] =
		derivative[direction].minimum;
	    result->derivative_maximum[direction] =
		derivative[direction].maximum;
	}
    }
    return 1;
}


struct brep_interval_edge_face_frame {
    brep_interval_vector outward_normal;
    brep_interval_vector interior_conormal;
    brep_interval uv[2];
    int face_index = -1;
};


static bool
brep_interval_edge_face_frame_at(const struct brep_specific *bs,
    const brep_edge_trim_cell &cell, brep_interval_edge_face_frame &frame)
{
    if (!bs || !bs->brep || cell.trim_index < 0 ||
	    cell.trim_index >= bs->brep->m_T.Count() ||
	    !cell.trim_domain.IsIncreasing())
	return false;
    const ON_BrepTrim &trim = bs->brep->m_T[cell.trim_index];
    const ON_BrepFace *face = trim.Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!face || !surface)
	return false;
    int fixed_direction = -1;
    int varying_direction = -1;
    double varying_delta = 0.0;
    brep_interval trim_derivative[2];
    switch (trim.m_iso) {
	case ON_Surface::W_iso:
	case ON_Surface::E_iso:
	    fixed_direction = 0;
	    break;
	case ON_Surface::S_iso:
	case ON_Surface::N_iso:
	    fixed_direction = 1;
	    break;
	default:
	    break;
    }
    if (fixed_direction >= 0) {
	varying_direction = 1 - fixed_direction;
	const ON_3dPoint start = trim.PointAt(cell.trim_domain.Min());
	const ON_3dPoint end = trim.PointAt(cell.trim_domain.Max());
	if (!start.IsValid() || !end.IsValid())
	    return false;
	for (int direction = 0; direction < 2; ++direction) {
	    frame.uv[direction] = brep_interval_expanded(
		std::min(start[direction], end[direction]),
		std::max(start[direction], end[direction]));
	}
	varying_delta = end[varying_direction] -
	    start[varying_direction];
	if (!std::isfinite(varying_delta) ||
		std::fpclassify(varying_delta) == FP_ZERO)
	    return false;
    } else if (!brep_interval_trim_cell_geometry(bs, cell, frame.uv,
	    trim_derivative)) {
	return false;
    }

    const brep_face_record *face_record = brep_prepared_face(bs,
	face->m_face_index);
    if (!face_record || !face_record->supported)
	return false;
    const brep_surface_span *selected_span = NULL;
    for (size_t span_index = face_record->span_begin;
	    span_index < face_record->span_begin + face_record->span_count;
	    ++span_index) {
	const brep_surface_span &span = bs->surface_spans[span_index];
	bool contains = true;
	for (int direction = 0; direction < 2; ++direction) {
	    const ON_Interval &domain = span.surface_domain[direction];
	    const double scale = std::max(1.0,
		std::max(fabs(domain.Min()), fabs(domain.Max())));
	    const double padding = 1024.0 * DBL_EPSILON * scale;
	    contains = contains && frame.uv[direction].minimum >=
		domain.Min() - padding && frame.uv[direction].maximum <=
		domain.Max() + padding;
	}
	if (!contains)
	    continue;
	if (selected_span)
	    return false;
	selected_span = &span;
    }
    if (!selected_span)
	return false;

    brep_interval_vector surface_derivative[2];
    if (!brep_interval_surface_derivatives(*selected_span, frame.uv,
	    surface_derivative))
	return false;
    const brep_interval_vector parameter_normal =
	brep_interval_vector_cross(surface_derivative[0],
	    surface_derivative[1]);
    frame.outward_normal = face->m_bRev ?
	brep_interval_vector_scale(-1.0, parameter_normal) :
	parameter_normal;
    const brep_interval_vector loop_tangent = fixed_direction >= 0 ?
	brep_interval_vector_scale(varying_delta > 0.0 ? 1.0 : -1.0,
	    surface_derivative[varying_direction]) :
	brep_interval_vector_add(
	    brep_interval_vector_scale(trim_derivative[0],
		surface_derivative[0]),
	    brep_interval_vector_scale(trim_derivative[1],
		surface_derivative[1]));
    frame.interior_conormal = brep_interval_vector_cross(parameter_normal,
	loop_tangent);
    frame.face_index = face->m_face_index;
    return true;
}


static double
brep_interval_point_distance_lower(const brep_interval_vector &point,
    const ON_3dPoint &other)
{
    double squared = 0.0;
    for (int component = 0; component < 3; ++component) {
	double distance = 0.0;
	if (other[component] < point.component[component].minimum)
	    distance = point.component[component].minimum - other[component];
	else if (other[component] > point.component[component].maximum)
	    distance = other[component] - point.component[component].maximum;
	squared = std::fma(distance, distance, squared);
    }
    return sqrt(std::max(0.0, squared));
}


static bool
brep_interval_sector_inside(const struct brep_specific *bs,
    const struct rt_brep_trace_edge &observation,
    const brep_edge_trim_cell &first_cell,
    const brep_edge_trim_cell &second_cell,
    const ON_Interval &edge_domain, const ON_Ray &ray,
    const brep_interval &ray_parameter,
    brep_interval_edge_face_frame frame[2])
{
    if (!bs || !bs->brep || !edge_domain.IsIncreasing() ||
	    ray_parameter.minimum > ray_parameter.maximum ||
	    first_cell.edge_span >= bs->edge_spans.size() ||
	    first_cell.edge_span != second_cell.edge_span)
	return false;
    brep_interval_vector edge_point;
    brep_interval_vector tangent;
    if (!brep_interval_curve_geometry(
	    bs->edge_spans[first_cell.edge_span], edge_domain, edge_point,
	    tangent) ||
	    !brep_interval_edge_face_frame_at(bs, first_cell, frame[0]) ||
	    !brep_interval_edge_face_frame_at(bs, second_cell, frame[1]))
	return false;
    const ON_BrepEdge &edge = bs->brep->m_E[observation.edge_index];
    const double coordinate_scale = std::max(1.0,
	std::max(fabs(ray.m_origin.x), std::max(fabs(ray.m_origin.y),
	    fabs(ray.m_origin.z))));
    const double vertex_roundoff = std::max(ON_ZERO_TOLERANCE,
	4096.0 * DBL_EPSILON * coordinate_scale);
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	const int vertex_index = edge.m_vi[endpoint];
	if (vertex_index < 0 || vertex_index >= bs->brep->m_V.Count() ||
		brep_interval_point_distance_lower(edge_point,
		    bs->brep->m_V[vertex_index].point) <=
		observation.edge_tolerance + vertex_roundoff)
	    return false;
    }

    const brep_interval tangent_squared = brep_interval_vector_dot(tangent,
	tangent);
    if (!(tangent_squared.minimum > 0.0))
	return false;
    const brep_interval_vector ray_origin =
	brep_interval_vector_from_point(ray.m_origin);
    brep_interval_vector ray_direction;
    for (int component = 0; component < 3; ++component) {
	ray_direction.component[component] = {
	    ray.m_dir[component], ray.m_dir[component]};
    }
    const brep_interval orientation_test = brep_interval_vector_dot(
	brep_interval_vector_cross(tangent,
	    frame[0].interior_conormal), frame[0].outward_normal);
    int orientation = 0;
    if (orientation_test.maximum < 0.0)
	orientation = 1;
    else if (orientation_test.minimum > 0.0)
	orientation = -1;
    if (!orientation)
	return false;
    const brep_interval_vector axis = brep_interval_vector_scale(
	orientation, tangent);
    const brep_interval arrival = brep_interval_vector_dot(
	brep_interval_vector_cross(axis, frame[1].interior_conormal),
	frame[1].outward_normal);
    if (!(arrival.minimum > 0.0))
	return false;
    const brep_interval sector_turn = brep_interval_vector_dot(axis,
	brep_interval_vector_cross(frame[0].interior_conormal,
	    frame[1].interior_conormal));
    /* Triple products already discard tangent components.  The dot test used
     * for a smooth pi sector needs the same edge-normal projection.  Multiply
     * the projected dot by positive |T|^2 to avoid interval division. */
    const brep_interval sector_dot = brep_interval_add(
	brep_interval_multiply(tangent_squared, brep_interval_vector_dot(
	    frame[0].interior_conormal, frame[1].interior_conormal)),
	brep_interval_scale(-1.0, brep_interval_multiply(
	    brep_interval_vector_dot(frame[0].interior_conormal, tangent),
	    brep_interval_vector_dot(frame[1].interior_conormal, tangent))));

    brep_interval endpoint_first[2];
    brep_interval endpoint_second[2];
    const double endpoint_parameter[2] = {
	ray_parameter.minimum, ray_parameter.maximum
    };
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	const brep_interval exact_parameter = {
	    endpoint_parameter[endpoint], endpoint_parameter[endpoint]
	};
	brep_interval_vector endpoint_point;
	for (int component = 0; component < 3; ++component) {
	    endpoint_point.component[component] = brep_interval_add(
		ray_origin.component[component], brep_interval_multiply(
		    exact_parameter, ray_direction.component[component]));
	}
	const brep_interval_vector endpoint_offset = brep_interval_vector_add(
	    endpoint_point, brep_interval_vector_scale(-1.0, edge_point));
	endpoint_first[endpoint] = brep_interval_vector_dot(axis,
	    brep_interval_vector_cross(frame[0].interior_conormal,
		endpoint_offset));
	endpoint_second[endpoint] = brep_interval_vector_dot(axis,
	    brep_interval_vector_cross(endpoint_offset,
		frame[1].interior_conormal));
    }
    /* For each fixed edge parameter both boundary determinants are affine in
     * ray t.  Strict endpoint signs therefore cover the complete box t hull.
     * A convex sector needs both half spaces; a reflex sector needs either
     * one to remain positive.  The latter is stronger than pointwise union
     * membership and prevents a segment from crossing the complementary
     * convex cone. */
    const bool first_segment_positive = endpoint_first[0].minimum > 0.0 &&
	endpoint_first[1].minimum > 0.0;
    const bool second_segment_positive =
	endpoint_second[0].minimum > 0.0 &&
	endpoint_second[1].minimum > 0.0;
    if (sector_turn.minimum > 0.0) {
	const bool inside = first_segment_positive &&
	    second_segment_positive;
	return inside;
    }
    if (sector_turn.maximum < 0.0) {
	const bool inside = first_segment_positive ||
	    second_segment_positive;
	return inside;
    }
    if (sector_dot.maximum < 0.0 && first_segment_positive &&
	    second_segment_positive)
	return true;
    return false;
}


static bool
brep_oblique_trim_cell_restrict(const ON_BrepEdge &edge,
    const ON_BrepTrim &trim, const brep_edge_trim_cell &source,
    const ON_Interval &edge_domain, brep_edge_trim_cell &result)
{
    if (!source.edge_domain.IsIncreasing() ||
	    !source.trim_domain.IsIncreasing() || !edge_domain.IsIncreasing() ||
	    edge_domain.Min() < source.edge_domain.Min() ||
	    edge_domain.Max() > source.edge_domain.Max())
	return false;
    double parameter[2] = {0.0, 0.0};
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	if (!brep_edge_trim_parameter_in_domain(edge, trim,
		edge_domain[endpoint], source.trim_domain, 9,
		parameter[endpoint]))
	    return false;
    }
    const double parameter_scale = std::max(1.0,
	std::max(fabs(source.trim_domain.Min()),
	    fabs(source.trim_domain.Max())));
    const double parameter_padding =
	512.0 * DBL_EPSILON * parameter_scale;
    if (trim.m_bRev3d ?
	    parameter[0] <= parameter[1] + parameter_padding :
	    parameter[1] <= parameter[0] + parameter_padding)
	return false;
    result = source;
    result.edge_domain = edge_domain;
    result.trim_domain = ON_Interval(std::min(parameter[0], parameter[1]),
	std::max(parameter[0], parameter[1]));
    return result.trim_domain.IsIncreasing();
}


static bool
brep_edge_sector_oblique_contact_inside(
    const struct brep_specific *bs,
    const struct rt_brep_trace_edge &observation, const ON_Ray &ray,
    double contact_t_min, double contact_t_max,
    const bool contact_box[RT_BREP_TRACE_MAX_SURFACE_BOXES],
    const struct rt_brep_trace_surface_box *boxes, size_t box_count,
    size_t &frame_cells, size_t &box_links)
{
    const size_t maximum_work_cells = 8192;
    const size_t maximum_local_depth = 20;
    const size_t stack_capacity = 32;
    frame_cells = 0;
    box_links = 0;
    if (!bs || !bs->brep || !boxes || observation.edge_index < 0 ||
	    observation.edge_index >= bs->brep->m_E.Count() ||
	    !observation.frame_interval_supported ||
	    !std::isfinite(contact_t_min) || !std::isfinite(contact_t_max) ||
	    !(contact_t_min < contact_t_max) ||
	    box_count > RT_BREP_TRACE_MAX_SURFACE_BOXES)
	return false;
    const brep_edge_record *record = brep_vertex_edge_record(bs,
	observation.edge_index);
    if (!record || !record->frame_interval_supported ||
	    !record->trim_cell_count[0] || !record->trim_cell_count[1])
	return false;
    const ON_BrepEdge &edge = bs->brep->m_E[observation.edge_index];
    if (edge.m_ti.Count() != 2)
	return false;

    /* Each contact box must own a nonempty interval of its incident trim and
     * its full t hull must be inside every material frame on that interval.
     * Prep supplies paired, discrepancy-certified trim partitions.  Shot
     * refines them on a fixed stack, proves a nonzero consistent edge sweep,
     * and finally proves that the certified box intervals cover the complete
     * contact hull.  No proximity sample or ray projection stands in for UV
     * ownership, and every cap or unsupported frame fails closed. */
    brep_interval coverage[RT_BREP_TRACE_MAX_SURFACE_BOXES];
    int sweep_sign = 0;
    size_t work_cells = 0;
    for (size_t box_index = 0; box_index < box_count; ++box_index) {
	if (!contact_box[box_index])
	    continue;
	const struct rt_brep_trace_surface_box &box = boxes[box_index];
	if (!std::isfinite(box.t_min) || !std::isfinite(box.t_max) ||
		box.t_min > box.t_max || box.t_min < contact_t_min ||
		box.t_max > contact_t_max)
	    return false;
	bool certified = false;
	size_t first_index = 0;
	size_t second_index = 0;
	while (!certified && first_index < record->trim_cell_count[0] &&
		second_index < record->trim_cell_count[1]) {
	    const brep_edge_trim_cell &first = bs->edge_trim_cells[
		record->trim_cell_begin[0] + first_index];
	    const brep_edge_trim_cell &second = bs->edge_trim_cells[
		record->trim_cell_begin[1] + second_index];
	    const double edge_minimum = std::max(first.edge_domain.Min(),
		second.edge_domain.Min());
	    const double edge_maximum = std::min(first.edge_domain.Max(),
		second.edge_domain.Max());
	    if (edge_minimum < edge_maximum &&
		    first.edge_span == second.edge_span) {
		struct oblique_cell {
		    brep_edge_trim_cell side[2];
		    size_t depth = 0;
		} stack[stack_capacity];
		size_t stack_count = 0;
		const ON_Interval parent_domain(edge_minimum, edge_maximum);
		const ON_BrepTrim &first_trim =
		    bs->brep->m_T[first.trim_index];
		const ON_BrepTrim &second_trim =
		    bs->brep->m_T[second.trim_index];
		oblique_cell initial;
		if (!brep_oblique_trim_cell_restrict(edge, first_trim, first,
			parent_domain, initial.side[0]) ||
			!brep_oblique_trim_cell_restrict(edge, second_trim, second,
			parent_domain, initial.side[1]))
		    return false;
		stack[stack_count++] = initial;
		while (!certified && stack_count) {
		    if (++work_cells > maximum_work_cells)
			return false;
		    const oblique_cell cell = stack[--stack_count];
		    const ON_Interval edge_domain = cell.side[0].edge_domain;
		    int box_side = -1;
		    brep_interval trim_uv[2];
		    for (int side = 0; side < 2; ++side) {
			const ON_BrepTrim &trim = bs->brep->m_T[
			    cell.side[side].trim_index];
			const ON_BrepFace *face = trim.Face();
			if (!face || face->m_face_index != box.face_index)
			    continue;
			if (box_side >= 0)
			    return false;
			box_side = side;
			if (trim.m_iso == ON_Surface::W_iso ||
				trim.m_iso == ON_Surface::E_iso ||
				trim.m_iso == ON_Surface::S_iso ||
				trim.m_iso == ON_Surface::N_iso) {
			    const ON_3dPoint start = trim.PointAt(
				cell.side[side].trim_domain.Min());
			    const ON_3dPoint end = trim.PointAt(
				cell.side[side].trim_domain.Max());
			    if (!start.IsValid() || !end.IsValid())
				return false;
			    for (int direction = 0; direction < 2; ++direction)
				trim_uv[direction] = brep_interval_expanded(
				    std::min(start[direction], end[direction]),
				    std::max(start[direction], end[direction]));
			} else {
			    brep_interval trim_derivative[2];
			    if (!brep_interval_trim_cell_geometry(bs,
				    cell.side[side], trim_uv, trim_derivative))
				return false;
			}
		    }
		    if (box_side < 0)
			continue;
		    bool disjoint = false;
		    bool contained = true;
		    for (int direction = 0; direction < 2; ++direction) {
			const double scale = std::max(1.0,
			    std::max(fabs(box.uv_min[direction]),
				fabs(box.uv_max[direction])));
			const double padding = 1024.0 * DBL_EPSILON * scale;
			disjoint = disjoint ||
			    trim_uv[direction].maximum <
				box.uv_min[direction] - padding ||
			    trim_uv[direction].minimum >
				box.uv_max[direction] + padding;
			contained = contained &&
			    trim_uv[direction].minimum >=
				box.uv_min[direction] - padding &&
			    trim_uv[direction].maximum <=
				box.uv_max[direction] + padding;
		    }
		    if (disjoint)
			continue;
		    if (contained) {
			const brep_interval box_t = {box.t_min, box.t_max};
			brep_interval_edge_face_frame frames[2];
			if (brep_interval_sector_inside(bs, observation,
				cell.side[0], cell.side[1], edge_domain, ray,
				box_t, frames)) {
			    brep_interval_vector edge_point;
			    brep_interval_vector tangent;
			    if (!brep_interval_curve_geometry(bs->edge_spans[
				    cell.side[0].edge_span], edge_domain,
				    edge_point, tangent))
				return false;
			    brep_interval_vector direction;
			    for (int component = 0; component < 3; ++component)
				direction.component[component] = {
				    ray.m_dir[component], ray.m_dir[component]};
			    const brep_interval sweep = brep_interval_vector_dot(
				tangent, direction);
			    const int cell_sign = sweep.minimum > 0.0 ? 1 :
				(sweep.maximum < 0.0 ? -1 : 0);
			    if (!cell_sign ||
				    (sweep_sign && sweep_sign != cell_sign))
				return false;
			    sweep_sign = cell_sign;
			    if (frame_cells >= RT_BREP_TRACE_MAX_SURFACE_BOXES)
				return false;
			    coverage[frame_cells++] = box_t;
			    box_links++;
			    certified = true;
			    continue;
			}
		    }
		    const double midpoint = edge_domain.Mid();
		    if (cell.depth >= maximum_local_depth ||
			    !(edge_domain.Min() < midpoint) ||
			    !(midpoint < edge_domain.Max()) ||
			    stack_count + 2 > stack_capacity)
			continue;
		    oblique_cell children[2];
		    const ON_Interval child_domain[2] = {
			ON_Interval(edge_domain.Min(), midpoint),
			ON_Interval(midpoint, edge_domain.Max())
		    };
		    for (int child = 0; child < 2; ++child) {
			if (!brep_oblique_trim_cell_restrict(edge, first_trim,
				cell.side[0], child_domain[child],
				children[child].side[0]) ||
				!brep_oblique_trim_cell_restrict(edge, second_trim,
				cell.side[1], child_domain[child],
				children[child].side[1]))
			    return false;
			children[child].depth = cell.depth + 1;
		    }
		    const int preferred = observation.edge_parameter >= midpoint ?
			1 : 0;
		    stack[stack_count++] = children[1 - preferred];
		    stack[stack_count++] = children[preferred];
		}
	    }
	    const double first_end = first.edge_domain.Max();
	    const double second_end = second.edge_domain.Max();
	    const double scale = std::max(1.0,
		std::max(fabs(first_end), fabs(second_end)));
	    const double padding = 512.0 * DBL_EPSILON * scale;
	    if (first_end <= second_end + padding)
		first_index++;
	    if (second_end <= first_end + padding)
		second_index++;
	}
	if (!certified)
	    return false;
    }
    if (!frame_cells || !sweep_sign || frame_cells != box_links)
	return false;
    for (size_t cell_index = 1; cell_index < frame_cells; ++cell_index) {
	const brep_interval value = coverage[cell_index];
	size_t previous = cell_index;
	while (previous && coverage[previous - 1].minimum > value.minimum) {
	    coverage[previous] = coverage[previous - 1];
	    previous--;
	}
	coverage[previous] = value;
    }
    const double t_scale = std::max(1.0,
	std::max(fabs(contact_t_min), fabs(contact_t_max)));
    const double t_padding = 8192.0 * DBL_EPSILON * t_scale;
    if (coverage[0].minimum > contact_t_min + t_padding)
	return false;
    double covered = coverage[0].maximum;
    for (size_t cell_index = 1; cell_index < frame_cells; ++cell_index) {
	if (coverage[cell_index].minimum > covered + t_padding)
	    return false;
	covered = std::max(covered, coverage[cell_index].maximum);
    }
    return covered >= contact_t_max - t_padding;
}


static void
brep_observe_edges(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs,
    const ON_Ray &ray)
{
    if (!trace || !bs)
	return;
    trace->prepared_edge_spans = bs->edge_spans.size();
    for (std::vector<brep_edge_record>::const_iterator record_it =
	    bs->edge_records.begin(); record_it != bs->edge_records.end();
	    ++record_it) {
	const brep_edge_record &record = *record_it;
	trace->manifold_edges++;
	if (!record.supported) {
	    trace->edge_evaluation_failures++;
	    continue;
	}

	double edge_parameter = 0.0;
	double distance = DBL_MAX;
	double ray_dist = 0.0;
	size_t candidate_spans = 0;
	bool evaluated = false;
	for (size_t span_index = record.span_begin;
		span_index < record.span_begin + record.span_count;
		++span_index) {
	    const brep_edge_span &span = bs->edge_spans[span_index];
	    const bool valid_tolerance = ON_IsValid(record.tolerance) &&
		record.tolerance >= 0.0 && record.discrepancy_authorized &&
		record.correspondence_supported;
	    const double expansion = valid_tolerance ? record.tolerance : 0.0;
	    if (valid_tolerance && brep_line_intersects_box(ray, span.bbox,
		    expansion)) {
		candidate_spans++;
		trace->candidate_edge_spans++;
	    }

	    double local_parameter = 0.0;
	    double candidate_distance = DBL_MAX;
	    double candidate_ray_dist = 0.0;
	    if (!brep_line_curve_distance(span.curve, ray, local_parameter,
		    candidate_distance, candidate_ray_dist))
		continue;
	    evaluated = true;
	    if (candidate_distance < distance) {
		distance = candidate_distance;
		ray_dist = candidate_ray_dist;
		edge_parameter = span.edge_domain.ParameterAt(local_parameter);
	    }
	}
	if (!evaluated) {
	    trace->edge_evaluation_failures++;
	    continue;
	}
	trace->edge_observations++;
	if (trace->stored_edges >= RT_BREP_TRACE_MAX_EDGES) {
	    trace->edge_overflow++;
	    continue;
	}

	struct rt_brep_trace_edge &observation =
	    trace->edges[trace->stored_edges++];
	observation.distance = distance;
	observation.ray_dist = ray_dist;
	observation.edge_parameter = edge_parameter;
	observation.edge_tolerance = record.tolerance;
	observation.model_tolerance = record.model_tolerance;
	observation.declared_tolerance = record.declared_tolerance;
	observation.measured_discrepancy = record.measured_discrepancy;
	observation.discrepancy_lower_bound =
	    record.discrepancy_lower_bound;
	observation.discrepancy_upper_bound =
	    record.discrepancy_upper_bound;
	observation.discrepancy_bound_tolerance =
	    record.discrepancy_bound_tolerance;
	observation.discrepancy_bound_cells =
	    record.discrepancy_bound_cells;
	observation.discrepancy_bound_depth =
	    record.discrepancy_bound_depth;
	observation.frame_interval_cells = record.trim_cell_count[0] +
	    record.trim_cell_count[1];
	observation.edge_index = record.edge_index;
	observation.face_index[0] = record.face_index[0];
	observation.face_index[1] = record.face_index[1];
	observation.candidate_spans = candidate_spans;
	observation.discrepancy_measured = record.discrepancy_measured;
	observation.correspondence_screened =
	    record.correspondence_screened;
	observation.correspondence_supported =
	    record.correspondence_supported;
	observation.correspondence_cells = record.correspondence_cells;
	observation.correspondence_depth = record.correspondence_depth;
	observation.correspondence_exhausted =
	    record.correspondence_exhausted;
	observation.discrepancy_bounded = record.discrepancy_bounded;
	observation.discrepancy_bound_exhausted =
	    record.discrepancy_bound_exhausted;
	observation.discrepancy_endpoints_certified =
	    record.discrepancy_endpoints_certified;
	observation.frame_interval_supported =
	    record.frame_interval_supported &&
	    record.trim_cell_count[0] && record.trim_cell_count[1];
	observation.discrepancy_sample_authorized =
	    record.discrepancy_sample_authorized;
	observation.discrepancy_proof_class = record.discrepancy_proof_class;
	observation.discrepancy_authorized = record.discrepancy_authorized;
	observation.tolerance_inferred = record.tolerance_inferred;
	const double roundoff = std::max(ON_ZERO_TOLERANCE,
	    128.0 * DBL_EPSILON * std::max(1.0, distance));
	observation.within_edge_tolerance =
	    ON_IsValid(record.tolerance) && record.tolerance >= 0.0 &&
	    record.discrepancy_authorized && record.correspondence_supported &&
	    distance <= record.tolerance + roundoff;
	if (observation.within_edge_tolerance) {
	    trace->edges_within_tolerance++;
	    brep_classify_edge_sector(observation, bs, record, ray);
	}
    }
}


static bool
brep_seam_closure_tolerance(const struct rt_brep_trace_edge &edge,
    double &tolerance)
{
    if (!ON_IsValid(edge.model_tolerance) || edge.model_tolerance < 0.0)
	return false;
    tolerance = edge.model_tolerance;
    if (ON_IsValid(edge.declared_tolerance) &&
	edge.declared_tolerance >= 0.0)
	tolerance = std::max(tolerance, (double)edge.declared_tolerance);
    return std::isfinite(tolerance) && tolerance >= 0.0;
}


static void
brep_classify_closure(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const brep_hit *hit)
{
    if (!trace || !bs || !bs->is_solid || bs->plate_mode ||
	    !hit)
	return;
    for (size_t edge_index = 0; edge_index < trace->stored_edges;
	    ++edge_index) {
	const struct rt_brep_trace_edge &edge = trace->edges[edge_index];
	const double closure_tolerance = edge.model_tolerance;
	const bool closure_tolerance_valid =
	    ON_IsValid(closure_tolerance) && closure_tolerance >= 0.0;
	const double roundoff = std::max(ON_ZERO_TOLERANCE,
	    128.0 * DBL_EPSILON * std::max(1.0,
	    edge.measured_discrepancy));
	if (!edge.within_edge_tolerance || !edge.sector_valid ||
		edge.closest_state != 1 || !edge.discrepancy_measured ||
		!closure_tolerance_valid ||
		edge.measured_discrepancy > closure_tolerance + roundoff ||
		std::max(edge.lift_distance[0], edge.lift_distance[1]) >
		closure_tolerance + roundoff ||
		(hit->face->m_face_index != edge.face_index[0] &&
		 hit->face->m_face_index != edge.face_index[1]))
	    continue;
	const bool ordered = hit->direction == brep_hit::ENTERING ?
	    edge.ray_dist > hit->dist + BREP_SAME_POINT_TOLERANCE :
	    edge.ray_dist < hit->dist - BREP_SAME_POINT_TOLERANCE;
	if (!ordered)
	    continue;
	trace->closure_candidates++;
	if (trace->closure_edge_index >= 0)
	    continue;
	trace->closure_edge_dist = edge.ray_dist;
	trace->closure_existing_dist = hit->dist;
	trace->closure_edge_index = edge.edge_index;
	trace->closure_missing_direction = hit->direction == brep_hit::ENTERING ?
	    brep_hit::LEAVING : brep_hit::ENTERING;
    }
}


/* This classifier supplies a provisional seed only to the prepared contact
 * transaction.  Unlike ordinary and legacy closure, it may use a valid
 * tolerance explicitly declared on the edge.  It must never be applied to a
 * user-visible legacy trace or used directly to publish a boundary event;
 * its caller must subsequently prove the exact one-hit/one-miss contact
 * pattern and the complete box/material transaction. */
static void
brep_classify_declared_contact_closure(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const brep_hit *hit)
{
    if (!trace || !bs || !bs->is_solid || bs->plate_mode || !hit)
	return;
    for (size_t edge_index = 0; edge_index < trace->stored_edges;
	    ++edge_index) {
	const struct rt_brep_trace_edge &edge = trace->edges[edge_index];
	double closure_tolerance = 0.0;
	const bool explicitly_expanded = !edge.tolerance_inferred &&
	    ON_IsValid(edge.model_tolerance) && edge.model_tolerance >= 0.0 &&
	    ON_IsValid(edge.declared_tolerance) &&
	    edge.declared_tolerance > edge.model_tolerance &&
	    brep_seam_closure_tolerance(edge, closure_tolerance);
	const double roundoff = std::max(ON_ZERO_TOLERANCE,
	    128.0 * DBL_EPSILON * std::max(1.0,
	    edge.measured_discrepancy));
	if (!edge.within_edge_tolerance || !edge.sector_valid ||
		edge.closest_state != 1 || !edge.discrepancy_measured ||
		!explicitly_expanded ||
		edge.measured_discrepancy > closure_tolerance + roundoff ||
		std::max(edge.lift_distance[0], edge.lift_distance[1]) >
		closure_tolerance + roundoff ||
		(hit->face->m_face_index != edge.face_index[0] &&
		 hit->face->m_face_index != edge.face_index[1]))
	    continue;
	const bool ordered = hit->direction == brep_hit::ENTERING ?
	    edge.ray_dist > hit->dist + BREP_SAME_POINT_TOLERANCE :
	    edge.ray_dist < hit->dist - BREP_SAME_POINT_TOLERANCE;
	if (!ordered)
	    continue;
	trace->closure_candidates++;
	if (trace->closure_edge_index >= 0)
	    continue;
	trace->closure_edge_dist = edge.ray_dist;
	trace->closure_existing_dist = hit->dist;
	trace->closure_edge_index = edge.edge_index;
	trace->closure_missing_direction =
	    hit->direction == brep_hit::ENTERING ?
	    brep_hit::LEAVING : brep_hit::ENTERING;
    }
}


static bool
brep_bezier_surface_derivatives(const ON_BezierSurface &surface,
    const ON_2dPoint &uv, ON_3dPoint &point, ON_3dVector &du,
    ON_3dVector &dv)
{
    double values[9] = {0.0};
    if (!surface.Evaluate(uv.x, uv.y, 1, 3, values))
	return false;
    point.Set(values[0], values[1], values[2]);
    du.Set(values[3], values[4], values[5]);
    dv.Set(values[6], values[7], values[8]);
    return point.IsValid() && du.IsValid() && dv.IsValid();
}


static brep_continuation_result
brep_continuation_newton(const brep_surface_span &span, const ON_Ray &ray,
    const ON_2dPoint &seed, const double minimum[2],
    const double maximum[2])
{
    brep_continuation_result result;
    result.uv = seed;
    ON_3dVector first;
    ON_3dVector second;
    if (!brep_ray_plane_frame(ray, first, second) ||
	    !std::isfinite(minimum[0]) || !std::isfinite(minimum[1]) ||
	    !std::isfinite(maximum[0]) || !std::isfinite(maximum[1]) ||
	    !(minimum[0] < maximum[0]) || !(minimum[1] < maximum[1]) ||
	    seed.x < minimum[0] || seed.x > maximum[0] ||
	    seed.y < minimum[1] || seed.y > maximum[1])
	return result;
    const double span_scale = span.bbox.IsValid() ?
	span.bbox.Diagonal().Length() : 0.0;
    if (!(span_scale > DBL_MIN) || !std::isfinite(span_scale))
	return result;
    /* This solver is used on prepared patches ranging over many model scales.
     * Tie the ordinary along-ray target to the local support size.  The
     * separate evaluation floor below accounts for world-coordinate
     * cancellation and is intentionally not a modeling/seam tolerance. */
    const double root_tolerance = BREP_DIRECT_ROOT_RELATIVE_TOLERANCE *
	span_scale;
    result.status = BREP_CORRECTOR_ITERATION_LIMIT;
    for (size_t iteration = 0; iteration < 24; ++iteration) {
	ON_3dVector derivative_u;
	ON_3dVector derivative_v;
	if (!brep_bezier_surface_derivatives(span.surface, result.uv,
		result.point, derivative_u, derivative_v)) {
	    result.status = BREP_CORRECTOR_EVALUATION_FAILED;
	    return result;
	}
	const ON_3dVector offset = result.point - ray.m_origin;
	const double f = offset * first;
	const double g = offset * second;
	result.residual = hypot(f, g);
	result.iterations = iteration + 1;
	result.normal = ON_CrossProduct(derivative_u, derivative_v);
	const double ray_length = ray.m_dir.Length();
	if (!std::isfinite(result.residual) ||
		!(ray_length > DBL_MIN) || !std::isfinite(ray_length)) {
	    result.status = BREP_CORRECTOR_NONFINITE;
	    return result;
	}
	if (!result.normal.Unitize()) {
	    result.status = BREP_CORRECTOR_DEGENERATE_NORMAL;
	    return result;
	}
	const double normal_dot = fabs(result.normal * ray.m_dir) / ray_length;
	const double coordinate_scale = std::max(span_scale,
	    std::max(fabs(ray.m_origin.x),
	    std::max(fabs(ray.m_origin.y),
	    std::max(fabs(ray.m_origin.z),
	    std::max(fabs(result.point.x),
	    std::max(fabs(result.point.y), fabs(result.point.z)))))));
	const double evaluation_floor = BREP_DIRECT_EVALUATION_ULPS *
	    DBL_EPSILON * coordinate_scale;
	result.acceptance_limit = std::max(evaluation_floor,
	    root_tolerance * normal_dot);
	if (result.residual <= result.acceptance_limit) {
	    result.dist = utah_calc_t(ray, result.point);
	    result.converged = std::isfinite(result.dist);
	    result.status = result.converged ? BREP_CORRECTOR_CONVERGED :
		BREP_CORRECTOR_NONFINITE;
	    return result;
	}

	const double j11 = derivative_u * first;
	const double j12 = derivative_v * first;
	const double j21 = derivative_u * second;
	const double j22 = derivative_v * second;
	const double determinant = j11 * j22 - j12 * j21;
	const double u_scale = hypot(j11, j21);
	const double v_scale = hypot(j12, j22);
	if (!std::isfinite(determinant) || !std::isfinite(u_scale) ||
		!std::isfinite(v_scale)) {
	    result.status = BREP_CORRECTOR_NONFINITE;
	    return result;
	}
	if (u_scale <= ON_ZERO_TOLERANCE ||
		v_scale <= ON_ZERO_TOLERANCE ||
		fabs(determinant) <=
		BREP_INTERSECTION_ROOT_EPSILON * u_scale * v_scale) {
	    result.status = BREP_CORRECTOR_JACOBIAN_SINGULAR;
	    return result;
	}
	const ON_2dVector step((j22 * f - j12 * g) / determinant,
	    (j11 * g - j21 * f) / determinant);
	if (!step.IsValid()) {
	    result.status = BREP_CORRECTOR_NONFINITE;
	    return result;
	}

	bool improved = false;
	double fraction = 1.0;
	for (int line_search = 0; line_search < 8; ++line_search) {
	    const ON_2dPoint candidate(
		std::max(minimum[0], std::min(maximum[0],
		result.uv.x - fraction * step.x)),
		std::max(minimum[1], std::min(maximum[1],
		result.uv.y - fraction * step.y)));
	    ON_3dPoint candidate_point;
	    ON_3dVector candidate_u;
	    ON_3dVector candidate_v;
	    if (!brep_bezier_surface_derivatives(span.surface, candidate,
		    candidate_point, candidate_u, candidate_v)) {
		fraction *= 0.5;
		continue;
	    }
	    const ON_3dVector candidate_offset = candidate_point - ray.m_origin;
	    const double candidate_residual = hypot(candidate_offset * first,
		candidate_offset * second);
	    if (std::isfinite(candidate_residual) &&
		    candidate_residual < result.residual) {
		result.uv = candidate;
		improved = true;
		break;
	    }
	    fraction *= 0.5;
	}
	if (!improved) {
	    result.status = BREP_CORRECTOR_NO_IMPROVEMENT;
	    return result;
	}
    }
    return result;
}


static void
brep_resolve_continuation(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const brep_hit *hit, brep_hit *repaired_hit)
{
    if (!trace || !bs || !bs->brep || trace->closure_candidates != 1 ||
	    trace->closure_edge_index < 0 || !hit)
	return;
    const ON_Brep &brep = *bs->brep;
    if (trace->closure_edge_index >= brep.m_E.Count())
	return;
    const ON_BrepEdge &edge = brep.m_E[trace->closure_edge_index];
    if (edge.m_ti.Count() != 2)
	return;

    const ON_BrepTrim *trim = NULL;
    for (int side = 0; side < 2; ++side) {
	const int trim_index = edge.m_ti[side];
	if (trim_index >= 0 && trim_index < brep.m_T.Count() &&
		brep.m_T[trim_index].FaceIndexOf() == hit->face->m_face_index) {
	    trim = &brep.m_T[trim_index];
	    break;
	}
    }
    const struct rt_brep_trace_edge *observation = NULL;
    for (size_t edge_index = 0; edge_index < trace->stored_edges;
	    ++edge_index) {
	if (trace->edges[edge_index].edge_index ==
		trace->closure_edge_index) {
	    observation = &trace->edges[edge_index];
	    break;
	}
    }
    if (!trim || !observation)
	return;

    double trim_parameter = 0.0;
    if (!brep_edge_trim_parameter(edge, *trim,
	    observation->edge_parameter, trim_parameter))
	return;
    ON_3dPoint edge_uv3;
    ON_3dVector trim_derivative3;
    if (!trim->Ev1Der(trim_parameter, edge_uv3,
	    trim_derivative3) || !edge_uv3.IsValid() ||
	    !trim_derivative3.IsValid())
	return;
    const ON_2dPoint edge_uv(edge_uv3.x, edge_uv3.y);
    const ON_2dPoint hit_uv(hit->uv[0], hit->uv[1]);
    brep_hit continuation_hit;
    bool have_continuation_hit = false;

    for (std::vector<brep_face_record>::const_iterator face_it =
	    bs->face_records.begin(); face_it != bs->face_records.end();
	    ++face_it) {
	const brep_face_record &face_record = *face_it;
	if (!face_record.supported ||
		face_record.face_index != hit->face->m_face_index)
	    continue;
	for (size_t span_index = face_record.span_begin;
		span_index < face_record.span_begin + face_record.span_count;
		++span_index) {
	    const brep_surface_span &span = bs->surface_spans[span_index];
	    const double u_length = span.surface_domain[0].Length();
	    const double v_length = span.surface_domain[1].Length();
	    if (!(u_length > 0.0) || !(v_length > 0.0))
		continue;
	    const ON_2dPoint local_edge(
		span.surface_domain[0].NormalizedParameterAt(edge_uv.x),
		span.surface_domain[1].NormalizedParameterAt(edge_uv.y));
	    const ON_2dPoint local_hit(
		span.surface_domain[0].NormalizedParameterAt(hit_uv.x),
		span.surface_domain[1].NormalizedParameterAt(hit_uv.y));
	    const double parameter_epsilon = 128.0 * DBL_EPSILON;
	    if (local_edge.x < -parameter_epsilon ||
		    local_edge.x > 1.0 + parameter_epsilon ||
		    local_edge.y < -parameter_epsilon ||
		    local_edge.y > 1.0 + parameter_epsilon ||
		    local_hit.x < -parameter_epsilon ||
		    local_hit.x > 1.0 + parameter_epsilon ||
		    local_hit.y < -parameter_epsilon ||
		    local_hit.y > 1.0 + parameter_epsilon)
		continue;

	    ON_2dVector local_tangent(trim_derivative3.x / u_length,
		trim_derivative3.y / v_length);
	    if (!local_tangent.Unitize())
		continue;
	    const ON_2dVector from_edge = local_hit - local_edge;
	    const ON_2dPoint seed = local_edge +
		2.0 * (from_edge * local_tangent) * local_tangent - from_edge;
	    const double outside = std::max(
		std::max(0.0, std::max(-seed.x, seed.x - 1.0)),
		std::max(0.0, std::max(-seed.y, seed.y - 1.0)));
	    if (!(outside > parameter_epsilon) || outside > 0.25)
		continue;
	    const double extension = std::min(0.25,
		std::max(1.0e-6, 4.0 * outside));
	    const double solve_minimum[2] = {-extension, -extension};
	    const double solve_maximum[2] = {1.0 + extension,
		1.0 + extension};
	    trace->continuation_attempts++;
	    brep_continuation_result result = brep_continuation_newton(span,
		ray, seed, solve_minimum, solve_maximum);
	    if (!result.converged)
		continue;
	    ON_3dVector oriented_normal = result.normal;
	    if (hit->face->m_bRev)
		oriented_normal.Reverse();
	    const double normal_dot = oriented_normal * ray.m_dir;
	    const int direction = normal_dot < 0.0 ? brep_hit::ENTERING :
		brep_hit::LEAVING;
	    const bool ordered = direction == brep_hit::LEAVING ?
		result.dist > observation->ray_dist :
		result.dist < observation->ray_dist;
	    if (direction != trace->closure_missing_direction || !ordered)
		continue;
	    const ON_2dPoint root_uv(
		span.surface_domain[0].ParameterAt(result.uv.x),
		span.surface_domain[1].ParameterAt(result.uv.y));
	    const double padding = std::max(1.0 / 4096.0, 0.25 * outside);
	    double certificate_minimum[2] = {
		std::min(local_edge.x, std::min(seed.x, result.uv.x)) - padding,
		std::min(local_edge.y, std::min(seed.y, result.uv.y)) - padding
	    };
	    double certificate_maximum[2] = {
		std::max(local_edge.x, std::max(seed.x, result.uv.x)) + padding,
		std::max(local_edge.y, std::max(seed.y, result.uv.y)) + padding
	    };
	    for (int parameter_direction = 0; parameter_direction < 2;
		    ++parameter_direction) {
		certificate_minimum[parameter_direction] = std::max(-0.25,
		    certificate_minimum[parameter_direction]);
		certificate_maximum[parameter_direction] = std::min(1.25,
		    certificate_maximum[parameter_direction]);
	    }
	    trace->continuation_candidates++;
	    if (brep_continuation_certificate(trace, span, ray,
		    certificate_minimum, certificate_maximum, root_uv,
		    result.dist, hit->dist))
		trace->continuation_certified_candidates++;
	    if (trace->continuation_face_index >= 0)
		continue;
	    trace->continuation_iterations = result.iterations;
	    trace->continuation_dist = result.dist;
	    trace->continuation_uv[0] = root_uv.x;
	    trace->continuation_uv[1] = root_uv.y;
	    trace->continuation_residual = result.residual;
	    trace->continuation_normal_dot = normal_dot;
	    trace->continuation_face_index = hit->face->m_face_index;
	    trace->continuation_span_index = span.span_index;
	    vect_t point;
	    vect_t normal;
	    pt2d_t uv = {root_uv.x, root_uv.y};
	    const ON_3dPoint ray_point = ray.m_origin +
		(ray.m_dir * result.dist);
	    VMOVE(point, ray_point);
	    VMOVE(normal, oriented_normal);
	    continuation_hit = brep_hit(*hit->face, result.dist, ray, point,
		normal, uv);
	    continuation_hit.closeToEdge = true;
	    continuation_hit.trimmed = true;
	    continuation_hit.hit = brep_hit::CRACK_HIT;
	    continuation_hit.direction =
		(brep_hit::hit_direction)direction;
	    continuation_hit.sbv = hit->sbv;
	    continuation_hit.m_adj_face_index =
		edge.m_ti[0] >= 0 && edge.m_ti[0] < brep.m_T.Count() &&
		brep.m_T[edge.m_ti[0]].FaceIndexOf() !=
		hit->face->m_face_index ?
		brep.m_T[edge.m_ti[0]].FaceIndexOf() :
		(edge.m_ti[1] >= 0 && edge.m_ti[1] < brep.m_T.Count() ?
		brep.m_T[edge.m_ti[1]].FaceIndexOf() : -99);
	    trace->continuation_adjacent_face_index =
		continuation_hit.m_adj_face_index;
	    have_continuation_hit = true;
	}
    }
    if (trace->continuation_candidates == 1 &&
	    trace->continuation_certified_candidates == 1 &&
	    trace->continuation_face_index >= 0) {
	const double ray_length = ray.m_dir.Length();
	if (!(ray_length > DBL_MIN) || !std::isfinite(ray_length))
	    return;
	const double minimum_t = std::max(0.0,
	    (double)observation->model_tolerance) / ray_length;
	if (hit->direction == brep_hit::ENTERING &&
		trace->closure_missing_direction == brep_hit::LEAVING &&
		trace->continuation_dist - hit->dist >= minimum_t) {
	    trace->closure_shadow_segments = 1;
	    trace->closure_shadow_in_dist = hit->dist;
	    trace->closure_shadow_out_dist = trace->continuation_dist;
	} else if (trace->closure_missing_direction == brep_hit::ENTERING &&
		hit->direction == brep_hit::LEAVING &&
		hit->dist - trace->continuation_dist >= minimum_t) {
	    trace->closure_shadow_segments = 1;
	    trace->closure_shadow_in_dist = trace->continuation_dist;
	    trace->closure_shadow_out_dist = hit->dist;
	}
    }
    if (repaired_hit && trace->closure_shadow_segments == 1 &&
	    have_continuation_hit)
	*repaired_hit = continuation_hit;
}


static void
brep_trace_isolated_roots(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray)
{
    if (!trace || !bs || !bs->brep)
	return;
    for (size_t box_index = 0; box_index < trace->stored_surface_boxes;
	    ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	if (box.span_index < 0 ||
		(size_t)box.span_index >= bs->surface_spans.size()) {
	    trace->local_root_failures++;
	    continue;
	}
	const brep_surface_span &span = bs->surface_spans[box.span_index];
	if (span.face_index != box.face_index)
	    continue;
	double minimum[2] = {
	    span.surface_domain[0].NormalizedParameterAt(box.uv_min[0]),
	    span.surface_domain[1].NormalizedParameterAt(box.uv_min[1])
	};
	double maximum[2] = {
	    span.surface_domain[0].NormalizedParameterAt(box.uv_max[0]),
	    span.surface_domain[1].NormalizedParameterAt(box.uv_max[1])
	};
	const double span_scale = span.bbox.IsValid() ?
	    span.bbox.Diagonal().Length() : 0.0;
	const double seed_fraction[9][2] = {
	    {0.5, 0.5},
	    {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0},
	    {0.5, 0.0}, {0.5, 1.0}, {0.0, 0.5}, {1.0, 0.5}
	};
	for (size_t seed_index = 0; seed_index < 9; ++seed_index) {
	    const ON_2dPoint seed(
		minimum[0] + seed_fraction[seed_index][0] *
		(maximum[0] - minimum[0]),
		minimum[1] + seed_fraction[seed_index][1] *
		(maximum[1] - minimum[1]));
	    trace->local_root_attempts++;
	    brep_continuation_result result = brep_continuation_newton(span,
		ray, seed, minimum, maximum);
	    if ((size_t)result.status < RT_BREP_TRACE_CORRECTOR_STATUS_COUNT)
		trace->local_corrector_status[result.status]++;
	    if (!result.converged && result.acceptance_limit > 0.0 &&
		    std::isfinite(result.acceptance_limit) &&
		    std::isfinite(result.residual)) {
		const double ratio = result.residual / result.acceptance_limit;
		if (!trace->local_corrector_failure_ratios)
		    trace->local_corrector_min_failure_ratio = ratio;
		else
		    trace->local_corrector_min_failure_ratio = std::min(
			(double)trace->local_corrector_min_failure_ratio, ratio);
		trace->local_corrector_max_failure_ratio = std::max(
		    (double)trace->local_corrector_max_failure_ratio, ratio);
		trace->local_corrector_failure_ratios++;
	    }
	    const double coordinate_scale = std::max(span_scale,
		std::max(fabs(ray.m_origin.x),
		std::max(fabs(ray.m_origin.y),
		std::max(fabs(ray.m_origin.z),
		std::max(fabs(result.point.x),
		std::max(fabs(result.point.y), fabs(result.point.z)))))));
	    const double distance_tolerance = std::max(
		BREP_DIRECT_ROOT_RELATIVE_TOLERANCE * span_scale,
		BREP_DIRECT_EVALUATION_ULPS * DBL_EPSILON * coordinate_scale);
	    if (!result.converged ||
		    !std::isfinite(distance_tolerance) ||
		    result.dist < box.t_min - distance_tolerance ||
		    result.dist > box.t_max + distance_tolerance) {
		trace->local_root_failures++;
		continue;
	    }
	    const double nurbs_uv[2] = {
		span.surface_domain[0].ParameterAt(result.uv.x),
		span.surface_domain[1].ParameterAt(result.uv.y)
	    };
	    ON_2dPoint surface_uv;
	    ON_3dPoint mapped_point;
	    ON_3dVector mapped_normal;
	    const ON_BrepFace *face = box.face_index >= 0 &&
		box.face_index < bs->brep->m_F.Count() ?
		&bs->brep->m_F[box.face_index] : NULL;
	    const ON_Surface *original_surface = face ? face->SurfaceOf() : NULL;
	    if (!brep_surface_parameter_from_nurbs(bs, box.face_index,
		    nurbs_uv, surface_uv) || !original_surface ||
		    !surface_EvNormal(original_surface, surface_uv.x, surface_uv.y,
			mapped_point, mapped_normal) || !mapped_normal.Unitize() ||
		    mapped_point.DistanceTo(result.point) >
			4.0 * distance_tolerance) {
		trace->local_trim_failures++;
		continue;
	    }

	    bool duplicate = false;
	    for (size_t root_index = 0;
		    root_index < trace->stored_local_roots; ++root_index) {
		const struct rt_brep_trace_local_root &root =
		    trace->local_roots[root_index];
		if (root.span_index == box.span_index &&
			fabs(root.dist - result.dist) <=
			4.0 * distance_tolerance) {
		    duplicate = true;
		    break;
		}
	    }
	    if (duplicate) {
		trace->local_root_duplicates++;
		continue;
	    }

	    trace->local_root_candidates++;
	    if (trace->stored_local_roots >= RT_BREP_TRACE_MAX_LOCAL_ROOTS) {
		trace->local_root_overflow++;
		continue;
	    }
	    struct rt_brep_trace_local_root &root =
		trace->local_roots[trace->stored_local_roots++];
	    root.dist = result.dist;
	    root.uv[0] = nurbs_uv[0];
	    root.uv[1] = nurbs_uv[1];
	    root.residual = result.residual;
	    ON_3dVector normal = mapped_normal;
	    if (face->m_bRev)
		normal.Reverse();
	    root.normal_dot = normal * ray.m_dir;
	    root.iterations = result.iterations;
	    root.face_index = box.face_index;
	    root.span_index = box.span_index;
	    root.trim_distance = -1.0;
	    root.adjacent_face_index = -99;
	    root.trim_status = 1;
	    root.hit_class = brep_hit::CLEAN_MISS;
	    root.direction = root.normal_dot < 0.0 ? brep_hit::ENTERING :
		brep_hit::LEAVING;
	    if (box.face_index < 0 ||
		    (size_t)box.face_index >= bs->ctrees.size() ||
		    !bs->ctrees[box.face_index]) {
		trace->local_trim_failures++;
		continue;
	    }
	    const BRNode *trim_node = NULL;
	    size_t trim_candidates = 0;
	    trace->local_trim_queries++;
	    root.trim_status = bs->ctrees[box.face_index]->isTrimmed(
		surface_uv, &trim_node,
		root.trim_distance, BREP_EDGE_MISS_TOLERANCE,
		&trim_candidates);
	    trace->local_trim_candidates += trim_candidates;
	    root.adjacent_face_index = trim_node ?
		trim_node->m_adj_face_index : -99;
	    root.hit_class = brep_initial_hit_class(root.trim_status,
		root.trim_distance);
	}
    }
}


static void
brep_trace_local_clusters(struct rt_brep_shot_trace *trace,
    const struct bn_tol *tol)
{
    if (!trace)
	return;
    trace->local_cluster_tolerance = tol && tol->dist > 0.0 &&
	std::isfinite(tol->dist) ? 0.1 * tol->dist : 0.0;

    size_t order[RT_BREP_TRACE_MAX_LOCAL_ROOTS];
    for (size_t i = 0; i < trace->stored_local_roots; ++i) {
	order[i] = i;
	for (size_t j = i; j > 0 &&
		trace->local_roots[order[j]].dist <
		trace->local_roots[order[j - 1]].dist; --j)
	    std::swap(order[j], order[j - 1]);
    }

    for (size_t order_index = 0;
	    order_index < trace->stored_local_roots; ++order_index) {
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[order[order_index]];
	struct rt_brep_trace_local_cluster *cluster = NULL;
	for (size_t cluster_index = 0;
		cluster_index < trace->stored_local_clusters; ++cluster_index) {
	    struct rt_brep_trace_local_cluster &candidate =
		trace->local_clusters[cluster_index];
	    if (candidate.face_index == root.face_index &&
		    root.dist >= candidate.dist_min -
		    trace->local_cluster_tolerance &&
		    root.dist <= candidate.dist_max +
		    trace->local_cluster_tolerance) {
		cluster = &candidate;
		break;
	    }
	}
	if (!cluster) {
	    trace->local_root_clusters++;
	    if (trace->stored_local_clusters >=
		    RT_BREP_TRACE_MAX_LOCAL_CLUSTERS) {
		trace->local_cluster_overflow++;
		continue;
	    }
	    cluster = &trace->local_clusters[trace->stored_local_clusters++];
	    *cluster = {};
	    cluster->dist_min = root.dist;
	    cluster->dist_max = root.dist;
	    cluster->normal_dot_min = root.normal_dot;
	    cluster->normal_dot_max = root.normal_dot;
	    cluster->face_index = root.face_index;
	}
	cluster->dist_min = std::min((double)cluster->dist_min,
	    (double)root.dist);
	cluster->dist_max = std::max((double)cluster->dist_max,
	    (double)root.dist);
	cluster->normal_dot_min = std::min((double)cluster->normal_dot_min,
	    (double)root.normal_dot);
	cluster->normal_dot_max = std::max((double)cluster->normal_dot_max,
	    (double)root.normal_dot);
	cluster->roots++;
	if (root.normal_dot < -BREP_GRAZING_DOT_TOL)
	    cluster->entering_roots++;
	else if (root.normal_dot > BREP_GRAZING_DOT_TOL)
	    cluster->leaving_roots++;
	else
	    cluster->tangent_roots++;
    }

    for (size_t i = 0; i < trace->stored_local_clusters; ++i) {
	struct rt_brep_trace_local_cluster &cluster = trace->local_clusters[i];
	if (cluster.tangent_roots ||
		(cluster.entering_roots && cluster.leaving_roots))
	    cluster.classification = RT_BREP_TRACE_LOCAL_CONTACT;
	else if (cluster.entering_roots)
	    cluster.classification = RT_BREP_TRACE_ENTERING;
	else if (cluster.leaving_roots)
	    cluster.classification = RT_BREP_TRACE_LEAVING;
	else
	    cluster.classification = RT_BREP_TRACE_LOCAL_UNRESOLVED;
    }

    for (size_t i = 1; i < trace->stored_local_clusters; ++i) {
	struct rt_brep_trace_local_cluster value = trace->local_clusters[i];
	size_t j = i;
	while (j > 0 && value.dist_min <
		trace->local_clusters[j - 1].dist_min) {
	    trace->local_clusters[j] = trace->local_clusters[j - 1];
	    --j;
	}
	trace->local_clusters[j] = value;
    }
}


struct brep_trace_event_group {
    double dist_min;
    double dist_max;
    double closest_trim[2];
    double event_dist[2];
    double event_uv[2][2];
    double event_normal_dot[2];
    double event_trim_distance[2];
    int event_adjacency[2];
    bool present[2];
    bool entering[2];
    bool leaving[2];
    bool tangent[2];
    int face_index;
    int trim_status;
    int hit_class;
    int direction_class;
    int adjacency;
    double dist;
    double uv[2];
    double normal_dot;
    double trim_distance;
};


template <typename RootType>
static size_t
brep_trace_event_groups(const RootType *roots, size_t root_count,
    size_t *order, brep_trace_event_group *groups, double tolerance)
{
    for (size_t i = 0; i < root_count; ++i) {
	order[i] = i;
	for (size_t j = i; j > 0; --j) {
	    const RootType &left = roots[order[j - 1]];
	    const RootType &right = roots[order[j]];
	    if (left.face_index < right.face_index ||
		    (left.face_index == right.face_index &&
		    left.dist <= right.dist))
		break;
	    std::swap(order[j - 1], order[j]);
	}
    }

    size_t group_count = 0;
    for (size_t i = 0; i < root_count; ++i) {
	const RootType &root = roots[order[i]];
	brep_trace_event_group *group = group_count ?
	    &groups[group_count - 1] : NULL;
	if (!group || group->face_index != root.face_index ||
		root.dist - group->dist_min > tolerance) {
	    group = &groups[group_count++];
	    *group = {};
	    group->face_index = root.face_index;
	    group->dist_min = root.dist;
	    group->dist_max = root.dist;
	    group->closest_trim[0] = DBL_MAX;
	    group->closest_trim[1] = DBL_MAX;
	    group->event_adjacency[0] = -99;
	    group->event_adjacency[1] = -99;
	}
	group->dist_max = std::max(group->dist_max, (double)root.dist);
	const int state = root.trim_status == 1 ? 1 : 0;
	const double trim_distance = fabs(root.trim_distance);
	if (!group->present[state] ||
		trim_distance < group->closest_trim[state]) {
	    group->present[state] = true;
	    group->closest_trim[state] = trim_distance;
	    group->event_dist[state] = root.dist;
	    group->event_uv[state][0] = root.uv[0];
	    group->event_uv[state][1] = root.uv[1];
	    group->event_normal_dot[state] = root.normal_dot;
	    group->event_trim_distance[state] = root.trim_distance;
	    group->event_adjacency[state] = root.adjacent_face_index;
	}
	if (root.normal_dot < -BREP_GRAZING_DOT_TOL)
	    group->entering[state] = true;
	else if (root.normal_dot > BREP_GRAZING_DOT_TOL)
	    group->leaving[state] = true;
	else
	    group->tangent[state] = true;
    }

    for (size_t i = 0; i < group_count; ++i) {
	brep_trace_event_group &group = groups[i];
	const int state = group.present[0] ? 0 : 1;
	group.trim_status = state;
	const bool near_trim = group.closest_trim[state] <
	    BREP_EDGE_MISS_TOLERANCE;
	group.hit_class = state == 0 ?
	    (near_trim ? brep_hit::NEAR_HIT : brep_hit::CLEAN_HIT) :
	    (near_trim ? brep_hit::NEAR_MISS : brep_hit::CLEAN_MISS);
	if (group.tangent[state] ||
		(group.entering[state] && group.leaving[state]))
	    group.direction_class = RT_BREP_TRACE_LOCAL_CONTACT;
	else if (group.entering[state])
	    group.direction_class = brep_hit::ENTERING;
	else if (group.leaving[state])
	    group.direction_class = brep_hit::LEAVING;
	else
	    group.direction_class = RT_BREP_TRACE_LOCAL_UNRESOLVED;
	group.adjacency = group.event_adjacency[state];
	group.dist = group.event_dist[state];
	group.uv[0] = group.event_uv[state][0];
	group.uv[1] = group.event_uv[state][1];
	group.normal_dot = group.event_normal_dot[state];
	group.trim_distance = group.event_trim_distance[state];
    }
    return group_count;
}


static void
brep_trace_root_coverage(struct rt_brep_shot_trace *trace)
{
    if (!trace)
	return;
    const double tolerance = trace->local_cluster_tolerance > 0.0 &&
	std::isfinite(trace->local_cluster_tolerance) ?
	trace->local_cluster_tolerance : BREP_SAME_POINT_TOLERANCE;
    size_t legacy_order[RT_BREP_TRACE_MAX_ROOTS];
    size_t local_order[RT_BREP_TRACE_MAX_LOCAL_ROOTS];
    brep_trace_event_group legacy_groups[RT_BREP_TRACE_MAX_ROOTS];
    brep_trace_event_group local_groups[RT_BREP_TRACE_MAX_LOCAL_ROOTS];
    const size_t legacy_count = brep_trace_event_groups(trace->roots,
	trace->stored_roots, legacy_order, legacy_groups, tolerance);
    const size_t local_count = brep_trace_event_groups(trace->local_roots,
	trace->stored_local_roots, local_order, local_groups, tolerance);

    size_t legacy_index = 0;
    size_t local_index = 0;
    size_t matched = 0;
    while (legacy_index < legacy_count && local_index < local_count) {
	const brep_trace_event_group &legacy = legacy_groups[legacy_index];
	const brep_trace_event_group &local = local_groups[local_index];
	if (legacy.face_index < local.face_index) {
	    legacy_index++;
	    continue;
	}
	if (legacy.face_index > local.face_index) {
	    local_index++;
	    continue;
	}
	if (legacy.dist_max < local.dist_min - tolerance) {
	    legacy_index++;
	    continue;
	}
	if (local.dist_max < legacy.dist_min - tolerance) {
	    local_index++;
	    continue;
	}
	trace->matched_root_events++;
	trace->root_match_max_t_error = std::max(
	    (double)trace->root_match_max_t_error,
	    fabs(legacy.dist - local.dist));
	trace->root_match_max_uv_error = std::max(
	    (double)trace->root_match_max_uv_error,
	    hypot(legacy.uv[0] - local.uv[0],
		legacy.uv[1] - local.uv[1]));
	trace->root_match_max_normal_dot_error = std::max(
	    (double)trace->root_match_max_normal_dot_error,
	    fabs(legacy.normal_dot - local.normal_dot));
	bool event_mismatch = false;
	if (legacy.trim_status != local.trim_status) {
	    trace->root_trim_status_mismatches++;
	    event_mismatch = true;
	}
	if (legacy.hit_class != local.hit_class) {
	    trace->root_hit_class_mismatches++;
	    event_mismatch = true;
	}
	if (legacy.direction_class != local.direction_class) {
	    trace->root_direction_mismatches++;
	    event_mismatch = true;
	}
	const bool near_trim = legacy.hit_class == brep_hit::NEAR_HIT ||
	    legacy.hit_class == brep_hit::NEAR_MISS ||
	    local.hit_class == brep_hit::NEAR_HIT ||
	    local.hit_class == brep_hit::NEAR_MISS;
	if (near_trim)
	    trace->root_match_max_trim_error = std::max(
		(double)trace->root_match_max_trim_error,
		fabs(legacy.trim_distance - local.trim_distance));
	if (near_trim && legacy.adjacency != local.adjacency) {
	    trace->root_adjacency_mismatches++;
	    event_mismatch = true;
	}
	if (event_mismatch)
	    trace->root_event_mismatches++;
	matched++;
	legacy_index++;
	local_index++;
    }
    trace->legacy_unique_roots = legacy_count;
    trace->legacy_unique_roots_matched = matched;
    trace->legacy_unique_roots_unmatched = legacy_count - matched;
    trace->local_unique_roots = local_count;
    trace->local_unique_roots_matched = matched;
    trace->local_unique_roots_unmatched = local_count - matched;
}


static int
utah_brep_intersect(const BBNode* sbv, const ON_BrepFace* face,
    const ON_Surface* surf, pt2d_t& uv, const ON_Ray& ray,
    std::list<brep_hit> *hits, brep_hit_workspace *fixed_hits,
    struct rt_brep_shot_trace *trace)
{
#define MAX_BREP_SUBDIVISION_INTERSECTS 5
    ON_3dVector N[MAX_BREP_SUBDIVISION_INTERSECTS];
    double t[MAX_BREP_SUBDIVISION_INTERSECTS];
    ON_2dPoint ouv[MAX_BREP_SUBDIVISION_INTERSECTS];
    int found = BREP_INTERSECT_ROOT_DIVERGED;
    int numhits;

    double grazing_float = sbv->m_normal * ray.m_dir;

    brep_solver_result solver_result;
    if (fabs(grazing_float) < 0.2) {
	solver_result = utah_newton_4corner_solver(sbv, surf, ray, ouv, t, N,
	    MAX_BREP_SUBDIVISION_INTERSECTS, 1);
    } else {
	solver_result = utah_newton_4corner_solver(sbv, surf, ray, ouv, t, N,
	    MAX_BREP_SUBDIVISION_INTERSECTS, 0);
    }
    numhits = solver_result.intersections;
    if (trace) {
	trace->solver_calls++;
	const size_t status = (size_t)solver_result.status;
	if (status < RT_BREP_TRACE_SOLVER_STATUS_COUNT)
	    trace->solver_status[status]++;
    }

    if (numhits > 0) {
	for (int i = 0; i < numhits; i++) {
	    double closesttrim;
	    const BRNode* trimBR = NULL;
	    size_t trim_candidates = 0;
	    int trim_status = sbv->isTrimmed(ouv[i], &trimBR, closesttrim,
		BREP_EDGE_MISS_TOLERANCE, &trim_candidates);
	    if (trace) {
		double allocating_closesttrim;
		const BRNode *allocating_trimBR = NULL;
		size_t allocating_candidates = 0;
		int allocating_status = sbv->isTrimmedAllocating(ouv[i],
		    &allocating_trimBR, allocating_closesttrim,
		    BREP_EDGE_MISS_TOLERANCE, &allocating_candidates);
		bool mismatch = false;
		trace->trim_queries++;
		trace->trim_noalloc_candidates += trim_candidates;
		trace->trim_allocating_candidates += allocating_candidates;
		if (trim_candidates != allocating_candidates) {
		    trace->trim_candidate_mismatches++;
		    mismatch = true;
		}
		if (trim_status != allocating_status) {
		    trace->trim_status_mismatches++;
		    mismatch = true;
		}
		if (trimBR != allocating_trimBR) {
		    trace->trim_closest_mismatches++;
		    mismatch = true;
		}
		if (std::memcmp(&closesttrim, &allocating_closesttrim,
			sizeof(closesttrim)) != 0) {
		    trace->trim_distance_mismatches++;
		    mismatch = true;
		}
		if (mismatch)
		    trace->trim_equivalence_mismatches++;
	    }
	    if (trace) {
		bool mismatch = false;
		if (!sbv->m_ctree) {
		    trace->face_trim_equivalence_mismatches++;
		} else {
		    const BRNode *face_trim_node = NULL;
		    double face_trim_distance = -1.0;
		    size_t face_trim_candidates = 0;
		    trace->face_trim_queries++;
		    const int face_trim_status = sbv->m_ctree->isTrimmed(
			ouv[i], &face_trim_node, face_trim_distance,
			BREP_EDGE_MISS_TOLERANCE, &face_trim_candidates);
		    trace->face_trim_candidates += face_trim_candidates;
		    const int leaf_hit_class = brep_initial_hit_class(
			trim_status, closesttrim);
		    const int face_hit_class = brep_initial_hit_class(
			face_trim_status, face_trim_distance);
		    if (trim_status != face_trim_status) {
			trace->face_trim_status_mismatches++;
			mismatch = true;
		    }
		    if (leaf_hit_class != face_hit_class) {
			trace->face_trim_hit_class_mismatches++;
			mismatch = true;
		    }
		    const bool near_trim =
			leaf_hit_class == brep_hit::NEAR_HIT ||
			leaf_hit_class == brep_hit::NEAR_MISS ||
			face_hit_class == brep_hit::NEAR_HIT ||
			face_hit_class == brep_hit::NEAR_MISS;
		    if (near_trim) {
			trace->face_trim_max_near_distance_error = std::max(
			    (double)trace->face_trim_max_near_distance_error,
			    fabs(closesttrim - face_trim_distance));
			const int leaf_adjacency = trimBR ?
			    trimBR->m_adj_face_index : -99;
			const int face_adjacency = face_trim_node ?
			    face_trim_node->m_adj_face_index : -99;
			if (leaf_adjacency != face_adjacency) {
			    trace->face_trim_adjacency_mismatches++;
			    mismatch = true;
			}
		    }
		    if (mismatch)
			trace->face_trim_equivalence_mismatches++;
		}
	    }
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
		brep_trace_root(trace, face, t[i], ouv[i], N[i], ray,
		    trim_status, closesttrim, trimBR, bh.hit);
		if (VDOT(ray.m_dir, vnorm) < 0.0)
		    bh.direction = brep_hit::ENTERING;
		else
		    bh.direction = brep_hit::LEAVING;
		bh.sbv = sbv;
		if (hits)
		    hits->push_back(bh);
		if (fixed_hits)
		    fixed_hits->push_back(bh);
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
		brep_trace_root(trace, face, t[i], ouv[i], N[i], ray,
		    trim_status, closesttrim, trimBR, bh.hit);
		if (VDOT(ray.m_dir, vnorm) < 0.0)
		    bh.direction = brep_hit::ENTERING;
		else
		    bh.direction = brep_hit::LEAVING;
		bh.sbv = sbv;
		if (hits)
		    hits->push_back(bh);
		if (fixed_hits)
		    fixed_hits->push_back(bh);
		found = BREP_INTERSECT_FOUND;
	    } else {
		brep_trace_root(trace, face, t[i], ouv[i], N[i], ray,
		    trim_status, closesttrim, trimBR, brep_hit::CLEAN_MISS);
	    }
	}
    }
    return found;
}


static void
collect_brep_hits(const BBNode *const *fixed_leaves, size_t fixed_leaf_count,
    const std::list<const BBNode *> &fallback_leaves,
    bool fixed_leaf_overflow, const ON_Ray &ray,
    std::list<brep_hit> *hits, brep_hit_workspace *fixed_hits,
    struct rt_brep_shot_trace *trace)
{
    if (fixed_leaf_overflow) {
	for (std::list<const BBNode *>::const_iterator i =
		fallback_leaves.begin(); i != fallback_leaves.end(); ++i) {
	    const BBNode *sbv = *i;
	    const ON_BrepFace *face = &sbv->get_face();
	    const ON_Surface *surface = face->SurfaceOf();
	    pt2d_t uv = {sbv->m_u.Mid(), sbv->m_v.Mid()};
	    utah_brep_intersect(sbv, face, surface, uv, ray, hits,
		fixed_hits, trace);
	}
	return;
    }

    for (size_t leaf_index = 0; leaf_index < fixed_leaf_count;
	    ++leaf_index) {
	const BBNode *sbv = fixed_leaves[leaf_index];
	const ON_BrepFace *face = &sbv->get_face();
	const ON_Surface *surface = face->SurfaceOf();
	pt2d_t uv = {sbv->m_u.Mid(), sbv->m_v.Mid()};
	utah_brep_intersect(sbv, face, surface, uv, ray, hits, fixed_hits,
	    trace);
    }
}


static int
sign(double val)
{
    return (val >= 0.0) ? 1 : -1;
}


struct brep_hit_cleanup_state {
    size_t after_near_miss;
    size_t after_near_hit;
    size_t after_grazing;
    size_t after_duplicates;
    size_t after_direction;
};


#define BREP_CLEANUP_STAGE_COUNT RT_BREP_TRACE_CLEANUP_STAGES

struct brep_cleanup_event {
    double dist_min;
    double dist_max;
    bool actual;
    bool near_actual;
    bool crack;
    bool entering;
    bool leaving;
    bool tangent;
};


struct brep_cleanup_signature {
    size_t event_count;
    brep_cleanup_event events[RT_BREP_MAX_HITS];
};


struct brep_cleanup_observation {
    brep_cleanup_signature stages[BREP_CLEANUP_STAGE_COUNT];
    double tolerance;
};


static void
observe_brep_cleanup(const brep_hit_workspace &hits,
	brep_cleanup_signature &signature, const struct xray &ray,
	double tolerance)
{
    signature = {};
    for (size_t hit_index = 0; hit_index < hits.size(); ++hit_index) {
	const brep_hit &hit = hits[hit_index];
	brep_cleanup_event *event = signature.event_count ?
	    &signature.events[signature.event_count - 1] : NULL;
	if (!event || hit.dist - event->dist_min > tolerance) {
	    event = &signature.events[signature.event_count++];
	    *event = {};
	    event->dist_min = hit.dist;
	    event->dist_max = hit.dist;
	}
	event->dist_max = std::max(event->dist_max, (double)hit.dist);
	if (hit.hit != brep_hit::NEAR_MISS) {
	    event->actual = true;
	    event->near_actual = event->near_actual || hit.closeToEdge;
	    event->crack = event->crack || hit.hit == brep_hit::CRACK_HIT;
	}
	const double normal_dot = VDOT(hit.normal, ray.r_dir);
	if (normal_dot < -BREP_GRAZING_DOT_TOL)
	    event->entering = true;
	else if (normal_dot > BREP_GRAZING_DOT_TOL)
	    event->leaving = true;
	else
	    event->tangent = true;
    }
}


static int
brep_cleanup_event_class(const brep_cleanup_event &event)
{
    if (!event.actual)
	return brep_hit::NEAR_MISS;
    if (event.crack)
	return brep_hit::CRACK_HIT;
    return event.near_actual ? brep_hit::NEAR_HIT : brep_hit::CLEAN_HIT;
}


static int
brep_cleanup_event_direction(const brep_cleanup_event &event)
{
    if (event.tangent || (event.entering && event.leaving))
	return RT_BREP_TRACE_LOCAL_CONTACT;
    if (event.entering)
	return brep_hit::ENTERING;
    if (event.leaving)
	return brep_hit::LEAVING;
    return RT_BREP_TRACE_LOCAL_UNRESOLVED;
}


static bool
brep_cleanup_signatures_match(const brep_cleanup_signature &legacy,
	const brep_cleanup_signature &local, double tolerance)
{
    if (legacy.event_count != local.event_count)
	return false;
    for (size_t event_index = 0; event_index < legacy.event_count;
	    ++event_index) {
	const brep_cleanup_event &first = legacy.events[event_index];
	const brep_cleanup_event &second = local.events[event_index];
	if (first.dist_max < second.dist_min - tolerance ||
		second.dist_max < first.dist_min - tolerance ||
		brep_cleanup_event_class(first) !=
		brep_cleanup_event_class(second) ||
		brep_cleanup_event_direction(first) !=
		brep_cleanup_event_direction(second))
	    return false;
    }
    return true;
}


static bool
fixed_hits_contain(const brep_hit_workspace &hits, brep_hit::hit_type type)
{
    for (size_t i = 0; i < hits.size(); ++i) {
	if (hits[i].hit == type)
	    return true;
    }
    return false;
}


static bool
brep_resolved_grazing_pair(const brep_hit &first, const brep_hit &second,
	const struct xray &ray, const struct bn_tol *tol)
{
    const double ray_length = MAGNITUDE(ray.r_dir);
    if (!(ray_length > DBL_MIN) || !std::isfinite(ray_length) ||
	    !std::isfinite(first.dist) || !std::isfinite(second.dist) ||
	    (first.trimmed && !first.closeToEdge) || first.oob ||
	    (second.trimmed && !second.closeToEdge) || second.oob ||
	    first.direction != brep_hit::ENTERING ||
	    second.direction != brep_hit::LEAVING)
	return false;
    const double minimum_segment = tol && tol->dist > 0.0 &&
	std::isfinite(tol->dist) ? tol->dist / ray_length :
	BREP_SAME_POINT_TOLERANCE;
    const double parameter_scale = std::max(1.0,
	std::max(fabs(first.dist), fabs(second.dist)));
    const double evaluation_slack = 128.0 * DBL_EPSILON * parameter_scale;
    return second.dist - first.dist >= minimum_segment - evaluation_slack;
}


static brep_hit_cleanup_state
cleanup_fixed_brep_hits(brep_hit_workspace &hits, const struct xray &ray,
	const struct bn_tol *tol,
	brep_cleanup_observation *observation = NULL)
{
    brep_hit_cleanup_state state = {};

    if (hits.size() > 1 &&
	    fixed_hits_contain(hits, brep_hit::NEAR_MISS)) {
	size_t curr = 0;
	while (curr < hits.size()) {
	    if (hits[curr].hit == brep_hit::NEAR_MISS) {
		if (curr > 0 &&
			hits[curr - 1].hit != brep_hit::NEAR_MISS &&
			hits[curr - 1].direction == hits[curr].direction) {
		    hits.erase(curr);
		    curr = 0;
		    continue;
		}
		if (curr + 1 < hits.size() &&
			hits[curr + 1].hit != brep_hit::NEAR_MISS &&
			hits[curr + 1].direction == hits[curr].direction) {
		    hits.erase(curr);
		    curr = 0;
		    continue;
		}
	    }
	    ++curr;
	}

	curr = 0;
	while (curr < hits.size()) {
	    if (curr > 0) {
		const size_t prev = curr - 1;
		if (hits[curr].hit == brep_hit::NEAR_MISS) {
		    if (hits[prev].hit == brep_hit::NEAR_MISS) {
			if (hits[prev].m_adj_face_index ==
				hits[curr].face->m_face_index) {
			    if (hits[prev].direction == hits[curr].direction) {
				hits[prev].hit = brep_hit::CRACK_HIT;
				hits.erase(curr);
				continue;
			    }
			    hits.erase(prev);
			    hits.erase(prev);
			    curr = prev;
			    continue;
			}
			hits.erase(prev);
			--curr;
		    }
		} else if ((hits[curr].hit == brep_hit::CLEAN_HIT ||
			hits[curr].hit == brep_hit::NEAR_HIT) &&
			hits[prev].hit == brep_hit::NEAR_MISS) {
		    if (hits[curr].direction == brep_hit::ENTERING) {
			hits.erase(prev);
			--curr;
		    } else {
			hits[prev].hit = brep_hit::CRACK_HIT;
		    }
		}
	    }
	    ++curr;
	}

	curr = 0;
	while (curr < hits.size()) {
	    if (hits[curr].hit == brep_hit::CLEAN_HIT && curr > 0 &&
		    hits[curr - 1].hit == brep_hit::CLEAN_HIT &&
		    hits[curr - 1].direction == hits[curr].direction &&
		    hits[curr - 1].face->m_face_index ==
		    hits[curr].m_adj_face_index) {
		const brep_hit::hit_direction first_direction =
		    hits.front().direction;
		if (first_direction == hits[curr].direction) {
		    hits.erase(curr - 1);
		    --curr;
		} else {
		    hits.erase(curr);
		}
		continue;
	    }
	    ++curr;
	}

	if (!hits.empty() && hits.size() % 2 != 0 &&
		hits.back().hit == brep_hit::NEAR_MISS)
	    hits.pop_back();
	if (!hits.empty() && hits.size() % 2 != 0 &&
		hits.front().hit == brep_hit::NEAR_MISS)
	    hits.pop_front();
    }
    state.after_near_miss = hits.size();
    if (observation)
	observe_brep_cleanup(hits, observation->stages[0], ray,
	    observation->tolerance);

    if (hits.size() > 1 && fixed_hits_contain(hits, brep_hit::NEAR_HIT)) {
	size_t curr = 0;
	while (curr < hits.size()) {
	    if (hits[curr].hit == brep_hit::NEAR_HIT) {
		if (curr > 0 &&
			hits[curr - 1].hit != brep_hit::NEAR_HIT &&
			hits[curr - 1].direction == hits[curr].direction) {
		    hits.erase(curr);
		    continue;
		}
		if (curr + 1 < hits.size() &&
			hits[curr + 1].hit != brep_hit::NEAR_HIT &&
			hits[curr + 1].direction == hits[curr].direction) {
		    hits.erase(curr);
		    continue;
		}
	    }
	    ++curr;
	}

	curr = 0;
	while (curr < hits.size()) {
	    if (curr > 0 && hits[curr].hit == brep_hit::NEAR_HIT &&
		    hits[curr - 1].hit == brep_hit::NEAR_HIT &&
		    hits[curr - 1].direction == hits[curr].direction) {
		hits[curr - 1].hit = brep_hit::CRACK_HIT;
		hits.erase(curr);
		continue;
	    }
	    ++curr;
	}
    }
    state.after_near_hit = hits.size();
    if (observation)
	observe_brep_cleanup(hits, observation->stages[1], ray,
	    observation->tolerance);

    if (!hits.empty()) {
	/* Angular grazing is not affine invariant.  A separated entering/leaving
	 * pair still represents material and is retained at model resolution. */
	/* Preserve the list loop's advancement after erasing its first node:
	 * the new first node is skipped, while later returned nodes are tested. */
	size_t i = 0;
	while (i < hits.size()) {
	    const brep_hit &hit = hits[i];
	    const bool invalid = (hit.trimmed && !hit.closeToEdge) || hit.oob;
	    const bool grazing = NEAR_ZERO(VDOT(hit.normal, ray.r_dir),
		BREP_GRAZING_DOT_TOL);
	    const bool resolved_before = i > 0 &&
		brep_resolved_grazing_pair(hits[i - 1], hit, ray, tol);
	    const bool resolved_after = i + 1 < hits.size() &&
		brep_resolved_grazing_pair(hit, hits[i + 1], ray, tol);
	    if (invalid || (grazing && !resolved_before && !resolved_after)) {
		hits.erase(i);
		if (!i) {
		    if (!hits.empty())
			++i;
		} else {
		    /* erase returned i; --i and the loop's ++i cancel. */
		}
		continue;
	    }
	    ++i;
	}
    }
    state.after_grazing = hits.size();
    if (observation)
	observe_brep_cleanup(hits, observation->stages[2], ray,
	    observation->tolerance);

    if (!hits.empty()) {
	size_t last = 0;
	size_t i = 1;
	while (i < hits.size()) {
	    if (hits[i] == hits[last]) {
		const double last_dot = VDOT(hits[last].normal, ray.r_dir);
		const double current_dot = VDOT(hits[i].normal, ray.r_dir);
		if (sign(last_dot) != sign(current_dot)) {
		    hits.erase(last);
		    hits.erase(last);
		    i = last;
		    if (i < hits.size())
			++i;
		} else {
		    hits.erase(i);
		}
	    } else {
		last = i;
		++i;
	    }
	}
    }
    state.after_duplicates = hits.size();
    if (observation)
	observe_brep_cleanup(hits, observation->stages[3], ray,
	    observation->tolerance);

    if (!hits.empty()) {
	size_t last = 0;
	size_t i = 1;
	int entering = 1;
	while (i < hits.size()) {
	    const double last_dot = VDOT(hits[last].normal, ray.r_dir);
	    const double current_dot = VDOT(hits[i].normal, ray.r_dir);
	    if (!i)
		entering = sign(current_dot);
	    if (sign(last_dot) == sign(current_dot)) {
		if (sign(current_dot) == entering) {
		    hits.erase(last);
		    i = last;
		    if (i < hits.size())
			++i;
		} else {
		    hits.erase(i);
		}
	    } else {
		last = i;
		++i;
	    }
	}
    }

    if (hits.size() > 1 && hits.size() % 2 != 0) {
	const double first_dot = VDOT(hits.front().normal, ray.r_dir);
	const double last_dot = VDOT(hits.back().normal, ray.r_dir);
	if (sign(first_dot) == sign(last_dot))
	    hits.pop_back();
    }
    state.after_direction = hits.size();
    if (observation)
	observe_brep_cleanup(hits, observation->stages[4], ray,
	    observation->tolerance);
    return state;
}


static bool
repair_fixed_brep_crack(brep_hit_workspace &hits,
	const struct brep_specific *bs, const ON_Ray &ray);


static bool
brep_trace_hits_equivalent(const brep_hit &legacy, const brep_hit &local,
	double tolerance, struct rt_brep_shot_trace *trace)
{
    const double t_error = fabs(legacy.dist - local.dist);
    trace->local_event_max_t_error = std::max(
	(double)trace->local_event_max_t_error, t_error);
    bool equivalent = true;
    if (t_error > tolerance) {
	trace->local_event_t_mismatches++;
	equivalent = false;
    }
    if (legacy.face->m_face_index != local.face->m_face_index) {
	trace->local_event_face_mismatches++;
	equivalent = false;
    }
    if (legacy.trimmed != local.trimmed) {
	trace->local_event_trim_mismatches++;
	equivalent = false;
    }
    if (legacy.closeToEdge != local.closeToEdge) {
	trace->local_event_edge_mismatches++;
	equivalent = false;
    }
    if (legacy.hit != local.hit) {
	trace->local_event_class_mismatches++;
	equivalent = false;
    }
    if (legacy.direction != local.direction) {
	trace->local_event_direction_mismatches++;
	equivalent = false;
    }
    if (legacy.closeToEdge &&
	    legacy.m_adj_face_index != local.m_adj_face_index) {
	trace->local_event_adjacency_mismatches++;
	equivalent = false;
    }
    return equivalent;
}


static bool
brep_trace_make_hit(const struct brep_specific *bs, const ON_Ray &ray,
	int face_index, double dist, const double uv[2], int trim_status,
	int hit_class, int direction, int adjacency, bool nurbs_parameter,
	brep_hit &hit)
{
    if (!bs || !bs->brep || face_index < 0 ||
	    face_index >= bs->brep->m_F.Count() ||
	    (direction != brep_hit::ENTERING &&
	     direction != brep_hit::LEAVING))
	return false;
    const ON_BrepFace &face = bs->brep->m_F[face_index];
    const ON_Surface *surface = face.SurfaceOf();
    ON_2dPoint surface_uv(uv[0], uv[1]);
    if (nurbs_parameter &&
	    !brep_surface_parameter_from_nurbs(bs, face_index, uv, surface_uv))
	return false;
    ON_3dPoint surface_point;
    ON_3dVector normal;
    if (!surface || !surface_EvNormal(surface, surface_uv.x, surface_uv.y,
	    surface_point, normal) || !normal.Unitize())
	return false;
    if (face.m_bRev)
	normal.Reverse();
    const double normal_dot = normal * ray.m_dir;
    const int evaluated_direction = normal_dot < 0.0 ?
	brep_hit::ENTERING : brep_hit::LEAVING;
    if (!std::isfinite(normal_dot) ||
	    (evaluated_direction != direction &&
	     fabs(normal_dot) > BREP_GRAZING_DOT_TOL))
	return false;

    point_t point;
    vect_t hit_normal;
    pt2d_t hit_uv = {surface_uv.x, surface_uv.y};
    const ON_3dPoint ray_point = ray.m_origin + ray.m_dir * dist;
    VMOVE(point, ray_point);
    VMOVE(hit_normal, normal);
    hit = brep_hit(face, dist, ray, point, hit_normal, hit_uv);
    hit.trimmed = trim_status == 1;
    hit.closeToEdge = hit_class == brep_hit::NEAR_HIT ||
	hit_class == brep_hit::NEAR_MISS || hit_class == brep_hit::CRACK_HIT;
    hit.hit = (brep_hit::hit_type)hit_class;
    hit.direction = (brep_hit::hit_direction)direction;
    hit.m_adj_face_index = adjacency;
    hit.sbv = NULL;
    return true;
}


static void
brep_trace_prepared_event_cleanup(struct rt_brep_shot_trace *trace,
	const struct brep_specific *bs, const ON_Ray &ray,
	const struct xray &xray, const struct bn_tol *tol,
	const std::list<brep_hit> &legacy_hits,
	const brep_hit *legacy_repaired_hit)
{
    if (!trace || !bs || !bs->brep)
	return;

    size_t order[RT_BREP_TRACE_MAX_LOCAL_ROOTS];
    brep_trace_event_group groups[RT_BREP_TRACE_MAX_LOCAL_ROOTS];
    const double tolerance = trace->local_cluster_tolerance > 0.0 &&
	std::isfinite(trace->local_cluster_tolerance) ?
	trace->local_cluster_tolerance : BREP_SAME_POINT_TOLERANCE;
    const size_t group_count = brep_trace_event_groups(trace->local_roots,
	trace->stored_local_roots, order, groups, tolerance);
    trace->local_event_groups = group_count;

    brep_hit_workspace candidate_hits;
    for (size_t root_index = 0; root_index < trace->stored_local_roots;
	    ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	if (root.hit_class == brep_hit::CLEAN_MISS)
	    continue;
	brep_hit hit;
	if (!brep_trace_make_hit(bs, ray, root.face_index, root.dist, root.uv,
		root.trim_status, root.hit_class, root.direction,
		root.adjacent_face_index, true, hit)) {
	    trace->local_candidate_failures++;
	    continue;
	}
	candidate_hits.push_back(hit);
    }
    candidate_hits.sort();
    trace->local_candidate_hits = candidate_hits.total();
    trace->local_candidate_overflow = candidate_hits.overflow() ? 1 : 0;

    brep_hit_workspace legacy_candidate_hits;
    for (size_t root_index = 0; root_index < trace->stored_roots;
	    ++root_index) {
	const struct rt_brep_trace_root &root = trace->roots[root_index];
	if (root.hit_class == brep_hit::CLEAN_MISS)
	    continue;
	brep_hit hit;
	if (!brep_trace_make_hit(bs, ray, root.face_index, root.dist, root.uv,
		root.trim_status, root.hit_class, root.direction,
		root.adjacent_face_index, false, hit)) {
	    trace->local_candidate_failures++;
	    continue;
	}
	legacy_candidate_hits.push_back(hit);
    }
    legacy_candidate_hits.sort();
    if (legacy_candidate_hits.overflow())
	trace->local_candidate_overflow++;
    if (!candidate_hits.overflow()) {
	brep_cleanup_observation local_observation = {};
	brep_cleanup_observation legacy_observation = {};
	local_observation.tolerance = tolerance;
	legacy_observation.tolerance = tolerance;
	const brep_hit_cleanup_state candidate_cleanup =
	    cleanup_fixed_brep_hits(candidate_hits, xray, tol,
		&local_observation);
	(void)cleanup_fixed_brep_hits(legacy_candidate_hits, xray, tol,
	    &legacy_observation);
	trace->local_candidate_after_near_miss =
	    candidate_cleanup.after_near_miss;
	trace->local_candidate_after_near_hit =
	    candidate_cleanup.after_near_hit;
	trace->local_candidate_after_grazing =
	    candidate_cleanup.after_grazing;
	trace->local_candidate_after_duplicates =
	    candidate_cleanup.after_duplicates;
	trace->local_candidate_after_direction_cleanup =
	    candidate_cleanup.after_direction;
	if (candidate_cleanup.after_near_miss != trace->after_near_miss)
	    trace->local_candidate_stage_mismatches++;
	if (candidate_cleanup.after_near_hit != trace->after_near_hit)
	    trace->local_candidate_stage_mismatches++;
	if (candidate_cleanup.after_grazing != trace->after_grazing)
	    trace->local_candidate_stage_mismatches++;
	if (candidate_cleanup.after_duplicates != trace->after_duplicates)
	    trace->local_candidate_stage_mismatches++;
	if (candidate_cleanup.after_direction != trace->after_direction_cleanup)
	    trace->local_candidate_stage_mismatches++;
	for (size_t stage_index = 0; stage_index < BREP_CLEANUP_STAGE_COUNT;
		++stage_index) {
	    if (!brep_cleanup_signatures_match(
		    legacy_observation.stages[stage_index],
		    local_observation.stages[stage_index], tolerance)) {
		trace->local_candidate_semantic_stage_mismatches++;
		trace->local_candidate_semantic_stage[stage_index]++;
	    }
	}
	if (candidate_hits.size() != legacy_hits.size()) {
	    trace->local_candidate_hit_mismatches++;
	} else {
	    size_t hit_index = 0;
	    for (std::list<brep_hit>::const_iterator legacy =
		    legacy_hits.begin(); legacy != legacy_hits.end();
		    ++legacy, ++hit_index) {
		const brep_hit &local = candidate_hits[hit_index];
		if (fabs(legacy->dist - local.dist) > tolerance ||
			legacy->face->m_face_index !=
			local.face->m_face_index ||
			legacy->trimmed != local.trimmed ||
			legacy->closeToEdge != local.closeToEdge ||
			legacy->hit != local.hit ||
			legacy->direction != local.direction ||
			(legacy->closeToEdge &&
			 legacy->m_adj_face_index != local.m_adj_face_index))
		    trace->local_candidate_hit_mismatches++;
	    }
	}
    }

    brep_hit_workspace local_hits;
    for (size_t group_index = 0; group_index < group_count; ++group_index) {
	const brep_trace_event_group &group = groups[group_index];
	if (group.direction_class == RT_BREP_TRACE_LOCAL_CONTACT) {
	    trace->local_event_contacts++;
	    continue;
	}
	if (group.hit_class == brep_hit::CLEAN_MISS) {
	    trace->local_event_clean_misses++;
	    continue;
	}
	brep_hit hit;
	if (!brep_trace_make_hit(bs, ray, group.face_index, group.dist,
		group.uv, group.trim_status, group.hit_class,
		group.direction_class, group.adjacency, true, hit)) {
	    trace->local_event_failures++;
	    continue;
	}
	local_hits.push_back(hit);
    }
    local_hits.sort();
    trace->local_event_hits = local_hits.total();
    trace->local_event_overflow = local_hits.overflow() ? 1 : 0;
    if (local_hits.overflow())
	return;

    const brep_hit_cleanup_state local_cleanup =
	cleanup_fixed_brep_hits(local_hits, xray, tol);
    trace->local_event_after_near_miss = local_cleanup.after_near_miss;
    trace->local_event_after_near_hit = local_cleanup.after_near_hit;
    trace->local_event_after_grazing = local_cleanup.after_grazing;
    trace->local_event_after_duplicates = local_cleanup.after_duplicates;
    trace->local_event_after_direction_cleanup =
	local_cleanup.after_direction;
    if (local_cleanup.after_near_miss != trace->after_near_miss)
	trace->local_event_stage_mismatches++;
    if (local_cleanup.after_near_hit != trace->after_near_hit)
	trace->local_event_stage_mismatches++;
    if (local_cleanup.after_grazing != trace->after_grazing)
	trace->local_event_stage_mismatches++;
    if (local_cleanup.after_duplicates != trace->after_duplicates)
	trace->local_event_stage_mismatches++;
    if (local_cleanup.after_direction != trace->after_direction_cleanup)
	trace->local_event_stage_mismatches++;

    if (local_hits.size() != legacy_hits.size()) {
	trace->local_event_hit_mismatches++;
	trace->local_event_count_mismatches++;
    } else {
	size_t hit_index = 0;
	for (std::list<brep_hit>::const_iterator legacy = legacy_hits.begin();
		legacy != legacy_hits.end(); ++legacy, ++hit_index) {
	    if (!brep_trace_hits_equivalent(*legacy, local_hits[hit_index],
		    tolerance, trace))
		trace->local_event_hit_mismatches++;
	}
    }

    trace->local_event_repaired =
	repair_fixed_brep_crack(local_hits, bs, ray) ? 1 : 0;
    trace->local_event_final_hits = local_hits.size();
    trace->local_event_final_segments = bs->plate_mode ? local_hits.size() :
	(local_hits.size() > 1 && local_hits.size() % 2 == 0 ?
	 local_hits.size() / 2 : 0);
    if (bs->plate_mode) {
	trace->local_event_segment_overflow =
	    trace->local_event_final_segments ? 1 : 0;
    } else {
	for (size_t hit_index = 0; hit_index + 1 < local_hits.size();
		hit_index += 2) {
	    if (trace->local_event_stored_segments ==
		    RT_BREP_TRACE_MAX_LOCAL_SEGMENTS) {
		trace->local_event_segment_overflow++;
		continue;
	    }
	    const size_t segment_index = trace->local_event_stored_segments++;
	    trace->local_event_segment_in[segment_index] =
		local_hits[hit_index].dist;
	    trace->local_event_segment_out[segment_index] =
		local_hits[hit_index + 1].dist;
	}
    }

    brep_hit_workspace expected_hits;
    for (std::list<brep_hit>::const_iterator legacy = legacy_hits.begin();
	    legacy != legacy_hits.end(); ++legacy)
	expected_hits.push_back(*legacy);
    if (legacy_repaired_hit)
	expected_hits.push_back(*legacy_repaired_hit);
    expected_hits.sort();
    const size_t expected_segments = bs->plate_mode ? expected_hits.size() :
	(expected_hits.size() > 1 && expected_hits.size() % 2 == 0 ?
	 expected_hits.size() / 2 : 0);
    bool partition_mismatch =
	expected_segments != trace->local_event_final_segments;
    if (!partition_mismatch && expected_segments > 0) {
	if (expected_hits.size() != local_hits.size()) {
	    partition_mismatch = true;
	} else {
	    for (size_t hit_index = 0; hit_index < expected_hits.size();
		    ++hit_index) {
		const double t_error = fabs(expected_hits[hit_index].dist -
		    local_hits[hit_index].dist);
		trace->local_event_max_t_error = std::max(
		    (double)trace->local_event_max_t_error, t_error);
		if (t_error > tolerance) {
		    partition_mismatch = true;
		    break;
		}
	    }
	}
    }
    if (partition_mismatch)
	trace->local_event_final_mismatches++;
}


static bool
repair_fixed_brep_crack(brep_hit_workspace &hits,
    const struct brep_specific *bs, const ON_Ray &ray)
{
    if (!bs || !bs->is_solid || bs->plate_mode || hits.size() != 1)
	return false;

    struct rt_brep_shot_trace repair = {};
    repair.closure_edge_index = -1;
    repair.closure_missing_direction = -1;
    repair.continuation_face_index = -1;
    repair.continuation_span_index = -1;
    repair.continuation_adjacent_face_index = -99;
    brep_observe_edges(&repair, bs, ray);
    if (repair.edge_overflow || repair.edge_evaluation_failures ||
	    repair.edge_observations != repair.stored_edges)
	return false;
    brep_classify_closure(&repair, bs, &hits.front());
    brep_hit repaired_hit;
    brep_resolve_continuation(&repair, bs, ray, &hits.front(), &repaired_hit);
    if (repair.closure_shadow_segments != 1 ||
	    repair.continuation_candidates != 1 ||
	    repair.continuation_certified_candidates != 1 ||
	    repair.continuation_certificate_exhausted ||
	    repair.continuation_certificate_existing_overlap)
	return false;

    const brep_hit &existing_hit = hits.front();
    const bool ordered = existing_hit.direction == brep_hit::ENTERING ?
	existing_hit.dist < repaired_hit.dist &&
	    repaired_hit.direction == brep_hit::LEAVING :
	repaired_hit.dist < existing_hit.dist &&
	    repaired_hit.direction == brep_hit::ENTERING;
    if (!ordered)
	return false;

    hits.push_back(repaired_hit);
    hits.sort();
    return true;
}


static bool
brep_prepared_box_matches_local_root(
    const struct rt_brep_trace_surface_box &box,
    const struct rt_brep_trace_local_root &root, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    const double ray_length = ray.m_dir.Length();
    if (!(ray_length > DBL_MIN) || !std::isfinite(ray_length))
	return false;
    const double model_tolerance = tol && tol->dist > 0.0 &&
	std::isfinite(tol->dist) ? 0.1 * tol->dist / ray_length : 0.0;
    const double t_scale = std::max(1.0,
	std::max(fabs(box.t_min), fabs(box.t_max)));
    const double t_tolerance = std::max(model_tolerance,
	128.0 * DBL_EPSILON * t_scale);
    const double u_scale = std::max(1.0,
	std::max(fabs(box.uv_min[0]), fabs(box.uv_max[0])));
    const double v_scale = std::max(1.0,
	std::max(fabs(box.uv_min[1]), fabs(box.uv_max[1])));
    const double u_tolerance = 128.0 * DBL_EPSILON * u_scale;
    const double v_tolerance = 128.0 * DBL_EPSILON * v_scale;
    return root.face_index == box.face_index &&
	root.span_index == box.span_index &&
	root.dist >= box.t_min - t_tolerance &&
	root.dist <= box.t_max + t_tolerance &&
	root.uv[0] >= box.uv_min[0] - u_tolerance &&
	root.uv[0] <= box.uv_max[0] + u_tolerance &&
	root.uv[1] >= box.uv_min[1] - v_tolerance &&
	root.uv[1] <= box.uv_max[1] + v_tolerance;
}


static bool
brep_prepared_box_matches_fold_root(
    const struct rt_brep_trace_surface_box &box,
    const struct rt_brep_trace_fold_root &root, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    const double ray_length = ray.m_dir.Length();
    if (!(ray_length > DBL_MIN) || !std::isfinite(ray_length))
	return false;
    const double model_tolerance = tol && tol->dist > 0.0 &&
	std::isfinite(tol->dist) ? 0.1 * tol->dist / ray_length : 0.0;
    const double t_scale = std::max(1.0,
	std::max(fabs(box.t_min), fabs(box.t_max)));
    const double t_tolerance = std::max(model_tolerance,
	128.0 * DBL_EPSILON * t_scale);
    const double u_scale = std::max(1.0,
	std::max(fabs(box.uv_min[0]), fabs(box.uv_max[0])));
    const double v_scale = std::max(1.0,
	std::max(fabs(box.uv_min[1]), fabs(box.uv_max[1])));
    const double u_tolerance = 128.0 * DBL_EPSILON * u_scale;
    const double v_tolerance = 128.0 * DBL_EPSILON * v_scale;
    return root.face_index == box.face_index &&
	root.span_index == box.span_index &&
	root.t_max >= box.t_min - t_tolerance &&
	root.t_min <= box.t_max + t_tolerance &&
	root.uv[0] >= box.uv_min[0] - u_tolerance &&
	root.uv[0] <= box.uv_max[0] + u_tolerance &&
	root.uv[1] >= box.uv_min[1] - v_tolerance &&
	root.uv[1] <= box.uv_max[1] + v_tolerance;
}


static bool
brep_prepared_box_has_root(const struct rt_brep_shot_trace *trace,
    const struct rt_brep_trace_surface_box &box, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    if (!trace)
	return false;
    for (size_t root_index = 0; root_index < trace->stored_local_roots;
	    ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	if (brep_prepared_box_matches_local_root(box, root, ray, tol))
	    return true;
    }
    for (size_t root_index = 0;
	    root_index < trace->stored_surface_fold_roots; ++root_index) {
	const struct rt_brep_trace_fold_root &root =
	    trace->surface_fold_roots_data[root_index];
	if (brep_prepared_box_matches_fold_root(box, root, ray, tol))
	    return true;
    }
    return false;
}


static bool
brep_fold_root_matches_local(const struct rt_brep_trace_fold_root &fold,
    const struct rt_brep_trace_local_root &local)
{
    if (fold.face_index != local.face_index ||
	    fold.span_index != local.span_index)
	return false;
    const double t_scale = std::max(1.0,
	std::max(fabs(fold.t_min), fabs(fold.t_max)));
    const double t_tolerance = 128.0 * DBL_EPSILON * t_scale;
    return local.dist >= fold.t_min - t_tolerance &&
	local.dist <= fold.t_max + t_tolerance;
}


static void
brep_trace_finalize_physical_events(struct rt_brep_shot_trace *trace,
    const ON_Ray &ray, const struct bn_tol *tol, bool complete)
{
    if (!trace)
	return;
    for (size_t i = 1; i < trace->stored_physical_events; ++i) {
	const struct rt_brep_trace_physical_event value =
	    trace->physical_events[i];
	size_t j = i;
	while (j && value.t_min < trace->physical_events[j - 1].t_min) {
	    trace->physical_events[j] = trace->physical_events[j - 1];
	    --j;
	}
	trace->physical_events[j] = value;
    }

    brep_interval minimum_t;
    if (complete && !brep_fold_minimum_t_interval(ray, tol, minimum_t)) {
	trace->physical_event_state_failures++;
	complete = false;
    }
    int state = 0;
    size_t entering_index = 0;
    for (size_t event_index = 0;
	    complete && event_index < trace->stored_physical_events;
	    ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace->physical_events[event_index];
	if (event_index && state == 0 &&
		trace->physical_events[event_index - 1].t_max >= event.t_min) {
	    trace->physical_event_state_failures++;
	    complete = false;
	    break;
	}
	if (event.direction == brep_hit::ENTERING && state == 0) {
	    entering_index = event_index;
	    state = 1;
	    continue;
	}
	if (event.direction != brep_hit::LEAVING || state != 1) {
	    trace->physical_event_state_failures++;
	    complete = false;
	    break;
	}
	const struct rt_brep_trace_physical_event &entering =
	    trace->physical_events[entering_index];
	brep_interval gap;
	const int gap_class = brep_fold_gap_classify(
	    {entering.t_min, entering.t_max}, {event.t_min, event.t_max},
	    minimum_t, gap);
	if (gap_class == RT_BREP_FOLD_GAP_RESOLVED) {
	    trace->physical_event_material_segments++;
	} else if (gap_class == RT_BREP_FOLD_GAP_SUBMINIMUM) {
	    trace->physical_event_subminimum_contacts++;
	} else {
	    trace->physical_event_tolerance_ambiguous++;
	    trace->physical_event_state_failures++;
	    complete = false;
	    break;
	}
	state = 0;
    }
    if (complete && state != 0) {
	trace->physical_event_state_failures++;
	complete = false;
    }
    if (complete)
	trace->physical_event_complete++;
}


static void
brep_trace_regular_physical_events(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    if (!trace || !bs || !bs->brep)
	return;

    bool complete = trace->prepared_surface_spans &&
	!trace->unsupported_surface_faces &&
	trace->supported_surface_faces == bs->face_records.size() &&
	trace->candidate_surface_spans + trace->excluded_surface_spans ==
	    trace->prepared_surface_spans &&
	!trace->surface_workspace_exhausted &&
	!trace->surface_clip_restriction_failures &&
	!trace->surface_box_overflow &&
	trace->surface_isolated_boxes == trace->stored_surface_boxes &&
	trace->surface_isolated_boxes == trace->surface_krawczyk_boxes &&
	!trace->local_root_overflow && !trace->local_trim_failures &&
	trace->local_root_candidates == trace->stored_local_roots;
    bool root_owned[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};

    for (size_t box_index = 0; box_index < trace->stored_surface_boxes;
	    ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	trace->physical_event_attempts++;
	if (box.disposition != RT_BREP_TRACE_BOX_RESOLVED_REGULAR ||
		!box.determinant_sign || box.face_index < 0 ||
		box.face_index >= bs->brep->m_F.Count()) {
	    trace->physical_event_unresolved++;
	    complete = false;
	    continue;
	}

	size_t matching_root = 0;
	size_t matches = 0;
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index) {
	    if (!brep_prepared_box_matches_local_root(box,
		    trace->local_roots[root_index], ray, tol))
		continue;
	    matching_root = root_index;
	    matches++;
	}
	if (matches != 1 || root_owned[matching_root]) {
	    trace->physical_event_unresolved++;
	    complete = false;
	    continue;
	}
	root_owned[matching_root] = true;
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[matching_root];
	int oriented_sign = box.determinant_sign;
	if (bs->brep->m_F[box.face_index].m_bRev)
	    oriented_sign = -oriented_sign;
	const int direction = oriented_sign < 0 ? brep_hit::ENTERING :
	    brep_hit::LEAVING;
	trace->physical_event_direction_checks++;
	if (!std::isfinite(root.normal_dot) || root.direction != direction) {
	    trace->physical_event_direction_mismatches++;
	    trace->physical_event_unresolved++;
	    complete = false;
	    continue;
	}

	if (root.hit_class == brep_hit::CLEAN_MISS &&
		root.trim_status == 1) {
	    trace->physical_event_clean_outside++;
	    continue;
	}
	if (root.hit_class != brep_hit::CLEAN_HIT ||
		root.trim_status == 1) {
	    trace->physical_event_near_trim++;
	    trace->physical_event_unresolved++;
	    complete = false;
	    continue;
	}

	if (trace->stored_physical_events >=
		RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	    trace->physical_event_overflow++;
	    complete = false;
	    continue;
	}
	struct rt_brep_trace_physical_event &event =
	    trace->physical_events[trace->stored_physical_events++];
	event = {};
	event.dist = root.dist;
	event.t_min = box.t_min;
	event.t_max = box.t_max;
	event.uv[0] = root.uv[0];
	event.uv[1] = root.uv[1];
	event.source_box = box_index;
	event.source_box_count = 1;
	event.source_root = matching_root;
	event.source_kind = RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT;
	event.edge_index = -1;
	event.vertex_index = -1;
	event.face_index = root.face_index;
	event.span_index = root.span_index;
	event.certificate = RT_BREP_TRACE_EVENT_REGULAR_INTERIOR;
	event.determinant_sign = box.determinant_sign;
	event.hit_class = root.hit_class;
	event.trim_status = root.trim_status;
	event.adjacent_face_index = root.adjacent_face_index;
	event.direction = direction;
	trace->physical_event_regular++;
    }

    if (complete) {
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index) {
	    if (!root_owned[root_index]) {
		trace->physical_event_unresolved++;
		complete = false;
	    }
	}
    }
    brep_trace_finalize_physical_events(trace, ray, tol, complete);
}


static ON_3dVector
brep_vertex_arc_direction(const brep_vertex_arc &arc, double angle)
{
    const double cosine = cos(angle);
    const double sine = sin(angle);
    ON_3dVector direction = cosine * arc.outgoing -
	sine * ON_CrossProduct(arc.outward_normal, arc.outgoing);
    direction.Unitize();
    return direction;
}


/* The oriented winding of the spherical vertex link about the ray direction
 * is I(+D)-I(-D).  Thus +1 is an entering transition and -1 is a leaving
 * transition even for a nonconvex fan; zero is a contact or an unsupported
 * ambiguity and is deliberately not published here.  Reflex face arcs are
 * subdivided along their stored major sweep so endpoint atan2 branches cannot
 * silently replace them with the complementary minor arc. */
static int
brep_vertex_fan_direction(const brep_vertex_record &record,
    const ON_3dVector &ray_direction, size_t &winding_checks)
{
    if (!record.supported || record.arcs.size() < 3)
	return -1;
    ON_3dVector axis = ray_direction;
    if (!axis.Unitize())
	return -1;

    long double winding = 0.0L;
    size_t segments = 0;
    for (size_t arc_index = 0; arc_index < record.arcs.size();
	    ++arc_index) {
	const brep_vertex_arc &arc = record.arcs[arc_index];
	if (!arc.outgoing.IsValid() || !arc.outward_normal.IsValid() ||
		!std::isfinite(arc.clockwise_sweep) ||
		arc.clockwise_sweep <= 0.0 ||
		arc.clockwise_sweep >= 2.0 * ON_PI ||
		fabs(arc.outward_normal * axis) <= 1.0e-8)
	    return -1;
	const size_t subdivisions = std::max((size_t)1,
	    (size_t)ceil(arc.clockwise_sweep / (0.25 * ON_PI)));
	if (subdivisions > 8)
	    return -1;
	ON_3dVector start = arc.outgoing;
	for (size_t subdivision = 1; subdivision <= subdivisions;
		++subdivision) {
	    const ON_3dVector end = subdivision == subdivisions ?
		record.arcs[(arc_index + 1) % record.arcs.size()].outgoing :
		brep_vertex_arc_direction(arc,
		    arc.clockwise_sweep * (double)subdivision /
		    (double)subdivisions);
	    const long double numerator = (long double)(axis *
		ON_CrossProduct(start, end));
	    const long double denominator = (long double)(start * end) -
		(long double)(axis * start) * (long double)(axis * end);
	    if (!std::isfinite((double)numerator) ||
		    !std::isfinite((double)denominator) ||
		    (fabsl(numerator) <= 64.0L * LDBL_EPSILON &&
		     fabsl(denominator) <= 64.0L * LDBL_EPSILON))
		return -1;
	    winding += atan2l(numerator, denominator);
	    start = end;
	    segments++;
	}
    }
    winding_checks += segments;
    const long double turn = 2.0L * (long double)ON_PI;
    const long double error =
	std::max(1.0e-10L, 4096.0L * LDBL_EPSILON * segments);
    if (fabsl(winding - turn) <= error)
	return brep_hit::ENTERING;
    if (fabsl(winding + turn) <= error)
	return brep_hit::LEAVING;
    return -1;
}


static bool
brep_vertex_ray_parameter(const brep_vertex_record &record,
    const ON_Ray &ray, double &parameter, double &distance_tolerance)
{
    const double length_squared = ray.m_dir.LengthSquared();
    if (!(length_squared > DBL_MIN) || !std::isfinite(length_squared))
	return false;
    parameter = ((record.point - ray.m_origin) * ray.m_dir) /
	length_squared;
    if (!std::isfinite(parameter))
	return false;
    const ON_3dPoint closest = ray.m_origin + parameter * ray.m_dir;
    const double coordinate_scale = std::max(1.0,
	std::max(fabs(record.point.x), std::max(fabs(record.point.y),
	std::max(fabs(record.point.z), std::max(fabs(ray.m_origin.x),
	std::max(fabs(ray.m_origin.y), fabs(ray.m_origin.z)))))));
    const double line_tolerance = std::max(ON_ZERO_TOLERANCE,
	4096.0 * DBL_EPSILON * coordinate_scale);
    if (!closest.IsValid() || closest.DistanceTo(record.point) >
	    line_tolerance)
	return false;
    distance_tolerance = std::max(line_tolerance / sqrt(length_squared),
	4096.0 * DBL_EPSILON * std::max(1.0, fabs(parameter)));
    return true;
}


/* Collapse one exactly witnessed planar manifold vertex fan and combine it
 * with any ordinary certified roots on the rest of the ray.  Selection,
 * complete many-boxes-to-one-root ownership, direction agreement, and the
 * entire material state stream are checked before any disposition or event is
 * committed. */
static bool
brep_trace_vertex_physical_events(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    if (!trace || !bs || !bs->brep || !bs->is_solid || bs->plate_mode ||
	    bs->vertex_records.empty())
	return false;
    const bool workspace_complete = trace->prepared_surface_spans &&
	!trace->unsupported_surface_faces &&
	trace->supported_surface_faces == bs->face_records.size() &&
	trace->candidate_surface_spans + trace->excluded_surface_spans ==
	    trace->prepared_surface_spans &&
	!trace->surface_workspace_exhausted &&
	!trace->surface_clip_restriction_failures &&
	!trace->surface_box_overflow &&
	trace->surface_isolated_boxes == trace->stored_surface_boxes &&
	!trace->local_root_overflow && !trace->local_trim_failures &&
	trace->local_root_candidates == trace->stored_local_roots &&
	trace->stored_surface_boxes && trace->stored_local_roots &&
	!trace->stored_physical_events;
    if (!workspace_complete)
	return false;

    struct vertex_event_candidate {
	size_t vertex_record = 0;
	size_t canonical_root = 0;
	size_t first_box = 0;
	size_t roots = 0;
	size_t boxes = 0;
	double parameter = 0.0;
	double t_min = 0.0;
	double t_max = 0.0;
	int direction = -1;
    } candidates[RT_BREP_TRACE_MAX_PHYSICAL_EVENTS];
    bool selected_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    bool selected_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    size_t selected_roots = 0;
    size_t selected_boxes = 0;
    size_t candidate_count = 0;
    bool candidate_conflict = false;
    bool saw_vertex_line = false;
    size_t winding_checks = 0;
    size_t winding_ambiguous = 0;

    for (size_t record_index = 0;
	    record_index < bs->vertex_records.size(); ++record_index) {
	const brep_vertex_record &record = bs->vertex_records[record_index];
	if (!record.supported)
	    continue;
	double parameter = 0.0;
	double t_tolerance = 0.0;
	if (!brep_vertex_ray_parameter(record, ray, parameter, t_tolerance))
	    continue;
	saw_vertex_line = true;
	const double ray_length = ray.m_dir.Length();
	const double root_t_tolerance = std::max(t_tolerance,
	    tol && tol->dist > 0.0 && std::isfinite(tol->dist) &&
	    ray_length > DBL_MIN ? 0.1 * tol->dist / ray_length : 0.0);
	const int direction = brep_vertex_fan_direction(record, ray.m_dir,
	    winding_checks);
	if (direction != brep_hit::ENTERING &&
		direction != brep_hit::LEAVING) {
	    winding_ambiguous++;
	    continue;
	}

	bool candidate_roots[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
	bool candidate_boxes[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
	size_t canonical_root = (size_t)-1;
	size_t first_box = (size_t)-1;
	size_t root_count = 0;
	size_t box_count = 0;
	double t_min = DBL_MAX;
	double t_max = -DBL_MAX;
	bool complete = true;
	bool face_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
	for (size_t matching_root = 0; complete &&
		matching_root < trace->stored_local_roots; ++matching_root) {
	    const struct rt_brep_trace_local_root &root =
		trace->local_roots[matching_root];
	    const ON_3dPoint root_point = ray.m_origin +
		root.dist * ray.m_dir;
	    if (!root_point.IsValid() ||
		    root_point.DistanceTo(record.point) >
		    root_t_tolerance * ray_length)
		continue;
	    size_t face_slot = 0;
	    while (face_slot < record.arcs.size() &&
		    record.arcs[face_slot].face_index != root.face_index)
		++face_slot;
	    if (face_slot == record.arcs.size() ||
		    face_root[face_slot] ||
		    fabs(root.dist - parameter) > root_t_tolerance ||
		    !std::isfinite(root.normal_dot) ||
		    fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL ||
		    root.direction != direction ||
		    root.hit_class == brep_hit::CRACK_HIT) {
		complete = false;
		break;
	    }
	    size_t box_matches = 0;
	    for (size_t box_index = 0;
		    box_index < trace->stored_surface_boxes; ++box_index) {
		const struct rt_brep_trace_surface_box &box =
		    trace->surface_boxes[box_index];
		if (!brep_prepared_box_matches_local_root(box,
			trace->local_roots[matching_root], ray, tol))
		    continue;
		const bool unresolved =
		    box.disposition == RT_BREP_TRACE_BOX_UNRESOLVED &&
		    !box.determinant_sign;
		const bool regular =
		    box.disposition == RT_BREP_TRACE_BOX_RESOLVED_REGULAR &&
		    box.determinant_sign;
		if (candidate_boxes[box_index] || (!unresolved && !regular)) {
		    complete = false;
		    break;
		}
		candidate_boxes[box_index] = true;
		if (first_box == (size_t)-1)
		    first_box = box_index;
		t_min = std::min(t_min, (double)box.t_min);
		t_max = std::max(t_max, (double)box.t_max);
		box_matches++;
	    }
	    if (!complete || !box_matches) {
		complete = false;
		break;
	    }
	    face_root[face_slot] = true;
	    candidate_roots[matching_root] = true;
	    root_count++;
	    box_count += box_matches;
	    if (canonical_root == (size_t)-1 ||
		    ((root.hit_class == brep_hit::CLEAN_HIT ||
		      root.hit_class == brep_hit::NEAR_HIT) &&
		     (trace->local_roots[canonical_root].hit_class !=
			 brep_hit::CLEAN_HIT &&
		      trace->local_roots[canonical_root].hit_class !=
			 brep_hit::NEAR_HIT)))
		canonical_root = matching_root;
	}
	for (size_t box_index = 0; complete &&
		box_index < trace->stored_surface_boxes; ++box_index) {
	    const struct rt_brep_trace_surface_box &box =
		trace->surface_boxes[box_index];
	    if (parameter < box.t_min - root_t_tolerance ||
		    parameter > box.t_max + root_t_tolerance)
		continue;
	    bool incident_face = false;
	    for (size_t arc_index = 0; arc_index < record.arcs.size();
		    ++arc_index)
		if (record.arcs[arc_index].face_index == box.face_index)
		    incident_face = true;
	    if (incident_face && !candidate_boxes[box_index])
		complete = false;
	}
	if (!complete || !root_count || !box_count ||
		canonical_root == (size_t)-1 || first_box == (size_t)-1)
	    continue;

	trace->physical_event_vertex_candidates++;
	if (candidate_count >= RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	    candidate_conflict = true;
	    continue;
	}
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index)
	    if (candidate_roots[root_index] && selected_root[root_index])
		candidate_conflict = true;
	for (size_t box_index = 0;
		box_index < trace->stored_surface_boxes; ++box_index)
	    if (candidate_boxes[box_index] && selected_box[box_index])
		candidate_conflict = true;
	if (candidate_conflict)
	    continue;
	vertex_event_candidate &candidate = candidates[candidate_count++];
	candidate.vertex_record = record_index;
	candidate.canonical_root = canonical_root;
	candidate.first_box = first_box;
	candidate.roots = root_count;
	candidate.boxes = box_count;
	candidate.parameter = parameter;
	candidate.t_min = t_min;
	candidate.t_max = t_max;
	candidate.direction = direction;
	selected_roots += root_count;
	selected_boxes += box_count;
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index)
	    if (candidate_roots[root_index])
		selected_root[root_index] = true;
	for (size_t box_index = 0;
		box_index < trace->stored_surface_boxes; ++box_index)
	    if (candidate_boxes[box_index])
		selected_box[box_index] = true;
    }
    if (!saw_vertex_line)
	return false;
    trace->physical_event_vertex_attempts++;
    trace->physical_event_vertex_winding_checks += winding_checks;
    trace->physical_event_vertex_winding_ambiguous += winding_ambiguous;
    if (!candidate_count || candidate_conflict) {
	trace->physical_event_vertex_failures++;
	return false;
    }

    struct rt_brep_trace_physical_event
	events[RT_BREP_TRACE_MAX_PHYSICAL_EVENTS] = {};
    size_t event_count = 0;
    bool root_owned[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    for (size_t root_index = 0; root_index < trace->stored_local_roots;
	    ++root_index)
	root_owned[root_index] = selected_root[root_index];

    for (size_t candidate_index = 0; candidate_index < candidate_count;
	    ++candidate_index) {
	const vertex_event_candidate &candidate = candidates[candidate_index];
	const brep_vertex_record &record =
	    bs->vertex_records[candidate.vertex_record];
	const struct rt_brep_trace_local_root &canonical =
	    trace->local_roots[candidate.canonical_root];
	struct rt_brep_trace_physical_event &vertex_event =
	    events[event_count++];
	vertex_event.dist = candidate.parameter;
	vertex_event.t_min = candidate.t_min;
	vertex_event.t_max = candidate.t_max;
	vertex_event.uv[0] = canonical.uv[0];
	vertex_event.uv[1] = canonical.uv[1];
	vertex_event.source_box = candidate.first_box;
	vertex_event.source_box_count = candidate.boxes;
	vertex_event.source_root = candidate.canonical_root;
	vertex_event.source_kind = RT_BREP_TRACE_EVENT_SOURCE_VERTEX_FAN;
	vertex_event.edge_index = -1;
	vertex_event.vertex_index = record.vertex_index;
	vertex_event.face_index = canonical.face_index;
	vertex_event.span_index = canonical.span_index;
	vertex_event.certificate = RT_BREP_TRACE_EVENT_VERTEX_FAN;
	vertex_event.determinant_sign = 0;
	vertex_event.hit_class = brep_hit::CRACK_HIT;
	vertex_event.trim_status = canonical.trim_status;
	vertex_event.adjacent_face_index = canonical.adjacent_face_index;
	vertex_event.direction = candidate.direction;
    }

    bool complete = true;
    size_t regular_events = 0;
    size_t regular_roots = 0;
    size_t clean_outside = 0;
    bool box_owned[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index)
	box_owned[box_index] = selected_box[box_index];
    for (size_t root_index = 0; complete &&
	    root_index < trace->stored_local_roots; ++root_index) {
	if (root_owned[root_index])
	    continue;
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	size_t first_box = (size_t)-1;
	size_t box_count = 0;
	double t_min = DBL_MAX;
	double t_max = -DBL_MAX;
	int direction = -1;
	int determinant_sign = 0;
	for (size_t box_index = 0; complete &&
		box_index < trace->stored_surface_boxes; ++box_index) {
	    const struct rt_brep_trace_surface_box &box =
		trace->surface_boxes[box_index];
	    if (!brep_prepared_box_matches_local_root(box, root, ray, tol))
		continue;
	    if (box_owned[box_index] ||
		    box.disposition != RT_BREP_TRACE_BOX_RESOLVED_REGULAR ||
		    !box.determinant_sign || box.face_index < 0 ||
		    box.face_index >= bs->brep->m_F.Count()) {
		complete = false;
		break;
	    }
	    int oriented_sign = box.determinant_sign;
	    if (bs->brep->m_F[box.face_index].m_bRev)
		oriented_sign = -oriented_sign;
	    const int box_direction = oriented_sign < 0 ?
		brep_hit::ENTERING : brep_hit::LEAVING;
	    if ((direction >= 0 && direction != box_direction) ||
		    root.direction != box_direction) {
		complete = false;
		break;
	    }
	    direction = box_direction;
	    determinant_sign = box.determinant_sign;
	    box_owned[box_index] = true;
	    if (first_box == (size_t)-1)
		first_box = box_index;
	    t_min = std::min(t_min, (double)box.t_min);
	    t_max = std::max(t_max, (double)box.t_max);
	    box_count++;
	}
	if (!complete || !box_count || first_box == (size_t)-1 ||
		!std::isfinite(root.normal_dot) || root.direction != direction) {
	    complete = false;
	    break;
	}
	root_owned[root_index] = true;
	regular_roots++;
	if (root.hit_class == brep_hit::CLEAN_MISS &&
		root.trim_status == 1) {
	    clean_outside++;
	    continue;
	}
	if (root.hit_class != brep_hit::CLEAN_HIT ||
		root.trim_status == 1 ||
		event_count >= RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	    complete = false;
	    break;
	}
	struct rt_brep_trace_physical_event &event = events[event_count++];
	event.dist = root.dist;
	event.t_min = t_min;
	event.t_max = t_max;
	event.uv[0] = root.uv[0];
	event.uv[1] = root.uv[1];
	event.source_box = first_box;
	event.source_box_count = box_count;
	event.source_root = root_index;
	event.source_kind = RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT;
	event.edge_index = -1;
	event.vertex_index = -1;
	event.face_index = root.face_index;
	event.span_index = root.span_index;
	event.certificate = RT_BREP_TRACE_EVENT_REGULAR_INTERIOR;
	event.determinant_sign = determinant_sign;
	event.hit_class = root.hit_class;
	event.trim_status = root.trim_status;
	event.adjacent_face_index = root.adjacent_face_index;
	event.direction = direction;
	regular_events++;
    }
    for (size_t root_index = 0; complete &&
	    root_index < trace->stored_local_roots; ++root_index)
	if (!root_owned[root_index])
	    complete = false;
    for (size_t box_index = 0; complete &&
	    box_index < trace->stored_surface_boxes; ++box_index)
	if (!box_owned[box_index])
	    complete = false;
    if (!complete || !event_count ||
	    event_count > RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	trace->physical_event_vertex_failures++;
	return false;
    }

    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index)
	if (selected_box[box_index])
	    trace->surface_boxes[box_index].disposition =
		RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY;
    for (size_t event_index = 0; event_index < event_count; ++event_index)
	trace->physical_events[trace->stored_physical_events++] =
	    events[event_index];
    trace->physical_event_attempts += trace->stored_surface_boxes;
    trace->physical_event_direction_checks +=
	selected_roots + regular_roots;
    trace->physical_event_regular += regular_events;
    trace->physical_event_clean_outside += clean_outside;
    trace->physical_event_near_trim += selected_roots;
    trace->physical_event_vertex += candidate_count;
    trace->physical_event_vertex_owned_boxes += selected_boxes;
    trace->physical_event_vertex_owned_roots += selected_roots;
    brep_trace_finalize_physical_events(trace, ray, tol, true);
    if (trace->physical_event_complete == 1)
	trace->physical_event_vertex_certified++;
    else
	trace->physical_event_vertex_failures++;
    return true;
}


/* Establish a topological edge point on the represented ray line without
 * using model tolerance to move the line.  Prefer an exact 3-D edge crossing.
 * If the edge locus is discrepant, two incident trim lifts that coincide with
 * each other and the ray to roundoff supply the same exact topological
 * witness; the independently certified edge tolerance only authorizes their
 * displacement from the 3-D edge.  A merely near line is never moved. */
static bool
brep_edge_exact_line_witness(const struct brep_specific *bs,
    const struct rt_brep_trace_edge &observation, const ON_Ray &ray,
    ON_3dPoint &edge_point, double &parameter, double &cluster_distance,
    double &cluster_parameter_tolerance,
    double &witness_parameter_tolerance, bool &lift_witness)
{
    lift_witness = false;
    if (!bs || !bs->brep || observation.edge_index < 0 ||
	    observation.edge_index >= bs->brep->m_E.Count() ||
	    !observation.candidate_spans ||
	    !observation.correspondence_screened ||
	    !observation.correspondence_supported ||
	    observation.correspondence_exhausted ||
	    !observation.discrepancy_endpoints_certified ||
	    !observation.discrepancy_bounded ||
	    observation.discrepancy_bound_exhausted ||
	    !observation.discrepancy_authorized ||
	    observation.discrepancy_proof_class != RT_BREP_SEAM_GAP_INSIDE ||
	    !ON_IsValid(observation.edge_tolerance) ||
	    observation.edge_tolerance < 0.0)
	return false;
    const ON_BrepEdge &edge = bs->brep->m_E[observation.edge_index];
    if (edge.m_ti.Count() != 2 || !edge.Domain().IsIncreasing())
	return false;
    ON_3dPoint curve_point;
    ON_3dVector edge_tangent;
    if (!edge.Ev1Der(observation.edge_parameter, curve_point,
	    edge_tangent) || !curve_point.IsValid() || !edge_tangent.Unitize())
	return false;
    const double ray_length_squared = ray.m_dir.LengthSquared();
    if (!(ray_length_squared > DBL_MIN) ||
	    !std::isfinite(ray_length_squared))
	return false;
    parameter = ((curve_point - ray.m_origin) * ray.m_dir) /
	ray_length_squared;
    if (!std::isfinite(parameter))
	return false;
    ON_3dPoint ray_point = ray.m_origin + parameter * ray.m_dir;
    const double coordinate_scale = std::max(1.0,
	std::max(fabs(curve_point.x), std::max(fabs(curve_point.y),
	std::max(fabs(curve_point.z), std::max(fabs(ray.m_origin.x),
	std::max(fabs(ray.m_origin.y), fabs(ray.m_origin.z)))))));
    const double line_roundoff = std::max(ON_ZERO_TOLERANCE,
	4096.0 * DBL_EPSILON * coordinate_scale);
    const bool exact_curve_witness = ray_point.IsValid() &&
	ray_point.DistanceTo(curve_point) <= line_roundoff &&
	std::isfinite(observation.distance) &&
	observation.distance <= line_roundoff;
    if (exact_curve_witness) {
	edge_point = curve_point;
    } else {
	brep_edge_face_frame frame[2];
	for (int side = 0; side < 2; ++side) {
	    const int trim_index = edge.m_ti[side];
	    if (trim_index < 0 || trim_index >= bs->brep->m_T.Count() ||
		    !brep_edge_face_frame_at(*bs->brep, edge,
			bs->brep->m_T[trim_index], observation.edge_parameter,
			edge_tangent, frame[side]) ||
		    frame[side].lift.DistanceTo(curve_point) >
			observation.edge_tolerance + line_roundoff)
		return false;
	    const double lift_parameter = ((frame[side].lift - ray.m_origin) *
		ray.m_dir) / ray_length_squared;
	    const ON_3dPoint lift_ray_point = ray.m_origin +
		lift_parameter * ray.m_dir;
	    if (!std::isfinite(lift_parameter) || !lift_ray_point.IsValid() ||
		    lift_ray_point.DistanceTo(frame[side].lift) > line_roundoff)
		return false;
	}
	if (frame[0].lift.DistanceTo(frame[1].lift) > line_roundoff)
	    return false;
	edge_point = ON_3dPoint(
	    0.5 * (frame[0].lift.x + frame[1].lift.x),
	    0.5 * (frame[0].lift.y + frame[1].lift.y),
	    0.5 * (frame[0].lift.z + frame[1].lift.z));
	parameter = ((edge_point - ray.m_origin) * ray.m_dir) /
	    ray_length_squared;
	ray_point = ray.m_origin + parameter * ray.m_dir;
	if (!std::isfinite(parameter) || !ray_point.IsValid() ||
		ray_point.DistanceTo(edge_point) > line_roundoff)
	    return false;
	lift_witness = true;
    }

    /* Keep vertex neighborhoods out of the edge theorem.  They are governed
     * by the cyclic fan theorem or explicit whole-ray fallback. */
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	const int vertex_index = edge.m_vi[endpoint];
	if (vertex_index < 0 || vertex_index >= bs->brep->m_V.Count() ||
		curve_point.DistanceTo(
		    bs->brep->m_V[vertex_index].point) <=
		observation.edge_tolerance + line_roundoff)
	    return false;
    }

    const double ray_length = sqrt(ray_length_squared);
    cluster_distance = observation.edge_tolerance + line_roundoff;
    witness_parameter_tolerance = std::max(line_roundoff / ray_length,
	4096.0 * DBL_EPSILON * std::max(1.0, fabs(parameter)));
    cluster_parameter_tolerance = std::max(
	cluster_distance / ray_length, witness_parameter_tolerance);
    return std::isfinite(cluster_distance) &&
	std::isfinite(cluster_parameter_tolerance) &&
	std::isfinite(witness_parameter_tolerance);
}


static bool
brep_edge_root_matches_incident(const struct brep_specific *bs,
    const struct rt_brep_trace_edge &observation,
    const struct rt_brep_trace_local_root &root, const ON_Ray &ray,
    const ON_3dPoint &edge_point, double cluster_distance)
{
    if (!bs || !bs->brep || root.face_index < 0 ||
	    root.face_index >= bs->brep->m_F.Count() ||
	    observation.edge_index < 0 ||
	    observation.edge_index >= bs->brep->m_E.Count() ||
	    (root.face_index != observation.face_index[0] &&
	     root.face_index != observation.face_index[1]) ||
	    (root.hit_class == brep_hit::CLEAN_MISS &&
	     root.trim_status != 1))
	return false;
    const ON_3dPoint root_point = ray.m_origin + root.dist * ray.m_dir;
    if (!root_point.IsValid() ||
	    root_point.DistanceTo(edge_point) > cluster_distance)
	return false;
    const ON_BrepEdge &edge = bs->brep->m_E[observation.edge_index];
    ON_3dVector edge_tangent = edge.TangentAt(observation.edge_parameter);
    if (!edge_tangent.Unitize())
	return false;
    for (int side = 0; side < edge.m_ti.Count(); ++side) {
	const int trim_index = edge.m_ti[side];
	if (trim_index < 0 || trim_index >= bs->brep->m_T.Count())
	    return false;
	const ON_BrepTrim &trim = bs->brep->m_T[trim_index];
	if (trim.FaceIndexOf() != root.face_index)
	    continue;
	brep_edge_face_frame frame;
	if (brep_edge_face_frame_at(*bs->brep, edge, trim,
		observation.edge_parameter, edge_tangent, frame) &&
		root_point.DistanceTo(frame.lift) <= cluster_distance)
	    return true;
    }
    return false;
}


static bool
brep_trace_surface_boxes_connected(
    const struct rt_brep_trace_surface_box &first,
    const struct rt_brep_trace_surface_box &second);


/* Prove that a rectangular component hull contains no ray/surface root
 * outside a bounded set of already certified local root boxes.  Splits first
 * align work boxes with every local-box boundary; boxes wholly inside a local
 * certificate are owned, while the remaining complement is excluded by
 * outward Bernstein/rotated-hull bounds and fixed-stack subdivision. */
static bool
brep_surface_component_complement_excluded(
    const brep_surface_coefficients &coefficients,
    const brep_subdivision_box &component,
    const brep_subdivision_box *local_root, size_t local_root_count,
    size_t &visited, size_t &high_water)
{
    static const size_t maximum_visited = 8192;
    brep_subdivision_box pending[BREP_DIRECT_SUBDIVISION_CAPACITY];
    size_t pending_count = 1;
    visited = 0;
    high_water = pending_count;
    if (!local_root || !local_root_count ||
	local_root_count > RT_BREP_TRACE_MAX_LOCAL_ROOTS)
	return false;
    for (size_t root_index = 0; root_index < local_root_count; ++root_index) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (!(component.minimum[direction] <=
		    local_root[root_index].minimum[direction]) ||
		!(local_root[root_index].minimum[direction] <
		    local_root[root_index].maximum[direction]) ||
		!(local_root[root_index].maximum[direction] <=
		    component.maximum[direction]))
		return false;
	}
	for (size_t other_index = 0; other_index < root_index;
		++other_index) {
	    bool interior_overlap = true;
	    for (int direction = 0; direction < 2; ++direction)
		interior_overlap = interior_overlap &&
		    local_root[root_index].minimum[direction] <
			local_root[other_index].maximum[direction] &&
		    local_root[other_index].minimum[direction] <
			local_root[root_index].maximum[direction];
	    if (interior_overlap)
		return false;
    }
    }
    pending[0] = component;

    const size_t coefficient_count = (size_t)coefficients.order[0] *
	coefficients.order[1];
    while (pending_count) {
	const brep_subdivision_box box = pending[--pending_count];
	if (++visited > maximum_visited)
	    return false;
	bool owned = false;
	bool boundary_split = false;
	int split_direction = -1;
	double split_parameter = 0.0;
	for (size_t root_index = 0; root_index < local_root_count;
		++root_index) {
	    bool contained = true;
	    bool overlaps = true;
	    for (int direction = 0; direction < 2; ++direction) {
		contained = contained &&
		    box.minimum[direction] >=
			local_root[root_index].minimum[direction] &&
		    box.maximum[direction] <=
			local_root[root_index].maximum[direction];
		overlaps = overlaps && box.minimum[direction] <
		    local_root[root_index].maximum[direction] &&
		    local_root[root_index].minimum[direction] <
			box.maximum[direction];
	    }
	    if (contained) {
		owned = true;
		break;
	    }
	    if (!overlaps)
		continue;
	    for (int direction = 0; direction < 2 && !boundary_split;
		    ++direction) {
		const double boundary[2] = {
		    local_root[root_index].minimum[direction],
		    local_root[root_index].maximum[direction]
		};
		for (int side = 0; side < 2; ++side) {
		    if (boundary[side] > box.minimum[direction] &&
			    boundary[side] < box.maximum[direction]) {
			split_direction = direction;
			split_parameter = boundary[side];
			boundary_split = true;
			break;
		    }
		}
	    }
	    if (!boundary_split)
		return false;
	    break;
	}
	if (owned)
	    continue;
	if (boundary_split) {
	    if (box.depth >= BREP_DIRECT_SUBDIVISION_MAX_DEPTH ||
		    pending_count + 2 > BREP_DIRECT_SUBDIVISION_CAPACITY)
		return false;
	    brep_subdivision_box &upper = pending[pending_count++];
	    upper = box;
	    upper.minimum[split_direction] = split_parameter;
	    upper.depth++;
	    brep_subdivision_box &lower = pending[pending_count++];
	    lower = box;
	    lower.maximum[split_direction] = split_parameter;
	    lower.depth++;
	    high_water = std::max(high_water, pending_count);
	    continue;
	}
	double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
	double restricted_error[2] = {0.0, 0.0};
	bool excluded = false;
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_scalar_surface_restrict_bounded(
		    coefficients.value[equation], coefficients.order[0],
		    coefficients.order[1], coefficients.error[equation],
		    box.minimum[0], box.maximum[0], box.minimum[1],
		    box.maximum[1], restricted[equation],
		    restricted_error[equation]))
		return false;
	    if (brep_coefficient_hull_excluded(restricted[equation],
		    coefficient_count, restricted_error[equation])) {
		excluded = true;
		break;
	    }
	}
	if (excluded || brep_rotated_surface_hull_status(restricted,
		coefficients.order, restricted_error) ==
		    BREP_ROTATED_HULL_EXCLUDED)
	    continue;
	if (box.depth >= BREP_DIRECT_SUBDIVISION_MAX_DEPTH ||
		pending_count + 2 > BREP_DIRECT_SUBDIVISION_CAPACITY)
	    return false;
	int direction = box.maximum[0] - box.minimum[0] >=
	    box.maximum[1] - box.minimum[1] ? 0 : 1;
	const double midpoint = 0.5 * box.minimum[direction] +
	    0.5 * box.maximum[direction];
	if (!(midpoint > box.minimum[direction]) ||
		!(midpoint < box.maximum[direction]))
	    return false;
	brep_subdivision_box &upper = pending[pending_count++];
	upper = box;
	upper.minimum[direction] = midpoint;
	upper.depth++;
	brep_subdivision_box &lower = pending[pending_count++];
	lower = box;
	lower.maximum[direction] = midpoint;
	lower.depth++;
	high_water = std::max(high_water, pending_count);
    }
    return true;
}


/* Certify one connected many-boxes-to-one-root component away from a
 * topological boundary event.  The source roots are immutable observations;
 * authority comes from strict Krawczyk inclusion over the complete connected
 * terminal-box hull and a constant determinant sign over that same hull.
 * Rootless unresolved boxes may join only through face/span/UV/t adjacency.
 * Every array is caller-owned and fixed-capacity. */
static bool
brep_trace_regular_root_component(
    const struct rt_brep_shot_trace *trace, const struct brep_specific *bs,
    const ON_Ray &ray, const struct bn_tol *tol,
    const bool component_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS],
    const bool unavailable_box[RT_BREP_TRACE_MAX_SURFACE_BOXES],
    bool component_box[RT_BREP_TRACE_MAX_SURFACE_BOXES],
    size_t &first_box, size_t &root_count, size_t &box_count,
    double &t_minimum, double &t_maximum, int &determinant_sign,
    int &failure_stage)
{
    first_box = (size_t)-1;
    root_count = 0;
    box_count = 0;
    t_minimum = DBL_MAX;
    t_maximum = -DBL_MAX;
    determinant_sign = 0;
    failure_stage = 1;
    if (!trace || !bs || !component_root || !unavailable_box ||
	!component_box || !bs->brep ||
	trace->stored_local_roots > RT_BREP_TRACE_MAX_LOCAL_ROOTS ||
	trace->stored_surface_boxes > RT_BREP_TRACE_MAX_SURFACE_BOXES)
	return false;

    size_t canonical_root = (size_t)-1;
    int face_index = -1;
    int span_index = -1;
    int direction = -1;
    for (size_t root_index = 0;
	    root_index < trace->stored_local_roots; ++root_index) {
	if (!component_root[root_index])
	    continue;
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	if (canonical_root == (size_t)-1) {
	    canonical_root = root_index;
	    face_index = root.face_index;
	    span_index = root.span_index;
	    direction = root.direction;
	} else if (root.face_index != face_index ||
		root.span_index != span_index || root.direction != direction) {
	    return false;
	}
	if (root.face_index < 0 || root.span_index < 0 ||
		!std::isfinite(root.dist) || !std::isfinite(root.uv[0]) ||
		!std::isfinite(root.uv[1]) ||
		!std::isfinite(root.normal_dot) ||
		fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL)
	    return false;
	root_count++;
    }
    if (canonical_root == (size_t)-1 || !root_count ||
	(size_t)span_index >= bs->surface_spans.size())
	return false;
    const brep_surface_span &span = bs->surface_spans[span_index];
    if (span.face_index != face_index ||
	!span.surface_domain[0].IsIncreasing() ||
	!span.surface_domain[1].IsIncreasing())
	return false;

    failure_stage = 2;
    bool seed_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool candidate_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index) {
	component_box[box_index] = false;
	const struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	bool matches = false;
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index) {
	    if (component_root[root_index] &&
		    brep_prepared_box_matches_local_root(box,
			trace->local_roots[root_index], ray, tol)) {
		matches = true;
		break;
	    }
	}
	if (matches) {
	    const bool unresolved =
		box.disposition == RT_BREP_TRACE_BOX_UNRESOLVED &&
		!box.determinant_sign;
	    const bool regular =
		box.disposition == RT_BREP_TRACE_BOX_RESOLVED_REGULAR &&
		box.determinant_sign;
	    if (unavailable_box[box_index] || (!unresolved && !regular))
		return false;
	    seed_box[box_index] = true;
	    component_box[box_index] = true;
	    box_count++;
	    continue;
	}
	if (!unavailable_box[box_index] && box.face_index == face_index &&
		box.span_index == span_index &&
		box.disposition == RT_BREP_TRACE_BOX_UNRESOLVED &&
		!box.determinant_sign &&
		!brep_prepared_box_has_root(trace, box, ray, tol))
	    candidate_box[box_index] = true;
    }
    if (!box_count)
	return false;

    bool changed = true;
    while (changed) {
	changed = false;
	for (size_t candidate_index = 0;
		candidate_index < trace->stored_surface_boxes;
		++candidate_index) {
	    if (!candidate_box[candidate_index] ||
		    component_box[candidate_index])
		continue;
	    for (size_t component_index = 0;
		    component_index < trace->stored_surface_boxes;
		    ++component_index) {
		if (!component_box[component_index] ||
			!brep_trace_surface_boxes_connected(
			    trace->surface_boxes[candidate_index],
			    trace->surface_boxes[component_index]))
		    continue;
		component_box[candidate_index] = true;
		box_count++;
		changed = true;
		break;
	    }
	}
    }

    double uv_minimum[2] = {DBL_MAX, DBL_MAX};
    double uv_maximum[2] = {-DBL_MAX, -DBL_MAX};
    double stored_t_minimum = DBL_MAX;
    double stored_t_maximum = -DBL_MAX;
    size_t connected_seeds = 0;
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index) {
	if (!component_box[box_index])
	    continue;
	const struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	if (first_box == (size_t)-1)
	    first_box = box_index;
	connected_seeds += seed_box[box_index] ? 1 : 0;
	for (int parameter = 0; parameter < 2; ++parameter) {
	    uv_minimum[parameter] = std::min(uv_minimum[parameter],
		(double)box.uv_min[parameter]);
	    uv_maximum[parameter] = std::max(uv_maximum[parameter],
		(double)box.uv_max[parameter]);
	}
	stored_t_minimum = std::min(stored_t_minimum, (double)box.t_min);
	stored_t_maximum = std::max(stored_t_maximum, (double)box.t_max);
    }
    if (first_box == (size_t)-1 || connected_seeds == 0)
	return false;
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index)
	if (seed_box[box_index] && !component_box[box_index])
	    return false;

    failure_stage = 3;
    brep_subdivision_box hull = {};
    double local_root[2];
    const struct rt_brep_trace_local_root &canonical =
	trace->local_roots[canonical_root];
    for (int parameter = 0; parameter < 2; ++parameter) {
	hull.minimum[parameter] = std::max(0.0, std::nextafter(
	    span.surface_domain[parameter].NormalizedParameterAt(
		uv_minimum[parameter]), -INFINITY));
	hull.maximum[parameter] = std::min(1.0, std::nextafter(
	    span.surface_domain[parameter].NormalizedParameterAt(
		uv_maximum[parameter]), INFINITY));
	const double root_parameter = span.surface_domain[parameter].
	    NormalizedParameterAt(canonical.uv[parameter]);
	if (!std::isfinite(hull.minimum[parameter]) ||
		!std::isfinite(hull.maximum[parameter]) ||
		!std::isfinite(root_parameter) ||
		!(hull.minimum[parameter] < hull.maximum[parameter]) ||
		root_parameter < hull.minimum[parameter] ||
		root_parameter > hull.maximum[parameter])
	    return false;
	local_root[parameter] = (root_parameter - hull.minimum[parameter]) /
	    (hull.maximum[parameter] - hull.minimum[parameter]);
    }

    ON_3dVector first;
    ON_3dVector second;
    brep_surface_coefficients coefficients;
    if (!brep_ray_plane_frame(ray, first, second) ||
	!brep_surface_coefficients_init(coefficients, span, ray, first, second))
	return false;
    double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
    double restricted_error[2] = {0.0, 0.0};
    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_scalar_surface_restrict_bounded(
		coefficients.value[equation], coefficients.order[0],
		coefficients.order[1], coefficients.error[equation],
		hull.minimum[0], hull.maximum[0], hull.minimum[1],
		hull.maximum[1], restricted[equation],
		restricted_error[equation]))
	    return false;
    }
    failure_stage = 4;
    bool localized = false;
    if (!brep_surface_krawczyk_certified(restricted,
	    coefficients.order[0], coefficients.order[1], restricted_error,
	    local_root)) {
	double normalized_root[2];
	double clearance = DBL_MAX;
	for (int parameter = 0; parameter < 2; ++parameter) {
	    normalized_root[parameter] = span.surface_domain[parameter].
		NormalizedParameterAt(canonical.uv[parameter]);
	    clearance = std::min(clearance, std::min(
		normalized_root[parameter] - hull.minimum[parameter],
		hull.maximum[parameter] - normalized_root[parameter]));
	}
	const double maximum_radius = std::nextafter(0.5 * clearance, 0.0);
	struct rt_brep_local_root_test_result local = {};
	if (!(maximum_radius > 0.0) || !std::isfinite(maximum_radius) ||
		!brep_surface_local_root_certificate(span, ray, canonical.uv,
		    maximum_radius, local) || !local.available ||
		!local.certified || !(local.radius > 0.0) ||
		!(local.radius < maximum_radius))
	    return false;
	brep_subdivision_box local_box = {};
	for (int parameter = 0; parameter < 2; ++parameter) {
	    local_box.minimum[parameter] = std::nextafter(
		normalized_root[parameter] - local.radius, -INFINITY);
	    local_box.maximum[parameter] = std::nextafter(
		normalized_root[parameter] + local.radius, INFINITY);
	    if (!(local_box.minimum[parameter] > hull.minimum[parameter]) ||
		    !(local_box.maximum[parameter] < hull.maximum[parameter]))
		return false;
	}
	size_t complement_visited = 0;
	size_t complement_high_water = 0;
	if (!brep_surface_component_complement_excluded(coefficients, hull,
		&local_box, 1, complement_visited, complement_high_water))
	    return false;
	hull = local_box;
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_scalar_surface_restrict_bounded(
		    coefficients.value[equation], coefficients.order[0],
		    coefficients.order[1], coefficients.error[equation],
		    hull.minimum[0], hull.maximum[0], hull.minimum[1],
		    hull.maximum[1], restricted[equation],
		    restricted_error[equation]))
		return false;
	}
	localized = true;
    }
    failure_stage = 5;
    if (!brep_surface_determinant_sign(restricted, coefficients.order,
	    restricted_error, determinant_sign) || !determinant_sign)
	return false;
    failure_stage = 6;
    if (!brep_surface_box_t_range(coefficients, hull, t_minimum, t_maximum))
	return false;

    const double t_scale = std::max(1.0,
	std::max(fabs(t_minimum), fabs(t_maximum)));
    const double t_roundoff = 512.0 * DBL_EPSILON * t_scale;
    if (canonical.dist < t_minimum - t_roundoff ||
	canonical.dist > t_maximum + t_roundoff ||
	(!localized && (stored_t_minimum < t_minimum - t_roundoff ||
	 stored_t_maximum > t_maximum + t_roundoff)))
	return false;
    failure_stage = 0;
    return true;
}


/* Split a connected coarse component that jointly contains one exact
 * manifold-edge observation and one nearby regular root.  Independent local
 * contraction boxes certify both surface roots; fixed-stack Bernstein work
 * excludes their complete complement.  Both local boxes supply constant
 * orientations; the regular box also supplies a tight t enclosure. */
static bool
brep_trace_edge_joint_root_component(
    const struct rt_brep_shot_trace *trace, const struct brep_specific *bs,
    const ON_Ray &ray, const struct bn_tol *tol, size_t boundary_root,
    const bool regular_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS],
    bool component_box[RT_BREP_TRACE_MAX_SURFACE_BOXES],
    size_t &first_box, size_t &regular_roots, size_t &box_count,
    double &regular_t_minimum, double &regular_t_maximum,
    int &boundary_determinant_sign, int &determinant_sign,
    size_t &complement_visited,
    size_t &complement_high_water, int &failure_stage)
{
    first_box = (size_t)-1;
    regular_roots = 0;
    box_count = 0;
    regular_t_minimum = DBL_MAX;
    regular_t_maximum = -DBL_MAX;
    boundary_determinant_sign = 0;
    determinant_sign = 0;
    complement_visited = 0;
    complement_high_water = 0;
    failure_stage = 1;
    if (!trace || !bs || !bs->brep || !regular_root || !component_box ||
	boundary_root >= trace->stored_local_roots)
	return false;
    const struct rt_brep_trace_local_root &boundary =
	trace->local_roots[boundary_root];
    size_t canonical_regular = (size_t)-1;
    for (size_t root_index = 0;
	    root_index < trace->stored_local_roots; ++root_index) {
	if (!regular_root[root_index])
	    continue;
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	if (canonical_regular == (size_t)-1)
	    canonical_regular = root_index;
	if (root.face_index != boundary.face_index ||
		root.span_index != boundary.span_index ||
		root.direction == boundary.direction ||
		!std::isfinite(root.normal_dot) ||
		fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL)
	    return false;
	regular_roots++;
    }
    if (canonical_regular == (size_t)-1 || !regular_roots ||
	boundary.face_index < 0 || boundary.span_index < 0 ||
	(size_t)boundary.span_index >= bs->surface_spans.size() ||
	!std::isfinite(boundary.normal_dot) ||
	fabs(boundary.normal_dot) <= BREP_GRAZING_DOT_TOL)
	return false;
    const struct rt_brep_trace_local_root &regular =
	trace->local_roots[canonical_regular];
    const brep_surface_span &span = bs->surface_spans[boundary.span_index];
    if (span.face_index != boundary.face_index ||
	!span.surface_domain[0].IsIncreasing() ||
	!span.surface_domain[1].IsIncreasing())
	return false;

    failure_stage = 2;
    bool seed_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool candidate_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool shared_box = false;
    size_t seed_count = 0;
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index) {
	component_box[box_index] = false;
	const struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	const bool unresolved =
	    box.disposition == RT_BREP_TRACE_BOX_UNRESOLVED &&
	    !box.determinant_sign;
	const bool resolved =
	    box.disposition == RT_BREP_TRACE_BOX_RESOLVED_REGULAR &&
	    box.determinant_sign;
	const bool boundary_match = brep_prepared_box_matches_local_root(
	    box, boundary, ray, tol);
	bool regular_match = false;
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index) {
	    if (regular_root[root_index] &&
		    brep_prepared_box_matches_local_root(box,
			trace->local_roots[root_index], ray, tol)) {
		regular_match = true;
		break;
	    }
	}
	if (boundary_match || regular_match) {
	    if ((!unresolved && !resolved) ||
		box.face_index != boundary.face_index ||
		box.span_index != boundary.span_index)
		return false;
	    seed_box[box_index] = true;
	    component_box[box_index] = true;
	    seed_count++;
	    shared_box = shared_box || (boundary_match && regular_match);
	    continue;
	}
	if (box.face_index == boundary.face_index &&
		box.span_index == boundary.span_index && unresolved &&
		!brep_prepared_box_has_root(trace, box, ray, tol))
	    candidate_box[box_index] = true;
    }
    if (!seed_count || !shared_box)
	return false;

    bool changed = true;
    while (changed) {
	changed = false;
	for (size_t candidate_index = 0;
		candidate_index < trace->stored_surface_boxes;
		++candidate_index) {
	    if (!candidate_box[candidate_index] ||
		    component_box[candidate_index])
		continue;
	    for (size_t component_index = 0;
		    component_index < trace->stored_surface_boxes;
		    ++component_index) {
		if (!component_box[component_index] ||
			!brep_trace_surface_boxes_connected(
			    trace->surface_boxes[candidate_index],
			    trace->surface_boxes[component_index]))
		    continue;
		component_box[candidate_index] = true;
		changed = true;
		break;
	    }
	}
    }

    double uv_minimum[2] = {DBL_MAX, DBL_MAX};
    double uv_maximum[2] = {-DBL_MAX, -DBL_MAX};
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index) {
	if (seed_box[box_index] && !component_box[box_index])
	    return false;
	if (!component_box[box_index])
	    continue;
	if (first_box == (size_t)-1)
	    first_box = box_index;
	box_count++;
	for (int parameter = 0; parameter < 2; ++parameter) {
	    uv_minimum[parameter] = std::min(uv_minimum[parameter],
		(double)trace->surface_boxes[box_index].uv_min[parameter]);
	    uv_maximum[parameter] = std::max(uv_maximum[parameter],
		(double)trace->surface_boxes[box_index].uv_max[parameter]);
	}
    }
    if (first_box == (size_t)-1 || !box_count)
	return false;

    failure_stage = 3;
    brep_subdivision_box hull = {};
    double normalized[2][2];
    const struct rt_brep_trace_local_root *root[2] = {&boundary, &regular};
    for (int parameter = 0; parameter < 2; ++parameter) {
	hull.minimum[parameter] = std::max(0.0, std::nextafter(
	    span.surface_domain[parameter].NormalizedParameterAt(
		uv_minimum[parameter]), -INFINITY));
	hull.maximum[parameter] = std::min(1.0, std::nextafter(
	    span.surface_domain[parameter].NormalizedParameterAt(
		uv_maximum[parameter]), INFINITY));
	if (!(hull.minimum[parameter] < hull.maximum[parameter]))
	    return false;
	for (int root_index = 0; root_index < 2; ++root_index) {
	    normalized[root_index][parameter] = span.surface_domain[parameter].
		NormalizedParameterAt(root[root_index]->uv[parameter]);
	    if (!std::isfinite(normalized[root_index][parameter]) ||
		    normalized[root_index][parameter] < 0.0 ||
		    normalized[root_index][parameter] > 1.0)
		return false;
	}
    }
    double separation = 0.0;
    for (int parameter = 0; parameter < 2; ++parameter)
	separation = std::max(separation, fabs(normalized[0][parameter] -
	    normalized[1][parameter]));
    const double maximum_radius = std::nextafter(0.2 * separation, 0.0);
    if (!(maximum_radius > 0.0) || !std::isfinite(maximum_radius))
	return false;

    brep_subdivision_box local_box[2] = {};
    struct rt_brep_local_root_test_result local_certificate[2] = {};
    for (int root_index = 0; root_index < 2; ++root_index) {
	if (!brep_surface_local_root_certificate_mode(span, ray,
		    root[root_index]->uv, maximum_radius, true,
		    local_certificate[root_index]) ||
		!local_certificate[root_index].available ||
		!local_certificate[root_index].certified ||
		!(local_certificate[root_index].radius > 0.0) ||
		!(local_certificate[root_index].radius < maximum_radius))
	    return false;
	for (int parameter = 0; parameter < 2; ++parameter) {
	    const double local_minimum = std::nextafter(
		normalized[root_index][parameter] -
		local_certificate[root_index].radius, -INFINITY);
	    const double local_maximum = std::nextafter(
		normalized[root_index][parameter] +
		local_certificate[root_index].radius, INFINITY);
	    local_box[root_index].minimum[parameter] = std::max(
		hull.minimum[parameter], local_minimum);
	    local_box[root_index].maximum[parameter] = std::min(
		hull.maximum[parameter], local_maximum);
	    if (!(local_box[root_index].minimum[parameter] <
		    local_box[root_index].maximum[parameter]) ||
		    normalized[root_index][parameter] <
			local_box[root_index].minimum[parameter] ||
		    normalized[root_index][parameter] >
			local_box[root_index].maximum[parameter])
		return false;
	}
    }
    for (size_t root_index = 0;
	    root_index < trace->stored_local_roots; ++root_index) {
	if (root_index == boundary_root || regular_root[root_index])
	    continue;
	const struct rt_brep_trace_local_root &other =
	    trace->local_roots[root_index];
	if (other.face_index != boundary.face_index ||
		other.span_index != boundary.span_index)
	    continue;
	double other_normalized[2];
	bool inside = true;
	for (int parameter = 0; parameter < 2; ++parameter) {
	    other_normalized[parameter] = span.surface_domain[parameter].
		NormalizedParameterAt(other.uv[parameter]);
	    inside = inside && other_normalized[parameter] >=
		hull.minimum[parameter] && other_normalized[parameter] <=
		hull.maximum[parameter];
	}
	if (inside)
	    return false;
    }

    ON_3dVector first;
    ON_3dVector second;
    brep_surface_coefficients coefficients;
    if (!brep_ray_plane_frame(ray, first, second) ||
	!brep_surface_coefficients_init(coefficients, span, ray, first, second))
	return false;
    failure_stage = 4;
    if (!brep_surface_component_complement_excluded(coefficients, hull,
	    local_box, 2, complement_visited, complement_high_water))
	return false;

    failure_stage = 5;
    double boundary_restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
    double boundary_error[2] = {0.0, 0.0};
    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_scalar_surface_restrict_bounded(
		coefficients.value[equation], coefficients.order[0],
		coefficients.order[1], coefficients.error[equation],
		local_box[0].minimum[0], local_box[0].maximum[0],
		local_box[0].minimum[1], local_box[0].maximum[1],
		boundary_restricted[equation], boundary_error[equation]))
	    return false;
    }
    if (!brep_surface_determinant_sign(boundary_restricted,
	    coefficients.order, boundary_error, boundary_determinant_sign) ||
	    !boundary_determinant_sign)
	return false;

    failure_stage = 6;
    const double root_scale = std::max(1.0,
	std::max(fabs(normalized[1][0]), fabs(normalized[1][1])));
    double event_radius = std::max(std::nextafter(
	4.0 * local_certificate[1].correction_bound, INFINITY),
	1024.0 * DBL_EPSILON * root_scale);
    bool event_certified = false;
    for (size_t attempt = 0; attempt < 48 &&
	    event_radius <= local_certificate[1].radius; ++attempt) {
	brep_subdivision_box event_box = {};
	double event_root[2];
	for (int parameter = 0; parameter < 2; ++parameter) {
	    event_box.minimum[parameter] = std::max(hull.minimum[parameter],
		std::nextafter(normalized[1][parameter] - event_radius,
		    -INFINITY));
	    event_box.maximum[parameter] = std::min(hull.maximum[parameter],
		std::nextafter(normalized[1][parameter] + event_radius,
		    INFINITY));
	    if (!(event_box.minimum[parameter] <
		    event_box.maximum[parameter]))
		return false;
	    event_root[parameter] = (normalized[1][parameter] -
		event_box.minimum[parameter]) /
		(event_box.maximum[parameter] - event_box.minimum[parameter]);
	}
	double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
	double restricted_error[2] = {0.0, 0.0};
	bool usable = true;
	for (int equation = 0; equation < 2 && usable; ++equation)
	    usable = brep_scalar_surface_restrict_bounded(
		coefficients.value[equation], coefficients.order[0],
		coefficients.order[1], coefficients.error[equation],
		event_box.minimum[0], event_box.maximum[0],
		event_box.minimum[1], event_box.maximum[1],
		restricted[equation], restricted_error[equation]);
	int event_sign = 0;
	double event_t_minimum = DBL_MAX;
	double event_t_maximum = -DBL_MAX;
	usable = usable && brep_surface_krawczyk_certified(restricted,
	    coefficients.order[0], coefficients.order[1], restricted_error,
	    event_root) && brep_surface_determinant_sign(restricted,
	    coefficients.order, restricted_error, event_sign) && event_sign &&
	    brep_surface_box_t_range(coefficients, event_box,
		event_t_minimum, event_t_maximum);
	const double t_roundoff = 512.0 * DBL_EPSILON * std::max(1.0,
	    std::max(fabs(event_t_minimum), fabs(event_t_maximum)));
	usable = usable && regular.dist >= event_t_minimum - t_roundoff &&
	    regular.dist <= event_t_maximum + t_roundoff;
	for (size_t root_index = 0; usable &&
		root_index < trace->stored_local_roots; ++root_index) {
	    if (!regular_root[root_index])
		continue;
	    const struct rt_brep_trace_local_root &duplicate =
		trace->local_roots[root_index];
	    for (int parameter = 0; parameter < 2; ++parameter) {
		const double value = span.surface_domain[parameter].
		    NormalizedParameterAt(duplicate.uv[parameter]);
		usable = usable && value >= event_box.minimum[parameter] &&
		    value <= event_box.maximum[parameter];
	    }
	}
	if (usable) {
	    determinant_sign = event_sign;
	    regular_t_minimum = event_t_minimum;
	    regular_t_maximum = event_t_maximum;
	    event_certified = true;
	    break;
	}
	if (!(event_radius < local_certificate[1].radius))
	    break;
	if (event_radius > 0.5 * local_certificate[1].radius)
	    event_radius = local_certificate[1].radius;
	else
	    event_radius *= 2.0;
    }
    if (!event_certified)
	return false;
    failure_stage = 0;
    return true;
}


/* Resolve exact interior crossings of certified two-trim manifold edges.
 * Incident surface roots are observations of one topological event.  The
 * oriented cross-section decides ENTER/LEAVE/contact, and every actual root
 * and terminal box is owned before any disposition or event is committed. */
static bool
brep_trace_edge_physical_events(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    if (!trace || !bs || !bs->brep || !bs->is_solid || bs->plate_mode)
	return false;
    const bool workspace_complete = trace->prepared_surface_spans &&
	!trace->unsupported_surface_faces &&
	trace->supported_surface_faces == bs->face_records.size() &&
	trace->candidate_surface_spans + trace->excluded_surface_spans ==
	    trace->prepared_surface_spans &&
	!trace->surface_workspace_exhausted &&
	!trace->surface_clip_restriction_failures &&
	!trace->surface_box_overflow &&
	trace->surface_isolated_boxes == trace->stored_surface_boxes &&
	!trace->local_root_overflow && !trace->local_trim_failures &&
	trace->local_root_candidates == trace->stored_local_roots &&
	trace->stored_surface_boxes && trace->stored_local_roots &&
	!trace->stored_physical_events;
    if (!workspace_complete)
	return false;

    struct rt_brep_shot_trace edge_trace = {};
    edge_trace.closure_edge_index = -1;
    edge_trace.closure_missing_direction = -1;
    edge_trace.continuation_face_index = -1;
    edge_trace.continuation_span_index = -1;
    edge_trace.continuation_adjacent_face_index = -99;
    brep_observe_edges(&edge_trace, bs, ray);
    if (edge_trace.edge_overflow || edge_trace.edge_evaluation_failures ||
	    edge_trace.edge_observations != edge_trace.stored_edges)
	return false;

    struct edge_event_candidate {
	size_t observation = 0;
	size_t canonical_root = 0;
	size_t first_box = 0;
	size_t roots = 0;
	size_t boxes = 0;
	double parameter = 0.0;
	double t_min = 0.0;
	double t_max = 0.0;
	double witness_parameter_tolerance = 0.0;
	int direction = -1;
	bool contact = false;
	bool lift_witness = false;
	bool requires_joint_transition = false;
    } candidates[RT_BREP_TRACE_MAX_PHYSICAL_EVENTS];
    bool selected_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    bool selected_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool selected_contact_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    size_t selected_roots = 0;
    size_t selected_boxes = 0;
    size_t candidate_count = 0;
    bool candidate_conflict = false;
    bool saw_exact_edge = false;
    bool exact_edge_unresolved = false;
    bool tolerance_transition_candidate = false;
    int candidate_failure_stage = 0;

    for (size_t observation_index = 0;
	    observation_index < edge_trace.stored_edges;
	    ++observation_index) {
	const struct rt_brep_trace_edge &observation =
	    edge_trace.edges[observation_index];
	ON_3dPoint edge_point;
	double parameter = 0.0;
	double cluster_distance = 0.0;
	double cluster_parameter_tolerance = 0.0;
	double witness_parameter_tolerance = 0.0;
	bool lift_witness = false;
	if (!brep_edge_exact_line_witness(bs, observation, ray, edge_point,
		parameter, cluster_distance, cluster_parameter_tolerance,
		witness_parameter_tolerance, lift_witness))
	    continue;
	saw_exact_edge = true;
	if (!observation.sector_valid || !observation.line_state_valid) {
	    candidate_failure_stage = std::max(candidate_failure_stage, 1);
	    exact_edge_unresolved = true;
	    continue;
	}
	const int direction = observation.line_transition_direction;
	const bool contact = direction != brep_hit::ENTERING &&
	    direction != brep_hit::LEAVING;
	if (contact && observation.line_before_state !=
		observation.line_after_state) {
	    candidate_failure_stage = std::max(candidate_failure_stage, 2);
	    continue;
	}

	bool candidate_roots[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
	bool candidate_boxes[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
	size_t canonical_root = (size_t)-1;
	size_t first_box = (size_t)-1;
	size_t root_count = 0;
	size_t box_count = 0;
	double t_min = parameter;
	double t_max = parameter;
	bool complete = true;
	bool root_direction_consistent = true;
	int incident_root_direction = -1;
	int face_root_count[2] = {0, 0};
	for (size_t root_index = 0; complete &&
		root_index < trace->stored_local_roots; ++root_index) {
	    const struct rt_brep_trace_local_root &root =
		trace->local_roots[root_index];
	    const ON_3dPoint root_point = ray.m_origin +
		root.dist * ray.m_dir;
	    if (!root_point.IsValid() ||
		    root_point.DistanceTo(edge_point) > cluster_distance)
		continue;
	    if (!brep_edge_root_matches_incident(bs, observation, root, ray,
		    edge_point, cluster_distance) ||
		    fabs(root.dist - parameter) >
			cluster_parameter_tolerance)
		continue;

	    bool root_boxes[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
	    size_t box_matches = 0;
	    for (size_t box_index = 0;
		    box_index < trace->stored_surface_boxes; ++box_index) {
		const struct rt_brep_trace_surface_box &box =
		    trace->surface_boxes[box_index];
		if (!brep_prepared_box_matches_local_root(box, root, ray, tol) ||
			parameter < box.t_min - witness_parameter_tolerance ||
			parameter > box.t_max + witness_parameter_tolerance)
		    continue;
		const bool unresolved =
		    box.disposition == RT_BREP_TRACE_BOX_UNRESOLVED &&
		    !box.determinant_sign;
		const bool regular =
		    box.disposition == RT_BREP_TRACE_BOX_RESOLVED_REGULAR &&
		    box.determinant_sign;
		if (candidate_boxes[box_index] || (!unresolved && !regular)) {
		    candidate_failure_stage = std::max(candidate_failure_stage, 3);
		    complete = false;
		    break;
		}
		root_boxes[box_index] = true;
		box_matches++;
	    }
	    if (!complete)
		break;
	    if (!box_matches)
		continue;
	    if (!std::isfinite(root.normal_dot) ||
		    fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL ||
		    (!contact && root.direction != direction)) {
		candidate_failure_stage = std::max(candidate_failure_stage, 4);
		complete = false;
		break;
	    }
	    int face_slot = root.face_index == observation.face_index[0] ?
		0 : 1;
	    if (face_root_count[face_slot] &&
		(canonical_root == (size_t)-1 ||
		 root.direction !=
		     trace->local_roots[canonical_root].direction)) {
		candidate_failure_stage = std::max(candidate_failure_stage, 5);
		complete = false;
		break;
	    }
	    if (incident_root_direction == -1)
		incident_root_direction = root.direction;
	    else if (incident_root_direction != root.direction)
		root_direction_consistent = false;
	    face_root_count[face_slot]++;
	    for (size_t box_index = 0;
		    box_index < trace->stored_surface_boxes; ++box_index) {
		if (!root_boxes[box_index])
		    continue;
		const struct rt_brep_trace_surface_box &box =
		    trace->surface_boxes[box_index];
		candidate_boxes[box_index] = true;
		if (first_box == (size_t)-1)
		    first_box = box_index;
		t_min = std::min(t_min, (double)box.t_min);
		t_max = std::max(t_max, (double)box.t_max);
	    }
	    candidate_roots[root_index] = true;
	    root_count++;
	    box_count += box_matches;
	    if (canonical_root == (size_t)-1 ||
		    ((root.hit_class == brep_hit::CLEAN_HIT ||
		      root.hit_class == brep_hit::NEAR_HIT) &&
		     (trace->local_roots[canonical_root].hit_class !=
			 brep_hit::CLEAN_HIT &&
		      trace->local_roots[canonical_root].hit_class !=
			 brep_hit::NEAR_HIT)))
		canonical_root = root_index;
	}
	for (size_t box_index = 0; complete &&
		box_index < trace->stored_surface_boxes; ++box_index) {
	    const struct rt_brep_trace_surface_box &box =
		trace->surface_boxes[box_index];
	    if (box.t_max < parameter - witness_parameter_tolerance ||
		    box.t_min > parameter + witness_parameter_tolerance)
		continue;
	    if ((box.face_index == observation.face_index[0] ||
		 box.face_index == observation.face_index[1]) &&
		!candidate_boxes[box_index]) {
		candidate_failure_stage = std::max(candidate_failure_stage, 6);
		complete = false;
	    }
	}
	if (!complete || !root_count || !box_count ||
		canonical_root == (size_t)-1 || first_box == (size_t)-1) {
	    if (!candidate_failure_stage)
		candidate_failure_stage = 7;
	    if (!contact || !complete || root_count || box_count)
		exact_edge_unresolved = true;
	    continue;
	}

	trace->physical_event_edge_candidates++;
	if (candidate_count >= RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	    candidate_failure_stage = std::max(candidate_failure_stage, 8);
	    candidate_conflict = true;
	    continue;
	}
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index)
	    if (candidate_roots[root_index] && selected_root[root_index])
		candidate_conflict = true;
	for (size_t box_index = 0;
		box_index < trace->stored_surface_boxes; ++box_index)
	    if (candidate_boxes[box_index] && selected_box[box_index])
		candidate_conflict = true;
	if (candidate_conflict) {
	    candidate_failure_stage = std::max(candidate_failure_stage, 8);
	    continue;
	}
	/* A sub-tolerance trim displacement can make the literal trim-frame
	 * wedge call an exact edge line a contact even though both incident
	 * surface roots consistently cross the represented solid boundary.  Keep
	 * that transition provisional: it may publish only after the joint local
	 * theorem separates and certifies the nearby opposite regular root. */
	const bool tolerance_transition = contact && root_direction_consistent &&
	    (incident_root_direction == brep_hit::ENTERING ||
	     incident_root_direction == brep_hit::LEAVING) &&
	    face_root_count[0] > 0 && face_root_count[1] > 0;
	edge_event_candidate &candidate = candidates[candidate_count++];
	candidate.observation = observation_index;
	candidate.canonical_root = canonical_root;
	candidate.first_box = first_box;
	candidate.roots = root_count;
	candidate.boxes = box_count;
	candidate.parameter = parameter;
	candidate.t_min = t_min;
	candidate.t_max = t_max;
	candidate.witness_parameter_tolerance =
	    witness_parameter_tolerance;
	candidate.direction = tolerance_transition ? incident_root_direction :
	    direction;
	candidate.contact = contact && !tolerance_transition;
	candidate.lift_witness = lift_witness;
	candidate.requires_joint_transition = tolerance_transition;
	tolerance_transition_candidate = tolerance_transition_candidate ||
	    tolerance_transition;
	selected_roots += root_count;
	selected_boxes += box_count;
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index)
	    if (candidate_roots[root_index])
		selected_root[root_index] = true;
	for (size_t box_index = 0;
		box_index < trace->stored_surface_boxes; ++box_index) {
	    if (!candidate_boxes[box_index])
		continue;
	    selected_box[box_index] = true;
	    selected_contact_box[box_index] = candidate.contact;
	}
    }
    if (!saw_exact_edge)
	return false;
    trace->physical_event_edge_attempts++;
    if (!candidate_count || candidate_conflict || exact_edge_unresolved) {
	trace->physical_event_edge_candidate_failure_stage =
	    candidate_failure_stage;
	trace->physical_event_edge_failures++;
	return true;
    }

    struct rt_brep_trace_physical_event
	events[RT_BREP_TRACE_MAX_PHYSICAL_EVENTS] = {};
    size_t event_count = 0;
    size_t transition_count = 0;
    size_t contact_count = 0;
    size_t lift_witness_count = 0;
    size_t candidate_event[RT_BREP_TRACE_MAX_PHYSICAL_EVENTS];
    for (size_t candidate_index = 0;
	    candidate_index < RT_BREP_TRACE_MAX_PHYSICAL_EVENTS;
	    ++candidate_index)
	candidate_event[candidate_index] = (size_t)-1;
    bool root_owned[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    for (size_t root_index = 0; root_index < trace->stored_local_roots;
	    ++root_index)
	root_owned[root_index] = selected_root[root_index];

    for (size_t candidate_index = 0; candidate_index < candidate_count;
	    ++candidate_index) {
	const edge_event_candidate &candidate = candidates[candidate_index];
	lift_witness_count += candidate.lift_witness ? 1 : 0;
	if (candidate.contact) {
	    contact_count++;
	    continue;
	}
	if (event_count >= RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	    trace->physical_event_edge_failures++;
	    return true;
	}
	const struct rt_brep_trace_edge &observation =
	    edge_trace.edges[candidate.observation];
	const struct rt_brep_trace_local_root &canonical =
	    trace->local_roots[candidate.canonical_root];
	candidate_event[candidate_index] = event_count;
	struct rt_brep_trace_physical_event &event = events[event_count++];
	event.dist = candidate.parameter;
	event.t_min = candidate.t_min;
	event.t_max = candidate.t_max;
	event.uv[0] = canonical.uv[0];
	event.uv[1] = canonical.uv[1];
	event.source_box = candidate.first_box;
	event.source_box_count = candidate.boxes;
	event.source_root = candidate.canonical_root;
	event.source_kind = RT_BREP_TRACE_EVENT_SOURCE_MANIFOLD_EDGE;
	event.edge_index = observation.edge_index;
	event.vertex_index = -1;
	event.face_index = canonical.face_index;
	event.span_index = canonical.span_index;
	event.certificate = RT_BREP_TRACE_EVENT_MANIFOLD_EDGE;
	event.determinant_sign = 0;
	event.hit_class = brep_hit::CRACK_HIT;
	event.trim_status = canonical.trim_status;
	event.adjacent_face_index = canonical.adjacent_face_index;
	event.direction = candidate.direction;
	transition_count++;
    }

    bool complete = true;
    bool joint_transition_certified = false;
    size_t regular_events = 0;
    size_t regular_roots = 0;
    size_t clean_outside = 0;
    bool box_owned[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool regular_component_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    int regular_component_sign[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index)
	box_owned[box_index] = selected_box[box_index];
    for (size_t root_index = 0; complete &&
	    root_index < trace->stored_local_roots; ++root_index) {
	if (root_owned[root_index])
	    continue;
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	const double duplicate_tolerance = 4096.0 * DBL_EPSILON *
	    std::max(1.0, fabs(root.dist));
	bool duplicate_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
	size_t duplicate_count = 0;
	for (size_t other_index = root_index;
		other_index < trace->stored_local_roots; ++other_index) {
	    if (root_owned[other_index])
		continue;
	    const struct rt_brep_trace_local_root &other =
		trace->local_roots[other_index];
	    if (other.face_index != root.face_index ||
		    other.span_index != root.span_index ||
		    fabs(other.dist - root.dist) > duplicate_tolerance)
		continue;
	    if (!std::isfinite(other.normal_dot) ||
		    other.direction != root.direction ||
		    other.hit_class != root.hit_class ||
		    other.trim_status != root.trim_status) {
		complete = false;
		break;
	    }
	    duplicate_root[other_index] = true;
	    duplicate_count++;
	}
	if (!complete || !duplicate_count)
	    break;
	size_t first_box = (size_t)-1;
	size_t certified_roots = 0;
	size_t source_boxes = 0;
	double t_min = DBL_MAX;
	double t_max = -DBL_MAX;
	int boundary_determinant_sign = 0;
	int determinant_sign = 0;
	int component_failure_stage = 0;
	bool component_boxes[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
	bool joint_component = false;
	size_t boundary_root = (size_t)-1;
	size_t boundary_roots = 0;
	for (size_t selected_index = 0;
		selected_index < trace->stored_local_roots; ++selected_index) {
	    if (!selected_root[selected_index])
		continue;
	    const struct rt_brep_trace_local_root &boundary =
		trace->local_roots[selected_index];
	    if (boundary.face_index != root.face_index ||
		    boundary.span_index != root.span_index)
		continue;
	    bool shares_box = false;
	    for (size_t box_index = 0; !shares_box &&
		    box_index < trace->stored_surface_boxes; ++box_index) {
		if (!selected_box[box_index] ||
			!brep_prepared_box_matches_local_root(
			    trace->surface_boxes[box_index], boundary, ray, tol))
		    continue;
		for (size_t regular_index = 0;
			regular_index < trace->stored_local_roots;
			++regular_index) {
		    if (duplicate_root[regular_index] &&
			    brep_prepared_box_matches_local_root(
				trace->surface_boxes[box_index],
				trace->local_roots[regular_index], ray, tol)) {
			shares_box = true;
			break;
		    }
		}
	    }
	    if (shares_box) {
		boundary_root = selected_index;
		boundary_roots++;
	    }
	}
	if (boundary_roots) {
	    int joint_failure_stage = 1;
	    size_t joint_complement_visited = 0;
	    size_t joint_complement_high_water = 0;
	    trace->physical_event_edge_joint_attempts++;
	    if (boundary_roots != 1)
		joint_failure_stage = 10;
	    else if (candidate_count != 1)
		joint_failure_stage = 11;
	    else if (candidates[0].contact)
		joint_failure_stage = 12;
	    else if (candidate_event[0] == (size_t)-1)
		joint_failure_stage = 13;
	    const bool joint_preconditions = joint_failure_stage == 1;
	    const bool joint_certified = joint_preconditions &&
		brep_trace_edge_joint_root_component(trace, bs, ray, tol,
			boundary_root, duplicate_root, component_boxes, first_box,
			certified_roots, source_boxes, t_min, t_max,
			boundary_determinant_sign, determinant_sign,
			joint_complement_visited,
			joint_complement_high_water, joint_failure_stage);
	    if (joint_certified && certified_roots != duplicate_count)
		joint_failure_stage = 14;
	    bool boundary_direction_certified = joint_certified &&
		boundary_determinant_sign;
	    if (boundary_direction_certified) {
		int oriented_sign = boundary_determinant_sign;
		const int boundary_face =
		    trace->local_roots[boundary_root].face_index;
		if (boundary_face < 0 ||
			boundary_face >= bs->brep->m_F.Count()) {
		    boundary_direction_certified = false;
		} else {
		    if (bs->brep->m_F[boundary_face].m_bRev)
			oriented_sign = -oriented_sign;
		    const int boundary_direction = oriented_sign < 0 ?
			brep_hit::ENTERING : brep_hit::LEAVING;
		    boundary_direction_certified =
			boundary_direction == candidates[0].direction;
		}
	    }
	    if (joint_certified && !boundary_direction_certified)
		joint_failure_stage = 15;
	    if (!joint_certified || certified_roots != duplicate_count ||
		    !boundary_direction_certified) {
		trace->physical_event_edge_joint_complement_visited +=
		    joint_complement_visited;
		trace->physical_event_edge_joint_complement_high_water =
		    std::max(trace->physical_event_edge_joint_complement_high_water,
			joint_complement_high_water);
		trace->physical_event_edge_joint_failure_stage =
		    joint_failure_stage;
		complete = false;
		break;
	    }
	    trace->physical_event_edge_joint_complement_visited +=
		joint_complement_visited;
	    trace->physical_event_edge_joint_complement_high_water = std::max(
		trace->physical_event_edge_joint_complement_high_water,
		joint_complement_high_water);
	    joint_component = true;
	    const double boundary_tolerance =
		candidates[0].witness_parameter_tolerance;
	    struct rt_brep_trace_physical_event &boundary_event =
		events[candidate_event[0]];
	    boundary_event.t_min = std::nextafter(
		candidates[0].parameter - boundary_tolerance, -INFINITY);
	    boundary_event.t_max = std::nextafter(
		candidates[0].parameter + boundary_tolerance, INFINITY);
	    trace->physical_event_edge_joint_components++;
	    trace->physical_event_edge_joint_boxes += source_boxes;
	    trace->physical_event_edge_joint_roots += certified_roots + 1;
	    joint_transition_certified =
		joint_transition_certified ||
		candidates[0].requires_joint_transition;
	} else {
	    trace->physical_event_regular_component_attempts++;
	    if (!brep_trace_regular_root_component(trace, bs, ray, tol,
		    duplicate_root, box_owned, component_boxes, first_box,
		    certified_roots, source_boxes, t_min, t_max,
		    determinant_sign, component_failure_stage) ||
		    certified_roots != duplicate_count) {
		trace->physical_event_regular_component_failure_stage =
		    component_failure_stage;
		complete = false;
		break;
	    }
	    trace->physical_event_regular_component_certified++;
	    trace->physical_event_regular_component_boxes += source_boxes;
	    trace->physical_event_regular_component_roots += certified_roots;
	}
	if (root.face_index < 0 ||
		root.face_index >= bs->brep->m_F.Count()) {
	    complete = false;
	    break;
	}
	int oriented_sign = determinant_sign;
	if (bs->brep->m_F[root.face_index].m_bRev)
	    oriented_sign = -oriented_sign;
	const int direction = oriented_sign < 0 ? brep_hit::ENTERING :
	    brep_hit::LEAVING;
	if (!std::isfinite(root.normal_dot) || root.direction != direction) {
	    complete = false;
	    break;
	}
	for (size_t box_index = 0;
		box_index < trace->stored_surface_boxes; ++box_index) {
	    if (!component_boxes[box_index])
		continue;
	    box_owned[box_index] = true;
	    if (joint_component) {
		if (!selected_box[box_index]) {
		    selected_box[box_index] = true;
		    selected_boxes++;
		}
	    } else {
		regular_component_box[box_index] = true;
		regular_component_sign[box_index] = determinant_sign;
	    }
	}
	for (size_t other_index = root_index;
		other_index < trace->stored_local_roots; ++other_index)
	    if (duplicate_root[other_index])
		root_owned[other_index] = true;
	regular_roots += certified_roots;
	if (root.hit_class == brep_hit::CLEAN_MISS &&
		root.trim_status == 1) {
	    clean_outside++;
	    continue;
	}
	if (root.hit_class != brep_hit::CLEAN_HIT ||
		root.trim_status == 1 ||
		event_count >= RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	    complete = false;
	    break;
	}
	struct rt_brep_trace_physical_event &event = events[event_count++];
	event.dist = root.dist;
	event.t_min = t_min;
	event.t_max = t_max;
	event.uv[0] = root.uv[0];
	event.uv[1] = root.uv[1];
	event.source_box = first_box;
	event.source_box_count = source_boxes;
	event.source_root = root_index;
	event.source_kind = RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT;
	event.edge_index = -1;
	event.vertex_index = -1;
	event.face_index = root.face_index;
	event.span_index = root.span_index;
	event.certificate = RT_BREP_TRACE_EVENT_REGULAR_INTERIOR;
	event.determinant_sign = determinant_sign;
	event.hit_class = root.hit_class;
	event.trim_status = root.trim_status;
	event.adjacent_face_index = root.adjacent_face_index;
	event.direction = direction;
	regular_events++;
    }
    for (size_t root_index = 0; complete &&
	    root_index < trace->stored_local_roots; ++root_index)
	if (!root_owned[root_index])
	    complete = false;
    for (size_t box_index = 0; complete &&
	    box_index < trace->stored_surface_boxes; ++box_index)
	if (!box_owned[box_index])
	    complete = false;
    if (tolerance_transition_candidate && !joint_transition_certified)
	complete = false;
    if (!complete || event_count > RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	trace->physical_event_edge_failures++;
	return true;
    }

    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index) {
	if (!regular_component_box[box_index])
	    continue;
	trace->surface_boxes[box_index].disposition =
	    RT_BREP_TRACE_BOX_RESOLVED_REGULAR;
	trace->surface_boxes[box_index].determinant_sign =
	    regular_component_sign[box_index];
    }
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index) {
	if (!selected_box[box_index])
	    continue;
	trace->surface_boxes[box_index].disposition =
	    selected_contact_box[box_index] ?
	    RT_BREP_TRACE_BOX_RESOLVED_CONTACT :
	    RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY;
    }
    for (size_t event_index = 0; event_index < event_count; ++event_index)
	trace->physical_events[trace->stored_physical_events++] =
	    events[event_index];
    trace->physical_event_attempts += trace->stored_surface_boxes;
    trace->physical_event_direction_checks +=
	selected_roots + regular_roots;
    trace->physical_event_regular += regular_events;
    trace->physical_event_clean_outside += clean_outside;
    trace->physical_event_near_trim += selected_roots;
    trace->physical_event_edge += transition_count;
    trace->physical_event_edge_contacts += contact_count;
    trace->physical_event_edge_lift_witnesses += lift_witness_count;
    trace->physical_event_edge_tolerance_transitions +=
	tolerance_transition_candidate ? 1 : 0;
    trace->physical_event_edge_owned_boxes += selected_boxes;
    trace->physical_event_edge_owned_roots += selected_roots;
    brep_trace_finalize_physical_events(trace, ray, tol, true);
    if (trace->physical_event_complete == 1)
	trace->physical_event_edge_certified++;
    else
	trace->physical_event_edge_failures++;
    return true;
}


/* A declared-tolerance closure is worth a continuation solve only when the
 * immutable surface-root ledger already has the narrow classifier pattern it
 * is intended to repair.  This is a screening condition, not authority: the
 * complete theorem below must repeat these checks after continuation and
 * prove the edge/trim tube, ordering, ownership, and material sector. */
static bool
brep_trace_seam_contact_miss_screen(
    const struct rt_brep_shot_trace *trace, size_t selected_root,
    const struct rt_brep_trace_edge &edge_observation)
{
    if (!trace || selected_root >= trace->stored_local_roots)
	return false;
    const struct rt_brep_trace_local_root &existing =
	trace->local_roots[selected_root];
    if (existing.face_index != edge_observation.face_index[0] &&
	    existing.face_index != edge_observation.face_index[1])
	return false;

    const struct rt_brep_trace_local_root *contact[2] = {NULL, NULL};
    size_t contact_count = 0;
    size_t trim_hits = 0;
    size_t trim_misses = 0;
    for (size_t root_index = 0;
	    root_index < trace->stored_local_roots; ++root_index) {
	if (root_index == selected_root)
	    continue;
	if (contact_count >= 2)
	    return false;
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	const bool incident =
	    root.face_index == edge_observation.face_index[0] ||
	    root.face_index == edge_observation.face_index[1];
	const bool trim_hit = root.trim_status != 1 &&
	    (root.hit_class == brep_hit::CLEAN_HIT ||
	     root.hit_class == brep_hit::NEAR_HIT);
	const bool trim_miss = root.trim_status == 1 &&
	    (root.hit_class == brep_hit::CLEAN_MISS ||
	     root.hit_class == brep_hit::NEAR_MISS);
	if (!incident || root.face_index == existing.face_index ||
		(!trim_hit && !trim_miss) || !std::isfinite(root.normal_dot) ||
		fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL)
	    return false;
	contact[contact_count++] = &root;
	trim_hits += trim_hit ? 1 : 0;
	trim_misses += trim_miss ? 1 : 0;
    }
    if (contact_count != 2 || trim_hits != 1 || trim_misses != 1)
	return false;
    if (contact[1]->dist < contact[0]->dist)
	std::swap(contact[0], contact[1]);
    return contact[0]->face_index == contact[1]->face_index &&
	contact[0]->span_index == contact[1]->span_index &&
	contact[0]->direction == brep_hit::ENTERING &&
	contact[1]->direction == brep_hit::LEAVING &&
	contact[0]->dist < contact[1]->dist;
}


/* Certify that the one classifier miss in a declared-tolerance repair is a
 * unique regular root of the analytically extended Bezier system.  The root
 * box is capped well below the neighboring root separation.  Extrapolation
 * across a span boundary is authorized only when the complete positive-weight
 * rational image of that box remains inside both the shared-edge and lifted-
 * trim tolerance tubes. */
static bool
brep_trace_declared_contact_local_root(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct rt_brep_trace_local_root &miss,
    const struct rt_brep_trace_local_root &other,
    const ON_3dPoint &edge_point, const ON_3dPoint &trim_lift,
    double closure_tolerance)
{
    if (!trace || !bs || !edge_point.IsValid() || !trim_lift.IsValid() ||
	    miss.face_index != other.face_index ||
	    miss.span_index != other.span_index || miss.span_index < 0 ||
	    (size_t)miss.span_index >= bs->surface_spans.size() ||
	    !std::isfinite(closure_tolerance) || closure_tolerance < 0.0)
	return false;
    const brep_surface_span &span = bs->surface_spans[miss.span_index];
    if (span.face_index != miss.face_index ||
	    !span.surface_domain[0].IsIncreasing() ||
	    !span.surface_domain[1].IsIncreasing())
	return false;
    double normalized_miss[2];
    double normalized_other[2];
    double separation = 0.0;
    for (int direction = 0; direction < 2; ++direction) {
	normalized_miss[direction] = span.surface_domain[direction].
	    NormalizedParameterAt(miss.uv[direction]);
	normalized_other[direction] = span.surface_domain[direction].
	    NormalizedParameterAt(other.uv[direction]);
	if (!std::isfinite(normalized_miss[direction]) ||
		!std::isfinite(normalized_other[direction]) ||
		normalized_miss[direction] < 0.0 ||
		normalized_miss[direction] > 1.0 ||
		normalized_other[direction] < 0.0 ||
		normalized_other[direction] > 1.0)
	    return false;
	const double difference = std::nextafter(fabs(
	    normalized_miss[direction] - normalized_other[direction]), 0.0);
	separation = std::max(separation, difference);
    }
    const double maximum_radius = std::nextafter(0.25 * separation, 0.0);
    if (!(maximum_radius > 0.0) || !std::isfinite(maximum_radius))
	return false;

    trace->physical_event_seam_local_root_attempts++;
    struct rt_brep_local_root_test_result result = {};
    if (!brep_surface_local_root_certificate(span, ray, miss.uv,
	    maximum_radius, result))
	return false;
    if (result.available)
	trace->physical_event_seam_local_root_available++;
    if (!result.available || !result.certified ||
	    !(result.radius < maximum_radius))
	return false;
    trace->physical_event_seam_local_root_certified++;
    trace->physical_event_seam_local_root_radius = result.radius;
    trace->physical_event_seam_local_root_model_image =
	result.model_image_displacement;
    bool extension = false;
    for (int direction = 0; direction < 2; ++direction)
	extension = extension || result.normalized_root[direction] -
	    result.radius < 0.0 || result.normalized_root[direction] +
	    result.radius > 1.0;
    if (extension)
	trace->physical_event_seam_local_root_extensions++;

    const ON_3dPoint root_point = span.surface.PointAt(
	result.normalized_root[0], result.normalized_root[1]);
    if (!result.model_image_available || !root_point.IsValid() ||
	    !(result.weight_minimum > 0.0) ||
	    !std::isfinite(result.model_image_displacement)) {
	trace->physical_event_seam_local_root_tube_failures++;
	return false;
    }
    const double coordinate_scale = std::max(1.0,
	std::max(fabs(root_point.x), std::max(fabs(root_point.y),
	std::max(fabs(root_point.z), std::max(fabs(edge_point.x),
	std::max(fabs(edge_point.y), std::max(fabs(edge_point.z),
	std::max(fabs(trim_lift.x), std::max(fabs(trim_lift.y),
	    fabs(trim_lift.z))))))))));
    const double roundoff = std::max(ON_ZERO_TOLERANCE,
	4096.0 * DBL_EPSILON * coordinate_scale);
    fastf_t upper[2] = {0.0, 0.0};
    const bool tube_contained = brep_local_root_tube_contained(
	result.model_image_displacement, root_point.DistanceTo(edge_point),
	root_point.DistanceTo(trim_lift), closure_tolerance, roundoff, upper);
    trace->physical_event_seam_local_root_edge_upper = upper[0];
    trace->physical_event_seam_local_root_trim_upper = upper[1];
    trace->physical_event_seam_local_root_tube_tolerance = closure_tolerance;
    trace->physical_event_seam_local_root_tube_roundoff = roundoff;
    if (!tube_contained) {
	trace->physical_event_seam_local_root_tube_failures++;
	return false;
    }
    return true;
}


/* Qualify the root portion of a tolerance-near incident-face contact.  The
 * accepted pair is not merged by t distance: it must be a complete ordered
 * ENTER/LEAVE lobe on the other incident face, lie strictly inside the
 * existing/continuation material segment, and arise from a non-exact but
 * explicitly authorized edge corridor.  At most one member may be a trim
 * classifier miss, and then its surface point must lie in the declared
 * closure tube of both the shared edge and corresponding lifted trim.
 * Complete box ownership and the constant material-sector proof are
 * completed below. */
static bool
brep_trace_seam_contact_roots(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray, size_t selected_root,
    const struct rt_brep_trace_edge &edge_observation,
    double continuation_dist, bool declared_contact_closure,
    bool contact_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS],
    int &contact_face, int &contact_span, ON_2dPoint &edge_uv,
    size_t &contact_miss_roots)
{
    contact_miss_roots = 0;
    if (!trace || !bs || !bs->brep ||
	    selected_root >= trace->stored_local_roots ||
	    edge_observation.edge_index < 0 ||
	    edge_observation.edge_index >= bs->brep->m_E.Count() ||
	    edge_observation.closest_state != 1 ||
	    !std::isfinite(edge_observation.ray_edge_dot) ||
	    fabs(edge_observation.ray_edge_dot) >= 1.0 - 1.0e-10)
	return false;
    const ON_BrepEdge &edge =
	bs->brep->m_E[edge_observation.edge_index];
    if (edge.m_ti.Count() != 2)
	return false;
    const ON_3dPoint edge_point = edge.PointAt(
	edge_observation.edge_parameter);
    if (!edge_point.IsValid())
	return false;
    const double coordinate_scale = std::max(1.0,
	std::max(fabs(edge_point.x), std::max(fabs(edge_point.y),
	std::max(fabs(edge_point.z), std::max(fabs(ray.m_origin.x),
	std::max(fabs(ray.m_origin.y), fabs(ray.m_origin.z)))))));
    const double line_roundoff = std::max(ON_ZERO_TOLERANCE,
	4096.0 * DBL_EPSILON * coordinate_scale);
    const double model_roundoff = std::max(ON_ZERO_TOLERANCE,
	128.0 * DBL_EPSILON * std::max(1.0,
	    edge_observation.measured_discrepancy));
    double closure_tolerance = 0.0;
    if (!(edge_observation.distance > line_roundoff) ||
	    !ON_IsValid(edge_observation.model_tolerance) ||
	    edge_observation.model_tolerance < 0.0 ||
	    !brep_seam_closure_tolerance(edge_observation,
		closure_tolerance) ||
	    !std::isfinite(edge_observation.measured_discrepancy) ||
	    !std::isfinite(edge_observation.lift_distance[0]) ||
	    !std::isfinite(edge_observation.lift_distance[1]) ||
	    edge_observation.measured_discrepancy >
		closure_tolerance + model_roundoff ||
	    std::max(edge_observation.lift_distance[0],
		edge_observation.lift_distance[1]) >
		closure_tolerance + model_roundoff)
	return false;

    const struct rt_brep_trace_local_root &existing =
	trace->local_roots[selected_root];
    size_t roots[2] = {(size_t)-1, (size_t)-1};
    size_t root_count = 0;
    for (size_t root_index = 0;
	    root_index < trace->stored_local_roots; ++root_index) {
	if (root_index == selected_root)
	    continue;
	if (root_count >= 2)
	    return false;
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	const bool incident = root.face_index == edge_observation.face_index[0] ||
	    root.face_index == edge_observation.face_index[1];
	const bool trim_hit = root.trim_status != 1 &&
	    (root.hit_class == brep_hit::CLEAN_HIT ||
	     root.hit_class == brep_hit::NEAR_HIT);
	const bool trim_miss = root.trim_status == 1 &&
	    (root.hit_class == brep_hit::CLEAN_MISS ||
	     root.hit_class == brep_hit::NEAR_MISS);
	if (!incident || root.face_index == existing.face_index ||
		(!trim_hit && !trim_miss) ||
		!std::isfinite(root.normal_dot) ||
		fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL)
	    return false;
	roots[root_count++] = root_index;
    }
    if (root_count != 2)
	return false;
    if (trace->local_roots[roots[1]].dist <
	    trace->local_roots[roots[0]].dist)
	std::swap(roots[0], roots[1]);
    const struct rt_brep_trace_local_root &lower =
	trace->local_roots[roots[0]];
    const struct rt_brep_trace_local_root &upper =
	trace->local_roots[roots[1]];
    const double t_scale = std::max(1.0,
	std::max(fabs(existing.dist), std::max(fabs(continuation_dist),
	    std::max(fabs(lower.dist), fabs(upper.dist)))));
    const double t_roundoff = 4096.0 * DBL_EPSILON * t_scale;
    const double outer_minimum = std::min(existing.dist, continuation_dist);
    const double outer_maximum = std::max(existing.dist, continuation_dist);
    if (lower.face_index != upper.face_index ||
	    lower.span_index != upper.span_index ||
	    lower.direction != brep_hit::ENTERING ||
	    upper.direction != brep_hit::LEAVING ||
	    !(lower.dist < upper.dist) ||
	    lower.dist <= outer_minimum + t_roundoff ||
	    upper.dist >= outer_maximum - t_roundoff)
	return false;

    const ON_BrepTrim *contact_trim = NULL;
    for (int side = 0; side < 2; ++side) {
	const int trim_index = edge.m_ti[side];
	if (trim_index >= 0 && trim_index < bs->brep->m_T.Count() &&
		bs->brep->m_T[trim_index].FaceIndexOf() == lower.face_index) {
	    contact_trim = &bs->brep->m_T[trim_index];
	    break;
	}
    }
    double trim_parameter = 0.0;
    ON_3dPoint trim_uv;
    if (!contact_trim || !brep_edge_trim_parameter(edge, *contact_trim,
	    edge_observation.edge_parameter, trim_parameter) ||
	    !contact_trim->EvPoint(trim_parameter, trim_uv) ||
	    !trim_uv.IsValid())
	return false;
    edge_uv = ON_2dPoint(trim_uv.x, trim_uv.y);
    contact_face = lower.face_index;
    contact_span = lower.span_index;
    if (contact_face < 0 || contact_face >= bs->brep->m_F.Count())
	return false;
    const ON_Surface *surface = bs->brep->m_F[contact_face].SurfaceOf();
    const ON_3dPoint trim_lift = surface ?
	surface->PointAt(edge_uv.x, edge_uv.y) : ON_3dPoint::UnsetPoint;
    size_t trim_hits = 0;
    size_t trim_misses = 0;
    size_t trim_hit_root = (size_t)-1;
    size_t trim_miss_root = (size_t)-1;
    const size_t contact_roots[2] = {roots[0], roots[1]};
    for (size_t root_offset = 0; root_offset < 2; ++root_offset) {
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[contact_roots[root_offset]];
	if (root.trim_status != 1 &&
		(root.hit_class == brep_hit::CLEAN_HIT ||
		 root.hit_class == brep_hit::NEAR_HIT)) {
	    trim_hits++;
	    trim_hit_root = contact_roots[root_offset];
	    continue;
	}
	const ON_3dPoint root_point = surface ?
	    surface->PointAt(root.uv[0], root.uv[1]) :
	    ON_3dPoint::UnsetPoint;
	if (!root_point.IsValid() || !trim_lift.IsValid())
	    return false;
	const double root_scale = std::max(coordinate_scale,
	    std::max(fabs(root_point.x), std::max(fabs(root_point.y),
		fabs(root_point.z))));
	const double tube_roundoff = std::max(ON_ZERO_TOLERANCE,
	    4096.0 * DBL_EPSILON * root_scale);
	if (root_point.DistanceTo(edge_point) >
		closure_tolerance + tube_roundoff ||
		root_point.DistanceTo(trim_lift) >
		closure_tolerance + tube_roundoff)
	    return false;
	trim_misses++;
	trim_miss_root = contact_roots[root_offset];
    }
    if (trim_hits + trim_misses != 2 || trim_misses > 1)
	return false;
    if (declared_contact_closure &&
	    (trim_hits != 1 || trim_misses != 1 ||
	     trim_hit_root == (size_t)-1 || trim_miss_root == (size_t)-1 ||
	     !brep_trace_declared_contact_local_root(trace, bs, ray,
		 trace->local_roots[trim_miss_root],
		 trace->local_roots[trim_hit_root], edge_point, trim_lift,
		 closure_tolerance)))
	return false;
    contact_miss_roots = trim_misses;
    contact_root[roots[0]] = true;
    contact_root[roots[1]] = true;
    return true;
}


static bool
brep_trace_surface_boxes_connected(
    const struct rt_brep_trace_surface_box &first,
    const struct rt_brep_trace_surface_box &second)
{
    if (first.face_index != second.face_index ||
	first.span_index != second.span_index)
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	const double scale = std::max(1.0,
	    std::max(fabs(first.uv_min[direction]),
	    std::max(fabs(first.uv_max[direction]),
	    std::max(fabs(second.uv_min[direction]),
		fabs(second.uv_max[direction])))));
	const double tolerance = 256.0 * DBL_EPSILON * scale;
	if (first.uv_max[direction] <
		second.uv_min[direction] - tolerance ||
	    second.uv_max[direction] <
		first.uv_min[direction] - tolerance)
	    return false;
    }
    const double t_scale = std::max(1.0,
	std::max(fabs(first.t_min), std::max(fabs(first.t_max),
	std::max(fabs(second.t_min), fabs(second.t_max)))));
    const double t_tolerance = 256.0 * DBL_EPSILON * t_scale;
    return first.t_max >= second.t_min - t_tolerance &&
	second.t_max >= first.t_min - t_tolerance;
}


/* Build the complete terminal-box component seeded by boxes already known to
 * contain one root.  Role qualification is deliberately supplied by the
 * caller: production derives it from immutable root/box provenance, while the
 * private test hook supplies synthetic roles. */
static bool
brep_source_union_component(
    const struct rt_brep_trace_surface_box *boxes, size_t box_count,
    const bool *root_box, const bool *candidate_box, bool *component_box,
    size_t &eligible_boxes, size_t &root_boxes, size_t &component_boxes,
    double uv_minimum[2], double uv_maximum[2])
{
    eligible_boxes = 0;
    root_boxes = 0;
    component_boxes = 0;
    uv_minimum[0] = uv_minimum[1] = DBL_MAX;
    uv_maximum[0] = uv_maximum[1] = -DBL_MAX;
    if (!boxes || !root_box || !candidate_box || !component_box ||
	!box_count || box_count > RT_BREP_TRACE_MAX_SURFACE_BOXES)
	return false;

    int face_index = -1;
    int span_index = -1;
    for (size_t box_index = 0; box_index < box_count; ++box_index) {
	component_box[box_index] = false;
	if (root_box[box_index] && candidate_box[box_index])
	    return false;
	if (!root_box[box_index] && !candidate_box[box_index])
	    continue;
	const struct rt_brep_trace_surface_box &box = boxes[box_index];
	if (!std::isfinite(box.uv_min[0]) ||
		!std::isfinite(box.uv_min[1]) ||
		!std::isfinite(box.uv_max[0]) ||
		!std::isfinite(box.uv_max[1]) ||
		!std::isfinite(box.t_min) || !std::isfinite(box.t_max) ||
		!(box.uv_min[0] < box.uv_max[0]) ||
		!(box.uv_min[1] < box.uv_max[1]) || box.t_min > box.t_max ||
		box.face_index < 0 || box.span_index < 0)
	    return false;
	if (face_index < 0) {
	    face_index = box.face_index;
	    span_index = box.span_index;
	} else if (box.face_index != face_index ||
		box.span_index != span_index) {
	    return false;
	}
	eligible_boxes++;
	if (!root_box[box_index])
	    continue;
	component_box[box_index] = true;
	root_boxes++;
	component_boxes++;
    }
    if (!root_boxes || eligible_boxes <= root_boxes)
	return false;

    bool changed = true;
    while (changed) {
	changed = false;
	for (size_t candidate_index = 0;
		candidate_index < box_count; ++candidate_index) {
	    if (!candidate_box[candidate_index] ||
		    component_box[candidate_index])
		continue;
	    for (size_t component_index = 0;
		    component_index < box_count; ++component_index) {
		if (!component_box[component_index] ||
			!brep_trace_surface_boxes_connected(
			    boxes[candidate_index], boxes[component_index]))
		    continue;
		component_box[candidate_index] = true;
		component_boxes++;
		changed = true;
		break;
	    }
	}
    }

    for (size_t box_index = 0; box_index < box_count; ++box_index) {
	if (!component_box[box_index])
	    continue;
	for (int direction = 0; direction < 2; ++direction) {
	    uv_minimum[direction] = std::min(uv_minimum[direction],
		(double)boxes[box_index].uv_min[direction]);
	    uv_maximum[direction] = std::max(uv_maximum[direction],
		(double)boxes[box_index].uv_max[direction]);
	}
    }
    return std::isfinite(uv_minimum[0]) &&
	std::isfinite(uv_minimum[1]) && std::isfinite(uv_maximum[0]) &&
	std::isfinite(uv_maximum[1]) &&
	uv_minimum[0] < uv_maximum[0] &&
	uv_minimum[1] < uv_maximum[1];
}


extern "C" int
_rt_brep_source_union_test(const fastf_t *first_coefficients,
    const fastf_t *second_coefficients, int u_order, int v_order,
    const fastf_t coefficient_error[2],
    const struct rt_brep_source_union_test_box *input_boxes,
    size_t box_count, const fastf_t root[2],
    struct rt_brep_source_union_test_result *result)
{
    if (!first_coefficients || !second_coefficients || !coefficient_error ||
	!input_boxes || !root || !result || u_order < 2 || v_order < 2 ||
	u_order > BREP_DIRECT_BEZIER_MAX_ORDER ||
	v_order > BREP_DIRECT_BEZIER_MAX_ORDER || !box_count ||
	box_count > RT_BREP_TRACE_MAX_SURFACE_BOXES ||
	!std::isfinite(coefficient_error[0]) ||
	!std::isfinite(coefficient_error[1]) || coefficient_error[0] < 0.0 ||
	coefficient_error[1] < 0.0 || !std::isfinite(root[0]) ||
	!std::isfinite(root[1]) || root[0] < 0.0 || root[0] > 1.0 ||
	root[1] < 0.0 || root[1] > 1.0)
	return 0;
    *result = {};

    double values[2][BREP_DIRECT_BEZIER_MAX_CVS] = {};
    const fastf_t *input[2] = {first_coefficients, second_coefficients};
    const size_t coefficient_count = (size_t)u_order * v_order;
    for (int equation = 0; equation < 2; ++equation) {
	for (size_t coefficient_index = 0;
		coefficient_index < coefficient_count; ++coefficient_index) {
	    if (!std::isfinite(input[equation][coefficient_index]))
		return 0;
	    values[equation][coefficient_index] =
		input[equation][coefficient_index];
	}
    }

    struct rt_brep_trace_surface_box
	boxes[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool root_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool candidate_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool component_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    for (size_t box_index = 0; box_index < box_count; ++box_index) {
	const struct rt_brep_source_union_test_box &input_box =
	    input_boxes[box_index];
	if (input_box.role != RT_BREP_SOURCE_UNION_TEST_ROOT &&
		input_box.role != RT_BREP_SOURCE_UNION_TEST_CANDIDATE)
	    return 0;
	struct rt_brep_trace_surface_box &box = boxes[box_index];
	for (int direction = 0; direction < 2; ++direction) {
	    if (!std::isfinite(input_box.uv_minimum[direction]) ||
		    !std::isfinite(input_box.uv_maximum[direction]) ||
		    input_box.uv_minimum[direction] < 0.0 ||
		    input_box.uv_maximum[direction] > 1.0 ||
		    !(input_box.uv_minimum[direction] <
			input_box.uv_maximum[direction]))
		return 0;
	    box.uv_min[direction] = input_box.uv_minimum[direction];
	    box.uv_max[direction] = input_box.uv_maximum[direction];
	}
	if (!std::isfinite(input_box.t_minimum) ||
		!std::isfinite(input_box.t_maximum) ||
		input_box.t_minimum > input_box.t_maximum)
	    return 0;
	box.t_min = input_box.t_minimum;
	box.t_max = input_box.t_maximum;
	box.face_index = 0;
	box.span_index = 0;
	if (input_box.role == RT_BREP_SOURCE_UNION_TEST_ROOT) {
	    for (int direction = 0; direction < 2; ++direction) {
		if (root[direction] < box.uv_min[direction] ||
			root[direction] > box.uv_max[direction])
		    return 0;
	    }
	    root_box[box_index] = true;
	} else {
	    candidate_box[box_index] = true;
	}
    }

    size_t eligible_boxes = 0;
    size_t root_boxes = 0;
    size_t component_boxes = 0;
    double uv_minimum[2];
    double uv_maximum[2];
    if (!brep_source_union_component(boxes, box_count, root_box,
	    candidate_box, component_box, eligible_boxes, root_boxes,
	    component_boxes, uv_minimum, uv_maximum))
	return 0;
    result->eligible_boxes = eligible_boxes;
    result->root_boxes = root_boxes;
    result->component_boxes = component_boxes;
    result->component_complete = component_boxes == eligible_boxes;
    for (int direction = 0; direction < 2; ++direction) {
	result->uv_minimum[direction] = uv_minimum[direction];
	result->uv_maximum[direction] = uv_maximum[direction];
    }
    if (!result->component_complete)
	return 1;

    double minimum[2];
    double maximum[2];
    double local_root[2];
    for (int direction = 0; direction < 2; ++direction) {
	minimum[direction] = std::max(0.0,
	    std::nextafter(uv_minimum[direction], -INFINITY));
	maximum[direction] = std::min(1.0,
	    std::nextafter(uv_maximum[direction], INFINITY));
	if (!(minimum[direction] < maximum[direction]) ||
		root[direction] < minimum[direction] ||
		root[direction] > maximum[direction])
	    return 0;
	local_root[direction] = (root[direction] - minimum[direction]) /
	    (maximum[direction] - minimum[direction]);
    }

    double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS] = {};
    double restricted_error[2] = {};
    result->krawczyk_attempted = 1;
    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_scalar_surface_restrict_bounded(values[equation], u_order,
		v_order, coefficient_error[equation], minimum[0], maximum[0],
		minimum[1], maximum[1], restricted[equation],
		restricted_error[equation]))
	    return 1;
    }
    result->certified = brep_surface_krawczyk_certified(restricted,
	u_order, v_order, restricted_error, local_root);
    return 1;
}


/*
 * A strict Krawczyk inclusion over the rectangular hull of a connected
 * terminal-box component proves that the component contains exactly the one
 * already isolated source root.  This is a surface-root count theorem, not a
 * proximity rule: disconnected boxes, other roots, faces, spans, and a failed
 * hull certificate remain unowned.
 */
static bool
brep_trace_seam_source_union(
    const struct rt_brep_shot_trace *trace, const struct brep_specific *bs,
    const ON_Ray &ray, const struct bn_tol *tol, size_t selected_root,
    int contact_face, int contact_span,
    bool component_box[RT_BREP_TRACE_MAX_SURFACE_BOXES],
    size_t &root_boxes, size_t &component_boxes)
{
    root_boxes = 0;
    component_boxes = 0;
    if (!trace || !bs || !component_box ||
	selected_root >= trace->stored_local_roots)
	return false;
    const struct rt_brep_trace_local_root &root =
	trace->local_roots[selected_root];
    if (root.face_index == contact_face && root.span_index == contact_span)
	return false;
    if (root.span_index < 0 ||
	(size_t)root.span_index >= bs->surface_spans.size())
	return false;
    const brep_surface_span &span = bs->surface_spans[root.span_index];
    if (span.face_index != root.face_index ||
	!span.surface_domain[0].IsIncreasing() ||
	!span.surface_domain[1].IsIncreasing())
	return false;

    bool root_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool candidate_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    int determinant_sign = 0;
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	if (brep_prepared_box_matches_local_root(box, root, ray, tol)) {
	    if (box.disposition != RT_BREP_TRACE_BOX_RESOLVED_REGULAR ||
		!box.determinant_sign ||
		(determinant_sign &&
		 box.determinant_sign != determinant_sign))
		return false;
	    determinant_sign = box.determinant_sign;
	    root_box[box_index] = true;
	    continue;
	}
	if (box.face_index != root.face_index ||
		box.span_index != root.span_index ||
		box.disposition != RT_BREP_TRACE_BOX_UNRESOLVED ||
		box.determinant_sign ||
		brep_prepared_box_has_root(trace, box, ray, tol))
	    continue;
	candidate_box[box_index] = true;
    }
    size_t eligible_boxes = 0;
    double uv_minimum[2];
    double uv_maximum[2];
    if (!brep_source_union_component(trace->surface_boxes,
	    trace->stored_surface_boxes, root_box, candidate_box, component_box,
	    eligible_boxes, root_boxes, component_boxes, uv_minimum,
	    uv_maximum) || component_boxes != eligible_boxes)
	return false;

    double minimum[2];
    double maximum[2];
    double local_root[2];
    for (int direction = 0; direction < 2; ++direction) {
	minimum[direction] = std::max(0.0, std::nextafter(
	    span.surface_domain[direction].NormalizedParameterAt(
		uv_minimum[direction]), -INFINITY));
	maximum[direction] = std::min(1.0, std::nextafter(
	    span.surface_domain[direction].NormalizedParameterAt(
		uv_maximum[direction]), INFINITY));
	const double root_parameter =
	    span.surface_domain[direction].NormalizedParameterAt(
		root.uv[direction]);
	if (!std::isfinite(minimum[direction]) ||
		!std::isfinite(maximum[direction]) ||
		!std::isfinite(root_parameter) ||
		!(minimum[direction] < maximum[direction]) ||
		root_parameter < minimum[direction] ||
		root_parameter > maximum[direction])
	    return false;
	local_root[direction] = (root_parameter - minimum[direction]) /
	    (maximum[direction] - minimum[direction]);
    }

    ON_3dVector first;
    ON_3dVector second;
    brep_surface_coefficients coefficients;
    if (!brep_ray_plane_frame(ray, first, second) ||
	!brep_surface_coefficients_init(coefficients, span, ray, first, second))
	return false;
    double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
    double restricted_error[2] = {0.0, 0.0};
    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_scalar_surface_restrict_bounded(
		coefficients.value[equation], coefficients.order[0],
		coefficients.order[1], coefficients.error[equation],
		minimum[0], maximum[0], minimum[1], maximum[1],
		restricted[equation], restricted_error[equation]))
	    return false;
    }
    return brep_surface_krawczyk_certified(restricted,
	coefficients.order[0], coefficients.order[1], restricted_error,
	local_root);
}


/* A crack continuation is a separate boundary theorem, not a weakened
 * regular-root certificate.  The accepted case has one non-tangent root, one
 * authorized manifold-edge witness, and one independently isolated
 * continuation root.  Any additional retained roots must be tangent witnesses
 * on that same edge corridor. */
static bool
brep_trace_seam_physical_events(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    if (!trace || !bs || !bs->brep || !bs->is_solid || bs->plate_mode)
	return false;
    trace->physical_event_seam_attempts++;
    const bool workspace_complete = trace->prepared_surface_spans &&
	!trace->unsupported_surface_faces &&
	trace->supported_surface_faces == bs->face_records.size() &&
	trace->candidate_surface_spans + trace->excluded_surface_spans ==
	    trace->prepared_surface_spans &&
	!trace->surface_workspace_exhausted &&
	!trace->surface_clip_restriction_failures &&
	!trace->surface_box_overflow &&
	trace->surface_isolated_boxes == trace->stored_surface_boxes &&
	!trace->local_root_overflow && !trace->local_trim_failures &&
	trace->local_root_candidates == trace->stored_local_roots &&
	trace->stored_surface_boxes && trace->stored_local_roots &&
	!trace->stored_physical_events;
    if (!workspace_complete) {
	trace->physical_event_seam_failures++;
	return false;
    }

    struct rt_brep_shot_trace edge_trace = {};
    edge_trace.closure_edge_index = -1;
    edge_trace.closure_missing_direction = -1;
    edge_trace.continuation_face_index = -1;
    edge_trace.continuation_span_index = -1;
    edge_trace.continuation_adjacent_face_index = -99;
    brep_observe_edges(&edge_trace, bs, ray);
    if (edge_trace.edge_overflow || edge_trace.edge_evaluation_failures ||
	edge_trace.edge_observations != edge_trace.stored_edges) {
	trace->physical_event_seam_failures++;
	return false;
    }

    size_t certified_candidates = 0;
    size_t selected_root = 0;
    struct rt_brep_trace_edge selected_edge = {};
    double continuation_dist = 0.0;
    double continuation_uv[2] = {0.0, 0.0};
    double continuation_t_min = 0.0;
    double continuation_t_max = 0.0;
    int continuation_face = -1;
    int continuation_span = -1;
    int continuation_adjacency = -99;
    int continuation_direction = -1;
    bool selected_declared_contact_closure = false;
    for (size_t root_index = 0;
	    root_index < trace->stored_local_roots; ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	if (root.trim_status == 1 ||
		(root.hit_class != brep_hit::CLEAN_HIT &&
		 root.hit_class != brep_hit::NEAR_HIT) ||
		!std::isfinite(root.normal_dot) ||
		fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL)
	    continue;
	brep_hit existing_hit;
	if (!brep_trace_make_hit(bs, ray, root.face_index, root.dist,
		root.uv, root.trim_status, root.hit_class, root.direction,
		root.adjacent_face_index, true, existing_hit))
	    continue;
	trace->physical_event_seam_root_candidates++;

	struct rt_brep_shot_trace candidate = {};
	candidate.closure_edge_index = -1;
	candidate.closure_missing_direction = -1;
	candidate.continuation_face_index = -1;
	candidate.continuation_span_index = -1;
	candidate.continuation_adjacent_face_index = -99;
	candidate.stored_edges = edge_trace.stored_edges;
	for (size_t edge_index = 0;
		edge_index < edge_trace.stored_edges; ++edge_index)
	    candidate.edges[edge_index] = edge_trace.edges[edge_index];
	brep_classify_closure(&candidate, bs, &existing_hit);
	bool declared_contact_closure = false;
	if (!candidate.closure_candidates &&
		candidate.closure_edge_index < 0) {
	    brep_classify_declared_contact_closure(&candidate, bs,
		&existing_hit);
	    declared_contact_closure = candidate.closure_candidates == 1 &&
		candidate.closure_edge_index >= 0;
	}
	if (candidate.closure_candidates != 1 ||
		candidate.closure_edge_index < 0)
	    continue;
	if (declared_contact_closure) {
	    const struct rt_brep_trace_edge *declared_observation = NULL;
	    for (size_t edge_index = 0;
		    edge_index < candidate.stored_edges; ++edge_index) {
		if (candidate.edges[edge_index].edge_index ==
			candidate.closure_edge_index) {
		    declared_observation = &candidate.edges[edge_index];
		    break;
		}
	    }
	    if (!declared_observation ||
		    !brep_trace_seam_contact_miss_screen(trace, root_index,
			*declared_observation))
		continue;
	} else {
	    trace->physical_event_seam_closure_candidates++;
	}
	brep_resolve_continuation(&candidate, bs, ray, &existing_hit, NULL);
	if (candidate.closure_shadow_segments != 1 ||
		candidate.continuation_candidates != 1 ||
		candidate.continuation_certified_candidates != 1 ||
		candidate.continuation_certificate_exhausted ||
		candidate.continuation_certificate_existing_overlap ||
		candidate.continuation_certificate_root_boxes !=
		candidate.continuation_certificate_isolated ||
		!candidate.continuation_certificate_root_boxes ||
		candidate.continuation_face_index != root.face_index ||
		candidate.continuation_span_index < 0 ||
		candidate.continuation_dist <
		candidate.continuation_certificate_t_min ||
		candidate.continuation_dist >
		candidate.continuation_certificate_t_max ||
		!std::isfinite(candidate.continuation_normal_dot) ||
		fabs(candidate.continuation_normal_dot) <=
		BREP_GRAZING_DOT_TOL)
	    continue;
	const struct rt_brep_trace_edge *observation = NULL;
	for (size_t edge_index = 0;
		edge_index < candidate.stored_edges; ++edge_index) {
	    if (candidate.edges[edge_index].edge_index ==
		    candidate.closure_edge_index) {
		observation = &candidate.edges[edge_index];
		break;
	    }
	}
	if (!observation || !observation->within_edge_tolerance ||
		!observation->sector_valid || observation->closest_state != 1 ||
		!observation->correspondence_supported ||
		observation->correspondence_exhausted ||
		!observation->discrepancy_endpoints_certified ||
		!observation->discrepancy_bounded ||
		observation->discrepancy_bound_exhausted ||
		observation->discrepancy_proof_class !=
		RT_BREP_SEAM_GAP_INSIDE ||
		!observation->discrepancy_authorized)
	    continue;
	if (!declared_contact_closure)
	    trace->physical_event_seam_continuation_candidates++;
	certified_candidates++;
	if (certified_candidates != 1)
	    continue;
	selected_root = root_index;
	selected_edge = *observation;
	continuation_dist = candidate.continuation_dist;
	continuation_uv[0] = candidate.continuation_uv[0];
	continuation_uv[1] = candidate.continuation_uv[1];
	continuation_t_min = candidate.continuation_certificate_t_min;
	continuation_t_max = candidate.continuation_certificate_t_max;
	continuation_face = candidate.continuation_face_index;
	continuation_span = candidate.continuation_span_index;
	continuation_adjacency =
	    candidate.continuation_adjacent_face_index;
	continuation_direction = candidate.closure_missing_direction;
	selected_declared_contact_closure = declared_contact_closure;
    }
    if (certified_candidates != 1) {
	trace->physical_event_seam_failures++;
	return false;
    }

    const double ray_length = ray.m_dir.Length();
    if (!(ray_length > DBL_MIN) || !std::isfinite(ray_length)) {
	trace->physical_event_seam_failures++;
	return false;
    }
    const double t_scale = std::max(1.0,
	std::max(fabs(selected_edge.ray_dist), fabs(continuation_dist)));
    const double witness_tolerance = std::max(
	(double)selected_edge.model_tolerance / ray_length,
	128.0 * DBL_EPSILON * t_scale);
    bool witness_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    size_t witness_roots = 0;
    bool narrow_witness = true;
    for (size_t root_index = 0;
	    root_index < trace->stored_local_roots; ++root_index) {
	if (root_index == selected_root)
	    continue;
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	const bool incident = root.face_index == selected_edge.face_index[0] ||
	    root.face_index == selected_edge.face_index[1];
	if (!incident || !std::isfinite(root.normal_dot) ||
		fabs(root.normal_dot) > BREP_GRAZING_DOT_TOL ||
		fabs(root.dist - selected_edge.ray_dist) > witness_tolerance) {
	    narrow_witness = false;
	    break;
	}
	witness_root[root_index] = true;
	witness_roots++;
    }
    bool contact_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    bool contact_pair = false;
    int contact_face = -1;
    int contact_span = -1;
    ON_2dPoint contact_edge_uv(0.0, 0.0);
    size_t contact_miss_roots = 0;
    if (!narrow_witness) {
	for (size_t root_index = 0;
		root_index < RT_BREP_TRACE_MAX_LOCAL_ROOTS; ++root_index)
	    witness_root[root_index] = false;
	witness_roots = 0;
	contact_pair = brep_trace_seam_contact_roots(trace, bs, ray,
	    selected_root, selected_edge, continuation_dist,
	    selected_declared_contact_closure, contact_root, contact_face,
	    contact_span, contact_edge_uv,
	    contact_miss_roots);
	if (!contact_pair) {
	    if (!selected_declared_contact_closure) {
		trace->physical_event_seam_ownership_failures++;
		trace->physical_event_seam_witness_failures++;
	    }
	    trace->physical_event_seam_failures++;
	    return false;
	}
    }
    if (selected_declared_contact_closure &&
	    (!contact_pair || contact_miss_roots != 1)) {
	trace->physical_event_seam_failures++;
	return false;
    }
    if (selected_declared_contact_closure) {
	trace->physical_event_seam_closure_candidates++;
	trace->physical_event_seam_continuation_candidates++;
	trace->physical_event_seam_declared_contact_pairs++;
    }
    if (narrow_witness && !witness_roots) {
	trace->physical_event_seam_edge_only_candidates++;
    }

    size_t first_source_box = (size_t)-1;
    size_t source_boxes = 0;
    size_t witness_boxes = 0;
    size_t contact_boxes = 0;
    bool source_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool witness_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool contact_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool root_box_owned[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    double existing_t_min = DBL_MAX;
    double existing_t_max = -DBL_MAX;
    double contact_t_min = DBL_MAX;
    double contact_t_max = -DBL_MAX;
    size_t oblique_frame_cells = 0;
    size_t oblique_box_links = 0;
    const bool perpendicular_contact = contact_pair &&
	fabs(selected_edge.ray_edge_dot) <= 1.0e-10;
    bool source_union_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    size_t source_union_root_boxes = 0;
    size_t source_union_boxes = 0;
    const bool source_union_certified = contact_pair &&
	!perpendicular_contact && brep_trace_seam_source_union(trace, bs, ray,
	    tol, selected_root, contact_face, contact_span, source_union_box,
	    source_union_root_boxes, source_union_boxes);
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	const bool source_root_box = brep_prepared_box_matches_local_root(box,
	    trace->local_roots[selected_root], ray, tol);
	const bool source = source_root_box ||
	    (source_union_certified && source_union_box[box_index]);
	bool witness = false;
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index) {
	    if (!witness_root[root_index] ||
		    !brep_prepared_box_matches_local_root(box,
		    trace->local_roots[root_index], ray, tol))
		continue;
	    witness = true;
	    root_box_owned[root_index] = true;
	}
	bool contact = false;
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index) {
	    if (!contact_root[root_index] ||
		    !brep_prepared_box_matches_local_root(box,
		    trace->local_roots[root_index], ray, tol))
		continue;
	    contact = true;
	    root_box_owned[root_index] = true;
	}
	/* An oblique contact lobe may leave adjacent terminal boxes unresolved
	 * without a nominal root.  Admit the complete same-face/span determinant
	 * corridor to the interval proof below; every admitted box must still own
	 * a seam UV interval and its complete t hull must pass the material-sector
	 * theorem before any publication. */
	if (!source && !witness && !contact && contact_pair &&
		!perpendicular_contact && box.face_index == contact_face &&
		box.span_index == contact_span &&
		box.disposition == RT_BREP_TRACE_BOX_UNRESOLVED &&
		!box.determinant_sign)
	    contact = true;
	const double u_scale = std::max(1.0,
	    std::max(fabs(box.uv_min[0]), fabs(box.uv_max[0])));
	const double v_scale = std::max(1.0,
	    std::max(fabs(box.uv_min[1]), fabs(box.uv_max[1])));
	const double u_tolerance = 256.0 * DBL_EPSILON * u_scale;
	const double v_tolerance = 256.0 * DBL_EPSILON * v_scale;
	const bool contact_box_valid = contact &&
	    box.face_index == contact_face && box.span_index == contact_span &&
	    box.disposition == RT_BREP_TRACE_BOX_UNRESOLVED &&
	    !box.determinant_sign && (!perpendicular_contact ||
	    (contact_edge_uv.x >= box.uv_min[0] - u_tolerance &&
	     contact_edge_uv.x <= box.uv_max[0] + u_tolerance &&
	     contact_edge_uv.y >= box.uv_min[1] - v_tolerance &&
	     contact_edge_uv.y <= box.uv_max[1] + v_tolerance &&
	     selected_edge.ray_dist >= box.t_min - witness_tolerance &&
	     selected_edge.ray_dist <= box.t_max + witness_tolerance));
	const size_t roles = (source ? 1 : 0) + (witness ? 1 : 0) +
	    (contact ? 1 : 0);
	if (roles != 1 || (contact && !contact_box_valid) || (witness &&
		(selected_edge.ray_dist < box.t_min - witness_tolerance ||
		 selected_edge.ray_dist > box.t_max + witness_tolerance))) {
	    trace->physical_event_seam_ownership_failures++;
	    trace->physical_event_seam_box_failures++;
	    trace->physical_event_seam_failures++;
	    return false;
	}
	if (source) {
	    source_box[box_index] = true;
	    if (source_root_box && first_source_box == (size_t)-1)
		first_source_box = box_index;
	    source_boxes++;
	    root_box_owned[selected_root] = true;
	    existing_t_min = std::min(existing_t_min, (double)box.t_min);
	    existing_t_max = std::max(existing_t_max, (double)box.t_max);
	} else if (witness) {
	    witness_box[box_index] = true;
	    witness_boxes++;
	} else {
	    contact_box[box_index] = true;
	    contact_boxes++;
	    contact_t_min = std::min(contact_t_min, (double)box.t_min);
	    contact_t_max = std::max(contact_t_max, (double)box.t_max);
	}
    }
    if (!source_boxes || first_source_box == (size_t)-1 ||
	    (source_union_certified &&
	     source_boxes != source_union_boxes) ||
	    (witness_roots && !witness_boxes) ||
	    (contact_pair && !contact_boxes)) {
	trace->physical_event_seam_ownership_failures++;
	trace->physical_event_seam_box_failures++;
	trace->physical_event_seam_failures++;
	return false;
    }
    for (size_t root_index = 0;
	    root_index < trace->stored_local_roots; ++root_index) {
	if (!root_box_owned[root_index]) {
	    trace->physical_event_seam_ownership_failures++;
	    trace->physical_event_seam_root_coverage_failures++;
	    trace->physical_event_seam_failures++;
	    return false;
	}
    }

    if (contact_pair) {
	const struct rt_brep_trace_local_root &existing =
	    trace->local_roots[selected_root];
	const double outer_minimum = std::min(existing.dist, continuation_dist);
	const double outer_maximum = std::max(existing.dist, continuation_dist);
	const ON_3dPoint before = ray.m_origin + contact_t_min * ray.m_dir;
	const ON_3dPoint after = ray.m_origin + contact_t_max * ray.m_dir;
	if (!std::isfinite(contact_t_min) || !std::isfinite(contact_t_max) ||
		!(contact_t_min < contact_t_max) ||
		contact_t_min <= outer_minimum ||
		contact_t_max >= outer_maximum ||
		(perpendicular_contact ?
		 !brep_edge_sector_segment_inside(bs, selected_edge,
		     before, after) :
		 !brep_edge_sector_oblique_contact_inside(bs, selected_edge,
		     ray, contact_t_min, contact_t_max, contact_box,
		     trace->surface_boxes, trace->stored_surface_boxes,
		     oblique_frame_cells, oblique_box_links))) {
	    trace->physical_event_seam_ownership_failures++;
	    trace->physical_event_seam_witness_failures++;
	    trace->physical_event_seam_failures++;
	    return false;
	}
    }

    if (trace->stored_physical_events + 2 >
	    RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	trace->physical_event_overflow++;
	trace->physical_event_seam_failures++;
	return true;
    }

    /* Publish box dispositions only after the complete ownership proof. */
    for (size_t box_index = 0;
	    box_index < trace->stored_surface_boxes; ++box_index) {
	if (source_box[box_index])
	    trace->surface_boxes[box_index].disposition =
		RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY;
	else if (witness_box[box_index] || contact_box[box_index])
	    trace->surface_boxes[box_index].disposition =
		RT_BREP_TRACE_BOX_RESOLVED_CONTACT;
    }
    const struct rt_brep_trace_local_root &existing =
	trace->local_roots[selected_root];
    struct rt_brep_trace_physical_event &existing_event =
	trace->physical_events[trace->stored_physical_events++];
    existing_event = {};
    existing_event.dist = existing.dist;
    existing_event.t_min = existing_t_min;
    existing_event.t_max = existing_t_max;
    existing_event.uv[0] = existing.uv[0];
    existing_event.uv[1] = existing.uv[1];
    existing_event.source_box = first_source_box;
    existing_event.source_box_count = source_boxes;
    existing_event.source_root = selected_root;
    existing_event.source_kind = RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT;
    existing_event.edge_index = selected_edge.edge_index;
    existing_event.vertex_index = -1;
    existing_event.face_index = existing.face_index;
    existing_event.span_index = existing.span_index;
    existing_event.certificate = contact_pair ?
	RT_BREP_TRACE_EVENT_SEAM_CONTACT_EXISTING :
	RT_BREP_TRACE_EVENT_SEAM_EXISTING;
    existing_event.hit_class = existing.hit_class;
    existing_event.trim_status = existing.trim_status;
    existing_event.adjacent_face_index = existing.adjacent_face_index;
    existing_event.direction = existing.direction;

    struct rt_brep_trace_physical_event &continuation_event =
	trace->physical_events[trace->stored_physical_events++];
    continuation_event = {};
    continuation_event.dist = continuation_dist;
    continuation_event.t_min = continuation_t_min;
    continuation_event.t_max = continuation_t_max;
    continuation_event.uv[0] = continuation_uv[0];
    continuation_event.uv[1] = continuation_uv[1];
    continuation_event.source_box = (size_t)-1;
    continuation_event.source_root = (size_t)-1;
    continuation_event.source_kind =
	RT_BREP_TRACE_EVENT_SOURCE_SEAM_CONTINUATION;
    continuation_event.edge_index = selected_edge.edge_index;
    continuation_event.vertex_index = -1;
    continuation_event.face_index = continuation_face;
    continuation_event.span_index = continuation_span;
    continuation_event.certificate = contact_pair ?
	RT_BREP_TRACE_EVENT_SEAM_CONTACT_CONTINUATION :
	RT_BREP_TRACE_EVENT_SEAM_CONTINUATION;
    continuation_event.hit_class = brep_hit::CRACK_HIT;
    continuation_event.trim_status = 1;
    continuation_event.adjacent_face_index = continuation_adjacency;
    continuation_event.direction = continuation_direction;

    trace->physical_event_attempts += trace->stored_surface_boxes;
    trace->physical_event_direction_checks += 2 +
	(contact_pair ? 2 : 0);
    trace->physical_event_near_trim += witness_boxes + contact_boxes;
    trace->physical_event_seam += 2;
    trace->physical_event_seam_witness_boxes += witness_boxes;
    trace->physical_event_seam_witness_roots += witness_roots;
    if (contact_pair) {
	trace->physical_event_seam_contact_pairs++;
	trace->physical_event_seam_contact_boxes += contact_boxes;
	trace->physical_event_seam_contact_roots += 2;
	trace->physical_event_seam_contact_miss_roots += contact_miss_roots;
	if (!perpendicular_contact) {
	    trace->physical_event_seam_oblique_pairs++;
	    trace->physical_event_seam_oblique_cells += oblique_frame_cells;
	    trace->physical_event_seam_oblique_box_links += oblique_box_links;
	}
    }
    if (source_union_certified) {
	trace->physical_event_seam_source_union_certified++;
	trace->physical_event_seam_source_union_boxes += source_union_boxes;
	trace->physical_event_seam_source_union_root_boxes +=
	    source_union_root_boxes;
    }
    brep_trace_finalize_physical_events(trace, ray, tol, true);
    if (trace->physical_event_complete == 1)
	trace->physical_event_seam_certified++;
    else
	trace->physical_event_seam_failures++;
    return true;
}


static bool
brep_prepared_fold_pair_eligible(const struct rt_brep_shot_trace *trace)
{
    if (!trace || trace->surface_isolated_boxes != 2 ||
	    trace->surface_krawczyk_boxes != 0 ||
	    trace->surface_fold_complete != 2 ||
	    trace->surface_fold_roots != 2 ||
	    trace->stored_surface_fold_roots != 2 ||
	    trace->surface_fold_root_overflow ||
	    trace->surface_fold_root_failures ||
	    trace->surface_fold_localization_failures ||
	    trace->surface_fold_direction_checks != 2 ||
	    trace->surface_fold_direction_mismatches ||
	    trace->surface_fold_trim_queries != 2 ||
	    trace->surface_fold_trim_failures ||
	    trace->surface_fold_topology_pairs != 1 ||
	    trace->surface_fold_duplicate_events ||
	    trace->surface_fold_material_pairs != 1 ||
	    trace->surface_fold_void_pairs ||
	    trace->surface_fold_resolved_pairs != 1 ||
	    trace->surface_fold_subminimum_contacts ||
	    trace->surface_fold_tolerance_ambiguous ||
	    trace->surface_fold_unmatched_roots)
	return false;

    const struct rt_brep_trace_fold_root *lower =
	&trace->surface_fold_roots_data[0];
    const struct rt_brep_trace_fold_root *upper =
	&trace->surface_fold_roots_data[1];
    if (upper->dist < lower->dist)
	std::swap(lower, upper);
    if (lower->trim_status != 0 || upper->trim_status != 0 ||
	    lower->hit_class != brep_hit::CLEAN_HIT ||
	    upper->hit_class != brep_hit::CLEAN_HIT ||
	    lower->direction != brep_hit::ENTERING ||
	    upper->direction != brep_hit::LEAVING ||
	    !(lower->t_max < upper->t_min) ||
	    !(trace->surface_fold_pair_gap_min >
	    trace->surface_fold_minimum_t))
	return false;

    for (size_t local_index = 0;
	    local_index < trace->stored_local_roots; ++local_index) {
	bool matched = false;
	for (size_t fold_index = 0;
		fold_index < trace->stored_surface_fold_roots; ++fold_index) {
	    if (brep_fold_root_matches_local(
		    trace->surface_fold_roots_data[fold_index],
		    trace->local_roots[local_index])) {
		matched = true;
		break;
	    }
	}
	/* An ill-conditioned local corrector may collapse the two fold roots
	 * onto one observation between them.  It is redundant only when it
	 * remains on one of the two proven adjacent spans and inside the
	 * certified material-pair hull. */
	const double pair_scale = std::max(1.0,
	    std::max(fabs(lower->t_min), fabs(upper->t_max)));
	const double pair_tolerance = 128.0 * DBL_EPSILON * pair_scale;
	if (!matched) {
	    const struct rt_brep_trace_local_root &local =
		trace->local_roots[local_index];
	    matched = local.face_index == lower->face_index &&
		(local.span_index == lower->span_index ||
		 local.span_index == upper->span_index) &&
		local.dist >= lower->t_min - pair_tolerance &&
		local.dist <= upper->t_max + pair_tolerance;
	}
	if (!matched)
	    return false;
    }
    return true;
}


static void
brep_trace_fold_physical_events(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    if (!trace || !bs || !bs->brep)
	return;
    bool complete = brep_prepared_fold_pair_eligible(trace) &&
	trace->prepared_surface_spans && !trace->unsupported_surface_faces &&
	trace->supported_surface_faces == bs->face_records.size() &&
	trace->candidate_surface_spans + trace->excluded_surface_spans ==
	    trace->prepared_surface_spans &&
	!trace->surface_workspace_exhausted &&
	!trace->surface_clip_restriction_failures &&
	!trace->surface_box_overflow &&
	trace->surface_isolated_boxes == trace->stored_surface_boxes &&
	!trace->local_root_overflow && !trace->local_trim_failures &&
	trace->local_root_candidates == trace->stored_local_roots;
    bool root_owned[RT_BREP_TRACE_MAX_FOLD_ROOTS] = {};

    for (size_t box_index = 0; box_index < trace->stored_surface_boxes;
	    ++box_index) {
	struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	trace->physical_event_attempts++;
	size_t matching_root = 0;
	size_t matches = 0;
	for (size_t root_index = 0;
		root_index < trace->stored_surface_fold_roots; ++root_index) {
	    if (!brep_prepared_box_matches_fold_root(box,
		    trace->surface_fold_roots_data[root_index], ray, tol))
		continue;
	    matching_root = root_index;
	    matches++;
	}
	if (matches != 1 || root_owned[matching_root]) {
	    trace->physical_event_unresolved++;
	    complete = false;
	    continue;
	}
	root_owned[matching_root] = true;
	const struct rt_brep_trace_fold_root &root =
	    trace->surface_fold_roots_data[matching_root];
	if (!root.determinant_sign || root.face_index < 0 ||
		root.face_index >= bs->brep->m_F.Count() ||
		root.hit_class != brep_hit::CLEAN_HIT ||
		root.trim_status == 1) {
	    trace->physical_event_unresolved++;
	    complete = false;
	    continue;
	}
	int oriented_sign = root.determinant_sign;
	if (bs->brep->m_F[root.face_index].m_bRev)
	    oriented_sign = -oriented_sign;
	const int direction = oriented_sign < 0 ? brep_hit::ENTERING :
	    brep_hit::LEAVING;
	trace->physical_event_direction_checks++;
	if (root.direction != direction) {
	    trace->physical_event_direction_mismatches++;
	    trace->physical_event_unresolved++;
	    complete = false;
	    continue;
	}

	box.disposition = RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY;
	box.determinant_sign = root.determinant_sign;
	if (trace->stored_physical_events >=
		RT_BREP_TRACE_MAX_PHYSICAL_EVENTS) {
	    trace->physical_event_overflow++;
	    complete = false;
	    continue;
	}
	struct rt_brep_trace_physical_event &event =
	    trace->physical_events[trace->stored_physical_events++];
	event = {};
	event.dist = root.dist;
	event.t_min = root.t_min;
	event.t_max = root.t_max;
	event.uv[0] = root.uv[0];
	event.uv[1] = root.uv[1];
	event.source_box = box_index;
	event.source_box_count = 1;
	event.source_root = matching_root;
	event.source_kind = RT_BREP_TRACE_EVENT_SOURCE_FOLD_ROOT;
	event.edge_index = -1;
	event.vertex_index = -1;
	event.face_index = root.face_index;
	event.span_index = root.span_index;
	event.certificate = RT_BREP_TRACE_EVENT_BOUNDARY_FOLD;
	event.determinant_sign = root.determinant_sign;
	event.hit_class = root.hit_class;
	event.trim_status = root.trim_status;
	event.adjacent_face_index = root.adjacent_face_index;
	event.direction = direction;
	trace->physical_event_boundary++;
    }

    for (size_t root_index = 0;
	    complete && root_index < trace->stored_surface_fold_roots;
	    ++root_index) {
	if (!root_owned[root_index]) {
	    trace->physical_event_unresolved++;
	    complete = false;
	}
    }
    brep_trace_finalize_physical_events(trace, ray, tol, complete);
}


static bool
brep_prepared_mixed_fold_pair_indices(
    const struct rt_brep_shot_trace *trace, const ON_Ray &ray,
    const struct bn_tol *tol, size_t &fold_box_index,
    size_t &regular_box_index, size_t &fold_local_index,
    size_t &regular_local_index, brep_interval &gap)
{
    /* Fold localization tries a nested sequence of optional contractions.
     * Individual failed levels are telemetry, not failed proof obligations:
     * the stored fold interval is either the best certified contraction or
     * the complete parent box.  The strict outward gap test below consumes
     * that actual conservative interval. */
    if (!trace || trace->surface_isolated_boxes != 2 ||
	    trace->stored_surface_boxes != 2 ||
	    trace->surface_krawczyk_boxes != 1 ||
	    trace->surface_fold_complete != 1 ||
	    trace->surface_fold_roots != 1 ||
	    trace->stored_surface_fold_roots != 1 ||
	    trace->surface_fold_root_overflow ||
	    trace->surface_fold_root_failures ||
	    trace->surface_fold_direction_checks != 1 ||
	    trace->surface_fold_direction_mismatches ||
	    trace->surface_fold_trim_queries != 1 ||
	    trace->surface_fold_trim_failures ||
	    trace->surface_fold_topology_pairs ||
	    trace->surface_fold_duplicate_events ||
	    trace->surface_fold_material_pairs ||
	    trace->surface_fold_void_pairs ||
	    trace->surface_fold_resolved_pairs ||
	    trace->surface_fold_subminimum_contacts ||
	    trace->surface_fold_tolerance_ambiguous ||
	    trace->surface_fold_unmatched_roots != 1 ||
	    trace->stored_local_roots != 2 || trace->stored_physical_events)
	return false;

    const struct rt_brep_trace_fold_root &fold =
	trace->surface_fold_roots_data[0];
    if (fold.trim_status != 0 || fold.hit_class != brep_hit::CLEAN_HIT ||
	    !fold.determinant_sign || !std::isfinite(fold.dist) ||
	    !std::isfinite(fold.t_min) || !std::isfinite(fold.t_max) ||
	    fold.t_min > fold.dist || fold.dist > fold.t_max)
	return false;

    size_t fold_boxes = 0;
    size_t regular_boxes = 0;
    for (size_t box_index = 0; box_index < 2; ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	if (brep_prepared_box_matches_fold_root(box, fold, ray, tol)) {
	    fold_box_index = box_index;
	    fold_boxes++;
	}
	if (box.disposition == RT_BREP_TRACE_BOX_RESOLVED_REGULAR &&
		box.determinant_sign) {
	    regular_box_index = box_index;
	    regular_boxes++;
	}
    }
    if (fold_boxes != 1 || regular_boxes != 1 ||
	    fold_box_index == regular_box_index ||
	    trace->surface_boxes[fold_box_index].disposition !=
		RT_BREP_TRACE_BOX_UNRESOLVED)
	return false;

    size_t fold_locals = 0;
    size_t regular_locals = 0;
    for (size_t local_index = 0; local_index < 2; ++local_index) {
	const struct rt_brep_trace_local_root &local =
	    trace->local_roots[local_index];
	/* The safeguarded corrector is only a representative and may not land
	 * inside the much tighter certified fold-root t interval.  Ownership is
	 * by the complete fold parent box: its corridor theorem proves one root
	 * and its adjacent strip proof excludes another. */
	const bool matches_fold = brep_prepared_box_matches_local_root(
	    trace->surface_boxes[fold_box_index], local, ray, tol);
	const bool matches_regular = brep_prepared_box_matches_local_root(
	    trace->surface_boxes[regular_box_index], local, ray, tol);
	if (matches_fold) {
	    fold_local_index = local_index;
	    fold_locals++;
	}
	if (matches_regular) {
	    regular_local_index = local_index;
	    regular_locals++;
	}
	if (matches_fold == matches_regular || local.trim_status != 0 ||
		local.hit_class != brep_hit::CLEAN_HIT)
	    return false;
    }
    if (fold_locals != 1 || regular_locals != 1 ||
	    fold_local_index == regular_local_index)
	return false;
    const struct rt_brep_trace_local_root &regular =
	trace->local_roots[regular_local_index];
    if (regular.face_index != fold.face_index ||
	    regular.span_index != fold.span_index ||
	    regular.direction == fold.direction)
	return false;

    const brep_interval fold_interval = {fold.t_min, fold.t_max};
    const struct rt_brep_trace_surface_box &regular_box =
	trace->surface_boxes[regular_box_index];
    const brep_interval regular_interval = {
	regular_box.t_min, regular_box.t_max
    };
    const brep_interval *lower = &fold_interval;
    const brep_interval *upper = &regular_interval;
    int lower_direction = fold.direction;
    int upper_direction = regular.direction;
    if (regular.dist < fold.dist) {
	lower = &regular_interval;
	upper = &fold_interval;
	lower_direction = regular.direction;
	upper_direction = fold.direction;
    }
    brep_interval minimum_t;
    if (lower_direction != brep_hit::ENTERING ||
	    upper_direction != brep_hit::LEAVING ||
	    !brep_fold_minimum_t_interval(ray, tol, minimum_t) ||
	    brep_fold_gap_classify(*lower, *upper, minimum_t, gap) !=
		RT_BREP_FOLD_GAP_RESOLVED)
	return false;
    return true;
}


static bool
brep_prepared_mixed_fold_pair_eligible(
    const struct rt_brep_shot_trace *trace, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    size_t fold_box_index = 0;
    size_t regular_box_index = 0;
    size_t fold_local_index = 0;
    size_t regular_local_index = 0;
    brep_interval gap;
    return brep_prepared_mixed_fold_pair_indices(trace, ray, tol,
	fold_box_index, regular_box_index, fold_local_index,
	regular_local_index, gap);
}


static void
brep_trace_mixed_fold_physical_events(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    if (!trace || !bs || !bs->brep)
	return;
    size_t fold_box_index = 0;
    size_t regular_box_index = 0;
    size_t fold_local_index = 0;
    size_t regular_local_index = 0;
    brep_interval gap;
    if (!brep_prepared_mixed_fold_pair_indices(trace, ray, tol,
	    fold_box_index, regular_box_index, fold_local_index,
	    regular_local_index, gap))
	return;

    const struct rt_brep_trace_fold_root &fold =
	trace->surface_fold_roots_data[0];
    const struct rt_brep_trace_local_root &regular =
	trace->local_roots[regular_local_index];
    struct rt_brep_trace_surface_box &fold_box =
	trace->surface_boxes[fold_box_index];
    const struct rt_brep_trace_surface_box &regular_box =
	trace->surface_boxes[regular_box_index];
    int regular_sign = regular_box.determinant_sign;
    if (regular.face_index < 0 ||
	    regular.face_index >= bs->brep->m_F.Count())
	return;
    if (bs->brep->m_F[regular.face_index].m_bRev)
	regular_sign = -regular_sign;
    const int regular_direction = regular_sign < 0 ?
	brep_hit::ENTERING : brep_hit::LEAVING;
    trace->physical_event_direction_checks += 2;
    if (regular.direction != regular_direction) {
	trace->physical_event_direction_mismatches++;
	return;
    }

    fold_box.disposition = RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY;
    fold_box.determinant_sign = fold.determinant_sign;
    struct rt_brep_trace_physical_event &fold_event =
	trace->physical_events[trace->stored_physical_events++];
    fold_event = {};
    fold_event.dist = fold.dist;
    fold_event.t_min = fold.t_min;
    fold_event.t_max = fold.t_max;
    fold_event.uv[0] = fold.uv[0];
    fold_event.uv[1] = fold.uv[1];
    fold_event.source_box = fold_box_index;
    fold_event.source_box_count = 1;
    fold_event.source_root = 0;
    fold_event.source_kind = RT_BREP_TRACE_EVENT_SOURCE_FOLD_ROOT;
    fold_event.edge_index = -1;
    fold_event.vertex_index = -1;
    fold_event.face_index = fold.face_index;
    fold_event.span_index = fold.span_index;
    fold_event.certificate = RT_BREP_TRACE_EVENT_BOUNDARY_FOLD;
    fold_event.determinant_sign = fold.determinant_sign;
    fold_event.hit_class = fold.hit_class;
    fold_event.trim_status = fold.trim_status;
    fold_event.adjacent_face_index = fold.adjacent_face_index;
    fold_event.direction = fold.direction;

    struct rt_brep_trace_physical_event &regular_event =
	trace->physical_events[trace->stored_physical_events++];
    regular_event = {};
    regular_event.dist = regular.dist;
    regular_event.t_min = regular_box.t_min;
    regular_event.t_max = regular_box.t_max;
    regular_event.uv[0] = regular.uv[0];
    regular_event.uv[1] = regular.uv[1];
    regular_event.source_box = regular_box_index;
    regular_event.source_box_count = 1;
    regular_event.source_root = regular_local_index;
    regular_event.source_kind = RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT;
    regular_event.edge_index = -1;
    regular_event.vertex_index = -1;
    regular_event.face_index = regular.face_index;
    regular_event.span_index = regular.span_index;
    regular_event.certificate = RT_BREP_TRACE_EVENT_REGULAR_INTERIOR;
    regular_event.determinant_sign = regular_box.determinant_sign;
    regular_event.hit_class = regular.hit_class;
    regular_event.trim_status = regular.trim_status;
    regular_event.adjacent_face_index = regular.adjacent_face_index;
    regular_event.direction = regular_direction;

    trace->physical_event_attempts += 2;
    trace->physical_event_boundary++;
    trace->physical_event_regular++;
    trace->surface_fold_pair_gap_min = gap.minimum;
    trace->surface_fold_pair_gap_max = gap.maximum;
    brep_trace_finalize_physical_events(trace, ray, tol, true);
    if (trace->physical_event_complete == 1)
	trace->surface_fold_mixed_pairs++;
}


static void
brep_trace_regular_direction_counts(const struct rt_brep_shot_trace *trace,
    size_t &entering, size_t &leaving)
{
    entering = 0;
    leaving = 0;
    if (!trace)
	return;
    for (size_t root_index = 0;
	    root_index < trace->stored_local_roots; ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace->local_roots[root_index];
	if (root.trim_status == 1 ||
		(root.hit_class != brep_hit::CLEAN_HIT &&
		 root.hit_class != brep_hit::NEAR_HIT) ||
		!std::isfinite(root.normal_dot) ||
		fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL)
	    continue;
	if (root.direction == brep_hit::ENTERING)
	    entering++;
	else if (root.direction == brep_hit::LEAVING)
	    leaving++;
    }
}


static void
brep_trace_physical_events(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct bn_tol *tol)
{
    size_t regular_entering = 0;
    size_t regular_leaving = 0;
    brep_trace_regular_direction_counts(trace, regular_entering,
	regular_leaving);
    /* Krawczyk certifies a surface root, not completeness of the solid
     * boundary stream.  If the clean non-tangent directions are imbalanced,
     * give the seam-continuation theorem the same opportunity regardless of
     * which representation isolated the surviving root. */
    const bool seam_completion_needed = trace &&
	(trace->surface_isolated_boxes != trace->surface_krawczyk_boxes ||
	 regular_entering != regular_leaving);
    if (brep_prepared_fold_pair_eligible(trace))
	brep_trace_fold_physical_events(trace, bs, ray, tol);
    else if (brep_prepared_mixed_fold_pair_eligible(trace, ray, tol))
	brep_trace_mixed_fold_physical_events(trace, bs, ray, tol);
    else if (trace && trace->surface_isolated_boxes !=
	    trace->surface_krawczyk_boxes &&
	    brep_trace_vertex_physical_events(trace, bs, ray, tol))
	return;
    else if (trace &&
	    brep_trace_edge_physical_events(trace, bs, ray, tol))
	return;
    else if (seam_completion_needed &&
	    brep_trace_seam_physical_events(trace, bs, ray, tol))
	return;
    else
	brep_trace_regular_physical_events(trace, bs, ray, tol);
}


static bool
brep_prepared_seam_pair_eligible(const struct rt_brep_shot_trace *trace)
{
    if (!trace || trace->physical_event_complete != 1 ||
	    trace->physical_event_seam != 2 ||
	    trace->physical_event_seam_certified != 1 ||
	    trace->physical_event_seam_failures ||
	    trace->physical_event_unresolved ||
	    trace->physical_event_direction_mismatches ||
	    trace->physical_event_overflow ||
	    trace->physical_event_state_failures ||
	    trace->physical_event_material_segments != 1 ||
	    trace->physical_event_subminimum_contacts ||
	    trace->physical_event_tolerance_ambiguous ||
	    trace->stored_physical_events != 2)
	return false;
    const struct rt_brep_trace_physical_event *existing = NULL;
    const struct rt_brep_trace_physical_event *continuation = NULL;
    bool contact_pair = false;
    for (size_t event_index = 0; event_index < 2; ++event_index) {
	const struct rt_brep_trace_physical_event *event =
	    &trace->physical_events[event_index];
	if (event->certificate == RT_BREP_TRACE_EVENT_SEAM_EXISTING)
	    existing = event;
	else if (event->certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTINUATION)
	    continuation = event;
	else if (event->certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTACT_EXISTING) {
	    existing = event;
	    contact_pair = true;
	} else if (event->certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTACT_CONTINUATION) {
	    continuation = event;
	    contact_pair = true;
	} else {
	    return false;
	}
    }
    const bool matching_certificates = existing && continuation &&
	((!contact_pair &&
	  existing->certificate == RT_BREP_TRACE_EVENT_SEAM_EXISTING &&
	  continuation->certificate ==
	    RT_BREP_TRACE_EVENT_SEAM_CONTINUATION) ||
	 (contact_pair && existing->certificate ==
	    RT_BREP_TRACE_EVENT_SEAM_CONTACT_EXISTING &&
	  continuation->certificate ==
	    RT_BREP_TRACE_EVENT_SEAM_CONTACT_CONTINUATION));
    const bool matching_contact_evidence = contact_pair ?
	(trace->physical_event_seam_contact_pairs == 1 &&
	 trace->physical_event_seam_contact_boxes > 0 &&
	 trace->physical_event_seam_contact_roots == 2 &&
	 trace->physical_event_seam_contact_miss_roots <= 1 &&
	 !trace->physical_event_seam_witness_boxes &&
	 !trace->physical_event_seam_witness_roots &&
	 !trace->physical_event_seam_edge_only_candidates &&
	 trace->physical_event_near_trim ==
	    trace->physical_event_seam_contact_boxes &&
	 trace->physical_event_direction_checks == 4 &&
	 ((trace->physical_event_seam_oblique_pairs == 1 &&
	   trace->physical_event_seam_oblique_cells > 0 &&
	   trace->physical_event_seam_oblique_box_links ==
	    trace->physical_event_seam_contact_boxes) ||
	  (!trace->physical_event_seam_oblique_pairs &&
	   !trace->physical_event_seam_oblique_cells &&
	   !trace->physical_event_seam_oblique_box_links))) :
	(!trace->physical_event_seam_contact_pairs &&
	 !trace->physical_event_seam_contact_boxes &&
	 !trace->physical_event_seam_contact_roots &&
	 !trace->physical_event_seam_contact_miss_roots &&
	 !trace->physical_event_seam_oblique_pairs &&
	 !trace->physical_event_seam_oblique_cells &&
	 !trace->physical_event_seam_oblique_box_links &&
	 trace->physical_event_near_trim ==
	    trace->physical_event_seam_witness_boxes &&
	 trace->physical_event_direction_checks == 2 &&
	 ((trace->physical_event_seam_witness_roots &&
	   trace->physical_event_seam_witness_boxes &&
	   !trace->physical_event_seam_edge_only_candidates) ||
	  (!trace->physical_event_seam_witness_roots &&
	   !trace->physical_event_seam_witness_boxes &&
	   trace->physical_event_seam_edge_only_candidates == 1)));
    const bool matching_source_union_evidence =
	(!trace->physical_event_seam_source_union_certified &&
	 !trace->physical_event_seam_source_union_boxes &&
	 !trace->physical_event_seam_source_union_root_boxes) ||
	(trace->physical_event_seam_source_union_certified == 1 && existing &&
	 trace->physical_event_seam_source_union_boxes ==
	    existing->source_box_count &&
	 trace->physical_event_seam_source_union_root_boxes > 0 &&
	 trace->physical_event_seam_source_union_root_boxes <
	    trace->physical_event_seam_source_union_boxes);
    const bool matching_local_root_evidence =
	trace->physical_event_seam_declared_contact_pairs ?
	(trace->physical_event_seam_declared_contact_pairs == 1 &&
	 contact_pair &&
	 trace->physical_event_seam_contact_miss_roots == 1 &&
	 trace->physical_event_seam_local_root_attempts == 1 &&
	 trace->physical_event_seam_local_root_available == 1 &&
	 trace->physical_event_seam_local_root_certified == 1 &&
	 trace->physical_event_seam_local_root_extensions <= 1 &&
	 !trace->physical_event_seam_local_root_tube_failures &&
	 trace->physical_event_seam_local_root_radius > 0.0 &&
	 trace->physical_event_seam_local_root_model_image >= 0.0 &&
	 std::isfinite(trace->physical_event_seam_local_root_edge_upper) &&
	 std::isfinite(trace->physical_event_seam_local_root_trim_upper) &&
	 trace->physical_event_seam_local_root_edge_upper <=
	    trace->physical_event_seam_local_root_tube_tolerance +
	    trace->physical_event_seam_local_root_tube_roundoff &&
	 trace->physical_event_seam_local_root_trim_upper <=
	    trace->physical_event_seam_local_root_tube_tolerance +
	    trace->physical_event_seam_local_root_tube_roundoff) :
	(!trace->physical_event_seam_local_root_attempts &&
	 !trace->physical_event_seam_local_root_available &&
	 !trace->physical_event_seam_local_root_certified &&
	 !trace->physical_event_seam_local_root_extensions &&
	 !trace->physical_event_seam_local_root_tube_failures);
    return matching_certificates && matching_contact_evidence &&
	matching_source_union_evidence && matching_local_root_evidence &&
	existing->edge_index >= 0 &&
	existing->edge_index == continuation->edge_index &&
	existing->source_kind == RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT &&
	existing->source_box_count > 0 &&
	continuation->source_kind ==
	    RT_BREP_TRACE_EVENT_SOURCE_SEAM_CONTINUATION &&
	continuation->source_box_count == 0 &&
	((existing->direction == brep_hit::ENTERING &&
	  continuation->direction == brep_hit::LEAVING) ||
	 (continuation->direction == brep_hit::ENTERING &&
	  existing->direction == brep_hit::LEAVING));
}


static bool
brep_prepared_vertex_events_eligible(const struct rt_brep_shot_trace *trace)
{
    if (!trace || trace->physical_event_complete != 1 ||
	    !trace->physical_event_vertex ||
	    trace->physical_event_vertex_certified != 1 ||
	    trace->physical_event_vertex_failures ||
	    trace->physical_event_unresolved ||
	    trace->physical_event_direction_mismatches ||
	    trace->physical_event_overflow ||
	    trace->physical_event_state_failures ||
	    trace->physical_event_subminimum_contacts ||
	    trace->physical_event_tolerance_ambiguous ||
	    !trace->physical_event_vertex_owned_boxes ||
	    !trace->physical_event_vertex_owned_roots ||
	    trace->stored_physical_events !=
	    2 * trace->physical_event_material_segments)
	return false;
    size_t vertex_events = 0;
    for (size_t event_index = 0;
	    event_index < trace->stored_physical_events; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace->physical_events[event_index];
	if (event.certificate != RT_BREP_TRACE_EVENT_VERTEX_FAN)
	    continue;
	if (event.source_kind != RT_BREP_TRACE_EVENT_SOURCE_VERTEX_FAN ||
		event.vertex_index < 0 || event.edge_index != -1 ||
		!event.source_box_count ||
		event.hit_class != brep_hit::CRACK_HIT)
	    return false;
	vertex_events++;
    }
    return vertex_events == trace->physical_event_vertex;
}


static bool
brep_prepared_edge_events_eligible(const struct rt_brep_shot_trace *trace)
{
    if (!trace || trace->physical_event_complete != 1 ||
	    trace->physical_event_edge_attempts != 1 ||
	    trace->physical_event_edge_certified != 1 ||
	    trace->physical_event_edge_failures ||
	    trace->physical_event_unresolved ||
	    trace->physical_event_direction_mismatches ||
	    trace->physical_event_overflow ||
	    trace->physical_event_state_failures ||
	    trace->physical_event_subminimum_contacts ||
	    trace->physical_event_tolerance_ambiguous ||
	    !trace->physical_event_edge_candidates ||
	    trace->physical_event_edge_candidates !=
		trace->physical_event_edge +
		trace->physical_event_edge_contacts ||
	    !trace->physical_event_edge_owned_boxes ||
	    !trace->physical_event_edge_owned_roots ||
	    trace->stored_physical_events !=
		2 * trace->physical_event_material_segments)
	return false;
    size_t edge_events = 0;
    for (size_t event_index = 0;
	    event_index < trace->stored_physical_events; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace->physical_events[event_index];
	if (event.certificate != RT_BREP_TRACE_EVENT_MANIFOLD_EDGE)
	    continue;
	if (event.source_kind !=
		RT_BREP_TRACE_EVENT_SOURCE_MANIFOLD_EDGE ||
		event.edge_index < 0 || event.vertex_index != -1 ||
		!event.source_box_count ||
		event.hit_class != brep_hit::CRACK_HIT)
	    return false;
	edge_events++;
    }
    return edge_events == trace->physical_event_edge;
}


static int
brep_build_prepared_event_partition(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    brep_hit_workspace &hits)
{
    if (!trace || !bs || !bs->brep || trace->physical_event_complete != 1 ||
	    trace->physical_event_unresolved ||
	    trace->physical_event_direction_mismatches ||
	    trace->physical_event_overflow ||
	    trace->physical_event_state_failures ||
	    trace->physical_event_subminimum_contacts ||
	    trace->physical_event_tolerance_ambiguous ||
	    trace->stored_physical_events !=
	    2 * trace->physical_event_material_segments)
	return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;

    size_t fold_events = 0;
    size_t seam_existing_events = 0;
    size_t seam_continuation_events = 0;
    size_t vertex_events = 0;
    size_t edge_events = 0;
    for (size_t event_index = 0;
	    event_index < trace->stored_physical_events; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace->physical_events[event_index];
	if ((event.direction != brep_hit::ENTERING &&
		 event.direction != brep_hit::LEAVING) ||
		!std::isfinite(event.dist) ||
		!std::isfinite(event.t_min) ||
		!std::isfinite(event.t_max) ||
		event.t_min > event.dist || event.dist > event.t_max)
	    return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	if (event.certificate == RT_BREP_TRACE_EVENT_REGULAR_INTERIOR ||
		event.certificate == RT_BREP_TRACE_EVENT_BOUNDARY_FOLD) {
	    if (event.hit_class != brep_hit::CLEAN_HIT ||
		    event.trim_status == 1 || event.edge_index != -1 ||
		    event.vertex_index != -1)
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	} else if (event.certificate ==
		RT_BREP_TRACE_EVENT_SEAM_EXISTING ||
	    event.certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTACT_EXISTING) {
	    if ((event.hit_class != brep_hit::CLEAN_HIT &&
		    event.hit_class != brep_hit::NEAR_HIT) ||
		    event.trim_status == 1 || event.edge_index < 0 ||
		    event.vertex_index != -1 || !event.source_box_count)
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	    seam_existing_events++;
	} else if (event.certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTINUATION ||
	    event.certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTACT_CONTINUATION) {
	    if (event.hit_class != brep_hit::CRACK_HIT ||
		    event.trim_status != 1 || event.edge_index < 0 ||
		    event.vertex_index != -1 || event.source_box_count)
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	    seam_continuation_events++;
	} else if (event.certificate == RT_BREP_TRACE_EVENT_VERTEX_FAN) {
	    if (event.hit_class != brep_hit::CRACK_HIT ||
		    event.edge_index != -1 || event.vertex_index < 0 ||
		    event.source_kind !=
		    RT_BREP_TRACE_EVENT_SOURCE_VERTEX_FAN ||
		    !event.source_box_count)
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	    vertex_events++;
	} else if (event.certificate ==
		RT_BREP_TRACE_EVENT_MANIFOLD_EDGE) {
	    if (event.hit_class != brep_hit::CRACK_HIT ||
		    event.edge_index < 0 || event.vertex_index != -1 ||
		    event.source_kind !=
			RT_BREP_TRACE_EVENT_SOURCE_MANIFOLD_EDGE ||
		    !event.source_box_count)
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	    edge_events++;
	} else {
	    return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	}
	brep_hit hit;
	if (!brep_trace_make_hit(bs, ray, event.face_index, event.dist,
		event.uv, event.trim_status, event.hit_class, event.direction,
		event.adjacent_face_index, true, hit))
	    return RT_BREP_PREPARED_FALLBACK_HIT_BUILD;
	hits.push_back(hit);
	if (event.certificate == RT_BREP_TRACE_EVENT_BOUNDARY_FOLD)
	    fold_events++;
    }
    if (hits.overflow())
	return RT_BREP_PREPARED_FALLBACK_HIT_WORKSPACE;
    hits.sort();
    if (hits.size() != trace->stored_physical_events)
	return RT_BREP_PREPARED_FALLBACK_HIT_WORKSPACE;
    for (size_t hit_index = 0; hit_index < hits.size(); ++hit_index) {
	const int expected_direction = hit_index % 2 ? brep_hit::LEAVING :
	    brep_hit::ENTERING;
	if (hits[hit_index].direction != expected_direction ||
		!std::isfinite(hits[hit_index].dist) ||
		(hit_index &&
		 !(hits[hit_index - 1].dist < hits[hit_index].dist)))
	    return RT_BREP_PREPARED_FALLBACK_PARTITION;
    }
    if (fold_events) {
	const bool full_fold_pair = fold_events == 2 && hits.size() == 2;
	const bool mixed_fold_pair = fold_events == 1 && hits.size() == 2 &&
	    trace->surface_fold_mixed_pairs == 1;
	if (!full_fold_pair && !mixed_fold_pair)
	    return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	trace->surface_fold_promoted_pairs++;
    }
    if (seam_existing_events || seam_continuation_events) {
	if (seam_existing_events != 1 || seam_continuation_events != 1 ||
		hits.size() != 2 || !brep_prepared_seam_pair_eligible(trace))
	    return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
    }
    if (vertex_events && (vertex_events != trace->physical_event_vertex ||
	    !brep_prepared_vertex_events_eligible(trace)))
	return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
    if ((edge_events || trace->physical_event_edge_contacts) &&
	    (edge_events != trace->physical_event_edge ||
	     !brep_prepared_edge_events_eligible(trace)))
	return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
    return RT_BREP_PREPARED_FALLBACK_NONE;
}


static int
brep_build_prepared_partition(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct xray &xray, const struct bn_tol *tol,
    brep_hit_workspace &hits)
{
    if (!trace || !bs || !bs->brep)
	return RT_BREP_PREPARED_FALLBACK_UNSUPPORTED;
    if (bs->plate_mode)
	return RT_BREP_PREPARED_FALLBACK_PLATE;
    if (!bs->is_solid)
	return RT_BREP_PREPARED_FALLBACK_NON_SOLID;
    if (!trace->prepared_surface_spans || trace->unsupported_surface_faces ||
	    trace->supported_surface_faces != bs->face_records.size() ||
	    trace->candidate_surface_spans + trace->excluded_surface_spans !=
	    trace->prepared_surface_spans)
	return RT_BREP_PREPARED_FALLBACK_UNSUPPORTED;
    if (trace->surface_workspace_exhausted ||
	    trace->surface_clip_restriction_failures)
	return RT_BREP_PREPARED_FALLBACK_SURFACE_WORKSPACE;
    if (trace->surface_box_overflow ||
	    trace->surface_isolated_boxes != trace->stored_surface_boxes)
	return RT_BREP_PREPARED_FALLBACK_SURFACE_BOXES;
    const bool fold_pair = brep_prepared_fold_pair_eligible(trace);
    const bool mixed_fold_pair = trace->surface_fold_mixed_pairs == 1;
    const bool seam_pair = brep_prepared_seam_pair_eligible(trace);
    const bool vertex_events = brep_prepared_vertex_events_eligible(trace);
    const bool edge_events = brep_prepared_edge_events_eligible(trace);
    if (trace->surface_isolated_boxes != trace->surface_krawczyk_boxes &&
	    !fold_pair && !mixed_fold_pair && !seam_pair && !vertex_events &&
	    !edge_events)
	return RT_BREP_PREPARED_FALLBACK_UNCERTIFIED;
    if (trace->local_root_overflow || trace->local_trim_failures ||
	    trace->local_cluster_overflow ||
	    trace->local_root_candidates != trace->stored_local_roots)
	return RT_BREP_PREPARED_FALLBACK_LOCAL_WORKSPACE;
    if (trace->physical_event_complete != 1 ||
	    trace->physical_event_unresolved ||
	    trace->physical_event_direction_mismatches ||
	    trace->physical_event_overflow ||
	    trace->physical_event_state_failures ||
	    trace->physical_event_tolerance_ambiguous)
	return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;

    /* A status-2 NURBS form has an exact common locus, but its parameter map
     * is currently certified only pointwise.  Regular and complete clean-
     * interior fold proofs live entirely in NURBS parameter space and map only
     * the final point for trim, normal, and hit publication.  Seam, edge, and
     * vertex events need interval enclosures of the map and fail closed here. */
    if (trace->reparameterized_surface_faces) {
	for (size_t box_index = 0;
		box_index < trace->stored_surface_boxes; ++box_index) {
	    const struct rt_brep_trace_surface_box &box =
		trace->surface_boxes[box_index];
	    const brep_face_record *record = brep_face_surface_record(bs,
		box.face_index);
	    if (record && record->nurb_form_status == 2 &&
		    (box.disposition != RT_BREP_TRACE_BOX_RESOLVED_REGULAR &&
		     box.disposition != RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY))
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	    if (record && record->nurb_form_status == 2 &&
		    !box.determinant_sign)
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	}
	for (size_t root_index = 0;
		root_index < trace->stored_local_roots; ++root_index) {
	    const struct rt_brep_trace_local_root &root =
		trace->local_roots[root_index];
	    const brep_face_record *record = brep_face_surface_record(bs,
		root.face_index);
	    if (record && record->nurb_form_status == 2 &&
		    (root.hit_class != brep_hit::CLEAN_HIT &&
		     root.hit_class != brep_hit::CLEAN_MISS))
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	}
	for (size_t event_index = 0;
		event_index < trace->stored_physical_events; ++event_index) {
	    const struct rt_brep_trace_physical_event &event =
		trace->physical_events[event_index];
	    const brep_face_record *record = brep_face_surface_record(bs,
		event.face_index);
	    if (record && record->nurb_form_status == 2 &&
		    (event.certificate !=
			RT_BREP_TRACE_EVENT_REGULAR_INTERIOR &&
		     event.certificate !=
			RT_BREP_TRACE_EVENT_BOUNDARY_FOLD))
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	    if (record && record->nurb_form_status == 2 &&
		    (event.edge_index != -1 || event.vertex_index != -1))
		return RT_BREP_PREPARED_FALLBACK_EVENT_CLASS;
	}
    }

    for (size_t box_index = 0; box_index < trace->stored_surface_boxes;
	    ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace->surface_boxes[box_index];
	if (brep_prepared_box_has_root(trace, box, ray, tol))
	    continue;
	/* A rootless terminal box is normally incomplete surface evidence.  The
	 * sole exception is a box already resolved by the complete oblique
	 * contact transaction: every such box owns a certified seam/frame cell,
	 * and the link count covers the complete contact-box set. */
	const bool certified_contact_corridor =
	    box.disposition == RT_BREP_TRACE_BOX_RESOLVED_CONTACT &&
	    !box.determinant_sign &&
	    trace->physical_event_seam_oblique_pairs == 1 &&
	    trace->physical_event_seam_oblique_cells > 0 &&
	    trace->physical_event_seam_oblique_box_links ==
		trace->physical_event_seam_contact_boxes &&
	    trace->physical_event_seam_contact_boxes > 0;
	const bool certified_source_union = seam_pair &&
	    box.disposition == RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY &&
	    !box.determinant_sign &&
	    trace->physical_event_seam_source_union_certified == 1 &&
	    trace->physical_event_seam_source_union_boxes >
		trace->physical_event_seam_source_union_root_boxes &&
	    trace->physical_event_seam_source_union_root_boxes > 0;
	if (!certified_contact_corridor && !certified_source_union)
	    return RT_BREP_PREPARED_FALLBACK_ROOT_COVERAGE;
    }

    (void)xray;
    return brep_build_prepared_event_partition(trace, bs, ray, hits);
}


static int
brep_prepared_object_fallback(const struct brep_specific *bs)
{
    if (!bs || !bs->brep)
	return RT_BREP_PREPARED_FALLBACK_UNSUPPORTED;
    if (bs->plate_mode)
	return RT_BREP_PREPARED_FALLBACK_PLATE;
    if (!bs->is_solid)
	return RT_BREP_PREPARED_FALLBACK_NON_SOLID;
    if (bs->face_records.empty() || bs->surface_spans.empty())
	return RT_BREP_PREPARED_FALLBACK_UNSUPPORTED;
    for (std::vector<brep_face_record>::const_iterator record =
	    bs->face_records.begin(); record != bs->face_records.end();
	    ++record) {
	if (!record->supported)
	    return RT_BREP_PREPARED_FALLBACK_UNSUPPORTED;
    }
    return RT_BREP_PREPARED_FALLBACK_NONE;
}


static int
brep_try_prepared_partition(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const struct xray &xray, const struct bn_tol *tol,
    brep_hit_workspace &hits)
{
    trace->prepared_production_attempts++;
    const int object_fallback = brep_prepared_object_fallback(bs);
    if (object_fallback != RT_BREP_PREPARED_FALLBACK_NONE) {
	trace->prepared_production_fallback = object_fallback;
	return object_fallback;
    }
    trace->prepared_vertex_records = bs->vertex_records.size();
    for (std::vector<brep_vertex_record>::const_iterator record_it =
	    bs->vertex_records.begin(); record_it != bs->vertex_records.end();
	    ++record_it)
	if (record_it->supported)
	    trace->supported_vertex_records++;
    brep_trace_surface_spans(trace, bs, ray, tol);
    brep_trace_fold_events(trace, bs, ray, tol);
    brep_trace_isolated_roots(trace, bs, ray);
    brep_trace_local_clusters(trace, tol);
    brep_trace_physical_events(trace, bs, ray, tol);
    /* Publication is authorized only after every retained root box and the
     * complete physical partition pass the conservative qualification above.
     * Any other result leaves the legacy SurfaceTree path untouched. */
    const int fallback = brep_build_prepared_partition(trace, bs, ray,
	xray, tol, hits);
    trace->prepared_production_fallback = fallback;
    trace->prepared_production_hits = hits.size();
    if (fallback == RT_BREP_PREPARED_FALLBACK_NONE)
	trace->prepared_production_eligible++;
    return fallback;
}


static int
brep_try_prepared_partition(const struct brep_specific *bs,
    const ON_Ray &ray, const struct xray &xray, const struct bn_tol *tol,
    brep_hit_workspace &hits)
{
    struct rt_brep_shot_trace trace = {};
    trace.closure_edge_index = -1;
    trace.closure_missing_direction = -1;
    trace.continuation_face_index = -1;
    trace.continuation_span_index = -1;
    trace.continuation_adjacent_face_index = -99;
    return brep_try_prepared_partition(&trace, bs, ray, xray, tol, hits);
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
    const ON_Surface* surf = hit.face->SurfaceOf();
    const ON_BrepFace& face = *hit.face;

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


static int
emit_fixed_brep_hits(const brep_hit_workspace &hits, struct soltab *stp,
    struct xray *ray, struct application *ap, struct seg *seghead,
    const struct brep_specific *bs)
{
    if (bs->plate_mode) {
	for (size_t i = 0; i < hits.size(); ++i) {
	    const brep_hit &in = hits[i];
	    const brep_hit &out = hits[i];
	    const double los = brep_platemode_thickness(*ray, in, *bs);

	    struct seg *segp;
	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;

	    segp->seg_in.hit_dist = in.dist - los * 0.5;
	    segp->seg_in.hit_surfno = in.face->m_face_index;
	    VSET(segp->seg_in.hit_vpriv, in.uv[0], in.uv[1], 0.0);
	    VMOVE(segp->seg_in.hit_normal, in.normal);
	    VJOIN1(segp->seg_in.hit_point, ray->r_pt,
		segp->seg_in.hit_dist, ray->r_dir);
	    segp->seg_in.hit_rayp = &ap->a_ray;

	    segp->seg_out.hit_dist = out.dist + los * 0.5;
	    segp->seg_out.hit_surfno = out.face->m_face_index;
	    VSET(segp->seg_out.hit_vpriv, out.uv[0], out.uv[1], 0.0);
	    VREVERSE(segp->seg_out.hit_normal, out.normal);
	    VJOIN1(segp->seg_out.hit_point, ray->r_pt,
		segp->seg_out.hit_dist, ray->r_dir);
	    segp->seg_out.hit_rayp = &ap->a_ray;

	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	}
	return (int)hits.size();
    }

    if (hits.size() <= 1 || hits.size() % 2 != 0)
	return 0;

    for (size_t i = 0; i < hits.size(); i += 2) {
	const brep_hit &in = hits[i];
	const brep_hit &out = hits[i + 1];
	struct seg *segp;
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	VMOVE(segp->seg_in.hit_point, in.point);
	VMOVE(segp->seg_in.hit_normal, in.normal);
	segp->seg_in.hit_dist = in.dist;
	segp->seg_in.hit_surfno = in.face->m_face_index;
	VSET(segp->seg_in.hit_vpriv, in.uv[0], in.uv[1], 0.0);

	VMOVE(segp->seg_out.hit_point, out.point);
	VMOVE(segp->seg_out.hit_normal, out.normal);
	segp->seg_out.hit_dist = out.dist;
	segp->seg_out.hit_surfno = out.face->m_face_index;
	VSET(segp->seg_out.hit_vpriv, out.uv[0], out.uv[1], 0.0);

	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }
    return (int)hits.size();
}


/**
 * Intersect a ray with a brep.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
static int
rt_brep_shot_impl(struct soltab *stp, struct xray *rp,
    struct application *ap, struct seg *seghead,
    struct rt_brep_shot_trace *trace, bool allow_prepared)
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
    ON_Ray r = toXRay(rp);
    const struct bn_tol *tol = stp->st_rtip ? &stp->st_rtip->rti_tol : NULL;
    brep_hit_workspace prepared_hits;
    int prepared_fallback = RT_BREP_PREPARED_FALLBACK_UNSUPPORTED;
    if (trace)
	prepared_fallback = brep_try_prepared_partition(trace, bs, r, *rp,
	    tol, prepared_hits);
    else if (allow_prepared)
	prepared_fallback = brep_try_prepared_partition(bs, r, *rp, tol,
	    prepared_hits);
    const bool prepared_selected = allow_prepared &&
	prepared_fallback == RT_BREP_PREPARED_FALLBACK_NONE;
    if (trace && prepared_selected)
	trace->prepared_production_selected++;
    if (!trace && prepared_selected)
	return emit_fixed_brep_hits(prepared_hits, stp, rp, ap, seghead, bs);

    const BBNode *fixed_leaves[RT_BREP_MAX_LEAVES] = {};
    size_t fixed_leaf_count = 0;
    bool fixed_leaf_overflow = false;
    bs->bvh->intersectsHierarchy(r, fixed_leaves,
	RT_BREP_MAX_LEAVES, fixed_leaf_count, fixed_leaf_overflow);

    /* Overflow is resolved before any surface solve or hit mutation.  Trace
     * mode also builds the list so the fixed path remains equivalence-gated. */
    std::list<const BBNode*> fallback_leaves;
    if (fixed_leaf_overflow || trace)
	bs->bvh->intersectsHierarchy(r, fallback_leaves);
    if (trace) {
	trace->intersected_leaves = fallback_leaves.size();
	trace->fixed_leaf_count = fixed_leaf_count;
	trace->fixed_leaf_stored = std::min(trace->fixed_leaf_count,
	    (size_t)RT_BREP_MAX_LEAVES);
	trace->fixed_leaf_overflow = fixed_leaf_overflow ? 1 : 0;
	trace->fixed_leaf_fallback = fixed_leaf_overflow ? 1 : 0;
	if (!fixed_leaf_overflow &&
		trace->fixed_leaf_count == fallback_leaves.size()) {
	    size_t leaf_index = 0;
	    for (std::list<const BBNode*>::const_iterator i =
		    fallback_leaves.begin(); i != fallback_leaves.end();
		    ++i, ++leaf_index) {
		if (fixed_leaves[leaf_index] != *i)
		    trace->fixed_leaf_mismatches++;
	    }
	} else {
	    trace->fixed_leaf_mismatches++;
	}
    }
    brep_observe_edges(trace, bs, r);
    if (!fixed_leaf_count) {
	brep_trace_root_coverage(trace);
	if (trace) {
	    const std::list<brep_hit> empty_hits;
	    brep_trace_prepared_event_cleanup(trace, bs, r, *rp, tol,
		empty_hits, NULL);
	}
	if (trace && prepared_selected) {
	    trace->final_segments = prepared_hits.size() / 2;
	    return emit_fixed_brep_hits(prepared_hits, stp, rp, ap, seghead,
		bs);
	}
	return 0; // MISS
    }

    /* Collect into the fixed workspace in production.  Trace mode also
     * retains the legacy list so each transition stage remains gated. */
    std::list<brep_hit> hits;
    brep_hit_workspace fixed_hits;
    collect_brep_hits(fixed_leaves, fixed_leaf_count, fallback_leaves,
	fixed_leaf_overflow, r, trace ? &hits : NULL, &fixed_hits, trace);
    fixed_hits.sort();
    brep_trace_root_coverage(trace);

    if (!trace && !fixed_hits.overflow()) {
	(void)cleanup_fixed_brep_hits(fixed_hits, *rp, tol);
	(void)repair_fixed_brep_crack(fixed_hits, bs, r);
	return emit_fixed_brep_hits(fixed_hits, stp, rp, ap, seghead, bs);
    }

    if (!trace) {
	/* Capacity overflow is known before cleanup or segment publication.
	 * Re-run the deterministic solve into the legacy allocating container. */
	collect_brep_hits(fixed_leaves, fixed_leaf_count, fallback_leaves,
	    fixed_leaf_overflow, r, &hits, NULL, NULL);
    }
    hits.sort();
    if (trace) {
	trace->raw_hits = hits.size();
	trace->fixed_hit_count = fixed_hits.total();
	trace->fixed_hit_stored = fixed_hits.size();
	trace->fixed_hit_overflow = fixed_hits.overflow() ? 1 : 0;
	trace->fixed_hit_fallback = fixed_hits.overflow() ? 1 : 0;
	if (!fixed_hits.overflow() && fixed_hits.size() == hits.size()) {
	    size_t hit_index = 0;
	    for (std::list<brep_hit>::const_iterator i = hits.begin();
		    i != hits.end(); ++i, ++hit_index) {
		if (!brep_hits_identical(fixed_hits[hit_index], *i))
		    trace->fixed_hit_mismatches++;
	    }
	} else {
	    trace->fixed_hit_mismatches++;
	}
    }

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
			if (prev_hit.m_adj_face_index == curr_hit.face->m_face_index) {
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
			(prev_hit.face->m_face_index == curr_hit.m_adj_face_index)) {
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

    if (trace)
	trace->after_near_miss = hits.size();

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

    if (trace)
	trace->after_near_hit = hits.size();

    if (!hits.empty()) {
	/* Remove isolated grazing contacts, but do not discard a resolved solid
	 * interval merely because an affine distortion makes both endpoint
	 * normals nearly perpendicular to the ray. */
	TRACE("-- Remove grazing hits --");
	//int num = 0;
	for (std::list<brep_hit>::iterator i = hits.begin(); i != hits.end(); ++i) {
	    const brep_hit &curr_hit = *i;
	    std::list<brep_hit>::const_iterator grazing_prev = i;
	    const bool have_prev = i != hits.begin();
	    if (have_prev)
		--grazing_prev;
	    std::list<brep_hit>::const_iterator grazing_next = i;
	    ++grazing_next;
	    const bool resolved_before = have_prev &&
		brep_resolved_grazing_pair(*grazing_prev, curr_hit, *rp, tol);
	    const bool resolved_after = grazing_next != hits.end() &&
		brep_resolved_grazing_pair(curr_hit, *grazing_next, *rp, tol);
	    const bool invalid =
		(curr_hit.trimmed && !curr_hit.closeToEdge) || curr_hit.oob;
	    const bool grazing = NEAR_ZERO(VDOT(curr_hit.normal, rp->r_dir),
		BREP_GRAZING_DOT_TOL);
	    if (invalid || (grazing && !resolved_before && !resolved_after)) {
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
	    //TRACE("hit " << num << ": " << PT(i->point) << " [" << VDOT(i->normal, rp->r_dir) << "]");
	    //++num;
	}
    }

    if (trace)
	trace->after_grazing = hits.size();

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

    if (trace)
	trace->after_duplicates = hits.size();

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

    if (trace) {
	trace->after_direction_cleanup = hits.size();
	trace->final_hits = hits.size();
	const brep_hit_cleanup_state fixed_cleanup =
	    cleanup_fixed_brep_hits(fixed_hits, *rp, tol);
	trace->fixed_after_near_miss = fixed_cleanup.after_near_miss;
	trace->fixed_after_near_hit = fixed_cleanup.after_near_hit;
	trace->fixed_after_grazing = fixed_cleanup.after_grazing;
	trace->fixed_after_duplicates = fixed_cleanup.after_duplicates;
	trace->fixed_after_direction_cleanup = fixed_cleanup.after_direction;
	if (trace->fixed_after_near_miss != trace->after_near_miss)
	    trace->fixed_cleanup_mismatches++;
	if (trace->fixed_after_near_hit != trace->after_near_hit)
	    trace->fixed_cleanup_mismatches++;
	if (trace->fixed_after_grazing != trace->after_grazing)
	    trace->fixed_cleanup_mismatches++;
	if (trace->fixed_after_duplicates != trace->after_duplicates)
	    trace->fixed_cleanup_mismatches++;
	if (trace->fixed_after_direction_cleanup !=
		trace->after_direction_cleanup) {
	    trace->fixed_cleanup_mismatches++;
	} else {
	    size_t hit_index = 0;
	    for (std::list<brep_hit>::const_iterator i = hits.begin();
		    i != hits.end(); ++i, ++hit_index) {
		if (!brep_hits_identical(fixed_hits[hit_index], *i))
		    trace->fixed_cleanup_mismatches++;
	    }
	}
    }
    const brep_hit *unmatched_hit = hits.size() == 1 ? &hits.front() : NULL;
    brep_classify_closure(trace, bs, unmatched_hit);
    brep_hit repaired_hit;
    brep_resolve_continuation(trace, bs, r, unmatched_hit, &repaired_hit);
    if (trace)
	brep_trace_prepared_event_cleanup(trace, bs, r, *rp, tol, hits,
	    trace->closure_shadow_segments == 1 ? &repaired_hit : NULL);
    if (trace && trace->closure_shadow_segments == 1) {
	hits.push_back(repaired_hit);
	hits.sort();
    }

    if (trace && prepared_selected) {
	trace->final_segments = prepared_hits.size() / 2;
	return emit_fixed_brep_hits(prepared_hits, stp, rp, ap, seghead, bs);
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
		segp->seg_in.hit_surfno = in.face->m_face_index;
		VSET(segp->seg_in.hit_vpriv, in.uv[0], in.uv[1], 0.0);
		VMOVE(segp->seg_in.hit_normal, in.normal);
		VJOIN1(segp->seg_in.hit_point, rp->r_pt, segp->seg_in.hit_dist, rp->r_dir);
		segp->seg_in.hit_rayp = &ap->a_ray;

		VMOVE(segp->seg_out.hit_point, out.point);
		VMOVE(segp->seg_out.hit_normal, out.normal);
		segp->seg_out.hit_dist = out.dist;

		/* set out hit */
		segp->seg_out.hit_dist = out.dist + (los*0.5); // centered
		segp->seg_out.hit_surfno = out.face->m_face_index;
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
	if (trace)
	    trace->final_segments = nhits;

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
		    segp->seg_in.hit_surfno = in.face->m_face_index;
		    VSET(segp->seg_in.hit_vpriv, in.uv[0], in.uv[1], 0.0);

		    VMOVE(segp->seg_out.hit_point, out.point);
		    VMOVE(segp->seg_out.hit_normal, out.normal);
		    segp->seg_out.hit_dist = out.dist;
		    segp->seg_out.hit_surfno = out.face->m_face_index;
		    VSET(segp->seg_out.hit_vpriv, out.uv[0], out.uv[1], 0.0);

		    BU_LIST_INSERT(&(seghead->l), &(segp->l));
		    if (trace)
			trace->final_segments++;
		}
		hit = true;
	    }
	}

	return (hit) ? (int)hits.size() : 0; // MISS
    }

    return 0;
}


int
rt_brep_shot(struct soltab *stp, struct xray *rp, struct application *ap,
    struct seg *seghead)
{
    return rt_brep_shot_impl(stp, rp, ap, seghead, NULL, true);
}


int
_rt_brep_shot_trace(struct soltab *stp, struct xray *rp,
    struct application *ap, struct seg *seghead,
    struct rt_brep_shot_trace *trace)
{
    if (!trace)
	return rt_brep_shot_impl(stp, rp, ap, seghead, NULL, true);
    *trace = {};
    trace->closure_edge_index = -1;
    trace->closure_missing_direction = -1;
    trace->continuation_face_index = -1;
    trace->continuation_span_index = -1;
    trace->continuation_adjacent_face_index = -99;
    return rt_brep_shot_impl(stp, rp, ap, seghead, trace, true);
}


int
_rt_brep_shot_legacy(struct soltab *stp, struct xray *rp,
    struct application *ap, struct seg *seghead)
{
    return rt_brep_shot_impl(stp, rp, ap, seghead, NULL, false);
}


/**
 * Baseline flat-array vshot: delegates to the scalar shot via rt_vshot_via_shot().
 */
void
rt_brep_vshot(struct soltab *stp[], struct xray *rp[], struct seg *segp, int n, struct application *ap)
{
    rt_vshot_via_shot(rt_brep_shot, stp, rp, segp, n, ap);
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
    if (!st || !st->Valid())
	return;
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
    struct bu_list *vlfree = &rt_vlfree;
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
    struct bu_list *vlfree = &rt_vlfree;
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

    ON__UINT64 Internal_CurrentPositionOverride() const;
    bool Internal_SeekFromCurrentPositionOverride(int);
    bool Internal_SeekToStartOverride();
    size_t Internal_ReadOverride( size_t, void* );
    size_t Internal_WriteOverride( size_t, const void* );

private:
    size_t pos;
    std::vector<char> m_buffer;
};


class RT_BrepSemaphoreGuard
{
public:
    explicit RT_BrepSemaphoreGuard(int semaphore) : m_semaphore(semaphore)
    {
	bu_semaphore_acquire(m_semaphore);
    }

    ~RT_BrepSemaphoreGuard()
    {
	bu_semaphore_release(m_semaphore);
    }

private:
    RT_BrepSemaphoreGuard(const RT_BrepSemaphoreGuard &);
    RT_BrepSemaphoreGuard &operator=(const RT_BrepSemaphoreGuard &);

    int m_semaphore;
};


/* openNURBS constructs, reads, writes, and resets render-setting defaults
 * through non-reentrant localtime().  Keep the guard alive until after the
 * model member is destroyed (members are destroyed in reverse order). */
class RT_SerializedONXModel
{
private:
    RT_BrepSemaphoreGuard m_guard;

public:
    RT_SerializedONXModel() : m_guard(BU_SEM_ID_DATETIME), model()
    {
    }

    ONX_Model model;

private:
    RT_SerializedONXModel(const RT_SerializedONXModel &);
    RT_SerializedONXModel &operator=(const RT_SerializedONXModel &);
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

ON__UINT64
RT_MemoryArchive::Internal_CurrentPositionOverride() const
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
RT_MemoryArchive::Internal_SeekFromCurrentPositionOverride(int seek_to)
{
    return SeekFromCurrentPosition(seek_to);
}

bool
RT_MemoryArchive::Internal_SeekToStartOverride()
{
    pos = 0;
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

size_t
RT_MemoryArchive::Internal_ReadOverride(size_t amount, void* buf)
{
    return Read(amount, buf);
}

size_t
RT_MemoryArchive::Internal_WriteOverride(size_t amount, const void* buf)
{
    return Write(amount, buf);
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

#define ON_opennurbs4_id { 0x17b3ecda, 0x17ba, 0x4e45,{ 0x9e, 0x67, 0xa2, 0xb8, 0xd9, 0xbe, 0x52, 0xd } }
#define ON_brlcad_default_layer_id { 0xc4b29a7d, 0x766e, 0x478f,{ 0xa4, 0xe2, 0x5b, 0x61, 0xd4, 0xaf, 0x23, 0x91 } }

static void
brep_dbi2on(const struct rt_db_internal *intern, ONX_Model& model)
{
    struct rt_brep_internal *bi = (struct rt_brep_internal *)intern->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

#if 0
    ON_Layer default_layer;
    default_layer.Default();
    default_layer.SetLayerIndex(0);
    default_layer.SetLayerName("Default");
    model.m_layer_table.Reserve(1);
    model.m_layer_table.Append(default_layer);
#endif
    /* ONX_Model::AddDefaultLayer assigns a random component UUID.  A BREP
     * primitive serializes a self-contained model, so that otherwise makes
     * identical BRL-CAD databases differ on every write. */
    const ON_UUID default_layer_id = ON_brlcad_default_layer_id;
    ON_Layer default_layer;
    default_layer.SetId(default_layer_id);
    default_layer.SetIndex(0);
    default_layer.SetName(L"Default");
    default_layer.SetColor(ON_Color::UnsetColor);
    model.AddModelComponent(default_layer, true);
    /* Keep both forms coherent: SetCurrentLayerId() clears the legacy index,
     * while SetV5CurrentLayerIndex() preserves the UUID.  BREP primitives are
     * currently serialized as version 4 archives, which use the index. */
    model.m_settings.SetCurrentLayerId(default_layer_id);
    model.m_settings.SetV5CurrentLayerIndex(0);

#if 0
    ON_DimStyle default_style;
    default_style.SetDefaults();
    model.m_dimstyle_table.Reserve(1);
    model.m_dimstyle_table.Append(default_style);

    model.m_object_table.SetCapacity(1);
#endif

    ON_3dmObjectAttributes* attributes = new ON_3dmObjectAttributes();
    attributes->m_uuid = ON_opennurbs4_id;
    attributes->m_name = "brep";
    ON_ModelGeometryComponent *gc = ON_ModelGeometryComponent::CreateForExperts(false, ON_Geometry::Cast(bi->brep), true, attributes, NULL);
    ON_ModelGeometryComponent ngc(*gc);
    delete gc;
    model.AddModelComponent(ngc);

    /* ONX_Model::Write creates a current-time revision when the count is
     * zero.  BREP primitives do not need an edit history; use a fixed valid
     * epoch so serialization is reproducible. */
    ON_3dmRevisionHistory &revision = model.m_properties.m_RevisionHistory;
    revision = ON_3dmRevisionHistory::Empty;
    revision.m_revision_count = 1;
    revision.m_create_time.tm_year = 100;
    revision.m_create_time.tm_mon = 0;
    revision.m_create_time.tm_mday = 1;
    revision.m_last_edit_time = revision.m_create_time;
    model.m_properties.m_Application.m_application_name = "BRL-CAD B-Rep primitive";
    //model.Polish();
}


int
rt_brep_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    if (attr == (char *)NULL) {
	bu_vls_sprintf(logstr, "brep");

	RT_SerializedONXModel serialized_model;
	ONX_Model &model = serialized_model.model;
	brep_dbi2on(intern, model);

	/* Create a serialized version for base-64 encoding */
	RT_MemoryArchive archive;
	ON_TextLog err(stderr);
	bool ok = model.Write(archive, 4, &err);
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
    RT_SerializedONXModel serialized_model;
    ONX_Model &model = serialized_model.model;
    if (argc == 1 && argv[0]) {
	int decoded_size = bu_b64_decode(&decoded, (const signed char *)argv[0]);
	RT_MemoryArchive archive(decoded, decoded_size);
	ON_wString wonstr;
	ON_TextLog log(wonstr);

	RT_BREP_CK_MAGIC(bi);
	model.Read(archive, &log);
	bu_vls_printf(logstr, "%s", ON_String(wonstr).Array());

	ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::ModelGeometry);
	ON_ModelComponentReference cr = it.FirstComponentReference();
	const ON_ModelGeometryComponent *mo = ON_ModelGeometryComponent::Cast(cr.ModelComponent());
	bi->brep = ON_Brep::New(*ON_Brep::Cast(mo->ExclusiveGeometry()));
    }
    return BRLCAD_OK;
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

    RT_SerializedONXModel serialized_model;
    ONX_Model &model = serialized_model.model;
    brep_dbi2on(ip, model);

    RT_MemoryArchive archive;
    ON_TextLog err(stderr);
    bool ok = model.Write(archive, 4, &err);
    if (ok) {
	ep->ext_nbytes = (long)archive.Size();
	ep->ext_buf = archive.CreateCopy();
	return 0;
    } else {
	return -1;
    }
}

int
rt_brep_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !mat)
	return BRLCAD_OK;

    // For the moment, we only support applying a mat to a brep in place - the
    // input and output must be the same.
    if (ip && rop != ip) {
	bu_log("rt_brep_mat:  alignment of points between multiple breps is unsupported - input brep must be the same as the output brep.\n");
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *bi = (struct rt_brep_internal *)rop->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    ON_Xform xform(mat);
    if (!xform.IsIdentity()) {
	bi->brep->Transform(xform);
	if (bn_mat_det3(mat) < 0.0)
	    bi->brep->Flip();
    }

    return BRLCAD_OK;
}

int
rt_brep_mirror(struct rt_db_internal *ip, const plane_t plane)
{
    mat_t mirmat;
    mat_t rmat;
    mat_t temp;
    vect_t nvec;
    vect_t xvec;
    vect_t mirror_dir;
    point_t mirror_pt;
    fastf_t ang;

    static point_t origin = {0.0, 0.0, 0.0};

    RT_CK_DB_INTERNAL(ip);

    MAT_IDN(mirmat);

    VMOVE(mirror_dir, plane);
    VSCALE(mirror_pt, plane, plane[W]);

    mirmat[0] = -1.0;

    VSET(xvec, 1, 0, 0);
    VCROSS(nvec, xvec, mirror_dir);
    VUNITIZE(nvec);
    ang = -acos(VDOT(xvec, mirror_dir));
    bn_mat_arb_rot(rmat, origin, nvec, ang*2.0);

    MAT_COPY(temp, mirmat);
    bn_mat_mul(mirmat, temp, rmat);

    mirmat[3 + X*4] += mirror_pt[X] * mirror_dir[X];
    mirmat[3 + Y*4] += mirror_pt[Y] * mirror_dir[Y];
    mirmat[3 + Z*4] += mirror_pt[Z] * mirror_dir[Z];

    return rt_brep_mat(ip, mirmat, NULL);
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
    RT_SerializedONXModel serialized_model;
    ONX_Model &model = serialized_model.model;
    ON_TextLog err(stderr);
    unsigned int obj_filter = ON::brep_object;
    model.Read(archive, 0, obj_filter, &err);

    /* grab the first geometry item from the manifest */
    const ON_ComponentManifestItem* geom = model.Manifest().FirstItem(ON_ModelComponent::Type::ModelGeometry);
    /* sanity check */
    if (model.Manifest().NextItem(geom) != nullptr)
	bu_log("WARNING: geometry may be getting lost\n");

    /* do the necessary API calls to get a usable geometry component from the manifest item */
    ON_ModelComponentReference geom_ref = model.ModelGeometryFromId(geom->Id());
    const ON_ModelGeometryComponent* geom_comp = ON_ModelGeometryComponent::Cast(geom_ref.ModelComponent());

    bi->brep = ON_Brep::New(*ON_Brep::Cast(geom_comp->Geometry(NULL)));

    /* Apply transform */
    return rt_brep_mat(ip, mat, NULL);
}


void
rt_brep_ifree(struct rt_db_internal *ip)
{
    struct rt_brep_internal* bi;
    RT_CK_DB_INTERNAL(ip);

    TRACE1("rt_brep_ifree");

    bi = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    delete bi->brep;
    bi->brep = NULL;
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

    /* NOTE: this value is not arbitrary, but is dependent on the init in libged/list/list.c */
    if (verbose <= 99) {
	bu_vls_strcat(str, "    use -v (verbose) for all data\n");
	return 0;
    }

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
rt_brep_make(const struct rt_functab *ftp, struct rt_db_internal *intern, const char *UNUSED(variant), const point_t UNUSED(origin), double UNUSED(scale))
{
    struct rt_brep_internal* ip;

    intern->idb_type = ID_BREP;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;

    BU_ASSERT(&OBJ[intern->idb_type] == ftp);
    intern->idb_meth = ftp;

    BU_ALLOC(ip, struct rt_brep_internal);
    intern->idb_ptr = (void *)ip;

    ip->magic = RT_BREP_INTERNAL_MAGIC;
    ip->brep = (ON_Brep *)brep_create();

    return BRLCAD_OK;
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

	// TODO: should only consider vertexes in untrimmed regions
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

    // narrow down the list to just the control vertices closest to
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
	const struct bn_tol *prepared_tol = stp->st_rtip ?
	    &stp->st_rtip->rti_tol : NULL;
	brep_build_edge_data(specific, prepared_tol);
	brep_build_surface_data(specific);
	brep_certify_edge_correspondences(specific);
	brep_bound_edge_discrepancies(specific);
	brep_build_vertex_data(specific);

	Deserializer deserializer(*external);
	const uint32_t num_children = deserializer.read_uint32();

	for (uint32_t i = 0; i < num_children; ++i) {
	    const CurveTree * const ctree = new CurveTree(deserializer, *specific->brep->m_F.At(i));
	    specific->ctrees.push_back(ctree);
	    specific->bvh->addChild(new BBNode(deserializer, *ctree));
	}

	specific->bvh->BuildBBox();
	/* Version 0 did not serialize the construction depth.  Preserve that
	 * fact while still reporting the hierarchy actually loaded. */
	specific->surface_tree_depth_limit = -1;
	brep_collect_bvh_stats(specific);

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

int rt_brep_plot_poly(struct bu_list *vhead, const struct directory *dp, struct rt_db_internal *ip,
		      const struct bg_tess_tol *ttol, const struct bn_tol *tol,
		      const struct bview *UNUSED(info))
{
    TRACE1("rt_brep_plot");

    if (!vhead || dp == RT_DIR_NULL || !ip || !ttol || !tol)
	return -1;

    struct bu_list *vlfree = &rt_vlfree;
    struct rt_brep_internal* bi;
    const char *solid_name =  dp->d_namep;
    ON_wString wstr;
    ON_TextLog tl(wstr);

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    bi = (struct rt_brep_internal*) ip->idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    ON_Brep* brep = bi->brep;

    if (brep_facecdt_plot(NULL, solid_name, ttol, tol, brep, vhead, NULL, vlfree, -1, 0, -1)) {
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
