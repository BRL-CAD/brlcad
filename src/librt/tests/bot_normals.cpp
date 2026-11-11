/*              B O T _ N O R M A L S . C P P
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

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "bot_test.h"

namespace {
constexpr double NormalTolerance = 1.0e-9;
constexpr size_t RayCount = 65; // exercises a full packet and its tail

struct Mesh {
    std::array<fastf_t, 12> vertices = {0,0,0, 2,0,0, 0,3,0, 2,3,0};
    std::array<fastf_t, 12> normals = {0,0,1, 0.6,0,0.8, 0,0.8,0.6, 0,0,1};
    std::array<int, 3> faces = {0,1,2};
    std::array<int, 3> face_normals = {0,1,2};
    struct rt_bot_internal bot = {};

    explicit Mesh(unsigned char orientation = RT_BOT_CCW)
    {
	if (orientation == RT_BOT_CW) {
	    std::swap(faces[1], faces[2]);
	    std::swap(face_normals[1], face_normals[2]);
	}
	bot.magic = RT_BOT_INTERNAL_MAGIC;
	bot.mode = RT_BOT_SURFACE;
	bot.orientation = orientation;
	bot.bot_flags = RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS;
	bot.num_vertices = vertices.size() / ELEMENTS_PER_POINT;
	bot.vertices = vertices.data();
	bot.num_faces = 1;
	bot.faces = faces.data();
	bot.num_normals = normals.size() / ELEMENTS_PER_VECT;
	bot.normals = normals.data();
	bot.num_face_normals = 1;
	bot.face_normals = face_normals.data();
    }
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
};

bool
same_normal(const fastf_t *actual, const fastf_t *expected)
{
    return !VINVALID(actual) && VNEAR_EQUAL(actual, expected, NormalTolerance);
}

bool
check_interpolation(struct rt_i *rtip, struct resource *resource, unsigned char orientation)
{
    Mesh mesh(orientation);
    bot_test::PreparedBot prepared(mesh.bot, rtip);
    if (prepared.status != 0) return false;
    // Prepared normals must survive destruction of the database internal.
    std::fill(mesh.normals.begin(), mesh.normals.end(), NAN);
    struct application application;
    RT_APPLICATION_INIT(&application);
    application.a_rt_i = rtip;
    application.a_resource = resource;
    std::array<struct xray, RayCount> rays = {};
    std::array<struct xray *, RayCount> ray_ptrs;
    std::array<struct soltab *, RayCount> solids;
    std::array<struct seg, RayCount> segments = {};
    vect_t expected = {0.18, 0.4, 0.74}; // barycentric weights (0.2, 0.3, 0.5)
    VUNITIZE(expected);
    bool ok = true;
    for (size_t i = 0; i < RayCount; ++i) {
	rays[i].magic = RT_RAY_MAGIC;
	VSET(rays[i].r_pt, 0.6, 1.5, 2);
	VSET(rays[i].r_dir, 0, 0, -1);
	ray_ptrs[i] = &rays[i];
	solids[i] = &prepared.solid;
	struct seg head;
	BU_LIST_INIT(&head.l);
	if (rt_obj_shot(&prepared.solid, &rays[i], &application, &head) <= 0 || BU_LIST_IS_EMPTY(&head.l)) {
	    ok = false;
	} else {
	    struct hit hit = BU_LIST_FIRST(seg, &head.l)->seg_in;
	    ok = rt_obj_norm(&hit, &prepared.solid, &rays[i]) == 0 && same_normal(hit.hit_normal, expected) && ok;
	}
	struct seg *segment;
	while (BU_LIST_WHILE(segment, seg, &head.l)) {
	    BU_LIST_DEQUEUE(&segment->l);
	    RT_FREE_SEG(segment, resource);
	}
    }
    OBJ[ID_BOT].ft_vshot(solids.data(), ray_ptrs.data(), segments.data(), RayCount, &application);
    for (size_t i = 0; i < RayCount; ++i) {
	if (!segments[i].seg_stp) {
	    ok = false;
	} else {
	    auto hit = segments[i].seg_in;
	    ok = rt_obj_norm(&hit, &prepared.solid, &rays[i]) == 0 && same_normal(hit.hit_normal, expected) && ok;
	}
    }
    if (!ok) bu_log("BOT normal interpolation failed (orientation=%d)\n", orientation);
    return ok;
}

bool
check_transform(bool in_place, bool independent_count, bool shear)
{
    Mesh source, destination;
    if (independent_count)
	source.bot.num_normals = destination.bot.num_normals = 3;
    const auto original_normals = source.normals;
    struct rt_db_internal input, output;
    RT_DB_INTERNAL_INIT(&input);
    RT_DB_INTERNAL_INIT(&output);
    input.idb_type = output.idb_type = ID_BOT;
    input.idb_meth = output.idb_meth = &OBJ[ID_BOT];
    input.idb_ptr = &source.bot;
    output.idb_ptr = &destination.bot;
    mat_t matrix;
    MAT_IDN(matrix);
    // Rotate around Z after nonuniform scale; translation cannot affect normals.
    matrix[0] = matrix[5] = 0;
    matrix[1] = -3; matrix[4] = 2; matrix[10] = 4;
    if (shear) {
	matrix[0] = 2; matrix[1] = 1; matrix[4] = 0; matrix[5] = 3;
    }
    MAT_DELTAS(matrix, 17, 23, -5);
    bool ok = OBJ[ID_BOT].ft_mat(in_place ? &input : &output, matrix, &input) == BRLCAD_OK;
    const auto &result = in_place ? source : destination;
    for (size_t i = 0; i < source.bot.num_normals; ++i) {
	const fastf_t *normal = &original_normals[i * ELEMENTS_PER_VECT];
	vect_t expected = {-normal[Y] / 3, normal[X] / 2, normal[Z] / 4};
	if (shear)
	    VSET(expected, normal[X] / 2, normal[Y] / 3 - normal[X] / 6, normal[Z] / 4);
	VUNITIZE(expected);
	ok = same_normal(&result.normals[i * ELEMENTS_PER_VECT], expected) && ok;
    }
    if (!in_place) ok = source.normals == original_normals && ok;
    if (!ok) bu_log("BOT normal transform failed (in_place=%d, independent_count=%d, shear=%d)\n", in_place, independent_count, shear);
    return ok;
}
bool
check_rejected_transforms()
{
    enum class Invalid { Count, MissingInput, MissingOutput, Singular };
    bool ok = true;
    for (Invalid invalid : {Invalid::Count, Invalid::MissingInput, Invalid::MissingOutput, Invalid::Singular}) {
	Mesh source, destination;
	const auto vertices = destination.vertices;
	const auto normals = destination.normals;
	struct rt_db_internal input, output;
	RT_DB_INTERNAL_INIT(&input);
	RT_DB_INTERNAL_INIT(&output);
	input.idb_ptr = &source.bot;
	output.idb_ptr = &destination.bot;
	mat_t matrix;
	MAT_IDN(matrix);
	MAT_DELTAS(matrix, 17, 23, -5);
	switch (invalid) {
	    case Invalid::Count: destination.bot.num_normals--; break;
	    case Invalid::MissingInput: source.bot.normals = nullptr; break;
	    case Invalid::MissingOutput: destination.bot.normals = nullptr; break;
	    case Invalid::Singular: matrix[0] = 0; break;
	}
	const bool rejected = OBJ[ID_BOT].ft_mat(&output, matrix, &input) == BRLCAD_ERROR &&
	    destination.vertices == vertices && destination.normals == normals;
	if (!rejected) bu_log("BOT normal transform failed to reject invalid case %d atomically\n", static_cast<int>(invalid));
	ok = rejected && ok;
    }
    return ok;
}
} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1) return 1;
    struct rt_i *rtip = rt_dirbuild_inmem(nullptr, 0, nullptr, 0);
    if (!rtip) return 1;
    struct resource resource = RT_RESOURCE_INIT_ZERO;
    rt_init_resource(&resource, 0, rtip);
    bool ok = true;
    for (unsigned char orientation : {RT_BOT_CCW, RT_BOT_CW})
	ok = check_interpolation(rtip, &resource, orientation) && ok;
    for (bool in_place : {false, true})
	for (bool independent_count : {false, true})
	    for (bool shear : {false, true})
		ok = check_transform(in_place, independent_count, shear) && ok;
    ok = check_rejected_transforms() && ok;
    rt_i_destroy(rtip);
    rt_clean_resource_basic(nullptr, &resource);
    return ok ? 0 : 1;
}
