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
constexpr double DerivativeStep = 1.0e-5;
constexpr size_t RayCount = 65; // exercises a full packet and its tail

struct Mesh {
    std::array<fastf_t, 12> vertices = {0,0,0, 2,0,0, 0,3,0, 2,3,0};
    std::array<fastf_t, 12> normals = {0,0,1, 0.6,0,0.8, 0,0.8,0.6, 0,0,1};
    std::array<fastf_t, 9> uvs = {0,0,0, 1,0,0, 0,1,0};
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
	bot.bot_flags = RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS | RT_BOT_HAS_TEXTURE_UVS;
	bot.num_vertices = vertices.size() / ELEMENTS_PER_POINT;
	bot.vertices = vertices.data();
	bot.num_faces = 1;
	bot.faces = faces.data();
	bot.num_normals = normals.size() / ELEMENTS_PER_VECT;
	bot.normals = normals.data();
	bot.num_face_normals = 1;
	bot.face_normals = face_normals.data();
	bot.num_uvs = uvs.size() / ELEMENTS_PER_VECT;
	bot.num_face_uvs = bot.num_faces;
	bot.uvs = uvs.data();
	bot.face_uvs = faces.data();
    }
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
};

bool
same_vector(const fastf_t *actual, const fastf_t *expected)
{
    return !VINVALID(actual) && VNEAR_EQUAL(actual, expected, NormalTolerance);
}

std::array<fastf_t, 3>
normal_at(const std::array<fastf_t, 12> &normals, double u, double v)
{
    std::array<fastf_t, 3> result;
    VCOMB3(result.data(), 1.0 - u - v, &normals[0], u, &normals[3], v, &normals[6]);
    VUNITIZE(result.data());
    return result;
}

struct rt_surface_derivatives
expected_derivatives(const std::array<fastf_t, 12> &normals, const vect_t dpdu, const vect_t dpdv)
{
    struct rt_surface_derivatives result = RT_SURFACE_DERIVATIVES_INIT_ZERO;
    VMOVE(result.dpdu, dpdu); VMOVE(result.dpdv, dpdv);
    const auto left = normal_at(normals, 0.3 - DerivativeStep, 0.5);
    const auto right = normal_at(normals, 0.3 + DerivativeStep, 0.5);
    const auto down = normal_at(normals, 0.3, 0.5 - DerivativeStep);
    const auto up = normal_at(normals, 0.3, 0.5 + DerivativeStep);
    VSUB2SCALE(result.dndu, right.data(), left.data(), 0.5 / DerivativeStep);
    VSUB2SCALE(result.dndv, up.data(), down.data(), 0.5 / DerivativeStep);
    return result;
}

bool
check_hit_derivatives(struct application &application, struct soltab *solid,
    const struct hit &hit, const struct rt_surface_derivatives &expected)
{
    const struct hit original = hit;
    for (size_t repeat = 0; repeat < 2; ++repeat) {
	struct rt_surface_derivatives derivatives;
	if (rt_obj_surface_derivatives(&application, solid, &hit, &derivatives) != RT_DERIVATIVES_OK ||
	    !bot_test::same_derivatives(derivatives, expected, NormalTolerance) ||
	    !same_vector(hit.hit_normal, original.hit_normal) ||
	    !same_vector(hit.hit_vpriv, original.hit_vpriv))
	    return false;
    }
    return true;
}

bool
check_hit_normals(struct hit &hit, struct soltab *solid, struct xray *ray, const vect_t expected)
{
    const struct hit original = hit;
    const vect_t expected_geometric = {0, 0, 1};
    bool ok = true;
    for (size_t repeat = 0; repeat < 2; ++repeat) {
	vect_t geometric, shading;
	ok = rt_obj_normals(&hit, solid, ray, geometric, shading) == 0 &&
	    same_vector(geometric, expected_geometric) && same_vector(shading, expected) &&
	    same_vector(hit.hit_normal, original.hit_normal) &&
	    same_vector(hit.hit_vpriv, original.hit_vpriv) && ok;
    }
    return rt_obj_norm(&hit, solid, ray) == 0 && same_vector(hit.hit_normal, expected) && ok;
}

bool
check_interpolation(struct rt_i *rtip, struct resource *resource, unsigned char orientation)
{
    Mesh mesh(orientation);
    bot_test::PreparedBot prepared(mesh.bot, rtip);
    if (prepared.status != 0) return false;
    const vect_t dpdu = {2, 0, 0}, dpdv = {0, 3, 0};
    const auto derivatives = expected_derivatives(mesh.normals, dpdu, dpdv);
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
	bot_test::Shot shot(application, &prepared.solid, rays[i]);
	const struct seg *segment = shot.first();
	if (!segment) {
	    ok = false;
	} else {
	    struct hit hit = segment->seg_in;
	    ok = check_hit_derivatives(application, &prepared.solid, hit, derivatives) && ok;
	    ok = check_hit_normals(hit, &prepared.solid, &rays[i], expected) && ok;
	}

    }
    OBJ[ID_BOT].ft_vshot(solids.data(), ray_ptrs.data(), segments.data(), RayCount, &application);
    for (size_t i = 0; i < RayCount; ++i) {
	if (!segments[i].seg_stp) {
	    ok = false;
	} else {
	    auto hit = segments[i].seg_in;
	    ok = check_hit_derivatives(application, &prepared.solid, hit, derivatives) && ok;
	    ok = check_hit_normals(hit, &prepared.solid, &rays[i], expected) && ok;
	}
    }
    if (!ok) bu_log("BOT normal interpolation failed (orientation=%d)\n", orientation);
    return ok;
}

bool
check_transform(struct rt_i *rtip, struct resource *resource, bool in_place, bool independent_count, bool shear)
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
    auto &result = in_place ? source : destination;
    for (size_t i = 0; i < source.bot.num_normals; ++i) {
	const fastf_t *normal = &original_normals[i * ELEMENTS_PER_VECT];
	vect_t expected = {-normal[Y] / 3, normal[X] / 2, normal[Z] / 4};
	if (shear)
	    VSET(expected, normal[X] / 2, normal[Y] / 3 - normal[X] / 6, normal[Z] / 4);
	VUNITIZE(expected);
	ok = same_vector(&result.normals[i * ELEMENTS_PER_VECT], expected) && ok;
    }
    if (!in_place) ok = source.normals == original_normals && ok;
    bot_test::PreparedBot prepared(result.bot, rtip);
    if (prepared.status != 0) return false;
    struct application application;
    RT_APPLICATION_INIT(&application);
    application.a_rt_i = rtip;
    application.a_resource = resource;
    application.a_ray.magic = RT_RAY_MAGIC;
    const point_t target = {0.6, 1.5, 0};
    MAT4X3PNT(application.a_ray.r_pt, matrix, target);
    application.a_ray.r_pt[Z] += 2;
    VSET(application.a_ray.r_dir, 0, 0, -1);
    const vect_t du = {2, 0, 0}, dv = {0, 3, 0};
    vect_t dpdu, dpdv;
    MAT4X3VEC(dpdu, matrix, du);
    MAT4X3VEC(dpdv, matrix, dv);
    const auto derivatives = expected_derivatives(result.normals, dpdu, dpdv);
    bot_test::Shot shot(application, &prepared.solid, application.a_ray);
    const struct seg *segment = shot.first();
    ok = segment && check_hit_derivatives(application, &prepared.solid, segment->seg_in, derivatives) && ok;
    if (!ok) bu_log("BOT normal transform failed (in_place=%d, independent_count=%d, shear=%d)\n", in_place, independent_count, shear);
    return ok;
}

bool
check_view_independent_normals(struct rt_i *rtip, struct resource *resource)
{
    Mesh mesh;
    bot_test::PreparedBot prepared(mesh.bot, rtip);
    if (prepared.status != 0) return false;
    const point_t target = {0.6, 1.5, 0};
    const vect_t directions[] = {{0, 0, -1}, {0, 0, 1}, {1, 0, -0.01}};
    bool ok = true;
    for (const auto &direction : directions) {
	struct application application;
	RT_APPLICATION_INIT(&application);
	application.a_rt_i = rtip;
	application.a_resource = resource;
	application.a_ray.magic = RT_RAY_MAGIC;
	VMOVE(application.a_ray.r_dir, direction);
	VUNITIZE(application.a_ray.r_dir);
	VJOIN1(application.a_ray.r_pt, target, -2, application.a_ray.r_dir);
	bot_test::Shot shot(application, &prepared.solid, application.a_ray);
	const struct seg *segment = shot.first();
	bool passed = segment != nullptr;
	if (passed) {
	    const double side = direction[Z] < 0 ? 1.0 : -1.0;
	    vect_t expected = {0.18 * side, 0.4 * side, 0.74 * side};
	    const vect_t expected_geometric = {0, 0, side};
	    VUNITIZE(expected);
	    vect_t geometric, shading;
	    passed = rt_obj_normals(&segment->seg_in, &prepared.solid,
		&application.a_ray, geometric, shading) == 0 &&
		same_vector(geometric, expected_geometric) && same_vector(shading, expected);
	    const vect_t dpdu = {2, 0, 0}, dpdv = {0, 3, 0};
	    auto derivatives = expected_derivatives(mesh.normals, dpdu, dpdv);
	    VSCALE(derivatives.dndu, derivatives.dndu, side);
	    VSCALE(derivatives.dndv, derivatives.dndv, side);
	    passed = check_hit_derivatives(application, &prepared.solid,
		segment->seg_in, derivatives) && passed;
	}

	if (!passed) bu_log("BOT shading normal changed with view direction (%g %g %g)\n", V3ARGS(direction));
	ok = passed && ok;
    }
    return ok;
}

bool
check_invalid_normals(struct rt_i *rtip, struct resource *resource)
{
    bool ok = true;
    for (double invalid : {0.0, static_cast<double>(NAN)}) {
	Mesh mesh;
	std::fill(mesh.normals.begin(), mesh.normals.end(), invalid);
	bot_test::PreparedBot prepared(mesh.bot, rtip);
	if (prepared.status != 0) return false;
	struct application application;
	RT_APPLICATION_INIT(&application);
	application.a_rt_i = rtip;
	application.a_resource = resource;
	application.a_ray.magic = RT_RAY_MAGIC;
	VSET(application.a_ray.r_pt, 0.6, 1.5, 2);
	VSET(application.a_ray.r_dir, 0, 0, -1);
	bot_test::Shot shot(application, &prepared.solid, application.a_ray);
	const struct seg *segment = shot.first();
	bool rejected = false;
	if (segment) {
	    const vect_t sentinel = {2, 3, 4};
	    vect_t geometric, shading;
	    VMOVE(geometric, sentinel); VMOVE(shading, sentinel);
	    rejected = rt_obj_normals(&segment->seg_in, &prepared.solid,
		&application.a_ray, geometric, shading) < 0 &&
		same_vector(geometric, sentinel) && same_vector(shading, sentinel);
	    const auto marker = bot_test::derivative_marker();
	    struct rt_surface_derivatives derivatives = marker;
	    rejected = rt_obj_surface_derivatives(&application, &prepared.solid,
		&segment->seg_in, &derivatives) == RT_DERIVATIVES_ERROR &&
		bot_test::same_derivatives(derivatives, marker, NormalTolerance) && rejected;
	}

	ok = rejected && ok;
    }
    if (!ok) bu_log("Invalid BOT normals were not rejected with unchanged outputs\n");
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
		ok = check_transform(rtip, &resource, in_place, independent_count, shear) && ok;
    ok = check_rejected_transforms() && ok;
    ok = check_invalid_normals(rtip, &resource) && ok;
    ok = check_view_independent_normals(rtip, &resource) && ok;
    rt_i_destroy(rtip);
    rt_clean_resource_basic(nullptr, &resource);
    return ok ? 0 : 1;
}
