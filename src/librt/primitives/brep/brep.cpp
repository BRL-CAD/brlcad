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
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

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
static const double BREP_DIRECT_ROOT_RELATIVE_TOLERANCE = 5.0e-9;
static const double BREP_DIRECT_EVALUATION_ULPS = 32.0;


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
	/* Preserve both the surface locus and its parameterization.  A
	 * reparameterized form cannot safely share the face's trim domains. */
	if (surface->GetNurbForm(nurbs) != 1 || nurbs.m_order[0] < 2 ||
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
	    const ON_BrepFace *face = trim.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    const ON_Interval trim_domain = trim.Domain();
	    if (!surface || !trim_domain.IsIncreasing())
		return false;
	    double trim_fraction = trim.m_bRev3d ? 1.0 - edge_fraction :
		edge_fraction;
	    const ON_3dPoint uv = trim.PointAt(
		trim_domain.ParameterAt(trim_fraction));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    if (!uv.IsValid() || !lift.IsValid())
		return false;
	    maximum = std::max(maximum, edge_point.DistanceTo(lift));
	}
    }
    return std::isfinite(maximum);
}


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
	if (record.tolerance_inferred && record.discrepancy_measured)
	    record.tolerance = std::max(record.tolerance,
		record.measured_discrepancy);
	const double coordinate_scale = std::max(1.0,
	    std::max(fabs(edge.PointAtStart().x),
	    std::max(fabs(edge.PointAtStart().y),
	    fabs(edge.PointAtStart().z))));
	const double roundoff = std::max(ON_ZERO_TOLERANCE,
	    128.0 * DBL_EPSILON * coordinate_scale);
	record.discrepancy_authorized = record.discrepancy_measured &&
	    record.measured_discrepancy <= record.tolerance + roundoff;
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

    brep_build_edge_data(bs, tol);
    brep_build_surface_data(bs);

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


struct brep_surface_coefficients {
    double value[2][BREP_DIRECT_BEZIER_MAX_CVS];
    double ray_numerator[BREP_DIRECT_BEZIER_MAX_CVS];
    double weight[BREP_DIRECT_BEZIER_MAX_CVS];
    double error[2] = {0.0, 0.0};
    double ray_numerator_error = 0.0;
    double weight_error = 0.0;
    int order[2] = {0, 0};
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

    double magnitude[2] = {0.0, 0.0};
    double arithmetic_scale[2] = {0.0, 0.0};
    double ray_magnitude = 0.0;
    double ray_arithmetic_scale = 0.0;
    double maximum_weight = 0.0;
    const ON_3dVector planes[2] = {first, second};
    const double direction_squared = ray.m_dir * ray.m_dir;
    if (!(direction_squared > DBL_MIN) || !std::isfinite(direction_squared))
	return false;
    for (int i = 0; i < coefficients.order[0]; ++i) {
	for (int j = 0; j < coefficients.order[1]; ++j) {
	    ON_4dPoint cv;
	    if (!span.surface.GetCV(i, j, cv) || !cv.IsValid() ||
		    !(cv.w > 0.0) || !std::isfinite(cv.w))
		return false;
	    const ON_3dVector numerator(cv.x - ray.m_origin.x * cv.w,
		cv.y - ray.m_origin.y * cv.w,
		cv.z - ray.m_origin.z * cv.w);
	    const size_t index = (size_t)i * coefficients.order[1] + j;
	    coefficients.ray_numerator[index] =
		(numerator * ray.m_dir) / direction_squared;
	    coefficients.weight[index] = cv.w;
	    if (!std::isfinite(coefficients.ray_numerator[index]))
		return false;
	    ray_magnitude = std::max(ray_magnitude,
		fabs(coefficients.ray_numerator[index]));
	    ray_arithmetic_scale = std::max(ray_arithmetic_scale,
		((fabs(cv.x) + fabs(ray.m_origin.x * cv.w)) *
		fabs(ray.m_dir.x) +
		(fabs(cv.y) + fabs(ray.m_origin.y * cv.w)) *
		fabs(ray.m_dir.y) +
		(fabs(cv.z) + fabs(ray.m_origin.z * cv.w)) *
		fabs(ray.m_dir.z)) / direction_squared);
	    maximum_weight = std::max(maximum_weight, cv.w);
	    for (int equation = 0; equation < 2; ++equation) {
		const double value = numerator * planes[equation];
		if (!std::isfinite(value))
		    return false;
		coefficients.value[equation][index] = value;
		magnitude[equation] = std::max(magnitude[equation],
		    fabs(value));
		const double scale =
		    (fabs(cv.x) + fabs(ray.m_origin.x * cv.w)) *
		    fabs(planes[equation].x) +
		    (fabs(cv.y) + fabs(ray.m_origin.y * cv.w)) *
		    fabs(planes[equation].y) +
		    (fabs(cv.z) + fabs(ray.m_origin.z * cv.w)) *
		    fabs(planes[equation].z);
		arithmetic_scale[equation] = std::max(
		    arithmetic_scale[equation], scale);
	    }
	}
    }
    for (int equation = 0; equation < 2; ++equation)
	coefficients.error[equation] = 128.0 * DBL_EPSILON *
	    std::max(1.0, std::max(magnitude[equation],
	    arithmetic_scale[equation]));
    coefficients.ray_numerator_error = 128.0 * DBL_EPSILON *
	std::max(1.0, std::max(ray_magnitude, ray_arithmetic_scale));
    coefficients.weight_error = 128.0 * DBL_EPSILON *
	std::max(1.0, maximum_weight);
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


static double
brep_scalar_bezier_reparameterization_norm(int order, double minimum,
    double maximum)
{
    double row_sum[BREP_DIRECT_BEZIER_MAX_ORDER] = {0.0};
    for (int basis = 0; basis < order; ++basis) {
	double input[BREP_DIRECT_BEZIER_MAX_ORDER] = {0.0};
	double output[BREP_DIRECT_BEZIER_MAX_ORDER] = {0.0};
	input[basis] = 1.0;
	if (!brep_scalar_bezier_reparameterize(input, order, minimum, maximum,
		output))
	    return INFINITY;
	for (int row = 0; row < order; ++row)
	    row_sum[row] += fabs(output[row]);
    }
    double norm = 0.0;
    for (int row = 0; row < order; ++row)
	norm = std::max(norm, row_sum[row]);
    return norm;
}


static bool
brep_surface_coefficients_reparameterize(
    const brep_surface_coefficients &source, const double minimum[2],
    const double maximum[2], brep_surface_coefficients &result)
{
    result.order[0] = source.order[0];
    result.order[1] = source.order[1];
    const double u_norm = brep_scalar_bezier_reparameterization_norm(
	source.order[0], minimum[0], maximum[0]);
    const double v_norm = brep_scalar_bezier_reparameterization_norm(
	source.order[1], minimum[1], maximum[1]);
    const double amplification = u_norm * v_norm;
    if (!std::isfinite(amplification) || !(amplification > 0.0))
	return false;

    for (int equation = 0; equation < 2; ++equation) {
	if (!brep_scalar_surface_reparameterize(source.value[equation],
		source.order[0], source.order[1], minimum[0], maximum[0],
		minimum[1], maximum[1], result.value[equation]))
	    return false;
    }
    if (!brep_scalar_surface_reparameterize(source.ray_numerator,
	    source.order[0], source.order[1], minimum[0], maximum[0],
	    minimum[1], maximum[1], result.ray_numerator) ||
	    !brep_scalar_surface_reparameterize(source.weight,
	    source.order[0], source.order[1], minimum[0], maximum[0],
	    minimum[1], maximum[1], result.weight))
	return false;

    const size_t count = (size_t)source.order[0] * source.order[1];
    for (int equation = 0; equation < 2; ++equation) {
	double magnitude = 0.0;
	for (size_t i = 0; i < count; ++i)
	    magnitude = std::max(magnitude, fabs(result.value[equation][i]));
	result.error[equation] = source.error[equation] * amplification +
	    128.0 * DBL_EPSILON * std::max(1.0, magnitude);
    }
    double ray_magnitude = 0.0;
    double weight_magnitude = 0.0;
    double minimum_weight = DBL_MAX;
    for (size_t i = 0; i < count; ++i) {
	ray_magnitude = std::max(ray_magnitude,
	    fabs(result.ray_numerator[i]));
	weight_magnitude = std::max(weight_magnitude, fabs(result.weight[i]));
	minimum_weight = std::min(minimum_weight, result.weight[i]);
    }
    result.ray_numerator_error =
	source.ray_numerator_error * amplification +
	128.0 * DBL_EPSILON * std::max(1.0, ray_magnitude);
    result.weight_error = source.weight_error * amplification +
	128.0 * DBL_EPSILON * std::max(1.0, weight_magnitude);
    return minimum_weight > result.weight_error;
}


struct brep_subdivision_box {
    double minimum[2];
    double maximum[2];
    int depth;
};


static bool
brep_surface_box_t_range(const brep_surface_coefficients &coefficients,
    const brep_subdivision_box &box, double &minimum_t, double &maximum_t)
{
    double numerator[BREP_DIRECT_BEZIER_MAX_CVS];
    double weight[BREP_DIRECT_BEZIER_MAX_CVS];
    if (!brep_scalar_surface_restrict(coefficients.ray_numerator,
	    coefficients.order[0], coefficients.order[1], box.minimum[0],
	    box.maximum[0], box.minimum[1], box.maximum[1], numerator) ||
	    !brep_scalar_surface_restrict(coefficients.weight,
	    coefficients.order[0], coefficients.order[1], box.minimum[0],
	    box.maximum[0], box.minimum[1], box.maximum[1], weight))
	return false;
    const size_t count = (size_t)coefficients.order[0] *
	coefficients.order[1];
    minimum_t = DBL_MAX;
    maximum_t = -DBL_MAX;
    double minimum_weight = DBL_MAX;
    for (size_t i = 0; i < count; ++i) {
	if (!(weight[i] > 0.0) || !std::isfinite(weight[i]) ||
		!std::isfinite(numerator[i]))
	    return false;
	const double value = numerator[i] / weight[i];
	if (!std::isfinite(value))
	    return false;
	minimum_t = std::min(minimum_t, value);
	maximum_t = std::max(maximum_t, value);
	minimum_weight = std::min(minimum_weight, weight[i]);
    }
    const double depth_factor = 1.0 + 8.0 * box.depth;
    const double numerator_error =
	coefficients.ray_numerator_error * depth_factor;
    const double weight_error = coefficients.weight_error * depth_factor;
    if (!(minimum_weight > weight_error))
	return false;
    const double magnitude = std::max(fabs(minimum_t), fabs(maximum_t));
    const double margin = (numerator_error + magnitude * weight_error) /
	(minimum_weight - weight_error) +
	128.0 * DBL_EPSILON * std::max(1.0, magnitude);
    minimum_t -= margin;
    maximum_t += margin;
    return std::isfinite(minimum_t) && std::isfinite(maximum_t) &&
	minimum_t <= maximum_t;
}


static void
brep_trace_surface_isolation(struct rt_brep_shot_trace *trace,
    const brep_surface_coefficients &coefficients,
    const brep_surface_span &span)
{
    brep_subdivision_box pending[BREP_DIRECT_SUBDIVISION_CAPACITY];
    size_t pending_count = 1;
    pending[0].minimum[0] = 0.0;
    pending[0].minimum[1] = 0.0;
    pending[0].maximum[0] = 1.0;
    pending[0].maximum[1] = 1.0;
    pending[0].depth = 0;
    trace->surface_workspace_high_water = std::max(
	trace->surface_workspace_high_water, pending_count);

    double restricted[2][BREP_DIRECT_BEZIER_MAX_CVS];
    const size_t count = (size_t)coefficients.order[0] *
	coefficients.order[1];
    while (pending_count) {
	const brep_subdivision_box box = pending[--pending_count];
	trace->surface_subdivision_boxes++;
	trace->surface_subdivision_max_depth = std::max(
	    trace->surface_subdivision_max_depth, (size_t)box.depth);
	bool excluded = false;
	for (int equation = 0; equation < 2; ++equation) {
	    if (!brep_scalar_surface_restrict(coefficients.value[equation],
		    coefficients.order[0], coefficients.order[1],
		    box.minimum[0], box.maximum[0], box.minimum[1],
		    box.maximum[1], restricted[equation])) {
		trace->surface_workspace_exhausted++;
		return;
	    }
	    double magnitude = 0.0;
	    for (size_t i = 0; i < count; ++i)
		magnitude = std::max(magnitude,
		    fabs(restricted[equation][i]));
	    const double error = coefficients.error[equation] *
		(1.0 + 8.0 * box.depth) + 128.0 * DBL_EPSILON *
		std::max(1.0, magnitude);
	    if (brep_coefficient_hull_excluded(restricted[equation], count,
		    error)) {
		excluded = true;
		break;
	    }
	}
	if (excluded)
	    continue;
	if (box.depth >= BREP_DIRECT_SUBDIVISION_MAX_DEPTH) {
	    double minimum_t = 0.0;
	    double maximum_t = 0.0;
	    if (!brep_surface_box_t_range(coefficients, box, minimum_t,
		    maximum_t)) {
		trace->surface_workspace_exhausted++;
		return;
	    }
	    trace->surface_isolated_boxes++;
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
	    }
	    continue;
	}

	const int direction =
	    box.maximum[0] - box.minimum[0] >=
	    box.maximum[1] - box.minimum[1] ? 0 : 1;
	const double midpoint = 0.5 *
	    (box.minimum[direction] + box.maximum[direction]);
	if (pending_count + 2 > BREP_DIRECT_SUBDIVISION_CAPACITY) {
	    trace->surface_workspace_exhausted++;
	    return;
	}
	brep_subdivision_box &second = pending[pending_count++];
	second = box;
	second.minimum[direction] = midpoint;
	second.depth++;
	brep_subdivision_box &first = pending[pending_count++];
	first = box;
	first.maximum[direction] = midpoint;
	first.depth++;
	trace->surface_workspace_high_water = std::max(
	    trace->surface_workspace_high_water, pending_count);
    }
}


static bool
brep_trace_continuation_certificate(struct rt_brep_shot_trace *trace,
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
    brep_trace_surface_isolation(&certificate, extension, extension_span);
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
    const double distance_tolerance = 1.0e-7;
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
    const struct brep_specific *bs, const ON_Ray &ray)
{
    if (!trace || !bs)
	return;
    trace->prepared_surface_spans = bs->surface_spans.size();
    ON_3dVector first;
    ON_3dVector second;
    const bool valid_frame = brep_ray_plane_frame(ray, first, second);
    for (std::vector<brep_face_record>::const_iterator record_it =
	    bs->face_records.begin(); record_it != bs->face_records.end();
	    ++record_it) {
	const brep_face_record &record = *record_it;
	if (!record.supported || !valid_frame) {
	    trace->unsupported_surface_faces++;
	    continue;
	}
	trace->supported_surface_faces++;
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
			bs->surface_spans[span_index]);
		else
		    trace->surface_workspace_exhausted++;
	    }
	}
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
    const ON_Interval edge_domain = edge.Domain();
    const ON_Interval trim_domain = trim.Domain();
    if (!face || !surface || !edge_domain.IsIncreasing() ||
	    !trim_domain.IsIncreasing() || trim.m_ei != edge.m_edge_index)
	return false;

    double fraction = edge_domain.NormalizedParameterAt(edge_parameter);
    if (!std::isfinite(fraction) || fraction < -ON_ZERO_TOLERANCE ||
	    fraction > 1.0 + ON_ZERO_TOLERANCE)
	return false;
    fraction = std::max(0.0, std::min(1.0, fraction));
    if (trim.m_bRev3d)
	fraction = 1.0 - fraction;

    const double trim_parameter = trim_domain.ParameterAt(fraction);
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
brep_trace_edge_sector(struct rt_brep_trace_edge &observation,
    const struct brep_specific *bs, const brep_edge_record &record,
    const ON_Ray &ray)
{
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


static void
brep_trace_edges(struct rt_brep_shot_trace *trace,
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
		record.tolerance >= 0.0 && record.discrepancy_authorized;
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
	observation.edge_index = record.edge_index;
	observation.face_index[0] = record.face_index[0];
	observation.face_index[1] = record.face_index[1];
	observation.candidate_spans = candidate_spans;
	observation.discrepancy_measured = record.discrepancy_measured;
	observation.discrepancy_authorized = record.discrepancy_authorized;
	observation.tolerance_inferred = record.tolerance_inferred;
	const double roundoff = std::max(ON_ZERO_TOLERANCE,
	    128.0 * DBL_EPSILON * std::max(1.0, distance));
	observation.within_edge_tolerance =
	    ON_IsValid(record.tolerance) && record.tolerance >= 0.0 &&
	    record.discrepancy_authorized &&
	    distance <= record.tolerance + roundoff;
	if (observation.within_edge_tolerance) {
	    trace->edges_within_tolerance++;
	    brep_trace_edge_sector(observation, bs, record, ray);
	}
    }
}


static void
brep_trace_closure(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const std::list<brep_hit> &hits)
{
    if (!trace || !bs || !bs->is_solid || bs->plate_mode ||
	    hits.size() != 1)
	return;
    const brep_hit &hit = hits.front();
    for (size_t edge_index = 0; edge_index < trace->stored_edges;
	    ++edge_index) {
	const struct rt_brep_trace_edge &edge = trace->edges[edge_index];
	if (!edge.within_edge_tolerance || !edge.sector_valid ||
		edge.closest_state != 1 ||
		(hit.face.m_face_index != edge.face_index[0] &&
		 hit.face.m_face_index != edge.face_index[1]))
	    continue;
	const bool ordered = hit.direction == brep_hit::ENTERING ?
	    edge.ray_dist > hit.dist + BREP_SAME_POINT_TOLERANCE :
	    edge.ray_dist < hit.dist - BREP_SAME_POINT_TOLERANCE;
	if (!ordered)
	    continue;
	trace->closure_candidates++;
	if (trace->closure_edge_index >= 0)
	    continue;
	trace->closure_edge_dist = edge.ray_dist;
	trace->closure_existing_dist = hit.dist;
	trace->closure_edge_index = edge.edge_index;
	trace->closure_missing_direction = hit.direction == brep_hit::ENTERING ?
	    brep_hit::LEAVING : brep_hit::ENTERING;
    }
}


struct brep_continuation_result {
    ON_2dPoint uv;
    ON_3dPoint point;
    ON_3dVector normal;
    double dist = 0.0;
    double residual = DBL_MAX;
    size_t iterations = 0;
    bool converged = false;
};


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
    for (size_t iteration = 0; iteration < 24; ++iteration) {
	ON_3dVector derivative_u;
	ON_3dVector derivative_v;
	if (!brep_bezier_surface_derivatives(span.surface, result.uv,
		result.point, derivative_u, derivative_v))
	    return result;
	const ON_3dVector offset = result.point - ray.m_origin;
	const double f = offset * first;
	const double g = offset * second;
	result.residual = hypot(f, g);
	result.iterations = iteration + 1;
	result.normal = ON_CrossProduct(derivative_u, derivative_v);
	const double ray_length = ray.m_dir.Length();
	if (!std::isfinite(result.residual) || !result.normal.Unitize() ||
		!(ray_length > DBL_MIN) || !std::isfinite(ray_length))
	    return result;
	const double normal_dot = fabs(result.normal * ray.m_dir) / ray_length;
	const double coordinate_scale = std::max(span_scale,
	    std::max(fabs(ray.m_origin.x),
	    std::max(fabs(ray.m_origin.y),
	    std::max(fabs(ray.m_origin.z),
	    std::max(fabs(result.point.x),
	    std::max(fabs(result.point.y), fabs(result.point.z)))))));
	const double evaluation_floor = BREP_DIRECT_EVALUATION_ULPS *
	    DBL_EPSILON * coordinate_scale;
	if (result.residual <= std::max(evaluation_floor,
		root_tolerance * normal_dot)) {
	    result.dist = utah_calc_t(ray, result.point);
	    result.converged = std::isfinite(result.dist);
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
		!std::isfinite(v_scale) || u_scale <= ON_ZERO_TOLERANCE ||
		v_scale <= ON_ZERO_TOLERANCE ||
		fabs(determinant) <=
		BREP_INTERSECTION_ROOT_EPSILON * u_scale * v_scale)
	    return result;
	const ON_2dVector step((j22 * f - j12 * g) / determinant,
	    (j11 * g - j21 * f) / determinant);
	if (!step.IsValid())
	    return result;

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
	if (!improved)
	    return result;
    }
    return result;
}


static void
brep_trace_continuation(struct rt_brep_shot_trace *trace,
    const struct brep_specific *bs, const ON_Ray &ray,
    const std::list<brep_hit> &hits)
{
    if (!trace || !bs || !bs->brep || trace->closure_candidates != 1 ||
	    trace->closure_edge_index < 0 || hits.size() != 1)
	return;
    const brep_hit &hit = hits.front();
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
		brep.m_T[trim_index].FaceIndexOf() == hit.face.m_face_index) {
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

    const ON_Interval edge_domain = edge.Domain();
    const ON_Interval trim_domain = trim->Domain();
    if (!edge_domain.IsIncreasing() || !trim_domain.IsIncreasing())
	return;
    double fraction = edge_domain.NormalizedParameterAt(
	observation->edge_parameter);
    if (!std::isfinite(fraction))
	return;
    fraction = std::max(0.0, std::min(1.0, fraction));
    if (trim->m_bRev3d)
	fraction = 1.0 - fraction;
    ON_3dPoint edge_uv3;
    ON_3dVector trim_derivative3;
    if (!trim->Ev1Der(trim_domain.ParameterAt(fraction), edge_uv3,
	    trim_derivative3) || !edge_uv3.IsValid() ||
	    !trim_derivative3.IsValid())
	return;
    const ON_2dPoint edge_uv(edge_uv3.x, edge_uv3.y);
    const ON_2dPoint hit_uv(hit.uv[0], hit.uv[1]);

    for (std::vector<brep_face_record>::const_iterator face_it =
	    bs->face_records.begin(); face_it != bs->face_records.end();
	    ++face_it) {
	const brep_face_record &face_record = *face_it;
	if (!face_record.supported ||
		face_record.face_index != hit.face.m_face_index)
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
	    if (hit.face.m_bRev)
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
	    if (brep_trace_continuation_certificate(trace, span, ray,
		    certificate_minimum, certificate_maximum, root_uv,
		    result.dist, hit.dist))
		trace->continuation_certified_candidates++;
	    if (trace->continuation_face_index >= 0)
		continue;
	    trace->continuation_iterations = result.iterations;
	    trace->continuation_dist = result.dist;
	    trace->continuation_uv[0] = root_uv.x;
	    trace->continuation_uv[1] = root_uv.y;
	    trace->continuation_residual = result.residual;
	    trace->continuation_normal_dot = normal_dot;
	    trace->continuation_face_index = hit.face.m_face_index;
	}
    }
    if (trace->continuation_candidates == 1 &&
	    trace->continuation_certified_candidates == 1 &&
	    trace->continuation_face_index >= 0) {
	if (hit.direction == brep_hit::ENTERING &&
		trace->closure_missing_direction == brep_hit::LEAVING &&
		trace->continuation_dist > hit.dist) {
	    trace->closure_shadow_segments = 1;
	    trace->closure_shadow_in_dist = hit.dist;
	    trace->closure_shadow_out_dist = trace->continuation_dist;
	} else if (trace->closure_missing_direction == brep_hit::ENTERING &&
		hit.direction == brep_hit::LEAVING &&
		trace->continuation_dist < hit.dist) {
	    trace->closure_shadow_segments = 1;
	    trace->closure_shadow_in_dist = trace->continuation_dist;
	    trace->closure_shadow_out_dist = hit.dist;
	}
    }
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
	    root.uv[0] = span.surface_domain[0].ParameterAt(result.uv.x);
	    root.uv[1] = span.surface_domain[1].ParameterAt(result.uv.y);
	    root.residual = result.residual;
	    ON_3dVector normal = result.normal;
	    if (box.face_index >= 0 && box.face_index < bs->brep->m_F.Count() &&
		    bs->brep->m_F[box.face_index].m_bRev)
		normal.Reverse();
	    root.normal_dot = normal * ray.m_dir;
	    root.iterations = result.iterations;
	    root.face_index = box.face_index;
	    root.span_index = box.span_index;
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


static int
utah_brep_intersect(const BBNode* sbv, const ON_BrepFace* face,
    const ON_Surface* surf, pt2d_t& uv, const ON_Ray& ray,
    std::list<brep_hit>& hits, struct rt_brep_shot_trace *trace)
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
		brep_trace_root(trace, face, t[i], ouv[i], N[i], ray,
		    trim_status, closesttrim, trimBR, bh.hit);
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
		brep_trace_root(trace, face, t[i], ouv[i], N[i], ray,
		    trim_status, closesttrim, trimBR, bh.hit);
		if (VDOT(ray.m_dir, vnorm) < 0.0)
		    bh.direction = brep_hit::ENTERING;
		else
		    bh.direction = brep_hit::LEAVING;
		bh.sbv = sbv;
		hits.push_back(bh);
		found = BREP_INTERSECT_FOUND;
	    } else {
		brep_trace_root(trace, face, t[i], ouv[i], N[i], ray,
		    trim_status, closesttrim, trimBR, brep_hit::CLEAN_MISS);
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
static int
rt_brep_shot_impl(struct soltab *stp, struct xray *rp,
    struct application *ap, struct seg *seghead,
    struct rt_brep_shot_trace *trace)
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
    brep_trace_surface_spans(trace, bs, r);
    brep_trace_isolated_roots(trace, bs, r);
    brep_trace_local_clusters(trace,
	stp->st_rtip ? &stp->st_rtip->rti_tol : NULL);
    brep_trace_edges(trace, bs, r);
    if (!fixed_leaf_count)
	return 0; // MISS

    // find all the hits (XXX very inefficient right now!)
    std::list<brep_hit> hits;
    MissList misses;
    if (fixed_leaf_overflow) {
	for (std::list<const BBNode*>::const_iterator i = fallback_leaves.begin();
		i != fallback_leaves.end(); ++i) {
	    const BBNode* sbv = *i;
	    const ON_BrepFace* f = &sbv->get_face();
	    const ON_Surface* surf = f->SurfaceOf();
	    pt2d_t uv = {sbv->m_u.Mid(), sbv->m_v.Mid()};
	    utah_brep_intersect(sbv, f, surf, uv, r, hits, trace);
	}
    } else {
	for (size_t leaf_index = 0; leaf_index < fixed_leaf_count;
		++leaf_index) {
	    const BBNode* sbv = fixed_leaves[leaf_index];
	    const ON_BrepFace* f = &sbv->get_face();
	    const ON_Surface* surf = f->SurfaceOf();
	    pt2d_t uv = {sbv->m_u.Mid(), sbv->m_v.Mid()};
	    utah_brep_intersect(sbv, f, surf, uv, r, hits, trace);
	}
    }

    // sort the hits
    hits.sort();
    if (trace)
	trace->raw_hits = hits.size();

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
	// remove grazing hits with with normal to ray dot less than
	// BREP_GRAZING_DOT_TOL (>= 89.999 degrees obliq)
	TRACE("-- Remove grazing hits --");
	//int num = 0;
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
    }
    brep_trace_closure(trace, bs, hits);
    brep_trace_continuation(trace, bs, r, hits);

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
		    segp->seg_in.hit_surfno = in.face.m_face_index;
		    VSET(segp->seg_in.hit_vpriv, in.uv[0], in.uv[1], 0.0);

		    VMOVE(segp->seg_out.hit_point, out.point);
		    VMOVE(segp->seg_out.hit_normal, out.normal);
		    segp->seg_out.hit_dist = out.dist;
		    segp->seg_out.hit_surfno = out.face.m_face_index;
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
    return rt_brep_shot_impl(stp, rp, ap, seghead, NULL);
}


int
_rt_brep_shot_trace(struct soltab *stp, struct xray *rp,
    struct application *ap, struct seg *seghead,
    struct rt_brep_shot_trace *trace)
{
    if (!trace)
	return rt_brep_shot_impl(stp, rp, ap, seghead, NULL);
    *trace = {};
    trace->closure_edge_index = -1;
    trace->closure_missing_direction = -1;
    trace->continuation_face_index = -1;
    return rt_brep_shot_impl(stp, rp, ap, seghead, trace);
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

	ONX_Model model;
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
    ONX_Model model;
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

    ONX_Model model;
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
    ONX_Model model;
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

	Deserializer deserializer(*external);
	const uint32_t num_children = deserializer.read_uint32();

	for (uint32_t i = 0; i < num_children; ++i) {
	    const CurveTree * const ctree = new CurveTree(deserializer, *specific->brep->m_F.At(i));
	    specific->ctrees.push_back(ctree);
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
