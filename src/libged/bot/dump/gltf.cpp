/*                   G L T F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <vector>

#include "tiny_gltf_v3.h"

#include "vmath.h"
#include "raytrace.h"
#include "../ged_bot.h"
#include "./ged_bot_dump.h"

constexpr size_t COORDINATES_PER_VERTEX = 3;
constexpr size_t INDICES_PER_TRIANGLE = 3;

struct position_bounds {
    std::array<double, COORDINATES_PER_VERTEX> min;
    std::array<double, COORDINATES_PER_VERTEX> max;
};

struct bot_dump_gltf_internal {
    std::string filename;
    std::deque<std::string> strings;
    std::vector<uint8_t> buffer_data;
    tg3_buffer buffer = {};
    std::vector<tg3_buffer_view> buffer_views;
    std::vector<tg3_accessor> accessors;
    std::deque<position_bounds> position_bounds_data;
    std::vector<tg3_str_int_pair> attributes;
    std::vector<tg3_primitive> primitives;
    std::vector<tg3_material> materials;
    std::vector<tg3_mesh> meshes;
    std::vector<tg3_node> nodes;
    std::vector<int32_t> scene_nodes;
    tg3_scene scene = {};
    tg3_model model = {};
    bool failed = false;
};


static tg3_str
store_string(struct bot_dump_gltf_internal *state, const std::string &value)
{
    if (value.size() > std::numeric_limits<uint32_t>::max()) {
	bu_log("Error: glTF output string exceeds the 32-bit size limit\n");
	state->failed = true;
	return {};
    }
    state->strings.push_back(value);
    const std::string &stored = state->strings.back();
    return {stored.c_str(), static_cast<uint32_t>(stored.size())};
}


static bool
append_buffer_data(struct bot_dump_gltf_internal *state, const void *data,
    size_t byte_count, size_t alignment, uint64_t *offset)
{
    const size_t max_buffer_bytes = std::numeric_limits<uint32_t>::max();
    if ((!data && byte_count > 0) || alignment == 0 ||
	state->buffer_data.size() > max_buffer_bytes) {
	bu_log("Error: invalid glTF output buffer data\n");
	state->failed = true;
	return false;
    }

    size_t remainder = state->buffer_data.size() % alignment;
    size_t padding = remainder ? alignment - remainder : 0;
    size_t available = max_buffer_bytes - state->buffer_data.size();
    if (padding > available || byte_count > available - padding) {
	bu_log("Error: glTF output buffer exceeds the GLB 32-bit size limit\n");
	state->failed = true;
	return false;
    }

    state->buffer_data.resize(state->buffer_data.size() + padding, 0);
    *offset = static_cast<uint64_t>(state->buffer_data.size());
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    if (byte_count > 0)
	state->buffer_data.insert(state->buffer_data.end(), bytes, bytes + byte_count);
    return true;
}


static void
initialize_material(tg3_material &material)
{
    material.alpha_cutoff = 0.5;
    material.pbr_metallic_roughness.base_color_factor[0] = 1.0;
    material.pbr_metallic_roughness.base_color_factor[1] = 1.0;
    material.pbr_metallic_roughness.base_color_factor[2] = 1.0;
    material.pbr_metallic_roughness.base_color_factor[3] = 1.0;
    material.pbr_metallic_roughness.metallic_factor = 1.0;
    material.pbr_metallic_roughness.roughness_factor = 1.0;
    material.pbr_metallic_roughness.base_color_texture.index = TG3_INDEX_NONE;
    material.pbr_metallic_roughness.metallic_roughness_texture.index = TG3_INDEX_NONE;
    material.normal_texture.index = TG3_INDEX_NONE;
    material.occlusion_texture.index = TG3_INDEX_NONE;
    material.emissive_texture.index = TG3_INDEX_NONE;
}


static void
initialize_node(tg3_node &node)
{
    node.camera = TG3_INDEX_NONE;
    node.skin = TG3_INDEX_NONE;
    node.mesh = TG3_INDEX_NONE;
    node.light = TG3_INDEX_NONE;
    node.emitter = TG3_INDEX_NONE;
    node.rotation[3] = 1.0;
    node.scale[0] = 1.0;
    node.scale[1] = 1.0;
    node.scale[2] = 1.0;
}


static void
prepare_model(struct bot_dump_gltf_internal *state)
{
    state->buffer.byte_length = state->buffer_data.size();
    state->buffer.data.data = state->buffer_data.empty() ? nullptr : state->buffer_data.data();
    state->buffer.data.count = state->buffer_data.size();

    for (size_t i = 0; i < state->primitives.size(); ++i) {
	state->primitives[i].attributes = &state->attributes[i];
	state->primitives[i].attributes_count = 1;
	state->meshes[i].primitives = &state->primitives[i];
	state->meshes[i].primitives_count = 1;
    }

    state->scene.nodes = state->scene_nodes.empty() ? nullptr : state->scene_nodes.data();
    state->scene.nodes_count = static_cast<uint32_t>(state->scene_nodes.size());

    state->model.buffers = state->buffer_data.empty() ? nullptr : &state->buffer;
    state->model.buffers_count = state->buffer_data.empty() ? 0 : 1;
    state->model.buffer_views = state->buffer_views.empty() ? nullptr : state->buffer_views.data();
    state->model.buffer_views_count = static_cast<uint32_t>(state->buffer_views.size());
    state->model.accessors = state->accessors.empty() ? nullptr : state->accessors.data();
    state->model.accessors_count = static_cast<uint32_t>(state->accessors.size());
    state->model.materials = state->materials.empty() ? nullptr : state->materials.data();
    state->model.materials_count = static_cast<uint32_t>(state->materials.size());
    state->model.meshes = state->meshes.empty() ? nullptr : state->meshes.data();
    state->model.meshes_count = static_cast<uint32_t>(state->meshes.size());
    state->model.nodes = state->nodes.empty() ? nullptr : state->nodes.data();
    state->model.nodes_count = static_cast<uint32_t>(state->nodes.size());
    state->model.scenes = &state->scene;
    state->model.scenes_count = 1;
}


static void
log_write_errors(const tg3_error_stack &errors)
{
    for (uint32_t i = 0; i < errors.count; ++i) {
	const tg3_error_entry &entry = errors.entries[i];
	bu_log("glTF write error (%d): %s\n", static_cast<int>(entry.code),
	    entry.message ? entry.message : "no details");
    }
}


static void
gltf_internal(struct _ged_bot_dump_client_data *d, const char *fname)
{
    d->gltf.i = new bot_dump_gltf_internal;
    struct bot_dump_gltf_internal *state = d->gltf.i;
    state->filename = fname;
    state->model.default_scene = 0;
    state->model.asset.version = store_string(state, "2.0");
    state->model.asset.generator = store_string(state, "BRL-CAD bot dump command");
    state->scene.name = store_string(state, "default");
}


int
gltf_validate_options(struct _ged_bot_dump_client_data *d)
{
    if (d && d->full_precision) {
	bu_vls_printf(d->gedp->ged_result_str,
	    "Error: -F/--full-precision is not supported for glTF or GLB output.\n"
	    "glTF 2.0 section 3.6.2.2, 'Accessor Data Types', defines FLOAT "
	    "(componentType 5126) as its only floating-point accessor type and "
	    "requires IEEE-754 single precision. Section 3.7.2.1, 'Meshes', "
	    "requires POSITION attributes to use VEC3/FLOAT32. The former "
	    "DOUBLE value (componentType 5130) is not part of glTF 2.0 and may "
	    "not be readable by conforming tools.\n"
	    "Omit -F to write a conforming glTF 2.0 file. BRL-CAD can still "
	    "import legacy files written with -F. See "
	    "https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
gltf_setup(struct _ged_bot_dump_client_data *d, const char *fname)
{
    if (gltf_validate_options(d) != BRLCAD_OK)
	return BRLCAD_ERROR;

    /* One-file-per-BoT output recreates this state after each gltf_finish. */
    if (!d->gltf.i)
	gltf_internal(d, fname);

    return BRLCAD_OK;
}


int
gltf_finish(struct _ged_bot_dump_client_data *d)
{
    if (!d->gltf.i)
	return BRLCAD_ERROR;

    struct bot_dump_gltf_internal *state = d->gltf.i;
    if (state->failed) {
	delete state;
	d->gltf.i = NULL;
	return BRLCAD_ERROR;
    }

    prepare_model(state);
    tg3_write_options options;
    tg3_write_options_init(&options);
    options.write_binary = d->binary ? 1 : 0;
    options.embed_buffers = d->binary ? 0 : 1;

    tg3_error_stack errors;
    tg3_error_stack_init(&errors);
    tg3_error_code result = TG3_ERR_WRITE_FAILED;
    if (state->filename.size() <= std::numeric_limits<uint32_t>::max()) {
	result = tg3_write_to_file(&state->model, &errors, state->filename.c_str(),
	    static_cast<uint32_t>(state->filename.size()), &options);
    }
    log_write_errors(errors);
    tg3_error_stack_free(&errors);

    if (result != TG3_OK) {
	bu_log("Error: unable to write BoT to %s file %s\n",
	    d->binary ? "glb" : "glTF", state->filename.c_str());
    }

    delete state;
    d->gltf.i = NULL;
    return result == TG3_OK ? BRLCAD_OK : BRLCAD_ERROR;
}


void
gltf_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot,
    char *name)
{
    struct bot_dump_gltf_internal *state = d->gltf.i;
    if (!state || !bot || !name)
	return;
    if (state->failed)
	return;
    if (bot->num_vertices == 0 || bot->num_faces == 0) {
	bu_log("Error: cannot write an empty BoT to glTF\n");
	state->failed = true;
	return;
    }
    if (bot->num_vertices > std::numeric_limits<size_t>::max() / COORDINATES_PER_VERTEX ||
	bot->num_faces > std::numeric_limits<size_t>::max() / INDICES_PER_TRIANGLE) {
	bu_log("Error: BoT is too large for glTF output\n");
	state->failed = true;
	return;
    }

    size_t vertex_value_count = bot->num_vertices * COORDINATES_PER_VERTEX;
    if (vertex_value_count > std::numeric_limits<size_t>::max() / sizeof(float)) {
	bu_log("Error: BoT vertex buffer is too large for glTF output\n");
	state->failed = true;
	return;
    }

    uint64_t vertex_offset;
    size_t vertex_byte_count = vertex_value_count * sizeof(float);
    std::vector<float> vertices(vertex_value_count);
    for (size_t i = 0; i < vertex_value_count; ++i) {
	vertices[i] = static_cast<float>(bot->vertices[i]);
	if (!std::isfinite(vertices[i])) {
	    bu_log("Error: BoT contains a vertex coordinate that cannot be "
		"represented as a finite glTF FLOAT value\n");
	    state->failed = true;
	    return;
	}
    }
    if (!append_buffer_data(state, vertices.data(), vertex_byte_count,
	    sizeof(float), &vertex_offset))
	return;

    tg3_buffer_view vertex_view = {};
    vertex_view.buffer = 0;
    vertex_view.byte_offset = vertex_offset;
    vertex_view.byte_length = vertex_byte_count;
    vertex_view.target = TG3_TARGET_ARRAY_BUFFER;
    state->buffer_views.push_back(vertex_view);

    position_bounds bounds;
    for (size_t axis = 0; axis < COORDINATES_PER_VERTEX; ++axis) {
	bounds.min[axis] = vertices[axis];
	bounds.max[axis] = vertices[axis];
    }
    for (size_t vertex = 1; vertex < bot->num_vertices; ++vertex) {
	for (size_t axis = 0; axis < COORDINATES_PER_VERTEX; ++axis) {
	    double coordinate = vertices[vertex * COORDINATES_PER_VERTEX + axis];
	    bounds.min[axis] = std::min(bounds.min[axis], coordinate);
	    bounds.max[axis] = std::max(bounds.max[axis], coordinate);
	}
    }
    state->position_bounds_data.push_back(bounds);

    tg3_accessor vertex_accessor = {};
    vertex_accessor.buffer_view = static_cast<int32_t>(state->buffer_views.size() - 1);
    vertex_accessor.component_type = TG3_COMPONENT_TYPE_FLOAT;
    vertex_accessor.count = bot->num_vertices;
    vertex_accessor.type = TG3_TYPE_VEC3;
    vertex_accessor.min_values = state->position_bounds_data.back().min.data();
    vertex_accessor.min_values_count = COORDINATES_PER_VERTEX;
    vertex_accessor.max_values = state->position_bounds_data.back().max.data();
    vertex_accessor.max_values_count = COORDINATES_PER_VERTEX;
    state->accessors.push_back(vertex_accessor);
    int32_t vertex_accessor_index = static_cast<int32_t>(state->accessors.size() - 1);

    size_t face_index_count = bot->num_faces * INDICES_PER_TRIANGLE;
    if (face_index_count > std::numeric_limits<size_t>::max() / sizeof(uint32_t)) {
	bu_log("Error: BoT index buffer is too large for glTF output\n");
	state->failed = true;
	return;
    }

    std::vector<uint32_t> face_indices(face_index_count);
    for (size_t i = 0; i < face_index_count; ++i) {
	if (bot->faces[i] < 0 || static_cast<size_t>(bot->faces[i]) >= bot->num_vertices) {
	    bu_log("Error: BoT contains an invalid vertex index\n");
	    state->failed = true;
	    return;
	}
	face_indices[i] = static_cast<uint32_t>(bot->faces[i]);
    }

    uint64_t face_offset;
    size_t face_byte_count = face_index_count * sizeof(uint32_t);
    if (!append_buffer_data(state, face_indices.data(), face_byte_count,
	    sizeof(uint32_t), &face_offset))
	return;

    tg3_buffer_view face_view = {};
    face_view.buffer = 0;
    face_view.byte_offset = face_offset;
    face_view.byte_length = face_byte_count;
    face_view.target = TG3_TARGET_ELEMENT_ARRAY_BUFFER;
    state->buffer_views.push_back(face_view);

    tg3_accessor face_accessor = {};
    face_accessor.buffer_view = static_cast<int32_t>(state->buffer_views.size() - 1);
    face_accessor.component_type = TG3_COMPONENT_TYPE_UNSIGNED_INT;
    face_accessor.count = face_index_count;
    face_accessor.type = TG3_TYPE_SCALAR;
    state->accessors.push_back(face_accessor);
    int32_t face_accessor_index = static_cast<int32_t>(state->accessors.size() - 1);

    tg3_str_int_pair position_attribute = {};
    position_attribute.key = {"POSITION", 8};
    position_attribute.value = vertex_accessor_index;
    state->attributes.push_back(position_attribute);

    tg3_primitive primitive = {};
    primitive.indices = face_accessor_index;
    primitive.material = TG3_INDEX_NONE;
    primitive.mode = TG3_MODE_TRIANGLES;
    if (d->curr_obj_color_valid) {
	tg3_material material = {};
	initialize_material(material);
	material.name = store_string(state, std::string(name) + "_material");
	material.pbr_metallic_roughness.base_color_factor[0] = d->curr_obj_red / 255.0;
	material.pbr_metallic_roughness.base_color_factor[1] = d->curr_obj_green / 255.0;
	material.pbr_metallic_roughness.base_color_factor[2] = d->curr_obj_blue / 255.0;
	material.pbr_metallic_roughness.base_color_factor[3] = d->curr_obj_alpha;
	material.pbr_metallic_roughness.metallic_factor = 0.0;
	if (d->curr_obj_alpha < 1.0)
	    material.alpha_mode = store_string(state, "BLEND");
	state->materials.push_back(material);
	primitive.material = static_cast<int32_t>(state->materials.size() - 1);
    }
    state->primitives.push_back(primitive);

    tg3_mesh mesh = {};
    mesh.name = store_string(state, name);
    state->meshes.push_back(mesh);
    int32_t mesh_index = static_cast<int32_t>(state->meshes.size() - 1);

    tg3_node node = {};
    initialize_node(node);
    node.name = store_string(state, name);
    node.mesh = mesh_index;
    state->nodes.push_back(node);
    state->scene_nodes.push_back(static_cast<int32_t>(state->nodes.size() - 1));
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
