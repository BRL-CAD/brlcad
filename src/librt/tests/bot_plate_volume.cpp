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
#include "bot_test.h"

namespace {
constexpr double BoundTolerance = 1.0e-5;
constexpr double VolumeTolerance = 1.0e-5;
using Vertices = std::array<fastf_t, 12>;
using Faces = std::array<int, 6>;
enum class PlateLayout { Triangle, Quad, ReversedQuad };

void
free_bot(struct rt_bot_internal *bot)
{
    rt_bot_internal_free(bot);
    bu_free(bot, "converted plate");
}
using BotPtr = std::unique_ptr<struct rt_bot_internal, decltype(&free_bot)>;

BotPtr
make_volume(Vertices &vertices, Faces &faces, size_t face_count, double thickness)
{
    const auto original_vertices = vertices;
    const auto original_faces = faces;
    fastf_t thicknesses[] = {thickness, thickness};
    std::unique_ptr<struct bu_bitv, decltype(&bu_bitv_free)> face_mode(bu_bitv_new(face_count), bu_bitv_free);
    struct rt_bot_internal input = {};
    input.magic = RT_BOT_INTERNAL_MAGIC;
    input.mode = RT_BOT_PLATE;
    input.orientation = RT_BOT_UNORIENTED;
    input.num_vertices = vertices.size() / ELEMENTS_PER_POINT;
    input.num_faces = face_count;
    input.vertices = vertices.data();
    input.faces = faces.data();
    input.thickness = thicknesses;
    input.face_mode = face_mode.get();
    struct rt_bot_internal *converted = nullptr;
    // Disable simplification to measure the extrusion itself.
    const int status = rt_bot_plate_to_vol(&converted, &input, 0, 1, 0, 0);
    BotPtr output(converted, free_bot);
    if (status != BRLCAD_OK || !output || output->mode != RT_BOT_SOLID) {
	bu_log("Plate conversion failed\n");
	return BotPtr(nullptr, free_bot);
    }
    bool unchanged =
	!std::memcmp(vertices.data(), original_vertices.data(), vertices.size() * sizeof(vertices[0])) &&
	!std::memcmp(faces.data(), original_faces.data(), faces.size() * sizeof(faces[0]));
    for (size_t face = 0; face < face_count; ++face)
	unchanged = EQUAL(thicknesses[face], thickness) && !BU_BITTEST(face_mode.get(), face) && unchanged;
    if (!unchanged) {
	bu_log("Plate conversion modified its input\n");
	return BotPtr(nullptr, free_bot);
    }
    return output;
}

bool
check_plate(double scale, const point_t translation, PlateLayout layout)
{
    Vertices vertices = {-10, -10, 0, 10, -10, 0, 10, 10, 0, -10, 10, 0};
    Faces faces = {0, 1, 2, 0, 2, 3};
    const size_t face_count = layout == PlateLayout::Triangle ? 1 : 2;
    if (layout == PlateLayout::ReversedQuad)
	std::swap(faces[3], faces[5]);
    for (size_t vertex = 0; vertex < vertices.size() / ELEMENTS_PER_POINT; ++vertex)
	for (size_t axis = 0; axis < ELEMENTS_PER_POINT; ++axis)
	    vertices[vertex * ELEMENTS_PER_POINT + axis] =
		vertices[vertex * ELEMENTS_PER_POINT + axis] * scale + translation[axis];
    auto output = make_volume(vertices, faces, face_count, 2 * scale);
    if (!output)
	return false;
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
    // Each triangular face has area 200; the authored thickness is two units.
    const double expected_volume = face_count * 400 * scale * scale * scale;
    const double volume = bg_trimesh_volume(output->faces, output->num_faces,
	reinterpret_cast<const point_t *>(local_vertices.data()), output->num_vertices);
    passed = NEAR_EQUAL(volume, expected_volume, VolumeTolerance * expected_volume) && passed;
    if (!passed)
	bu_log("Extruded plate scale=%g translation=(%g %g %g) faces=%zu reversed=%d: bounds=(%g %g %g)-(%g %g %g), volume=%g expected=%g\n",
	    scale, V3ARGS(translation), face_count, layout == PlateLayout::ReversedQuad,
	    V3ARGS(low), V3ARGS(high), volume, expected_volume);
    return passed;
}

bool
check_bent_join()
{
    Vertices vertices = {0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 10};
    Faces faces = {0, 1, 2, 0, 2, 3};
    auto output = make_volume(vertices, faces, 2, 2);
    if (!output)
	return false;
    struct rt_i *rtip = rt_dirbuild_inmem(nullptr, 0, nullptr, 0);
    if (!rtip)
	return false;
    struct resource resource = RT_RESOURCE_INIT_ZERO;
    rt_init_resource(&resource, 0, rtip);
    bool passed = false;
    {
	bot_test::PreparedBot prepared(*output, rtip);
	if (prepared.status == 0) {
	    struct application application;
	    RT_APPLICATION_INIT(&application);
	    application.a_rt_i = rtip;
	    application.a_resource = &resource;
	    application.a_ray.magic = RT_RAY_MAGIC;
	    // This point lies in the rounded join, outside both face prisms.
	    VSET(application.a_ray.r_pt, -0.5, 5, -0.5);
	    VSET(application.a_ray.r_dir, 1, 0, 0);
	    bot_test::Shot shot(application, &prepared.solid, application.a_ray);
	    const struct seg *segment = shot.first();
	    passed = segment && segment->seg_in.hit_dist < 0 && segment->seg_out.hit_dist > 0;
	}
    }
    rt_i_destroy(rtip);
    rt_clean_resource_basic(nullptr, &resource);
    if (!passed)
	bu_log("Plate conversion lost the rounded join between noncoplanar faces\n");
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
    for (const auto layout : {PlateLayout::Triangle, PlateLayout::Quad, PlateLayout::ReversedQuad}) {
	for (const double scale : {0.01, 1.0, 1000.0}) {
	    passed = check_plate(scale, origin, layout) && passed;
	    passed = check_plate(scale, translated, layout) && passed;
	}
    }
    passed = check_bent_join() && passed;
    return passed ? 0 : 1;
}
