/*                B R E P _ C O N T A C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

#include "bg/trimesh.h"
#include "brep.h"
#include "brep/cdt.h"
#include "manifold/manifold.h"

#include "./tessellate.h"

namespace {

/* Two pairings per edge. Bound the work on imported nonmanifold shapes. */
constexpr size_t max_contact_edges = 4;
constexpr double rounding_margin = 4096 * std::numeric_limits<double>::epsilon();

using TrimPair = std::array<int, 2>;

struct ContactEdge {
    int index;
    TrimPair along;
    TrimPair against;
};

class TrimCorners {
public:
    explicit TrimCorners(int count) : parent(count)
    {
	std::iota(parent.begin(), parent.end(), 0);
    }

    int root(int corner)
    {
	while (corner != parent[corner]) {
	    parent[corner] = parent[parent[corner]];
	    corner = parent[corner];
	}
	return corner;
    }

    void join(int a, int b)
    {
	parent[root(a)] = root(b);
    }

private:
    std::vector<int> parent;
};

bool
contact_edges(std::vector<ContactEdge> &contacts, ON_Plane &plane,
	const ON_Brep &source)
{
    if (!source.IsValidTopology())
	return false;

    for (int i = 0; i < source.m_E.Count(); ++i) {
	const ON_BrepEdge &edge = source.m_E[i];
	if (edge.TrimCount() == 2 || !edge.TrimCount())
	    continue;
	if (edge.TrimCount() != 4 || contacts.size() == max_contact_edges)
	    return false;

	const ON_3dPoint from = edge.PointAtStart();
	const ON_3dPoint to = edge.PointAtEnd();
	const double tolerance = std::max(ON_ZERO_TOLERANCE,
	    rounding_margin * std::max(from.MaximumCoordinate(), to.MaximumCoordinate()));
	ON_3dVector axis = to - from;
	if (axis.Length() <= tolerance || !axis.Unitize() || !edge.IsLinear(tolerance))
	    return false;

	if (contacts.empty()) {
	    /* Use the edge direction rather than the trim orientation. The
	     * splitting plane must contain the complete line of contact. */
	    plane = ON_Plane(from, axis);
	    if (!plane.IsValid())
		return false;
	} else {
	    if (ON_CrossProduct(plane.zaxis, from - plane.origin).Length() > tolerance ||
		    ON_CrossProduct(plane.zaxis, to - plane.origin).Length() > tolerance)
		return false;
	}

	ContactEdge contact = {i, {}, {}};
	size_t along = 0;
	size_t against = 0;
	for (int j = 0; j < edge.TrimCount(); ++j) {
	    const ON_BrepTrim *trim = edge.Trim(j);
	    const bool forward = trim->m_bRev3d == trim->Face()->m_bRev;
	    TrimPair &group = forward ? contact.along : contact.against;
	    size_t &used = forward ? along : against;
	    if (used == group.size())
		return false;
	    group[used++] = trim->m_trim_index;
	}
	contacts.push_back(contact);
    }
    return !contacts.empty();
}

bool
separate_contact(ON_Brep &brep, const std::vector<ContactEdge> &contacts,
	unsigned pairing)
{
    const int edge_count = brep.m_E.Count();
    std::vector<std::vector<TrimPair>> edge_pairs(edge_count);
    for (int i = 0; i < edge_count; ++i) {
	const ON_BrepEdge &edge = brep.m_E[i];
	if (edge.TrimCount() == 2)
	    edge_pairs[i].push_back({edge.m_ti[0], edge.m_ti[1]});
    }
    for (const ContactEdge &contact : contacts) {
	const unsigned swap = pairing & 1u;
	pairing >>= 1;
	edge_pairs[contact.index] = {
	    {contact.along[0], contact.against[swap]},
	    {contact.along[1], contact.against[1u - swap]}
	};
    }

    const int trim_count = brep.m_T.Count();
    std::vector<int> next(trim_count, -1);
    for (int i = 0; i < brep.m_L.Count(); ++i) {
	const ON_BrepLoop &loop = brep.m_L[i];
	for (int j = 0; j < loop.TrimCount(); ++j) {
	    const int trim = loop.m_ti[j];
	    const int successor = loop.m_ti[(j + 1) % loop.TrimCount()];
	    if (brep.m_T[trim].m_vi[1] != brep.m_T[successor].m_vi[0])
		return false;
	    next[trim] = successor;
	}
    }
    if (std::find(next.begin(), next.end(), -1) != next.end())
	return false;

    /* A trim start is a distinct face corner. Join corners across each
     * oriented pair at the same edge endpoint to find the vertex fans. */
    TrimCorners corners(trim_count);
    for (int i = 0; i < trim_count; ++i) {
	if (brep.m_T[i].m_type == ON_BrepTrim::singular)
	    corners.join(i, next[i]);
    }
    const auto corner_at = [&](int trim, bool end) {
	return end == brep.m_T[trim].m_bRev3d ? trim : next[trim];
    };
    for (const auto &pairs : edge_pairs) {
	for (const TrimPair &pair : pairs) {
	    corners.join(corner_at(pair[0], false), corner_at(pair[1], false));
	    corners.join(corner_at(pair[0], true), corner_at(pair[1], true));
	}
    }

    std::vector<int> vertex_at_root(trim_count, -1);
    std::vector<bool> claimed(brep.m_V.Count(), false);
    for (int i = 0; i < trim_count; ++i) {
	const int root = corners.root(i);
	const int vertex = brep.m_T[i].m_vi[0];
	if (brep.m_T[root].m_vi[0] != vertex)
	    return false;

	int &assigned = vertex_at_root[root];
	if (assigned >= 0)
	    continue;
	if (claimed[vertex]) {
	    const ON_3dPoint point = brep.m_V[vertex].Point();
	    assigned = brep.NewVertex(point, brep.m_V[vertex].m_tolerance).m_vertex_index;
	} else {
	    assigned = vertex;
	    claimed[vertex] = true;
	}
    }

    for (int i = 0; i < trim_count; ++i) {
	brep.m_T[i].m_vi[0] = vertex_at_root[corners.root(i)];
	brep.m_T[i].m_vi[1] = vertex_at_root[corners.root(next[i])];
    }
    for (int i = 0; i < brep.m_V.Count(); ++i)
	brep.m_V[i].m_ei.Empty();

    for (int i = 0; i < edge_count; ++i) {
	const auto &pairs = edge_pairs[i];
	if (pairs.empty()) {
	    for (int vertex : brep.m_E[i].m_vi)
		brep.m_V[vertex].m_ei.Append(i);
	    continue;
	}
	for (size_t j = 0; j < pairs.size(); ++j) {
	    const TrimPair &pair = pairs[j];
	    const ON_BrepTrim &trim = brep.m_T[pair.front()];
	    const int from = trim.m_vi[trim.m_bRev3d ? 1 : 0];
	    const int to = trim.m_vi[trim.m_bRev3d ? 0 : 1];
	    int edge_index = i;
	    if (j) {
		ON_Curve *duplicate = brep.m_E[i].DuplicateCurve();
		if (!duplicate)
		    return false;
		const int curve = brep.AddEdgeCurve(duplicate);
		edge_index = brep.NewEdge(brep.m_V[from], brep.m_V[to], curve,
		    nullptr, brep.m_E[i].m_tolerance).m_edge_index;
	    } else {
		brep.m_V[from].m_ei.Append(i);
		brep.m_V[to].m_ei.Append(i);
	    }
	    ON_BrepEdge &edge = brep.m_E[edge_index];
	    edge.m_vi[0] = from;
	    edge.m_vi[1] = to;
	    edge.m_ti.Empty();
	    for (int trim_index : pair) {
		edge.m_ti.Append(trim_index);
		brep.m_T[trim_index].m_ei = edge_index;
	    }
	}
    }
    brep.SetTrimTypeFlags();
    return brep.IsValid() && brep.IsSolid();
}

bool
contact_mesh(manifold::MeshGL64 &mesh, ON_Brep &brep, const char *name,
	const bg_tess_tol *ttol)
{
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.abs = ttol->abs;
    tolerance.rel = ttol->rel;
    tolerance.norm = ttol->norm;
    std::unique_ptr<ON_Brep_CDT_State, decltype(&ON_Brep_CDT_Destroy)> state(
	ON_Brep_CDT_Create(&brep, name), ON_Brep_CDT_Destroy);
    ON_Brep_CDT_Tol_Set(state.get(), &tolerance);
    const int result = ON_Brep_CDT_Tessellate(state.get(), 0, nullptr);
    struct brep_cdt_diagnostic diagnostic = {};
    ON_Brep_CDT_Diagnostic(&diagnostic, state.get());
    /* Only the coincident contact may fail the final solid check. Every
     * source chart must have a certified triangulation. */
    if (diagnostic.completed_faces != brep.m_F.Count() || diagnostic.failed_faces ||
	    (result && diagnostic.result != BREP_CDT_RESULT_GEOMETRIC_FAILED))
	return false;

    int *faces = nullptr;
    fastf_t *vertices = nullptr;
    int face_count = 0;
    int vertex_count = 0;
    const int exported = ON_Brep_CDT_Mesh(&faces, &face_count, &vertices,
	&vertex_count, nullptr, nullptr, nullptr, nullptr, state.get(), 0, nullptr);
    const bool closed = !exported && face_count <= std::numeric_limits<int>::max() / 3 &&
	!bg_trimesh_solid2(vertex_count, face_count, vertices, faces, nullptr);
    if (closed) {
	mesh.vertProperties.assign(vertices, vertices + 3 * static_cast<size_t>(vertex_count));
	mesh.triVerts.assign(faces, faces + 3 * static_cast<size_t>(face_count));
    }
    bu_free(faces, "contact mesh faces");
    bu_free(vertices, "contact mesh vertices");
    return closed;
}

rt_bot_internal *
certified_part(const manifold::Manifold &solid)
{
    if (solid.Status() != manifold::Manifold::Error::NoError || solid.IsEmpty())
	return nullptr;
    const auto mesh = solid.GetMeshGL64();
    if (mesh.numProp != 3 || mesh.NumTri() > std::numeric_limits<int>::max() / 3 ||
	    mesh.NumVert() > std::numeric_limits<int>::max())
	return nullptr;

    std::vector<int> faces(mesh.triVerts.begin(), mesh.triVerts.end());
    const auto *vertices = reinterpret_cast<const point_t *>(mesh.vertProperties.data());
    if (bg_trimesh_self_isect(faces.data(), mesh.NumTri(), vertices, mesh.NumVert()))
	return nullptr;

    struct rt_bot_internal *bot;
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->num_vertices = mesh.NumVert();
    bot->num_faces = mesh.NumTri();
    bot->vertices = static_cast<fastf_t *>(bu_malloc(mesh.vertProperties.size() *
	sizeof(fastf_t), "contact partition vertices"));
    bot->faces = static_cast<int *>(bu_malloc(faces.size() * sizeof(int),
	"contact partition faces"));
    std::copy(mesh.vertProperties.begin(), mesh.vertProperties.end(), bot->vertices);
    std::copy(faces.begin(), faces.end(), bot->faces);
    if (bg_trimesh_solid2(bot->num_vertices, bot->num_faces, bot->vertices,
	    bot->faces, nullptr)) {
	_tess_facetize_free_bot(bot);
	return nullptr;
    }
    return bot;
}

bool
split_contact(FacetizeBotParts &parts, const manifold::MeshGL64 &mesh,
	const ON_Plane &frame)
{
    const manifold::Manifold source(mesh);
    if (source.Status() != manifold::Manifold::Error::NoError || source.IsEmpty())
	return false;

    using vec3 = manifold::vec3;
    const vec3 origin(frame.origin.x, frame.origin.y, frame.origin.z);
    const vec3 x(frame.xaxis.x, frame.xaxis.y, frame.xaxis.z);
    const vec3 y(frame.yaxis.x, frame.yaxis.y, frame.yaxis.z);
    const auto bounds = source.BoundingBox();
    const double length = std::max({bounds.Size().x, bounds.Size().y, bounds.Size().z});
    const double dist_tol = rounding_margin * std::max(length, bounds.Scale());
    const double volume_tol = rounding_margin * length * length * length;
    const double volume = source.Volume();
    if (!std::isfinite(volume) || !(volume > volume_tol))
	return false;

    /* A plane through the contact line puts each occupied sector into a
     * separate half-space. Try the frame planes and their bisectors. */
    for (const vec3 &normal : {x, y, manifold::la::normalize(x + y), manifold::la::normalize(x - y)}) {
	const auto split = source.SplitByPlane(normal, manifold::la::dot(origin, normal));
	const double a = split.first.Volume();
	const double b = split.second.Volume();
	if (!(a > volume_tol && b > volume_tol &&
		std::abs(a + b - volume) <= volume_tol))
	    continue;

	const auto abounds = split.first.BoundingBox();
	const auto bbounds = split.second.BoundingBox();
	bool bounds_preserved = true;
	for (int k = 0; k < 3; ++k) {
	    bounds_preserved = bounds_preserved &&
		std::abs(std::min(abounds.min[k], bbounds.min[k]) - bounds.min[k]) <= dist_tol &&
		std::abs(std::max(abounds.max[k], bbounds.max[k]) - bounds.max[k]) <= dist_tol;
	}
	if (!bounds_preserved)
	    continue;

	rt_bot_internal *first = certified_part(split.first);
	if (!first)
	    continue;
	rt_bot_internal *second = certified_part(split.second);
	if (second) {
	    parts = {first, second};
	    return true;
	}
	_tess_facetize_free_bot(first);
    }
    return false;
}

} // namespace

bool
brep_contact_partition(FacetizeBotParts &parts, const struct rt_db_internal *intern,
	const char *name, const struct bg_tess_tol *ttol)
{
    const auto *bi = static_cast<const rt_brep_internal *>(intern->idb_ptr);
    RT_BREP_CK_MAGIC(bi);
    const ON_Brep &source = *bi->brep;
    std::vector<ContactEdge> contacts;
    ON_Plane frame;
    if (!contact_edges(contacts, frame, source))
	return false;

    for (unsigned pairing = 0; pairing < (1u << contacts.size()); ++pairing) {
	ON_Brep separated(source);
	if (!separate_contact(separated, contacts, pairing))
	    continue;

	manifold::MeshGL64 mesh;
	mesh.numProp = 3;
	if (contact_mesh(mesh, separated, name, ttol) && split_contact(parts, mesh, frame)) {
	    bu_log("%s: partitioned coincident B-Rep contacts into %zu closed BoTs\n",
		name, parts.size());
	    return true;
	}
    }
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
