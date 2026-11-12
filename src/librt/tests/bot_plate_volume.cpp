/*              B O T _ P L A T E _ V O L U M E . C P P
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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"
#include "rt/primitives/bot.h"

namespace {
constexpr double BoundTolerance = 1.0e-5;
constexpr double VolumeTolerance = 1.0e-5;

void
free_bot(struct rt_bot_internal *bot)
{
    rt_bot_internal_free(bot);
    bu_free(bot, "converted plate");
}

bool
check_triangle(double scale, const point_t translation)
{
    std::array<fastf_t, 9> vertices = {-10, -10, 0, 10, -10, 0, 10, 10, 0};
    std::array<int, 3> faces = {0, 1, 2};
    fastf_t thickness[] = {2 * scale};
    for (size_t vertex = 0; vertex < vertices.size() / ELEMENTS_PER_POINT; ++vertex)
	for (size_t axis = 0; axis < ELEMENTS_PER_POINT; ++axis)
	    vertices[vertex * ELEMENTS_PER_POINT + axis] =
		vertices[vertex * ELEMENTS_PER_POINT + axis] * scale + translation[axis];
    const auto original_vertices = vertices;
    const auto original_faces = faces;
    std::unique_ptr<struct bu_bitv, decltype(&bu_bitv_free)> face_mode(bu_bitv_new(1), bu_bitv_free);
    struct rt_bot_internal input = {};
    input.magic = RT_BOT_INTERNAL_MAGIC;
    input.mode = RT_BOT_PLATE;
    input.orientation = RT_BOT_UNORIENTED;
    input.num_vertices = vertices.size() / ELEMENTS_PER_POINT;
    input.num_faces = faces.size() / 3;
    input.vertices = vertices.data();
    input.faces = faces.data();
    input.thickness = thickness;
    input.face_mode = face_mode.get();
    struct rt_bot_internal *converted = nullptr;
    // Disable simplification to measure the extrusion itself.
    const int status = rt_bot_plate_to_vol(&converted, &input, 0, 1, 0, 0);
    std::unique_ptr<struct rt_bot_internal, decltype(&free_bot)> output(converted, free_bot);
    if (status != BRLCAD_OK || !output || output->mode != RT_BOT_SOLID) {
	bu_log("Plate conversion failed at scale %g\n", scale);
	return false;
    }
    if (std::memcmp(vertices.data(), original_vertices.data(), vertices.size() * sizeof(vertices[0])) ||
	std::memcmp(faces.data(), original_faces.data(), faces.size() * sizeof(faces[0])) ||
	!EQUAL(thickness[0], 2 * scale) || BU_BITTEST(face_mode.get(), 0)) {
	bu_log("Plate conversion modified its input\n");
	return false;
    }
    point_t low = {INFINITY, INFINITY, INFINITY};
    point_t high = {-INFINITY, -INFINITY, -INFINITY};
    std::vector<fastf_t> local_vertices(output->vertices,
	output->vertices + output->num_vertices * ELEMENTS_PER_POINT);
    for (size_t vertex = 0; vertex < output->num_vertices; ++vertex) {
	for (size_t axis = 0; axis < ELEMENTS_PER_POINT; ++axis) {
	    auto &coordinate = local_vertices[vertex * ELEMENTS_PER_POINT + axis];
	    coordinate -= translation[axis];
	    low[axis] = std::min(low[axis], coordinate);
	    high[axis] = std::max(high[axis], coordinate);
	}
    }
    bool passed = bg_trimesh_solid(output->num_vertices, output->num_faces,
	output->vertices, output->faces, nullptr) == 0;
    for (size_t axis = 0; axis < ELEMENTS_PER_POINT; ++axis) {
	const double half_extent = (axis == Z ? 1 : 10) * scale;
	passed = NEAR_EQUAL(low[axis], -half_extent, BoundTolerance * scale) &&
	    NEAR_EQUAL(high[axis], half_extent, BoundTolerance * scale) && passed;
    }
    // Base area is 200 square units and the authored thickness is two units.
    const double expected_volume = 400 * scale * scale * scale;
    const double volume = bg_trimesh_volume(output->faces, output->num_faces,
	reinterpret_cast<const point_t *>(local_vertices.data()), output->num_vertices);
    passed = NEAR_EQUAL(volume, expected_volume, VolumeTolerance * expected_volume) && passed;
    if (!passed)
	bu_log("Extruded plate scale=%g translation=(%g %g %g): bounds=(%g %g %g)-(%g %g %g), volume=%g expected=%g\n",
	    scale, V3ARGS(translation), V3ARGS(low), V3ARGS(high), volume, expected_volume);
    return passed;
}
} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    bool passed = true;
    const point_t origin = {0, 0, 0}, translated = {1.0e6, -2.0e6, 3.0e6};
    for (const double scale : {0.01, 1.0, 1000.0}) {
	passed = check_triangle(scale, origin) && passed;
	passed = check_triangle(scale, translated) && passed;
    }
    return passed ? 0 : 1;
}
