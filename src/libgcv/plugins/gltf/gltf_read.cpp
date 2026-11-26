/*                   G L T F _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2002-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */

#include "common.h"

#include <climits>
#include <cmath>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "bg/trimesh.h"
#include "bu/cv.h"
#include "gcv/api.h"
#include "icv.h"
#include "vmath.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

#include "gltf_read_util.h"

struct mesh_members
{
    std::vector<std::string> names;
    bool contains_regions = false;
};

struct conversion_state
{
    struct rt_wdb *fd_out;
    brlcad_gltf::confined_file_reader *reader;
    std::vector<mesh_members> meshes;
    std::vector<std::string> image_shaders;
    struct wmember scene;
};


static void
free_image_bytes(void *data)
{
    bu_free(data, "glTF image bytes");
}


static tg3_span_u8
image_bytes(const tg3_model &model, const tg3_image &image,
    brlcad_gltf::confined_file_reader &reader, std::unique_ptr<void, decltype(&free_image_bytes)> &storage)
{
    if (image.image.data)
	return image.image;

    if (image.buffer_view != TG3_INDEX_NONE) {
	if (image.buffer_view < 0 || static_cast<uint32_t>(image.buffer_view) >= model.buffer_views_count)
	    return {};
	const tg3_buffer_view &view = model.buffer_views[image.buffer_view];
	if (view.buffer < 0 || static_cast<uint32_t>(view.buffer) >= model.buffers_count)
	    return {};
	const tg3_span_u8 &buffer = model.buffers[view.buffer].data;
	if (!buffer.data || view.byte_offset > buffer.count ||
	    view.byte_length > buffer.count - view.byte_offset)
	    return {};
	return {buffer.data + view.byte_offset, view.byte_length};
    }

    if (tg3_is_data_uri(image.uri.data, image.uri.len)) {
	const std::string uri = brlcad_gltf::to_string(image.uri);
	const std::string marker = ";base64,";
	const size_t offset = uri.find(marker);
	if (offset == std::string::npos)
	    return {};
	const size_t start = offset + marker.size();
	/* The base64 helper returns an int and overallocates its output. */
	if (uri.size() - start > brlcad_gltf::MAX_FILE_BYTES / 8)
	    return {};
	signed char *data = nullptr;
	int count = bu_b64_decode_block(&data,
	    reinterpret_cast<const signed char *>(uri.data() + start), uri.size() - start);
	storage.reset(data);
	if (count <= 0)
	    return {};
	return {reinterpret_cast<const uint8_t *>(data), static_cast<uint64_t>(count)};
    }

    uint8_t *data = nullptr;
    uint64_t count = 0;
    if (!reader.read_resource(image.uri, &data, &count))
	return {};
    storage.reset(data);
    return {data, count};
}


static bu_mime_image_t
image_format(tg3_span_u8 bytes)
{
    const uint8_t png_signature[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    const uint8_t jpeg_signature[] = {0xff, 0xd8, 0xff};
    if (bytes.count >= sizeof(png_signature) &&
	std::memcmp(bytes.data, png_signature, sizeof(png_signature)) == 0)
	return BU_MIME_IMAGE_PNG;
    if (bytes.count >= sizeof(jpeg_signature) &&
	std::memcmp(bytes.data, jpeg_signature, sizeof(jpeg_signature)) == 0)
	return BU_MIME_IMAGE_JPEG;
    return BU_MIME_IMAGE_UNKNOWN;
}


static const std::string &
texture_shader(struct conversion_state &state, const tg3_model &model, uint32_t image_index)
{
    std::string &shader = state.image_shaders[image_index];
    if (!shader.empty())
	return shader;

    std::unique_ptr<void, decltype(&free_image_bytes)> storage(nullptr, free_image_bytes);
    const tg3_span_u8 bytes = image_bytes(model, model.images[image_index], *state.reader, storage);
    const bu_mime_image_t format = image_format(bytes);
    std::unique_ptr<icv_image_t, decltype(&icv_destroy)> image(
	format == BU_MIME_IMAGE_UNKNOWN ? nullptr : icv_read_mem(bytes.data, bytes.count, format, 0, 0),
	icv_destroy);
    if (!image || !image->width || !image->height || image->width > INT_MAX ||
	image->height > INT_MAX / image->width / ELEMENTS_PER_VECT) {
	bu_log("Warning: unable to decode glTF base-color image %u\n", image_index);
	return shader;
    }

    unsigned char *pixels = nullptr;
    size_t count = 0;
    int result = icv_write_mem(image.get(), &pixels, &count, BU_MIME_IMAGE_PIX);
    std::unique_ptr<void, decltype(&free_image_bytes)> pixels_storage(pixels, free_image_bytes);
    if (result || count != image->width * image->height * ELEMENTS_PER_VECT) {
	bu_log("Warning: unable to convert glTF base-color image %u to RGB\n", image_index);
	return shader;
    }

    const std::string name = brlcad_gltf::unique_object_name(state.fd_out->dbip,
	"gltf_image_" + std::to_string(image_index), image_index);
    if (mk_binunif(state.fd_out, name.c_str(), pixels, WDB_BINUNIF_UINT8, static_cast<long>(count)) < 0) {
	bu_log("Warning: unable to embed glTF base-color image %u\n", image_index);
	return shader;
    }

    shader = "obj=" + name + " w=" + std::to_string(image->width) +
	" n=" + std::to_string(image->height);
    return shader;
}


static bool
read_uvs(const tg3_model &model, const tg3_primitive &primitive, int32_t tex_coord,
    size_t vertex_count, std::vector<fastf_t> &uvs)
{
    const std::string attribute = "TEXCOORD_" + std::to_string(tex_coord);
    brlcad_gltf::accessor_view view;
    if (!brlcad_gltf::get_accessor_view(model, brlcad_gltf::find_attribute(primitive, attribute.c_str()), view) ||
	view.accessor->type != TG3_TYPE_VEC2 || view.accessor->count != vertex_count)
	return false;

    const int32_t component = view.accessor->component_type;
    const int32_t component_size = tg3_component_size(component);
    const double divisor = (component == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) ? UINT8_MAX : UINT16_MAX;
    uvs.resize(vertex_count * ELEMENTS_PER_VECT);
    for (size_t i = 0; i < vertex_count; ++i) {
	for (size_t axis = 0; axis < ELEMENTS_PER_POINT2D; ++axis) {
	    const uint8_t *data = view.data + i * view.stride + axis * component_size;
	    double coordinate;
	    if (component == TG3_COMPONENT_TYPE_FLOAT) {
		if (!brlcad_gltf::read_coordinate(data, component, coordinate) || !std::isfinite(coordinate))
		    return false;
	    } else {
		size_t value;
		if (!view.accessor->normalized || component_size > 2 ||
		    !brlcad_gltf::read_index(data, component, value))
		    return false;
		coordinate = value / divisor;
	    }
	    /* The texture shader counts rows from the bottom. */
	    uvs[i * ELEMENTS_PER_VECT + axis] = (axis == Y) ? 1.0 - coordinate : coordinate;
	}
    }
    return true;
}


static std::string
primitive_shader(struct conversion_state &state, const tg3_model &model,
    const tg3_primitive &primitive, size_t vertex_count, std::vector<fastf_t> &uvs)
{
    if (primitive.material < 0 || static_cast<uint32_t>(primitive.material) >= model.materials_count)
	return {};
    const tg3_texture_info &color = model.materials[primitive.material].pbr_metallic_roughness.base_color_texture;
    if (color.index < 0 || static_cast<uint32_t>(color.index) >= model.textures_count)
	return {};
    const int32_t image = model.textures[color.index].source;
    if (image < 0 || static_cast<uint32_t>(image) >= model.images_count)
	return {};
    if (!read_uvs(model, primitive, color.tex_coord, vertex_count, uvs)) {
	bu_log("Warning: glTF base-color texture %d has no usable TEXCOORD_%d data\n",
	    color.index, color.tex_coord);
	return {};
    }
    return texture_shader(state, model, static_cast<uint32_t>(image));
}


static bool
write_mesh(struct conversion_state &state, const tg3_model &model,
    int32_t mesh_index, int32_t node_index, mesh_members &members)
{
    const tg3_mesh &mesh = model.meshes[mesh_index];
    std::string shape_name = brlcad_gltf::to_string(mesh.name);
    if (shape_name.empty())
	shape_name = "shape" + std::to_string(node_index);

    std::vector<std::string> shaders;
    for (uint32_t i = 0; i < mesh.primitives_count; ++i) {
	std::vector<fastf_t> vertices;
	std::vector<int> faces;
	if (!brlcad_gltf::append_triangle_primitive(model, mesh_index, i, vertices, faces))
	    return false;

	const size_t vertex_count = vertices.size() / ELEMENTS_PER_POINT;
	const size_t face_count = faces.size() / brlcad_gltf::TRIANGLE_VERTICES;
	const std::string bot_name = brlcad_gltf::unique_object_name(state.fd_out->dbip,
	    shape_name + std::to_string(i), mesh_index);
	std::vector<fastf_t> uvs;
	const std::string shader = primitive_shader(state, model, mesh.primitives[i], vertex_count, uvs);
	const bool textured = !shader.empty();
	int mode = RT_BOT_SOLID;
	if (textured && !bg_trimesh_manifold_closed(vertex_count, face_count, vertices.data(), faces.data()))
	    mode = RT_BOT_SURFACE;
	if (mk_bot_w_normals_and_uvs(state.fd_out, bot_name.c_str(), mode, RT_BOT_UNORIENTED,
		textured ? RT_BOT_HAS_TEXTURE_UVS : 0, vertex_count, face_count, vertices.data(),
		faces.data(), NULL, NULL, 0, NULL, NULL, textured ? vertex_count : 0,
		uvs.data(), faces.data()) < 0) {
	    bu_log("Unable to write glTF mesh %d primitive %u\n", mesh_index, i);
	    return false;
	}
	members.names.push_back(bot_name);
	shaders.push_back(shader);
	members.contains_regions |= textured;
    }

    if (!members.contains_regions)
	return true;

    /* A glTF mesh can bind a different material to each primitive. */
    for (size_t i = 0; i < members.names.size(); ++i) {
	std::string &name = members.names[i];
	const std::string region_name = brlcad_gltf::unique_object_name(state.fd_out->dbip,
	    name + ".r", mesh_index);
	struct wmember region;
	BU_LIST_INIT(&region.l);
	if (!mk_addmember(name.c_str(), &region.l, NULL, WMOP_UNION))
	    return false;
	const bool textured = !shaders[i].empty();
	if (mk_lrcomb(state.fd_out, region_name.c_str(), &region, 1, textured ? "texture" : NULL,
		textured ? shaders[i].c_str() : NULL, NULL, 0, 0, 0, 100, 0) < 0)
	    return false;
	name = region_name;
    }
    return true;
}


static bool
add_mesh(struct conversion_state &state, const tg3_model &model, int32_t node_index,
    wmember &region, bool &contains_regions)
{
    const int32_t mesh_index = model.nodes[node_index].mesh;
    if (mesh_index < 0 || static_cast<uint32_t>(mesh_index) >= model.meshes_count) {
	bu_log("glTF node %d has invalid mesh index %d\n", node_index, mesh_index);
	return false;
    }

    auto &members = state.meshes[mesh_index];
    if (members.names.empty() && !write_mesh(state, model, mesh_index, node_index, members))
	return false;

    for (const auto &name : members.names) {
	if (!mk_addmember(name.c_str(), &region.l, NULL, WMOP_UNION))
	    return false;
    }
    contains_regions = members.contains_regions;
    return true;
}

static bool
handle_node(struct conversion_state &state, const tg3_model &model,
    int32_t node_index, struct wmember &regions, std::set<int32_t> &active_nodes)
{
    brlcad_gltf::active_node active(model, node_index, active_nodes);
    const tg3_node *node = active.get();
    if (!node)
	return false;
    if (node->children_count == 0 && node->mesh == TG3_INDEX_NONE)
	return true;

    struct wmember region;
    BU_LIST_INIT(&region.l);
    struct wmember_list_guard {
	struct bu_list *head;
	~wmember_list_guard() { if (head) mk_freemembers(head); }
    } region_guard = {&region.l};

    bool contains_regions = false;
    if (node->mesh != TG3_INDEX_NONE && !add_mesh(state, model, node_index, region, contains_regions))
	return false;
    for (uint32_t i = 0; i < node->children_count; ++i) {
	if (!handle_node(state, model, node->children[i], region, active_nodes))
	    return false;
    }
    if (BU_LIST_IS_EMPTY(&region.l))
	return true;

    const std::string region_name = brlcad_gltf::unique_object_name(state.fd_out->dbip,
	"Region_" + std::to_string(node_index), node_index);
    int comb_result = mk_lrcomb(state.fd_out, region_name.c_str(), &region,
	node->children_count == 0 && !contains_regions ? 1 : 0, NULL, NULL, NULL, 0, 0, 0, 100, 0);
    region_guard.head = NULL;
    if (comb_result < 0)
	return false;

    mat_t matrix;
    brlcad_gltf::node_transform(matrix, *node);
    return mk_addmember(region_name.c_str(), &regions.l, matrix, WMOP_UNION) != NULL;
}


static bool
convert_from_gltf(struct conversion_state &state, const tg3_model &model)
{
    if (model.scenes_count == 0) {
	bu_log("glTF input does not define a scene\n");
	return false;
    }
    const tg3_scene *scene = brlcad_gltf::default_scene(model);
    if (!scene)
	return false;

    std::set<int32_t> active_nodes;
    for (uint32_t i = 0; i < scene->nodes_count; ++i) {
	if (!handle_node(state, model, scene->nodes[i], state.scene, active_nodes))
	    return false;
    }
    return true;
}


static int
gltf_read(struct gcv_context *context, const struct gcv_opts *UNUSED(gcv_options),
    const void *UNUSED(options_data), const char *source_path)
{
    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;
    brlcad_gltf::confined_file_reader reader(source_path);
    tg3_error_code parse_result = reader.parse(model.get(), errors.get());
    brlcad_gltf::log_errors(*errors.get());
    if (parse_result != TG3_OK) {
	bu_log("Failed to parse glTF input %s (error %d)\n", source_path,
	    static_cast<int>(parse_result));
	return 0;
    }

    struct conversion_state state = {};
    state.fd_out = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_DEFAULT);
    state.reader = &reader;
    state.meshes.resize(model->meshes_count);
    state.image_shaders.resize(model->images_count);
    BU_LIST_INIT(&state.scene.l);

    std::string title = "glTF conversion from " + std::string(source_path);
    if (mk_id(state.fd_out, title.c_str()) < 0)
	return 0;
    if (!convert_from_gltf(state, *model.get())) {
	mk_freemembers(&state.scene.l);
	return 0;
    }

    const std::string scene_name = brlcad_gltf::unique_object_name(context->dbip, "all", 0);
    if (mk_lcomb(state.fd_out, scene_name.c_str(), &state.scene, 0, NULL, NULL, NULL, 0) < 0)
	return 0;

    return 1;
}


static int
gltf_can_read(const char *UNUSED(data))
{
    /* FIXME */
    return 0;
}

extern "C"
{
    static const struct gcv_filter gcv_conv_gltf_read = {
	"GLTF Reader", GCV_FILTER_READ, BU_MIME_MODEL_GLTF, gltf_can_read,
	NULL, NULL, gltf_read
    };

    static const struct gcv_filter * const filters[] = { &gcv_conv_gltf_read, NULL, NULL };

    const struct gcv_plugin gcv_plugin_info_s = { filters };

    COMPILER_DLLEXPORT const struct gcv_plugin *
	gcv_plugin_info() { return &gcv_plugin_info_s; }
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
