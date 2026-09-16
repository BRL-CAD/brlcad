/*             B U I L D I N G _ G E O M E T R Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file building_geometry.cpp
 *
 * BRL-CAD realization of validated procedural building specifications.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "bu/file.h"
#include "raytrace.h"
#include "rt/db_attr.h"
#include "rt/geom.h"
#include "wdb.h"

#include "building.h"

namespace building {
namespace {

constexpr double MM_PER_METER = 1000.0;
constexpr double PI = 3.14159265358979323846;
constexpr double GEOMETRY_EPSILON_METERS = 0.002;
constexpr double AXIS_ALIGNMENT_TOLERANCE_METERS = 1.0e-9;

struct mesh {
    std::vector<fastf_t> vertices;
    std::vector<int> faces;
};

struct roof_mesh {
    std::vector<point3> vertices;
    std::vector<int> faces;
    std::string realization;
};

struct writer_state {
    struct rt_wdb *fp = nullptr;
    generation_report report;
    std::set<std::string> names;
};

double
distance(const point2 &a, const point2 &b)
{
    return std::hypot(b.x - a.x, b.y - a.y);
}

point2
operator+(const point2 &a, const point2 &b)
{
    return {a.x + b.x, a.y + b.y};
}

point2
operator-(const point2 &a, const point2 &b)
{
    return {a.x - b.x, a.y - b.y};
}

point2
operator*(const point2 &a, double scale)
{
    return {a.x * scale, a.y * scale};
}

double
cross(const point2 &a, const point2 &b, const point2 &c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

double
signed_area(const std::vector<point2> &points)
{
    double area = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
	const point2 &a = points[i];
	const point2 &b = points[(i + 1) % points.size()];
	area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5;
}

bool
axis_aligned_rectangle(const std::vector<point2> &points)
{
    if (points.size() != 4)
	return false;
    size_t horizontal_edges = 0;
    size_t vertical_edges = 0;
    for (size_t i = 0; i < points.size(); ++i) {
	const point2 &a = points[i];
	const point2 &b = points[(i + 1) % points.size()];
	const bool horizontal = std::fabs(a.y - b.y) <= AXIS_ALIGNMENT_TOLERANCE_METERS;
	const bool vertical = std::fabs(a.x - b.x) <= AXIS_ALIGNMENT_TOLERANCE_METERS;
	if (horizontal == vertical)
	    return false;
	horizontal_edges += horizontal ? 1 : 0;
	vertical_edges += vertical ? 1 : 0;
    }
    return horizontal_edges == 2 && vertical_edges == 2;
}

bool
point_in_triangle(const point2 &point, const point2 &a, const point2 &b, const point2 &c)
{
    constexpr double epsilon = 1.0e-12;
    const double c1 = cross(a, b, point);
    const double c2 = cross(b, c, point);
    const double c3 = cross(c, a, point);
    return c1 >= -epsilon && c2 >= -epsilon && c3 >= -epsilon;
}

std::vector<std::array<int, 3>>
triangulate(const std::vector<point2> &points)
{
    if (points.size() < 3)
	throw std::runtime_error("cannot triangulate a polygon with fewer than three points");
    std::vector<int> remaining;
    remaining.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i)
	remaining.push_back(static_cast<int>(i));
    if (signed_area(points) < 0.0)
	std::reverse(remaining.begin(), remaining.end());
    std::vector<std::array<int, 3>> result;
    result.reserve(points.size() - 2);
    size_t attempts = 0;
    while (remaining.size() > 3) {
	bool clipped = false;
	for (size_t i = 0; i < remaining.size(); ++i) {
	    const int previous = remaining[(i + remaining.size() - 1) % remaining.size()];
	    const int current = remaining[i];
	    const int next = remaining[(i + 1) % remaining.size()];
	    if (cross(points[static_cast<size_t>(previous)], points[static_cast<size_t>(current)], points[static_cast<size_t>(next)]) <= 1.0e-12)
		continue;
	    bool contains_vertex = false;
	    for (int candidate : remaining) {
		if (candidate == previous || candidate == current || candidate == next)
		    continue;
		if (point_in_triangle(points[static_cast<size_t>(candidate)], points[static_cast<size_t>(previous)],
			points[static_cast<size_t>(current)], points[static_cast<size_t>(next)])) {
		    contains_vertex = true;
		    break;
		}
	    }
	    if (contains_vertex)
		continue;
	    result.push_back({previous, current, next});
	    remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
	    clipped = true;
	    break;
	}
	if (!clipped || ++attempts > points.size() * points.size())
	    throw std::runtime_error("unable to triangulate footprint; check for duplicate or collinear vertices");
    }
    result.push_back({remaining[0], remaining[1], remaining[2]});
    return result;
}

point2
line_intersection(point2 p, point2 direction_p, point2 q, point2 direction_q, bool &ok)
{
    const double denominator = direction_p.x * direction_q.y - direction_p.y * direction_q.x;
    if (std::fabs(denominator) < 1.0e-12) {
	ok = false;
	return p;
    }
    const point2 difference = q - p;
    const double t = (difference.x * direction_q.y - difference.y * direction_q.x) / denominator;
    ok = true;
    return p + direction_p * t;
}

std::vector<point2>
offset_polygon(const std::vector<point2> &points, double outward_distance)
{
    if (std::fabs(outward_distance) < 1.0e-12)
	return points;
    std::vector<point2> result;
    result.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
	const point2 &previous = points[(i + points.size() - 1) % points.size()];
	const point2 &current = points[i];
	const point2 &next = points[(i + 1) % points.size()];
	const point2 previous_direction = current - previous;
	const point2 next_direction = next - current;
	const double previous_length = std::hypot(previous_direction.x, previous_direction.y);
	const double next_length = std::hypot(next_direction.x, next_direction.y);
	const point2 previous_normal = {previous_direction.y / previous_length, -previous_direction.x / previous_length};
	const point2 next_normal = {next_direction.y / next_length, -next_direction.x / next_length};
	bool ok = false;
	const point2 intersection = line_intersection(
	    current + previous_normal * outward_distance, previous_direction,
	    current + next_normal * outward_distance, next_direction, ok);
	if (ok) {
	    result.push_back(intersection);
	} else {
	    const point2 average = previous_normal + next_normal;
	    const double average_length = std::hypot(average.x, average.y);
	    result.push_back(current + (average_length > 1.0e-12 ? average * (outward_distance / average_length) : next_normal * outward_distance));
	}
    }
    if (result.size() < 3 || signed_area(result) <= 1.0e-9)
	return points;
    return result;
}

bool
point_in_polygon(const point2 &point, const std::vector<point2> &polygon)
{
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
	const point2 &a = polygon[i];
	const point2 &b = polygon[j];
	const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
	    (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x);
	if (crosses)
	    inside = !inside;
    }
    return inside;
}

std::string
sanitize_name(const std::string &raw)
{
    std::string result;
    result.reserve(raw.size());
    for (unsigned char c : raw) {
	if (std::isalnum(c) || c == '_' || c == '-' || c == '.')
	    result.push_back(static_cast<char>(c));
	else
	    result.push_back('_');
    }
    while (!result.empty() && result.front() == '.')
	result.erase(result.begin());
    if (result.empty())
	result = "building";
    return result;
}

void
reserve_name(writer_state &state, const std::string &name)
{
    if (!state.names.insert(name).second || db_lookup(state.fp->dbip, name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	throw std::runtime_error("output object name already exists: " + name);
}

void
set_attribute(writer_state &state, const std::string &object, const std::string &key, const std::string &value)
{
    if (db5_update_attribute(object.c_str(), key.c_str(), value.c_str(), state.fp->dbip) != 0)
	throw std::runtime_error("unable to set attribute '" + key + "' on " + object);
}

std::string
number_string(double value)
{
    std::ostringstream out;
    out.precision(12);
    out << value;
    return out.str();
}

void
add_vertex(mesh &result, const point2 &point, double z)
{
    result.vertices.push_back(static_cast<fastf_t>(point.x * MM_PER_METER));
    result.vertices.push_back(static_cast<fastf_t>(point.y * MM_PER_METER));
    result.vertices.push_back(static_cast<fastf_t>(z * MM_PER_METER));
}

mesh
prism_mesh(const std::vector<point2> &polygon, double z0, double z1)
{
    mesh result;
    const auto triangles = triangulate(polygon);
    const int count = static_cast<int>(polygon.size());
    result.vertices.reserve(polygon.size() * 6);
    for (const point2 &point : polygon)
	add_vertex(result, point, z0);
    for (const point2 &point : polygon)
	add_vertex(result, point, z1);
    for (const auto &triangle : triangles) {
	result.faces.insert(result.faces.end(), {triangle[2], triangle[1], triangle[0]});
	result.faces.insert(result.faces.end(), {triangle[0] + count, triangle[1] + count, triangle[2] + count});
    }
    for (int i = 0; i < count; ++i) {
	const int next = (i + 1) % count;
	result.faces.insert(result.faces.end(), {i, next, next + count});
	result.faces.insert(result.faces.end(), {i, next + count, i + count});
    }
    return result;
}

void
write_solid_bot(writer_state &state, const std::string &name, mesh &geometry)
{
    reserve_name(state, name);
    if (mk_bot(state.fp, name.c_str(), RT_BOT_SOLID, RT_BOT_CCW, 0,
	geometry.vertices.size() / 3, geometry.faces.size() / 3,
	geometry.vertices.data(), geometry.faces.data(), nullptr, nullptr) != 0)
	throw std::runtime_error("unable to write solid BoT " + name);
    ++state.report.primitive_count;
}

void
write_prism(writer_state &state, const std::string &name, const std::vector<point2> &polygon, double z0, double z1)
{
    mesh geometry = prism_mesh(polygon, z0, z1);
    write_solid_bot(state, name, geometry);
}

void
add_upward_face(roof_mesh &roof, int a, int b, int c)
{
    const point3 &p0 = roof.vertices[static_cast<size_t>(a)];
    const point3 &p1 = roof.vertices[static_cast<size_t>(b)];
    const point3 &p2 = roof.vertices[static_cast<size_t>(c)];
    const double normal_z = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    if (normal_z >= 0.0)
	roof.faces.insert(roof.faces.end(), {a, b, c});
    else
	roof.faces.insert(roof.faces.end(), {a, c, b});
}

mesh
roof_solid_mesh(const roof_mesh &roof, double thickness)
{
    mesh result;
    result.vertices.reserve(roof.vertices.size() * 6);
    for (const point3 &point : roof.vertices) {
	result.vertices.insert(result.vertices.end(), {
	    static_cast<fastf_t>(point.x * MM_PER_METER),
	    static_cast<fastf_t>(point.y * MM_PER_METER),
	    static_cast<fastf_t>(point.z * MM_PER_METER)
	});
    }
    for (const point3 &point : roof.vertices) {
	result.vertices.insert(result.vertices.end(), {
	    static_cast<fastf_t>(point.x * MM_PER_METER),
	    static_cast<fastf_t>(point.y * MM_PER_METER),
	    static_cast<fastf_t>((point.z - thickness) * MM_PER_METER)
	});
    }

    const int vertex_count = static_cast<int>(roof.vertices.size());
    std::map<std::pair<int, int>, std::pair<std::pair<int, int>, size_t>> edges;
    for (size_t i = 0; i < roof.faces.size(); i += 3) {
	const int a = roof.faces[i];
	const int b = roof.faces[i + 1];
	const int c = roof.faces[i + 2];
	result.faces.insert(result.faces.end(), {a, b, c});
	result.faces.insert(result.faces.end(), {c + vertex_count, b + vertex_count, a + vertex_count});
	const std::pair<int, int> directed_edges[] = {{a, b}, {b, c}, {c, a}};
	for (const auto &edge : directed_edges) {
	    const auto key = std::minmax(edge.first, edge.second);
	    auto position = edges.find(key);
	    if (position == edges.end())
		edges.emplace(key, std::make_pair(edge, 1));
	    else
		++position->second.second;
	}
    }
    for (const auto &entry : edges) {
	if (entry.second.second != 1)
	    continue;
	const int a = entry.second.first.first;
	const int b = entry.second.first.second;
	result.faces.insert(result.faces.end(), {
	    a, a + vertex_count, b + vertex_count,
	    a, b + vertex_count, b
	});
    }
    return result;
}

void
write_segment_box(
    writer_state &state,
    const std::string &name,
    const point2 &start,
    const point2 &end,
    double along_start,
    double along_end,
    double side_start,
    double side_end,
    double z0,
    double z1)
{
    const double length = distance(start, end);
    const point2 tangent = {(end.x - start.x) / length, (end.y - start.y) / length};
    const point2 inward = {-tangent.y, tangent.x};
    const point2 a = start + tangent * along_start;
    const point2 b = start + tangent * along_end;
    const point2 corners[4] = {
	a + inward * side_start, b + inward * side_start,
	b + inward * side_end, a + inward * side_end
    };
    point_t points[8];
    for (size_t i = 0; i < 4; ++i) {
	VSET(points[i], corners[i].x * MM_PER_METER, corners[i].y * MM_PER_METER, z0 * MM_PER_METER);
	VSET(points[i + 4], corners[i].x * MM_PER_METER, corners[i].y * MM_PER_METER, z1 * MM_PER_METER);
    }
    reserve_name(state, name);
    if (mk_arb8(state.fp, name.c_str(), &points[0][X]) != 0)
	throw std::runtime_error("unable to write oriented box " + name);
    ++state.report.primitive_count;
}

void
write_axis_box(writer_state &state, const std::string &name, point3 minimum, point3 maximum)
{
    point_t p0, p1;
    VSET(p0, minimum.x * MM_PER_METER, minimum.y * MM_PER_METER, minimum.z * MM_PER_METER);
    VSET(p1, maximum.x * MM_PER_METER, maximum.y * MM_PER_METER, maximum.z * MM_PER_METER);
    reserve_name(state, name);
    if (mk_rpp(state.fp, name.c_str(), p0, p1) != 0)
	throw std::runtime_error("unable to write box " + name);
    ++state.report.primitive_count;
}

void
write_beam_segment(
    writer_state &state,
    const std::string &name,
    const point3 &start,
    const point3 &end,
    double width,
    double depth)
{
    const double length = std::hypot(end.x - start.x, end.y - start.y);
    if (length <= GEOMETRY_EPSILON_METERS)
	throw std::runtime_error("beam endpoints are coincident");
    const point2 normal = {-(end.y - start.y) / length, (end.x - start.x) / length};
    const point2 offset = normal * (width * 0.5);
    const point2 start_xy = {start.x, start.y};
    const point2 end_xy = {end.x, end.y};
    const point2 corners[4] = {
	start_xy - offset, end_xy - offset, end_xy + offset, start_xy + offset
    };
    point_t points[8];
    VSET(points[0], corners[0].x * MM_PER_METER, corners[0].y * MM_PER_METER, (start.z - depth) * MM_PER_METER);
    VSET(points[1], corners[1].x * MM_PER_METER, corners[1].y * MM_PER_METER, (end.z - depth) * MM_PER_METER);
    VSET(points[2], corners[2].x * MM_PER_METER, corners[2].y * MM_PER_METER, (end.z - depth) * MM_PER_METER);
    VSET(points[3], corners[3].x * MM_PER_METER, corners[3].y * MM_PER_METER, (start.z - depth) * MM_PER_METER);
    VSET(points[4], corners[0].x * MM_PER_METER, corners[0].y * MM_PER_METER, start.z * MM_PER_METER);
    VSET(points[5], corners[1].x * MM_PER_METER, corners[1].y * MM_PER_METER, end.z * MM_PER_METER);
    VSET(points[6], corners[2].x * MM_PER_METER, corners[2].y * MM_PER_METER, end.z * MM_PER_METER);
    VSET(points[7], corners[3].x * MM_PER_METER, corners[3].y * MM_PER_METER, start.z * MM_PER_METER);
    reserve_name(state, name);
    if (mk_arb8(state.fp, name.c_str(), &points[0][X]) != 0)
	throw std::runtime_error("unable to write roof beam " + name);
    ++state.report.primitive_count;
}

void
write_cylinder(writer_state &state, const std::string &name, point3 base, double height, double radius)
{
    point_t base_point;
    vect_t height_vector;
    VSET(base_point, base.x * MM_PER_METER, base.y * MM_PER_METER, base.z * MM_PER_METER);
    VSET(height_vector, 0.0, 0.0, height * MM_PER_METER);
    reserve_name(state, name);
    if (mk_rcc(state.fp, name.c_str(), base_point, height_vector, radius * MM_PER_METER) != 0)
	throw std::runtime_error("unable to write cylinder " + name);
    ++state.report.primitive_count;
}

void
write_region(
    writer_state &state,
    const std::string &name,
    struct wmember &members,
    const std::string &material,
    rgb_color color)
{
    reserve_name(state, name);
    unsigned char rgb[] = {color.r, color.g, color.b};
    const bool glass = material == "glass" || material == "mirror";
    const char *shader = glass ? "glass" : "plastic";
    const char *parameters = glass ? "tr=.65 ri=1.5" : "sh=12 sp=.25 di=.75";
    if (mk_lcomb(state.fp, name.c_str(), &members, 1, shader, parameters, rgb, 0) != 0)
	throw std::runtime_error("unable to write region " + name);
    set_attribute(state, name, "building::material", material);
    ++state.report.region_count;
}

void
write_group(writer_state &state, const std::string &name, struct wmember &members)
{
    reserve_name(state, name);
    if (mk_lcomb(state.fp, name.c_str(), &members, 0, nullptr, nullptr, nullptr, 0) != 0)
	throw std::runtime_error("unable to write group " + name);
}

void
subtract_members(struct wmember &members, const std::vector<std::string> &cutters)
{
    for (const std::string &cutter : cutters)
	(void)mk_addmember(cutter.c_str(), &members.l, nullptr, WMOP_SUBTRACT);
}

double
sum(const std::vector<double> &values, size_t begin = 0, size_t end = std::numeric_limits<size_t>::max())
{
    end = std::min(end, values.size());
    double result = 0.0;
    for (size_t i = begin; i < end; ++i)
	result += values[i];
    return result;
}

std::string
join(const std::vector<std::string> &values)
{
    std::ostringstream result;
    for (size_t i = 0; i < values.size(); ++i) {
	if (i > 0)
	    result << ';';
	result << values[i];
    }
    return result.str();
}

double
facade_top(const building_spec &spec)
{
    if (spec.total_height > 0.0)
	return spec.origin.z + spec.total_height - effective_roof_height(spec);
    return spec.origin.z + sum(spec.floor_heights);
}

double
facade_base(const building_spec &spec)
{
    if (spec.min_height >= 0.0)
	return spec.origin.z + spec.min_height;
    return spec.origin.z + sum(spec.floor_heights, 0, static_cast<size_t>(spec.min_level));
}

double
basement_bottom(const building_spec &spec)
{
    return spec.origin.z - sum(spec.basement_heights);
}

double
level_z(const building_spec &spec, int level)
{
    if (level >= 0)
	return spec.origin.z + sum(spec.floor_heights, 0, static_cast<size_t>(level));
    return spec.origin.z - sum(spec.basement_heights, 0, static_cast<size_t>(-level));
}

std::vector<point2>
world_footprint(const building_spec &spec)
{
    std::vector<point2> result = spec.footprint;
    for (point2 &point : result) {
	point.x += spec.origin.x;
	point.y += spec.origin.y;
    }
    return result;
}

std::vector<opening_spec>
automatic_openings(const building_spec &spec, const std::vector<point2> &footprint)
{
    if (!spec.auto_openings || !spec.openings.empty() || !spec.walls)
	return spec.openings;
    std::vector<opening_spec> result;
    const bool garage = spec.type == "garage" || spec.type == "garages";
    const bool bunker = spec.type == "bunker";
    opening_spec door;
    door.id = garage ? "overhead_door" : (bunker ? "armored_door" : "main_door");
    door.kind = "door";
    door.type = garage ? "overhead" : (bunker ? "armored" : "main");
    door.wall = 0;
    door.level = spec.min_level;
    door.width = garage ? 3.2 : (bunker ? 1.2 : 1.0);
    door.height = garage ? 2.4 : 2.1;
    door.sill = 0.0;
    door.material = garage || bunker ? "metal" : "wood";
    if (bunker)
	door.color = {75, 82, 82};
    const double front_length = distance(footprint[0], footprint[1]);
    door.offset = std::max(0.25, (front_length - door.width) * 0.5);
    const bool door_added = door.offset + door.width < front_length - 0.1;
    if (door_added)
	result.push_back(door);
    if (garage || bunker)
	return result;

    const bool sparse = spec.type == "warehouse" || spec.type == "barn" || spec.type == "church" || spec.type == "sports_hall";
    for (int level = spec.min_level; level < spec.levels_above; ++level) {
	const double storey_height = spec.floor_heights[static_cast<size_t>(level)];
	for (size_t wall = 0; wall < footprint.size(); ++wall) {
	    if (door_added && level == door.level && wall == door.wall)
		continue;
	    const double wall_length = distance(footprint[wall], footprint[(wall + 1) % footprint.size()]);
	    const double margin = std::min(1.2, wall_length * 0.16);
	    const double desired_spacing = sparse ? 5.0 : (spec.type == "office" ? 2.2 : 3.0);
	    int count = static_cast<int>(std::floor((wall_length - 2.0 * margin) / desired_spacing));
	    count = std::max(0, std::min(count, 16));
	    if (count == 0)
		continue;
	    opening_spec window;
	    window.id = "windows_l" + std::to_string(level) + "_w" + std::to_string(wall);
	    window.kind = "window";
	    window.type = spec.type == "office" ? "curtain_wall_panel" : "yes";
	    window.wall = wall;
	    window.level = level;
	    window.width = std::min(spec.type == "office" ? 1.5 : 1.3, (wall_length - 2.0 * margin) / static_cast<double>(count) * 0.68);
	    window.height = std::min(spec.type == "church" ? 3.6 : 1.5, storey_height - 0.8);
	    window.sill = spec.type == "church" ? 1.5 : 0.85;
	    if (window.sill + window.height > storey_height - 0.15)
		window.height = storey_height - window.sill - 0.15;
	    window.count = count;
	    window.spacing = count == 1 ? 0.0 : (wall_length - 2.0 * margin - window.width) / static_cast<double>(count - 1);
	    window.offset = margin;
	    window.material = "glass";
	    window.color = {90, 120, 140};
	    result.push_back(window);
	}
    }
    return result;
}

std::vector<double>
roof_profile(const std::string &raw_shape, double height)
{
    const std::string shape = raw_shape;
    if (shape == "skillion")
	return {0.0, 0.0, 1.0, height};
    if (shape == "saltbox" || shape == "gabled_height_moved")
	return {0.0, 0.0, 0.36, height, 1.0, 0.0};
    if (shape == "gambrel" || shape == "bellcast_gable")
	return {0.0, 0.0, 0.20, height * 0.64, 0.50, height, 0.80, height * 0.64, 1.0, 0.0};
    if (shape == "mansard" || shape == "equal_mansard")
	return {0.0, 0.0, 0.18, height * 0.78, 0.34, height, 0.66, height, 0.82, height * 0.78, 1.0, 0.0};
    if (shape == "butterfly")
	return {0.0, height, 0.50, 0.0, 1.0, height};
    if (shape == "sawtooth")
	return {0.0, 0.0, 0.20, height, 0.22, 0.0, 0.45, height, 0.47, 0.0, 0.70, height, 0.72, 0.0, 1.0, height};
    if (shape == "round" || shape == "parabolic" || shape == "round_gabled") {
	std::vector<double> result;
	constexpr size_t segments = 12;
	for (size_t i = 0; i <= segments; ++i) {
	    const double t = static_cast<double>(i) / static_cast<double>(segments);
	    result.push_back(t);
	    result.push_back(shape == "parabolic" ? height * 4.0 * t * (1.0 - t) : height * std::sin(PI * t));
	}
	return result;
    }
    return {0.0, 0.0, 0.50, height, 1.0, 0.0};
}

std::string
normalized_roof_shape(std::string shape)
{
    std::replace(shape.begin(), shape.end(), '-', '_');
    return shape;
}

bool
level_roof_shape(const std::string &shape)
{
    const std::string normalized = normalized_roof_shape(shape);
    return normalized == "flat" || normalized == "many";
}

double
roof_solid_depth(const building_spec &spec)
{
    return level_roof_shape(spec.roof.shape) ? effective_roof_height(spec) : spec.roof.thickness;
}

roof_mesh
make_profile_roof(
    const std::vector<point2> &footprint,
    double eave,
    double height,
    const std::string &shape,
    bool across,
    bool reverse_profile)
{
    roof_mesh result;
    result.realization = shape;
    if (footprint.size() != 4) {
	point2 center;
	for (const point2 &point : footprint) {
	    center.x += point.x;
	    center.y += point.y;
	    result.vertices.push_back({point.x, point.y, eave});
	}
	center.x /= static_cast<double>(footprint.size());
	center.y /= static_cast<double>(footprint.size());
	result.vertices.push_back({center.x, center.y, eave + height});
	const int apex = static_cast<int>(result.vertices.size() - 1);
	for (size_t i = 0; i < footprint.size(); ++i)
	    add_upward_face(result, static_cast<int>(i), static_cast<int>((i + 1) % footprint.size()), apex);
	result.realization += "_pyramidal_fallback";
	return result;
    }
    size_t p0 = 0, p1 = 1, p2 = 2, p3 = 3;
    const bool edge01_longer = distance(footprint[0], footprint[1]) >= distance(footprint[1], footprint[2]);
    const bool cross_on_03 = edge01_longer != across;
    if (!cross_on_03) {
	p0 = 1; p1 = 2; p2 = 3; p3 = 0;
    }
    const std::vector<double> profile = roof_profile(shape, height);
    for (size_t i = 0; i < profile.size(); i += 2) {
	const double t = reverse_profile ? 1.0 - profile[i] : profile[i];
	const double z = eave + profile[i + 1];
	const point2 a = footprint[p0] * (1.0 - t) + footprint[p3] * t;
	const point2 b = footprint[p1] * (1.0 - t) + footprint[p2] * t;
	result.vertices.push_back({a.x, a.y, z});
	result.vertices.push_back({b.x, b.y, z});
    }
    for (size_t row = 0; row + 1 < result.vertices.size() / 2; ++row) {
	const int a = static_cast<int>(row * 2);
	const int b = a + 1;
	const int c = a + 3;
	const int d = a + 2;
	add_upward_face(result, a, b, c);
	add_upward_face(result, a, c, d);
    }
    return result;
}

roof_mesh
make_pyramid_roof(const std::vector<point2> &footprint, double eave, double height, const std::string &shape)
{
    roof_mesh result;
    result.realization = shape;
    point2 center;
    for (const point2 &point : footprint) {
	result.vertices.push_back({point.x, point.y, eave});
	center.x += point.x;
	center.y += point.y;
    }
    center.x /= static_cast<double>(footprint.size());
    center.y /= static_cast<double>(footprint.size());
    result.vertices.push_back({center.x, center.y, eave + height});
    const int apex = static_cast<int>(result.vertices.size() - 1);
    for (size_t i = 0; i < footprint.size(); ++i)
	add_upward_face(result, static_cast<int>(i), static_cast<int>((i + 1) % footprint.size()), apex);
    return result;
}

roof_mesh
make_hipped_roof(const std::vector<point2> &footprint, double eave, double height, const std::string &shape, bool across)
{
    if (footprint.size() != 4) {
	roof_mesh fallback = make_pyramid_roof(footprint, eave, height, shape);
	fallback.realization += "_pyramidal_fallback";
	return fallback;
    }
    roof_mesh result;
    result.realization = shape;
    for (const point2 &point : footprint)
	result.vertices.push_back({point.x, point.y, eave});
    const double edge01 = distance(footprint[0], footprint[1]);
    const double edge12 = distance(footprint[1], footprint[2]);
    bool ridge_along_01 = edge01 >= edge12;
    if (across)
	ridge_along_01 = !ridge_along_01;
    if (ridge_along_01) {
	const point2 left = (footprint[0] + footprint[3]) * 0.5;
	const point2 right = (footprint[1] + footprint[2]) * 0.5;
	const double inset = std::min(0.45, edge12 / std::max(edge01, 1.0e-9) * 0.5);
	const point2 r0 = left * (1.0 - inset) + right * inset;
	const point2 r1 = left * inset + right * (1.0 - inset);
	result.vertices.push_back({r0.x, r0.y, eave + height});
	result.vertices.push_back({r1.x, r1.y, eave + height});
	add_upward_face(result, 0, 1, 5); add_upward_face(result, 0, 5, 4);
	add_upward_face(result, 3, 4, 5); add_upward_face(result, 3, 5, 2);
	add_upward_face(result, 0, 4, 3); add_upward_face(result, 1, 2, 5);
    } else {
	const point2 bottom = (footprint[0] + footprint[1]) * 0.5;
	const point2 top = (footprint[3] + footprint[2]) * 0.5;
	const double inset = std::min(0.45, edge01 / std::max(edge12, 1.0e-9) * 0.5);
	const point2 r0 = bottom * (1.0 - inset) + top * inset;
	const point2 r1 = bottom * inset + top * (1.0 - inset);
	result.vertices.push_back({r0.x, r0.y, eave + height});
	result.vertices.push_back({r1.x, r1.y, eave + height});
	add_upward_face(result, 0, 1, 4); add_upward_face(result, 1, 5, 4);
	add_upward_face(result, 3, 4, 5); add_upward_face(result, 3, 5, 2);
	add_upward_face(result, 0, 4, 3); add_upward_face(result, 1, 2, 5);
    }
    return result;
}

roof_mesh
make_radial_roof(const std::vector<point2> &footprint, double eave, double height, const std::string &shape)
{
    if (shape == "cone")
	return make_pyramid_roof(footprint, eave, height, shape);
    roof_mesh result;
    result.realization = shape;
    point2 center;
    for (const point2 &point : footprint) {
	center.x += point.x;
	center.y += point.y;
    }
    center.x /= static_cast<double>(footprint.size());
    center.y /= static_cast<double>(footprint.size());
    constexpr size_t rings = 7;
    for (size_t ring = 0; ring < rings; ++ring) {
	const double t = static_cast<double>(ring) / static_cast<double>(rings);
	const double theta = t * PI * 0.5;
	double radius = std::cos(theta);
	double z_factor = std::sin(theta);
	if (shape == "onion") {
	    radius *= 1.0 + 0.34 * std::sin(theta * 2.0);
	    z_factor = std::pow(z_factor, 0.82);
	}
	for (const point2 &point : footprint) {
	    const point2 ring_point = center + (point - center) * radius;
	    result.vertices.push_back({ring_point.x, ring_point.y, eave + height * z_factor});
	}
    }
    const int count = static_cast<int>(footprint.size());
    for (size_t ring = 0; ring + 1 < rings; ++ring) {
	for (int i = 0; i < count; ++i) {
	    const int next = (i + 1) % count;
	    const int a = static_cast<int>(ring) * count + i;
	    const int b = static_cast<int>(ring) * count + next;
	    const int c = static_cast<int>(ring + 1) * count + next;
	    const int d = static_cast<int>(ring + 1) * count + i;
	    add_upward_face(result, a, b, c);
	    add_upward_face(result, a, c, d);
	}
    }
    result.vertices.push_back({center.x, center.y, eave + height});
    const int apex = static_cast<int>(result.vertices.size() - 1);
    const int last_ring = static_cast<int>(rings - 1) * count;
    for (int i = 0; i < count; ++i)
	add_upward_face(result, last_ring + i, last_ring + (i + 1) % count, apex);
    return result;
}

roof_mesh
make_flat_roof(const std::vector<point2> &footprint, double eave, const std::string &realization)
{
    roof_mesh result;
    result.realization = realization;
    for (const point2 &point : footprint)
	result.vertices.push_back({point.x, point.y, eave});
    for (const auto &face : triangulate(footprint))
	add_upward_face(result, face[0], face[1], face[2]);
    return result;
}

struct roof_orientation {
    bool across = false;
    bool reverse_profile = false;
};

point2
profile_downhill_direction(const std::vector<point2> &footprint, bool across)
{
    size_t p0 = 0, p1 = 1, p2 = 2, p3 = 3;
    const bool edge01_longer = distance(footprint[0], footprint[1]) >= distance(footprint[1], footprint[2]);
    if (edge01_longer == across) {
	p0 = 1; p1 = 2; p2 = 3; p3 = 0;
    }
    const point2 low = (footprint[p0] + footprint[p1]) * 0.5;
    const point2 high = (footprint[p3] + footprint[p2]) * 0.5;
    const point2 direction = low - high;
    const double length = std::hypot(direction.x, direction.y);
    return direction * (1.0 / length);
}

roof_orientation
resolve_roof_orientation(const building_spec &spec, const std::vector<point2> &footprint)
{
    roof_orientation result;
    result.across = spec.roof.orientation == "across";
    if (spec.roof.direction_degrees < 0.0 || footprint.size() != 4)
	return result;
    const double radians = spec.roof.direction_degrees * PI / 180.0;
    const point2 requested = {std::sin(radians), std::cos(radians)};
    const point2 along_downhill = profile_downhill_direction(footprint, false);
    const point2 across_downhill = profile_downhill_direction(footprint, true);
    const double along_alignment = along_downhill.x * requested.x + along_downhill.y * requested.y;
    const double across_alignment = across_downhill.x * requested.x + across_downhill.y * requested.y;
    result.across = std::fabs(across_alignment) > std::fabs(along_alignment);
    const double alignment = result.across ? across_alignment : along_alignment;
    result.reverse_profile = alignment < 0.0;
    return result;
}

roof_mesh
build_roof_mesh(const building_spec &spec, const std::vector<point2> &footprint, double eave)
{
    const std::vector<point2> roof_footprint = offset_polygon(footprint, spec.roof.overhang);
    const std::string shape = normalized_roof_shape(spec.roof.shape);
    double height = effective_roof_height(spec);
    const roof_orientation orientation = resolve_roof_orientation(spec, roof_footprint);
    const double surface_eave = eave + spec.roof.thickness;
    height = std::max(0.0, height - spec.roof.thickness);
    if (shape == "flat" || shape == "many" || height <= 1.0e-9)
	return make_flat_roof(roof_footprint, surface_eave + std::max(0.0, height), shape == "many" ? "many_flat_fallback" : "flat");
    if (shape == "pyramidal" || shape == "crosspitched" || shape == "apse_gabled")
	return make_pyramid_roof(roof_footprint, surface_eave, height, shape);
    if (shape == "hipped" || shape == "half_hipped" || shape == "side_hipped" ||
	shape == "side_half_hipped" || shape == "hipped_and_gabled" || shape == "equal_hipped")
	return make_hipped_roof(roof_footprint, surface_eave, height, shape, orientation.across);
    if (shape == "cone" || shape == "dome" || shape == "half_dome" || shape == "onion")
	return make_radial_roof(roof_footprint, surface_eave, height, shape == "half_dome" ? "dome" : shape);
    const std::set<std::string> profile_shapes = {
	"gabled", "pitched", "gabled_height_moved", "skillion", "saltbox", "gambrel", "bellcast_gable",
	"mansard", "equal_mansard", "butterfly", "sawtooth", "round", "round_gabled", "parabolic"
    };
    if (profile_shapes.count(shape))
	return make_profile_roof(roof_footprint, surface_eave, height, shape == "pitched" ? "gabled" : shape,
	    orientation.across, orientation.reverse_profile);
    return make_flat_roof(roof_footprint, surface_eave + height, shape + "_flat_fallback");
}

void
add_roof_closures(
    writer_state &state,
    const building_spec &spec,
    const std::vector<point2> &footprint,
    const roof_mesh &roof,
    double eave,
    const std::string &prefix,
    const std::string &roof_solid,
    const std::set<size_t> &selected_walls,
    struct wmember &wall_members)
{
    const double roof_depth = roof_solid_depth(spec);
    double maximum_underside = eave;
    double maximum_surface = eave;
    for (const point3 &vertex : roof.vertices) {
	maximum_underside = std::max(maximum_underside, vertex.z - roof_depth);
	maximum_surface = std::max(maximum_surface, vertex.z);
    }
    if (maximum_underside <= eave + GEOMETRY_EPSILON_METERS)
	return;

    struct wmember closure_members;
    BU_LIST_INIT(&closure_members.l);
    for (size_t i = 0; i < footprint.size(); ++i) {
	if (!selected_walls.empty() && selected_walls.count(i) == 0)
	    continue;
	const double length = distance(footprint[i], footprint[(i + 1) % footprint.size()]);
	const std::string closure = prefix + "_roof_closure_" + std::to_string(i) + ".s";
	write_segment_box(state, closure, footprint[i], footprint[(i + 1) % footprint.size()],
	    -spec.wall_thickness * 0.5, length + spec.wall_thickness * 0.5,
	    0.0, spec.wall_thickness, eave, maximum_surface);
	(void)mk_addmember(closure.c_str(), &closure_members.l, nullptr, WMOP_UNION);
    }

    const std::string envelope = prefix + "_roof_envelope.s";
    mesh envelope_geometry = roof_solid_mesh(roof, maximum_surface - eave + GEOMETRY_EPSILON_METERS);
    write_solid_bot(state, envelope, envelope_geometry);
    (void)mk_addmember(envelope.c_str(), &closure_members.l, nullptr, WMOP_INTERSECT);
    (void)mk_addmember(roof_solid.c_str(), &closure_members.l, nullptr, WMOP_SUBTRACT);
    const std::string closure = prefix + "_roof_closure.c";
    write_group(state, closure, closure_members);
    (void)mk_addmember(closure.c_str(), &wall_members.l, nullptr, WMOP_UNION);
}

std::vector<double>
axis_positions(double minimum, double maximum, double spacing, double inset)
{
    const double low = minimum + inset;
    const double high = maximum - inset;
    if (high <= low)
	return {(minimum + maximum) * 0.5};
    std::vector<double> result = {low};
    for (double value = low + spacing; value < high - spacing * 0.25; value += spacing)
	result.push_back(value);
    if (high - result.back() > spacing * 0.25)
	result.push_back(high);
    return result;
}

void
add_opening_geometry(
    writer_state &state,
    const building_spec &spec,
    const std::vector<point2> &footprint,
    const opening_spec &opening,
    const std::string &opening_prefix,
    struct wmember &wall_members,
    struct wmember &opening_members)
{
    const point2 &wall_start = footprint[opening.wall];
    const point2 &wall_end = footprint[(opening.wall + 1) % footprint.size()];
    const double wall_depth = spec.wall_thickness;
    for (int repeat = 0; repeat < opening.count; ++repeat) {
	const double start = opening.offset + static_cast<double>(repeat) * opening.spacing;
	const double end = start + opening.width;
	const double z0 = level_z(spec, opening.level) + opening.sill;
	const double z1 = z0 + opening.height;
	const std::string base = opening_prefix + "_" + std::to_string(repeat);
	const std::string cutter = base + "_cut.s";
	write_segment_box(state, cutter, wall_start, wall_end, start, end,
	    -GEOMETRY_EPSILON_METERS, wall_depth + GEOMETRY_EPSILON_METERS,
	    z0 - GEOMETRY_EPSILON_METERS, z1 + GEOMETRY_EPSILON_METERS);
	(void)mk_addmember(cutter.c_str(), &wall_members.l, nullptr, WMOP_SUBTRACT);
	++state.report.opening_count;

	if (opening.kind == "opening")
	    continue;
	const double frame = std::min(opening.frame_width, std::min(opening.width, opening.height) * 0.24);
	const double frame_side0 = std::max(0.0, (wall_depth - opening.frame_depth) * 0.5);
	const double frame_side1 = std::min(wall_depth, frame_side0 + opening.frame_depth);
	auto add_frame_box = [&](const std::string &suffix, double along0, double along1, double bottom, double top) {
	    const std::string name = base + suffix;
	    write_segment_box(state, name, wall_start, wall_end, along0, along1, frame_side0, frame_side1, bottom, top);
	    (void)mk_addmember(name.c_str(), &opening_members.l, nullptr, WMOP_UNION);
	};
	add_frame_box("_frame_left.s", start, start + frame, z0, z1);
	add_frame_box("_frame_right.s", end - frame, end, z0, z1);
	add_frame_box("_frame_top.s", start + frame, end - frame, z1 - frame, z1);
	if (opening.kind == "window")
	    add_frame_box("_frame_bottom.s", start + frame, end - frame, z0, z0 + frame);
	const double panel_start = start + frame;
	const double panel_end = end - frame;
	const double panel_bottom = opening.kind == "window" ? z0 + frame : z0 + 0.015;
	const double panel_top = z1 - frame;
	if (panel_end > panel_start && panel_top > panel_bottom) {
	    const std::string panel = base + (opening.kind == "window" ? "_glass.s" : "_panel.s");
	    const double panel_depth = opening.kind == "window" ? 0.012 : std::min(0.05, opening.frame_depth);
	    const double panel_side0 = (wall_depth - panel_depth) * 0.5;
	    write_segment_box(state, panel, wall_start, wall_end, panel_start, panel_end,
		panel_side0, panel_side0 + panel_depth, panel_bottom, panel_top);
	    (void)mk_addmember(panel.c_str(), &opening_members.l, nullptr, WMOP_UNION);
	}
    }
}

void
write_structure(
    writer_state &state,
    const building_spec &spec,
    const std::vector<point2> &footprint,
    const roof_mesh &roof,
    const std::string &prefix,
    const std::vector<std::string> &part_cutters,
    struct wmember &building_members)
{
    if (!spec.structure.enabled)
	return;
    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    for (const point2 &point : footprint) {
	min_x = std::min(min_x, point.x); min_y = std::min(min_y, point.y);
	max_x = std::max(max_x, point.x); max_y = std::max(max_y, point.y);
    }
    const double member_half_width = std::max({spec.structure.column_width, spec.structure.column_depth, spec.structure.beam_width}) * 0.5;
    const double column_inset = (spec.walls ? spec.wall_thickness : 0.0) + member_half_width + GEOMETRY_EPSILON_METERS;
    const std::vector<double> x_positions = axis_positions(min_x, max_x, spec.structure.grid_x, column_inset);
    const std::vector<double> y_positions = axis_positions(min_y, max_y, spec.structure.grid_y, column_inset);
    const double roof_depth = roof_solid_depth(spec);
    auto roof_underside = [&roof, roof_depth](const point2 &position) {
	double height = -std::numeric_limits<double>::infinity();
	for (size_t i = 0; i < roof.faces.size(); i += 3) {
	    const point3 &a = roof.vertices[static_cast<size_t>(roof.faces[i])];
	    const point3 &b = roof.vertices[static_cast<size_t>(roof.faces[i + 1])];
	    const point3 &c = roof.vertices[static_cast<size_t>(roof.faces[i + 2])];
	    const double denominator = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
	    if (std::fabs(denominator) <= 1.0e-12)
		continue;
	    const double wa = ((b.y - c.y) * (position.x - c.x) + (c.x - b.x) * (position.y - c.y)) / denominator;
	    const double wb = ((c.y - a.y) * (position.x - c.x) + (a.x - c.x) * (position.y - c.y)) / denominator;
	    const double wc = 1.0 - wa - wb;
	    if (wa < -1.0e-9 || wb < -1.0e-9 || wc < -1.0e-9)
		continue;
	    height = std::max(height, wa * a.z + wb * b.z + wc * c.z - roof_depth);
	}
	if (!std::isfinite(height))
	    throw std::runtime_error("unable to locate structural support beneath roof");
	return height;
    };
    auto column_bearing_height = [&spec, &roof_underside](double x, double y) {
	const double half_x = spec.structure.column_shape == "round" ?
	    spec.structure.column_diameter * 0.5 : spec.structure.column_width * 0.5;
	const double half_y = spec.structure.column_shape == "round" ?
	    spec.structure.column_diameter * 0.5 : spec.structure.column_depth * 0.5;
	double height = std::numeric_limits<double>::infinity();
	const point2 corners[] = {
	    {x - half_x, y - half_y}, {x + half_x, y - half_y},
	    {x + half_x, y + half_y}, {x - half_x, y + half_y}
	};
	for (const point2 &corner : corners)
	    height = std::min(height, roof_underside(corner));
	return height;
    };

    std::vector<std::vector<bool>> grid(x_positions.size(), std::vector<bool>(y_positions.size(), false));
    for (size_t x = 0; x < x_positions.size(); ++x) {
	for (size_t y = 0; y < y_positions.size(); ++y)
	    grid[x][y] = point_in_polygon({x_positions[x], y_positions[y]}, footprint);
    }

    struct wmember members;
    BU_LIST_INIT(&members.l);
    size_t column_index = 0;
    size_t beam_index = 0;
    struct storey_span { double base; double ceiling; bool has_slab_above; bool roof_bearing; };
    std::vector<storey_span> storeys;
    for (int level = -spec.levels_below; level < 0; ++level)
	storeys.push_back({level_z(spec, level), level_z(spec, level + 1), true, false});
    for (int level = spec.min_level; level < spec.levels_above; ++level) {
	const bool last = level + 1 == spec.levels_above;
	storeys.push_back({level_z(spec, level), last ? facade_top(spec) : level_z(spec, level + 1), !last, last});
    }
    std::vector<double> beam_levels;
    for (const storey_span &storey : storeys) {
	for (size_t x = 0; x < x_positions.size(); ++x) {
	    for (size_t y = 0; y < y_positions.size(); ++y) {
		if (!grid[x][y])
		    continue;
		const double column_top = storey.roof_bearing ?
		    column_bearing_height(x_positions[x], y_positions[y]) :
		    storey.ceiling - (storey.has_slab_above ? spec.slab_thickness : 0.0);
		if (column_top <= storey.base)
		    continue;
		const std::string name = prefix + "_column_" + std::to_string(column_index++) + ".s";
		if (spec.structure.column_shape == "round") {
		    write_cylinder(state, name, {x_positions[x], y_positions[y], storey.base},
			column_top - storey.base, spec.structure.column_diameter * 0.5);
		} else {
		    write_axis_box(state, name,
			{x_positions[x] - spec.structure.column_width * 0.5,
			 y_positions[y] - spec.structure.column_depth * 0.5, storey.base},
			{x_positions[x] + spec.structure.column_width * 0.5,
			 y_positions[y] + spec.structure.column_depth * 0.5, column_top});
		}
		(void)mk_addmember(name.c_str(), &members.l, nullptr, WMOP_UNION);
		++state.report.column_count;
	    }
	}
	if (!storey.roof_bearing) {
	    beam_levels.push_back(storey.ceiling - (storey.has_slab_above ? spec.slab_thickness : 0.0));
	    continue;
	}

	const bool sloped_beams = normalized_roof_shape(spec.roof.shape) == "skillion";
	auto add_roof_beam = [&](size_t x0, size_t y0, size_t x1, size_t y1) {
	    if (!grid[x0][y0] || !grid[x1][y1])
		return;
	    point3 start = {x_positions[x0], y_positions[y0], facade_top(spec)};
	    point3 end = {x_positions[x1], y_positions[y1], facade_top(spec)};
	    if (sloped_beams) {
		const double length = std::hypot(end.x - start.x, end.y - start.y);
		const point2 lateral = {-(end.y - start.y) / length * spec.structure.beam_width * 0.5,
		    (end.x - start.x) / length * spec.structure.beam_width * 0.5};
		start.z = std::min(roof_underside({start.x - lateral.x, start.y - lateral.y}),
		    roof_underside({start.x + lateral.x, start.y + lateral.y}));
		end.z = std::min(roof_underside({end.x - lateral.x, end.y - lateral.y}),
		    roof_underside({end.x + lateral.x, end.y + lateral.y}));
	    }
	    const std::string name = prefix + "_roof_beam_" + std::to_string(beam_index++) + ".s";
	    write_beam_segment(state, name, start, end, spec.structure.beam_width, spec.structure.beam_depth);
	    (void)mk_addmember(name.c_str(), &members.l, nullptr, WMOP_UNION);
	    ++state.report.beam_count;
	};
	for (size_t x = 0; x < x_positions.size(); ++x) {
	    for (size_t y = 0; y < y_positions.size(); ++y) {
		if (x + 1 < x_positions.size())
		    add_roof_beam(x, y, x + 1, y);
		if (y + 1 < y_positions.size())
		    add_roof_beam(x, y, x, y + 1);
	    }
	}
    }
    for (double z : beam_levels) {
	for (double y : y_positions) {
	    const std::string name = prefix + "_beam_x_" + std::to_string(beam_index++) + ".s";
	    write_axis_box(state, name,
		{min_x + column_inset, y - spec.structure.beam_width * 0.5, z - spec.structure.beam_depth},
		{max_x - column_inset, y + spec.structure.beam_width * 0.5, z});
	    (void)mk_addmember(name.c_str(), &members.l, nullptr, WMOP_UNION);
	    ++state.report.beam_count;
	}
	for (double x : x_positions) {
	    const std::string name = prefix + "_beam_y_" + std::to_string(beam_index++) + ".s";
	    write_axis_box(state, name,
		{x - spec.structure.beam_width * 0.5, min_y + column_inset, z - spec.structure.beam_depth},
		{x + spec.structure.beam_width * 0.5, max_y - column_inset, z});
	    (void)mk_addmember(name.c_str(), &members.l, nullptr, WMOP_UNION);
	    ++state.report.beam_count;
	}
    }
    if (!BU_LIST_IS_EMPTY(&members.l)) {
	/* Rectangular grids are inset from every facade edge, so clipping them is
	 * redundant.  Avoiding that Boolean also works around ART dropping ARB/BoT
	 * intersections.  Arbitrary footprints still require an explicit clip. */
	if (!axis_aligned_rectangle(footprint)) {
	    const std::vector<point2> interior = spec.walls ? offset_polygon(footprint, -spec.wall_thickness) : footprint;
	    const std::string clip = prefix + "_structure_clip.s";
	    double structure_top = facade_top(spec);
	    for (const point3 &vertex : roof.vertices)
		structure_top = std::max(structure_top, vertex.z - roof_depth);
	    write_prism(state, clip, interior, basement_bottom(spec) - GEOMETRY_EPSILON_METERS,
		structure_top);
	    (void)mk_addmember(clip.c_str(), &members.l, nullptr, WMOP_INTERSECT);
	}
	subtract_members(members, part_cutters);
	const std::string region = prefix + "_structure.r";
	write_region(state, region, members, spec.structure.material, spec.structure.color);
	(void)mk_addmember(region.c_str(), &building_members.l, nullptr, WMOP_UNION);
    }
}

void
write_installations(
    writer_state &state,
    const building_spec &spec,
    const std::string &prefix,
    struct wmember &building_members)
{
    for (size_t i = 0; i < spec.installations.size(); ++i) {
	const installation_spec &installation = spec.installations[i];
	const std::string base = prefix + "_installation_" + sanitize_name(installation.id.empty() ? std::to_string(i) : installation.id);
	const std::string solid = base + ".s";
	const point3 position = {spec.origin.x + installation.position.x,
	    spec.origin.y + installation.position.y, spec.origin.z + installation.position.z};
	if (installation.shape == "cylinder") {
	    write_cylinder(state, solid,
		{position.x + installation.size.x * 0.5, position.y + installation.size.y * 0.5, position.z},
		installation.size.z, std::min(installation.size.x, installation.size.y) * 0.5);
	} else {
	    write_axis_box(state, solid, position,
		{position.x + installation.size.x, position.y + installation.size.y, position.z + installation.size.z});
	}
	struct wmember members;
	BU_LIST_INIT(&members.l);
	(void)mk_addmember(solid.c_str(), &members.l, nullptr, WMOP_UNION);
	const std::string region = base + ".r";
	write_region(state, region, members, installation.material, installation.color);
	for (const auto &attribute : installation.attributes)
	    set_attribute(state, region, "cityjson::" + attribute.first, attribute.second);
	set_attribute(state, region, "building::installation_kind", installation.kind);
	(void)mk_addmember(region.c_str(), &building_members.l, nullptr, WMOP_UNION);
    }
}

std::string write_building(writer_state &, const building_spec &, bool);

std::string
write_building(writer_state &state, const building_spec &spec, bool is_part)
{
    validate_spec(spec);
    if (is_part)
	++state.report.part_count;
    else
	++state.report.building_count;
    const std::string prefix = sanitize_name(spec.id);
    const std::vector<point2> footprint = world_footprint(spec);
    struct wmember building_members;
    BU_LIST_INIT(&building_members.l);

    std::vector<std::string> part_cutters;
    for (size_t i = 0; i < spec.parts.size(); ++i) {
	const building_spec &part = spec.parts[i];
	const std::string cutter = prefix + "_part_void_" + std::to_string(i) + ".s";
	const double bottom = basement_bottom(part) - part.foundation_thickness - GEOMETRY_EPSILON_METERS;
	const double top = facade_top(part) + effective_roof_height(part) + GEOMETRY_EPSILON_METERS;
	const double clearance = std::max(part.roof.overhang, part.wall_thickness * 0.5) + GEOMETRY_EPSILON_METERS;
	write_prism(state, cutter, offset_polygon(world_footprint(part), clearance), bottom, top);
	part_cutters.push_back(cutter);
    }

    const double wall_z1 = facade_top(spec);
    const roof_mesh roof = build_roof_mesh(spec, footprint, wall_z1);
    const std::string roof_solid = prefix + "_roof.bot";
    const double roof_solid_thickness = roof_solid_depth(spec);
    mesh roof_geometry = roof_solid_mesh(roof, roof_solid_thickness);
    write_solid_bot(state, roof_solid, roof_geometry);
    if (spec.walls && wall_z1 > facade_base(spec)) {
	struct wmember wall_members;
	BU_LIST_INIT(&wall_members.l);
	std::set<size_t> selected(spec.wall_indices.begin(), spec.wall_indices.end());
	for (size_t i = 0; i < footprint.size(); ++i) {
	    if (!selected.empty() && selected.count(i) == 0)
		continue;
	    const double length = distance(footprint[i], footprint[(i + 1) % footprint.size()]);
	    const std::string wall = prefix + "_wall_" + std::to_string(i) + "_above.s";
	    write_segment_box(state, wall, footprint[i], footprint[(i + 1) % footprint.size()],
		-spec.wall_thickness * 0.5, length + spec.wall_thickness * 0.5,
		0.0, spec.wall_thickness, facade_base(spec), wall_z1);
	    (void)mk_addmember(wall.c_str(), &wall_members.l, nullptr, WMOP_UNION);
	    if (spec.levels_below > 0) {
		const std::string basement_wall = prefix + "_wall_" + std::to_string(i) + "_basement.s";
		write_segment_box(state, basement_wall, footprint[i], footprint[(i + 1) % footprint.size()],
		    -spec.wall_thickness * 0.5, length + spec.wall_thickness * 0.5,
		    0.0, spec.wall_thickness, basement_bottom(spec), spec.origin.z);
		(void)mk_addmember(basement_wall.c_str(), &wall_members.l, nullptr, WMOP_UNION);
	    }
	}
	add_roof_closures(state, spec, footprint, roof, wall_z1, prefix, roof_solid, selected, wall_members);
	(void)mk_addmember(roof_solid.c_str(), &wall_members.l, nullptr, WMOP_SUBTRACT);
	subtract_members(wall_members, part_cutters);
	const std::vector<opening_spec> openings = automatic_openings(spec, footprint);
	for (size_t i = 0; i < openings.size(); ++i) {
	    const opening_spec &opening = openings[i];
	    if (!selected.empty() && selected.count(opening.wall) == 0)
		continue;
	    struct wmember opening_members;
	    BU_LIST_INIT(&opening_members.l);
	    const std::string opening_prefix = prefix + "_opening_" + std::to_string(i) + "_" + sanitize_name(opening.id);
	    add_opening_geometry(state, spec, footprint, opening, opening_prefix, wall_members, opening_members);
	    if (BU_LIST_IS_EMPTY(&opening_members.l))
		continue;
	    subtract_members(opening_members, part_cutters);
	    const std::string opening_region = opening_prefix + ".r";
	    const std::string material = opening.material.empty() ? (opening.kind == "window" ? "glass" : "wood") : opening.material;
	    write_region(state, opening_region, opening_members, material, opening.color);
	    set_attribute(state, opening_region, "building::opening_kind", opening.kind);
	    set_attribute(state, opening_region, "building::opening_type", opening.type);
	    set_attribute(state, opening_region, "building::opening_level", std::to_string(opening.level));
	    set_attribute(state, opening_region, "building::opening_wall", std::to_string(opening.wall));
	    for (const auto &tag : opening.osm_tags)
		set_attribute(state, opening_region, "osm::" + tag.first, tag.second);
	    (void)mk_addmember(opening_region.c_str(), &building_members.l, nullptr, WMOP_UNION);
	}
	const std::string wall_region = prefix + "_walls.r";
	write_region(state, wall_region, wall_members, spec.wall_material, spec.wall_color);
	(void)mk_addmember(wall_region.c_str(), &building_members.l, nullptr, WMOP_UNION);
    }

    struct wmember slab_members;
    BU_LIST_INIT(&slab_members.l);
    const std::vector<point2> slab_footprint = spec.walls ? offset_polygon(footprint, -spec.wall_thickness) : footprint;
    const double foundation_top = basement_bottom(spec);
    const std::string foundation = prefix + "_foundation.s";
    write_prism(state, foundation, footprint, foundation_top - spec.foundation_thickness, foundation_top);
    (void)mk_addmember(foundation.c_str(), &slab_members.l, nullptr, WMOP_UNION);
    std::vector<double> slab_elevations;
    for (int level = -spec.levels_below; level < spec.levels_above; ++level) {
	if (level >= 0 && level < spec.min_level)
	    continue;
	slab_elevations.push_back(level_z(spec, level));
    }
    if (slab_elevations.empty() && spec.levels_above == 0)
	slab_elevations.push_back(spec.origin.z);
    for (size_t i = 0; i < slab_elevations.size(); ++i) {
	const std::string slab = prefix + "_slab_" + std::to_string(i) + ".s";
	write_prism(state, slab, slab_footprint, slab_elevations[i] - spec.slab_thickness, slab_elevations[i]);
	(void)mk_addmember(slab.c_str(), &slab_members.l, nullptr, WMOP_UNION);
    }
    subtract_members(slab_members, part_cutters);
    const std::string slab_region = prefix + "_slabs.r";
    write_region(state, slab_region, slab_members, "reinforced_concrete", {145, 145, 140});
    (void)mk_addmember(slab_region.c_str(), &building_members.l, nullptr, WMOP_UNION);

    struct wmember roof_members;
    BU_LIST_INIT(&roof_members.l);
    (void)mk_addmember(roof_solid.c_str(), &roof_members.l, nullptr, WMOP_UNION);
    subtract_members(roof_members, part_cutters);
    const std::string roof_region = prefix + "_roof.r";
    write_region(state, roof_region, roof_members, spec.roof.material, spec.roof.color);
    set_attribute(state, roof_region, "building::roof_shape", spec.roof.shape);
    set_attribute(state, roof_region, "building::roof_realization", roof.realization);
    (void)mk_addmember(roof_region.c_str(), &building_members.l, nullptr, WMOP_UNION);

    write_structure(state, spec, footprint, roof, prefix, part_cutters, building_members);
    write_installations(state, spec, prefix, building_members);
    for (const building_spec &part : spec.parts) {
	const std::string part_group = write_building(state, part, true);
	(void)mk_addmember(part_group.c_str(), &building_members.l, nullptr, WMOP_UNION);
    }

    const std::string group = prefix;
    write_group(state, group, building_members);
    set_attribute(state, group, "building::generator", "mkbuilding");
    set_attribute(state, group, "building::schema_version", "1");
    set_attribute(state, group, "building::type", spec.type);
    set_attribute(state, group, "building::class", spec.building_class);
    set_attribute(state, group, "building::function", join(spec.function));
    set_attribute(state, group, "building::usage", join(spec.usage));
    set_attribute(state, group, "building::levels_above_ground", std::to_string(spec.levels_above));
    set_attribute(state, group, "building::levels_below_ground", std::to_string(spec.levels_below));
    set_attribute(state, group, "building::min_level", std::to_string(spec.min_level));
    set_attribute(state, group, "building::facade_material", spec.wall_material);
    set_attribute(state, group, "building::roof_shape", spec.roof.shape);
    set_attribute(state, group, "building::roof_material", spec.roof.material);
    set_attribute(state, group, "building::roof_direction_degrees", number_string(spec.roof.direction_degrees));
    set_attribute(state, group, "building::height_m", number_string(wall_z1 - spec.origin.z + effective_roof_height(spec)));
    set_attribute(state, group, "building::effective_spec", effective_spec_json({spec}));
    for (const auto &tag : spec.osm_tags)
	set_attribute(state, group, "osm::" + tag.first, tag.second);
    for (const auto &attribute : spec.cityjson_attributes)
	set_attribute(state, group, "cityjson::" + attribute.first, attribute.second);
    for (const auto &attribute : spec.metadata)
	set_attribute(state, group, "building::meta::" + attribute.first, attribute.second);
    return group;
}

struct rt_wdb *
open_database(const std::string &path, bool force, bool append)
{
    const bool exists = bu_file_exists(path.c_str(), nullptr);
    if (exists && !force && !append)
	throw std::runtime_error(path + " already exists; use --force to replace it or --append to add objects");
    if (exists && append) {
	struct db_i *dbip = db_open(path.c_str(), DB_OPEN_READWRITE);
	if (!dbip)
	    throw std::runtime_error("unable to open output database for append: " + path);
	if (db_dirbuild(dbip) < 0) {
	    db_close(dbip);
	    throw std::runtime_error("unable to read output database directory: " + path);
	}
	struct rt_wdb *fp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
	if (!fp) {
	    db_close(dbip);
	    throw std::runtime_error("unable to attach database writer: " + path);
	}
	return fp;
    }
    struct rt_wdb *fp = wdb_fopen(path.c_str());
    if (!fp)
	throw std::runtime_error("unable to create output database: " + path);
    mk_id(fp, "Procedural Building Generator");
    return fp;
}

} // namespace

generation_report
write_database(const std::vector<building_spec> &specs, const std::string &output_path, bool force, bool append)
{
    if (specs.empty())
	throw std::runtime_error("no building specifications were supplied");
    if (force && append)
	throw std::runtime_error("--force and --append are mutually exclusive");
    writer_state state;
    state.fp = open_database(output_path, force, append);
    try {
	struct wmember all_members;
	BU_LIST_INIT(&all_members.l);
	for (const building_spec &spec : specs) {
	    const std::string group = write_building(state, spec, false);
	    (void)mk_addmember(group.c_str(), &all_members.l, nullptr, WMOP_UNION);
	}
	if (specs.size() > 1 && db_lookup(state.fp->dbip, "all", LOOKUP_QUIET) == RT_DIR_NULL)
	    write_group(state, "all", all_members);
	db_close(state.fp->dbip);
    } catch (...) {
	db_close(state.fp->dbip);
	throw;
    }
    return state.report;
}

} // namespace building

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
