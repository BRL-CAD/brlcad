/*                     L I N T _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file lint_brep.cpp
 *
 * Bounded checks for geometric contradictions that ON_Brep::IsValid()
 * intentionally permits when the stored tolerances are large enough.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "brep/cdt.h"

#include "./ged_lint.h"


namespace {

const size_t issue_report_limit = 64;
const size_t trim_sample_count = 33;
const size_t max_loop_segments = 512;
const long fast_shading_time_limit_ms = 30000;
const int fast_shading_status_unset = -1;


struct issue_set {
    std::string type;
    std::vector<nlohmann::json> details;
    nlohmann::json analysis;
    size_t count = 0;
    bool analysis_limited = false;
};


struct uv_segment {
    ON_2dPoint a;
    ON_2dPoint b;
    int trim = -1;
    int edge = -1;
    bool singular = false;
    bool seam = false;
};


struct fast_shading_face_result {
    int status = fast_shading_status_unset;
    int result = BREP_CDT_RESULT_UNATTEMPTED;
    int stage = BREP_CDT_STAGE_NONE;
    std::string message;
    bool have_diagnostic = false;
};


static bool
check_enabled(const lint_data *ldata, const char *check)
{
    if (!ldata || !check)
	return false;
    if (ldata->im_techniques.empty())
	return true;

    auto bit = ldata->im_techniques.find(std::string("brep"));
    return bit != ldata->im_techniques.end() &&
	bit->second.find(std::string(check)) != bit->second.end();
}


static bool
valid_point(const ON_3dPoint &p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}


static bool
valid_point(const ON_2dPoint &p)
{
    return std::isfinite(p.x) && std::isfinite(p.y);
}


static double
usable_tolerance(double tolerance)
{
    if (!std::isfinite(tolerance) || tolerance <= 0.0 ||
	    NEAR_EQUAL(tolerance, ON_UNSET_VALUE, ON_ZERO_TOLERANCE))
	return 0.0;
    return tolerance;
}


static double
point_segment_distance(const ON_3dPoint &p, const ON_3dPoint &a,
	const ON_3dPoint &b)
{
    ON_3dVector ab = b - a;
    const double denom = ab * ab;
    if (!(denom > 0.0))
	return p.DistanceTo(a);
    double t = ((p - a) * ab) / denom;
    t = std::max(0.0, std::min(1.0, t));
    return p.DistanceTo(a + t * ab);
}


static double
point_polyline_distance(const ON_3dPoint &p,
	const std::vector<ON_3dPoint> &polyline)
{
    double d = std::numeric_limits<double>::max();
    for (size_t i = 1; i < polyline.size(); ++i)
	d = std::min(d, point_segment_distance(p, polyline[i - 1], polyline[i]));
    return d;
}


static double
polyline_span(const std::vector<ON_3dPoint> &points)
{
    if (points.empty())
	return 0.0;
    ON_3dPoint bmin = points.front();
    ON_3dPoint bmax = points.front();
    for (const ON_3dPoint &p : points) {
	bmin.x = std::min(bmin.x, p.x);
	bmin.y = std::min(bmin.y, p.y);
	bmin.z = std::min(bmin.z, p.z);
	bmax.x = std::max(bmax.x, p.x);
	bmax.y = std::max(bmax.y, p.y);
	bmax.z = std::max(bmax.z, p.z);
    }
    return bmin.DistanceTo(bmax);
}


static double
polyline_flatness(const std::vector<ON_3dPoint> &points)
{
    double flatness = 0.0;
    for (size_t i = 2; i < points.size(); i += 2)
	flatness = std::max(flatness,
		point_segment_distance(points[i - 1], points[i - 2], points[i]));
    return flatness;
}


static bool
sample_trim_geometry(const ON_BrepTrim &trim, size_t count,
	std::vector<ON_2dPoint> *uv, std::vector<ON_3dPoint> *surface_points)
{
    const ON_BrepFace *face = trim.Face();
    const ON_Surface *surface = face ?
	static_cast<const ON_Surface *>(face) : NULL;
    if (!surface || count < 2 || !uv || !surface_points)
	return false;

    const ON_Interval domain = trim.Domain();
    if (!domain.IsIncreasing())
	return false;

    uv->clear();
    surface_points->clear();
    uv->reserve(count);
    surface_points->reserve(count);
    for (size_t i = 0; i < count; ++i) {
	const double f = (double)i / (double)(count - 1);
	const ON_2dPoint p2 = trim.PointAt(domain.ParameterAt(f));
	if (!valid_point(p2))
	    return false;
	const ON_3dPoint p3 = surface->PointAt(p2.x, p2.y);
	if (!valid_point(p3))
	    return false;
	uv->push_back(p2);
	surface_points->push_back(p3);
    }
    return true;
}


static bool
sample_edge_geometry(const ON_BrepEdge &edge, size_t count,
	std::vector<ON_3dPoint> *points)
{
    if (count < 2 || !points || !edge.EdgeCurveOf())
	return false;
    const ON_Interval domain = edge.Domain();
    if (!domain.IsIncreasing())
	return false;

    points->clear();
    points->reserve(count);
    for (size_t i = 0; i < count; ++i) {
	const double f = (double)i / (double)(count - 1);
	const ON_3dPoint p = edge.PointAt(domain.ParameterAt(f));
	if (!valid_point(p))
	    return false;
	points->push_back(p);
    }
    return true;
}


static double
sampled_curve_distance(const std::vector<ON_3dPoint> &a,
	const std::vector<ON_3dPoint> &b)
{
    double d = 0.0;
    for (const ON_3dPoint &p : a)
	d = std::max(d, point_polyline_distance(p, b));
    for (const ON_3dPoint &p : b)
	d = std::max(d, point_polyline_distance(p, a));
    return d;
}


static int
singular_side(const ON_Surface *surface, const std::vector<ON_2dPoint> &uv)
{
    if (!surface || uv.empty())
	return -1;
    const ON_Interval u = surface->Domain(0);
    const ON_Interval v = surface->Domain(1);
    const double utol = std::max(1.0e-10, std::fabs(u.Length()) * 1.0e-8);
    const double vtol = std::max(1.0e-10, std::fabs(v.Length()) * 1.0e-8);

    for (int side = 0; side < 4; ++side) {
	if (!surface->IsSingular(side))
	    continue;
	bool on_side = true;
	for (const ON_2dPoint &p : uv) {
	    if ((side == 0 && std::fabs(p.y - v.Min()) > vtol) ||
		    (side == 1 && std::fabs(p.x - u.Max()) > utol) ||
		    (side == 2 && std::fabs(p.y - v.Max()) > vtol) ||
		    (side == 3 && std::fabs(p.x - u.Min()) > utol)) {
		on_side = false;
		break;
	    }
	}
	if (on_side)
	    return side;
    }
    return -1;
}


static double
orient2d(const ON_2dPoint &a, const ON_2dPoint &b, const ON_2dPoint &c)
{
    return (b.x - a.x) * (c.y - a.y) -
	(b.y - a.y) * (c.x - a.x);
}


static bool
segments_intersect(const uv_segment &a, const uv_segment &b, double tol)
{
    if (std::max(a.a.x, a.b.x) + tol < std::min(b.a.x, b.b.x) ||
	    std::max(b.a.x, b.b.x) + tol < std::min(a.a.x, a.b.x) ||
	    std::max(a.a.y, a.b.y) + tol < std::min(b.a.y, b.b.y) ||
	    std::max(b.a.y, b.b.y) + tol < std::min(a.a.y, a.b.y))
	return false;

    const double aa = orient2d(a.a, a.b, b.a);
    const double ab = orient2d(a.a, a.b, b.b);
    const double ba = orient2d(b.a, b.b, a.a);
    const double bb = orient2d(b.a, b.b, a.b);
    const double alen = a.a.DistanceTo(a.b);
    const double blen = b.a.DistanceTo(b.b);
    const double area_tol = tol * std::max(1.0, std::max(alen, blen));

    if (((aa > area_tol && ab < -area_tol) ||
		    (aa < -area_tol && ab > area_tol)) &&
	    ((ba > area_tol && bb < -area_tol) ||
		    (ba < -area_tol && bb > area_tol)))
	return true;

    if (std::fabs(aa) > area_tol || std::fabs(ab) > area_tol ||
	    std::fabs(ba) > area_tol || std::fabs(bb) > area_tol)
	return false;

    const bool use_x = std::fabs(a.b.x - a.a.x) >= std::fabs(a.b.y - a.a.y);
    const double a0 = use_x ? std::min(a.a.x, a.b.x) : std::min(a.a.y, a.b.y);
    const double a1 = use_x ? std::max(a.a.x, a.b.x) : std::max(a.a.y, a.b.y);
    const double b0 = use_x ? std::min(b.a.x, b.b.x) : std::min(b.a.y, b.b.y);
    const double b1 = use_x ? std::max(b.a.x, b.b.x) : std::max(b.a.y, b.b.y);
    return std::min(a1, b1) - std::max(a0, b0) > tol;
}


static void
unwrap_point(ON_2dPoint *p, const ON_2dPoint &previous,
	const ON_Surface *surface)
{
    if (!p || !surface)
	return;
    for (int dir = 0; dir < 2; ++dir) {
	if (!surface->IsClosed(dir) && !surface->IsPeriodic(dir))
	    continue;
	const ON_Interval domain = surface->Domain(dir);
	const double period = std::fabs(domain.Length());
	if (!(period > 0.0))
	    continue;
	double &value = dir ? p->y : p->x;
	const double prior = dir ? previous.y : previous.x;
	while (value - prior > 0.5 * period)
	    value -= period;
	while (value - prior < -0.5 * period)
	    value += period;
    }
}


static bool
point_on_singularity(const ON_Surface *surface, const ON_2dPoint &p)
{
    std::vector<ON_2dPoint> one(1, p);
    return singular_side(surface, one) >= 0;
}


static void
record_issue(issue_set *set, nlohmann::json detail)
{
    if (!set)
	return;
    ++set->count;
    if (set->details.size() < issue_report_limit)
	set->details.push_back(std::move(detail));
}


static void
emit_issues(lint_data *ldata, struct directory *dp, const issue_set &set)
{
    if (!ldata || !dp || !set.count)
	return;

    nlohmann::json result;
    result["problem_type"] = set.type;
    result["object_type"] = "brep";
    result["object_name"] = dp->d_namep;
    result["issue_count"] = set.count;
    result["issues"] = set.details;
    result["omitted"] = set.count - set.details.size();
    result["analysis_limited"] = set.analysis_limited;
    if (!set.analysis.is_null())
	result["analysis"] = set.analysis;
    std::ostringstream log;
    log << "B-Rep " << set.type << ": " << set.count << " issue";
    if (set.count != 1)
	log << "s";
    if (set.count > set.details.size())
	log << " (" << set.count - set.details.size() << " details omitted)";
    if (set.analysis_limited)
	log << "; bounded analysis limit reached";
    result["verbose_log"] = log.str();
    ldata->j.push_back(result);
}


static const char *
fast_shading_result_name(int result)
{
    switch (result) {
	case BREP_CDT_RESULT_UNATTEMPTED: return "unattempted";
	case BREP_CDT_RESULT_SUCCESS: return "success";
	case BREP_CDT_RESULT_PARTIAL: return "partial";
	case BREP_CDT_RESULT_REPAIRED: return "repaired";
	case BREP_CDT_RESULT_INVALID_BREP: return "invalid_brep";
	case BREP_CDT_RESULT_INVALID_TOLERANCE: return "invalid_tolerance";
	case BREP_CDT_RESULT_INITIALIZATION_FAILED: return "initialization_failed";
	case BREP_CDT_RESULT_FACE_FAILED: return "face_failed";
	case BREP_CDT_RESULT_MESH_EXPORT_FAILED: return "mesh_export_failed";
	case BREP_CDT_RESULT_NON_SOLID: return "non_solid";
	case BREP_CDT_RESULT_INVALID_PSLG: return "invalid_pslg";
	case BREP_CDT_RESULT_DETRIA_FAILED: return "detria_failed";
	case BREP_CDT_RESULT_CERTIFICATION_FAILED:
	    return "certification_failed";
	case BREP_CDT_RESULT_CHART_FAILED: return "chart_failed";
	case BREP_CDT_RESULT_REFINEMENT_LIMIT: return "refinement_limit";
	case BREP_CDT_RESULT_GEOMETRIC_FAILED: return "geometric_failed";
	case BREP_CDT_RESULT_REPAIR_FAILED: return "repair_failed";
	default: return "unknown";
    }
}


static const char *
fast_shading_stage_name(int stage)
{
    switch (stage) {
	case BREP_CDT_STAGE_NONE: return "none";
	case BREP_CDT_STAGE_INPUT: return "input";
	case BREP_CDT_STAGE_TOPOLOGY: return "topology";
	case BREP_CDT_STAGE_EDGE_INITIALIZATION: return "edge_initialization";
	case BREP_CDT_STAGE_FACE_TRIANGULATION: return "face_triangulation";
	case BREP_CDT_STAGE_MESH_ASSEMBLY: return "mesh_assembly";
	case BREP_CDT_STAGE_SOLID_VALIDATION: return "solid_validation";
	case BREP_CDT_STAGE_PSLG_VALIDATION: return "pslg_validation";
	case BREP_CDT_STAGE_DETRIA: return "detria";
	case BREP_CDT_STAGE_CHART_CONSTRUCTION: return "chart_construction";
	case BREP_CDT_STAGE_ADAPTIVE_REFINEMENT: return "adaptive_refinement";
	case BREP_CDT_STAGE_GEOMETRIC_VALIDATION:
	    return "geometric_validation";
	case BREP_CDT_STAGE_MESH_REPAIR: return "mesh_repair";
	default: return "unknown";
    }
}


static void
fast_shading_face_status(int face_index, int status, void *data)
{
    std::vector<fast_shading_face_result> *results =
	static_cast<std::vector<fast_shading_face_result> *>(data);
    if (!results || face_index < 0 ||
	    (size_t)face_index >= results->size())
	return;
    (*results)[(size_t)face_index].status = status;
}


static void
fast_shading_face_diagnostic(int face_index, int result, int stage,
	const char *message, void *data)
{
    std::vector<fast_shading_face_result> *results =
	static_cast<std::vector<fast_shading_face_result> *>(data);
    if (!results || face_index < 0 ||
	    (size_t)face_index >= results->size())
	return;
    fast_shading_face_result &face = (*results)[(size_t)face_index];
    face.result = result;
    face.stage = stage;
    face.message = message ? message : "";
    face.have_diagnostic = true;
}


static nlohmann::json
fast_shading_report_json(int return_code, int triangle_count, int point_count,
	const struct brep_cdt_fast_options &options,
	const struct brep_cdt_fast_report &report)
{
    nlohmann::json summary;
    summary["return_code"] = return_code;
    summary["requested_faces"] = report.requested_faces;
    summary["completed_faces"] = report.completed_faces;
    summary["failed_or_unprocessed_faces"] = report.failed_faces;
    summary["skipped_degenerate_faces"] = report.skipped_degenerate_faces;
    summary["skipped_tolerance_faces"] = report.skipped_tolerance_faces;
    summary["approximated_faces"] = report.approximated_faces;
    summary["triangles"] = triangle_count;
    summary["points"] = point_count;
    summary["hit_time_limit"] = report.hit_time_limit != 0;
    summary["hit_memory_limit"] = report.hit_memory_limit != 0;
    summary["hit_point_limit"] = report.hit_point_limit != 0;
    summary["time_limit_ms"] = options.max_time_ms;
    summary["peak_working_bytes"] = report.peak_working_bytes;
    summary["result_bytes"] = report.result_bytes;
    summary["triangle_budget"] = report.triangle_budget;
    summary["triangle_budget_limited_faces"] =
	report.triangle_budget_limited_faces;
    summary["boundary_envelope_incomplete_faces"] =
	report.boundary_envelope_incomplete_faces;
    return summary;
}

} // namespace


void
brep_fast_shading_check(lint_data *ldata, struct directory *dp,
	struct rt_brep_internal *bi)
{
    if (!ldata || !dp || !bi || !bi->brep)
	return;

    issue_set failures;
    failures.type = "fast_shading_failure";
    issue_set incomplete;
    incomplete.type = "fast_shading_incomplete";

    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    if (ldata->ftol > 0.0) {
	tol.dist = ldata->ftol;
	tol.dist_sq = tol.dist * tol.dist;
    }

    std::vector<fast_shading_face_result> results(
	(size_t)bi->brep->m_F.Count());
    struct brep_cdt_fast_options options;
    brep_cdt_fast_options_default(&options);
    /* Lint must terminate on pathological input; a limit means that the
     * capability result is inconclusive rather than that a face is bad. */
    options.max_time_ms = fast_shading_time_limit_ms;
    options.face_status = fast_shading_face_status;
    options.face_status_data = &results;
    options.face_diagnostic = fast_shading_face_diagnostic;
    options.face_diagnostic_data = &results;

    int *mesh_faces = NULL;
    int triangle_count = 0;
    vect_t *normals = NULL;
    point_t *points = NULL;
    int point_count = 0;
    struct brep_cdt_fast_report report = {};
    const int return_code = brep_cdt_fast_ex(&mesh_faces, &triangle_count,
	&normals, &points, &point_count, bi->brep, -1, &ttol, &tol,
	&options, &report);

    size_t reported_faces = 0;

    for (size_t face_index = 0; face_index < results.size();
	    ++face_index) {
	const fast_shading_face_result &face = results[face_index];
	issue_set *issues = NULL;
	const char *status = NULL;
	if (face.status == BREP_CDT_FAST_FACE_FAILED) {
	    issues = &failures;
	    status = "failed";
	} else if (face.status == BREP_CDT_FAST_FACE_NOT_PROCESSED) {
	    issues = &incomplete;
	    status = "not_processed";
	}
	if (!issues)
	    continue;

	nlohmann::json detail;
	detail["face"] = face_index;
	detail["status"] = status;
	if (face.have_diagnostic) {
	    detail["result"] = face.result;
	    detail["result_name"] = fast_shading_result_name(face.result);
	    detail["stage"] = face.stage;
	    detail["stage_name"] = fast_shading_stage_name(face.stage);
	    detail["message"] = face.message;
	}
	record_issue(issues, detail);
	reported_faces++;
    }

    if (!reported_faces && return_code != BREP_CDT_FAST_OK) {
	nlohmann::json detail;
	detail["face"] = -1;
	detail["status"] = return_code == BREP_CDT_FAST_LIMIT ?
	    "not_processed" : "request_failed";
	detail["message"] = "fast shading ended before reporting a face result";
	record_issue(return_code == BREP_CDT_FAST_LIMIT ? &incomplete :
	    &failures, detail);
    }

    const bool limited = report.hit_time_limit || report.hit_memory_limit ||
	report.hit_point_limit || return_code == BREP_CDT_FAST_LIMIT;
    incomplete.analysis_limited = limited;
    failures.analysis_limited = limited;
    const nlohmann::json summary = fast_shading_report_json(return_code,
	triangle_count, point_count, options, report);
    failures.analysis = summary;
    incomplete.analysis = summary;

    emit_issues(ldata, dp, failures);
    emit_issues(ldata, dp, incomplete);

    bu_free(mesh_faces, "B-Rep lint fast shading faces");
    bu_free(normals, "B-Rep lint fast shading normals");
    bu_free(points, "B-Rep lint fast shading points");
}


void
brep_checks(lint_data *ldata, struct directory *dp, struct rt_brep_internal *bi)
{
    if (!ldata || !dp || !bi || !bi->brep)
	return;
    const ON_Brep *brep = bi->brep;

    issue_set mismatch;
    mismatch.type = "edge_surface_mismatch";
    issue_set singular;
    singular.type = "singular_boundary";
    issue_set large_tol;
    large_tol.type = "large_tolerance";
    issue_set crossings;
    crossings.type = "trim_loop_crossing";

    ON_BoundingBox brep_bbox;
    brep->GetBoundingBox(brep_bbox, false);
    double model_diag = 1.0;
    if (brep_bbox.IsValid())
	model_diag = std::max(1.0, brep_bbox.m_min.DistanceTo(brep_bbox.m_max));
    const double tolerance = std::max((double)ldata->ftol,
	model_diag * 1.0e-10);

    if (check_enabled(ldata, "large_tolerance")) {
	for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    const double declared_tolerance =
		usable_tolerance(edge.m_tolerance);
	    if (!(declared_tolerance > 0.0))
		continue;
	    std::vector<ON_3dPoint> points;
	    if (!sample_edge_geometry(edge, 17, &points))
		continue;
	    const double span = polyline_span(points);
	    const double ratio = declared_tolerance /
		std::max(span, tolerance);
	    if (declared_tolerance <= 100.0 * tolerance || ratio <= 0.25)
		continue;
	    nlohmann::json detail;
	    detail["edge"] = ei;
	    detail["declared_tolerance"] = declared_tolerance;
	    detail["sampled_edge_span"] = span;
	    detail["tolerance_span_ratio"] = ratio;
	    record_issue(&large_tol, detail);
	}
    }

    if (check_enabled(ldata, "edge_surface_mismatch") ||
	    check_enabled(ldata, "singular_boundary")) {
	for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	    const ON_BrepTrim &trim = brep->m_T[ti];
	    if (trim.m_type == ON_BrepTrim::singular || trim.m_ei < 0 ||
		    trim.m_ei >= brep->m_E.Count())
		continue;
	    const ON_BrepFace *face = trim.Face();
	    const ON_BrepLoop *loop = trim.Loop();
	    const ON_Surface *surface = face ?
		static_cast<const ON_Surface *>(face) : NULL;
	    const ON_Surface *native_surface = face ? face->SurfaceOf() : NULL;
	    if (!face || !loop || !surface)
		continue;

	    std::vector<ON_2dPoint> uv;
	    std::vector<ON_3dPoint> surface_points;
	    std::vector<ON_3dPoint> edge_points;
	    if (!sample_trim_geometry(trim, trim_sample_count, &uv,
		    &surface_points) ||
		    !sample_edge_geometry(brep->m_E[trim.m_ei], trim_sample_count,
		    &edge_points))
		continue;

	    if (check_enabled(ldata, "singular_boundary")) {
		int side = singular_side(surface, uv);
		if (side < 0 && native_surface && native_surface != surface)
		    side = singular_side(native_surface, uv);
		const double edge_span = polyline_span(edge_points);
		const double surface_span = polyline_span(surface_points);
		const bool collapsed_surface_path = edge_span > tolerance &&
		    std::max(surface_span, tolerance) * 8.0 < edge_span;
		if (collapsed_surface_path) {
		    nlohmann::json detail;
		    detail["face"] = face->m_face_index;
		    detail["loop"] = loop->m_loop_index;
		    detail["trim"] = ti;
		    detail["edge"] = trim.m_ei;
		    detail["singular_side"] = side;
		    detail["surface_side_classified_singular"] = side >= 0;
		    detail["sampled_edge_span"] = edge_span;
		    detail["sampled_surface_span"] = surface_span;
		    detail["surface_edge_span_ratio"] =
			surface_span / edge_span;
		    detail["declared_edge_tolerance"] =
			brep->m_E[trim.m_ei].m_tolerance;
		    record_issue(&singular, detail);
		}
	    }

	    if (check_enabled(ldata, "edge_surface_mismatch")) {
		const double sampled = sampled_curve_distance(surface_points,
		    edge_points);
		const double sampling_error = 2.0 * std::max(
		    polyline_flatness(surface_points), polyline_flatness(edge_points));
		const double lower_bound = std::max(0.0, sampled - sampling_error);
		if (lower_bound > tolerance) {
		    const double declared_edge = usable_tolerance(
			brep->m_E[trim.m_ei].m_tolerance);
		    const double declared_trim = std::max(
			usable_tolerance(trim.m_tolerance[0]),
			usable_tolerance(trim.m_tolerance[1]));
		    const double declared = std::max(declared_edge,
			declared_trim);
		    nlohmann::json detail;
		    detail["face"] = face->m_face_index;
		    detail["loop"] = loop->m_loop_index;
		    detail["trim"] = ti;
		    detail["edge"] = trim.m_ei;
		    detail["sampled_distance"] = sampled;
		    detail["sampled_edge_span"] = polyline_span(edge_points);
		    detail["sampled_surface_span"] =
			polyline_span(surface_points);
		    detail["sampling_allowance"] = sampling_error;
		    detail["minimum_estimated_mismatch"] = lower_bound;
		    detail["threshold"] = tolerance;
		    detail["declared_edge_tolerance"] = declared_edge;
		    detail["declared_trim_tolerance"] = declared_trim;
		    detail["effective_declared_tolerance"] = declared;
		    detail["declared_tolerance_masks"] =
			std::isfinite(declared) && declared >= lower_bound;
		    record_issue(&mismatch, detail);
		}
	    }
	}
    }

    if (check_enabled(ldata, "trim_loop_crossing")) {
	for (int li = 0; li < brep->m_L.Count(); ++li) {
	    const ON_BrepLoop &loop = brep->m_L[li];
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ?
		static_cast<const ON_Surface *>(face) : NULL;
	    if (!face || !surface || loop.m_type == ON_BrepLoop::slit ||
		    loop.m_type == ON_BrepLoop::crvonsrf ||
		    loop.m_type == ON_BrepLoop::ptonsrf || loop.TrimCount() < 2)
		continue;

	    const size_t segments_per_trim = std::max((size_t)1,
		std::min((size_t)8, max_loop_segments /
		    (size_t)loop.TrimCount()));
	    if (segments_per_trim < 8)
		crossings.analysis_limited = true;
	    std::vector<uv_segment> segments;
	    segments.reserve(segments_per_trim * loop.TrimCount());
	    ON_2dPoint previous;
	    bool have_previous = false;
	    bool sample_failed = false;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		if (!trim || !trim->TrimCurveOf()) {
		    sample_failed = true;
		    break;
		}
		const ON_Interval domain = trim->Domain();
		if (!domain.IsIncreasing()) {
		    sample_failed = true;
		    break;
		}
		ON_2dPoint prior;
		for (size_t si = 0; si <= segments_per_trim; ++si) {
		    ON_2dPoint p = trim->PointAt(domain.ParameterAt(
			(double)si / (double)segments_per_trim));
		    if (!valid_point(p)) {
			sample_failed = true;
			break;
		    }
		    if (si == 0 && have_previous)
			unwrap_point(&p, previous, surface);
		    if (si > 0)
			unwrap_point(&p, prior, surface);
		    if (si > 0) {
			uv_segment seg;
			seg.a = prior;
			seg.b = p;
			seg.trim = trim->m_trim_index;
			seg.edge = trim->m_ei;
			seg.singular = point_on_singularity(surface, prior) ||
			    point_on_singularity(surface, p);
			seg.seam = trim->m_type == ON_BrepTrim::seam;
			segments.push_back(seg);
		    }
		    prior = p;
		}
		if (sample_failed)
		    break;
		previous = prior;
		have_previous = true;
	    }
	    if (sample_failed || segments.size() < 4)
		continue;

	    const ON_Interval u = surface->Domain(0);
	    const ON_Interval v = surface->Domain(1);
	    const double uvscale = std::max(1.0,
		std::max(std::fabs(u.Length()), std::fabs(v.Length())));
	    const double uvtol = uvscale * 1.0e-10;
	    bool found = false;
	    for (size_t i = 0; i < segments.size() && !found; ++i) {
		for (size_t j = i + 1; j < segments.size(); ++j) {
		    if (j == i + 1 || (i == 0 && j + 1 == segments.size()))
			continue;
		    const uv_segment &a = segments[i];
		    const uv_segment &b = segments[j];
		    if (a.singular || b.singular ||
			    (a.seam && b.seam && a.edge >= 0 && a.edge == b.edge))
			continue;
		    if (!segments_intersect(a, b, uvtol))
			continue;
		    nlohmann::json detail;
		    detail["face"] = face->m_face_index;
		    detail["loop"] = li;
		    detail["trim_a"] = a.trim;
		    detail["trim_b"] = b.trim;
		    detail["edge_a"] = a.edge;
		    detail["edge_b"] = b.edge;
		    detail["segment_a"] = {{a.a.x, a.a.y}, {a.b.x, a.b.y}};
		    detail["segment_b"] = {{b.a.x, b.a.y}, {b.b.x, b.b.y}};
		    detail["samples_per_trim"] = segments_per_trim + 1;
		    record_issue(&crossings, detail);
		    found = true;
		    break;
		}
	    }
	}
    }

    emit_issues(ldata, dp, mismatch);
    emit_issues(ldata, dp, singular);
    emit_issues(ldata, dp, large_tol);
    emit_issues(ldata, dp, crossings);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
