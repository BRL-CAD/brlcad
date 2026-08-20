/*            S T E P T E S S E L L A T E D M E S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "STEPTessellatedMesh.h"

#include <algorithm>
#include <cmath>
#include <limits>

brlcad::step::TessellatedMeshBuilder::TessellatedMeshBuilder(
    double length_factor, bool allow_winding_repair,
    double vertex_merge_tolerance, bool allow_repeated_triangle_repair)
    : length(length_factor), repair_winding(allow_winding_repair),
      repair_repeated_triangles(allow_repeated_triangle_repair),
      merge_tolerance(vertex_merge_tolerance)
{
}

const brlcad::step::TessellatedCoordinateData *
brlcad::step::TessellatedMeshBuilder::DefineCoordinates(int64_t entity_id,
    const std::vector<std::vector<double> > &rows, size_t expected_points)
{
    if (entity_id <= 0) {
	error = "missing coordinates_list";
	return NULL;
    }
    std::map<int64_t, TessellatedCoordinateData>::iterator existing =
	coordinate_cache.find(entity_id);
    if (existing != coordinate_cache.end()) return &existing->second;
    if (rows.empty() || rows.size() != expected_points) {
	error = "coordinates_list npoints or position_coords is invalid";
	return NULL;
    }
    TessellatedCoordinateData data;
    data.entity_id = entity_id;
    data.points.reserve(rows.size() * 3);
    for (size_t row = 0; row < rows.size(); ++row) {
	if (rows[row].size() != 3) {
	    error = "tessellated coordinate does not have three components";
	    return NULL;
	}
	for (size_t coordinate = 0; coordinate < 3; ++coordinate) {
	    const double value = rows[row][coordinate] * length;
	    if (!std::isfinite(value)) {
		error = "scaled tessellated coordinate is not finite";
		return NULL;
	    }
	    data.points.push_back(static_cast<fastf_t>(value));
	}
    }
    existing = coordinate_cache.insert(std::make_pair(entity_id, data)).first;
    return &existing->second;
}

int
brlcad::step::TessellatedMeshBuilder::Vertex(
    const TessellatedCoordinateData &coordinates, int coordinate_index)
{
    if (coordinate_index < 1 ||
	    static_cast<size_t>(coordinate_index) > coordinates.points.size() / 3) {
	error = "triangle coordinate index is out of range";
	return -1;
    }
    const std::pair<int64_t, int> key(coordinates.entity_id, coordinate_index);
    std::map<std::pair<int64_t, int>, int>::const_iterator existing =
	vertex_map.find(key);
    if (existing != vertex_map.end()) return existing->second;
    const size_t source = static_cast<size_t>(coordinate_index - 1) * 3;
    const std::array<fastf_t, 3> point = {
	coordinates.points[source], coordinates.points[source + 1],
	coordinates.points[source + 2]
    };
    std::map<std::array<fastf_t, 3>, int>::const_iterator exact =
	exact_vertices.find(point);
    if (exact != exact_vertices.end()) {
	vertex_map[key] = exact->second;
	return exact->second;
    }

    std::array<int64_t, 3> bucket = {0, 0, 0};
    bool have_bucket = merge_tolerance > 0.0 &&
	std::isfinite(merge_tolerance);
    for (size_t coordinate = 0; coordinate < 3 && have_bucket; ++coordinate) {
	const double scaled = std::floor(point[coordinate] / merge_tolerance);
	if (!std::isfinite(scaled) ||
		scaled < static_cast<double>(std::numeric_limits<int64_t>::min() + 1) ||
		scaled > static_cast<double>(std::numeric_limits<int64_t>::max() - 1)) {
	    have_bucket = false;
	} else {
	    bucket[coordinate] = static_cast<int64_t>(scaled);
	}
    }
    if (have_bucket) {
	const double tolerance_squared = merge_tolerance * merge_tolerance;
	for (int dx = -1; dx <= 1; ++dx) {
	    for (int dy = -1; dy <= 1; ++dy) {
		for (int dz = -1; dz <= 1; ++dz) {
		    const std::array<int64_t, 3> nearby = {
			bucket[0] + dx, bucket[1] + dy, bucket[2] + dz
		    };
		    std::map<std::array<int64_t, 3>, std::vector<int> >::const_iterator
			candidates = vertex_buckets.find(nearby);
		    if (candidates == vertex_buckets.end()) continue;
		    for (std::vector<int>::const_iterator candidate =
			    candidates->second.begin();
			 candidate != candidates->second.end(); ++candidate) {
			const size_t offset = static_cast<size_t>(*candidate) * 3;
			const double x = vertices[offset] - point[0];
			const double y = vertices[offset + 1] - point[1];
			const double z = vertices[offset + 2] - point[2];
			if (x * x + y * y + z * z <= tolerance_squared) {
			    vertex_map[key] = *candidate;
			    ++merged_by_tolerance;
			    return *candidate;
			}
		    }
		    }
		}
	    }
	}

	const int index = static_cast<int>(vertices.size() / 3);
    vertices.push_back(coordinates.points[source]);
    vertices.push_back(coordinates.points[source + 1]);
    vertices.push_back(coordinates.points[source + 2]);
    vertex_map[key] = index;
    exact_vertices[point] = index;
    if (have_bucket) vertex_buckets[bucket].push_back(index);
    return index;
}

bool
brlcad::step::TessellatedMeshBuilder::ValidateIndexing(
    const TessellatedCoordinateData &coordinates, int pnmax,
    const std::vector<int> &pnindex,
    const std::vector<std::vector<double> > &normals)
{
    if (pnmax < 1 || (!pnindex.empty() && static_cast<int>(pnindex.size()) != pnmax) ||
	    (pnindex.empty() && static_cast<size_t>(pnmax) != coordinates.points.size() / 3) ||
	    (!normals.empty() && normals.size() != 1 &&
	     static_cast<int>(normals.size()) != pnmax)) {
	error = "pnmax, pnindex, coordinates, and normal counts are inconsistent";
	return false;
    }
    for (size_t index = 0; index < pnindex.size(); ++index) {
	if (pnindex[index] < 1 ||
		static_cast<size_t>(pnindex[index]) > coordinates.points.size() / 3) {
	    error = "pnindex contains an out-of-range coordinate index";
	    return false;
	}
    }
    for (size_t normal = 0; normal < normals.size(); ++normal) {
	if (normals[normal].size() != 3) {
	    error = "normal list contains a vector with the wrong width";
	    return false;
	}
	const double magnitude = std::sqrt(normals[normal][0] * normals[normal][0] +
	    normals[normal][1] * normals[normal][1] +
	    normals[normal][2] * normals[normal][2]);
	if (!std::isfinite(magnitude) || magnitude <= SMALL_FASTF) {
	    error = "normal list contains a zero or non-finite vector";
	    return false;
	}
    }
    return true;
}

bool
brlcad::step::TessellatedMeshBuilder::AddTriangle(
    const TessellatedCoordinateData &coordinates,
    const std::vector<int> &pnindex, int pnmax,
    const std::vector<std::vector<double> > &normals,
    int first, int second, int third)
{
    const int logical[3] = {first, second, third};
    int coordinate_indices[3] = {0, 0, 0};
    for (size_t i = 0; i < 3; ++i) {
	if (logical[i] < 1 || logical[i] > pnmax) {
	    error = "triangle point-normal index is out of range";
	    return false;
	}
	coordinate_indices[i] = pnindex.empty() ? logical[i] :
	    pnindex[static_cast<size_t>(logical[i] - 1)];
    }
    int indices[3] = {
	Vertex(coordinates, coordinate_indices[0]),
	Vertex(coordinates, coordinate_indices[1]),
	Vertex(coordinates, coordinate_indices[2])
    };
    if (indices[0] < 0 || indices[1] < 0 || indices[2] < 0) return false;
    if (indices[0] == indices[1] || indices[1] == indices[2] ||
	    indices[2] == indices[0]) {
	if (repair_repeated_triangles) {
	    ++discarded_repeated_triangles;
	    return true;
	}
	error = "triangle contains repeated vertices";
	return false;
    }

    const fastf_t *a = &vertices[static_cast<size_t>(indices[0]) * 3];
    const fastf_t *b = &vertices[static_cast<size_t>(indices[1]) * 3];
    const fastf_t *c = &vertices[static_cast<size_t>(indices[2]) * 3];
    double ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    double ac[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    double cross[3] = {
	ab[1] * ac[2] - ab[2] * ac[1],
	ab[2] * ac[0] - ab[0] * ac[2],
	ab[0] * ac[1] - ab[1] * ac[0]
    };
    const double area_squared = VDOT(cross, cross);
    if (!std::isfinite(area_squared) || area_squared <= 1.0e-24) {
	error = "triangle is geometrically degenerate";
	return false;
    }

    if (!normals.empty()) {
	double expected[3] = {0.0, 0.0, 0.0};
	if (normals.size() == 1) {
	    for (size_t i = 0; i < 3; ++i) expected[i] = normals[0][i];
	} else {
	    for (size_t vertex = 0; vertex < 3; ++vertex)
		for (size_t i = 0; i < 3; ++i)
		    expected[i] += normals[static_cast<size_t>(logical[vertex] - 1)][i];
	}
	const double normal_length = MAGNITUDE(expected);
	if (!std::isfinite(normal_length) || normal_length <= SMALL_FASTF) {
	    error = "triangle normals do not define a usable orientation";
	    return false;
	}
	if (VDOT(cross, expected) < 0.0) {
	    if (!repair_winding) {
		error = "triangle winding opposes its supplied normal";
		return false;
	    }
	    std::swap(indices[1], indices[2]);
	    ++reversed_by_normals;
	}
    }
    faces.insert(faces.end(), indices, indices + 3);
    return true;
}

bool
brlcad::step::TessellatedMeshBuilder::Closed() const
{
    if (faces.size() < 12) return false;
    struct EdgeUse { size_t count = 0; int balance = 0; };
    std::map<std::pair<int, int>, EdgeUse> edges;
    for (size_t face = 0; face < faces.size(); face += 3) {
	for (size_t edge = 0; edge < 3; ++edge) {
	    const int first = faces[face + edge];
	    const int second = faces[face + (edge + 1) % 3];
	    const std::pair<int, int> key(std::min(first, second),
		std::max(first, second));
	    EdgeUse &use = edges[key];
	    ++use.count;
	    use.balance += first < second ? 1 : -1;
	}
    }
    for (std::map<std::pair<int, int>, EdgeUse>::const_iterator edge = edges.begin();
	 edge != edges.end(); ++edge)
	if (edge->second.count != 2 || edge->second.balance != 0) return false;
    return !edges.empty();
}
