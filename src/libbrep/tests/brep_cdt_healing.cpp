/*                    B R E P _ C D T _ H E A L I N G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "bu/log.h"
#include "bu/malloc.h"
#include "brep/cdt.h"
#include "cdt/heal.h"
#include "cdt/test_api.h"

namespace {

using edge_map = std::map<std::pair<int, int>, int>;

void
add_loop(ON_Brep &brep, ON_BrepFace &face, const ON_Plane &plane,
	const std::vector<int> &vertices, ON_BrepLoop::TYPE type, edge_map &edges)
{
    ON_BrepLoop &loop = brep.NewLoop(type, face);
    const auto *surface = ON_PlaneSurface::Cast(face.SurfaceOf());
    for (size_t i = 0; i < vertices.size(); ++i) {
	const int a = vertices[i];
	const int b = vertices[(i + 1) % vertices.size()];
	const std::pair<int, int> key = std::minmax(a, b);
	if (!edges.count(key)) {
	    const int curve = brep.AddEdgeCurve(new ON_LineCurve(
		brep.m_V[key.first].point, brep.m_V[key.second].point));
	    edges[key] = brep.NewEdge(brep.m_V[key.first], brep.m_V[key.second],
		curve, NULL, 0.0).m_edge_index;
	}
	ON_2dPoint uv_a, uv_b;
	plane.ClosestPointTo(brep.m_V[a].point, &uv_a.x, &uv_a.y);
	plane.ClosestPointTo(brep.m_V[b].point, &uv_b.x, &uv_b.y);
	for (int axis = 0; axis < 2; ++axis) {
	    uv_a[axis] = surface->Extents(axis).NormalizedParameterAt(uv_a[axis]);
	    uv_b[axis] = surface->Extents(axis).NormalizedParameterAt(uv_b[axis]);
	}
	const int curve = brep.AddTrimCurve(new ON_LineCurve(uv_a, uv_b));
	ON_BrepTrim &trim = brep.NewTrim(brep.m_E[edges[key]], a != key.first, loop, curve);
	trim.m_tolerance[0] = trim.m_tolerance[1] = 0.0;
    }
}

void
add_face(ON_Brep &brep, const std::vector<int> &outline,
	const std::vector<int> &hole, edge_map &edges)
{
    const ON_Plane plane(brep.m_V[outline[0]].point,
	brep.m_V[outline[1]].point, brep.m_V[outline[2]].point);
    ON_PlaneSurface *surface = new ON_PlaneSurface(plane);
    ON_BoundingBox bounds;
    for (int vi : outline) {
	ON_3dPoint uv(0.0, 0.0, 0.0);
	plane.ClosestPointTo(brep.m_V[vi].point, &uv.x, &uv.y);
	bounds.Set(uv, bounds.IsValid());
    }
    for (int axis = 0; axis < 2; ++axis) {
	surface->SetDomain(axis, 0.0, 1.0);
	surface->SetExtents(axis, ON_Interval(bounds.m_min[axis], bounds.m_max[axis]), false);
    }
    ON_BrepFace &face = brep.NewFace(brep.AddSurface(surface));
    add_loop(brep, face, plane, outline, ON_BrepLoop::outer, edges);
    if (!hole.empty())
	add_loop(brep, face, plane, hole, ON_BrepLoop::inner, edges);
}

std::unique_ptr<ON_Brep>
tube()
{
    std::unique_ptr<ON_Brep> brep(new ON_Brep());
    const ON_3dPoint vertices[] = {
	ON_3dPoint(0, 0, 0), ON_3dPoint(10, 0, 0), ON_3dPoint(10, 10, 0), ON_3dPoint(0, 10, 0),
	ON_3dPoint(0, 0, 10), ON_3dPoint(10, 0, 10), ON_3dPoint(10, 10, 10), ON_3dPoint(0, 10, 10),
	ON_3dPoint(3, 3, 0), ON_3dPoint(7, 3, 0), ON_3dPoint(7, 7, 0), ON_3dPoint(3, 7, 0),
	ON_3dPoint(3, 3, 10), ON_3dPoint(7, 3, 10), ON_3dPoint(7, 7, 10), ON_3dPoint(3, 7, 10)
    };
    for (const auto &point : vertices)
	brep->NewVertex(point, 0.0);
    edge_map edges;
    add_face(*brep, {4, 5, 6, 7}, {12, 15, 14, 13}, edges);
    add_face(*brep, {0, 3, 2, 1}, {8, 9, 10, 11}, edges);
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	add_face(*brep, {i, next, next + 4, i + 4}, {}, edges);
	add_face(*brep, {i + 8, i + 12, next + 12, next + 8}, {}, edges);
    }
    brep->SetTolerancesBoxesAndFlags();
    for (int ei = 0; ei < brep->m_E.Count(); ++ei)
	brep->m_E[ei].m_tolerance = 0.0;
    ON_TextLog log(stderr);
    if (!brep->IsValid(&log) || !brep->IsSolid())
	return nullptr;
    return brep;
}

struct provenance {
    int calls = 0;
    std::set<int> faces;
    std::set<int> edges;
};

void
capture(int, const int *faces, size_t face_count,
	const int *edges, size_t edge_count, void *data)
{
    auto &result = *static_cast<provenance *>(data);
    ++result.calls;
    if (face_count) result.faces.insert(faces, faces + face_count);
    if (edge_count) result.edges.insert(edges, edges + edge_count);
}

struct mesh {
    int *faces = NULL;
    int face_count = 0;
    fastf_t *vertices = NULL;
    int vertex_count = 0;
    int *face_normals = NULL;
    int face_normal_count = 0;
    fastf_t *normals = NULL;
    int normal_count = 0;

    ~mesh()
    {
	bu_free(faces, "healing test faces");
	bu_free(vertices, "healing test vertices");
	bu_free(face_normals, "healing test face normals");
	bu_free(normals, "healing test normals");
    }

    bool get(ON_Brep_CDT_State *state)
    {
	return ON_Brep_CDT_Mesh(&faces, &face_count, &vertices, &vertex_count,
	    &face_normals, &face_normal_count, &normals, &normal_count,
	    state, 0, NULL) == 0;
    }
};

bool
run_case(ON_Brep &source, bool accept, bool limited, bool enabled,
	int expected_outer, int expected_unused, const std::set<int> &expected_edges = {})
{
    const ON__UINT32 source_crc = source.DataCRC(0);
    std::unique_ptr<ON_Brep_CDT_State, decltype(&ON_Brep_CDT_Destroy)> state(
	ON_Brep_CDT_Create(&source, "topology healing regression"), ON_Brep_CDT_Destroy);
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_TOL;
    ON_Brep_CDT_Tol_Set(state.get(), &tolerance);
    if (ON_Brep_CDT_Tessellate(state.get(), 0, NULL) == 0)
	return false;
    struct brep_cdt_repair_settings settings = BREP_CDT_REPAIR_SETTINGS_INIT;
    settings.try_invalid_brep = enabled ? 1 : 0;
    settings.use_fast_face_fallback = 0;
    settings.mesh.require_manifold = 1;
    if (limited)
	settings.max_fast_points = 1;
    provenance identities;
    settings.provenance = capture;
    settings.provenance_data = &identities;
    struct brep_cdt_repair_report report = BREP_CDT_REPAIR_REPORT_INIT;
    const int result = ON_Brep_CDT_Repair(state.get(), &settings, &report);
    if (source.DataCRC(0) != source_crc || (result >= 0) != accept ||
	(report.healing.applied != 0) != accept)
	return false;
    mesh output;
    if (!accept)
	return !identities.calls && !output.get(state.get()) &&
	    (!limited || report.healing.limited);
    if (!report.mesh.solid || !report.mesh.manifold_accepted ||
	ON_Brep_CDT_Failed_Faces(NULL, 0, state.get()) != report.source_failed_faces ||
	report.healing.restored_outer_loops != expected_outer ||
	report.healing.removed_unused_edges != expected_unused ||
	identities.calls != 1 || !output.get(state.get()) || !output.face_count ||
	output.face_normal_count != output.face_count || !output.normal_count)
	return false;
    const double expected_volume = (10.0 * 10.0 - 4.0 * 4.0) * 10.0;
    const double volume = bg_trimesh_volume(output.faces, output.face_count,
	reinterpret_cast<const point_t *>(output.vertices), output.vertex_count);
    if (std::abs(volume - expected_volume) > expected_volume * ON_SQRT_EPSILON)
	return false;
    for (int edge : expected_edges)
	if (!identities.edges.count(edge)) return false;
    for (int edge : identities.edges)
	if (edge < 0 || edge >= source.m_E.Count()) return false;
    mesh repeated;
    if (!repeated.get(state.get()) || repeated.face_count != output.face_count ||
	repeated.vertex_count != output.vertex_count ||
	!std::equal(output.faces, output.faces + output.face_count * 3, repeated.faces) ||
	!std::equal(output.vertices, output.vertices + output.vertex_count * 3, repeated.vertices))
	return false;
    tolerance.rel /= 2.0;
    ON_Brep_CDT_Tol_Set(state.get(), &tolerance);
    mesh invalidated;
    return !invalidated.get(state.get());
}

bool
interpret(const ON_Brep &source, cdt_healing &result)
{
    const struct brep_cdt_repair_settings settings = BREP_CDT_REPAIR_SETTINGS_INIT;
    return cdt_heal_topology(source, BN_TOL_DIST, settings.max_fast_points,
	settings.max_fast_result_bytes, settings.max_fast_time_ms, result);
}

bool
reject_outline(const ON_Brep &source)
{
    const ON__UINT32 crc = source.DataCRC(0);
    cdt_healing result;
    interpret(source, result);
    return !result.outer_loops && !result.loop_roles && source.DataCRC(0) == crc;
}

bool
boundary_contracts(const ON_Brep &source)
{
    ON_Brep reversed(source);
    for (int li = 0; li < reversed.m_F[0].LoopCount(); ++li) {
	ON_BrepLoop &loop = *reversed.m_F[0].Loop(li);
	reversed.FlipLoop(loop);
	loop.m_type = ON_BrepLoop::inner;
    }
    const ON__UINT32 crc = reversed.DataCRC(0);
    cdt_healing normalized;
    if (!interpret(reversed, normalized) || normalized.loop_roles != 2 ||
	!normalized.brep->IsValid() || !normalized.brep->IsSolid() ||
	reversed.DataCRC(0) != crc || !run_case(reversed, true, false, true, 0, 0))
	return false;

    ON_Brep missing(source);
    const int edge = missing.m_F[0].OuterLoop()->Trim(0)->m_ei;
    missing.DeleteLoop(*missing.m_F[0].OuterLoop(), false);
    missing.Compact();

    ON_Brep displaced(missing);
    ON_Xform move = ON_Xform::TranslationTransformation(0.0, 0.0, 1.0);
    displaced.m_C3[displaced.m_E[edge].m_c3i]->Transform(move);
    if (!reject_outline(displaced))
	return false;

    ON_Brep ambiguous(missing);
    std::vector<int> ring;
    for (const ON_3dPoint &point : {ON_3dPoint(-5, -5, 10), ON_3dPoint(15, -5, 10),
	ON_3dPoint(15, 15, 10), ON_3dPoint(-5, 15, 10)})
	ring.push_back(ambiguous.NewVertex(point, 0.0).m_vertex_index);
    edge_map edges;
    const ON_Plane &plane = ON_PlaneSurface::Cast(ambiguous.m_F[1].SurfaceOf())->m_plane;
    /* Conflicting imported references offer two enclosing cycles.  Neither
     * traversal order nor another loop's defective pcurves may choose one. */
    add_loop(ambiguous, ambiguous.m_F[1], plane, ring, ON_BrepLoop::inner, edges);
    if (!reject_outline(ambiguous))
	return false;

    ON_Brep broken(missing);
    broken.m_T[0].m_li = broken.m_L.Count();
    cdt_healing invalid;
    if (interpret(broken, invalid) || invalid.brep)
	return false;

    cdt_healing bounded;
    return !cdt_heal_topology(missing, BN_TOL_DIST, 1, 1, 1, bounded) &&
	bounded.limited && !bounded.brep;
}

bool
cap_case(ON_Brep &source, double ceiling, bool accept, int expected_caps = 1)
{
    const ON__UINT32 crc = source.DataCRC(0);
    std::unique_ptr<ON_Brep_CDT_State, decltype(&ON_Brep_CDT_Destroy)> state(
	ON_Brep_CDT_Create(&source, "planar cap regression"), ON_Brep_CDT_Destroy);
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_TOL;
    ON_Brep_CDT_Tol_Set(state.get(), &tolerance);
    ON_Brep_CDT_Tessellate(state.get(), 0, NULL);
    struct brep_cdt_repair_settings settings = BREP_CDT_REPAIR_SETTINGS_INIT;
    settings.try_invalid_brep = 1;
    settings.use_fast_face_fallback = 0;
    settings.use_full_fast_fallback_if_needed = 1;
    settings.mesh.fill_holes = 0;
    settings.mesh.allow_self_intersections = 1;
    settings.max_planar_cap_area_percent = ceiling;
    provenance identities;
    settings.provenance = capture;
    settings.provenance_data = &identities;
    struct brep_cdt_repair_report report = BREP_CDT_REPAIR_REPORT_INIT;
    const int result = ON_Brep_CDT_Repair(state.get(), &settings, &report);
    struct brep_cdt_diagnostic diagnostic = {};
    if (ON_Brep_CDT_Diagnostic(&diagnostic, state.get()) != 0 ||
	std::string(diagnostic.message).find("invalid mesh repair limits") != std::string::npos)
	return false;
    mesh output;
    if (source.DataCRC(0) != crc || (result >= 0) != accept ||
	(report.healing.applied != 0) != accept)
	return false;
    if (!accept) {
	if (report.healing.cap_area_percent > ceiling &&
	    (!report.mesh.manifold_accepted || !report.mesh.solid ||
	    !(report.mesh.output_area > report.healing.cap_area_bound) ||
	    report.source_diagnostic.result != BREP_CDT_RESULT_INVALID_BREP))
	    return false;
	return !identities.calls && !output.get(state.get());
    }
    if (!report.mesh.solid || !report.mesh.manifold_accepted ||
	!report.mesh.self_intersections_allowed ||
	report.healing.capped_loops != expected_caps ||
	report.healing.cap_area_bound < expected_caps ||
	report.healing.cap_area_percent < 1.0 ||
	report.healing.cap_area_percent > ceiling || identities.calls != 1 ||
	identities.faces.empty() || identities.edges.empty() ||
	!output.get(state.get()) || output.face_normal_count != output.face_count)
	return false;
    for (int face : identities.faces)
	if (face < 0 || face >= source.m_F.Count()) return false;
    for (int edge : identities.edges)
	if (edge < 0 || edge >= source.m_E.Count()) return false;
    const double expected_volume = 20.0;
    const double volume = bg_trimesh_volume(output.faces, output.face_count,
	reinterpret_cast<const point_t *>(output.vertices), output.vertex_count);
    return std::fabs(volume - expected_volume) <= expected_volume * ON_SQRT_EPSILON;
}

bool
cap_contracts()
{
    /* The missing end has area 1 against 81 units of remaining surface.
     * This puts the same geometry on opposite sides of the area ceiling. */
    const ON_3dPoint corners[8] = {
	ON_3dPoint(0, 0, 0), ON_3dPoint(1, 0, 0),
	ON_3dPoint(1, 1, 0), ON_3dPoint(0, 1, 0),
	ON_3dPoint(0, 0, 20), ON_3dPoint(1, 0, 20),
	ON_3dPoint(1, 1, 20), ON_3dPoint(0, 1, 20)
    };
    std::unique_ptr<ON_Brep> box(ON_BrepBox(corners));
    if (!box)
	return false;
    int top = -1;
    for (int fi = 0; fi < box->m_F.Count(); ++fi)
	if (box->m_F[fi].BoundingBox().m_min.z > 19.0) top = fi;
    if (top < 0)
	return false;
    std::unique_ptr<ON_Brep> sheet(box->DuplicateFace(top, false));
    box->DeleteFace(box->m_F[top], false);
    box->Compact();
    box->SetTrimTypeFlags(false);
    if (!box->IsValid() || box->IsSolid() ||
	!cap_case(*box, 0.0, false) || !cap_case(*box, 1.0, false) ||
	!cap_case(*box, 2.0, true) || !cap_case(*sheet, 200.0, false))
	return false;

    ON_Brep two_boundaries(*box);
    for (int fi = 0; fi < two_boundaries.m_F.Count(); ++fi) {
	if (two_boundaries.m_F[fi].BoundingBox().m_max.z < 1.0) {
	    two_boundaries.DeleteFace(two_boundaries.m_F[fi], false);
	    break;
	}
    }
    two_boundaries.Compact();
    two_boundaries.SetTrimTypeFlags(false);
    if (!cap_case(two_boundaries, 2.0, false) ||
	!cap_case(two_boundaries, 3.0, true, 2))
	return false;

    /* Coplanar nested cycles describe an annular opening, not two disks. */
    std::unique_ptr<ON_Brep> nested = tube();
    if (!nested)
	return false;
    nested->DeleteFace(nested->m_F[0], false);
    nested->Compact();
    nested->SetTrimTypeFlags(false);
    cdt_healing rejected;
    const struct brep_cdt_repair_settings limits = BREP_CDT_REPAIR_SETTINGS_INIT;
    cdt_heal_topology(*nested, BN_TOL_DIST, limits.max_fast_points,
	limits.max_fast_result_bytes, limits.max_fast_time_ms, rejected, true);
    if (rejected.capped_loops)
	return false;

    ON_Brep displaced(*box);
    for (int ei = 0; ei < displaced.m_E.Count(); ++ei) {
	if (displaced.m_E[ei].TrimCount() != 1)
	    continue;
	displaced.m_C3[displaced.m_E[ei].m_c3i]->Transform(
	    ON_Xform::TranslationTransformation(0.0, 0.0, 1.0));
	break;
    }
    return cap_case(displaced, 200.0, false);
}

}

int
main()
{
    if (cdt_test_planar_cap_hulls()) {
	bu_log("planar cap curve hull contracts failed\n");
	return 1;
    }
    if (!cap_contracts()) {
	bu_log("planar cap contracts failed\n");
	return 1;
    }
    std::unique_ptr<ON_Brep> source = tube();
    if (!source) {
	bu_log("healing fixture construction failed\n");
	return 1;
    }
    if (!boundary_contracts(*source)) {
	bu_log("boundary interpretation contracts failed\n");
	return 1;
    }
    ON_Brep wired;
    wired.NewVertex(ON_3dPoint(4, 4, 4), 0.0);
    wired.NewVertex(ON_3dPoint(6, 6, 6), 0.0);
    const int curve = wired.AddEdgeCurve(new ON_LineCurve(wired.m_V[0].point, wired.m_V[1].point));
    wired.NewEdge(wired.m_V[0], wired.m_V[1], curve, NULL, 0.0);
    wired.Append(*source);
    if (!run_case(wired, true, false, true, 0, 1, {0})) {
	bu_log("unused-edge healing failed\n");
	return 1;
    }
    ON_Brep missing(wired);
    ON_BrepLoop *outline = missing.m_F[0].OuterLoop();
    std::set<int> outline_edges = {0};
    for (int ti = 0; ti < outline->TrimCount(); ++ti)
	outline_edges.insert(outline->Trim(ti)->m_ei);
    missing.DeleteLoop(*outline, false);
    missing.Compact();
    if (!run_case(missing, false, false, false, 0, 0) ||
	!run_case(missing, false, true, true, 0, 0) ||
	!run_case(missing, true, false, true, 1, 1, outline_edges)) {
	bu_log("missing-outline healing or transaction checks failed\n");
	return 1;
    }
    ON_Brep disconnected(missing);
    disconnected.DeleteLoop(*disconnected.m_F[1].OuterLoop(), false);
    disconnected.Compact();
    if (!run_case(disconnected, true, false, true, 2, 1)) {
	bu_log("disconnected-shell outline healing failed\n");
	return 1;
    }
    ON_Brep oriented(*source);
    oriented.FlipFace(oriented.m_F[0]);
    if (!run_case(oriented, true, false, true, 0, 0)) {
	bu_log("face-orientation healing failed\n");
	return 1;
    }
    std::unique_ptr<ON_Brep> sheet(source->DuplicateFace(0, false));
    sheet->DeleteLoop(*sheet->m_F[0].OuterLoop(), false);
    sheet->Compact();
    sheet->Append(*source);
    if (!run_case(*sheet, false, false, true, 0, 0)) {
	bu_log("unrelated boundary was used to fill a hole-only sheet\n");
	return 1;
    }
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
