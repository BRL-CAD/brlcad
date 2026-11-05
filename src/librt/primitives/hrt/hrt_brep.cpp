/*                    H R T _ B R E P . C P P
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
/** @file hrt_brep.cpp
 *
 * Faceted NMG and B-Rep conversion for the implicit HRT solid.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

#include "bg/trimesh.h"
#include "brep.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"

#include "../../librt_private.h"


extern "C" {
    extern int rt_bot_tess(struct nmgregion **r, struct model *m,
	struct rt_db_internal *ip, const struct bg_tess_tol *ttol,
	const struct bn_tol *tol);
}


namespace {

constexpr double HRT_Y_QUADRATIC_SCALE = 9.0 / 4.0;
constexpr double HRT_LOBE_Y_QUADRATIC_SCALE = 9.0 / 80.0;
constexpr double HRT_GRID_X_MINIMUM = -1.2;
constexpr double HRT_GRID_X_MAXIMUM = 1.2;
constexpr double HRT_GRID_Y_MINIMUM = -0.75;
constexpr double HRT_GRID_Y_MAXIMUM = 0.75;
constexpr double HRT_GRID_Z_MINIMUM = -1.1;
constexpr double HRT_GRID_Z_MAXIMUM = 1.3;
constexpr double HRT_MINIMUM_GRID_STEP = 0.025;
constexpr double HRT_MAXIMUM_GRID_STEP = 0.08;
constexpr double HRT_EXACT_VERTEX_TOLERANCE = 1.0e-12;
constexpr int HRT_EDGE_ROOT_ITERATIONS = 48;


struct HeartMesh {
    std::vector<fastf_t> vertices;
    std::vector<ON_3dPoint> local_vertices;
    std::vector<int> faces;
};


struct GridVertex {
    ON_3dPoint point;
    double value;
    size_t index;
};


typedef std::map<std::pair<size_t, size_t>, int> EdgeVertexMap;
typedef std::map<size_t, int> ExactVertexMap;


static double
canonical_implicit_value(const ON_3dPoint &point)
{
    const double quadratic = point.x * point.x +
	HRT_Y_QUADRATIC_SCALE * point.y * point.y +
	point.z * point.z - 1.0;
    return quadratic * quadratic * quadratic -
	point.z * point.z * point.z *
	(point.x * point.x +
	 HRT_LOBE_Y_QUADRATIC_SCALE * point.y * point.y);
}


static ON_3dVector
canonical_implicit_gradient(const ON_3dPoint &point)
{
    const double quadratic = point.x * point.x +
	HRT_Y_QUADRATIC_SCALE * point.y * point.y +
	point.z * point.z - 1.0;
    const double quadratic_squared = quadratic * quadratic;
    const double z_cubed = point.z * point.z * point.z;
    const double lobe_quadratic = point.x * point.x +
	HRT_LOBE_Y_QUADRATIC_SCALE * point.y * point.y;
    return ON_3dVector(
	6.0 * point.x * quadratic_squared - 2.0 * point.x * z_cubed,
	13.5 * point.y * quadratic_squared -
	    (9.0 / 40.0) * point.y * z_cubed,
	6.0 * point.z * quadratic_squared -
	    3.0 * point.z * point.z * lobe_quadratic);
}


static bool
valid_heart_axes(const struct rt_hrt_internal &heart,
    const struct bn_tol &tolerance, double &maximum_axis_length,
    bool &reverse_orientation)
{
    const double x_length = MAGNITUDE(heart.xdir);
    const double y_length = MAGNITUDE(heart.ydir);
    const double z_length = MAGNITUDE(heart.zdir);
    if (!std::isfinite(x_length) || !std::isfinite(y_length) ||
	!std::isfinite(z_length) || !std::isfinite(heart.d) ||
	x_length <= tolerance.dist || y_length <= tolerance.dist ||
	z_length <= tolerance.dist || heart.d <= tolerance.dist)
    {
	return false;
    }

    const double xy = fabs(VDOT(heart.xdir, heart.ydir) /
	(x_length * y_length));
    const double xz = fabs(VDOT(heart.xdir, heart.zdir) /
	(x_length * z_length));
    const double yz = fabs(VDOT(heart.ydir, heart.zdir) /
	(y_length * z_length));
    if (xy > tolerance.perp || xz > tolerance.perp ||
	yz > tolerance.perp)
    {
	return false;
    }

    vect_t cross;
    VCROSS(cross, heart.xdir, heart.ydir);
    const double normalized_determinant = VDOT(cross, heart.zdir) /
	(x_length * y_length * z_length);
    if (!std::isfinite(normalized_determinant) ||
	fabs(normalized_determinant) <= tolerance.perp)
    {
	return false;
    }

    maximum_axis_length = std::max(x_length, std::max(y_length, z_length));
    reverse_orientation = normalized_determinant < 0.0;
    return true;
}


/*
 * Deliberately use only the center and axes below.  Although HRT stores a
 * legacy parameter named d, the scalar ray implementation has ignored it
 * since the primitive was introduced.  Matching that established surface is
 * essential for CSG-to-B-Rep semantic equivalence; d is still validated by
 * valid_heart_axes() for database compatibility.
 */


static double
canonical_grid_step(const struct bg_tess_tol &tessellation_tolerance,
    double maximum_axis_length)
{
    double maximum_error = tessellation_tolerance.abs;
    if (maximum_error <= 0.0)
	maximum_error = maximum_axis_length *
	    tessellation_tolerance.rel;
    const double canonical_error = maximum_error / maximum_axis_length;
    const double step = sqrt(std::max(canonical_error,
	HRT_EXACT_VERTEX_TOLERANCE));
    return std::max(HRT_MINIMUM_GRID_STEP,
	std::min(HRT_MAXIMUM_GRID_STEP, step));
}


static bool
append_mesh_vertex(HeartMesh &mesh, const struct rt_hrt_internal &heart,
    const ON_3dPoint &local_point, int &vertex_index)
{
    point_t world_point;
    VJOIN3(world_point, heart.v, local_point.x, heart.xdir,
	local_point.y, heart.ydir, local_point.z, heart.zdir);
    if (!std::isfinite(world_point[X]) || !std::isfinite(world_point[Y]) ||
	!std::isfinite(world_point[Z]))
    {
	return false;
    }

    vertex_index = static_cast<int>(mesh.local_vertices.size());
    mesh.local_vertices.push_back(local_point);
    mesh.vertices.push_back(world_point[X]);
    mesh.vertices.push_back(world_point[Y]);
    mesh.vertices.push_back(world_point[Z]);
    return true;
}


static int
exact_mesh_vertex(HeartMesh &mesh, ExactVertexMap &exact_vertices,
    const struct rt_hrt_internal &heart, const GridVertex &grid_vertex)
{
    ExactVertexMap::const_iterator existing =
	exact_vertices.find(grid_vertex.index);
    if (existing != exact_vertices.end())
	return existing->second;

    int vertex_index = -1;
    if (!append_mesh_vertex(mesh, heart, grid_vertex.point, vertex_index))
	return -1;
    exact_vertices[grid_vertex.index] = vertex_index;
    return vertex_index;
}


static int
edge_mesh_vertex(HeartMesh &mesh, EdgeVertexMap &edge_vertices,
    ExactVertexMap &exact_vertices, const struct rt_hrt_internal &heart,
    const GridVertex &first, const GridVertex &second)
{
    if (fabs(first.value) <= HRT_EXACT_VERTEX_TOLERANCE)
	return exact_mesh_vertex(mesh, exact_vertices, heart, first);
    if (fabs(second.value) <= HRT_EXACT_VERTEX_TOLERANCE)
	return exact_mesh_vertex(mesh, exact_vertices, heart, second);

    const std::pair<size_t, size_t> key =
	first.index < second.index ?
	std::make_pair(first.index, second.index) :
	std::make_pair(second.index, first.index);
    EdgeVertexMap::const_iterator existing = edge_vertices.find(key);
    if (existing != edge_vertices.end())
	return existing->second;

    double lower_parameter = 0.0;
    double upper_parameter = 1.0;
    double lower_value = first.value;
    for (int iteration = 0; iteration < HRT_EDGE_ROOT_ITERATIONS;
	 ++iteration)
    {
	const double middle_parameter =
	    0.5 * (lower_parameter + upper_parameter);
	const ON_3dPoint middle_point = first.point +
	    middle_parameter * (second.point - first.point);
	const double middle_value = canonical_implicit_value(middle_point);
	if (!std::isfinite(middle_value))
	    return -1;
	if ((lower_value <= 0.0) == (middle_value <= 0.0)) {
	    lower_parameter = middle_parameter;
	    lower_value = middle_value;
	} else {
	    upper_parameter = middle_parameter;
	}
    }
    const double parameter = 0.5 * (lower_parameter + upper_parameter);
    const ON_3dPoint local_point = first.point +
	parameter * (second.point - first.point);
    int vertex_index = -1;
    if (!append_mesh_vertex(mesh, heart, local_point, vertex_index))
	return -1;
    edge_vertices[key] = vertex_index;
    return vertex_index;
}


static bool
append_triangle(HeartMesh &mesh, int first, int second, int third,
    bool reverse_orientation)
{
    if (first < 0 || second < 0 || third < 0 || first == second ||
	first == third || second == third)
    {
	return false;
    }

    const ON_3dPoint &a = mesh.local_vertices[first];
    const ON_3dPoint &b = mesh.local_vertices[second];
    const ON_3dPoint &c = mesh.local_vertices[third];
    ON_3dVector normal = ON_CrossProduct(b - a, c - a);
    if (!normal.Unitize())
	return false;
    const ON_3dPoint centroid((a.x + b.x + c.x) / 3.0,
	(a.y + b.y + c.y) / 3.0, (a.z + b.z + c.z) / 3.0);
    ON_3dVector gradient = canonical_implicit_gradient(centroid);
    if (gradient.Unitize() && normal * gradient < 0.0)
	std::swap(second, third);
    if (reverse_orientation)
	std::swap(second, third);

    mesh.faces.push_back(first);
    mesh.faces.push_back(second);
    mesh.faces.push_back(third);
    return true;
}


static bool
polygonize_tetrahedron(HeartMesh &mesh, EdgeVertexMap &edge_vertices,
    ExactVertexMap &exact_vertices, const struct rt_hrt_internal &heart,
    const std::array<GridVertex, 8> &cube, const int tetrahedron[4],
    bool reverse_orientation)
{
    int inside[4];
    int outside[4];
    int inside_count = 0;
    int outside_count = 0;
    for (int vertex = 0; vertex < 4; ++vertex) {
	if (cube[tetrahedron[vertex]].value <= 0.0)
	    inside[inside_count++] = tetrahedron[vertex];
	else
	    outside[outside_count++] = tetrahedron[vertex];
    }
    if (inside_count == 0 || inside_count == 4)
	return true;

    if (inside_count == 1 || inside_count == 3) {
	const int center = inside_count == 1 ? inside[0] : outside[0];
	const int *opposite = inside_count == 1 ? outside : inside;
	const int first = edge_mesh_vertex(mesh, edge_vertices, exact_vertices,
	    heart, cube[center], cube[opposite[0]]);
	const int second = edge_mesh_vertex(mesh, edge_vertices, exact_vertices,
	    heart, cube[center], cube[opposite[1]]);
	const int third = edge_mesh_vertex(mesh, edge_vertices, exact_vertices,
	    heart, cube[center], cube[opposite[2]]);
	return append_triangle(mesh, first, second, third,
	    reverse_orientation);
    }

    const int first_first = edge_mesh_vertex(mesh, edge_vertices,
	exact_vertices, heart, cube[inside[0]], cube[outside[0]]);
    const int first_second = edge_mesh_vertex(mesh, edge_vertices,
	exact_vertices, heart, cube[inside[0]], cube[outside[1]]);
    const int second_first = edge_mesh_vertex(mesh, edge_vertices,
	exact_vertices, heart, cube[inside[1]], cube[outside[0]]);
    const int second_second = edge_mesh_vertex(mesh, edge_vertices,
	exact_vertices, heart, cube[inside[1]], cube[outside[1]]);
    return append_triangle(mesh, first_first, second_first, second_second,
	       reverse_orientation) &&
	append_triangle(mesh, first_first, second_second, first_second,
	    reverse_orientation);
}


static GridVertex
grid_vertex(size_t x, size_t y, size_t z, size_t x_count, size_t y_count,
    double x_step, double y_step, double z_step)
{
    GridVertex vertex;
    vertex.point = ON_3dPoint(HRT_GRID_X_MINIMUM + x * x_step,
	HRT_GRID_Y_MINIMUM + y * y_step,
	HRT_GRID_Z_MINIMUM + z * z_step);
    vertex.value = canonical_implicit_value(vertex.point);
    vertex.index = x + x_count * (y + y_count * z);
    return vertex;
}


static bool
build_heart_mesh(HeartMesh &mesh, const struct rt_hrt_internal &heart,
    const struct bg_tess_tol &tessellation_tolerance,
    const struct bn_tol &tolerance)
{
    double maximum_axis_length = 0.0;
    bool reverse_orientation = false;
    if (!valid_heart_axes(heart, tolerance, maximum_axis_length,
	    reverse_orientation) ||
	(tessellation_tolerance.abs <= 0.0 &&
	 tessellation_tolerance.rel <= 0.0))
    {
	return false;
    }

    const double target_step = canonical_grid_step(tessellation_tolerance,
	maximum_axis_length);
    const size_t x_count = static_cast<size_t>(ceil(
	(HRT_GRID_X_MAXIMUM - HRT_GRID_X_MINIMUM) / target_step)) + 1;
    const size_t y_count = static_cast<size_t>(ceil(
	(HRT_GRID_Y_MAXIMUM - HRT_GRID_Y_MINIMUM) / target_step)) + 1;
    const size_t z_count = static_cast<size_t>(ceil(
	(HRT_GRID_Z_MAXIMUM - HRT_GRID_Z_MINIMUM) / target_step)) + 1;
    const double x_step = (HRT_GRID_X_MAXIMUM - HRT_GRID_X_MINIMUM) /
	static_cast<double>(x_count - 1);
    const double y_step = (HRT_GRID_Y_MAXIMUM - HRT_GRID_Y_MINIMUM) /
	static_cast<double>(y_count - 1);
    const double z_step = (HRT_GRID_Z_MAXIMUM - HRT_GRID_Z_MINIMUM) /
	static_cast<double>(z_count - 1);

    static const int cube_offsets[8][3] = {
	{0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0},
	{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}
    };
    static const int tetrahedra[6][4] = {
	{3, 1, 2, 5}, {3, 2, 6, 5}, {3, 6, 7, 5},
	{3, 7, 4, 5}, {3, 4, 0, 5}, {3, 0, 1, 5}
    };

    EdgeVertexMap edge_vertices;
    ExactVertexMap exact_vertices;
    mesh.vertices.reserve(60000);
    mesh.local_vertices.reserve(20000);
    mesh.faces.reserve(60000);
    for (size_t z = 0; z + 1 < z_count; ++z) {
	for (size_t y = 0; y + 1 < y_count; ++y) {
	    for (size_t x = 0; x + 1 < x_count; ++x) {
		std::array<GridVertex, 8> cube;
		for (size_t corner = 0; corner < cube.size(); ++corner) {
		    cube[corner] = grid_vertex(
			x + cube_offsets[corner][X],
			y + cube_offsets[corner][Y],
			z + cube_offsets[corner][Z],
			x_count, y_count, x_step, y_step, z_step);
		}
		for (const int *tetrahedron : tetrahedra) {
		    if (!polygonize_tetrahedron(mesh, edge_vertices,
			    exact_vertices, heart, cube, tetrahedron,
			    reverse_orientation))
		    {
			return false;
		    }
		}
	    }
	}
    }
    if (mesh.faces.empty() ||
	bg_trimesh_sync(mesh.faces.data(), mesh.faces.data(),
	    static_cast<int>(mesh.faces.size() / 3)) < 0)
    {
	return false;
    }
    return bg_trimesh_solid2(static_cast<int>(mesh.vertices.size() / 3),
	static_cast<int>(mesh.faces.size() / 3), mesh.vertices.data(),
	mesh.faces.data(), NULL) == 0;
}


static void
initialize_bot_internal(struct rt_db_internal &internal,
    struct rt_bot_internal &bot, HeartMesh &mesh)
{
    bot.magic = RT_BOT_INTERNAL_MAGIC;
    bot.mode = RT_BOT_SOLID;
    bot.orientation = RT_BOT_CCW;
    bot.num_vertices = mesh.vertices.size() / 3;
    bot.vertices = mesh.vertices.data();
    bot.num_faces = mesh.faces.size() / 3;
    bot.faces = mesh.faces.data();

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];
    internal.idb_ptr = &bot;
}


static bool
heart_mesh_brep(ON_Brep &brep, const HeartMesh &heart_mesh)
{
    const size_t vertex_count = heart_mesh.vertices.size() / 3;
    const size_t face_count = heart_mesh.faces.size() / 3;
    if (vertex_count > static_cast<size_t>(INT_MAX) ||
	face_count > static_cast<size_t>(INT_MAX))
    {
	return false;
    }

    ON_Mesh mesh;
    mesh.m_dV.Reserve(static_cast<int>(vertex_count));
    for (size_t vertex = 0; vertex < vertex_count; ++vertex) {
	mesh.m_dV.Append(ON_3dPoint(
	    heart_mesh.vertices[3 * vertex],
	    heart_mesh.vertices[3 * vertex + 1],
	    heart_mesh.vertices[3 * vertex + 2]));
    }
    mesh.UpdateSinglePrecisionVertices();

    mesh.m_F.SetCount(static_cast<int>(face_count));
    for (size_t face = 0; face < face_count; ++face) {
	if (!mesh.SetTriangle(static_cast<int>(face),
		heart_mesh.faces[3 * face], heart_mesh.faces[3 * face + 1],
		heart_mesh.faces[3 * face + 2]))
	{
	    return false;
	}
    }

    if (!mesh.IsValid())
	return false;
    brep.Destroy();
    if (ON_BrepFromMesh(mesh.Topology(), true, &brep) != &brep)
	return false;
    for (int vertex = 0; vertex < brep.m_V.Count(); ++vertex)
	brep.m_V[vertex].m_tolerance = 0.0;
    for (int edge = 0; edge < brep.m_E.Count(); ++edge)
	brep.m_E[edge].m_tolerance = 0.0;
    for (int trim = 0; trim < brep.m_T.Count(); ++trim) {
	brep.m_T[trim].m_tolerance[0] = 0.0;
	brep.m_T[trim].m_tolerance[1] = 0.0;
    }
    return true;
}

} // namespace


extern "C" int
rt_hrt_tess(struct nmgregion **region, struct model *model,
    struct rt_db_internal *ip, const struct bg_tess_tol *ttol,
    const struct bn_tol *tol)
{
    if (!region || !model || !ip || !ip->idb_ptr || !ttol || !tol)
	return -1;
    *region = NULL;
    NMG_CK_MODEL(model);
    RT_CK_DB_INTERNAL(ip);

    const struct rt_hrt_internal *heart =
	static_cast<const struct rt_hrt_internal *>(ip->idb_ptr);
    RT_HRT_CK_MAGIC(heart);
    HeartMesh mesh;
    if (!build_heart_mesh(mesh, *heart, *ttol, *tol))
	return -1;

    struct rt_bot_internal bot = {};
    struct rt_db_internal bot_internal;
    initialize_bot_internal(bot_internal, bot, mesh);
    return rt_bot_tess(region, model, &bot_internal, ttol, tol);
}


extern "C" void
rt_hrt_brep(ON_Brep **b, const struct rt_db_internal *ip,
    const struct bn_tol *tol)
{
    if (!b || !*b || !ip || !ip->idb_ptr || !tol)
	return;
    RT_CK_DB_INTERNAL(ip);
    const struct rt_hrt_internal *heart =
	static_cast<const struct rt_hrt_internal *>(ip->idb_ptr);
    RT_HRT_CK_MAGIC(heart);

    struct bg_tess_tol tessellation_tolerance = BG_TESS_TOL_INIT_ZERO;
    tessellation_tolerance.rel = RT_HRT_BREP_RELATIVE_TOLERANCE;
    HeartMesh mesh;
    if (!build_heart_mesh(mesh, *heart, tessellation_tolerance, *tol)) {
	(*b)->Destroy();
	return;
    }

    if (!heart_mesh_brep(**b, mesh))
	(*b)->Destroy();
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
