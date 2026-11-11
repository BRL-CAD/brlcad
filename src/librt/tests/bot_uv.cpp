/*              B O T _ U V . C P P
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
#include <vector>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "rt/geom.h"

namespace {
constexpr size_t TriangleCount = 12;
constexpr size_t RayCount = TriangleCount * 2 * 3; // crosses a full 64-ray packet and its tail
constexpr double UVTolerance = 1.0e-9;
constexpr double BeamRadius = 0.125;
constexpr double EdgeX = 2.0;
constexpr double EdgeY = 3.0;
constexpr double TriangleSpacing = 4.0;
const size_t Positions[TriangleCount] = {7, 2, 10, 0, 11, 5, 1, 9, 3, 8, 4, 6};

struct Mesh {
    std::vector<fastf_t> vertices, uvs;
    std::vector<int> faces, face_uvs;
    struct rt_bot_internal bot = {};

    Mesh(unsigned char orientation, bool textured)
    {
	// The first face is degenerate; prepared triangle indices then differ
	// from original face indices, independently of BVH spatial ordering.
	const size_t face_count = TriangleCount + 1;
	vertices.resize(face_count * 9);
	uvs.resize(face_count * 9);
	faces.resize(face_count * 3);
	face_uvs.resize(face_count * 3);
	for (size_t face = 0; face < face_count; ++face) {
	    const double x = face ? TriangleSpacing * Positions[face - 1] : 0.0;
	    for (size_t corner = 0; corner < 3; ++corner) {
		const size_t vertex = face * 3 + corner;
		vertices[vertex * 3] = x + (corner == 1 ? EdgeX : 0.0);
		vertices[vertex * 3 + 1] = corner == 2 ? EdgeY : 0.0;
		// Texture indices deliberately differ from geometry indices.
		const size_t uv_index = face_count * 3 - vertex - 1;
		uvs[uv_index * 3] = face + (corner == 1 ? 1.0 : 0.0);
		uvs[uv_index * 3 + 1] = corner == 2 ? 1.0 : 0.0;
		const size_t source_corner = orientation == RT_BOT_CW && corner ? 3 - corner : corner;
		faces[face * 3 + source_corner] = static_cast<int>(vertex);
		face_uvs[face * 3 + source_corner] = static_cast<int>(uv_index);
	    }
	}
	faces[0] = faces[1] = faces[2] = 0;
	bot.magic = RT_BOT_INTERNAL_MAGIC;
	bot.mode = RT_BOT_SURFACE;
	bot.orientation = orientation;
	bot.bot_flags = textured ? RT_BOT_HAS_TEXTURE_UVS : 0;
	bot.num_faces = face_count;
	bot.num_vertices = vertices.size() / 3;
	bot.vertices = vertices.data();
	bot.faces = faces.data();
	bot.num_uvs = uvs.size() / 3;
	bot.uvs = uvs.data();
	bot.num_face_uvs = face_count;
	bot.face_uvs = face_uvs.data();
    }
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
};

class PreparedBot {
  public:
    struct soltab solid = {};
    int status;
    PreparedBot(Mesh &mesh, struct rt_i *rtip)
    {
	solid.l.magic = RT_SOLTAB_MAGIC;
	solid.l2.magic = RT_SOLTAB2_MAGIC;
	solid.st_meth = &OBJ[ID_BOT];
	solid.st_id = ID_BOT;
	solid.st_rtip = rtip;
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_BOT;
	internal.idb_meth = &OBJ[ID_BOT];
	internal.idb_ptr = &mesh.bot;
	status = OBJ[ID_BOT].ft_prep(&solid, &internal, rtip);
    }
    ~PreparedBot() { if (solid.st_specific) OBJ[ID_BOT].ft_free(&solid); }
    PreparedBot(const PreparedBot &) = delete;
    PreparedBot &operator=(const PreparedBot &) = delete;
};

bool
check_uv(struct application &application, struct soltab *solid, struct xray &ray,
    struct hit &hit, size_t face, bool textured, const char *path)
{
    OBJ[ID_BOT].ft_norm(&hit, solid, &ray);
    struct uvcoord uv = {NAN, NAN, NAN, NAN};
    const int status = rt_obj_uv(&application, solid, &hit, &uv);
    if (status != 0) {
	bu_log("%s face %zu UV callback failed (%d)\n", path, face, status);
	return false;
    }
    const double expected_u = textured ? face + 0.3 : 0.0;
    const double expected_v = textured ? 0.5 : 0.0;
    const double expected_du = textured ? BeamRadius / EdgeX : 0.0;
    const double expected_dv = textured ? BeamRadius / EdgeY : 0.0;
    const bool ok = std::isfinite(uv.uv_u) && std::isfinite(uv.uv_v) &&
	std::isfinite(uv.uv_du) && std::isfinite(uv.uv_dv) &&
	NEAR_EQUAL(uv.uv_u, expected_u, UVTolerance) &&
	NEAR_EQUAL(uv.uv_v, expected_v, UVTolerance) &&
	NEAR_EQUAL(uv.uv_du, expected_du, UVTolerance) &&
	NEAR_EQUAL(uv.uv_dv, expected_dv, UVTolerance);
    if (!ok)
	bu_log("%s face %zu UV=(%g,%g) footprint=(%g,%g), expected=(%g,%g) footprint=(%g,%g)\n",
	    path, face, uv.uv_u, uv.uv_v, uv.uv_du, uv.uv_dv,
	    expected_u, expected_v, expected_du, expected_dv);
    return ok;
}

bool
check_mesh(struct rt_i *rtip, struct resource *resource, unsigned char orientation, bool textured)
{
    Mesh mesh(orientation, textured);
    PreparedBot prepared(mesh, rtip);
    if (prepared.status != 0)
	return false;
    // Prepared geometry must own its UVs after the database internal is freed.
    std::fill(mesh.uvs.begin(), mesh.uvs.end(), NAN);
    struct application application;
    RT_APPLICATION_INIT(&application);
    application.a_rt_i = rtip;
    application.a_resource = resource;
    application.a_rbeam = BeamRadius;
    std::array<struct xray, RayCount> rays = {};
    std::array<struct soltab *, RayCount> solids;
    std::array<struct xray *, RayCount> ray_ptrs;
    std::array<struct seg, RayCount> segments = {};
    bool ok = true;
    for (size_t i = 0; i < RayCount; ++i) {
	const size_t face = i % TriangleCount + 1;
	const double side = (i / TriangleCount) % 2 ? -1.0 : 1.0;
	rays[i].magic = RT_RAY_MAGIC;
	VSET(rays[i].r_pt, TriangleSpacing * Positions[face - 1] + 0.3 * EdgeX, 0.5 * EdgeY, side);
	VSET(rays[i].r_dir, 0, 0, -side);
	solids[i] = &prepared.solid;
	ray_ptrs[i] = &rays[i];
	struct seg head;
	BU_LIST_INIT(&head.l);
	const int hits = OBJ[ID_BOT].ft_shot(&prepared.solid, &rays[i], &application, &head);
	if (hits <= 0 || BU_LIST_IS_EMPTY(&head.l)) {
	    bu_log("Scalar BOT UV fixture missed face %zu\n", face);
	    ok = false;
	} else {
	    auto *segment = BU_LIST_FIRST(seg, &head.l);
	    ok = check_uv(application, &prepared.solid, rays[i], segment->seg_in, face, textured, "scalar") && ok;
	}
	struct seg *segment;
	while (BU_LIST_WHILE(segment, seg, &head.l)) {
	    BU_LIST_DEQUEUE(&segment->l);
	    RT_FREE_SEG(segment, resource);
	}
    }
    OBJ[ID_BOT].ft_vshot(solids.data(), ray_ptrs.data(), segments.data(), RayCount, &application);
    for (size_t i = 0; i < RayCount; ++i) {
	const size_t face = i % TriangleCount + 1;
	if (!segments[i].seg_stp) {
	    bu_log("Packet BOT UV fixture missed face %zu\n", face);
	    ok = false;
	} else {
	    ok = check_uv(application, &prepared.solid, rays[i], segments[i].seg_in, face, textured, "packet") && ok;
	}
    }
    return ok;
}

bool
check_transform(bool in_place)
{
    Mesh source(RT_BOT_CCW, true), destination(RT_BOT_CCW, true);
    const auto original_vertices = source.vertices;
    const auto original_uvs = source.uvs;
    if (!in_place) std::fill(destination.uvs.begin(), destination.uvs.end(), -1.0);
    struct rt_db_internal input, output;
    RT_DB_INTERNAL_INIT(&input);
    RT_DB_INTERNAL_INIT(&output);
    input.idb_type = output.idb_type = ID_BOT;
    input.idb_meth = output.idb_meth = &OBJ[ID_BOT];
    input.idb_ptr = &source.bot;
    output.idb_ptr = &destination.bot;
    mat_t transform;
    MAT_IDN(transform);
    transform[0] = transform[5] = 0;
    transform[1] = -3; transform[4] = 2; transform[10] = 4;
    MAT_DELTAS(transform, 17, 23, -5);
    if (OBJ[ID_BOT].ft_mat(in_place ? &input : &output, transform, &input) != BRLCAD_OK)
	return false;
    const auto &result = in_place ? source : destination;
    for (size_t vertex = 0; vertex < source.bot.num_vertices; ++vertex) {
	point_t expected;
	MAT4X3PNT(expected, transform, &original_vertices[vertex * 3]);
	if (!VNEAR_EQUAL(expected, &result.vertices[vertex * 3], UVTolerance)) {
	    bu_log("BOT geometry was not transformed correctly\n");
	    return false;
	}
    }
    if (result.uvs != original_uvs) {
	bu_log("BOT UV coordinates changed under a geometry transform (in_place=%d)\n", in_place);
	return false;
    }
    return true;
}

bool
check_invalid_uvs(struct rt_i *rtip)
{
    bool ok = true;
    enum class InvalidUV { FaceCount, NegativeIndex, LargeIndex, Nonfinite, MissingUVs, MissingIndices };
    for (InvalidUV invalid : {InvalidUV::FaceCount, InvalidUV::NegativeIndex, InvalidUV::LargeIndex,
	    InvalidUV::Nonfinite, InvalidUV::MissingUVs, InvalidUV::MissingIndices}) {
	Mesh mesh(RT_BOT_CCW, true);
	switch (invalid) {
	    case InvalidUV::FaceCount: mesh.bot.num_face_uvs = 1; break;
	    case InvalidUV::NegativeIndex: mesh.face_uvs[3] = -1; break;
	    case InvalidUV::LargeIndex: mesh.face_uvs[3] = static_cast<int>(mesh.bot.num_uvs); break;
	    case InvalidUV::Nonfinite: mesh.uvs[0] = NAN; break;
	    case InvalidUV::MissingUVs: mesh.bot.uvs = nullptr; break;
	    case InvalidUV::MissingIndices: mesh.bot.face_uvs = nullptr; break;
	}
	PreparedBot prepared(mesh, rtip);
	if (prepared.status == 0) {
	    bu_log("Malformed BOT UV case %d was accepted\n", static_cast<int>(invalid));
	    ok = false;
	}
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
	for (bool textured : {false, true})
	    ok = check_mesh(rtip, &resource, orientation, textured) && ok;
    ok = check_invalid_uvs(rtip) && ok;
    ok = check_transform(false) && check_transform(true) && ok;
    rt_i_destroy(rtip);
    rt_clean_resource_basic(nullptr, &resource);
    return ok ? 0 : 1;
}
