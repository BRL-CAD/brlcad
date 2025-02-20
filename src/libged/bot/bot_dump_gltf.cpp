/*               B O T _ D U M P _ G L T F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file bot_dump_gltf.cpp
 *
 */

#include "common.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "tiny_gltf.h"

#include "vmath.h"
#include "raytrace.h"
#include "ged_bot.h"

int
gltf_write_bot(struct rt_bot_internal *bot, const char *filename, const char *name, int binary)
{
    if (!bot || !filename)
	return BRLCAD_ERROR;

    tinygltf::Model model;
    tinygltf::Asset& asset = model.asset;
    asset.version = "2.0";
    asset.generator = "BRL-CAD bot dump command";

    tinygltf::Scene scene;
    scene.name = "gscene";
    model.scenes.push_back(scene);
    model.defaultScene = 0;

    tinygltf::Node node;
    node.name = "gnode";
    model.nodes.push_back(node);
    model.scenes[0].nodes.push_back(0);

    tinygltf::Mesh mesh;
    mesh.name = name;

    // Vertices
    tinygltf::Buffer verts;
    verts.data.resize(bot->num_vertices * sizeof(fastf_t) * 3);
    std::memcpy(verts.data.data(), bot->vertices, verts.data.size());
    model.buffers.push_back(verts);

    // Triangles
    tinygltf::Buffer tris;
    tris.data.resize(bot->num_faces * sizeof(int) * 3);
    std::memcpy(tris.data.data(), bot->faces, tris.data.size());
    model.buffers.push_back(tris);

    // Buffer views
    tinygltf::BufferView vertsView;
    vertsView.buffer = 0;
    vertsView.byteOffset = 0;
    vertsView.byteLength = verts.data.size();
    vertsView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    model.bufferViews.push_back(vertsView);

    tinygltf::BufferView trisView;
    trisView.buffer = 1;
    trisView.byteOffset = 0;
    trisView.byteLength = tris.data.size();
    trisView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    model.bufferViews.push_back(trisView);

    // Accessors
    tinygltf::Accessor vertAccessor;
    vertAccessor.bufferView = 0;
    vertAccessor.byteOffset = 0;
    // TODO - this should toggle on whether fastf_t is float or double...
    vertAccessor.componentType = TINYGLTF_COMPONENT_TYPE_DOUBLE;
    vertAccessor.count = 3;
    vertAccessor.type = TINYGLTF_TYPE_VEC3;
    model.accessors.push_back(vertAccessor);

    tinygltf::Accessor triAccessor;
    triAccessor.bufferView = 1;
    triAccessor.byteOffset = 0;
    triAccessor.componentType = TINYGLTF_COMPONENT_TYPE_INT;
    triAccessor.count = 3;
    triAccessor.type = TINYGLTF_TYPE_SCALAR;
    model.accessors.push_back(triAccessor);

    // Tell tinygltf where our mesh primitive is
    tinygltf::Primitive primitive;
    primitive.attributes["POSITION"] = 0;
    primitive.indices = 1;
    primitive.mode = TINYGLTF_MODE_TRIANGLES;
    mesh.primitives.push_back(primitive);

    model.meshes.push_back(mesh);
    model.nodes[0].mesh = 0;

    tinygltf::TinyGLTF writer;
    bool result = writer.WriteGltfSceneToFile(&model, std::string(filename), false, true, true, binary);
    if (!result) {
	bu_log("Error: unable to write BoT to glTF file %s", filename);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
