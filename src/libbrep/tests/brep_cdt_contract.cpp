/*                 B R E P _ C D T _ C O N T R A C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <limits>
#include <vector>

#include "brep/cdt.h"
#include "brep/surfacetree.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/brep.h"

struct mesh_output {
    std::vector<int> faces;
    std::vector<fastf_t> vertices;
};

struct repair_provenance_capture {
    int calls = 0;
    int tier = BREP_CDT_REPAIR_APPROX_NONE;
    std::vector<int> faces;
    std::vector<int> edges;
};

static void
repair_provenance_callback(int tier, const int *faces, size_t face_count,
	const int *edges, size_t edge_count, void *data)
{
    repair_provenance_capture *capture =
	(repair_provenance_capture *)data;
    if (!capture)
	return;
    capture->calls++;
    capture->tier = tier;
    if (faces && face_count)
	capture->faces.assign(faces, faces + face_count);
    if (edges && edge_count)
	capture->edges.assign(edges, edges + edge_count);
}

static bool
empty_mesh_contract()
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    ON_Brep *box = ON_BrepBox(corners);
    if (!box)
	return false;
    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(box,
	"empty mesh contract");
    int *faces = (int *)1;
    fastf_t *vertices = (fastf_t *)1;
    int face_count = 1;
    int vertex_count = 1;
    const bool rejected = ON_Brep_CDT_Mesh(&faces, &face_count, &vertices,
	&vertex_count, NULL, NULL, NULL, NULL, state, 0, NULL) < 0 &&
	!faces && !vertices && !face_count && !vertex_count;
    ON_Brep_CDT_Destroy(state);
    delete box;
    return rejected;
}

static bool
untrimmed_surface_tree_contract()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetDomain(0, -1.0, 1.0);
    surface->SetDomain(1, -1.0, 1.0);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    ON_BrepFace &face = brep.NewFace(brep.AddSurface(surface));
    brlcad::SurfaceTree tree(&face, true);
    if (!tree.Valid())
	return false;
    ON_Interval u, v;
    const ON_3dPoint point(0.25, -0.5, 0.75);
    const ON_2dPoint uv = tree.getClosestPointEstimate(point, u, v);
    const ON_3dPoint projection = surface->PointAt(uv.x, uv.y);
    return u.Includes(uv.x) && v.Includes(uv.y) &&
	std::isfinite(projection.x) && std::isfinite(projection.y) &&
	std::isfinite(projection.z);
}

static bool
mesh_get(mesh_output &output, struct ON_Brep_CDT_State *state)
{
    int *faces = NULL;
    fastf_t *vertices = NULL;
    int face_count = 0;
    int vertex_count = 0;
    if (ON_Brep_CDT_Mesh(&faces, &face_count, &vertices, &vertex_count,
	    NULL, NULL, NULL, NULL, state, 0, NULL) < 0)
	return false;
    output.faces.assign(faces, faces + (size_t)face_count * 3);
    output.vertices.assign(vertices, vertices + (size_t)vertex_count * 3);
    bu_free(faces, "contract test faces");
    bu_free(vertices, "contract test vertices");
    return true;
}

static bool
normal_mesh_get(struct ON_Brep_CDT_State *state)
{
    int *faces = NULL;
    fastf_t *vertices = NULL;
    int *face_normals = NULL;
    fastf_t *normals = NULL;
    int face_count = 0;
    int vertex_count = 0;
    int face_normal_count = 0;
    int normal_count = 0;
    if (ON_Brep_CDT_Mesh(&faces, &face_count, &vertices, &vertex_count,
	    &face_normals, &face_normal_count, &normals, &normal_count,
	    state, 0, NULL) < 0)
	return false;
    bool valid = face_count > 0 && vertex_count > 0 &&
	face_normal_count == face_count && normal_count > 0;
    for (int i = 0; valid && i < face_normal_count * 3; ++i)
	valid = face_normals[i] >= 0 && face_normals[i] < normal_count;
    bu_free(faces, "contract normal test faces");
    bu_free(vertices, "contract normal test vertices");
    bu_free(face_normals, "contract normal test face normals");
    bu_free(normals, "contract normal test normals");
    return valid;
}

static bool
repair_contract()
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    ON_Brep *box = ON_BrepBox(corners);
    if (!box)
	return false;
    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(box,
	"repair contract");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    std::vector<int> partial_faces;
    for (int face = 1; face < box->m_F.Count(); ++face)
	partial_faces.push_back(face);
    const int partial_result = ON_Brep_CDT_Tessellate(state,
	(int)partial_faces.size(), partial_faces.data());

    struct brep_cdt_repair_settings settings =
	BREP_CDT_REPAIR_SETTINGS_INIT;
    settings.mesh.fill_holes = 1;
    settings.mesh.max_hole_area_percent = 30.0;
    settings.mesh.max_hole_edges = 64;
    settings.max_area_change_percent = 30.0;
    settings.use_full_fast_fallback_if_needed = 1;
    repair_provenance_capture provenance;
    settings.provenance = repair_provenance_callback;
    settings.provenance_data = &provenance;
    struct brep_cdt_repair_report report = BREP_CDT_REPAIR_REPORT_INIT;
    struct brep_cdt_diagnostic diagnostic;
    const int repair_result = ON_Brep_CDT_Repair(state, &settings, &report);
    const int approximation_tier = report.approximation_tier;
    const int approximation_faces = report.approximation_faces;
    const int approximation_edges = report.approximation_edges;
    const int missing_rigorous = report.missing_rigorous_triangles;
    bool repaired = partial_result == (int)partial_faces.size() &&
	repair_result == 0 &&
	ON_Brep_CDT_Status(state) == 0 &&
	report.source_diagnostic.result == BREP_CDT_RESULT_PARTIAL &&
	report.mesh.solid && report.mesh.added_faces > 0 &&
	!report.full_fast_fallback_used &&
	report.changed_faces > 0 &&
	report.deviation_projection_failures == 0 &&
	approximation_tier == BREP_CDT_REPAIR_APPROX_LOCAL_MESH &&
	approximation_faces == 1 && approximation_edges == 4 &&
	!missing_rigorous && provenance.calls == 1 &&
	provenance.tier == BREP_CDT_REPAIR_APPROX_LOCAL_MESH &&
	provenance.faces == std::vector<int>({0}) &&
	provenance.edges.size() == 4 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_REPAIRED &&
	diagnostic.stage == BREP_CDT_STAGE_MESH_REPAIR &&
	normal_mesh_get(state) &&
	ON_Brep_CDT_Repair(state, &settings, &report) == 1 &&
	report.source_diagnostic.result == BREP_CDT_RESULT_PARTIAL;
    if (!repaired) {
	ON_Brep_CDT_Diagnostic(&diagnostic, state);
	bu_log("repair contract failed: partial %d/%zu, result %d stage %d, "
	    "solid %d, added %d, changed %d, projection failures %zu, "
	    "approx %d/%d/%d, missing %d, callback %d/%d/%zu/%zu: %s\n",
	    partial_result, partial_faces.size(), diagnostic.result,
	    diagnostic.stage, report.mesh.solid, report.mesh.added_faces,
	    report.changed_faces, report.deviation_projection_failures,
	    approximation_tier, approximation_faces, approximation_edges,
	    missing_rigorous, provenance.calls, provenance.tier,
	    provenance.faces.size(), provenance.edges.size(),
	    diagnostic.message);
    }
    ON_Brep_CDT_Destroy(state);
    delete box;
    return repaired;
}

static bool
relaxed_invalid_repair_contract()
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    ON_Brep *box = ON_BrepBox(corners);
    if (!box)
	return false;
    ON_BrepVertex &invalid_vertex = box->NewVertex(
	ON_3dPoint(0.5, 0.5, 0.5));
    invalid_vertex.m_vertex_index = -1;

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(box,
	"relaxed invalid repair contract");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    struct brep_cdt_diagnostic diagnostic = {};
    const bool rejected = ON_Brep_CDT_Tessellate(state, 0, NULL) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_INVALID_BREP;

    struct brep_cdt_repair_settings settings =
	BREP_CDT_REPAIR_SETTINGS_INIT;
    settings.mesh.fill_holes = 1;
    settings.mesh.max_hole_area_percent = 100.0;
    settings.mesh.max_hole_edges = 4096;
    settings.mesh.require_manifold = 1;
    settings.max_surface_deviation = 0.5;
    settings.max_area_change_percent = 100.0;
    settings.allow_untrimmed_surface_match = 1;
    settings.try_invalid_brep = 1;
    settings.use_full_fast_fallback_if_needed = 1;
    struct brep_cdt_repair_report report = BREP_CDT_REPAIR_REPORT_INIT;
    mesh_output output;
    const bool repaired = rejected &&
	ON_Brep_CDT_Repair(state, &settings, &report) == 0 &&
	report.relaxed_tessellation_attempted &&
	report.relaxed_tessellation_completed_faces == box->m_F.Count() &&
	!report.full_fast_fallback_used && report.mesh.solid &&
	report.mesh.manifold_accepted && mesh_get(output, state) &&
	!output.faces.empty();
    if (!repaired) {
	ON_Brep_CDT_Diagnostic(&diagnostic, state);
	bu_log("relaxed invalid repair contract failed: result %d stage %d, "
	    "relaxed %d/%d, full fast %d, solid %d, Manifold %d: %s\n",
	    diagnostic.result, diagnostic.stage,
	    report.relaxed_tessellation_attempted,
	    report.relaxed_tessellation_completed_faces,
	    report.full_fast_fallback_used, report.mesh.solid,
	    report.mesh.manifold_accepted, diagnostic.message);
    }
    ON_Brep_CDT_Destroy(state);
    delete box;
    return repaired;
}

static bool
relaxed_missing_outer_repair_contract()
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    ON_Brep *box = ON_BrepBox(corners);
    if (!box || !box->m_F[0].OuterLoop()) {
	delete box;
	return false;
    }
    box->m_F[0].OuterLoop()->m_type = ON_BrepLoop::inner;
    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(box,
	"relaxed missing outer contract");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    const bool rejected = ON_Brep_CDT_Tessellate(state, 0, NULL) == -1;
    struct brep_cdt_repair_settings settings =
	BREP_CDT_REPAIR_SETTINGS_INIT;
    settings.mesh.fill_holes = 1;
    settings.mesh.max_hole_area_percent = 100.0;
    settings.mesh.max_hole_edges = 4096;
    settings.mesh.require_manifold = 1;
    settings.max_surface_deviation = 0.5;
    settings.max_area_change_percent = 100.0;
    settings.allow_untrimmed_surface_match = 1;
    settings.try_invalid_brep = 1;
    struct brep_cdt_repair_report report = BREP_CDT_REPAIR_REPORT_INIT;
    mesh_output output;
    const bool repaired = rejected &&
	ON_Brep_CDT_Repair(state, &settings, &report) == 0 &&
	report.relaxed_tessellation_attempted &&
	report.relaxed_tessellation_completed_faces == box->m_F.Count() &&
	!report.full_fast_fallback_used && report.mesh.solid &&
	report.mesh.manifold_accepted && mesh_get(output, state) &&
	!output.faces.empty();
    if (!repaired) {
	struct brep_cdt_diagnostic diagnostic = {};
	ON_Brep_CDT_Diagnostic(&diagnostic, state);
	bu_log("relaxed missing outer contract failed: result %d stage %d, "
	    "relaxed %d/%d, solid %d, Manifold %d: %s\n",
	    diagnostic.result, diagnostic.stage,
	    report.relaxed_tessellation_attempted,
	    report.relaxed_tessellation_completed_faces, report.mesh.solid,
	    report.mesh.manifold_accepted, diagnostic.message);
    }
    ON_Brep_CDT_Destroy(state);
    delete box;
    return repaired;
}

static bool
paired_pcurve_edge_contract()
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    ON_Brep *box = ON_BrepBox(corners);
    if (!box || box->m_E.Count() <= 0) {
	delete box;
	return false;
    }
    ON_BrepEdge &edge = box->m_E[0];
    const ON_3dPoint start = edge.PointAtStart();
    const ON_3dPoint end = edge.PointAtEnd();
    ON_NurbsCurve *bad_curve = new ON_NurbsCurve(3, false, 3, 3);
    bad_curve->MakeClampedUniformKnotVector(1.0);
    bad_curve->SetCV(0, start);
    bad_curve->SetCV(1, 0.5 * (start + end) +
	ON_3dVector(0.0, 0.0, 10.0));
    bad_curve->SetCV(2, end);
    const int curve_index = box->AddEdgeCurve(bad_curve);
    if (curve_index < 0 || !edge.ChangeEdgeCurve(curve_index)) {
	delete box;
	return false;
    }
    edge.m_tolerance = 1.0e-5;

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(box,
	"paired p-curve edge contract");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    const int tessellation_result = ON_Brep_CDT_Tessellate(state, 0, NULL);
    struct brep_cdt_diagnostic diagnostic = {};
    ON_Brep_CDT_Diagnostic(&diagnostic, state);
    bool valid = tessellation_result > 0 &&
	diagnostic.stage > BREP_CDT_STAGE_EDGE_INITIALIZATION &&
	diagnostic.completed_faces > 0;
    struct brep_cdt_repair_settings settings =
	BREP_CDT_REPAIR_SETTINGS_INIT;
    settings.mesh.fill_holes = 1;
    settings.mesh.max_hole_area_percent = 100.0;
    settings.mesh.max_hole_edges = 4096;
    settings.mesh.allow_self_intersections = 1;
    settings.max_surface_deviation = 20.0;
    settings.max_area_change_percent = 100.0;
    settings.allow_untrimmed_surface_match = 1;
    struct brep_cdt_repair_report report = BREP_CDT_REPAIR_REPORT_INIT;
    ON_Brep_CDT_Repair(state, &settings, &report);
    valid = valid && report.fast_fallback_used_faces == 2 &&
	report.fast_fallback_constrained_edges >= 6 &&
	report.fast_fallback_constrained_samples > 0 &&
	report.missing_rigorous_triangles == 0;
    if (!valid) {
	bu_log("paired p-curve edge contract failed: result %d stage %d, "
	    "%d faces completed, fast %d constrained %zu/%zu: %s\n",
	    tessellation_result, diagnostic.stage, diagnostic.completed_faces,
	    report.fast_fallback_used_faces,
	    report.fast_fallback_constrained_edges,
	    report.fast_fallback_constrained_samples, diagnostic.message);
    }
    ON_Brep_CDT_Destroy(state);
    delete box;
    return valid;
}

static bool
invalid_poisson_repair_contract()
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    ON_Brep *box = ON_BrepBox(corners);
    if (!box)
	return false;
    ON_BrepVertex &invalid_vertex = box->NewVertex(
	ON_3dPoint(2.0, 2.0, 2.0));
    invalid_vertex.m_vertex_index = -1;

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(box,
	"invalid Poisson repair contract");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    struct brep_cdt_diagnostic diagnostic = {};
    const bool rejected = ON_Brep_CDT_Tessellate(state, 0, NULL) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_INVALID_BREP;

    struct brep_cdt_repair_settings settings =
	BREP_CDT_REPAIR_SETTINGS_INIT;
    settings.mesh.fill_holes = 1;
    settings.mesh.max_hole_area_percent = 100.0;
    settings.mesh.max_hole_edges = 4096;
    settings.mesh.union_components = 1;
    settings.max_surface_deviation = 0.5;
    settings.max_area_change_percent = 100.0;
    settings.allow_untrimmed_surface_match = 1;
    settings.use_full_fast_fallback = 1;
    settings.use_poisson_reconstruction = 1;
    settings.poisson_depth = 5;
    struct brep_cdt_repair_report report = BREP_CDT_REPAIR_REPORT_INIT;
    struct ON_Brep_CDT_State *local_state = ON_Brep_CDT_Create(box,
	"invalid local repair contract");
    ON_Brep_CDT_Tol_Set(local_state, &tolerance);
    struct brep_cdt_diagnostic local_diagnostic = {};
    mesh_output local_output;
    const bool local_repaired =
	ON_Brep_CDT_Tessellate(local_state, 0, NULL) == -1 &&
	ON_Brep_CDT_Repair(local_state, &settings, &report) == 0 &&
	!report.poisson_reconstruction_attempted &&
	!report.poisson_reconstruction_applied && !report.poisson_attempts &&
	report.mesh.solid && report.coverage_samples > 0 &&
	report.coverage_failures == 0 && mesh_get(local_output, local_state) &&
	!local_output.faces.empty() && normal_mesh_get(local_state);
    if (!local_repaired) {
	ON_Brep_CDT_Diagnostic(&local_diagnostic, local_state);
	bu_log("invalid local repair contract failed: result %d stage %d, "
	    "Poisson %d/%d, solid %d: %s\n", local_diagnostic.result,
	    local_diagnostic.stage, report.poisson_reconstruction_attempted,
	    report.poisson_reconstruction_applied, report.mesh.solid,
	    local_diagnostic.message);
    }
    ON_Brep_CDT_Destroy(local_state);

    settings.poisson_scale = 1.1;
    report = BREP_CDT_REPAIR_REPORT_INIT;
    mesh_output output;
    const bool repaired = local_repaired && rejected &&
	ON_Brep_CDT_Repair(state, &settings, &report) == 0 &&
	report.poisson_reconstruction_attempted &&
	report.poisson_reconstruction_applied &&
	report.poisson_components == 1 &&
	report.poisson_attempts >= 1 && report.poisson_attempts <= 2 &&
	NEAR_EQUAL(report.poisson_scale,
	    report.poisson_attempts == 1 ? 1.1 : 1.2, SMALL_FASTF) &&
	report.mesh.solid &&
	report.changed_faces > 0 && report.deviation_projection_failures == 0 &&
	report.coverage_samples > 0 && report.coverage_failures == 0 &&
	report.max_coverage_deviation <= report.allowed_surface_deviation &&
	mesh_get(output, state) && !output.faces.empty() &&
	normal_mesh_get(state);
    if (!repaired) {
	ON_Brep_CDT_Diagnostic(&diagnostic, state);
	bu_log("invalid Poisson repair contract failed: rejected %d, "
	    "result %d stage %d, Poisson %d/%d, attempts %d scale %.17g, "
	    "components %d, solid %d, changed %d, projection failures %zu, "
	    "coverage %zu/%zu max %.17g/%.17g: %s\n", (int)rejected,
	    diagnostic.result, diagnostic.stage,
	    report.poisson_reconstruction_attempted,
	    report.poisson_reconstruction_applied, report.poisson_attempts,
	    report.poisson_scale, report.poisson_components, report.mesh.solid,
	    report.changed_faces, report.deviation_projection_failures,
	    report.coverage_failures, report.coverage_samples,
	    report.max_coverage_deviation, report.allowed_surface_deviation,
	    diagnostic.message);
    }
    ON_Brep_CDT_Destroy(state);
    delete box;
    return repaired;
}

static bool
component_poisson_repair_contract()
{
    ON_3dPoint large_corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    ON_3dPoint small_corners[8] = {
	ON_3dPoint(10.0, 0.0, 0.0), ON_3dPoint(10.1, 0.0, 0.0),
	ON_3dPoint(10.1, 0.1, 0.0), ON_3dPoint(10.0, 0.1, 0.0),
	ON_3dPoint(10.0, 0.0, 0.1), ON_3dPoint(10.1, 0.0, 0.1),
	ON_3dPoint(10.1, 0.1, 0.1), ON_3dPoint(10.0, 0.1, 0.1)
    };
    ON_Brep *brep = ON_BrepBox(large_corners);
    ON_Brep *small = ON_BrepBox(small_corners);
    if (!brep || !small) {
	delete brep;
	delete small;
	return false;
    }
    brep->Append(*small);
    delete small;
    ON_BrepVertex &invalid_vertex = brep->NewVertex(
	ON_3dPoint(20.0, 20.0, 20.0));
    invalid_vertex.m_vertex_index = -1;

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(brep,
	"component Poisson repair contract");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    struct brep_cdt_diagnostic diagnostic = {};
    const bool rejected = ON_Brep_CDT_Tessellate(state, 0, NULL) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_INVALID_BREP;

    struct brep_cdt_repair_settings settings =
	BREP_CDT_REPAIR_SETTINGS_INIT;
    settings.mesh.fill_holes = 1;
    settings.mesh.max_hole_area_percent = 100.0;
    settings.mesh.max_hole_edges = 4096;
    settings.mesh.union_components = 1;
    settings.max_surface_deviation = 0.5;
    settings.max_area_change_percent = 100.0;
    settings.allow_untrimmed_surface_match = 1;
    settings.use_full_fast_fallback = 1;
    settings.use_poisson_reconstruction = 1;
    settings.poisson_depth = 5;
    settings.poisson_scale = 1.1;
    settings.max_poisson_components = 1;
    struct brep_cdt_repair_report report = BREP_CDT_REPAIR_REPORT_INIT;
    const bool bounded = rejected &&
	ON_Brep_CDT_Repair(state, &settings, &report) == -1 &&
	report.poisson_reconstruction_attempted &&
	!report.poisson_reconstruction_applied &&
	report.poisson_components == 2;

    settings.max_poisson_components = 2;
    mesh_output output;
    const bool repaired = bounded &&
	ON_Brep_CDT_Repair(state, &settings, &report) == 0 &&
	report.poisson_reconstruction_applied &&
	report.poisson_components == 2 && report.mesh.solid &&
	report.coverage_samples > 0 && report.coverage_failures == 0 &&
	mesh_get(output, state) && !output.faces.empty();
    if (!repaired) {
	ON_Brep_CDT_Diagnostic(&diagnostic, state);
	bu_log("component Poisson repair contract failed: rejected %d, "
	    "bounded %d, result %d stage %d, components %d, solid %d: %s\n",
	    (int)rejected, (int)bounded, diagnostic.result,
	    diagnostic.stage, report.poisson_components, report.mesh.solid,
	    diagnostic.message);
    }
    ON_Brep_CDT_Destroy(state);
    delete brep;
    return repaired;
}

int
main(int argc, const char **argv)
{
    if (argc != 3)
	return 2;

    struct db_i *dbip = db_open(argv[1], DB_OPEN_READONLY);
    if (dbip == DBI_NULL || db_dirbuild(dbip) < 0)
	return 2;
    struct directory *dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	db_close(dbip);
	return 2;
    }

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0 ||
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	db_close(dbip);
	return 2;
    }
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(bi->brep,
	dp->d_namep);
    struct brep_cdt_diagnostic diagnostic;
    bool initial = ON_Brep_CDT_Status(state) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_UNATTEMPTED;

    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    bool first_ok = ON_Brep_CDT_Tessellate(state, 0, NULL) == 0 &&
	ON_Brep_CDT_Status(state) == 0 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_SUCCESS &&
	ON_Brep_CDT_Failed_Faces(NULL, 0, state) == 0 &&
	ON_Brep_CDT_Failed_Faces(NULL, -1, state) == -1 &&
	ON_Brep_CDT_Face_Diagnostic(&diagnostic, 0, state) == -1 &&
	ON_Brep_CDT_Face_Diagnostic(NULL, 0, state) == -1;
    mesh_output first;
    first_ok = first_ok && mesh_get(first, state) && !first.faces.empty();
    mesh_output repeated;
    first_ok = first_ok && mesh_get(repeated, state) &&
	first.faces == repeated.faces && first.vertices == repeated.vertices &&
	normal_mesh_get(state);

    bool second_ok = ON_Brep_CDT_Tessellate(state, 0, NULL) == 0;
    mesh_output second;
    second_ok = second_ok && mesh_get(second, state) &&
	first.faces == second.faces && first.vertices == second.vertices;

    bool empty_rejected = empty_mesh_contract();
    bool untrimmed_tree = untrimmed_surface_tree_contract();
    bool repaired = repair_contract();
    bool relaxed_invalid = relaxed_invalid_repair_contract();
    bool relaxed_outer = relaxed_missing_outer_repair_contract();
    bool paired_edge = paired_pcurve_edge_contract();
    bool invalid_repaired = invalid_poisson_repair_contract();
    bool components_repaired = component_poisson_repair_contract();

    ON_Brep_CDT_Tol_Set(state, &tolerance);
    bool invalidated = ON_Brep_CDT_Status(state) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_UNATTEMPTED;

    tolerance.abs = std::numeric_limits<fastf_t>::quiet_NaN();
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    bool invalid_tolerance = ON_Brep_CDT_Tessellate(state, 0, NULL) == -1 &&
	ON_Brep_CDT_Status(state) == -3 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_INVALID_TOLERANCE;

    tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    int bad_face = bi->brep->m_F.Count();
    bool invalid_face = ON_Brep_CDT_Tessellate(state, 1, &bad_face) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_INVALID_BREP;

    ON_Brep_CDT_Destroy(state);
    rt_db_free_internal(&intern);
    db_close(dbip);

    ON_Brep invalid_brep;
    state = ON_Brep_CDT_Create(&invalid_brep, "invalid");
    bool invalid_input = ON_Brep_CDT_Tessellate(state, 0, NULL) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_INVALID_BREP &&
	diagnostic.stage == BREP_CDT_STAGE_TOPOLOGY;
    ON_Brep_CDT_Destroy(state);

    return initial && first_ok && second_ok && empty_rejected &&
	untrimmed_tree && repaired && relaxed_invalid && relaxed_outer &&
	paired_edge && invalid_repaired && components_repaired &&
	invalidated && invalid_tolerance && invalid_face && invalid_input ? 0 : 1;
}
