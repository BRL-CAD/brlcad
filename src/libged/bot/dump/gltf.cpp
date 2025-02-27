/*                   G L T F . C P P
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
/** @file gltf.cpp
 *
 */

#include "common.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "vmath.h"
#include "raytrace.h"
#include "../ged_bot.h"
#include "./ged_bot_dump.h"


struct bot_dump_gltf_internal {
    tinygltf::Model model;
    std::string filename;
};

static void
gltf_internal(struct _ged_bot_dump_client_data *d, const char *fname)
{
    d->gltf.i = new bot_dump_gltf_internal;
    d->gltf.i->filename = std::string(fname);

    tinygltf::Model &model = d->gltf.i->model;

    tinygltf::Asset& asset = model.asset;
    asset.version = "2.0";
    asset.generator = "BRL-CAD bot dump command";

    tinygltf::Scene scene;
    scene.name = "default";
    model.scenes.push_back(scene);
    model.defaultScene = model.scenes.size() - 1;
}

int
gltf_setup(struct _ged_bot_dump_client_data *d, const char *fname)
{
    // If we're writing out one file per bot, we need to keep
    // deleting and recreating the internal.  Otherwise, we
    // accumulate meshes in one structure.
    if (!d->gltf.i)
	gltf_internal(d, fname);

    return BRLCAD_OK;
}

int
gltf_finish(struct _ged_bot_dump_client_data *d)
{
    tinygltf::Model &model = d->gltf.i->model;
    std::string &filename = d->gltf.i->filename;

    tinygltf::TinyGLTF writer;

    bool result = writer.WriteGltfSceneToFile(&model, filename, false, true, true, d->binary);
    if (!result) {
	if (d->binary) {
	    bu_log("Error: unable to write BoT to glb file %s", filename.c_str());
	} else {
	    bu_log("Error: unable to write BoT to glTF file %s", filename.c_str());
	}
	return BRLCAD_ERROR;
    }

    delete d->gltf.i;
    d->gltf.i = NULL;

    return BRLCAD_OK;
}

void
gltf_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, char *name)
{

    tinygltf::Model &model = d->gltf.i->model;

    // Vertices
    tinygltf::Buffer verts;

    if (d->full_precision) {

	// tinygltf provides a TINYGLTF_COMPONENT_TYPE_DOUBLE type to write out
	// DOUBLE data, but that output produces a non-standard glb/glTF file.
	// BRL-CAD can read them back in, but other libraries such as
	// AssetImport can not.
	verts.data.resize(bot->num_vertices * sizeof(fastf_t) * 3);
	std::memcpy(verts.data.data(), bot->vertices, verts.data.size());

    } else {

	// Per https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html DOUBLE
	// is not a standard way to encode vertex info.  Step the vertex data
	// down to floats.
	verts.data.resize(bot->num_vertices * sizeof(float) * 3);
	float *fverts = (float *)bu_calloc(bot->num_vertices * 3, sizeof(float), "fverts");
	for (size_t i = 0; i < bot->num_vertices*3; i++)
	    fverts[i] = static_cast<float>(bot->vertices[i]);
	std::memcpy(verts.data.data(), fverts, verts.data.size());
	bu_free(fverts, "fverts");

    }
    model.buffers.push_back(verts);

    size_t vert_buffer_ind = model.buffers.size() - 1;

    tinygltf::BufferView vertsView;
    vertsView.buffer = vert_buffer_ind;
    vertsView.byteOffset = 0;
    vertsView.byteLength = verts.data.size();
    vertsView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    model.bufferViews.push_back(vertsView);

    size_t vert_view_accessor = model.bufferViews.size() - 1;

    tinygltf::Accessor vertAccessor;
    vertAccessor.bufferView = vert_view_accessor;
    vertAccessor.byteOffset = 0;
    if (d->full_precision) {
	// Non-standard tinygltf glTF type
	vertAccessor.componentType = (sizeof(fastf_t) == sizeof(double)) ? TINYGLTF_COMPONENT_TYPE_DOUBLE : TINYGLTF_COMPONENT_TYPE_FLOAT;
    } else {
	vertAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    }
    vertAccessor.count = bot->num_vertices;
    vertAccessor.type = TINYGLTF_TYPE_VEC3;
    model.accessors.push_back(vertAccessor);

    size_t vert_accessor = model.accessors.size() - 1;

    // Triangles
    tinygltf::Buffer tris;
    tris.data.resize(bot->num_faces * sizeof(unsigned int) * 3);
    // Per https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html it looks
    // like INT is a no-go data type here in general, even if tinygltf can
    // handle it.  Convert to unsigned int.  (TODO - we probably should do
    // that in the bot data structure anyway?   When do we need negative
    // numbers in the faces array?)
    unsigned int *ufaces = (unsigned int *)bu_calloc(bot->num_faces * 3, sizeof(unsigned int), "ufaces");
    for (size_t i = 0; i < bot->num_faces*3; i++) {
	if (bot->faces[i] < 0) {
	    // Shouldn't happen
	    ufaces[i] = 0;
	} else {
	    ufaces[i] = (unsigned int)bot->faces[i];
	}
    }
    std::memcpy(tris.data.data(), ufaces, tris.data.size());
    bu_free(ufaces, "ufaces");
    model.buffers.push_back(tris);

    size_t tris_buffer_ind = model.buffers.size() - 1;

    tinygltf::BufferView trisView;
    trisView.buffer = tris_buffer_ind;
    trisView.byteOffset = 0;
    trisView.byteLength = tris.data.size();
    trisView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    model.bufferViews.push_back(trisView);

    size_t tris_view_accessor = model.bufferViews.size() - 1;

    tinygltf::Accessor triAccessor;
    triAccessor.bufferView = tris_view_accessor;
    triAccessor.byteOffset = 0;
    triAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    triAccessor.count = bot->num_faces;
    triAccessor.type = TINYGLTF_TYPE_SCALAR;
    model.accessors.push_back(triAccessor);

    size_t tri_accessor = model.accessors.size() - 1;

    // Mesh
    tinygltf::Primitive primitive;
    primitive.attributes["POSITION"] = vert_accessor;
    primitive.indices = tri_accessor;
    primitive.mode = TINYGLTF_MODE_TRIANGLES;

    tinygltf::Mesh mesh;
    mesh.name = name;
    mesh.primitives.push_back(primitive);
    model.meshes.push_back(mesh);

    size_t mesh_ind = model.meshes.size() - 1;

    tinygltf::Node node;
    node.name = name;
    node.mesh = mesh_ind;
    model.nodes.push_back(node);

    size_t node_ind = model.nodes.size() - 1;

    model.scenes[model.defaultScene].nodes.push_back(node_ind);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
