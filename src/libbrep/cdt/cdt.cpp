/*                        C D T . C P P
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <limits>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "bu/str.h"
#include "cdt.h"
#include "assembly.h"
#include "heal.h"
#include "surface.h"

#define MAX_TRIANGULATION_ATTEMPTS 5
#define MAX_CHART_REFINEMENT_STAGNANT_ATTEMPTS 4
#define MAX_SHARED_EDGE_REFINEMENT_DISTANCE_RATIO 0.05

static constexpr double FACE_RESAMPLING_SCALE = 0.5;

static bool
cdt_tolerance_valid(const struct bg_tess_tol &tol)
{
    const fastf_t values[] = {
	tol.abs, tol.rel, tol.norm, tol.absmax, tol.absmin,
	tol.relmax, tol.relmin, tol.rel_lmax, tol.rel_lmin
    };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
	if (!std::isfinite(values[i]) || values[i] < -1.0)
	    return false;
    }
    if (tol.norm > ON_PI)
	return false;
    if (tol.absmax > 0.0 && tol.absmin > 0.0 &&
	    tol.absmax < tol.absmin)
	return false;
    if (tol.relmax > 0.0 && tol.relmin > 0.0 &&
	    tol.relmax < tol.relmin)
	return false;
    if (tol.rel_lmax > 0.0 && tol.rel_lmin > 0.0 &&
	    tol.rel_lmax < tol.rel_lmin)
	return false;
    return true;
}

/* cpolyedge_t containers historically used the default pointer comparator.
 * That is adequate for membership, but it must not determine the order of
 * geometry-changing operations: allocator layout can then change the order in
 * which singular trims are subdivided and, consequently, the final surface
 * sampling.  Every singular segment has an authoritative STEP trim index,
 * parameter interval, and pair of face-local topology vertices, which together
 * provide a stable processing order. */
static bool
singular_edge_process_less(const cpolyedge_t *a, const cpolyedge_t *b)
{
    if (a == b)
	return false;
    if (!a)
	return true;
    if (!b)
	return false;
    if (a->trim_ind != b->trim_ind)
	return a->trim_ind < b->trim_ind;
    if (a->trim_start < b->trim_start)
	return true;
    if (b->trim_start < a->trim_start)
	return false;
    if (a->trim_end < b->trim_end)
	return true;
    if (b->trim_end < a->trim_end)
	return false;
    if (a->v2d[0] != b->v2d[0])
	return a->v2d[0] < b->v2d[0];
    return a->v2d[1] < b->v2d[1];
}

static ON_3dVector
bseg_tangent(struct ON_Brep_CDT_State *s_cdt, bedge_seg_t *bseg, double eparam, double t1param, double t2param)
{
    ON_3dPoint tmp;
    ON_3dVector tangent = ON_3dVector::UnsetVector;
    if (!bseg->nc->EvTangent(eparam, tmp, tangent)) {
	if (t1param < DBL_MAX && t2param < DBL_MAX) {
	    // If the edge curve failed, average tangents from trims
	    ON_BrepEdge& edge = s_cdt->brep->m_E[bseg->edge_ind];
	    ON_BrepTrim *trim1 = edge.Trim(0);
	    ON_BrepTrim *trim2 = edge.Trim(1);
	    int evals = 0;
	    ON_3dPoint tmp1, tmp2;
	    ON_3dVector trim1_tangent, trim2_tangent;
	    evals += (trim1->EvTangent(t1param, tmp1, trim1_tangent)) ? 1 : 0;
	    evals += (trim2->EvTangent(t2param, tmp2, trim2_tangent)) ? 1 : 0;
	    if (evals == 2) {
		tangent = (trim1_tangent + trim2_tangent) / 2;
	    } else {
		tangent = ON_3dVector::UnsetVector;
	    }
	}
    }

    return tangent;
}


static bool
check_chart_boundary(struct ON_Brep_CDT_State *s_cdt, const cdt_mesh_t *mesh)
{
    if (mesh->chart_boundary_complete())
	return true;
    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_CERTIFICATION_FAILED,
	BREP_CDT_STAGE_ADAPTIVE_REFINEMENT, mesh->f_id, 0, 1,
	"3-D seam refinement left a boundary vertex unused");
    return false;
}

static bool
refine_triangulation(struct ON_Brep_CDT_State *s_cdt, cdt_mesh_t *fmesh,
	int cnt, int rebuild, bool allow_general_boundary_cleanup = false)
{
    if (!s_cdt || !fmesh) return false;

    const auto time_limit_reached = [&]() {
	if (!s_cdt->face_deadline || bu_gettime() < s_cdt->face_deadline)
	    return false;
	const std::string message = "rigorous face triangulation exceeded its " +
	    std::to_string(s_cdt->max_face_time_ms) +
	    " ms wall-clock limit during adaptive refinement";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REFINEMENT_LIMIT,
	    BREP_CDT_STAGE_ADAPTIVE_REFINEMENT, fmesh->f_id, 0, 1,
	    message.c_str());
	return true;
    };
    if (time_limit_reached())
	return false;

    if (cnt > MAX_TRIANGULATION_ATTEMPTS) {
	std::cerr << "Error: even after " << MAX_TRIANGULATION_ATTEMPTS << " iterations could not successfully refine triangulate face " << fmesh->f_id << " for solidity criteria\n";
	return false;
    }

    // If a previous pass has made changes in which points are active in the
    // surface set, we need to rebuild the whole triangulation.

    if (rebuild && !fmesh->cdt(allow_general_boundary_cleanup)) {
	bu_log("Fatal failure attempting full retriangulation of face\n");
	return false;
    }

    // A chart triangulation which already satisfies the face invariants does
    // not need the legacy UV repair heuristics.  In particular, applying a
    // best-fit-plane reparameterization to a valid singular chart can
    // reintroduce the degeneracy the chart was constructed to remove.

    const bool topology_chart = cdt_face_uses_topology_chart(
	s_cdt->brep->m_F[fmesh->f_id]);
    const auto valid_or_repair_normals = [&]() {
	if (fmesh->valid(0, false))
	    return fmesh->self_intersections(NULL, 1) == 0;
	return fmesh->repair_incorrect_normal_edges() && fmesh->valid(0);
    };
    if (valid_or_repair_normals())
	return true;
    if (fmesh->repair_toleranced_nonmanifold_edges())
	return true;

    /* Periodic seam copies can make distinct chart cells share the same
     * three model-space vertices.  Separate those cells before geometric
     * fold and intersection refinement works on the stitched mesh. */
    size_t collapsed_chart_points = 0;
    for (int attempt = 0; attempt < CDT_MAX_CHART_REFINEMENT_ATTEMPTS;
	    ++attempt) {
	if (time_limit_reached())
	    return false;
	const size_t remaining = CDT_MAX_CHART_REFINEMENT_POINTS -
	    collapsed_chart_points;
	if (!remaining)
	    break;
	const size_t inserted =
	    fmesh->refine_collapsed_chart_triangles(remaining);
	if (!inserted)
	    break;
	collapsed_chart_points += inserted;
	if (!fmesh->cdt(allow_general_boundary_cleanup)) {
	    bu_log("Face %d: collapsed chart cell retriangulation failed\n",
		fmesh->f_id);
	    return false;
	}
	if (time_limit_reached())
	    return false;
	if (valid_or_repair_normals()) {
	    bu_log("Face %d: separated collapsed periodic chart cells after "
		"%zu inserted points\n", fmesh->f_id,
		collapsed_chart_points);
	    return true;
	}
	if (fmesh->repair_toleranced_nonmanifold_edges())
	    return true;
    }

    if (!check_chart_boundary(s_cdt, fmesh))
	return false;

    const size_t initial_folds = fmesh->incorrect_normal_count();
    const size_t initial_intersections = fmesh->self_intersections(NULL,
	CDT_MAX_CHART_REFINEMENT_POINTS);
    const size_t initial_defects = initial_folds + initial_intersections;
    size_t refinement_points = 0;
    int refinement_attempts = 0;
    bool refinement_stalled = false;
    bool refinement_no_progress = false;
    size_t best_defects = initial_defects;
    int stagnant_attempts = 0;
    for (int attempt = 0; attempt < CDT_MAX_CHART_REFINEMENT_ATTEMPTS;
	    ++attempt) {
	if (time_limit_reached())
	    return false;
	const size_t remaining = CDT_MAX_CHART_REFINEMENT_POINTS -
	    refinement_points;
	if (!remaining)
	    break;
	refinement_attempts++;
	size_t inserted = fmesh->refine_self_intersections(remaining);
	if (inserted < remaining)
	    inserted += fmesh->refine_incorrect_normals(remaining - inserted);
	if (!inserted) {
	    refinement_stalled = true;
	    break;
	}
	refinement_points += inserted;
	if (!fmesh->cdt(allow_general_boundary_cleanup)) {
	    bu_log("Face %d: chart refinement retriangulation failed\n",
		fmesh->f_id);
	    return false;
	}
	if (time_limit_reached())
	    return false;
	if (valid_or_repair_normals()) {
	    bu_log("Face %d: chart refinement certified after %zu "
		"inserted points\n", fmesh->f_id, refinement_points);
	    return true;
	}
	if (fmesh->repair_toleranced_nonmanifold_edges())
	    return true;
	const size_t current_folds = fmesh->incorrect_normal_count();
	/* Progress only requires knowing whether the defect count improved.
	 * Once enough intersections have been found to equal the previous best,
	 * continuing the complete pair scan cannot change that decision. */
	size_t current_intersections = 0;
	size_t current_defects = best_defects;
	if (current_folds < best_defects) {
	    const size_t improvement_limit = best_defects - current_folds;
	    current_intersections = fmesh->self_intersections(NULL,
		improvement_limit);
	    if (current_intersections < improvement_limit)
		current_defects = current_folds + current_intersections;
	}
	if (current_defects < best_defects) {
	    best_defects = current_defects;
	    stagnant_attempts = 0;
	} else {
	    stagnant_attempts++;
	    if (stagnant_attempts >=
		    MAX_CHART_REFINEMENT_STAGNANT_ATTEMPTS) {
		refinement_no_progress = true;
		break;
	    }
	}
    }
    if (fmesh->repair_toleranced_nonmanifold_edges())
	return true;
    if (refinement_points) {
	const size_t remaining_folds = fmesh->incorrect_normal_count();
	const size_t remaining_intersections = fmesh->self_intersections(NULL,
	    CDT_MAX_CHART_REFINEMENT_POINTS);
	const std::string remaining_summary = std::to_string(remaining_folds) +
	    " folded triangles and " +
	    std::to_string(remaining_intersections) +
	    " intersecting triangle pairs";
	const std::string initial_summary = std::to_string(initial_folds) +
	    " folded triangles and " +
	    std::to_string(initial_intersections) +
	    " intersecting triangle pairs";
	const std::string message = refinement_no_progress ?
	    "adaptive chart refinement made no progress after " +
		std::to_string(refinement_attempts) + " rounds and " +
		std::to_string(refinement_points) + " points; best " +
		std::to_string(best_defects) +
		" geometric defects, remaining " + remaining_summary :
	    refinement_stalled ?
	    "adaptive chart refinement stalled after " +
		std::to_string(refinement_points) + " points with " +
		remaining_summary + " (initially " + initial_summary + ")" :
	    "adaptive chart refinement reached its limit after " +
		std::to_string(refinement_attempts) + " rounds and " +
		std::to_string(refinement_points) + " points with " +
		remaining_summary + " (initially " + initial_summary + ")";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REFINEMENT_LIMIT,
	    BREP_CDT_STAGE_ADAPTIVE_REFINEMENT, fmesh->f_id, 0, 1,
	    message.c_str());
	return false;
    }

    if (topology_chart) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_CHART_FAILED,
	    BREP_CDT_STAGE_CHART_CONSTRUCTION, fmesh->f_id, 0, 1,
	    "topology chart did not preserve the face validity invariants");
	return false;
    }

    // Now, the hard part - create local subsets, remesh them, and replace the original
    // triangles with the new ones.
    if (!fmesh->repair()) {
	bu_log("Face %d: repair FAILED!\n", fmesh->f_id);
	return false;
    }

    return true;
}

static bool
loop_edges(cdt_mesh_t *fmesh, cpolygon_t *loop)
{
    if (!fmesh || !loop)
	return false;
    cpolyedge_t *edge = loop->first_edge();
    if (!edge)
	return false;
    cpolyedge_t *first = edge;
    size_t edge_count = 0;
    const auto mesh_point = [&](long polygon_point) {
	const auto native = loop->p2o.find(polygon_point);
	if (native == loop->p2o.end())
	    return -1L;
	const auto point_3d = fmesh->p2d3d.find(native->second);
	if (point_3d == fmesh->p2d3d.end() || point_3d->second < 0 ||
		(size_t)point_3d->second >= fmesh->pnts.size())
	    return -1L;
	const auto canonical = fmesh->p2ind.find(
	    fmesh->pnts[(size_t)point_3d->second]);
	return canonical == fmesh->p2ind.end() ? -1L : canonical->second;
    };

    do {
	const long start = mesh_point(edge->v2d[0]);
	const long end = mesh_point(edge->v2d[1]);
	if (start < 0 || end < 0) {
	    bu_log("face %d has an incomplete boundary point mapping\n",
		fmesh->f_id);
	    return false;
	}
	fmesh->ep.insert(start);
	fmesh->ep.insert(end);
	if (start != end) {
	    const uedge_t boundary_edge(start, end);
	    fmesh->brep_edges.insert(boundary_edge);
	    fmesh->ue2b_map[boundary_edge] = edge->eseg;
	}
	edge = edge->next;
	edge_count++;
	if (!edge || edge_count > loop->poly.size()) {
	    std::cerr << "infinite loop when reading loop edges\n";
	    return false;
	}
    } while (edge != first);
    return true;
}

bool
cdt_rebuild_repair_face(struct ON_Brep_CDT_State *s_cdt, cdt_mesh_t *mesh)
{
    mesh->brep_edges.clear();
    mesh->chart_boundary_edges.clear();
    mesh->ue2b_map.clear();
    if (!mesh->cdt(true))
	return false;
    bool loops_mapped = loop_edges(mesh, &mesh->outer_loop);
    for (const auto &inner : mesh->inner_loops)
	loops_mapped = loop_edges(mesh, inner.second) && loops_mapped;
    if (!loops_mapped)
	return false;
    mesh->boundary_edges_update();
    return refine_triangulation(s_cdt, mesh, 0, 0, true) &&
	!mesh->geometric_degenerate_count();
}

static bool
do_triangulation(struct ON_Brep_CDT_State *s_cdt, int fi,
	size_t *resampling_budget = NULL, double relative_scale = 1.0)
{
    ON_BrepFace &face = s_cdt->brep->m_F[fi];
    if (s_cdt->max_face_time_ms > 0) {
	const int64_t now = bu_gettime();
	const int64_t max_delta =
	    std::numeric_limits<int64_t>::max() - now;
	const int64_t requested = (int64_t)s_cdt->max_face_time_ms;
	const int64_t delta = requested > max_delta / 1000 ?
	    max_delta : requested * 1000;
	s_cdt->face_deadline = now + delta;
    } else {
	s_cdt->face_deadline = 0;
    }

    // Document the min and max segment lengths - used to guide surface sampling
    int loop_cnt = face.LoopCount();
    double min_edge_seg_len = DBL_MAX;
    double max_edge_seg_len = 0;
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	for (int lti = 0; lti < loop->TrimCount(); lti++) {
	    ON_BrepTrim *trim = loop->Trim(lti);
	    ON_BrepEdge *edge = trim->Edge();
	    if (!edge) continue;
	    const ON_Curve* crv = edge->EdgeCurveOf();
	    if (!crv) continue;
	    std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge->m_edge_index];
	    if (!epsegs.size()) continue;
	    std::set<bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		bedge_seg_t *b = *e_it;
		double seg_dist = b->e_start->DistanceTo(*b->e_end);
		min_edge_seg_len = (min_edge_seg_len > seg_dist) ? seg_dist : min_edge_seg_len;
		max_edge_seg_len = (max_edge_seg_len < seg_dist) ? seg_dist : max_edge_seg_len;
	    }
	}
    }
    (*s_cdt->min_edge_seg_len)[face.m_face_index] = min_edge_seg_len;
    (*s_cdt->max_edge_seg_len)[face.m_face_index] = max_edge_seg_len;

    // Sample the surface, independent of the trimming curves, to get points that
    // will tie the mesh to the interior surface.
    cdt_mesh_t *fmesh = &s_cdt->fmeshes[fi];
    const size_t initial_points = fmesh->m_interior_pnts.size();
    const size_t additional_points = resampling_budget ? *resampling_budget : 0;
    if (!GetInteriorPoints(s_cdt, fi, additional_points, relative_scale)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REFINEMENT_LIMIT,
	    BREP_CDT_STAGE_ADAPTIVE_REFINEMENT, face.m_face_index, 0, 1,
	    "initial surface subdivision exceeded the face point limit");
	return false;
    }

    if (resampling_budget) {
	const size_t added = fmesh->m_interior_pnts.size() - initial_points;
	if (!added)
	    return true;
	*resampling_budget -= added;
    }

    fmesh->brep = s_cdt->brep;
    fmesh->p_cdt = (void *)s_cdt;
    fmesh->name = s_cdt->name;
    fmesh->f_id = face.m_face_index;
    fmesh->m_bRev = face.m_bRev;

    // cdt() records the final topology-chart boundary.  Clear stale native
    // loop data first, then add the original segment associations below for
    // edges that still have a one-to-one B-Rep representation.
    fmesh->brep_edges.clear();
    fmesh->chart_boundary_edges.clear();
    fmesh->ue2b_map.clear();

    // Mark singular vertices before orienting the initial triangles.  Their
    // averaged vertex normals are not a reliable face-local orientation
    // signal at a pole.
    fmesh->sv.clear();
    for (size_t i = 0; i < fmesh->pnts.size(); i++) {
	ON_3dPoint *p3d = fmesh->pnts[i];
	if (s_cdt->singular_vert_to_norms->find(p3d) != s_cdt->singular_vert_to_norms->end()) {
	    fmesh->sv.insert(fmesh->p2ind[p3d]);
	}
    }

    if (!fmesh->cdt()) {
	bu_log("Face %d: initial CDT (fmesh->cdt) FAILED\n", face.m_face_index);
	return false;
    }

    // List native loop edges in addition to the chart boundary.
    bool loops_mapped = loop_edges(fmesh, &fmesh->outer_loop);
    std::map<int, cpolygon_t*>::iterator i_it;
    for (i_it = fmesh->inner_loops.begin(); i_it != fmesh->inner_loops.end(); i_it++) {
	loops_mapped = loop_edges(fmesh, i_it->second) && loops_mapped;
    }
    if (!loops_mapped) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_CERTIFICATION_FAILED,
	    BREP_CDT_STAGE_FACE_TRIANGULATION, face.m_face_index, 0, 1,
	    "native loop boundary did not map to mesh vertices");
	return false;
    }
    fmesh->boundary_edges_update();

    /* The libbg triangulation is not guaranteed to have all the properties
     * we want out of the box - trigger a series of checks. */
    if (!refine_triangulation(s_cdt, fmesh, 0, 0))
	return false;
    if (!check_chart_boundary(s_cdt, fmesh))
	return false;
    const size_t geometric_degenerates =
	fmesh->geometric_degenerate_count();
    if (geometric_degenerates) {
	const std::string message = "face mesh contains " +
	    std::to_string(geometric_degenerates) +
	    " scale-aware zero-area triangles";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_CERTIFICATION_FAILED,
	    BREP_CDT_STAGE_FACE_TRIANGULATION, face.m_face_index, 0, 1,
	    message.c_str());
	return false;
    }
    /* A closed indexed mesh is insufficient evidence when evaluation has
     * escaped the source surface.  Quarantine that face before its triangles
     * can become the trusted reference for a later repair. */
    ON_BoundingBox source_bounds;
    const ON_Surface *source_surface =
	s_cdt->rebuilt_support_faces.count(face.m_face_index) ?
	face.SurfaceOf() : s_cdt->orig_brep->m_F[face.m_face_index].SurfaceOf();
    if (!source_surface || !source_surface->GetBoundingBox(source_bounds) ||
	!source_bounds.IsValid()) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_GEOMETRIC_FAILED,
	    BREP_CDT_STAGE_GEOMETRIC_VALIDATION, face.m_face_index, 0, 1,
	    "source surface has no finite extent bound");
	return false;
    }
    const double bounds_tolerance = std::isfinite(s_cdt->absmax) ?
	std::max((double)BN_TOL_DIST, (double)s_cdt->absmax) : BN_TOL_DIST;
    RTree<size_t, double, 3>::Iterator triangle;
    for (fmesh->tris_tree.GetFirst(triangle); !triangle.IsNull(); ++triangle) {
	const triangle_t &active = fmesh->tris_vect[*triangle];
	for (int corner = 0; corner < 3; ++corner) {
	    const ON_3dPoint &point = *fmesh->pnts[(size_t)active.v[corner]];
	    if (!point.IsValid() || point.DistanceTo(
		    source_bounds.ClosestPoint(point)) > bounds_tolerance) {
		cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_GEOMETRIC_FAILED,
		    BREP_CDT_STAGE_GEOMETRIC_VALIDATION, face.m_face_index, 0, 1,
		    "face mesh escapes the original support surface bounds");
		return false;
	    }
	}
    }
    return true;
}

static void
apply_derived_point_welds(struct ON_Brep_CDT_State *s_cdt,
	const std::map<ON_3dPoint *, ON_3dPoint *> &welds)
{
    if (!s_cdt || welds.empty())
	return;
    const auto canonical = [&](ON_3dPoint *point) {
	ON_3dPoint *current = point;
	std::set<ON_3dPoint *> visited;
	while (current && visited.insert(current).second) {
	    const auto next = welds.find(current);
	    if (next == welds.end() || next->second == current)
		break;
	    current = next->second;
	}
	return current;
    };

    for (auto &vertex : *s_cdt->vert_pnts)
	vertex.second = canonical(vertex.second);
    std::set<ON_3dPoint *> canonical_edge_points;
    for (ON_3dPoint *point : *s_cdt->edge_pnts)
	canonical_edge_points.insert(canonical(point));
    s_cdt->edge_pnts->swap(canonical_edge_points);
    for (auto &face : s_cdt->fmeshes) {
	cdt_mesh_t &mesh = face.second;
	for (ON_3dPoint *&point : mesh.pnts)
	    point = canonical(point);
	mesh.p2ind.clear();
	for (size_t i = 0; i < mesh.pnts.size(); ++i)
	    mesh.p2ind[mesh.pnts[i]] = (long)i;
    }
    for (auto &edge_segments : s_cdt->e2polysegs) {
	for (bedge_seg_t *segment : edge_segments.second) {
	    if (!segment)
		continue;
	    segment->e_start = canonical(segment->e_start);
	    segment->e_end = canonical(segment->e_end);
	    segment->e_root_start = canonical(segment->e_root_start);
	    segment->e_root_end = canonical(segment->e_root_end);
	}
    }
    for (auto &face_poles : s_cdt->strim_pnts) {
	for (auto &pole : face_poles.second)
	    pole.second = canonical(pole.second);
    }
    std::map<ON_3dPoint *, ON_3dPoint *> canonical_singular_norms;
    for (const auto &normal : *s_cdt->singular_vert_to_norms) {
	ON_3dPoint *point = canonical(normal.first);
	if (point && canonical_singular_norms.find(point) ==
		canonical_singular_norms.end())
	    canonical_singular_norms[point] = normal.second;
    }
    s_cdt->singular_vert_to_norms->swap(canonical_singular_norms);
}

/* Weld distinct topology vertices only when the same trimmed face places
 * them at one singular side of its surface.  This repairs imported sphere
 * caps which visit a geometric pole with more than one B-Rep vertex identity.
 * Proximity alone is not sufficient: candidates must share both a face and
 * a specific analytic surface singularity, and each component remains inside
 * the caller's scale-limited modeling tolerance. */
static size_t
collapse_coincident_surface_poles(struct ON_Brep_CDT_State *s_cdt)
{
    if (!s_cdt || !s_cdt->brep || !s_cdt->vert_pnts ||
	    !std::isfinite(s_cdt->absmin) || s_cdt->absmin <= 0.0)
	return 0;
    const double tolerance = std::min((double)BN_TOL_DIST,
	(double)s_cdt->absmin);
    if (!(tolerance > 0.0))
	return 0;

    std::map<ON_3dPoint *, ON_3dPoint *> welds;
    size_t welded_vertices = 0;
    for (int face_index = 0; face_index < s_cdt->brep->m_F.Count();
	    ++face_index) {
	const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	const ON_Surface *surface = face.SurfaceOf();
	if (!surface)
	    continue;
	for (int side = 0; side < 4; ++side) {
	    if (!surface->IsSingular(side))
		continue;
	    const int open_direction = (side == 0 || side == 2) ? 1 : 0;
	    const int closed_direction = 1 - open_direction;
	    ON_2dPoint pole_uv;
	    pole_uv[open_direction] = (side == 0 || side == 3) ?
		surface->Domain(open_direction).Min() :
		surface->Domain(open_direction).Max();
	    pole_uv[closed_direction] =
		surface->Domain(closed_direction).Mid();
	    const ON_3dPoint pole = surface->PointAt(pole_uv.x, pole_uv.y);
	    if (!pole.IsValid())
		continue;

	    std::set<int> face_vertices;
	    std::map<int, std::vector<const ON_BrepTrim *>> vertex_trims;
	    for (int loop_index = 0; loop_index < face.LoopCount();
		    ++loop_index) {
		const ON_BrepLoop *loop = face.Loop(loop_index);
		if (!loop)
		    continue;
		for (int trim_index = 0; trim_index < loop->TrimCount();
			++trim_index) {
		    const ON_BrepTrim *trim = loop->Trim(trim_index);
		    if (!trim)
			continue;
		    for (int endpoint = 0; endpoint < 2; ++endpoint) {
			const int vertex_index = trim->m_vi[endpoint];
			const auto point = s_cdt->vert_pnts->find(vertex_index);
			if (point != s_cdt->vert_pnts->end() && point->second &&
				point->second->DistanceTo(pole) <= tolerance) {
			    face_vertices.insert(vertex_index);
			    if (trim->m_ei >= 0)
				vertex_trims[vertex_index].push_back(trim);
			}
		    }
		}
	    }
	    if (face_vertices.size() < 2)
		continue;

	    struct pole_component {
		ON_BoundingBox bounds;
		std::vector<int> vertices;
	    };
	    std::vector<pole_component> components;
	    for (int vertex_index : face_vertices) {
		ON_3dPoint *point = (*s_cdt->vert_pnts)[vertex_index];
		bool joined = false;
		for (pole_component &component : components) {
		    ON_BoundingBox combined = component.bounds;
		    combined.Set(*point, true);
		    if (combined.Diagonal().Length() > tolerance)
			continue;
		    component.bounds = combined;
		    component.vertices.push_back(vertex_index);
		    joined = true;
		    break;
		}
		if (!joined) {
		    pole_component component;
		    component.bounds = ON_BoundingBox(*point, *point);
		    component.vertices.push_back(vertex_index);
		    components.push_back(component);
		}
	    }
	    for (const pole_component &component : components) {
		if (component.vertices.size() < 2)
		    continue;
		std::vector<int> parents(component.vertices.size(), -1);
		const auto root = [&parents](int item) {
		    int representative = item;
		    while (parents[(size_t)representative] >= 0)
			representative = parents[(size_t)representative];
		    while (item != representative) {
			const int next = parents[(size_t)item];
			parents[(size_t)item] = representative;
			item = next;
		    }
		    return representative;
		};
		for (size_t first = 0; first < component.vertices.size();
			++first) {
		    for (size_t second = first + 1;
			    second < component.vertices.size(); ++second) {
			bool retrace = false;
			for (const ON_BrepTrim *first_trim :
				vertex_trims[component.vertices[first]]) {
			    for (const ON_BrepTrim *second_trim :
				    vertex_trims[component.vertices[second]]) {
				retrace = cdt_trim_pcurves_retrace(first_trim,
				    second_trim);
				if (retrace)
				    break;
			    }
			    if (retrace)
				break;
			}
			if (!retrace)
			    continue;
			int first_root = root((int)first);
			int second_root = root((int)second);
			if (first_root == second_root)
			    continue;
			if (parents[(size_t)first_root] >
				parents[(size_t)second_root])
			    std::swap(first_root, second_root);
			parents[(size_t)first_root] +=
			    parents[(size_t)second_root];
			parents[(size_t)second_root] = first_root;
		    }
		}
		std::map<int, std::vector<int>> weld_components;
		for (size_t vertex = 0; vertex < component.vertices.size();
			++vertex)
		    weld_components[root((int)vertex)].push_back(
			component.vertices[vertex]);
		for (const auto &weld_component : weld_components) {
		    if (weld_component.second.size() < 2)
			continue;
		    const int representative_index = *std::min_element(
			weld_component.second.begin(),
			weld_component.second.end());
		    ON_3dPoint *representative =
			(*s_cdt->vert_pnts)[representative_index];
		    for (int vertex_index : weld_component.second) {
			ON_3dPoint *point =
			    (*s_cdt->vert_pnts)[vertex_index];
			if (point != representative &&
				welds.emplace(point, representative).second)
			    welded_vertices++;
		    }
		}
	    }
	}
    }
    if (welds.empty())
	return 0;
    for (const auto &weld : welds)
	s_cdt->collapsed_edge_pnts[weld.first] = weld.second;
    apply_derived_point_welds(s_cdt, welds);
    bu_log("%s: welded %zu coincident surface-pole B-Rep vert%s within "
	"%.17g\n", s_cdt->name ? s_cdt->name : "BREP",
	welded_vertices, welded_vertices == 1 ? "ex" : "ices", tolerance);
    return welded_vertices;
}

/* Collapse only explicit B-Rep edges whose complete curve is contained in
 * the effective modeling tolerance.  Honor a larger finite tolerance
 * declared by the edge, but never exceed the caller's feature resolution.
 * Endpoint proximity alone is insufficient: a long curve may close back on
 * itself or join two otherwise unrelated sheets.  The NURBS control-polygon
 * length and curve bounding box are both conservative guards for the whole
 * edge.
 *
 * This is derived mesh state.  It deliberately leaves the source and working
 * B-Reps untouched and welds the already shared edge samples in every
 * incident face mesh.  Components retain a bounded envelope so a chain of
 * individually short edges cannot transitively collapse a wider feature. */
static size_t
collapse_subtolerance_brep_edges(struct ON_Brep_CDT_State *s_cdt)
{
    if (!s_cdt || !s_cdt->brep || !s_cdt->w3dpnts ||
	    s_cdt->w3dpnts->empty())
	return 0;

    /* BN_TOL_DIST and imported edge tolerances can exceed an entire valid
     * micron-scale solid.  Never weld across a distance larger than the
     * minimum feature resolution derived from the caller's tolerance. */
    if (!std::isfinite(s_cdt->absmin) || s_cdt->absmin <= 0.0)
	return 0;
    struct collapse_candidate {
	int edge_index;
	ON_BoundingBox curve_bounds;
	std::vector<ON_3dPoint *> points;
	double tolerance;
    };
    std::vector<collapse_candidate> candidates;
    std::set<ON_3dPoint *> candidate_point_set;
    for (int edge_index = 0; edge_index < s_cdt->brep->m_E.Count();
	    ++edge_index) {
	ON_BrepEdge &edge = s_cdt->brep->m_E[edge_index];
	if (edge.m_vi[0] < 0 || edge.m_vi[1] < 0 ||
		edge.m_vi[0] == edge.m_vi[1])
	    continue;
	const auto segments = s_cdt->e2polysegs.find(edge_index);
	if (segments == s_cdt->e2polysegs.end() ||
		segments->second.empty())
	    continue;
	bedge_seg_t *first_segment = *segments->second.begin();
	double modeling_tolerance = BN_TOL_DIST;
	if (std::isfinite(edge.m_tolerance) && edge.m_tolerance > 0.0 &&
		!NEAR_EQUAL(edge.m_tolerance, ON_UNSET_VALUE,
		    ON_ZERO_TOLERANCE))
	    modeling_tolerance = std::max(modeling_tolerance,
		(double)edge.m_tolerance);
	const double collapse_tolerance = std::min(
	    (double)s_cdt->absmin, modeling_tolerance);
	if (!first_segment || !first_segment->nc ||
		!std::isfinite(first_segment->cp_len) ||
		first_segment->cp_len <= 0.0 ||
		first_segment->cp_len > collapse_tolerance)
	    continue;
	const ON_BoundingBox curve_bounds =
	    first_segment->nc->BoundingBox();
	if (!curve_bounds.IsValid())
	    continue;
	const double envelope = curve_bounds.Diagonal().Length();
	if (!std::isfinite(envelope) || envelope > collapse_tolerance)
	    continue;

	ON_3dPoint *first_vertex = (*s_cdt->vert_pnts)[edge.m_vi[0]];
	ON_3dPoint *second_vertex = (*s_cdt->vert_pnts)[edge.m_vi[1]];
	if (!first_vertex || !second_vertex ||
		s_cdt->singular_vert_to_norms->find(first_vertex) !=
		s_cdt->singular_vert_to_norms->end() ||
		s_cdt->singular_vert_to_norms->find(second_vertex) !=
		s_cdt->singular_vert_to_norms->end())
	    continue;

	std::set<ON_3dPoint *> members;
	for (bedge_seg_t *segment : segments->second) {
	    if (!segment || !segment->e_start || !segment->e_end)
		continue;
	    members.insert(segment->e_start);
	    members.insert(segment->e_end);
	}
	if (members.size() < 2)
	    continue;
	collapse_candidate candidate = {edge_index, curve_bounds,
	    std::vector<ON_3dPoint *>(members.begin(), members.end()),
	    collapse_tolerance};
	candidates.push_back(candidate);
	candidate_point_set.insert(members.begin(), members.end());
    }

    if (candidates.empty())
	return 0;

    /* Tiny explicit edges are rare.  Allocate union-find state only for
     * samples referenced by pre-screened candidates, not for every surface
     * and refinement point in a potentially million-point tessellation. */
    std::vector<ON_3dPoint *> candidate_points;
    candidate_points.reserve(candidate_point_set.size());
    for (ON_3dPoint *point : *s_cdt->w3dpnts) {
	if (candidate_point_set.find(point) != candidate_point_set.end())
	    candidate_points.push_back(point);
    }
    if (candidate_points.size() != candidate_point_set.size())
	return 0;
    const size_t point_count = candidate_points.size();
    std::map<ON_3dPoint *, size_t> point_index;
    for (size_t i = 0; i < point_count; ++i)
	point_index[candidate_points[i]] = i;

    std::vector<size_t> parent(point_count);
    std::vector<size_t> component_size(point_count, 1);
    std::vector<double> component_tolerance(point_count, DBL_MAX);
    std::vector<ON_BoundingBox> component_bounds;
    component_bounds.reserve(point_count);
    for (size_t i = 0; i < point_count; ++i) {
	parent[i] = i;
	component_bounds.push_back(ON_BoundingBox(*candidate_points[i],
	    *candidate_points[i]));
    }
    const auto root = [&](size_t point) {
	size_t current = point;
	while (parent[current] != current)
	    current = parent[current];
	return current;
    };
    const auto grow_bounds = [](ON_BoundingBox &target,
	    const ON_BoundingBox &source) {
	target.Set(source.Min(), true);
	target.Set(source.Max(), true);
    };

    std::map<int, double> accepted_edges;
    for (const collapse_candidate &candidate : candidates) {
	std::set<size_t> members;
	for (ON_3dPoint *point : candidate.points) {
	    const auto index = point_index.find(point);
	    if (index != point_index.end())
		members.insert(index->second);
	}
	if (members.size() < 2)
	    continue;

	std::set<size_t> roots;
	ON_BoundingBox combined = candidate.curve_bounds;
	double combined_tolerance = candidate.tolerance;
	for (size_t member : members)
	    roots.insert(root(member));
	for (size_t component : roots) {
	    grow_bounds(combined, component_bounds[component]);
	    combined_tolerance = std::min(combined_tolerance,
		component_tolerance[component]);
	}
	const double combined_envelope = combined.Diagonal().Length();
	if (!std::isfinite(combined_envelope) ||
		combined_envelope > combined_tolerance)
	    continue;

	const size_t combined_root = *roots.begin();
	const auto merged_point = [&](ON_3dPoint *point) {
	    const auto index = point_index.find(point);
	    if (index == point_index.end())
		return point;
	    const size_t component = root(index->second);
	    return candidate_points[roots.count(component) ? combined_root :
		component];
	};

	/* A bounded weld can still erase a face. Check every incident trim
	 * ring against the prospective merge while its samples are intact. */
	const size_t minimum_ring_vertices = 3;
	const auto ring_survives = [&](const ON_BrepLoop &loop) {
	    std::set<ON_3dPoint *> vertices;
	    for (int i = 0; i < loop.TrimCount(); ++i) {
		const ON_BrepTrim *trim = loop.Trim(i);
		if (!trim)
		    return false;
		const auto segments = s_cdt->e2polysegs.find(trim->m_ei);
		if (segments == s_cdt->e2polysegs.end())
		    continue;
		for (const bedge_seg_t *segment : segments->second) {
		    if (!segment || !segment->e_start || !segment->e_end)
			continue;
		    vertices.insert(merged_point(segment->e_start));
		    vertices.insert(merged_point(segment->e_end));
		    if (vertices.size() >= minimum_ring_vertices)
			return true;
		}
	    }
	    return false;
	};
	const ON_BrepEdge &edge = s_cdt->brep->m_E[candidate.edge_index];
	bool preserves_rings = true;
	for (int i = 0; i < edge.TrimCount(); ++i) {
	    const ON_BrepTrim *trim = edge.Trim(i);
	    const ON_BrepLoop *loop = trim ? trim->Loop() : NULL;
	    if (!loop || !ring_survives(*loop)) {
		preserves_rings = false;
		break;
	    }
	}
	if (!preserves_rings)
	    continue;

	for (size_t component : roots) {
	    if (component == combined_root)
		continue;
	    parent[component] = combined_root;
	    component_size[combined_root] += component_size[component];
	}
	component_bounds[combined_root] = combined;
	component_tolerance[combined_root] = combined_tolerance;
	accepted_edges[candidate.edge_index] = candidate.tolerance;
    }

    if (accepted_edges.empty())
	return 0;

    std::map<ON_3dPoint *, int> topology_vertex;
    for (const auto &vertex : *s_cdt->vert_pnts)
	topology_vertex[vertex.second] = vertex.first;
    std::map<size_t, std::vector<size_t>> components;
    for (size_t i = 0; i < point_count; ++i) {
	const size_t component = root(i);
	if (component_size[component] > 1)
	    components[component].push_back(i);
    }

    s_cdt->collapsed_edge_pnts.clear();
    for (const auto &component : components) {
	size_t representative = component.second.front();
	int representative_vertex = std::numeric_limits<int>::max();
	for (size_t member : component.second) {
	    ON_3dPoint *point = candidate_points[member];
	    const auto vertex = topology_vertex.find(point);
	    if (vertex != topology_vertex.end() &&
		    vertex->second < representative_vertex) {
		representative = member;
		representative_vertex = vertex->second;
	    } else if (representative_vertex ==
		    std::numeric_limits<int>::max() &&
		    member < representative) {
		representative = member;
	    }
	}
	ON_3dPoint *representative_point = candidate_points[representative];
	for (size_t member : component.second)
	    s_cdt->collapsed_edge_pnts[candidate_points[member]] =
		representative_point;
    }
    s_cdt->collapsed_edges.clear();
    for (const auto &accepted : accepted_edges)
	s_cdt->collapsed_edges.insert(accepted.first);

    for (auto &vertex : *s_cdt->vert_pnts) {
	const auto canonical = s_cdt->collapsed_edge_pnts.find(vertex.second);
	if (canonical != s_cdt->collapsed_edge_pnts.end())
	    vertex.second = canonical->second;
    }
    std::set<ON_3dPoint *> canonical_edge_points;
    for (ON_3dPoint *point : *s_cdt->edge_pnts) {
	const auto canonical = s_cdt->collapsed_edge_pnts.find(point);
	canonical_edge_points.insert(canonical ==
		s_cdt->collapsed_edge_pnts.end() ? point : canonical->second);
    }
    s_cdt->edge_pnts->swap(canonical_edge_points);
    for (auto &face : s_cdt->fmeshes) {
	cdt_mesh_t &mesh = face.second;
	for (ON_3dPoint *&point : mesh.pnts) {
	    const auto canonical = s_cdt->collapsed_edge_pnts.find(point);
	    if (canonical != s_cdt->collapsed_edge_pnts.end())
		point = canonical->second;
	}
	mesh.p2ind.clear();
	for (size_t i = 0; i < mesh.pnts.size(); ++i)
	    mesh.p2ind[mesh.pnts[i]] = (long)i;
    }

    /* Adjacent ordinary edges share the collapsed topology vertices.  Weld
     * every segment endpoint that references one of those vertices, not just
     * the segments belonging to the tiny edge itself, so later shared-edge
     * refinement cannot reintroduce the discarded point identity. */
    for (auto &edge_segments : s_cdt->e2polysegs) {
	for (bedge_seg_t *segment : edge_segments.second) {
	    if (!segment)
		continue;
	    const auto start = s_cdt->collapsed_edge_pnts.find(
		segment->e_start);
	    const auto end = s_cdt->collapsed_edge_pnts.find(segment->e_end);
	    const auto root_start = s_cdt->collapsed_edge_pnts.find(
		segment->e_root_start);
	    const auto root_end = s_cdt->collapsed_edge_pnts.find(
		segment->e_root_end);
	    if (start != s_cdt->collapsed_edge_pnts.end())
		segment->e_start = start->second;
	    if (end != s_cdt->collapsed_edge_pnts.end())
		segment->e_end = end->second;
	    if (root_start != s_cdt->collapsed_edge_pnts.end())
		segment->e_root_start = root_start->second;
	    if (root_end != s_cdt->collapsed_edge_pnts.end())
		segment->e_root_end = root_end->second;
	}
    }
    for (const auto &accepted : accepted_edges) {
	const int edge_index = accepted.first;
	const ON_BrepEdge &edge = s_cdt->brep->m_E[edge_index];
	const bedge_seg_t *segment = *s_cdt->e2polysegs[edge_index].begin();
	bu_log("%s: collapsed sub-tolerance B-Rep edge %d (V%d/V%d, "
	    "control polygon %.17g, envelope %.17g, effective tolerance "
	    "%.17g, BN_TOL_DIST %.17g)\n",
	    s_cdt->name ? s_cdt->name : "BREP", edge_index,
	    edge.m_vi[0], edge.m_vi[1], segment->cp_len,
	    segment->nc->BoundingBox().Diagonal().Length(),
	    accepted.second, (double)BN_TOL_DIST);
    }
    return accepted_edges.size();
}

int
cdt_test_subtolerance_edge_collapse(void)
{
    ON_Brep source;
    source.NewVertex(ON_3dPoint(0.0, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(0.0004, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(0.0008, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(100.0, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(200.0, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(200.0001, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(400.0, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(400.001, 0.0, 0.0));
    const int ring_first_vertex = source.m_V.Count();
    source.NewVertex(ON_3dPoint(0.0, 2.0, 0.0));
    source.NewVertex(ON_3dPoint(0.0004, 2.0, 0.0));
    source.NewVertex(ON_3dPoint(0.0, 3.0, 0.0));
    ON_BrepVertex &v0 = source.m_V[0];
    ON_BrepVertex &v1 = source.m_V[1];
    ON_BrepVertex &v2 = source.m_V[2];
    ON_BrepVertex &v3 = source.m_V[3];
    ON_BrepVertex &v4 = source.m_V[4];
    ON_BrepVertex &v5 = source.m_V[5];
    ON_BrepVertex &v6 = source.m_V[6];
    ON_BrepVertex &v7 = source.m_V[7];
    source.NewEdge(v1, v0, source.AddEdgeCurve(new ON_LineCurve(
	v1.Point(), v0.Point())));
    source.NewEdge(v1, v2, source.AddEdgeCurve(new ON_LineCurve(
	v1.Point(), v2.Point())));
    source.NewEdge(v2, v3, source.AddEdgeCurve(new ON_LineCurve(
	v2.Point(), v3.Point())));
    /* These endpoints are close, but the curve takes a long detour.  Whole
     * curve bounds must reject it despite endpoint proximity. */
    ON_3dPointArray detour_points;
    detour_points.Append(v4.Point());
    detour_points.Append(ON_3dPoint(300.0, 100.0, 0.0));
    detour_points.Append(v5.Point());
    source.NewEdge(v4, v5, source.AddEdgeCurve(new ON_PolylineCurve(
	detour_points)));
    ON_BrepEdge &declared_tolerant = source.NewEdge(v6, v7,
	source.AddEdgeCurve(new ON_LineCurve(v6.Point(), v7.Point())));
    declared_tolerant.m_tolerance = 0.01;

    /* The first edge is within tolerance, but welding it would erase the
     * triangular trim ring. It must keep all three authoritative vertices. */
    ON_BrepFace &face = source.NewFace(source.AddSurface(
	new ON_PlaneSurface(ON_xy_plane)));
    ON_BrepLoop &loop = source.NewLoop(ON_BrepLoop::outer, face);
    const int ring_edge_count = 3;
    for (int edge_index = 0; edge_index < ring_edge_count; ++edge_index) {
	ON_BrepVertex &first = source.m_V[ring_first_vertex + edge_index];
	ON_BrepVertex &second = source.m_V[ring_first_vertex +
	    (edge_index + 1) % ring_edge_count];
	ON_BrepEdge &edge = source.NewEdge(first, second, source.AddEdgeCurve(
	    new ON_LineCurve(first.Point(), second.Point())));
	const int trim_curve = source.AddTrimCurve(new ON_LineCurve(
	    ON_2dPoint(first.Point()), ON_2dPoint(second.Point())));
	source.NewTrim(edge, false, loop, trim_curve);
    }

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(&source,
	"sub-tolerance edge test");
    state->brep = new ON_Brep(source);
    for (int vertex_index = 0; vertex_index < state->brep->m_V.Count();
	    ++vertex_index) {
	ON_3dPoint *point = new ON_3dPoint(
	    state->brep->m_V[vertex_index].Point());
	(*state->vert_pnts)[vertex_index] = point;
	CDT_Add3DPnt(state, point, -1, vertex_index, -1, -1, -1, -1);
	state->edge_pnts->insert(point);
    }
    for (int edge_index = 0; edge_index < state->brep->m_E.Count();
	    ++edge_index) {
	ON_BrepEdge &edge = state->brep->m_E[edge_index];
	bedge_seg_t *segment = new bedge_seg_t;
	segment->edge_ind = edge_index;
	segment->brep = state->brep;
	segment->p_cdt = state;
	segment->nc = edge.EdgeCurveOf()->NurbsCurve();
	segment->cp_len = segment->nc->ControlPolygonLength();
	segment->e_start = (*state->vert_pnts)[edge.m_vi[0]];
	segment->e_end = (*state->vert_pnts)[edge.m_vi[1]];
	segment->e_root_start = segment->e_start;
	segment->e_root_end = segment->e_end;
	state->e2polysegs[edge_index].insert(segment);
    }

    ON_3dPoint *expected = (*state->vert_pnts)[0];
    ON_3dPoint *expected_declared = (*state->vert_pnts)[6];
    bedge_seg_t *adjacent = *state->e2polysegs[1].begin();
    state->absmin = 1.0e-5;
    if (collapse_subtolerance_brep_edges(state) != 0) {
	ON_Brep_CDT_Destroy(state);
	return 4;
    }
    state->absmin = 0.02;
    const size_t collapsed = collapse_subtolerance_brep_edges(state);
    int result = 0;
    if (collapsed != 2 || state->collapsed_edges.size() != 2 ||
	    state->collapsed_edges.find(0) == state->collapsed_edges.end() ||
	    state->collapsed_edges.find(3) != state->collapsed_edges.end() ||
	    state->collapsed_edges.find(4) == state->collapsed_edges.end())
	result = 1;
    else if ((*state->vert_pnts)[0] != expected ||
	    (*state->vert_pnts)[1] != expected ||
	    (*state->vert_pnts)[2] == expected ||
	    (*state->vert_pnts)[6] != expected_declared ||
	    (*state->vert_pnts)[7] != expected_declared ||
	    (*state->vert_pnts)[ring_first_vertex] ==
	    (*state->vert_pnts)[ring_first_vertex + 1])
	result = 2;
    else if (adjacent->e_start != expected ||
	    adjacent->e_root_start != expected)
	result = 3;

    ON_Brep_CDT_Destroy(state);
    return result;
}

ON_3dVector
calc_trim_vnorm(ON_BrepVertex& v, ON_BrepTrim *trim)
{
    ON_3dPoint t1, t2;
    ON_3dVector v1 = ON_3dVector::UnsetVector;
    ON_3dVector v2 = ON_3dVector::UnsetVector;
    ON_3dVector trim_norm = ON_3dVector::UnsetVector;

    ON_Interval trange = trim->Domain();
    ON_3dPoint t_2d1 = trim->PointAt(trange[0]);
    ON_3dPoint t_2d2 = trim->PointAt(trange[1]);

    ON_Plane fplane;
    const ON_Surface *s = trim->SurfaceOf();
    double ptol = s->BoundingBox().Diagonal().Length()*0.001;
    ptol = (ptol < BREP_PLANAR_TOL) ? ptol : BREP_PLANAR_TOL;
    if (s->IsPlanar(&fplane, ptol)) {
	trim_norm = fplane.Normal();
	if (trim->Face()->m_bRev) {
	    trim_norm = trim_norm * -1;
	}
    } else {
	int ev1 = 0;
	int ev2 = 0;
	if (cdt_surface_normal(s, ON_2dPoint(t_2d1), t1, v1)) {
	    if (trim->Face()->m_bRev) {
		v1 = v1 * -1;
	    }
	    ev1 = 1;
	}
	if (cdt_surface_normal(s, ON_2dPoint(t_2d2), t2, v2)) {
	    if (trim->Face()->m_bRev) {
		v2 = v2 * -1;
	    }
	    ev2 = 1;
	}
	// If we got both of them, go with the closest one
	if (ev1 && ev2) {
	    trim_norm = (v.Point().DistanceTo(t1) < v.Point().DistanceTo(t2)) ? v1 : v2;
	}

	if (ev1 && !ev2) {
	    trim_norm = v1;
	}

	if (!ev1 && ev2) {
	    trim_norm = v2;
	}
    }

    return trim_norm;
}

static void
calc_singular_vert_norm(struct ON_Brep_CDT_State *s_cdt, int index)
{
    ON_BrepVertex& v = s_cdt->brep->m_V[index];
    int have_calculated = 0;
    ON_3dVector vnrml(0.0, 0.0, 0.0);

    if (s_cdt->singular_vert_to_norms->find((*s_cdt->vert_pnts)[index]) != (s_cdt->singular_vert_to_norms->end())) {
	// Already processed this one
	return;
    }

    //bu_log("Processing vert %d (%f %f %f)\n", index, v.Point().x, v.Point().y, v.Point().z);

    for (int eind = 0; eind != v.EdgeCount(); eind++) {
	ON_3dVector trim1_norm = ON_3dVector::UnsetVector;
	ON_3dVector trim2_norm = ON_3dVector::UnsetVector;
	ON_BrepEdge& edge = s_cdt->brep->m_E[v.m_ei[eind]];
	if (edge.TrimCount() != 2) {
	    // Don't know what to do with this yet... skip.
	    continue;
	}
	ON_BrepTrim *trim1 = edge.Trim(0);
	ON_BrepTrim *trim2 = edge.Trim(1);

	if (trim1->m_type != ON_BrepTrim::singular) {
	    trim1_norm = calc_trim_vnorm(v, trim1);
	}
	if (trim2->m_type != ON_BrepTrim::singular) {
	    trim2_norm = calc_trim_vnorm(v, trim2);
	}

	// If one of the normals is unset and the other comes from a plane, use it
	if (trim1_norm == ON_3dVector::UnsetVector && trim2_norm != ON_3dVector::UnsetVector) {
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim1_norm = trim2_norm;
	}
	if (trim1_norm != ON_3dVector::UnsetVector && trim2_norm == ON_3dVector::UnsetVector) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim2_norm = trim1_norm;
	}

	// If we have disagreeing normals and one of them is from a planar surface, go
	// with that one
	if (NEAR_EQUAL(ON_DotProduct(trim1_norm, trim2_norm), -1, VUNITIZE_TOL)) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && !s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Normals severely disagree, no planar surface to fall back on - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Two disagreeing planes - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim2_norm = trim1_norm;
	    }
	    if (s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim1_norm = trim2_norm;
	    }
	}

	// Add the normals to the vnrml total
	vnrml += trim1_norm;
	vnrml += trim2_norm;
	have_calculated = 1;

    }
    if (!have_calculated) {
	return;
    }

    // Average all the successfully calculated normals into a new unit normal
    vnrml.Unitize();

    // We store this as a point to keep C++ happy...  If we try to
    // propagate the ON_3dVector type through all the CDT logic it
    // triggers issues with the compile.
    (*s_cdt->vert_avg_norms)[index] = new ON_3dPoint(vnrml);
    s_cdt->w3dnorms->push_back((*s_cdt->vert_avg_norms)[index]);

    // If we have a vertex normal, add it to the map which will allow us
    // to ascertain if a given point has such a normal.  This will allow
    // a point-based check even if we don't know a vertex index locally
    // in the code.
    (*s_cdt->singular_vert_to_norms)[(*s_cdt->vert_pnts)[index]] = (*s_cdt->vert_avg_norms)[index];

}

/* The face-local certificates cannot detect chords from distinct faces that
 * cross in model space.  Once final assembly identifies such a pair, feed the
 * exact source triangles back into their native surface charts.  Inserting an
 * interior surface sample shortens only those chords; genuine intersections
 * therefore remain and exhaust the bounded refinement budget, while
 * approximation-induced crossings converge without changing B-Rep topology. */
size_t
refine_assembled_intersection(struct ON_Brep_CDT_State *s_cdt,
	const assembled_mesh_validation &validation, size_t max_points)
{
    if (!s_cdt || !max_points ||
	    !validation.intersecting_triangle_pairs)
	return 0;
    int source_faces[2] = {-1, -1};
    bool prefer_triangle_edge_split = false;
    ON_3dPoint intersection(validation.first_intersection_point[0],
	validation.first_intersection_point[1],
	validation.first_intersection_point[2]);
    for (int pair_index = 0; pair_index < 2; ++pair_index) {
	const int bot_triangle = validation.first_intersection[pair_index];
	if (bot_triangle >= 0 && (size_t)bot_triangle <
		s_cdt->bot_face_to_brep_face.size())
	    source_faces[pair_index] = s_cdt->bot_face_to_brep_face[
		(size_t)bot_triangle];
    }

    /* Adjacent surface approximations most often cross in a thin layer next
     * to their shared curved edge.  Splitting the nearest shared master-edge
     * segment is preferable to perturbing either face interior: both charts
     * receive the same authoritative 3-D point and watertightness is retained
     * by construction. */
    if (validation.first_intersection_point_valid && source_faces[0] >= 0 &&
	    source_faces[1] >= 0 && source_faces[0] != source_faces[1]) {
	const auto face_edges = [&](int face_index) {
	    std::set<int> result;
	    const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	    for (int loop_index = 0; loop_index < face.LoopCount();
		    ++loop_index) {
		const ON_BrepLoop *loop = face.Loop(loop_index);
		if (!loop)
		    continue;
		for (int trim_index = 0; trim_index < loop->TrimCount();
			++trim_index) {
		    const ON_BrepTrim *trim = loop->Trim(trim_index);
		    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		    if (edge)
			result.insert(edge->m_edge_index);
		}
	    }
	    return result;
	};
	const std::set<int> first_edges = face_edges(source_faces[0]);
	const std::set<int> second_edges = face_edges(source_faces[1]);
	std::vector<int> shared_edges;
	std::set_intersection(first_edges.begin(), first_edges.end(),
	    second_edges.begin(), second_edges.end(),
	    std::back_inserter(shared_edges));
	const std::set<int> shared_edge_set(shared_edges.begin(),
	    shared_edges.end());
	bool pair_incident_to_shared_edge = !shared_edge_set.empty();
	double pair_triangle_scale = 0.0;
	for (int pair_index = 0; pair_index < 2; ++pair_index) {
	    const int bot_triangle = validation.first_intersection[pair_index];
	    const int face = source_faces[pair_index];
	    bool incident = false;
	    if (bot_triangle >= 0 && face >= 0 &&
		    (size_t)bot_triangle <
		    s_cdt->bot_face_to_cdt_triangle.size()) {
		const auto mesh_entry = s_cdt->fmeshes.find(face);
		const size_t local_triangle =
		    s_cdt->bot_face_to_cdt_triangle[(size_t)bot_triangle];
		if (mesh_entry != s_cdt->fmeshes.end() &&
			local_triangle < mesh_entry->second.tris_vect.size()) {
		    const triangle_t &triangle =
			mesh_entry->second.tris_vect[local_triangle];
		    for (int edge = 0; edge < 3; ++edge) {
			const uedge_t candidate(triangle.v[edge],
			    triangle.v[(edge + 1) % 3]);
			if (candidate.v[0] >= 0 && candidate.v[1] >= 0 &&
				(size_t)candidate.v[0] <
				mesh_entry->second.pnts.size() &&
				(size_t)candidate.v[1] <
				mesh_entry->second.pnts.size()) {
			    const ON_3dPoint *first_point =
				mesh_entry->second.pnts[(size_t)candidate.v[0]];
			    const ON_3dPoint *second_point =
				mesh_entry->second.pnts[(size_t)candidate.v[1]];
			    if (first_point && second_point)
				pair_triangle_scale = std::max(
				    pair_triangle_scale,
				    first_point->DistanceTo(*second_point));
			}
			const auto boundary =
			    mesh_entry->second.ue2b_map.find(candidate);
			if (boundary != mesh_entry->second.ue2b_map.end() &&
				boundary->second &&
				shared_edge_set.find(
				boundary->second->edge_ind) !=
				shared_edge_set.end()) {
			    incident = true;
			    break;
			}
		    }
		}
	    }
	    pair_incident_to_shared_edge =
		pair_incident_to_shared_edge && incident;
	}
	prefer_triangle_edge_split = !pair_incident_to_shared_edge;
	bedge_seg_t *nearest = NULL;
	double nearest_distance = DBL_MAX;
	for (int edge : shared_edges) {
	    const auto segments = s_cdt->e2polysegs.find(edge);
	    if (segments == s_cdt->e2polysegs.end())
		continue;
	    for (bedge_seg_t *segment : segments->second) {
		if (!segment || !segment->e_start || !segment->e_end)
		    continue;
		const ON_3dPoint closest = ON_Line(*segment->e_start,
		    *segment->e_end).ClosestPointTo(intersection);
		const double distance = closest.DistanceTo(intersection);
		if (distance < nearest_distance) {
		    nearest_distance = distance;
		    nearest = segment;
		}
	    }
	}
	/* The intersecting chords can be one element inward from their common
	 * boundary, so requiring direct incidence misses precisely the coarse
	 * approximation that this retry is meant to repair.  Still reject a
	 * remote shared edge: an intersection farther away than the local chord's
	 * sag scale is not plausibly caused by that boundary approximation. */
	const bool near_shared_edge = nearest &&
	    (pair_incident_to_shared_edge ||
	    nearest_distance <= MAX_SHARED_EDGE_REFINEMENT_DISTANCE_RATIO *
	    pair_triangle_scale + BN_TOL_DIST);
	if (near_shared_edge) {
	    const std::set<bedge_seg_t *> split = split_edge_seg(s_cdt,
		nearest, 1, NULL, 1);
	    if (!split.empty()) {
		std::set<int> changed_faces = {
		    source_faces[0], source_faces[1]
		};
		for (int face : changed_faces) {
		    cdt_mesh_t &mesh = s_cdt->fmeshes[face];
		    mesh.brep_edges.clear();
		    mesh.chart_boundary_edges.clear();
		    mesh.ue2b_map.clear();
		    if (!mesh.cdt())
			return 0;
		    bool mapped = loop_edges(&mesh, &mesh.outer_loop);
		    for (const auto &inner : mesh.inner_loops)
			mapped = loop_edges(&mesh, inner.second) && mapped;
		    if (!mapped || !refine_triangulation(s_cdt, &mesh, 0, 0) ||
			    mesh.geometric_degenerate_count())
			return 0;
		}
		return 1;
	    }
	}
    }

    std::map<int, std::vector<triangle_t>> targets;
    for (int pair_index = 0; pair_index < 2; ++pair_index) {
	const int bot_triangle = validation.first_intersection[pair_index];
	if (bot_triangle < 0 || (size_t)bot_triangle >=
		s_cdt->bot_face_to_brep_face.size() ||
		(size_t)bot_triangle >=
		s_cdt->bot_face_to_cdt_triangle.size())
	    continue;
	const int face = s_cdt->bot_face_to_brep_face[
	    (size_t)bot_triangle];
	const size_t local_triangle = s_cdt->bot_face_to_cdt_triangle[
	    (size_t)bot_triangle];
	auto mesh = s_cdt->fmeshes.find(face);
	if (mesh == s_cdt->fmeshes.end() ||
		local_triangle >= mesh->second.tris_vect.size())
	    continue;
	targets[face].push_back(mesh->second.tris_vect[local_triangle]);
    }

    size_t inserted = 0;
    std::vector<int> changed_faces;
    if (prefer_triangle_edge_split) {
	/* Refining a nearly exact chord leaves the crossing in place if
	 * the other face carries the approximation error. Prioritize the
	 * larger measured surface deviation and retain face order on ties. */
	std::vector<std::pair<double, int>> priority;
	for (const auto &entry : targets) {
	    cdt_mesh_t &mesh = s_cdt->fmeshes.at(entry.first);
	    double maximum = -1.0;
	    for (const triangle_t &triangle : entry.second) {
		double deviation;
		if (mesh.surface_triangle_deviation(triangle, &deviation))
		    maximum = std::max(maximum, deviation);
	    }
	    priority.emplace_back(maximum, entry.first);
	}
	std::stable_sort(priority.begin(), priority.end(),
	    [](const auto &first, const auto &second) {
		return first.first > second.first;
	    });
	for (const auto &entry : priority) {
	    const int face = entry.second;
	    cdt_mesh_t &mesh = s_cdt->fmeshes.at(face);
	    inserted = mesh.split_problem_triangle_edges(targets.at(face), 1,
		validation.first_intersection_point_valid ? &intersection : NULL);
	    if (!inserted)
		continue;
	    if (mesh.valid(0) || (mesh.repair_incorrect_normal_edges() &&
		    mesh.valid(0)))
		return inserted;
	    return 0;
	}
    }
    for (auto &entry : targets) {
	if (inserted >= max_points)
	    break;
	cdt_mesh_t &mesh = s_cdt->fmeshes[entry.first];
	const size_t face_inserted = mesh.refine_problem_triangles(
	    entry.second, max_points - inserted);
	if (!face_inserted)
	    continue;
	inserted += face_inserted;
	changed_faces.push_back(entry.first);
    }
    if (!inserted)
	return 0;

    for (int face : changed_faces) {
	cdt_mesh_t &mesh = s_cdt->fmeshes[face];
	if (!mesh.cdt()) {
	    return 0;
	}
	if (!refine_triangulation(s_cdt, &mesh, 0, 0)) {
	    return 0;
	}
	if (mesh.geometric_degenerate_count()) {
	    return 0;
	}
    }
    return inserted;
}

typedef std::pair<int, int> assembled_edge_t;
typedef std::pair<assembled_edge_t, std::set<int, std::greater<int>>>
    assembled_shared_chord_t;

static std::vector<assembled_shared_chord_t>
assembled_shared_chords(int vertex_count, int face_count, const int *faces,
	const std::vector<int> &source_faces)
{
    struct chord_incidence {
	size_t count = 0;
	std::set<int, std::greater<int>> source_faces;
    };
    std::vector<assembled_shared_chord_t> shared;
    if (vertex_count <= 0 || face_count <= 0 || !faces ||
	    source_faces.size() != (size_t)face_count)
	return shared;
    std::map<assembled_edge_t, chord_incidence> incidence;
    for (int triangle = 0; triangle < face_count; ++triangle) {
	for (int corner = 0; corner < 3; ++corner) {
	    int first = faces[3 * triangle + corner];
	    int second = faces[3 * triangle + (corner + 1) % 3];
	    if (first < 0 || second < 0 || first >= vertex_count ||
		    second >= vertex_count || first == second)
		continue;
	    if (second < first)
		std::swap(first, second);
	    chord_incidence &entry = incidence[{first, second}];
	    entry.count++;
	    entry.source_faces.insert(source_faces[(size_t)triangle]);
	}
    }
    for (const auto &entry : incidence) {
	if (entry.second.count > 2 && entry.second.source_faces.size() > 1)
	    shared.push_back({entry.first, entry.second.source_faces});
    }
    return shared;
}

int
cdt_test_assembled_shared_chords(void)
{
    const int common_diagonal[] = {
	0, 1, 2, 0, 2, 3,
	2, 1, 0, 3, 2, 0
    };
    const std::vector<int> two_faces = {0, 0, 1, 1};
    const std::vector<assembled_shared_chord_t> shared =
	assembled_shared_chords(4, 4, common_diagonal, two_faces);
    if (shared.size() != 1 || shared[0].first != assembled_edge_t(0, 2) ||
	    shared[0].second !=
	    std::set<int, std::greater<int>>({1, 0}))
	return 1;

    const int distinct_diagonals[] = {
	0, 1, 2, 0, 2, 3,
	1, 0, 3, 3, 2, 1
    };
    if (!assembled_shared_chords(4, 4, distinct_diagonals,
	    two_faces).empty())
	return 2;
    const std::vector<int> one_face = {0, 0, 0, 0};
    return assembled_shared_chords(4, 4, common_diagonal,
	one_face).empty() ? 0 : 3;
}

/* Two thin faces can legitimately share all of their B-Rep boundary points.
 * If their independent CDTs choose the same non-boundary chord, however, that
 * chord has four incident triangles in the assembled mesh and its endpoint
 * links cease to be disks.  Refine one incident face at the chord midpoint.
 * The sample is evaluated on that face's source surface, leaves the
 * authoritative B-Rep boundary untouched, and removes only the accidental
 * cross-face chord identity. */
static size_t
refine_assembled_shared_chords(struct ON_Brep_CDT_State *s_cdt,
	int vertex_count, int face_count, const int *faces,
	size_t max_points)
{
    if (!s_cdt || vertex_count <= 0 || face_count <= 0 || !faces ||
	    !max_points ||
	    s_cdt->bot_pnt_to_on_pnt->size() != (size_t)vertex_count ||
	    s_cdt->bot_face_to_brep_face.size() != (size_t)face_count)
	return 0;
    const std::vector<assembled_shared_chord_t> shared =
	assembled_shared_chords(vertex_count, face_count, faces,
	    s_cdt->bot_face_to_brep_face);

    size_t inserted = 0;
    std::set<int> changed_faces;
    for (const assembled_shared_chord_t &edge_entry : shared) {
	if (inserted >= max_points)
	    break;
	ON_3dPoint *first_point = (*s_cdt->bot_pnt_to_on_pnt)[
	    (size_t)edge_entry.first.first];
	ON_3dPoint *second_point = (*s_cdt->bot_pnt_to_on_pnt)[
	    (size_t)edge_entry.first.second];
	if (!first_point || !second_point)
	    continue;
	const ON_3dPoint midpoint = 0.5 * (*first_point + *second_point);
	for (int face : edge_entry.second) {
	    const auto found_mesh = s_cdt->fmeshes.find(face);
	    if (found_mesh == s_cdt->fmeshes.end())
		continue;
	    cdt_mesh_t &mesh = found_mesh->second;
	    const auto first_local = mesh.p2ind.find(first_point);
	    const auto second_local = mesh.p2ind.find(second_point);
	    if (first_local == mesh.p2ind.end() ||
		    second_local == mesh.p2ind.end() ||
		    first_local->second == second_local->second)
		continue;
	    const uedge_t local_edge(first_local->second,
		second_local->second);
	    if (mesh.brep_edges.find(local_edge) != mesh.brep_edges.end() ||
		    mesh.chart_boundary_edges.find(local_edge) !=
		    mesh.chart_boundary_edges.end())
		continue;
	    const auto local_incidence = mesh.uedges2tris.find(local_edge);
	    if (local_incidence == mesh.uedges2tris.end() ||
		    local_incidence->second.size() != 2)
		continue;
	    const size_t local_triangle = *local_incidence->second.begin();
	    if (local_triangle >= mesh.tris_vect.size())
		continue;
	    const std::vector<triangle_t> target = {
		mesh.tris_vect[local_triangle]
	    };
	    if (!mesh.split_problem_triangle_edges(target, 1, &midpoint,
		    &local_edge))
		continue;
	    inserted++;
	    changed_faces.insert(face);
	    break;
	}
    }
    if (!inserted)
	return 0;

    for (int face : changed_faces) {
	cdt_mesh_t &mesh = s_cdt->fmeshes[face];
	if (mesh.valid(0) || (mesh.repair_incorrect_normal_edges() &&
		mesh.valid(0)))
	    continue;
	return 0;
    }
    const char *dump = getenv("BRLCAD_CDT_DUMP_FAILURES");
    if (dump && dump[0] && !BU_STR_EQUAL(dump, "0"))
	bu_log("Refined %zu of %zu accidental cross-face shared chords on "
	    "%zu faces\n", inserted, shared.size(), changed_faces.size());
    return inserted;
}

static size_t
brep_cdt_promote_missing_outer_loops(ON_Brep *brep)
{
    if (!brep)
	return 0;
    size_t promoted = 0;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	ON_BrepFace &face = brep->m_F[face_index];
	if (face.OuterLoop() || face.LoopCount() <= 0)
	    continue;
	ON_BrepLoop *best_loop = NULL;
	double best_area = -1.0;
	for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	    ON_BrepLoop *loop = face.Loop(loop_index);
	    if (!loop)
		continue;
	    ON_BoundingBox bounds;
	    bool have_bounds = false;
	    for (int trim_index = 0; trim_index < loop->TrimCount();
		    ++trim_index) {
		const ON_BrepTrim *trim = loop->Trim(trim_index);
		if (!trim)
		    continue;
		ON_BoundingBox trim_bounds;
		if (!trim->GetBoundingBox(trim_bounds, false))
		    continue;
		if (!have_bounds) {
		    bounds = trim_bounds;
		    have_bounds = true;
		} else {
		    bounds.Union(trim_bounds);
		}
	    }
	    const double area = have_bounds ?
		fabs((bounds.m_max.x - bounds.m_min.x) *
		    (bounds.m_max.y - bounds.m_min.y)) : 0.0;
	    if (!best_loop || area > best_area) {
		best_loop = loop;
		best_area = area;
	    }
	}
	if (!best_loop)
	    continue;
	best_loop->m_type = ON_BrepLoop::outer;
	promoted++;
	bu_log("relaxed rigorous tessellation promoted loop %d on face %d "
	    "to outer\n", best_loop->m_loop_index, face_index);
    }
    return promoted;
}

static bool
brep_cdt_shrink_surfaces(ON_Brep &brep,
	std::set<int> *rebuilt_support = nullptr)
{
    /* Shrinking a periodic support to a pcurve bounding box can discard the
     * surface needed by trims in another periodic image.  Preserve closed
     * directions used by seam faces while conditioning the other domains.
     * These bits are the side masks in ON_Brep::ShrinkSurface. */
    enum shrink_side {
	west = 0x1, south = 0x2, east = 0x4, north = 0x8
    };
    for (int fi = 0; fi < brep.m_F.Count(); ++fi) {
	ON_BrepFace &face = brep.m_F[fi];
	const ON_Surface *surface = face.SurfaceOf();
	if (!surface)
	    continue;
	/* Shrinking a stretched cylindrical support can discard the full
	 * revolution needed to recover the angular trim parameterization. */
	if (rebuilt_support && restore_collapsed_cylinder_support(brep, face)) {
	    rebuilt_support->insert(face.m_face_index);
	    continue;
	}
	if (restore_cylinder_trim_parameters(brep, face))
	    continue;
	if (!restore_periodic_rim_trims(brep, face))
	    return false;
	int disabled_sides = 0;
	if (cdt_face_has_seam(face)) {
	    if (surface->IsClosed(0))
		disabled_sides |= west | east;
	    if (surface->IsClosed(1))
		disabled_sides |= south | north;
	}
	/* A false return also means the surface already has the requested size;
	 * shrinking is optional conditioning, not a tessellation prerequisite. */
	brep.ShrinkSurface(face, disabled_sides);
    }
    brep.Compact();
    return true;
}

int
cdt_test_periodic_surface_conditioning(void)
{
    const ON_Torus torus(ON_Circle(ON_xy_plane, 9.0), 2.5);
    ON_NurbsSurface full_surface;
    if (!torus.GetNurbForm(full_surface))
	return 1;
    ON_NurbsSurface band(full_surface);
    const ON_Interval domain = full_surface.Domain(0);
    if (!band.Trim(0, ON_Interval(domain.ParameterAt(0.2),
	    domain.ParameterAt(0.4))))
	return 2;
    ON_Brep source;
    ON_BrepFace *face = source.NewFace(band);
    if (!face)
	return 3;
    face->m_si = source.AddSurface(full_surface.DuplicateSurface());
    face->SetProxySurface(source.m_S[face->m_si]);
    source.SetTrimIsoFlags();
    source.Compact();
    if (!source.IsValid() || !cdt_face_has_seam(source.m_F[0]))
	return 4;

    const ON__UINT32 crc = source.DataCRC(0);
    ON_Brep working(source);
    if (!brep_cdt_shrink_surfaces(working))
	return 5;
    const ON_Surface *conditioned = working.m_F[0].SurfaceOf();
    if (!working.IsValid() || !conditioned->IsClosed(0) ||
	!conditioned->IsClosed(1) || source.DataCRC(0) != crc)
	return 5;
    for (int li = 0; li < source.m_F[0].LoopCount(); ++li) {
	const ON_BrepLoop *loop = source.m_F[0].Loop(li);
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = loop->Trim(ti);
	    const ON_3dPoint uv = trim->PointAt(trim->Domain().Mid());
	    if (conditioned->PointAt(uv.x, uv.y).DistanceTo(
		    full_surface.PointAt(uv.x, uv.y)) > ON_ZERO_TOLERANCE)
		return 6;
	}
    }
    return 0;
}

static bool
validate_tessellation_input(struct ON_Brep_CDT_State *s_cdt,
	int face_cnt, const int *faces, bool try_invalid_brep)
{
    if (!cdt_tolerance_valid(s_cdt->tol)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_TOLERANCE,
	    BREP_CDT_STAGE_INPUT, -1, 0, 0,
	    "invalid or contradictory tessellation tolerance");
	return false;
    }

    if (face_cnt < 0 || (face_cnt > 0 && !faces)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
	    BREP_CDT_STAGE_INPUT, -1, 0, 0,
	    "invalid face selection");
	return false;
    }
    for (int i = 0; i < face_cnt; ++i) {
	if (faces[i] < 0 || faces[i] >= s_cdt->orig_brep->m_F.Count()) {
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
		BREP_CDT_STAGE_INPUT, faces[i], 0, 0,
		"selected face index is out of range");
	    return false;
	}
    }

    // Check for any conditions that are show-stoppers
    ON_wString wonstr;
    ON_TextLog vout(wonstr);
    if (!s_cdt->orig_brep->IsValid(&vout)) {
	if (!try_invalid_brep) {
	    bu_log("brep is NOT valid, cannot produce watertight mesh\n");
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
		BREP_CDT_STAGE_TOPOLOGY, -1, 0, 0,
		"input B-Rep failed OpenNURBS validity checks");
	    return false;
	}
	std::string relaxed_failure;
	if (!cdt_topology_references_safe(s_cdt->orig_brep,
		&relaxed_failure)) {
	    const std::string message =
		"invalid B-Rep failed relaxed referential-safety checks: " +
		relaxed_failure;
	    bu_log("%s\n", message.c_str());
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
		BREP_CDT_STAGE_TOPOLOGY, -1, 0, 0,
		message.c_str());
	    return false;
	}
    }
    /* Imported solids can retain wire edges outside the trimmed shell.
     * Check shell-edge incidence first so the topology error names the
     * offending source edge. */
    for (int index = 0; index < s_cdt->orig_brep->m_E.Count(); ++index) {
	const ON_BrepEdge &edge = s_cdt->orig_brep->m_E[index];
	const int incident_trims = edge.TrimCount();
	if (incident_trims && incident_trims != 2) {
	    const std::string message = "B-Rep edge " +
		std::to_string(edge.m_edge_index) + " has " +
		std::to_string(incident_trims) +
		" incident trims, expected two for a closed manifold";
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
		BREP_CDT_STAGE_TOPOLOGY, -1, 0, 0, message.c_str());
	    return false;
	}
    }

    bool oriented = false;
    bool boundary = true;
    if (!s_cdt->orig_brep->IsManifold(&oriented, &boundary) || boundary ||
	    !oriented || !s_cdt->orig_brep->IsSolid()) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
	    BREP_CDT_STAGE_TOPOLOGY, -1, 0, 0,
	    "input B-Rep is not a closed oriented manifold solid");
	return false;
    }

    return true;
}

static bool
initialize_tessellation(struct ON_Brep_CDT_State *s_cdt, bool try_invalid_brep)
{
    // We may be changing the ON_Brep data, so work on a copy
    // rather than the original object
    s_cdt->brep = new ON_Brep(*s_cdt->orig_brep);
    if (try_invalid_brep)
	brep_cdt_promote_missing_outer_loops(s_cdt->brep);

    // Attempt to minimize situations where 2D and 3D distances get out of sync
    // by shrinking the surfaces down to the active area of the face
    if (!brep_cdt_shrink_surfaces(*s_cdt->brep,
	    &s_cdt->rebuilt_support_faces)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
	    BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
	    "failed to install corrected periodic trim curves");
	return false;
    }

    ON_Brep* brep = s_cdt->brep;

    // Translate global relative tolerances into physical dimensions based
    // on the BRep bounding box
    cdt_tol_global_calc(s_cdt);

    /* Vertex points need stable addresses throughout edge subdivision. */
    for (int index = 0; index < brep->m_V.Count(); index++) {
	ON_BrepVertex& v = brep->m_V[index];
	(*s_cdt->vert_pnts)[index] = new ON_3dPoint(v.Point());
	CDT_Add3DPnt(s_cdt, (*s_cdt->vert_pnts)[index], -1, v.m_vertex_index, -1, -1, -1, -1);
	// topologically, any vertex point will be on edges
	s_cdt->edge_pnts->insert((*s_cdt->vert_pnts)[index]);
    }

    /* Singular vertices use averaged normals from adjacent regular trims. */
    for (int index = 0; index < brep->m_T.Count(); index++) {
	ON_BrepTrim &trim = s_cdt->brep->m_T[index];
	if (trim.m_type == ON_BrepTrim::singular) {
	    ON_BrepVertex *v1 = trim.Vertex(0);
	    ON_BrepVertex *v2 = trim.Vertex(1);
	    calc_singular_vert_norm(s_cdt, v1->m_vertex_index);
	    calc_singular_vert_norm(s_cdt, v2->m_vertex_index);
	}
    }

    // Set up the edge containers that will manage the edge subdivision.
    initialize_edge_containers(s_cdt);

    // Next, for each face and each loop in each face define the initial
    // loop polygons.  Note there is no splitting of edges at this point -
    // we are simply establishing the initial closed polygons.
    if (!initialize_loop_polygons(s_cdt)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
	    BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
	    "failed to initialize trim loop polygons");
	return false;
    }

    for (const auto &edge : s_cdt->e2polysegs) {
	for (bedge_seg_t *segment : edge.second) {
	    segment->tan_start = bseg_tangent(s_cdt, segment,
		segment->edge_start, segment->tseg1->trim_start,
		segment->tseg2->trim_start);
	    segment->tan_end = bseg_tangent(s_cdt, segment,
		segment->edge_end, segment->tseg1->trim_end,
		segment->tseg2->trim_end);
	}
    }

    char pole_split_message[256] = {0};
    if (!split_edges_at_surface_poles(s_cdt, pole_split_message,
	    sizeof(pole_split_message))) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
	    BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
	    pole_split_message[0] ? pole_split_message :
	    "failed to split shared edges at surface poles");
	return false;
    }

    // Do the non-tolerance based initialization splits.
    char edge_init_message[256] = {0};
    if (!initialize_edge_segs(s_cdt, edge_init_message,
	    sizeof(edge_init_message))) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
	    BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
	    edge_init_message[0] ? edge_init_message :
	    "failed to initialize shared edge segments");
	return false;
    }

    // If edge segments are too close together in 2D space compared to their
    // length, it is difficult to mesh them successfully.  Refine edges that
    // are close to other edges.
    refine_close_edges(s_cdt);

    // On to tolerance based splitting.  Process the non-linear edges first -
    // we will need information from them to handle the linear edges
    tol_curved_edges_split(s_cdt);

    // After the initial curve split, make another pass looking for curved
    // edges sharing a vertex.  We want larger curves to refine close to the
    // median segment length of the smaller ones, since this situation can be a
    // sign that the surface will generate small triangles near large ones.
    curved_edges_refine(s_cdt);

    // Now, process the linear edges
    char message[256] = {0};
    if (!tol_linear_edges_split(s_cdt, message, sizeof(message))) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REFINEMENT_LIMIT,
	    BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
	    message);
	return false;
    }

    /* Subdivide singular trims for the chart triangulator; their collapsed
     * 3-D triangles are discarded during mesh installation. */
    const int singular_segment_target = 6;
    while (s_cdt->unsplit_singular_edges.size()) {
	std::queue<cpolyedge_t *> w1, w2;
	std::queue<cpolyedge_t *> *wq, *nq, *tmpq;
	int cnt = 0;
	wq = &w1;
	nq = &w2;
	std::set<cpolyedge_t *>::iterator first =
	    std::min_element(s_cdt->unsplit_singular_edges.begin(),
		    s_cdt->unsplit_singular_edges.end(),
		    singular_edge_process_less);
	nq->push(*first);
	s_cdt->unsplit_singular_edges.erase(first);
	while (cnt < singular_segment_target) {
	    cnt = 0;
	    tmpq = wq;
	    wq = nq;
	    nq = tmpq;
	    while (!wq->empty()) {
		cpolyedge_t *ce = wq->front();
		wq->pop();
		std::set<cpolyedge_t *> nedges = split_singular_seg(s_cdt, ce, 0);
		std::vector<cpolyedge_t *> ordered_edges(nedges.begin(),
			nedges.end());
		std::sort(ordered_edges.begin(), ordered_edges.end(),
			singular_edge_process_less);
		for (std::vector<cpolyedge_t *>::const_iterator n_it =
			ordered_edges.begin(); n_it != ordered_edges.end(); ++n_it) {
		    nq->push(*n_it);
		    cnt++;
		}
	    }
	}
    }

    /* A valid solid may contain a distinct topology edge whose complete
     * curve is smaller than the fixed modeling tolerance.  Weld only those
     * explicitly proven edges in derived mesh state, before any face chart
     * is built. */
    collapse_subtolerance_brep_edges(s_cdt);

    /* Some valid imports use distinct B-Rep vertex identities for repeated
     * visits to one geometric surface pole.  Weld only face-local,
     * singularity-proven coincidences before constructing pole charts. */
    collapse_coincident_surface_poles(s_cdt);

    /* When a safely welded pole makes two distinct analytic seam edges share
     * both endpoints, prove their master curves coincide within modeling
     * tolerance and give them a common refinement sequence. */
    std::map<ON_3dPoint *, ON_3dPoint *> coincident_edge_welds;
    if (synchronize_coincident_edge_samples(s_cdt,
	    coincident_edge_welds)) {
	for (const auto &weld : coincident_edge_welds)
	    s_cdt->collapsed_edge_pnts[weld.first] = weld.second;
	apply_derived_point_welds(s_cdt, coincident_edge_welds);
    }

    // Rebuild finalized 2D RTrees for faces (needed for surface processing)
    finalize_rtrees(s_cdt);

    return true;
}

static bool
triangulate_face(struct ON_Brep_CDT_State *s_cdt, int face,
	double relative_scale = 1.0)
{
    const auto inconsistent = s_cdt->inconsistent_edge_faces.find(face);
    if (inconsistent != s_cdt->inconsistent_edge_faces.end()) {
	const std::string message = "face pullback for closed B-Rep edge " +
	    std::to_string(inconsistent->second.first) +
	    " misses the authoritative shared edge by " +
	    std::to_string(inconsistent->second.second);
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_GEOMETRIC_FAILED,
	    BREP_CDT_STAGE_GEOMETRIC_VALIDATION, face, 0, 1,
	    message.c_str());
    } else if (do_triangulation(s_cdt, face, NULL, relative_scale)) {
	s_cdt->failed_face_diagnostics.erase(face);
	return true;
    }

    if (s_cdt->diagnostic.face_index != face ||
	    s_cdt->diagnostic.result >= 0) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_FACE_FAILED,
	    BREP_CDT_STAGE_FACE_TRIANGULATION, face, 0, 1,
	    "face triangulation failed without a specific diagnostic");
    }
    s_cdt->failed_face_diagnostics[face] = s_cdt->diagnostic;
    return false;
}

static void
refine_failed_shared_chords(struct ON_Brep_CDT_State *s_cdt)
{
    size_t split_count = 0;
    for (int attempt = 0; attempt < CDT_MAX_CHART_REFINEMENT_ATTEMPTS;
	    ++attempt) {
	/* Both folded triangles and crossing constraints can be caused
	 * by fixed boundary chords. Subdivide only implicated chords,
	 * then rebuild both incident faces from the shared samples. */
	std::map<std::pair<int, double>, bedge_seg_t *> chords;
	for (const auto &failed : s_cdt->failed_face_diagnostics) {
	    cdt_mesh_t &mesh = s_cdt->fmeshes.at(failed.first);
	    const bool adaptive = failed.second.stage ==
		BREP_CDT_STAGE_ADAPTIVE_REFINEMENT;
	    /* Preserve the original partial mesh when a failure unrelated
	     * to the shared chords blocks closure. */
	    if (!adaptive && (failed.second.stage != BREP_CDT_STAGE_PSLG_VALIDATION ||
		    mesh.crossing_boundary_segments().empty()))
		return;
	    const auto segments = adaptive ? mesh.folded_boundary_segments() :
		mesh.crossing_boundary_segments();
	    for (bedge_seg_t *segment : segments)
		chords.emplace(std::make_pair(segment->edge_ind,
		    segment->edge_start), segment);
	}

	std::set<int> changed_faces;
	for (const auto &entry : chords) {
	    if (split_count >= CDT_MAX_CHART_REFINEMENT_POINTS)
		break;
	    bedge_seg_t *segment = entry.second;
	    if (!segment->tseg1 || !segment->tseg2)
		continue;
	    const ON_BrepTrim &first = s_cdt->brep->m_T[segment->tseg1->trim_ind];
	    const ON_BrepTrim &second = s_cdt->brep->m_T[segment->tseg2->trim_ind];
	    const int first_face = first.Face()->m_face_index;
	    const int second_face = second.Face()->m_face_index;
	    if (split_edge_seg(s_cdt, segment, 1, NULL, 0).empty())
		continue;
	    changed_faces.insert(first_face);
	    changed_faces.insert(second_face);
	    ++split_count;
	}
	if (changed_faces.empty())
	    break;
	finalize_rtrees(s_cdt);
	for (int face : changed_faces) {
	    cdt_mesh_t &mesh = s_cdt->fmeshes.at(face);
	    /* An early rebuild failure can leave the old triangle state
	     * referencing a trim segment destroyed by the split. */
	    mesh.ue2b_map.clear();
	    mesh.reset();
	    (void)triangulate_face(s_cdt, face);
	}
    }
}

static void
retry_deferred_chart_cleanup(struct ON_Brep_CDT_State *state)
{
    auto failure = state->failed_face_diagnostics.begin();
    while (failure != state->failed_face_diagnostics.end()) {
	const auto current = failure++;
	const int face = current->first;
	if (current->second.stage == BREP_CDT_STAGE_PSLG_VALIDATION &&
		!state->fmeshes.at(face).crossing_boundary_segments().empty())
	    (void)triangulate_face(state, face);
    }
}

static void
refine_folded_face_sampling(struct ON_Brep_CDT_State *state)
{
    if (state->tol.rel <= ON_ZERO_TOLERANCE)
	return;

    auto failure = state->failed_face_diagnostics.begin();
    while (failure != state->failed_face_diagnostics.end()) {
	const auto current = failure++;
	if (current->second.stage != BREP_CDT_STAGE_ADAPTIVE_REFINEMENT ||
		current->second.result != BREP_CDT_RESULT_REFINEMENT_LIMIT)
	    continue;

	const int face = current->first;
	cdt_mesh_t &mesh = state->fmeshes.at(face);
	if (!mesh.incorrect_normal_count() || !finalize_face_rtrees(state, face))
	    continue;

	/* A coarse interior can fold even when its shared boundary is
	 * already resolved. Rebuild the failed chart with finer interior
	 * samples while retaining the existing shared edges. */
	mesh.m_interior_pnts.clear();
	(void)triangulate_face(state, face, FACE_RESAMPLING_SCALE);
    }
}

static int
triangulate_selected_faces(struct ON_Brep_CDT_State *s_cdt,
	int face_cnt, int *faces, int &face_successes)
{
    const bool all_faces = !face_cnt || !faces;
    const int count = all_faces ? s_cdt->brep->m_F.Count() : face_cnt;
    const bool refine_shared = all_faces && !s_cdt->max_face_time_ms;
    s_cdt->defer_chart_cleanup = refine_shared;
    for (int i = 0; i < count; ++i) {
	const int face = all_faces ? i : faces[i];
	if (triangulate_face(s_cdt, face))
	    ++face_successes;
	else
	    s_cdt->failed_face_indices.push_back(face);
    }

    if (refine_shared && !s_cdt->failed_face_indices.empty())
	refine_failed_shared_chords(s_cdt);

    s_cdt->defer_chart_cleanup = false;
    if (refine_shared && !s_cdt->failed_face_indices.empty()) {
	retry_deferred_chart_cleanup(s_cdt);
	refine_folded_face_sampling(s_cdt);
	s_cdt->failed_face_indices.clear();
	for (const auto &failure : s_cdt->failed_face_diagnostics)
	    s_cdt->failed_face_indices.push_back(failure.first);
	face_successes = count - (int)s_cdt->failed_face_indices.size();
    }

    const int face_failures = (int)s_cdt->failed_face_indices.size();
    const int first_failed_face = face_failures ?
	s_cdt->failed_face_indices.front() : -1;
    if (face_failures)
	s_cdt->diagnostic = s_cdt->failed_face_diagnostics.at(
	    s_cdt->failed_face_indices.back());

    // If we only tessellated some of the faces, we don't have the
    // full solid mesh yet (by definition).  Return accordingly.
    if (face_failures || !face_successes || face_successes < s_cdt->brep->m_F.Count()) {
	const bool specific_failure =
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_CHART_FAILED ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_INVALID_PSLG ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_DETRIA_FAILED ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_CERTIFICATION_FAILED ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_REFINEMENT_LIMIT ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_GEOMETRIC_FAILED;
	if (face_successes) {
	    s_cdt->status = face_successes;
	    if (specific_failure) {
		s_cdt->diagnostic.completed_faces = face_successes;
		s_cdt->diagnostic.failed_faces = face_failures;
	    } else {
		cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_PARTIAL,
		    BREP_CDT_STAGE_FACE_TRIANGULATION, first_failed_face,
		    face_successes, face_failures,
		    "only a subset of B-Rep faces was triangulated");
	    }
	    return face_successes;
	}
	s_cdt->status = BREP_CDT_FAILED;
	if (specific_failure) {
	    s_cdt->diagnostic.completed_faces = 0;
	    s_cdt->diagnostic.failed_faces = face_failures;
	} else {
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_FACE_FAILED,
		BREP_CDT_STAGE_FACE_TRIANGULATION, first_failed_face,
		0, face_failures, "no B-Rep faces were triangulated");
	}
	return -1;
    }

    return 0;
}

static bool
assemble_face_meshes(cdt_mesh_output &output, struct ON_Brep_CDT_State *state)
{
    output.clear();
    int *faces = NULL;
    fastf_t *vertices = NULL;
    const int result = ON_Brep_CDT_Mesh(&faces, &output.face_count,
	&vertices, &output.vertex_count, NULL, NULL, NULL, NULL, state, 0, NULL);
    output.faces.reset(faces);
    output.vertices.reset(vertices);
    return result >= 0;
}

static int
resample_intersecting_faces(struct ON_Brep_CDT_State *state,
	const int (&faces)[2], std::set<int> &resampled, size_t &budget,
	double relative_scale = 1.0)
{
    const size_t initial_budget = budget;
    for (int face : faces) {
	if (!budget || !resampled.insert(face).second)
	    continue;
	const ON_Surface *surface = state->brep->m_F[face].SurfaceOf();
	if (!surface || surface->IsPlanar(NULL, ON_ZERO_TOLERANCE))
	    continue;

	/* A shared-edge split replaces trim segments. Refresh the query
	 * indexes before sampling this face again. */
	if (!finalize_face_rtrees(state, face) ||
		!do_triangulation(state, face, &budget, relative_scale)) {
	    state->status = BREP_CDT_FAILED;
	    cdt_diagnostic_set(state, BREP_CDT_RESULT_GEOMETRIC_FAILED,
		BREP_CDT_STAGE_GEOMETRIC_VALIDATION, face, 0, 1,
		"could not recertify an intersecting face after resampling");
	    return -1;
	}
    }
    return budget < initial_budget;
}

static bool
uses_periodic_seam_chart(const struct ON_Brep_CDT_State *state, int face)
{
    const auto mesh = state->fmeshes.find(face);
    if (mesh == state->fmeshes.end() || mesh->second.m_face_charts.empty())
	return false;
    /* A cap atlas already resolves the seam with model-space charts. */
    return mesh->second.m_face_charts.front().type() != CDT_FACE_CHART_SPHERE_CAP &&
	cdt_face_has_seam(state->brep->m_F[face]);
}

static int
certify_assembled_mesh(struct ON_Brep_CDT_State *s_cdt, int face_successes)
{
    cdt_mesh_output output;
    assembled_mesh_validation mesh_validation;
    size_t assembled_refinement_points = 0;
    int assembled_refinement_attempts = 0;
    /* Each retry certifies a whole mesh. Limit the additional passes
     * independently of the point budget. */
    const int maximum_resampling_rounds = 4;
    int resampling_rounds = 0;
    size_t resampling_budget = CDT_MAX_ASSEMBLED_REFINEMENT_POINTS;
    std::set<int> resampled;
    std::set<int> resampled_finer;
    while (true) {
	if (!assemble_face_meshes(output, s_cdt)) {
	    s_cdt->status = BREP_CDT_FAILED;
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_MESH_EXPORT_FAILED,
		BREP_CDT_STAGE_MESH_ASSEMBLY, -1, face_successes, 0,
		"failed to assemble indexed triangle mesh");
	    return -1;
	}
	if (assembled_mesh_validate(output.vertex_count, output.face_count,
		output.vertices.get(), output.faces.get(), &mesh_validation))
	    break;

	const bool only_intersection =
	    mesh_validation.intersecting_triangle_pairs &&
	    !mesh_validation.invalid_indices &&
	    !mesh_validation.nonfinite_vertices &&
	    !mesh_validation.unused_vertices &&
	    !mesh_validation.degenerate_faces &&
	    !mesh_validation.invalid_vertex_links;
	int intersection_faces[2] = {-1, -1};
	if (mesh_validation.intersecting_triangle_pairs) {
	    for (size_t i = 0; i < 2; ++i) {
		const int triangle = mesh_validation.first_intersection[i];
		if (triangle >= 0 && (size_t)triangle < s_cdt->bot_face_to_brep_face.size())
		    intersection_faces[i] = s_cdt->bot_face_to_brep_face[(size_t)triangle];
	    }
	}
	const bool have_intersection_faces =
	    intersection_faces[0] >= 0 && intersection_faces[1] >= 0;
	const bool can_resample = only_intersection && have_intersection_faces &&
	    !s_cdt->max_face_time_ms && resampling_budget &&
	    resampling_rounds < maximum_resampling_rounds;

	/* Trim-proximity filtering can leave periodic charts under-sampled.
	 * Admit the omitted interior samples before refining individual
	 * crossing chords between those charts. */
	if (can_resample && !assembled_refinement_attempts && resampled.empty() &&
		intersection_faces[0] != intersection_faces[1] &&
		uses_periodic_seam_chart(s_cdt, intersection_faces[0]) &&
		uses_periodic_seam_chart(s_cdt, intersection_faces[1])) {
	    const int sampled = resample_intersecting_faces(s_cdt,
		intersection_faces, resampled, resampling_budget);
	    if (sampled < 0)
		return -1;
	    if (sampled) {
		++resampling_rounds;
		continue;
	    }
	}

	size_t inserted = 0;
	if (mesh_validation.invalid_vertex_links &&
		assembled_refinement_attempts <
		CDT_MAX_ASSEMBLED_REFINEMENT_ATTEMPTS &&
		assembled_refinement_points <
		CDT_MAX_ASSEMBLED_REFINEMENT_POINTS) {
	    inserted = refine_assembled_shared_chords(s_cdt,
		output.vertex_count, output.face_count, output.faces.get(),
		CDT_MAX_ASSEMBLED_REFINEMENT_POINTS - assembled_refinement_points);
	}
	if (only_intersection && assembled_refinement_attempts <
		CDT_MAX_ASSEMBLED_REFINEMENT_ATTEMPTS &&
		assembled_refinement_points <
		CDT_MAX_ASSEMBLED_REFINEMENT_POINTS) {
	    inserted = refine_assembled_intersection(s_cdt, mesh_validation,
		CDT_MAX_ASSEMBLED_REFINEMENT_POINTS -
		assembled_refinement_points);
	}
	if (inserted) {
	    assembled_refinement_points += inserted;
	    assembled_refinement_attempts++;

	    continue;
	}

	if (can_resample) {
	    int sampled = resample_intersecting_faces(s_cdt,
		intersection_faces, resampled, resampling_budget);
	    /* The original spacing can add no new points. In that case try
	     * finer interior samples while retaining all shared boundaries. */
	    if (!sampled && s_cdt->tol.rel > ON_ZERO_TOLERANCE)
		sampled = resample_intersecting_faces(s_cdt,
		    intersection_faces, resampled_finer, resampling_budget,
		    FACE_RESAMPLING_SCALE);
	    if (sampled < 0)
		return -1;
	    if (sampled) {
		++resampling_rounds;
		continue;
	    }
	}

	std::string message =
	    "assembled mesh failed geometric/link validation: indices " +
	    std::to_string(mesh_validation.invalid_indices) +
	    ", nonfinite vertices " +
	    std::to_string(mesh_validation.nonfinite_vertices) +
	    ", unused vertices " +
	    std::to_string(mesh_validation.unused_vertices) +
	    ", degenerate faces " +
	    std::to_string(mesh_validation.degenerate_faces) +
	    ", invalid vertex links " +
	    std::to_string(mesh_validation.invalid_vertex_links) +
	    ", intersecting triangle pairs " +
	    std::to_string(mesh_validation.intersecting_triangle_pairs) +
	    (mesh_validation.intersecting_triangle_pairs ?
	    " (first " + std::to_string(
		mesh_validation.first_intersection[0]) + ", " +
		std::to_string(mesh_validation.first_intersection[1]) + ")" :
	    "");
	if (have_intersection_faces) {
	    message += " from B-Rep faces " + std::to_string(intersection_faces[0]) +
		", " + std::to_string(intersection_faces[1]);
	}

	if (assembled_refinement_points)
	    message += " after " + std::to_string(
		assembled_refinement_attempts) + " refinement rounds and " +
		std::to_string(assembled_refinement_points) +
		" inserted points";

	s_cdt->status = BREP_CDT_FAILED;
	cdt_diagnostic_set(s_cdt,
	    mesh_validation.intersecting_triangle_pairs ?
	    BREP_CDT_RESULT_GEOMETRIC_FAILED :
	    BREP_CDT_RESULT_CERTIFICATION_FAILED,
	    mesh_validation.intersecting_triangle_pairs ?
	    BREP_CDT_STAGE_GEOMETRIC_VALIDATION :
	    BREP_CDT_STAGE_SOLID_VALIDATION,
	    -1, face_successes, 0, message.c_str());
	return -1;
    }
    if (assembled_refinement_points)
	bu_log("%s: assembled mesh certified after %d cross-face refinement "
	    "rounds and %zu inserted points\n",
	    s_cdt->name ? s_cdt->name : "BREP",
	    assembled_refinement_attempts, assembled_refinement_points);

    struct bg_trimesh_solid_errors se = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    const int invalid = bg_trimesh_solid2(output.vertex_count,
	output.face_count, output.vertices.get(), output.faces.get(), &se);

    if (invalid) {
	trimesh_error_report(s_cdt, output.face_count, output.vertex_count,
	    output.faces.get(), output.vertices.get(), &se);
    }

    bg_free_trimesh_solid_errors(&se);

    if (invalid) {
	s_cdt->status = BREP_CDT_NON_SOLID;
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_NON_SOLID,
	    BREP_CDT_STAGE_SOLID_VALIDATION, -1, face_successes, 0,
	    "assembled mesh failed closed-solid validation");
	return 1;
    }

    s_cdt->certified = std::move(output);
    s_cdt->status = BREP_CDT_SOLID;
    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_SUCCESS,
	BREP_CDT_STAGE_SOLID_VALIDATION, -1, face_successes, 0,
	"closed indexed mesh passed incidence, link, and area validation");
    return 0;
}

int
brep_cdt_tessellate(struct ON_Brep_CDT_State *s_cdt, int face_cnt,
	int *faces, bool try_invalid_brep)
{
    if (!s_cdt || !s_cdt->orig_brep)
	return -1;

    /* A call is a new transaction.  Never retain a previous mesh after a
     * failed retry or mix it with newly requested faces. */
    cdt_state_reset(s_cdt);
    s_cdt->status = BREP_CDT_FAILED;
    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
	BREP_CDT_STAGE_INPUT, -1, 0, 0, "tessellation started");

    if (!validate_tessellation_input(s_cdt, face_cnt, faces, try_invalid_brep) ||
	!initialize_tessellation(s_cdt, try_invalid_brep))
	return -1;

    int face_successes = 0;
    const int result = triangulate_selected_faces(s_cdt, face_cnt, faces,
	face_successes);
    if (result)
	return result;
    return certify_assembled_mesh(s_cdt, face_successes);
}

int
ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s_cdt, int face_cnt,
	int *faces)
{
    if (s_cdt) {
	s_cdt->allow_bounded_edge_approximation = false;
	s_cdt->bounded_edge_approximation_tolerance = 0.0;
    }
    return brep_cdt_tessellate(s_cdt, face_cnt, faces, false);
}

static bool
cache_certified_normals(struct ON_Brep_CDT_State *s_cdt)
{
    if (!s_cdt || !s_cdt->certified.faces ||
	    s_cdt->certified.face_count <= 0 ||
	    !s_cdt->certified.vertices ||
	    s_cdt->certified.vertex_count <= 0)
	return false;
    if (s_cdt->certified.face_normals && s_cdt->certified.normals &&
	    s_cdt->certified.face_normal_count > 0 &&
	    s_cdt->certified.normal_count > 0)
	return true;

    if (s_cdt->certified.repaired) {
	std::vector<ON_3dVector> accumulated(
	    (size_t)s_cdt->certified.vertex_count, ON_3dVector::ZeroVector);
	std::vector<ON_3dVector> fallback(
	    (size_t)s_cdt->certified.vertex_count, ON_3dVector::ZeroVector);
	for (int face = 0; face < s_cdt->certified.face_count; ++face) {
	    int indices[3];
	    ON_3dPoint points[3];
	    for (int corner = 0; corner < 3; ++corner) {
		indices[corner] =
		    s_cdt->certified.faces[(size_t)face * 3 + corner];
		if (indices[corner] < 0 || indices[corner] >=
			s_cdt->certified.vertex_count)
		    return false;
		points[corner] = ON_3dPoint(
		    s_cdt->certified.vertices[(size_t)indices[corner] * 3],
		    s_cdt->certified.vertices[(size_t)indices[corner] * 3 + 1],
		    s_cdt->certified.vertices[(size_t)indices[corner] * 3 + 2]);
	    }
	    const ON_3dVector normal = ON_CrossProduct(points[1] - points[0],
		points[2] - points[0]);
	    if (!normal.IsValid() || !(normal.LengthSquared() > 0.0))
		return false;
	    for (int corner = 0; corner < 3; ++corner) {
		accumulated[(size_t)indices[corner]] += normal;
		if (!(fallback[(size_t)indices[corner]].LengthSquared() > 0.0))
		    fallback[(size_t)indices[corner]] = normal;
	    }
	}

	fastf_t *normals = (fastf_t *)bu_malloc(
	    (size_t)s_cdt->certified.vertex_count * 3 * sizeof(fastf_t),
	    "certified repaired normals");
	cdt_owned_array<fastf_t> owned_normals(normals);
	for (int vertex = 0; vertex < s_cdt->certified.vertex_count; ++vertex) {
	    ON_3dVector normal = accumulated[(size_t)vertex];
	    if (!normal.Unitize()) {
		normal = fallback[(size_t)vertex];
		if (!normal.Unitize()) {
		    return false;
		}
	    }
	    normals[(size_t)vertex * 3] = normal.x;
	    normals[(size_t)vertex * 3 + 1] = normal.y;
	    normals[(size_t)vertex * 3 + 2] = normal.z;
	}
	const size_t corner_count =
	    (size_t)s_cdt->certified.face_count * 3;
	int *face_normals = (int *)bu_malloc(corner_count * sizeof(int),
	    "certified repaired face normals");
	cdt_owned_array<int> owned_face_normals(face_normals);
	memcpy(face_normals, s_cdt->certified.faces.get(),
	    corner_count * sizeof(int));
	s_cdt->certified.face_normals = std::move(owned_face_normals);
	s_cdt->certified.face_normal_count = s_cdt->certified.face_count;
	s_cdt->certified.normals = std::move(owned_normals);
	s_cdt->certified.normal_count = s_cdt->certified.vertex_count;
	return true;
    }

    if (s_cdt->bot_face_to_brep_face.size() !=
	    (size_t)s_cdt->certified.face_count ||
	    s_cdt->bot_face_to_cdt_triangle.size() !=
	    (size_t)s_cdt->certified.face_count)
	return false;

    const size_t corner_count =
	(size_t)s_cdt->certified.face_count * 3;
    std::vector<ON_3dPoint *> corner_normals(corner_count, NULL);
    std::vector<ON_3dPoint *> ordered_normals;
    ordered_normals.reserve(s_cdt->w3dnorms->size());
    std::unordered_map<ON_3dPoint *, size_t> normal_order;
    normal_order.reserve(s_cdt->w3dnorms->size());
    std::unordered_set<ON_3dPoint *> flip_normals;

    for (int face = 0; face < s_cdt->certified.face_count; ++face) {
	const int brep_face = s_cdt->bot_face_to_brep_face[(size_t)face];
	const size_t triangle_index =
	    s_cdt->bot_face_to_cdt_triangle[(size_t)face];
	const auto face_mesh = s_cdt->fmeshes.find(brep_face);
	if (face_mesh == s_cdt->fmeshes.end() ||
		triangle_index >= face_mesh->second.tris_vect.size())
	    return false;
	const cdt_mesh_t &mesh = face_mesh->second;
	const triangle_t &triangle = mesh.tris_vect[triangle_index];
	for (int corner = 0; corner < 3; ++corner) {
	    const int output_vertex =
		s_cdt->certified.faces[(size_t)face * 3 + corner];
	    if (output_vertex < 0 || (size_t)output_vertex >=
		    s_cdt->bot_pnt_to_on_pnt->size())
		return false;
	    ON_3dPoint *source_point =
		(*s_cdt->bot_pnt_to_on_pnt)[(size_t)output_vertex];
	    if (!source_point)
		return false;
	    int local_corner = -1;
	    for (int candidate = 0; candidate < 3; ++candidate) {
		const long point_index = triangle.v[candidate];
		if (point_index >= 0 && (size_t)point_index < mesh.pnts.size() &&
			mesh.pnts[(size_t)point_index] == source_point) {
		    local_corner = candidate;
		    break;
		}
	    }
	    if (local_corner < 0)
		return false;

	    const long point_index = triangle.v[local_corner];
	    ON_3dPoint *normal = NULL;
	    const auto singular =
		s_cdt->singular_vert_to_norms->find(source_point);
	    if (singular != s_cdt->singular_vert_to_norms->end()) {
		normal = singular->second;
	    } else {
		const auto normal_map = mesh.nmap.find(point_index);
		if (normal_map == mesh.nmap.end() || normal_map->second < 0 ||
			(size_t)normal_map->second >= mesh.normals.size())
		    return false;
		normal = mesh.normals[(size_t)normal_map->second];
	    }
	    if (!normal)
		return false;
	    corner_normals[(size_t)face * 3 + corner] = normal;
	    if (normal_order.emplace(normal, normal_order.size()).second)
		ordered_normals.push_back(normal);
	    if (mesh.m_bRev)
		flip_normals.insert(normal);
	}
    }

    std::sort(ordered_normals.begin(), ordered_normals.end(),
	[&normal_order](ON_3dPoint *a, ON_3dPoint *b) {
	    if (a->x < b->x) return true;
	    if (b->x < a->x) return false;
	    if (a->y < b->y) return true;
	    if (b->y < a->y) return false;
	    if (a->z < b->z) return true;
	    if (b->z < a->z) return false;
	    return normal_order.at(a) < normal_order.at(b);
	});
    for (size_t i = 0; i < ordered_normals.size(); ++i)
	normal_order[ordered_normals[i]] = i;

    int *face_normals = (int *)bu_malloc(corner_count * sizeof(int),
	"certified face normals");
    cdt_owned_array<int> owned_face_normals(face_normals);
    fastf_t *normals = (fastf_t *)bu_malloc(ordered_normals.size() * 3 *
	sizeof(fastf_t), "certified normals");
    cdt_owned_array<fastf_t> owned_normals(normals);
    for (size_t i = 0; i < corner_count; ++i)
	face_normals[i] = (int)normal_order.at(corner_normals[i]);
    for (size_t i = 0; i < ordered_normals.size(); ++i) {
	ON_3dVector normal(*ordered_normals[i]);
	if (flip_normals.find(ordered_normals[i]) != flip_normals.end())
	    normal = -normal;
	normals[i * 3] = normal.x;
	normals[i * 3 + 1] = normal.y;
	normals[i * 3 + 2] = normal.z;
    }

    s_cdt->certified.face_normals = std::move(owned_face_normals);
    s_cdt->certified.face_normal_count = s_cdt->certified.face_count;
    s_cdt->certified.normals = std::move(owned_normals);
    s_cdt->certified.normal_count = (int)ordered_normals.size();
    return true;
}

// Generate a BoT with normals.
int
ON_Brep_CDT_Mesh(
	int **faces, int *fcnt,
	fastf_t **vertices, int *vcnt,
	int **face_normals, int *fn_cnt,
	fastf_t **normals, int *ncnt,
	struct ON_Brep_CDT_State *s_cdt,
	int exp_face_cnt, int *exp_faces)
{
    size_t triangle_cnt = 0;
    if (!faces || !fcnt || !vertices || !vcnt || !s_cdt) {
	return -1;
    }
    /* We can ignore the face normals if we want, but if some of the
     * return variables are non-NULL they all need to be non-NULL */
    if (face_normals || fn_cnt || normals || ncnt) {
	if (!face_normals || !fn_cnt || !normals || !ncnt) {
	    return -1;
	}
    }

    *faces = NULL;
    *fcnt = 0;
    *vertices = NULL;
    *vcnt = 0;
    if (face_normals) {
	*face_normals = NULL;
	*fn_cnt = 0;
	*normals = NULL;
	*ncnt = 0;
    }

    const bool full_mesh = !exp_face_cnt || !exp_faces;
    const bool no_normals = !face_normals && !fn_cnt && !normals && !ncnt;
    if (full_mesh && s_cdt->status == BREP_CDT_SOLID &&
	    s_cdt->certified.faces && s_cdt->certified.vertices &&
	    s_cdt->certified.face_count > 0 &&
	    s_cdt->certified.vertex_count > 0) {
	if (!no_normals && !cache_certified_normals(s_cdt))
	    return -1;
	const size_t face_bytes = (size_t)s_cdt->certified.face_count * 3 *
	    sizeof(int);
	const size_t vertex_bytes = (size_t)s_cdt->certified.vertex_count * 3 *
	    sizeof(fastf_t);
	*faces = (int *)bu_malloc(face_bytes, "cached certified faces");
	*vertices = (fastf_t *)bu_malloc(vertex_bytes,
	    "cached certified vertices");
	memcpy(*faces, s_cdt->certified.faces.get(), face_bytes);
	memcpy(*vertices, s_cdt->certified.vertices.get(), vertex_bytes);
	*fcnt = s_cdt->certified.face_count;
	*vcnt = s_cdt->certified.vertex_count;
	if (!no_normals) {
	    const size_t face_normal_bytes =
		(size_t)s_cdt->certified.face_normal_count * 3 * sizeof(int);
	    const size_t normal_bytes =
		(size_t)s_cdt->certified.normal_count * 3 * sizeof(fastf_t);
	    *face_normals = (int *)bu_malloc(face_normal_bytes,
		"cached certified face normals");
	    *normals = (fastf_t *)bu_malloc(normal_bytes,
		"cached certified normals");
	    memcpy(*face_normals, s_cdt->certified.face_normals.get(),
		face_normal_bytes);
	    memcpy(*normals, s_cdt->certified.normals.get(), normal_bytes);
	    *fn_cnt = s_cdt->certified.face_normal_count;
	    *ncnt = s_cdt->certified.normal_count;
	}
	return 0;
    }
    if (!s_cdt->brep)
	return -1;

    s_cdt->bot_face_to_brep_face.clear();
    s_cdt->bot_face_to_cdt_triangle.clear();

    std::vector<int> active_faces;
    if (!exp_face_cnt || !exp_faces) {
	for (int face_index = 0; face_index < s_cdt->brep->m_F.Count(); face_index++) {
	    active_faces.push_back(face_index);
	}
    } else {
	for (int i = 0; i < exp_face_cnt; i++) {
	    if (exp_faces[i] < 0 || exp_faces[i] >= s_cdt->brep->m_F.Count())
		return -1;
	    active_faces.push_back(exp_faces[i]);
	}
    }
    std::sort(active_faces.begin(), active_faces.end());
    active_faces.erase(std::unique(active_faces.begin(), active_faces.end()),
	active_faces.end());

    /* RTree traversal order is an implementation detail.  Snapshot active
     * triangle IDs and order them by stable face and creation IDs. */
    std::map<int, std::vector<size_t>> active_triangles;
    for (size_t fi = 0; fi < active_faces.size(); fi++) {
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[active_faces[fi]];
	RTree<size_t, double, 3>::Iterator tree_it;
	fmesh->tris_tree.GetFirst(tree_it);
	while (!tree_it.IsNull()) {
	    active_triangles[active_faces[fi]].push_back(*tree_it);
	    ++tree_it;
	}
	std::sort(active_triangles[active_faces[fi]].begin(),
	    active_triangles[active_faces[fi]].end());
	triangle_cnt += active_triangles[active_faces[fi]].size();
    }
    if (!triangle_cnt || triangle_cnt > INT_MAX)
	return -1;

    /* We know now the final triangle set.  We need to build up the set of
     * unique points and normals to generate a mesh containing only the
     * information actually used by the final triangle set. */
    typedef std::pair<ON_3dPoint *, size_t> ordered_point_entry;
    std::vector<ordered_point_entry> ordered_points;
    std::vector<ordered_point_entry> ordered_normals;
    std::unordered_set<ON_3dPoint *> point_seen;
    std::unordered_set<ON_3dPoint *> normal_seen;
    std::unordered_set<ON_3dPoint *> flip_normals;
    const size_t max_corner_count = triangle_cnt > SIZE_MAX / 3 ?
	SIZE_MAX : triangle_cnt * 3;
    point_seen.reserve(std::min(max_corner_count, s_cdt->w3dpnts->size()));
    ordered_points.reserve(std::min(max_corner_count,
	s_cdt->w3dpnts->size()));
    if (normals) {
	normal_seen.reserve(std::min(max_corner_count,
	    s_cdt->w3dnorms->size()));
	ordered_normals.reserve(std::min(max_corner_count,
	    s_cdt->w3dnorms->size()));
    }
    for (size_t fi = 0; fi < active_faces.size(); fi++) {
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[active_faces[fi]];
	const std::vector<size_t> &face_triangles =
	    active_triangles[active_faces[fi]];
	for (size_t ti = 0; ti < face_triangles.size(); ++ti) {
	    triangle_t tri = fmesh->tris_vect[face_triangles[ti]];
	    for (size_t j = 0; j < 3; j++) {
		ON_3dPoint *p3d = fmesh->pnts[tri.v[j]];
		if (point_seen.insert(p3d).second)
		    ordered_points.push_back(std::make_pair(p3d,
			ordered_points.size()));
		if (normals) {
		    ON_3dPoint *onorm = NULL;
		    if (s_cdt->singular_vert_to_norms->find(p3d) !=
			    s_cdt->singular_vert_to_norms->end()) {
			// Use calculated normal for singularity points
			onorm = (*s_cdt->singular_vert_to_norms)[p3d];
		    } else {
			onorm = fmesh->normals[fmesh->nmap[tri.v[j]]];
		    }
		    if (onorm) {
			if (normal_seen.insert(onorm).second)
			    ordered_normals.push_back(std::make_pair(onorm,
				ordered_normals.size()));
			if (fmesh->m_bRev)
			    flip_normals.insert(onorm);
		    }
		}
	    }
	}
    }

    //bu_log("tri_cnt: %zd\n", triangle_cnt);

    const auto stable_point_less = [](const ordered_point_entry &a_entry,
	    const ordered_point_entry &b_entry) {
	ON_3dPoint *a = a_entry.first;
	ON_3dPoint *b = b_entry.first;
	if (a->x < b->x) return true;
	if (b->x < a->x) return false;
	if (a->y < b->y) return true;
	if (b->y < a->y) return false;
	if (a->z < b->z) return true;
	if (b->z < a->z) return false;
	return a_entry.second < b_entry.second;
    };
    const auto stable_normal_less = [](const ordered_point_entry &a_entry,
	    const ordered_point_entry &b_entry) {
	ON_3dPoint *a = a_entry.first;
	ON_3dPoint *b = b_entry.first;
	if (a->x < b->x) return true;
	if (b->x < a->x) return false;
	if (a->y < b->y) return true;
	if (b->y < a->y) return false;
	if (a->z < b->z) return true;
	if (b->z < a->z) return false;
	return a_entry.second < b_entry.second;
    };
    std::sort(ordered_points.begin(), ordered_points.end(), stable_point_less);
    std::sort(ordered_normals.begin(), ordered_normals.end(),
	stable_normal_less);

    if (ordered_points.empty() || ordered_points.size() > INT_MAX ||
	(normals && (ordered_normals.empty() ||
	ordered_normals.size() > INT_MAX)))
	return -1;

    /* Release collection hashes before allocating the final arrays and
     * pointer-to-index maps.  On million-triangle meshes, retaining both
     * generations at once needlessly adds hundreds of MiB to peak use. */
    std::unordered_set<ON_3dPoint *>().swap(point_seen);
    std::unordered_set<ON_3dPoint *>().swap(normal_seen);

    // We know how many faces, points and normals we need now - initialize BoT containers.
    *fcnt = (int)triangle_cnt;
    *faces = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new faces array");
    *vcnt = (int)ordered_points.size();
    *vertices = (fastf_t *)bu_calloc(ordered_points.size()*3,
	sizeof(fastf_t), "new vert array");
    if (normals) {
	*ncnt = (int)ordered_normals.size();
	*normals = (fastf_t *)bu_calloc(ordered_normals.size()*3,
	    sizeof(fastf_t), "new normals array");
	*fn_cnt = (int)triangle_cnt;
	*face_normals = (int *)bu_calloc(triangle_cnt*3, sizeof(int),
	    "new face_normals array");
    }

    // Index vertex points and assign them to the BoT array
    std::unordered_map<ON_3dPoint *, int> on_pnt_to_bot_pnt;
    on_pnt_to_bot_pnt.reserve(ordered_points.size());
    s_cdt->bot_pnt_to_on_pnt->assign(ordered_points.size(), NULL);
    int pnt_ind = 0;
    for (size_t pi = 0; pi < ordered_points.size(); ++pi) {
	ON_3dPoint *vp = ordered_points[pi].first;
	(*vertices)[pnt_ind*3] = vp->x;
	(*vertices)[pnt_ind*3+1] = vp->y;
	(*vertices)[pnt_ind*3+2] = vp->z;
	on_pnt_to_bot_pnt.emplace(vp, pnt_ind);
	(*s_cdt->bot_pnt_to_on_pnt)[(size_t)pnt_ind] = vp;
	pnt_ind++;
    }

    // Index vertex normal vectors and assign them to the BoT array.  Normal
    // vectors are not always uniquely mapped to vertices (consider, for
    // example, the triangles joining at a sharp edge of a box), but what we
    // are doing here is establishing unique integer identifiers for all normal
    // vectors.  In other words, this is not a unique vertex to normal mapping,
    // but a normal vector to unique index mapping.
    //
    // The mapping of 2D triangle point to its associated normal is the
    // responsibility of the  p2t_to_on3_norm_map container
    std::unordered_map<ON_3dPoint *, int> on_norm_to_bot_norm;
    size_t norm_ind = 0;
    if (normals) {
	on_norm_to_bot_norm.reserve(ordered_normals.size());
	for (size_t ni = 0; ni < ordered_normals.size(); ++ni) {
	    ON_3dPoint *vn = ordered_normals[ni].first;
	    ON_3dVector vnf(*vn);
	    if (flip_normals.find(vn) != flip_normals.end()) {
		vnf = -1 *vnf;
	    }
	    (*normals)[norm_ind*3] = vnf.x;
	    (*normals)[norm_ind*3+1] = vnf.y;
	    (*normals)[norm_ind*3+2] = vnf.z;
	    on_norm_to_bot_norm.emplace(vn, (int)norm_ind);
	    norm_ind++;
	}
    }

    // Iterate over faces, adding points and faces to BoT container.  Note: all
    // 3D points should be geometrically unique in this final container.
    int face_cnt = 0;
    std::vector<int> output_face_ids;
    std::vector<size_t> output_triangle_ids;
    output_face_ids.reserve(triangle_cnt);
    output_triangle_ids.reserve(triangle_cnt);
    for (size_t fi = 0; fi < active_faces.size(); fi++) {
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[active_faces[fi]];
	const std::vector<size_t> &face_triangles =
	    active_triangles[active_faces[fi]];
	for (size_t ti = 0; ti < face_triangles.size(); ++ti) {
	    triangle_t tri = fmesh->tris_vect[face_triangles[ti]];
	    for (size_t j = 0; j < 3; j++) {
		ON_3dPoint *op = fmesh->pnts[tri.v[j]];
		const auto output_point = on_pnt_to_bot_pnt.find(op);
		if (output_point == on_pnt_to_bot_pnt.end())
		    return -1;
		(*faces)[face_cnt*3 + j] = output_point->second;

		if (normals) {
		    ON_3dPoint *onorm;
		    if (s_cdt->singular_vert_to_norms->find(op) != s_cdt->singular_vert_to_norms->end()) {
			// Use calculated normal for singularity points
			onorm = (*s_cdt->singular_vert_to_norms)[op];
		    } else {
			onorm = fmesh->normals[fmesh->nmap[fmesh->p2ind[op]]];
		    }

		    const auto output_normal =
			on_norm_to_bot_norm.find(onorm);
		    if (output_normal == on_norm_to_bot_norm.end())
			return -1;
		    (*face_normals)[face_cnt*3 + j] =
			output_normal->second;
		}
	    }

	    output_face_ids.push_back(fmesh->f_id);
	    output_triangle_ids.push_back(face_triangles[ti]);
	    face_cnt++;
	}
    }

    /* A complete, topologically solid BREP supplies authoritative shell
     * orientation.  If tessellation produced an otherwise closed/manifold
     * mesh with inconsistent triangle winding, repair that winding only when
     * the fully synchronized candidate validates as a solid.  Partial/open
     * meshes retain their original per-face ordering. */
    if (!exp_face_cnt || !exp_faces) {
	const bool source_is_solid = s_cdt->orig_brep &&
	    s_cdt->orig_brep->IsSolid();
	const int removed = closed_mesh_component_filter(*faces, fcnt,
	    *vertices, *vcnt, face_normals ? *face_normals : NULL,
	    output_face_ids.data(), std::max(s_cdt->absmin,
		ON_ZERO_TOLERANCE), source_is_solid, true,
	    output_triangle_ids.data());
	if (removed > 0) {
	    bu_log("%s: removed %d redundant open triangle artifact%s after "
		"transactional component validation\n",
		s_cdt->name ? s_cdt->name : "BREP", removed,
		removed == 1 ? "" : "s");
	}
	if (source_is_solid) {
	    const int synchronized = closed_mesh_orientation_sync(*faces, *fcnt,
		*vertices, *vcnt, face_normals ? *face_normals : NULL);
	    if (synchronized > 0) {
		bu_log("%s: synchronized %d triangle orientations after complete closed-mesh validation\n",
		    s_cdt->name ? s_cdt->name : "BREP", synchronized);
	    }
	}

	/* Component filtering intentionally keeps vertex indices stable while it
	 * edits the face array.  Once that transaction and orientation repair are
	 * complete, compact the exported vertex array and its source-point map so
	 * rigorous assembled validation does not mistake legal stale BoT entries
	 * for a geometric failure. */
	std::vector<int> vertex_remap((size_t)*vcnt, -1);
	for (int face = 0; face < *fcnt; ++face) {
	    for (int corner = 0; corner < 3; ++corner) {
		const int vertex = (*faces)[(size_t)face * 3 + corner];
		if (vertex >= 0 && vertex < *vcnt)
		    vertex_remap[(size_t)vertex] = 0;
	    }
	}
	/* Preserve the original vertex order so in-place copies only move
	 * toward the beginning of the array.  Triangle encounter order can
	 * otherwise overwrite coordinates that have not yet been copied. */
	int compact_vertex_count = 0;
	for (int vertex = 0; vertex < *vcnt; ++vertex) {
	    if (vertex_remap[(size_t)vertex] >= 0)
		vertex_remap[(size_t)vertex] = compact_vertex_count++;
	}
	if (compact_vertex_count < *vcnt) {
	    std::vector<ON_3dPoint *> compact_point_map(
		(size_t)compact_vertex_count, NULL);
	    for (int old_vertex = 0; old_vertex < *vcnt; ++old_vertex) {
		const int new_vertex = vertex_remap[(size_t)old_vertex];
		if (new_vertex < 0)
		    continue;
		for (int axis = 0; axis < 3; ++axis)
		    (*vertices)[(size_t)new_vertex * 3 + axis] =
			(*vertices)[(size_t)old_vertex * 3 + axis];
		if ((size_t)old_vertex < s_cdt->bot_pnt_to_on_pnt->size())
		    compact_point_map[(size_t)new_vertex] =
			(*s_cdt->bot_pnt_to_on_pnt)[(size_t)old_vertex];
	    }
	    for (int face = 0; face < *fcnt; ++face) {
		for (int corner = 0; corner < 3; ++corner) {
		    int &vertex = (*faces)[(size_t)face * 3 + corner];
		    vertex = vertex_remap[(size_t)vertex];
		}
	    }
	    s_cdt->bot_pnt_to_on_pnt->swap(compact_point_map);
	    *vcnt = compact_vertex_count;
	}
    }

    output_face_ids.resize((size_t)*fcnt);
    output_triangle_ids.resize((size_t)*fcnt);
    if (fn_cnt)
	*fn_cnt = *fcnt;
    s_cdt->bot_face_to_brep_face = output_face_ids;
    s_cdt->bot_face_to_cdt_triangle = output_triangle_ids;

    return 0;
}

static bool
compacted_export_preserves_geometry(void)
{
    /* A face-local triangle collapses during export because two of its
     * vertices share a source point.  Removing it leaves an unused vertex
     * before the tetrahedron's vertices in the sorted output array. */
    ON_3dPoint points[] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(0.0, 1.0, 0.0), ON_3dPoint(0.0, 0.0, 1.0),
	ON_3dPoint(-1.0, 0.0, 0.0)
    };
    ON_3dPoint source_normals[] = {
	ON_3dPoint(1.0, 0.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(0.0, 0.0, -1.0),
	ON_3dPoint(0.0, -1.0, 0.0)
    };
    const int triangles[][3] = {
	{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}, {4, 5, 1}
    };
    ON_Brep brep;
    brep.m_F.AppendNew();
    ON_Brep_CDT_State state = {};
    state.status = BREP_CDT_UNTESSELLATED;
    state.brep = &brep;
    cdt_mesh_t &mesh = state.fmeshes[0];
    mesh.f_id = 0;
    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
	mesh.add_point(&points[i]);
	mesh.add_normal(&source_normals[i]);
	mesh.nmap[i] = i;
    }
    mesh.add_point(&points[4]);
    mesh.nmap[5] = 4;
    for (size_t i = 0; i < sizeof(triangles) / sizeof(triangles[0]); ++i) {
	triangle_t triangle;
	for (int corner = 0; corner < 3; ++corner)
	    triangle.v[corner] = triangles[i][corner];
	if (!mesh.tri_add(triangle))
	    return false;
    }
    std::vector<ON_3dPoint *> source_points;
    std::map<ON_3dPoint *, ON_3dPoint *> singular_normals;
    state.w3dpnts = &mesh.pnts;
    state.w3dnorms = &mesh.normals;
    state.bot_pnt_to_on_pnt = &source_points;
    state.singular_vert_to_norms = &singular_normals;

    /* Repeated exports must preserve the source as well as the coordinates,
     * corner normals, and provenance of every retained triangle. */
    for (int attempt = 0; attempt < 2; ++attempt) {
	int *faces = NULL, *face_normals = NULL;
	fastf_t *vertices = NULL, *normals = NULL;
	int face_count = 0, vertex_count = 0;
	int face_normal_count = 0, normal_count = 0;
	bool valid = ON_Brep_CDT_Mesh(&faces, &face_count, &vertices,
	    &vertex_count, &face_normals, &face_normal_count, &normals,
	    &normal_count, &state, 0, NULL) == 0;
	valid = valid && face_count == 4 && vertex_count == 4 &&
	    face_normal_count == face_count && source_points.size() == 4 &&
	    state.bot_face_to_brep_face.size() == 4 &&
	    state.bot_face_to_cdt_triangle.size() == 4;
	for (int face = 0; valid && face < face_count; ++face) {
	    valid = state.bot_face_to_brep_face[(size_t)face] == 0 &&
		state.bot_face_to_cdt_triangle[(size_t)face] == (size_t)face;
	    for (int corner = 0; valid && corner < 3; ++corner) {
		const int vertex = faces[face * 3 + corner];
		const int normal = face_normals[face * 3 + corner];
		const ON_3dPoint &expected = points[triangles[face][corner]];
		const ON_3dPoint &expected_normal =
		    source_normals[triangles[face][corner]];
		valid = vertex >= 0 && vertex < vertex_count &&
		    normal >= 0 && normal < normal_count &&
		    source_points[(size_t)vertex] == &expected;
		for (int axis = 0; valid && axis < 3; ++axis)
		    valid = NEAR_EQUAL(vertices[vertex * 3 + axis],
			expected[axis], ON_ZERO_TOLERANCE) &&
			NEAR_EQUAL(normals[normal * 3 + axis],
			expected_normal[axis], ON_ZERO_TOLERANCE);
	    }
	}
	bu_free(faces, "test exported faces");
	bu_free(vertices, "test exported vertices");
	bu_free(face_normals, "test exported face normals");
	bu_free(normals, "test exported normals");
	if (!valid)
	    return false;
    }
    return true;
}

int
cdt_test_spurious_components(void)
{
    fastf_t vertices[] = {
	0.0, 0.0, 0.0,
	1.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 1.0,
	2.0, 0.0, 0.0,
	3.0, 0.0, 0.0,
	2.0, 1.0, 0.0,
	4.0, 0.0, 0.0
    };
    int tetra_and_open[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	4, 5, 6
    };
    int face_count = 5;
    if (closed_mesh_component_filter(tetra_and_open, &face_count, vertices,
	    8, NULL) != 0 || face_count != 5)
	return 1;

    int two_open_components[] = {0, 1, 2, 4, 5, 6};
    face_count = 2;
    if (closed_mesh_component_filter(two_open_components, &face_count,
	    vertices, 8, NULL) != 0 || face_count != 2)
	return 2;

    fastf_t two_tetra_vertices[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
	3.0, 0.0, 0.0, 4.0, 0.0, 0.0,
	3.0, 1.0, 0.0, 3.0, 0.0, 1.0
    };
    int two_tetra_faces[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	4, 6, 5, 4, 5, 7, 4, 7, 6, 5, 6, 7
    };
    face_count = 8;
    if (closed_mesh_component_filter(two_tetra_faces, &face_count,
	    two_tetra_vertices, 8, NULL) != 0 || face_count != 8)
	return 3;

    fastf_t open_duplicate_vertices[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	1.0, 1.0, 0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	1.0, 1.0, 0.0
    };
    int open_duplicate_faces[] = {
	0, 1, 2, 0, 2, 3, 4, 5, 6
    };
    int open_duplicate_sources[] = {7, 7, 7};
    size_t open_duplicate_triangles[] = {10, 11, 12};
    face_count = 3;
    if (closed_mesh_component_filter(open_duplicate_faces, &face_count,
	    open_duplicate_vertices, 7, NULL, open_duplicate_sources,
	    1.0e-12, true, true, open_duplicate_triangles) != 1 ||
	    face_count != 2 || open_duplicate_triangles[0] != 10 ||
	    open_duplicate_triangles[1] != 11)
	return 4;

    open_duplicate_vertices[20] = 0.1;
    int distinct_faces[] = {0, 1, 2, 0, 2, 3, 4, 5, 6};
    face_count = 3;
    if (closed_mesh_component_filter(distinct_faces, &face_count,
	    open_duplicate_vertices, 7, NULL, open_duplicate_sources,
	    1.0e-6, true, false) != 0 || face_count != 3)
	return 5;

    /* An open triangle attached to a closed shell through an edge with three
     * uses is not enough by itself to prove redundancy.  Without source-face
     * coverage metadata the cleanup must preserve the candidate and let the
     * rigorous topology audit reject it. */
    fastf_t nonmanifold_vertices[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
	1.0, 0.0, 0.0
    };
    int nonmanifold_faces[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	0, 2, 4
    };
    face_count = 5;
    if (closed_mesh_component_filter(nonmanifold_faces, &face_count,
	    nonmanifold_vertices, 5, NULL) != 0 || face_count != 5)
	return 6;

    /* Face-local vertices can collapse to one shared output vertex.  The
     * resulting index-degenerate triangle must not survive into a BoT or
     * distort component discovery. */
    int degenerate_faces[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	4, 4, 1
    };
    face_count = 5;
    if (closed_mesh_component_filter(degenerate_faces, &face_count,
	    nonmanifold_vertices, 5, NULL) != 1 || face_count != 4)
	return 7;

    int invalid_faces[] = {0, 1, 2, 3, 3, 99};
    const int invalid_original[] = {0, 1, 2, 3, 3, 99};
    face_count = 2;
    if (closed_mesh_component_filter(invalid_faces, &face_count,
	    nonmanifold_vertices, 5, NULL) != 0 || face_count != 2 ||
	    !std::equal(invalid_faces, invalid_faces + 6, invalid_original))
	return 8;

    if (!compacted_export_preserves_geometry())
	return 9;

    return 0;
}

// TODO - need a function that not only checks the integrity of
// each face mesh, but also validates all the boundary edge related
// information needed to stitch them together.  This is beyond the
// scope of the face mesh validity routines, so we'll have to do it
// at the CDT state level.
bool
CDT_Audit(struct ON_Brep_CDT_State *s_cdt)
{
    bool ret = true;
    if (!s_cdt) return false;

    std::map<int, std::set<bedge_seg_t *>>::iterator ps_it;

    int bedge_cnt = 0;

    for (ps_it = s_cdt->e2polysegs.begin(); ps_it != s_cdt->e2polysegs.end(); ps_it++) {
	/* A mesh-only collapsed edge has no triangle edge by construction. */
	if (s_cdt->collapsed_edges.find(ps_it->first) !=
		s_cdt->collapsed_edges.end())
	    continue;
	std::set<bedge_seg_t *>::iterator b_it;
	for (b_it = ps_it->second.begin(); b_it != ps_it->second.end(); b_it++) {
	    bedge_seg_t *eseg = *b_it;
	    std::vector<std::pair<cdt_mesh_t *,uedge_t>> uedges = eseg->uedges();
	    cdt_mesh_t *fmesh_f1 = uedges[0].first;
	    cdt_mesh_t *fmesh_f2 = uedges[1].first;
	    uedge_t ue1 = uedges[0].second;
	    uedge_t ue2 = uedges[1].second;
	    std::set<size_t> f1_tris = fmesh_f1->uedges2tris[ue1];
	    std::set<size_t> f2_tris = fmesh_f2->uedges2tris[ue2];

	    bool bad_tris = false;

	    bad_tris = (bad_tris || ((fmesh_f1->f_id != fmesh_f2->f_id) && (f1_tris.size() != 1 || f2_tris.size() != 1)));
	    bad_tris = (bad_tris || ((fmesh_f1->f_id == fmesh_f2->f_id) && ((f1_tris.size() != 0 && f1_tris.size() != 2) || (f2_tris.size() != 0 && f2_tris.size() != 2) )));
	    bad_tris = (bad_tris || (!f1_tris.size() && !f2_tris.size()));

	    if (bad_tris) {
		if (fmesh_f1->f_id == fmesh_f2->f_id) {
		    if (f1_tris.size() == 1 || (!f1_tris.size() && !f2_tris.size())) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f1->name << "," << fmesh_f1->f_id << "\n";
			std::cerr << "uedge: " << ue1.v[0] << "," << ue1.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f1->f_id) + std::string(".plot3");
			fmesh_f1->tris_plot(fpname.c_str());
		    }
		    if (f2_tris.size() == 1 || (!f1_tris.size() && !f2_tris.size())) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f2->name << "," << fmesh_f2->f_id  << "\n";
			std::cerr << "uedge: " << ue2.v[0] << "," << ue2.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f2->f_id) + std::string(".plot3");
			fmesh_f2->tris_plot(fpname.c_str());
		    }
		} else {
		    if (f1_tris.size() == 0) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f1->name << "," << fmesh_f1->f_id << "\n";
			std::cerr << "uedge: " << ue1.v[0] << "," << ue1.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f1->f_id) + std::string(".plot3");
			fmesh_f1->tris_plot(fpname.c_str());
		    }
		    if (f2_tris.size() == 0) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f2->name << "," << fmesh_f2->f_id  << "\n";
			std::cerr << "uedge: " << ue2.v[0] << "," << ue2.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f2->f_id) + std::string(".plot3");
			fmesh_f2->tris_plot(fpname.c_str());
		    }
		}
		std::string ename = std::string(s_cdt->name) + std::string("_edge_") + std::to_string(bedge_cnt) + std::string(".plot3");
		eseg->plot(ename.c_str());
		ret = false;
	    }
	    bedge_cnt++;
	}
    }

    return ret;
}

/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
