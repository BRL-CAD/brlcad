/*                  V R M L 1 _ R E A D . C P P
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

#include "common.h"

#include "vrml1_read.h"

#include "vrml1_parser.h"

#include "bg/polygon.h"
#include "bg/trimesh.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "gcv/api.h"
#include "vmath.h"
#include "wdb.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr size_t MATRIX_DIMENSION = 4;
constexpr size_t PRIMITIVE_SEGMENTS = 32;
constexpr size_t PRIMITIVE_STACKS = 16;
constexpr double DEFAULT_DIFFUSE = 0.8;
constexpr double DEFAULT_SIZE = 2.0;
constexpr double DEFAULT_RADIUS = 1.0;
constexpr double MIN_AXIS_LENGTH = 1.0e-12;
constexpr double ORTHOGONAL_TOLERANCE = 1.0e-6;

using Matrix = std::array<double, MATRIX_DIMENSION * MATRIX_DIMENSION>;
using Point = std::array<double, 3>;
using Color = std::array<double, 3>;

struct Material {
    std::vector<Color> diffuse = {{DEFAULT_DIFFUSE, DEFAULT_DIFFUSE, DEFAULT_DIFFUSE}};
    std::vector<Color> emissive;
};

struct State {
    Matrix transform;
    std::vector<Point> coordinates;
    Material material;
    std::string material_binding = "OVERALL";
    std::string face_type = "CONVEX";
    int switch_choice = -1;
};

struct Mesh {
    std::vector<fastf_t> vertices;
    std::vector<int> faces;
};

Matrix
identity_matrix()
{
    Matrix result{};
    for (size_t i = 0; i < MATRIX_DIMENSION; ++i) result[i * MATRIX_DIMENSION + i] = 1.0;
    return result;
}

Matrix
multiply(const Matrix &left, const Matrix &right)
{
    Matrix result{};
    for (size_t row = 0; row < MATRIX_DIMENSION; ++row) {
	for (size_t column = 0; column < MATRIX_DIMENSION; ++column) {
	    for (size_t inner = 0; inner < MATRIX_DIMENSION; ++inner) {
		result[row * MATRIX_DIMENSION + column] +=
		    left[row * MATRIX_DIMENSION + inner] * right[inner * MATRIX_DIMENSION + column];
	    }
	}
    }
    return result;
}

Matrix
translation_matrix(const Point &translation)
{
    Matrix result = identity_matrix();
    result[3] = translation[0];
    result[7] = translation[1];
    result[11] = translation[2];
    return result;
}

Matrix
scale_matrix(const Point &scale)
{
    Matrix result = identity_matrix();
    result[0] = scale[0];
    result[5] = scale[1];
    result[10] = scale[2];
    return result;
}

Matrix
rotation_matrix(const std::array<double, 4> &rotation)
{
    Point axis = {rotation[0], rotation[1], rotation[2]};
    const double magnitude = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (magnitude < MIN_AXIS_LENGTH || std::abs(rotation[3]) < MIN_AXIS_LENGTH) return identity_matrix();
    for (double &component : axis) component /= magnitude;

    const double cosine = std::cos(rotation[3]);
    const double sine = std::sin(rotation[3]);
    const double one_minus_cosine = 1.0 - cosine;
    const double x = axis[0];
    const double y = axis[1];
    const double z = axis[2];

    Matrix result = identity_matrix();
    result[0] = cosine + x * x * one_minus_cosine;
    result[1] = x * y * one_minus_cosine - z * sine;
    result[2] = x * z * one_minus_cosine + y * sine;
    result[4] = y * x * one_minus_cosine + z * sine;
    result[5] = cosine + y * y * one_minus_cosine;
    result[6] = y * z * one_minus_cosine - x * sine;
    result[8] = z * x * one_minus_cosine - y * sine;
    result[9] = z * y * one_minus_cosine + x * sine;
    result[10] = cosine + z * z * one_minus_cosine;
    return result;
}

Point
transform_point(const Matrix &matrix, const Point &point, double scale_factor)
{
    return {
	scale_factor * (matrix[0] * point[0] + matrix[1] * point[1] + matrix[2] * point[2] + matrix[3]),
	scale_factor * (matrix[4] * point[0] + matrix[5] * point[1] + matrix[6] * point[2] + matrix[7]),
	scale_factor * (matrix[8] * point[0] + matrix[9] * point[1] + matrix[10] * point[2] + matrix[11])
    };
}

Point
transform_vector(const Matrix &matrix, const Point &vector, double scale_factor)
{
    return {
	scale_factor * (matrix[0] * vector[0] + matrix[1] * vector[1] + matrix[2] * vector[2]),
	scale_factor * (matrix[4] * vector[0] + matrix[5] * vector[1] + matrix[6] * vector[2]),
	scale_factor * (matrix[8] * vector[0] + matrix[9] * vector[1] + matrix[10] * vector[2])
    };
}

Point
point_field(const vrml1::Node &node, const char *name, const Point &fallback)
{
    const vrml1::Field *value = vrml1::field(node, name);
    if (!value || value->numbers.size() < 3) return fallback;
    return {value->numbers[0], value->numbers[1], value->numbers[2]};
}

std::array<double, 4>
rotation_field(const vrml1::Node &node, const char *name)
{
    const vrml1::Field *value = vrml1::field(node, name);
    if (!value || value->numbers.size() < 4) return {0.0, 0.0, 1.0, 0.0};
    return {value->numbers[0], value->numbers[1], value->numbers[2], value->numbers[3]};
}

double
number_field(const vrml1::Node &node, const char *name, double fallback)
{
    const vrml1::Field *value = vrml1::field(node, name);
    return value && !value->numbers.empty() ? value->numbers[0] : fallback;
}

long long
integer_field(const vrml1::Node &node, const char *name, long long fallback)
{
    const vrml1::Field *value = vrml1::field(node, name);
    return value && !value->integers.empty() ? value->integers[0] : fallback;
}

std::string
symbol_field(const vrml1::Node &node, const char *name, const char *fallback)
{
    const vrml1::Field *value = vrml1::field(node, name);
    return value && !value->symbol.empty() ? value->symbol : fallback;
}

Matrix
node_transform(const vrml1::Node &node)
{
    if (node.type == "Translation") {
	return translation_matrix(point_field(node, "translation", {0.0, 0.0, 0.0}));
    }
    if (node.type == "Rotation") return rotation_matrix(rotation_field(node, "rotation"));
    if (node.type == "Scale") return scale_matrix(point_field(node, "scaleFactor", {1.0, 1.0, 1.0}));
    if (node.type == "MatrixTransform") {
	const vrml1::Field *matrix = vrml1::field(node, "matrix");
	if (!matrix || matrix->numbers.size() != MATRIX_DIMENSION * MATRIX_DIMENSION) return identity_matrix();
	Matrix result{};
	/* SFMatrix is written for row-vector multiplication.  The importer uses
	 * column vectors, so the file matrix must be transposed. */
	for (size_t row = 0; row < MATRIX_DIMENSION; ++row) {
	    for (size_t column = 0; column < MATRIX_DIMENSION; ++column) {
		result[row * MATRIX_DIMENSION + column] =
		    matrix->numbers[column * MATRIX_DIMENSION + row];
	    }
	}
	return result;
    }
    if (node.type != "Transform") return identity_matrix();

    const Point translation = point_field(node, "translation", {0.0, 0.0, 0.0});
    const Point center = point_field(node, "center", {0.0, 0.0, 0.0});
    const Point scale = point_field(node, "scaleFactor", {1.0, 1.0, 1.0});
    std::array<double, 4> scale_orientation = rotation_field(node, "scaleOrientation");
    std::array<double, 4> inverse_scale_orientation = scale_orientation;
    inverse_scale_orientation[3] *= -1.0;
    const Point inverse_center = {-center[0], -center[1], -center[2]};

    Matrix result = translation_matrix(translation);
    result = multiply(result, translation_matrix(center));
    result = multiply(result, rotation_matrix(rotation_field(node, "rotation")));
    result = multiply(result, rotation_matrix(scale_orientation));
    result = multiply(result, scale_matrix(scale));
    result = multiply(result, rotation_matrix(inverse_scale_orientation));
    return multiply(result, translation_matrix(inverse_center));
}

Color
color_at(const Material &material, size_t index)
{
    const std::vector<Color> &colors = material.diffuse.empty() ? material.emissive : material.diffuse;
    if (colors.empty()) return {DEFAULT_DIFFUSE, DEFAULT_DIFFUSE, DEFAULT_DIFFUSE};
    return colors[std::min(index, colors.size() - 1)];
}

bool
perpendicular(const Point &first, const Point &second)
{
    const double first_magnitude = std::sqrt(first[0] * first[0] + first[1] * first[1] +
	first[2] * first[2]);
    const double second_magnitude = std::sqrt(second[0] * second[0] + second[1] * second[1] +
	second[2] * second[2]);
    if (first_magnitude < MIN_AXIS_LENGTH || second_magnitude < MIN_AXIS_LENGTH) return false;
    const double dot = first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
    return std::abs(dot / (first_magnitude * second_magnitude)) < ORTHOGONAL_TOLERANCE;
}

unsigned char
color_component(double value)
{
    constexpr double MAX_COLOR = 1.0;
    constexpr double CHANNEL_MAX = 255.0;
    return static_cast<unsigned char>(std::lround(std::clamp(value, 0.0, MAX_COLOR) * CHANNEL_MAX));
}

std::array<unsigned char, 3>
rgb_bytes(const Color &color)
{
    return {color_component(color[0]), color_component(color[1]), color_component(color[2])};
}

std::vector<Color>
color_values(const vrml1::Field *field_value)
{
    std::vector<Color> result;
    if (!field_value) return result;
    for (size_t i = 0; i + 2 < field_value->numbers.size(); i += 3)
	result.push_back({field_value->numbers[i], field_value->numbers[i + 1], field_value->numbers[i + 2]});
    return result;
}

class Importer {
public:
    Importer(struct rt_wdb *writer, double scale_factor, unsigned verbosity) :
	writer_(writer), scale_factor_(scale_factor), verbosity_(verbosity)
    {
	BU_LIST_INIT(&all_.l);
    }

    bool import(const std::vector<vrml1::NodePtr> &nodes)
    {
	State state;
	state.transform = identity_matrix();
	if (!traverse(nodes, state, std::string())) return false;
	if (!object_count_) {
	    bu_log("ERROR: VRML 1.0 file contains no convertible solid geometry\n");
	    return false;
	}
	if (mk_lcomb(writer_, "all", &all_, 0, nullptr, nullptr, nullptr, 0) != 0) {
	    bu_log("ERROR: failed to create VRML 1.0 top-level combination\n");
	    return false;
	}
	if (verbosity_) bu_log("%zu VRML 1.0 objects created\n", object_count_);
	return true;
    }

private:
    bool traverse(const std::vector<vrml1::NodePtr> &nodes, State &state, const std::string &name_hint)
    {
	for (const vrml1::NodePtr &node : nodes) {
	    if (!node) continue;
	    const std::string hint = node->def_name.empty() ? name_hint : node->def_name;

	    if (node->type == "Separator") {
		State child_state = state;
		if (!traverse(node->children, child_state, hint)) return false;
	    } else if (node->type == "TransformSeparator") {
		const Matrix saved_transform = state.transform;
		if (!traverse(node->children, state, hint)) return false;
		state.transform = saved_transform;
	    } else if (node->type == "Group" || node->type == "WWWAnchor") {
		if (!traverse(node->children, state, hint)) return false;
	    } else if (node->type == "LOD") {
		if (!node->children.empty()) {
		    std::vector<vrml1::NodePtr> selected = {node->children.front()};
		    if (!traverse(selected, state, hint)) return false;
		}
	    } else if (node->type == "Switch") {
		const long long requested = integer_field(*node, "whichChild", -1);
		const long long choice = requested == -2 ? state.switch_choice : requested;
		if (requested != -2) state.switch_choice = static_cast<int>(requested);
		if (choice == -3) {
		    if (!traverse(node->children, state, hint)) return false;
		} else if (choice >= 0 && static_cast<size_t>(choice) < node->children.size()) {
		    std::vector<vrml1::NodePtr> selected = {node->children[static_cast<size_t>(choice)]};
		    if (!traverse(selected, state, hint)) return false;
		}
	    } else if (is_transform(node->type)) {
		state.transform = multiply(state.transform, node_transform(*node));
	    } else if (node->type == "Coordinate3") {
		set_coordinates(*node, state);
	    } else if (node->type == "Material") {
		set_material(*node, state);
	    } else if (node->type == "MaterialBinding") {
		state.material_binding = symbol_field(*node, "value", "OVERALL");
	    } else if (node->type == "ShapeHints") {
		state.face_type = symbol_field(*node, "faceType", "CONVEX");
	    } else if (node->type == "Cube") {
		if (!write_cube(*node, state, hint)) return false;
	    } else if (node->type == "Sphere") {
		if (!write_sphere(*node, state, hint)) return false;
	    } else if (node->type == "Cylinder") {
		if (!write_cylinder(*node, state, hint)) return false;
	    } else if (node->type == "Cone") {
		if (!write_cone(*node, state, hint)) return false;
	    } else if (node->type == "IndexedFaceSet") {
		if (!write_indexed_face_set(*node, state, hint)) return false;
	    } else if (node->type == "IndexedLineSet" || node->type == "PointSet" ||
		    node->type == "AsciiText") {
		warn_once(node->type + " is rendering geometry without a BRL-CAD solid equivalent; skipping it");
	    } else if (node->type == "WWWInline") {
		warn_once("WWWInline external content is not fetched during conversion");
	    }
	}
	return true;
    }

    static bool is_transform(const std::string &type)
    {
	return type == "Translation" || type == "Rotation" || type == "Scale" ||
	    type == "Transform" || type == "MatrixTransform";
    }

    void set_coordinates(const vrml1::Node &node, State &state)
    {
	state.coordinates.clear();
	const vrml1::Field *points = vrml1::field(node, "point");
	if (!points) return;
	for (size_t i = 0; i + 2 < points->numbers.size(); i += 3)
	    state.coordinates.push_back({points->numbers[i], points->numbers[i + 1], points->numbers[i + 2]});
    }

    static void set_material(const vrml1::Node &node, State &state)
    {
	Material material;
	if (const vrml1::Field *diffuse = vrml1::field(node, "diffuseColor"))
	    material.diffuse = color_values(diffuse);
	if (const vrml1::Field *emissive = vrml1::field(node, "emissiveColor"))
	    material.emissive = color_values(emissive);
	state.material = std::move(material);
    }

    bool write_cube(const vrml1::Node &node, const State &state, const std::string &hint)
    {
	const double half_width = number_field(node, "width", DEFAULT_SIZE) / 2.0;
	const double half_height = number_field(node, "height", DEFAULT_SIZE) / 2.0;
	const double half_depth = number_field(node, "depth", DEFAULT_SIZE) / 2.0;
	const std::array<Point, 8> local = {{
	    {half_width, -half_height, -half_depth}, {half_width, half_height, -half_depth},
	    {half_width, half_height, half_depth}, {half_width, -half_height, half_depth},
	    {-half_width, -half_height, -half_depth}, {-half_width, half_height, -half_depth},
	    {-half_width, half_height, half_depth}, {-half_width, -half_height, half_depth}
	}};
	std::array<fastf_t, 24> points{};
	for (size_t i = 0; i < local.size(); ++i) {
	    const Point transformed = transform_point(state.transform, local[i], scale_factor_);
	    std::copy(transformed.begin(), transformed.end(), points.begin() + i * 3);
	}
	return write_primitive(state, hint, [&](const std::string &name) {
	    return mk_arb8(writer_, name.c_str(), points.data());
	});
    }

    bool write_sphere(const vrml1::Node &node, const State &state, const std::string &hint)
    {
	const double radius = number_field(node, "radius", DEFAULT_RADIUS);
	const Point center = transform_point(state.transform, {0.0, 0.0, 0.0}, scale_factor_);
	const Point a = transform_vector(state.transform, {radius, 0.0, 0.0}, scale_factor_);
	const Point b = transform_vector(state.transform, {0.0, radius, 0.0}, scale_factor_);
	const Point c = transform_vector(state.transform, {0.0, 0.0, radius}, scale_factor_);
	if (!perpendicular(a, b) || !perpendicular(a, c) || !perpendicular(b, c))
	    return write_sphere_mesh(state, hint, radius);
	return write_primitive(state, hint, [&](const std::string &name) {
	    return mk_ell(writer_, name.c_str(), center.data(), a.data(), b.data(), c.data());
	});
    }

    bool write_cylinder(const vrml1::Node &node, const State &state, const std::string &hint)
    {
	const double radius = number_field(node, "radius", DEFAULT_RADIUS);
	const double height = number_field(node, "height", DEFAULT_SIZE);
	const std::string parts = symbol_field(node, "parts", "ALL");
	if (parts.find("ALL") != std::string::npos) {
	    const Point base = transform_point(state.transform, {0.0, -height / 2.0, 0.0}, scale_factor_);
	    const Point top = transform_point(state.transform, {0.0, height / 2.0, 0.0}, scale_factor_);
	    const Point h = {top[0] - base[0], top[1] - base[1], top[2] - base[2]};
	    const Point a = transform_vector(state.transform, {radius, 0.0, 0.0}, scale_factor_);
	    const Point b = transform_vector(state.transform, {0.0, 0.0, radius}, scale_factor_);
	    if (!perpendicular(a, b) || !perpendicular(a, h) || !perpendicular(b, h))
		return write_partial_revolution(state, hint, radius, radius, height,
		    "SIDES|BOTTOM|TOP", true);
	    return write_primitive(state, hint, [&](const std::string &name) {
		return mk_tgc(writer_, name.c_str(), base.data(), h.data(), a.data(), b.data(), a.data(), b.data());
	    });
	}
	return write_partial_revolution(state, hint, radius, radius, height, parts, true);
    }

    bool write_cone(const vrml1::Node &node, const State &state, const std::string &hint)
    {
	const double radius = number_field(node, "bottomRadius", DEFAULT_RADIUS);
	const double height = number_field(node, "height", DEFAULT_SIZE);
	const std::string parts = symbol_field(node, "parts", "ALL");
	if (parts.find("ALL") != std::string::npos) {
	    const Point base = transform_point(state.transform, {0.0, -height / 2.0, 0.0}, scale_factor_);
	    const Point top = transform_point(state.transform, {0.0, height / 2.0, 0.0}, scale_factor_);
	    const Point h = {top[0] - base[0], top[1] - base[1], top[2] - base[2]};
	    const Point a = transform_vector(state.transform, {radius, 0.0, 0.0}, scale_factor_);
	    const Point b = transform_vector(state.transform, {0.0, 0.0, radius}, scale_factor_);
	    if (!perpendicular(a, b) || !perpendicular(a, h) || !perpendicular(b, h))
		return write_partial_revolution(state, hint, radius, 0.0, height,
		    "SIDES|BOTTOM", false);
	    const Point zero = {0.0, 0.0, 0.0};
	    return write_primitive(state, hint, [&](const std::string &name) {
		return mk_tgc(writer_, name.c_str(), base.data(), h.data(), a.data(), b.data(), zero.data(), zero.data());
	    });
	}
	return write_partial_revolution(state, hint, radius, 0.0, height, parts, false);
    }

    bool write_sphere_mesh(const State &state, const std::string &hint, double radius)
    {
	Mesh mesh;
	auto append_vertex = [&](const Point &local) {
	    const Point transformed = transform_point(state.transform, local, scale_factor_);
	    mesh.vertices.insert(mesh.vertices.end(), transformed.begin(), transformed.end());
	};
	append_vertex({0.0, -radius, 0.0});
	for (size_t stack = 1; stack < PRIMITIVE_STACKS; ++stack) {
	    const double latitude = -M_PI / 2.0 +
		M_PI * static_cast<double>(stack) / static_cast<double>(PRIMITIVE_STACKS);
	    const double ring_radius = radius * std::cos(latitude);
	    const double y = radius * std::sin(latitude);
	    for (size_t segment = 0; segment < PRIMITIVE_SEGMENTS; ++segment) {
		const double longitude = 2.0 * M_PI * static_cast<double>(segment) /
		    static_cast<double>(PRIMITIVE_SEGMENTS);
		append_vertex({ring_radius * std::cos(longitude), y,
		    ring_radius * std::sin(longitude)});
	    }
	}
	const int north = static_cast<int>(mesh.vertices.size() / 3);
	append_vertex({0.0, radius, 0.0});

	for (size_t segment = 0; segment < PRIMITIVE_SEGMENTS; ++segment) {
	    const int next = static_cast<int>((segment + 1) % PRIMITIVE_SEGMENTS);
	    mesh.faces.insert(mesh.faces.end(), {0, 1 + next, 1 + static_cast<int>(segment)});
	}
	for (size_t stack = 0; stack + 2 < PRIMITIVE_STACKS; ++stack) {
	    const int lower = 1 + static_cast<int>(stack * PRIMITIVE_SEGMENTS);
	    const int upper = lower + static_cast<int>(PRIMITIVE_SEGMENTS);
	    for (size_t segment = 0; segment < PRIMITIVE_SEGMENTS; ++segment) {
		const int next = static_cast<int>((segment + 1) % PRIMITIVE_SEGMENTS);
		mesh.faces.insert(mesh.faces.end(), {lower + static_cast<int>(segment), lower + next,
		    upper + static_cast<int>(segment)});
		mesh.faces.insert(mesh.faces.end(), {lower + next, upper + next,
		    upper + static_cast<int>(segment)});
	    }
	}
	const int final_ring = 1 + static_cast<int>((PRIMITIVE_STACKS - 2) * PRIMITIVE_SEGMENTS);
	for (size_t segment = 0; segment < PRIMITIVE_SEGMENTS; ++segment) {
	    const int next = static_cast<int>((segment + 1) % PRIMITIVE_SEGMENTS);
	    mesh.faces.insert(mesh.faces.end(), {final_ring + static_cast<int>(segment),
		final_ring + next, north});
	}
	return write_mesh(mesh, state, hint, 0);
    }

    bool write_partial_revolution(const State &state, const std::string &hint, double bottom_radius,
	    double top_radius, double height, const std::string &parts, bool has_top)
    {
	Mesh mesh;
	auto append_vertex = [&](const Point &local) {
	    const Point transformed = transform_point(state.transform, local, scale_factor_);
	    mesh.vertices.insert(mesh.vertices.end(), transformed.begin(), transformed.end());
	};
	for (size_t i = 0; i < PRIMITIVE_SEGMENTS; ++i) {
	    const double angle = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(PRIMITIVE_SEGMENTS);
	    append_vertex({bottom_radius * std::cos(angle), -height / 2.0, bottom_radius * std::sin(angle)});
	}
	const size_t top_start = mesh.vertices.size() / 3;
	if (top_radius > 0.0) {
	    for (size_t i = 0; i < PRIMITIVE_SEGMENTS; ++i) {
		const double angle = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(PRIMITIVE_SEGMENTS);
		append_vertex({top_radius * std::cos(angle), height / 2.0, top_radius * std::sin(angle)});
	    }
	} else {
	    append_vertex({0.0, height / 2.0, 0.0});
	}

	const bool sides = parts.find("SIDES") != std::string::npos;
	const bool bottom = parts.find("BOTTOM") != std::string::npos;
	const bool top = has_top && parts.find("TOP") != std::string::npos;
	if (sides) {
	    for (size_t i = 0; i < PRIMITIVE_SEGMENTS; ++i) {
		const int next = static_cast<int>((i + 1) % PRIMITIVE_SEGMENTS);
		if (top_radius > 0.0) {
		    mesh.faces.insert(mesh.faces.end(), {static_cast<int>(i), next,
			static_cast<int>(top_start + i)});
		    mesh.faces.insert(mesh.faces.end(), {next, static_cast<int>(top_start + next),
			static_cast<int>(top_start + i)});
		} else {
		    mesh.faces.insert(mesh.faces.end(), {static_cast<int>(i), next,
			static_cast<int>(top_start)});
		}
	    }
	}
	if (bottom) {
	    for (size_t i = 1; i + 1 < PRIMITIVE_SEGMENTS; ++i)
		mesh.faces.insert(mesh.faces.end(), {0, static_cast<int>(i + 1), static_cast<int>(i)});
	}
	if (top) {
	    for (size_t i = 1; i + 1 < PRIMITIVE_SEGMENTS; ++i)
		mesh.faces.insert(mesh.faces.end(), {static_cast<int>(top_start),
		    static_cast<int>(top_start + i), static_cast<int>(top_start + i + 1)});
	}
	if (mesh.faces.empty()) {
	    warn_once("primitive parts field selected no convertible surfaces");
	    return true;
	}
	return write_mesh(mesh, state, hint, 0);
    }

    bool write_indexed_face_set(const vrml1::Node &node, const State &state, const std::string &hint)
    {
	const vrml1::Field *indices = vrml1::field(node, "coordIndex");
	if (!indices || indices->integers.empty() || state.coordinates.empty()) {
	    warn_once("IndexedFaceSet has no coordinates or coordinate indices; skipping it");
	    return true;
	}

	std::vector<std::vector<int>> polygons;
	std::vector<int> polygon;
	for (long long index : indices->integers) {
	    if (index == -1) {
		if (!polygon.empty()) polygons.push_back(polygon);
		polygon.clear();
	    } else if (index < 0 || static_cast<size_t>(index) >= state.coordinates.size()) {
		bu_log("WARNING: VRML 1.0 IndexedFaceSet coordinate index %lld is out of range\n", index);
		return true;
	    } else {
		polygon.push_back(static_cast<int>(index));
	    }
	}
	if (!polygon.empty()) polygons.push_back(polygon);

	std::map<size_t, std::vector<int>> faces_by_material;
	for (size_t face_index = 0; face_index < polygons.size(); ++face_index) {
	    std::vector<int> triangles;
	    if (!triangulate(polygons[face_index], state.coordinates, state.face_type, triangles)) {
		bu_log("WARNING: unable to triangulate VRML 1.0 face %zu; skipping it\n", face_index);
		continue;
	    }
	    const size_t material_index = face_material(node, state, face_index, polygons[face_index]);
	    faces_by_material[material_index].insert(faces_by_material[material_index].end(),
		triangles.begin(), triangles.end());
	}

	for (const auto &entry : faces_by_material) {
	    Mesh mesh = compact_mesh(state.coordinates, state.transform, entry.second);
	    if (!write_mesh(mesh, state, hint, entry.first)) return false;
	}
	return true;
    }

    static bool triangulate(std::vector<int> polygon, const std::vector<Point> &coordinates,
	    const std::string &face_type, std::vector<int> &triangles)
    {
	while (polygon.size() > 1 && polygon.front() == polygon.back()) polygon.pop_back();
	if (polygon.size() < 3) return false;
	if (polygon.size() == 3) {
	    if (polygon[0] == polygon[1] || polygon[0] == polygon[2] || polygon[1] == polygon[2]) return false;
	    triangles = polygon;
	    return true;
	}
	if (face_type == "CONVEX") {
	    for (size_t i = 1; i + 1 < polygon.size(); ++i)
		triangles.insert(triangles.end(), {polygon[0], polygon[i], polygon[i + 1]});
	    return true;
	}

	Point normal = {0.0, 0.0, 0.0};
	for (size_t i = 0; i < polygon.size(); ++i) {
	    const Point &current = coordinates[static_cast<size_t>(polygon[i])];
	    const Point &next = coordinates[static_cast<size_t>(polygon[(i + 1) % polygon.size()])];
	    normal[0] += (current[1] - next[1]) * (current[2] + next[2]);
	    normal[1] += (current[2] - next[2]) * (current[0] + next[0]);
	    normal[2] += (current[0] - next[0]) * (current[1] + next[1]);
	}
	const size_t drop_axis = static_cast<size_t>(std::max_element(normal.begin(), normal.end(),
	    [](double left, double right) { return std::abs(left) < std::abs(right); }) - normal.begin());
	if (std::abs(normal[drop_axis]) < MIN_AXIS_LENGTH) return false;

	std::vector<fastf_t> projected;
	projected.reserve(polygon.size() * 2);
	for (int index : polygon) {
	    const Point &point = coordinates[static_cast<size_t>(index)];
	    projected.push_back(point[(drop_axis + 1) % 3]);
	    projected.push_back(point[(drop_axis + 2) % 3]);
	}
	double signed_area = 0.0;
	for (size_t i = 0; i < polygon.size(); ++i) {
	    const size_t next = (i + 1) % polygon.size();
	    signed_area += projected[i * 2] * projected[next * 2 + 1] -
		projected[next * 2] * projected[i * 2 + 1];
	}
	if (signed_area < 0.0) {
	    std::reverse(polygon.begin(), polygon.end());
	    projected.clear();
	    for (int index : polygon) {
		const Point &point = coordinates[static_cast<size_t>(index)];
		projected.push_back(point[(drop_axis + 1) % 3]);
		projected.push_back(point[(drop_axis + 2) % 3]);
	    }
	}

	int *local_faces = nullptr;
	int face_count = 0;
	const int result = bg_poly_triangulate(&local_faces, &face_count, nullptr, nullptr, nullptr, 0,
	    reinterpret_cast<const point2d_t *>(projected.data()), polygon.size(), TRI_EAR_CLIPPING);
	if (result || !local_faces || face_count <= 0) {
	    if (local_faces) bu_free(local_faces, "VRML 1.0 triangulation faces");
	    return false;
	}
	for (int i = 0; i < face_count * 3; ++i)
	    triangles.push_back(polygon[static_cast<size_t>(local_faces[i])]);
	bu_free(local_faces, "VRML 1.0 triangulation faces");
	return true;
    }

    size_t face_material(const vrml1::Node &node, const State &state, size_t face_index,
	    const std::vector<int> &polygon) const
    {
	const std::string &binding = state.material_binding;
	if (binding == "OVERALL" || binding == "DEFAULT") return 0;
	if (binding == "PER_FACE" || binding == "PER_PART") return face_index;

	const vrml1::Field *material_indices = vrml1::field(node, "materialIndex");
	std::vector<long long> explicit_indices;
	if (material_indices) {
	    for (long long index : material_indices->integers) {
		if (index >= 0) explicit_indices.push_back(index);
	    }
	}
	if (binding == "PER_FACE_INDEXED" || binding == "PER_PART_INDEXED") {
	    return explicit_indices.empty() ? face_index :
		static_cast<size_t>(explicit_indices[std::min(face_index, explicit_indices.size() - 1)]);
	}
	if (binding == "PER_VERTEX_INDEXED" && material_indices) {
	    size_t indexed_face = 0;
	    for (long long index : material_indices->integers) {
		if (index == -1) {
		    ++indexed_face;
		} else if (index >= 0 && indexed_face == face_index) {
		    return static_cast<size_t>(index);
		}
	    }
	}
	if ((binding == "PER_VERTEX" || binding == "PER_VERTEX_INDEXED") && !polygon.empty())
	    return static_cast<size_t>(std::max(0, polygon.front()));
	return 0;
    }

    Mesh compact_mesh(const std::vector<Point> &coordinates, const Matrix &transform,
	    const std::vector<int> &faces) const
    {
	Mesh mesh;
	std::map<int, int> remap;
	for (int source_index : faces) {
	    auto found = remap.find(source_index);
	    if (found == remap.end()) {
		const int target_index = static_cast<int>(remap.size());
		remap[source_index] = target_index;
		const Point point = transform_point(transform, coordinates[static_cast<size_t>(source_index)],
		    scale_factor_);
		mesh.vertices.insert(mesh.vertices.end(), point.begin(), point.end());
		mesh.faces.push_back(target_index);
	    } else {
		mesh.faces.push_back(found->second);
	    }
	}
	return mesh;
    }

    bool write_mesh(const Mesh &mesh, const State &state, const std::string &hint, size_t material_index)
    {
	if (mesh.faces.empty()) return true;
	const int vertex_count = static_cast<int>(mesh.vertices.size() / 3);
	const int face_count = static_cast<int>(mesh.faces.size() / 3);
	std::vector<fastf_t> vertices = mesh.vertices;
	std::vector<int> faces = mesh.faces;
	const int mode = bg_trimesh_solid(vertex_count, face_count, vertices.data(), faces.data(), nullptr) ?
	    RT_BOT_SURFACE : RT_BOT_SOLID;
	return write_primitive(state, hint, [&](const std::string &name) {
	    return mk_bot(writer_, name.c_str(), mode, RT_BOT_UNORIENTED, 0, vertex_count, face_count,
		vertices.data(), faces.data(), nullptr, nullptr);
	}, material_index);
    }

    template <typename Writer>
    bool write_primitive(const State &state, const std::string &hint, Writer write,
	    size_t material_index = 0)
    {
	const std::string base = unique_base(hint);
	const std::string solid_name = base + ".s";
	if (write(solid_name) != 0) {
	    bu_log("ERROR: failed to write VRML 1.0 solid %s\n", solid_name.c_str());
	    return false;
	}

	struct wmember members;
	BU_LIST_INIT(&members.l);
	if (!mk_addmember(solid_name.c_str(), &members.l, nullptr, WMOP_UNION)) {
	    bu_log("ERROR: failed to add VRML 1.0 solid %s to its region\n", solid_name.c_str());
	    return false;
	}
	const std::string region_name = base + ".r";
	const std::array<unsigned char, 3> rgb = rgb_bytes(color_at(state.material, material_index));
	if (mk_lrcomb(writer_, region_name.c_str(), &members, 1, nullptr, nullptr,
		const_cast<unsigned char *>(rgb.data()), next_id_++, 0, 0, 100, 0) != 0) {
	    bu_log("ERROR: failed to write VRML 1.0 region %s\n", region_name.c_str());
	    return false;
	}
	if (!mk_addmember(region_name.c_str(), &all_.l, nullptr, WMOP_UNION)) {
	    bu_log("ERROR: failed to add VRML 1.0 region %s to the top-level group\n", region_name.c_str());
	    return false;
	}
	++object_count_;
	return true;
    }

    std::string unique_base(const std::string &hint)
    {
	std::string sanitized;
	for (char character : hint) {
	    const unsigned char value = static_cast<unsigned char>(character);
	    sanitized.push_back(std::isalnum(value) || character == '_' || character == '-' ?
		character : '_');
	}
	if (sanitized.empty()) sanitized = "object";
	return "vrml1_" + sanitized + "_" + std::to_string(++name_counter_);
    }

    void warn_once(const std::string &message)
    {
	if (warnings_.insert(message).second) bu_log("WARNING: VRML 1.0: %s\n", message.c_str());
    }

    struct rt_wdb *writer_;
    double scale_factor_;
    unsigned verbosity_;
    struct wmember all_;
    int next_id_ = 1000;
    size_t name_counter_ = 0;
    size_t object_count_ = 0;
    std::set<std::string> warnings_;
};

} // namespace

int
vrml1_read(struct gcv_context *context, const struct gcv_opts *gcv_options, const char *source_path)
{
    std::ifstream input(source_path, std::ios::binary);
    if (!input) {
	bu_log("ERROR: cannot open VRML 1.0 input file %s\n", source_path);
	return 0;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
	bu_log("ERROR: failed while reading VRML 1.0 input file %s\n", source_path);
	return 0;
    }

    vrml1::Parser parser;
    std::vector<vrml1::NodePtr> nodes;
    std::string error;
    if (!parser.parse(contents.str(), nodes, error)) {
	bu_log("ERROR: cannot parse VRML 1.0 input file %s: %s\n", source_path, error.c_str());
	return 0;
    }

    struct rt_wdb *writer = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!writer) {
	bu_log("ERROR: cannot open the BRL-CAD output database for VRML 1.0 import\n");
	return 0;
    }
    if (mk_id_units(writer, "Conversion from VRML 1.0 format", "mm") != 0) {
	bu_log("ERROR: cannot initialize the BRL-CAD output database for VRML 1.0 import\n");
	return 0;
    }

    Importer importer(writer, gcv_options->scale_factor, gcv_options->verbosity_level);
    return importer.import(nodes) ? 1 : 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
