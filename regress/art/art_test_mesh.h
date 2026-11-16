/*              A R T _ T E S T _ M E S H . H
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
#ifndef ART_TEST_MESH_H
#define ART_TEST_MESH_H

#include "common.h"
#include "wdb.h"

namespace art_test {
enum class MeshUV { None, Authored, MirroredU, MirroredV };
enum class MeshNormals { Facet, Authored };

inline int
make_mesh(struct rt_wdb *db, const char *name, MeshUV mapping, MeshNormals shading = MeshNormals::Facet)
{
    fastf_t vertices[] = {
	-10, -10, -10, 10, -10, -10, 10, 10, -10, -10, 10, -10,
	-10, -10, 10, 10, -10, 10, 10, 10, 10, -10, 10, 10
    };
    int faces[] = {
	0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
	0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
	2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7
    };
    fastf_t uvs[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    // Reflect the chart without changing geometry or shading normals.
    for (size_t corner = 0; corner < 3; ++corner) {
	if (mapping == MeshUV::MirroredU)
	    uvs[3 * corner] = 1 - uvs[3 * corner];
	if (mapping == MeshUV::MirroredV)
	    uvs[3 * corner + 1] = 1 - uvs[3 * corner + 1];
    }
    int face_uvs[sizeof(faces) / sizeof(faces[0])];
    for (size_t corner = 0; corner < sizeof(face_uvs) / sizeof(face_uvs[0]); ++corner)
	face_uvs[corner] = static_cast<int>(corner % 3);
    fastf_t normals[sizeof(vertices) / sizeof(vertices[0])];
    for (size_t vertex = 0; vertex < sizeof(vertices) / (3 * sizeof(vertices[0])); ++vertex) {
	VMOVE(&normals[vertex * 3], &vertices[vertex * 3]);
	VUNITIZE(&normals[vertex * 3]);
    }
    const bool textured = mapping != MeshUV::None;
    const bool smooth = shading == MeshNormals::Authored;
    const unsigned char flags = (textured ? RT_BOT_HAS_TEXTURE_UVS : 0) |
	(smooth ? RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS : 0);
    return mk_bot_w_normals_and_uvs(db, name, RT_BOT_SOLID, RT_BOT_CCW, flags,
	sizeof(vertices) / (3 * sizeof(vertices[0])), sizeof(faces) / (3 * sizeof(faces[0])),
	vertices, faces, nullptr, nullptr,
	smooth ? sizeof(normals) / (3 * sizeof(normals[0])) : 0,
	smooth ? normals : nullptr, smooth ? faces : nullptr,
	textured ? 3 : 0, textured ? uvs : nullptr, textured ? face_uvs : nullptr);
}
} // namespace art_test
#endif
