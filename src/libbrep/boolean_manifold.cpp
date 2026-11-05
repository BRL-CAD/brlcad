/*          B O O L E A N _ M A N I F O L D . C P P
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
/** @file boolean_manifold.cpp
 *
 * Conservative Manifold classification of exact Boolean split-face proxies.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

#include <manifold/manifold.h>

#include "bu/malloc.h"
#include "brep/cdt.h"
#include "boolean_manifold.h"


namespace {

const double AREA_RELATIVE_TOLERANCE = 1.0e-6;
const double AREA_ROUNDOFF_MULTIPLIER = 128.0;
const double POLYGON_ROUNDOFF_MULTIPLIER = 256.0;
const double WINDING_COSINE_MINIMUM = 1.0e-12;
const double CDT_PROXY_RELATIVE_TOLERANCE = 1.0e-3;
const long CDT_PROXY_FACE_TIME_LIMIT_MS = 1000;
const int CDT_PROXY_MAX_FACES = 128;
const size_t CDT_PROXY_MAX_TRIANGLES = 4u * 1024u * 1024u;


struct RegionMeasure {
    double area = 0.0;
    bool saw_forward = false;
    bool saw_reverse = false;
};


struct ProxyMesh {
    manifold::MeshGL64 mesh;
    std::vector<RegionMeasure> regions;
    bool used_cdt = false;
};


struct CDTMeshArrays {
    int *faces = NULL;
    fastf_t *vertices = NULL;
    int face_count = 0;
    int vertex_count = 0;

    ~CDTMeshArrays()
    {
	if (faces)
	    bu_free(faces, "Boolean Manifold CDT faces");
	if (vertices)
	    bu_free(vertices, "Boolean Manifold CDT vertices");
    }
};


static bool
triangle_area(const manifold::MeshGL64 &mesh, size_t triangle, double &area)
{
    area = 0.0;
    if (mesh.numProp < 3 || triangle >= mesh.NumTri()) {
	return false;
    }

    ON_3dPoint points[3];
    for (size_t corner = 0; corner < 3; ++corner) {
	const uint64_t vertex = mesh.triVerts[3 * triangle + corner];
	if (vertex >= mesh.NumVert()) {
	    return false;
	}
	const size_t offset = static_cast<size_t>(vertex) * mesh.numProp;
	points[corner] = ON_3dPoint(mesh.vertProperties[offset],
	    mesh.vertProperties[offset + 1], mesh.vertProperties[offset + 2]);
	if (!points[corner].IsValid()) {
	    return false;
	}
    }

    const ON_3dVector first = points[1] - points[0];
    const ON_3dVector second = points[2] - points[0];
    area = 0.5 * ON_CrossProduct(first, second).Length();
    return std::isfinite(area);
}


static bool
face_failure(std::string &detail, int face_index, const char *message)
{
    std::ostringstream stream;
    stream << "split operand proxy face " << face_index << " " << message;
    detail = stream.str();
    return false;
}


static bool
append_planar_face(ProxyMesh &proxy, const ON_Brep &brep, int face_index,
    std::string &detail)
{
    const ON_BrepFace &face = brep.m_F[face_index];
    const ON_Surface *surface = face.SurfaceOf();
    ON_Plane plane;
    if (surface == NULL ||
	!surface->IsPlanar(&plane, ON_ZERO_TOLERANCE) || !plane.IsValid())
    {
	return face_failure(detail, face_index,
	    "is not supported by the certified planar proxy path");
    }

    ON_3dVector surface_normal = plane.Normal();
    if (!surface_normal.IsValid() || !surface_normal.Unitize()) {
	return face_failure(detail, face_index,
	    "has no reliable planar surface normal");
    }

    const ON_BrepLoop *loop = face.OuterLoop();
    if (face.LoopCount() != 1 || loop == NULL ||
	loop->m_type != ON_BrepLoop::outer || loop->TrimCount() < 3 ||
	loop->TrimCount() > 16)
    {
	return face_failure(detail, face_index,
	    "is not a bounded single-loop polygon");
    }

    std::vector<uint64_t> polygon;
    std::vector<ON_3dPoint> points;
    polygon.reserve(static_cast<size_t>(loop->TrimCount()));
    points.reserve(static_cast<size_t>(loop->TrimCount()));
    int previous_end = -1;
    for (int trim_index = 0; trim_index < loop->TrimCount(); ++trim_index) {
	const ON_BrepTrim *trim = loop->Trim(trim_index);
	const ON_Curve *trim_curve = trim ? trim->TrimCurveOf() : NULL;
	const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	const ON_Curve *edge_curve = edge ? edge->EdgeCurveOf() : NULL;
	if (trim == NULL || trim_curve == NULL || edge == NULL ||
	    edge_curve == NULL ||
	    !trim_curve->IsLinear(ON_ZERO_TOLERANCE) ||
	    !edge_curve->IsLinear(ON_ZERO_TOLERANCE))
	{
	    return face_failure(detail, face_index,
		"has a non-polygonal boundary");
	}
	if (trim->m_vi[0] < 0 || trim->m_vi[0] >= brep.m_V.Count() ||
	    trim->m_vi[1] < 0 || trim->m_vi[1] >= brep.m_V.Count() ||
	    trim->m_vi[0] == trim->m_vi[1])
	{
	    return face_failure(detail, face_index,
		"has an invalid or collapsed boundary edge");
	}
	const int expected_start = edge->m_vi[trim->m_bRev3d ? 1 : 0];
	const int expected_end = edge->m_vi[trim->m_bRev3d ? 0 : 1];
	if (trim->m_vi[0] != expected_start || trim->m_vi[1] != expected_end ||
	    (trim_index > 0 && trim->m_vi[0] != previous_end))
	{
	    return face_failure(detail, face_index,
		"has inconsistent ordered trim topology");
	}
	for (size_t existing = 0; existing < polygon.size(); ++existing) {
	    if (polygon[existing] == static_cast<uint64_t>(trim->m_vi[0])) {
		return face_failure(detail, face_index,
		    "repeats a boundary vertex");
	    }
	}
	polygon.push_back(static_cast<uint64_t>(trim->m_vi[0]));
	points.push_back(brep.m_V[trim->m_vi[0]].Point());
	previous_end = trim->m_vi[1];
    }
    if (previous_end != static_cast<int>(polygon.front())) {
	return face_failure(detail, face_index,
	    "does not have a closed ordered trim loop");
    }

    ON_3dPoint minimum = points.front();
    ON_3dPoint maximum = points.front();
    double coordinate_scale = 0.0;
    for (size_t vertex = 0; vertex < points.size(); ++vertex) {
	const ON_3dPoint &point = points[vertex];
	if (!point.IsValid()) {
	    return face_failure(detail, face_index,
		"has a non-finite boundary vertex");
	}
	minimum.x = std::min(minimum.x, point.x);
	minimum.y = std::min(minimum.y, point.y);
	minimum.z = std::min(minimum.z, point.z);
	maximum.x = std::max(maximum.x, point.x);
	maximum.y = std::max(maximum.y, point.y);
	maximum.z = std::max(maximum.z, point.z);
	coordinate_scale = std::max(coordinate_scale, fabs(point.x));
	coordinate_scale = std::max(coordinate_scale, fabs(point.y));
	coordinate_scale = std::max(coordinate_scale, fabs(point.z));
    }

    const double extent = (maximum - minimum).Length();
    const double length_tolerance = std::max(ON_ZERO_TOLERANCE,
	POLYGON_ROUNDOFF_MULTIPLIER * std::numeric_limits<double>::epsilon() *
	std::max(coordinate_scale, extent));
    const double area_tolerance = std::max(
	ON_ZERO_TOLERANCE * ON_ZERO_TOLERANCE,
	length_tolerance * extent);
    if (!(extent > length_tolerance) || !std::isfinite(extent) ||
	!std::isfinite(length_tolerance) || !std::isfinite(area_tolerance))
    {
	return face_failure(detail, face_index,
	    "has no reliable model-space extent");
    }

    for (size_t first = 0; first < points.size(); ++first) {
	if (fabs(plane.DistanceTo(points[first])) > length_tolerance) {
	    return face_failure(detail, face_index,
		"has boundary vertices outside its planar roundoff bound");
	}
	for (size_t second = first + 1; second < points.size(); ++second) {
	    const double distance = points[first].DistanceTo(points[second]);
	    if (!std::isfinite(distance) || distance <= length_tolerance) {
		return face_failure(detail, face_index,
		    "has coincident or numerically indistinct vertices");
	    }
	}
    }

    /* Requiring every other vertex to lie strictly on the same side of every
     * directed boundary edge certifies both simple boundary order and strict
     * convexity.  In particular, a self-intersecting star polygon cannot pass
     * this test even when all of its local turns have the same sign. */
    int winding = 0;
    for (size_t first = 0; first < points.size(); ++first) {
	const size_t second = (first + 1) % points.size();
	const ON_3dVector edge = points[second] - points[first];
	if (!(edge.Length() > length_tolerance)) {
	    return face_failure(detail, face_index,
		"has a zero-length or near-zero-length boundary edge");
	}
	for (size_t test = 0; test < points.size(); ++test) {
	    if (test == first || test == second) {
		continue;
	    }
	    const double side = ON_DotProduct(ON_CrossProduct(edge,
		points[test] - points[first]), surface_normal);
	    if (!std::isfinite(side) || fabs(side) <= area_tolerance) {
		return face_failure(detail, face_index,
		    "has collinear or numerically indeterminate topology");
	    }
	    const int side_sign = side > 0.0 ? 1 : -1;
	    if (winding == 0) {
		winding = side_sign;
	    } else if (side_sign != winding) {
		return face_failure(detail, face_index,
		    "is concave or self-intersecting");
	    }
	}
    }
    if (winding == 0) {
	return face_failure(detail, face_index,
	    "has indeterminate polygon winding");
    }

    const int desired_winding = face.m_bRev ? -1 : 1;
    double face_area = 0.0;
    for (size_t triangle = 1; triangle + 1 < polygon.size(); ++triangle) {
	uint64_t corners[3] = {polygon[0], polygon[triangle],
	    polygon[triangle + 1]};
	if (winding != desired_winding) {
	    std::swap(corners[1], corners[2]);
	}
	const ON_3dPoint first =
	    brep.m_V[static_cast<int>(corners[0])].Point();
	const ON_3dPoint second =
	    brep.m_V[static_cast<int>(corners[1])].Point();
	const ON_3dPoint third =
	    brep.m_V[static_cast<int>(corners[2])].Point();
	const ON_3dVector area_normal = ON_CrossProduct(second - first,
	    third - first);
	const double doubled_area = area_normal.Length();
	ON_3dVector expected_normal = surface_normal;
	if (face.m_bRev) {
	    expected_normal.Reverse();
	}
	const double alignment = ON_DotProduct(area_normal, expected_normal);
	if (!(doubled_area > area_tolerance) || !std::isfinite(doubled_area) ||
	    !std::isfinite(alignment) || alignment <= area_tolerance ||
	    alignment <= WINDING_COSINE_MINIMUM * doubled_area)
	{
	    return face_failure(detail, face_index,
		"has an indeterminate fan triangle orientation");
	}
	for (int corner = 0; corner < 3; ++corner) {
	    proxy.mesh.triVerts.push_back(corners[corner]);
	}
	proxy.mesh.faceID.push_back(static_cast<uint64_t>(face_index));
	face_area += 0.5 * doubled_area;
    }

    if (!(face_area > 0.0) || !std::isfinite(face_area)) {
	return face_failure(detail, face_index,
	    "has zero or non-finite triangulated area");
    }
    proxy.regions[static_cast<size_t>(face_index)].area = face_area;
    return true;
}


static bool
make_planar_proxy_mesh(const ON_Brep &brep, uint32_t original_id,
    ProxyMesh &proxy, std::string &detail)
{
    proxy = ProxyMesh();
    proxy.mesh.numProp = 3;
    proxy.regions.resize(static_cast<size_t>(brep.m_F.Count()));
    if (!brep.m_F.Count() || !brep.m_V.Count() || !brep.IsValid() ||
	!brep.IsSolid())
    {
	detail = "split operand proxy is not a valid closed solid B-Rep";
	return false;
    }
    if (static_cast<uint64_t>(brep.m_V.Count()) >
	    std::numeric_limits<uint32_t>::max() ||
	static_cast<size_t>(brep.m_V.Count()) >
	    proxy.mesh.vertProperties.max_size() / 3)
    {
	detail = "split operand proxy has too many vertices";
	return false;
    }
    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	if (brep.m_E[edge_index].TrimCount() != 2) {
	    detail = "split operand proxy does not have two trims per edge";
	    return false;
	}
    }

    proxy.mesh.vertProperties.reserve(3 * static_cast<size_t>(
	brep.m_V.Count()));
    for (int vertex_index = 0; vertex_index < brep.m_V.Count(); ++vertex_index) {
	const ON_3dPoint point = brep.m_V[vertex_index].Point();
	if (!point.IsValid()) {
	    detail = "split operand proxy has a non-finite vertex";
	    return false;
	}
	proxy.mesh.vertProperties.push_back(point.x);
	proxy.mesh.vertProperties.push_back(point.y);
	proxy.mesh.vertProperties.push_back(point.z);
    }

    for (int face_index = 0; face_index < brep.m_F.Count(); ++face_index) {
	if (proxy.mesh.NumTri() > std::numeric_limits<uint32_t>::max() - 14 ||
	    proxy.mesh.triVerts.size() > proxy.mesh.triVerts.max_size() - 42 ||
	    proxy.mesh.faceID.size() > proxy.mesh.faceID.max_size() - 14)
	{
	    detail = "split operand proxy has too many triangles";
	    return false;
	}
	if (!append_planar_face(proxy, brep, face_index, detail)) {
	    return false;
	}
    }

    proxy.mesh.runIndex.push_back(0);
    proxy.mesh.runIndex.push_back(
	static_cast<uint64_t>(proxy.mesh.triVerts.size()));
    proxy.mesh.runOriginalID.push_back(original_id);
    for (size_t region = 0; region < proxy.regions.size(); ++region) {
	if (!(proxy.regions[region].area > 0.0) ||
	    !std::isfinite(proxy.regions[region].area))
	{
	    detail = "split operand proxy has a zero-area face region";
	    return false;
	}
    }
    if (proxy.mesh.NumTri() == 0 ||
	proxy.mesh.faceID.size() != proxy.mesh.NumTri())
    {
	detail = "split operand proxy has incomplete face provenance";
	return false;
    }
    return true;
}


static bool
is_exact_planar_polygonal_brep(const ON_Brep &brep, std::string &detail)
{
    for (int face_index = 0; face_index < brep.m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep.m_F[face_index];
	const ON_Surface *surface = face.SurfaceOf();
	ON_Plane plane;
	if (!surface ||
		!surface->IsPlanar(&plane, ON_ZERO_TOLERANCE) ||
		!plane.IsValid())
	{
	    return face_failure(detail, face_index,
		"has a non-planar surface; curved CDT proxies require "
		"shared intersection sampling and fidelity certification");
	}
	if (face.LoopCount() <= 0) {
	    return face_failure(detail, face_index, "has no boundary loop");
	}
	for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	    const ON_BrepLoop *loop = face.Loop(loop_index);
	    if (!loop || loop->TrimCount() < 3) {
		return face_failure(detail, face_index,
		    "has an invalid polygonal boundary loop");
	    }
	    for (int trim_index = 0; trim_index < loop->TrimCount();
		    ++trim_index)
	    {
		const ON_BrepTrim *trim = loop->Trim(trim_index);
		const ON_Curve *trim_curve = trim ? trim->TrimCurveOf() : NULL;
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		const ON_Curve *edge_curve = edge ? edge->EdgeCurveOf() : NULL;
		if (!trim || !trim_curve || !edge || !edge_curve ||
			trim->m_type == ON_BrepTrim::singular ||
			!trim_curve->IsLinear(ON_ZERO_TOLERANCE) ||
			!edge_curve->IsLinear(ON_ZERO_TOLERANCE))
		{
		    return face_failure(detail, face_index,
			"has a non-polygonal boundary; curved CDT proxies "
			"require shared intersection sampling and fidelity "
			"certification");
		}
	    }
	}
    }
    return true;
}


static bool
make_cdt_proxy_mesh(const ON_Brep &brep, uint32_t original_id,
    ProxyMesh &proxy, std::string &detail)
{
    if (brep.m_F.Count() <= 0 || brep.m_F.Count() > CDT_PROXY_MAX_FACES) {
	detail = "split operand exceeds the certified CDT face limit";
	return false;
    }

    /* A topologically solid CDT export does not by itself certify that a
     * curved proxy covers the same subset of its source surfaces.  In
     * particular, a complete sphere has produced a topologically closed proxy
     * whose bounding box omitted the negative-radius hemisphere.  Linear
     * boundaries on planar faces have no chordal approximation error and are
     * therefore the only certified fallback until the proxy shares exact SSI
     * samples and verifies a model-space approximation tube. */
    if (!is_exact_planar_polygonal_brep(brep, detail))
	return false;

    /* ON_Brep_CDT_Create retains a non-owning source pointer for the complete
     * state lifetime.  Give it an owned copy both to satisfy that lifetime
     * and to prevent any future implementation change from mutating the
     * caller's split operand. */
    std::unique_ptr<ON_Brep> cdt_source(new ON_Brep(brep));

    std::unique_ptr<ON_Brep_CDT_State, decltype(&ON_Brep_CDT_Destroy)> state(
	ON_Brep_CDT_Create(cdt_source.get(),
	    "Boolean Manifold shadow proxy"), ON_Brep_CDT_Destroy);
    if (!state) {
	detail = "unable to create certified CDT state";
	return false;
    }

    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = CDT_PROXY_RELATIVE_TOLERANCE;
    ON_Brep_CDT_Tol_Set(state.get(), &tolerance);
    ON_Brep_CDT_Face_Time_Limit_Set(state.get(),
	CDT_PROXY_FACE_TIME_LIMIT_MS);
    if (ON_Brep_CDT_Tessellate(state.get(), 0, NULL) != 0 ||
	ON_Brep_CDT_Status(state.get()) != 0)
    {
	struct brep_cdt_diagnostic diagnostic = {};
	if (ON_Brep_CDT_Diagnostic(&diagnostic, state.get()) == 0 &&
		diagnostic.message[0] != '\0')
	{
	    detail = "certified CDT proxy failed: ";
	    detail += diagnostic.message;
	} else {
	    detail = "certified CDT proxy did not produce a closed solid";
	}
	return false;
    }

    CDTMeshArrays arrays;
    if (ON_Brep_CDT_Mesh(&arrays.faces, &arrays.face_count,
	    &arrays.vertices, &arrays.vertex_count, NULL, NULL, NULL, NULL,
	    state.get(), 0, NULL) != 0 || arrays.face_count <= 0 ||
	    arrays.vertex_count <= 0)
    {
	detail = "unable to export the certified CDT proxy";
	return false;
    }
    if ((size_t)arrays.face_count > CDT_PROXY_MAX_TRIANGLES ||
	(size_t)arrays.vertex_count > std::numeric_limits<uint32_t>::max())
    {
	detail = "certified CDT proxy exceeds the Manifold resource limit";
	return false;
    }

    const int source_count = ON_Brep_CDT_Mesh_Face_Indices(NULL, 0,
	state.get());
    if (source_count != arrays.face_count) {
	detail = "certified CDT proxy has incomplete face provenance";
	return false;
    }
    std::vector<int> source_faces((size_t)source_count);
    if (ON_Brep_CDT_Mesh_Face_Indices(source_faces.data(), source_count,
	    state.get()) != source_count)
    {
	detail = "unable to retrieve certified CDT face provenance";
	return false;
    }

    ProxyMesh candidate;
    candidate.mesh.numProp = 3;
    candidate.used_cdt = true;
    candidate.regions.resize((size_t)brep.m_F.Count());
    candidate.mesh.vertProperties.reserve((size_t)arrays.vertex_count * 3);
    for (int vertex = 0; vertex < arrays.vertex_count; ++vertex) {
	const size_t offset = (size_t)vertex * 3;
	const ON_3dPoint point(arrays.vertices[offset],
	    arrays.vertices[offset + 1], arrays.vertices[offset + 2]);
	if (!point.IsValid()) {
	    detail = "certified CDT proxy has a non-finite vertex";
	    return false;
	}
	candidate.mesh.vertProperties.push_back(point.x);
	candidate.mesh.vertProperties.push_back(point.y);
	candidate.mesh.vertProperties.push_back(point.z);
    }

    candidate.mesh.triVerts.reserve((size_t)arrays.face_count * 3);
    candidate.mesh.faceID.reserve((size_t)arrays.face_count);
    for (int triangle = 0; triangle < arrays.face_count; ++triangle) {
	const int source_face = source_faces[(size_t)triangle];
	if (source_face < 0 || source_face >= brep.m_F.Count()) {
	    detail = "certified CDT proxy references an unknown B-Rep face";
	    return false;
	}
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = arrays.faces[(size_t)triangle * 3 + corner];
	    if (vertex < 0 || vertex >= arrays.vertex_count) {
		detail = "certified CDT proxy has an invalid triangle index";
		return false;
	    }
	    candidate.mesh.triVerts.push_back((uint64_t)vertex);
	}
	candidate.mesh.faceID.push_back((uint64_t)source_face);
	double area = 0.0;
	if (!triangle_area(candidate.mesh, (size_t)triangle, area) ||
		!(area > 0.0))
	{
	    detail = "certified CDT proxy has a degenerate triangle";
	    return false;
	}
	candidate.regions[(size_t)source_face].area += area;
    }

    for (const RegionMeasure &region : candidate.regions) {
	if (!(region.area > 0.0) || !std::isfinite(region.area)) {
	    detail = "certified CDT proxy has a zero-area face region";
	    return false;
	}
    }
    candidate.mesh.runIndex.push_back(0);
    candidate.mesh.runIndex.push_back(
	(uint64_t)candidate.mesh.triVerts.size());
    candidate.mesh.runOriginalID.push_back(original_id);
    proxy = std::move(candidate);
    return true;
}


static bool
make_proxy_mesh(const ON_Brep &brep, uint32_t original_id,
    ProxyMesh &proxy, std::string &detail)
{
    std::string planar_detail;
    if (make_planar_proxy_mesh(brep, original_id, proxy, planar_detail))
	return true;

    std::string cdt_detail;
    if (make_cdt_proxy_mesh(brep, original_id, proxy, cdt_detail))
	return true;

    detail = planar_detail;
    if (!detail.empty() && !cdt_detail.empty())
	detail += "; ";
    detail += cdt_detail;
    return false;
}


static bool
accumulate_output(const manifold::MeshGL64 &mesh,
    uint32_t first_id, uint32_t second_id,
    std::vector<RegionMeasure> output[2], std::string &detail)
{
    if (mesh.runIndex.size() != mesh.runOriginalID.size() + 1 ||
	mesh.runIndex.empty() || mesh.runIndex.front() != 0 ||
	mesh.runIndex.back() != mesh.triVerts.size() ||
	mesh.faceID.size() != mesh.NumTri() ||
	mesh.runFlags.size() != mesh.runOriginalID.size())
    {
	detail = "Manifold result has incomplete provenance";
	return false;
    }

    for (size_t run = 0; run < mesh.runOriginalID.size(); ++run) {
	const uint32_t original_id = mesh.runOriginalID[run];
	const int operand = original_id == first_id ? 0 :
	    original_id == second_id ? 1 : -1;
	if (operand < 0 || mesh.runIndex[run] % 3 != 0 ||
	    mesh.runIndex[run + 1] % 3 != 0 ||
	    mesh.runIndex[run] > mesh.runIndex[run + 1])
	{
	    detail = "Manifold result has invalid provenance runs";
	    return false;
	}
	for (uint64_t triangle = mesh.runIndex[run] / 3;
	     triangle < mesh.runIndex[run + 1] / 3; ++triangle)
	{
	    if (triangle > std::numeric_limits<size_t>::max()) {
		detail = "Manifold result triangle index is not addressable";
		return false;
	    }
	    const size_t triangle_index = static_cast<size_t>(triangle);
	    const uint64_t face_id = mesh.faceID[triangle_index];
	    if (face_id >= output[operand].size()) {
		detail = "Manifold result has an unknown split-region ID";
		return false;
	    }
	    double area = 0.0;
	    if (!triangle_area(mesh, triangle_index, area))
	    {
		detail = "Manifold result has an invalid triangle";
		return false;
	    }
	    output[operand][face_id].area += area;
	    if (mesh.Backside(run)) {
		output[operand][face_id].saw_reverse = true;
	    } else {
		output[operand][face_id].saw_forward = true;
	    }
	}
    }
    return true;
}


static void
classify_regions(const std::vector<RegionMeasure> &input,
    const std::vector<RegionMeasure> &output,
    std::vector<BooleanManifoldDecision> &decisions)
{
    decisions.resize(input.size());
    for (size_t region = 0; region < input.size(); ++region) {
	BooleanManifoldDecision &decision = decisions[region];
	decision.input_area = input[region].area;
	decision.output_area = output[region].area;
	if (!(decision.input_area > 0.0) ||
	    !std::isfinite(decision.input_area) ||
	    !std::isfinite(decision.output_area))
	{
	    continue;
	}

	const double roundoff = AREA_ROUNDOFF_MULTIPLIER *
	    std::numeric_limits<double>::epsilon() * decision.input_area;
	const double area_tolerance = std::max(roundoff,
	    AREA_RELATIVE_TOLERANCE * decision.input_area);
	/* Only a complete absence of provenance certifies discard.  A tiny
	 * nonzero remnant may be a real thin region hidden by proxy error. */
	if (decision.output_area == 0.0 && !output[region].saw_forward &&
	    !output[region].saw_reverse)
	{
	    decision.selection = BOOLEAN_MANIFOLD_DISCARD;
	    continue;
	}
	if (fabs(decision.output_area - decision.input_area) > area_tolerance) {
	    continue;
	}

	if (output[region].saw_forward == output[region].saw_reverse) {
	    continue;
	}
	decision.selection = BOOLEAN_MANIFOLD_KEEP;
	decision.orientation = output[region].saw_reverse ? -1 : 1;
    }
}

} // namespace


bool
boolean_manifold_classify(const ON_Brep &first, const ON_Brep &second,
    op_type operation, BooleanManifoldReport &report)
{
    report = BooleanManifoldReport();
    const uint32_t first_id = manifold::Manifold::ReserveIDs(2);
    const uint32_t second_id = first_id + 1;
    ProxyMesh proxies[2];
    if (!make_proxy_mesh(first, first_id, proxies[0], report.detail)) {
	return false;
    }
    if (!make_proxy_mesh(second, second_id, proxies[1], report.detail)) {
	return false;
    }
    report.cdt_proxy_operands = (proxies[0].used_cdt ? 1u : 0u) +
	(proxies[1].used_cdt ? 1u : 0u);

    const manifold::Manifold first_manifold(proxies[0].mesh);
    const manifold::Manifold second_manifold(proxies[1].mesh);
    if (first_manifold.Status() != manifold::Manifold::Error::NoError ||
	second_manifold.Status() != manifold::Manifold::Error::NoError)
    {
	report.detail = "Manifold rejected a split operand proxy (first: " +
	    manifold::ToString(first_manifold.Status()) + ", second: " +
	    manifold::ToString(second_manifold.Status()) + ")";
	return false;
    }

    manifold::Manifold result;
    switch (operation) {
	case BOOLEAN_UNION:
	    result = first_manifold + second_manifold;
	    break;
	case BOOLEAN_INTERSECT:
	    result = first_manifold ^ second_manifold;
	    break;
	case BOOLEAN_DIFF:
	    result = first_manifold - second_manifold;
	    break;
	case BOOLEAN_XOR:
	    result = (first_manifold - second_manifold) +
		(second_manifold - first_manifold);
	    break;
	default:
	    report.detail = "unsupported Manifold shadow operation";
	    return false;
    }
    if (result.Status() != manifold::Manifold::Error::NoError) {
	report.detail = "Manifold proxy Boolean failed: " +
	    manifold::ToString(result.Status());
	return false;
    }

    const manifold::MeshGL64 output_mesh = result.GetMeshGL64();
    std::vector<RegionMeasure> output[2];
    output[0].resize(proxies[0].regions.size());
    output[1].resize(proxies[1].regions.size());
    if (!result.IsEmpty() && !accumulate_output(output_mesh, first_id,
	    second_id, output, report.detail))
    {
	return false;
    }

    classify_regions(proxies[0].regions, output[0], report.operand[0]);
    classify_regions(proxies[1].regions, output[1], report.operand[1]);
    report.input_triangles = proxies[0].mesh.NumTri() +
	proxies[1].mesh.NumTri();
    report.output_triangles = output_mesh.NumTri();
    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
