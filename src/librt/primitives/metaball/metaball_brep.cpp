/*                 M E T A B A L L _ B R E P . C P P
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
/** @file metaball_brep.cpp
 *
 * Convert a metaball's implicit isosurface to an indexed faceted B-Rep.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <map>
#include <vector>

#include "brep.h"
#include "raytrace.h"
#include "rt/geom.h"

#include "metaball.h"
#include "../../librt_private.h"


extern "C" {
    extern int rt_mc_cube_triangle_edges(int *triangle_edges, int pv);
}


namespace {

/* rt_metaball_tess converts a relative tolerance to a grid cell width of
 * rel * bounding_radius * 10. */
const size_t METABALL_BREP_MAXIMUM_GRID_POINTS = 16 * 1024 * 1024;


const int corner_offset[8][3] = {
    {0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0},
    {0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}
};


const int edge_corners[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {0, 3},
    {4, 5}, {5, 6}, {6, 7}, {4, 7},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};


struct GridEdge {
    int coordinate[3];
    int axis;

    bool operator<(const GridEdge &other) const
    {
	for (int component = 0; component < 3; ++component) {
	    if (coordinate[component] != other.coordinate[component])
		return coordinate[component] < other.coordinate[component];
	}
	return axis < other.axis;
    }
};


struct MetaballMesh {
    std::vector<ON_3dPoint> vertices;
    std::vector<std::array<int, 3> > faces;
};


static bool
checked_product(size_t first, size_t second, size_t &product)
{
    if (first != 0 && second > METABALL_BREP_MAXIMUM_GRID_POINTS / first)
	return false;
    product = first * second;
    return product <= METABALL_BREP_MAXIMUM_GRID_POINTS;
}


static size_t
grid_index(int x, int y, int z, int y_count, int z_count)
{
    return (static_cast<size_t>(x) * static_cast<size_t>(y_count) +
	static_cast<size_t>(y)) * static_cast<size_t>(z_count) +
	static_cast<size_t>(z);
}


static ON_3dPoint
grid_point(const point_t minimum, double cell_width, int x, int y, int z)
{
    return ON_3dPoint(minimum[X] + cell_width * static_cast<double>(x),
	minimum[Y] + cell_width * static_cast<double>(y),
	minimum[Z] + cell_width * static_cast<double>(z));
}


static GridEdge
global_edge(int cube_x, int cube_y, int cube_z, int local_edge)
{
    const int first_corner = edge_corners[local_edge][0];
    const int second_corner = edge_corners[local_edge][1];
    GridEdge edge = {{0, 0, 0}, -1};
    const int cube_coordinate[3] = {cube_x, cube_y, cube_z};
    for (int component = 0; component < 3; ++component) {
	const int first = cube_coordinate[component] +
	    corner_offset[first_corner][component];
	const int second = cube_coordinate[component] +
	    corner_offset[second_corner][component];
	edge.coordinate[component] = std::min(first, second);
	if (first != second)
	    edge.axis = component;
    }
    return edge;
}


static bool
metaball_grid(MetaballMesh &mesh, struct rt_metaball_internal &metaball,
    const struct bn_tol &tolerance)
{
    point_t center;
    VSETALL(center, 0.0);
    const fastf_t radius = rt_metaball_get_bounding_sphere(&center,
	metaball.threshold, &metaball);
    if (!(radius > 0.0) || !std::isfinite(radius))
	return false;

    double final_step = INFINITY;
    struct wdb_metaball_pnt *control_point;
    for (BU_LIST_FOR(control_point, wdb_metaball_pnt,
	    &metaball.metaball_ctrl_head))
    {
	if (!(control_point->field_strength > 0.0) ||
	    !std::isfinite(control_point->field_strength))
	{
	    return false;
	}
	final_step = std::min(final_step,
	    static_cast<double>(control_point->field_strength));
    }
    final_step /= 1.0e5;
    if (!(final_step > 0.0) || !std::isfinite(final_step))
	return false;

    const double model_tolerance = tolerance.dist > 0.0 ?
	tolerance.dist : BN_TOL_DIST;
    const double cell_width = std::max(model_tolerance,
	RT_METABALL_BREP_RELATIVE_TOLERANCE * radius * 10.0);
    if (!(cell_width > 0.0) || !std::isfinite(cell_width))
	return false;

    point_t minimum;
    point_t maximum;
    VSET(minimum, center[X] - radius - cell_width,
	center[Y] - radius - cell_width, center[Z] - radius - cell_width);
    VSET(maximum, center[X] + radius + cell_width,
	center[Y] + radius + cell_width, center[Z] + radius + cell_width);

    int cube_count[3];
    for (int component = 0; component < 3; ++component) {
	const double count = std::ceil((maximum[component] -
	    minimum[component]) / cell_width);
	if (!(count >= 1.0) || count > static_cast<double>(INT_MAX - 1))
	    return false;
	cube_count[component] = static_cast<int>(count);
    }
    const int point_count[3] = {
	cube_count[X] + 1, cube_count[Y] + 1, cube_count[Z] + 1
    };
    size_t xy_count = 0;
    size_t grid_point_count = 0;
    if (!checked_product(static_cast<size_t>(point_count[X]),
	    static_cast<size_t>(point_count[Y]), xy_count) ||
	!checked_product(xy_count, static_cast<size_t>(point_count[Z]),
	    grid_point_count))
    {
	return false;
    }

    std::vector<unsigned char> inside(grid_point_count, 0);
    for (int x = 0; x < point_count[X]; ++x) {
	for (int y = 0; y < point_count[Y]; ++y) {
	    for (int z = 0; z < point_count[Z]; ++z) {
		const ON_3dPoint sample = grid_point(minimum, cell_width,
		    x, y, z);
		point_t sample_point;
		VSET(sample_point, sample.x, sample.y, sample.z);
		inside[grid_index(x, y, z, point_count[Y], point_count[Z])] =
		    rt_metaball_point_inside((const point_t *)&sample_point,
			&metaball) ? 1 : 0;
	    }
	}
    }

    std::map<GridEdge, int> edge_vertices;
    for (int x = 0; x < cube_count[X]; ++x) {
	for (int y = 0; y < cube_count[Y]; ++y) {
	    for (int z = 0; z < cube_count[Z]; ++z) {
		int point_value = 0;
		ON_3dPoint corners[8];
		for (int corner = 0; corner < 8; ++corner) {
		    const int corner_x = x + corner_offset[corner][X];
		    const int corner_y = y + corner_offset[corner][Y];
		    const int corner_z = z + corner_offset[corner][Z];
		    corners[corner] = grid_point(minimum, cell_width,
			corner_x, corner_y, corner_z);
		    if (inside[grid_index(corner_x, corner_y, corner_z,
			    point_count[Y], point_count[Z])])
		    {
			point_value |= 1 << corner;
		    }
		}
		if (point_value == 0 || point_value == 255)
		    continue;

		int triangle_edges[15] = {0};
		const int triangle_count = rt_mc_cube_triangle_edges(
		    triangle_edges, point_value);
		if (triangle_count < 0 || triangle_count > 5)
		    return false;

		int local_vertex[12];
		std::fill(local_vertex, local_vertex + 12, -1);
		for (int triangle = 0; triangle < triangle_count; ++triangle) {
		    int face_vertex[3] = {-1, -1, -1};
		    for (int vertex = 0; vertex < 3; ++vertex) {
			const int local_edge = triangle_edges[3 * triangle + vertex];
			if (local_edge < 0 || local_edge >= 12)
			    return false;
			if (local_vertex[local_edge] < 0) {
			    const GridEdge edge = global_edge(x, y, z, local_edge);
			    std::map<GridEdge, int>::const_iterator existing =
				edge_vertices.find(edge);
			    if (existing != edge_vertices.end()) {
				local_vertex[local_edge] = existing->second;
			    } else {
				const int first_corner = edge_corners[local_edge][0];
				const int second_corner = edge_corners[local_edge][1];
				point_t first;
				point_t second;
				point_t intersection;
				VSET(first, corners[first_corner].x,
				    corners[first_corner].y, corners[first_corner].z);
				VSET(second, corners[second_corner].x,
				    corners[second_corner].y, corners[second_corner].z);
				if (rt_metaball_find_intersection(&intersection,
					&metaball, (const point_t *)&first,
					(const point_t *)&second, cell_width,
					final_step) != 0 ||
				    !ON_3dPoint(intersection).IsValid() ||
				    mesh.vertices.size() >= static_cast<size_t>(INT_MAX))
				{
				    return false;
				}
				local_vertex[local_edge] =
				    static_cast<int>(mesh.vertices.size());
				mesh.vertices.push_back(ON_3dPoint(intersection));
				edge_vertices[edge] = local_vertex[local_edge];
			    }
			}
			face_vertex[vertex] = local_vertex[local_edge];
		    }
		    if (face_vertex[0] == face_vertex[1] ||
			face_vertex[1] == face_vertex[2] ||
			face_vertex[2] == face_vertex[0] ||
			mesh.faces.size() >= static_cast<size_t>(INT_MAX))
		    {
			return false;
		    }
		    /* Match the orientation used by nmg_mc_realize_cube. */
		    mesh.faces.push_back({{
			face_vertex[0], face_vertex[2], face_vertex[1]
		    }});
		}
	    }
	}
    }
    return !mesh.vertices.empty() && !mesh.faces.empty();
}


static bool
mesh_brep(ON_Brep &brep, const MetaballMesh &metaball_mesh)
{
    ON_Mesh mesh;
    mesh.m_dV.Reserve(static_cast<int>(metaball_mesh.vertices.size()));
    for (size_t vertex = 0; vertex < metaball_mesh.vertices.size(); ++vertex)
	mesh.m_dV.Append(metaball_mesh.vertices[vertex]);
    mesh.UpdateSinglePrecisionVertices();

    mesh.m_F.SetCount(static_cast<int>(metaball_mesh.faces.size()));
    for (size_t face = 0; face < metaball_mesh.faces.size(); ++face) {
	if (!mesh.SetTriangle(static_cast<int>(face),
		metaball_mesh.faces[face][0], metaball_mesh.faces[face][1],
		metaball_mesh.faces[face][2]))
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


extern "C" void
rt_metaball_brep(ON_Brep **b, const struct rt_db_internal *ip,
    const struct bn_tol *tol)
{
    if (!b || !*b || !ip || !ip->idb_ptr || !tol)
	return;

    struct rt_metaball_internal *metaball =
	(struct rt_metaball_internal *)ip->idb_ptr;
    if (metaball->magic != RT_METABALL_INTERNAL_MAGIC) {
	(*b)->Destroy();
	return;
    }

    MetaballMesh mesh;
    if (!metaball_grid(mesh, *metaball, *tol) || !mesh_brep(**b, mesh))
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
