/*                T E S T _ G C V _ G L T F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/ptbl.h"
#include "gcv/api.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/animation.h"
#include "rt/primitives/bot.h"
#include "rt/primitives/joint.h"
#include "wdb.h"

#include "tiny_gltf_v3.h"

namespace {

void
require(bool condition, const std::string &message)
{
    if (!condition)
	throw std::runtime_error(message);
}

class context {
public:
    context() { gcv_context_init(&value); }
    ~context() { gcv_context_destroy(&value); }
    context(const context &) = delete;
    context &operator=(const context &) = delete;

    int execute(enum gcv_filter_type type, const char *path)
    {
	const struct bu_ptbl *filters = gcv_list_filters(&value);
	for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i) {
	    const auto *filter = reinterpret_cast<const struct gcv_filter *>(BU_PTBL_GET(filters, i));
	    if (filter->filter_type == type && filter->mime_type == BU_MIME_MODEL_GLTF)
		return gcv_execute(&value, filter, nullptr, 0, nullptr, path);
	}
	throw std::runtime_error("glTF plugin was not loaded");
    }

    bool supports(const char *path)
    {
	const struct bu_ptbl *filters = gcv_list_filters(&value);
	for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i) {
	    const auto *filter = reinterpret_cast<const struct gcv_filter *>(BU_PTBL_GET(filters, i));
	    if (filter->filter_type == GCV_FILTER_READ &&
		filter->mime_type == BU_MIME_MODEL_GLTF)
		return filter->data_supported && filter->data_supported(path) != 0;
	}
	throw std::runtime_error("glTF plugin was not loaded");
    }

    struct gcv_context value;
};

void
add_joint(context &destination, const char *name, const char *parent,
    const char *child, const char *dof)
{
    const char *arguments[] = {
	"type", "general", "parent", parent, "child", child, "dof0", dof
    };
    struct rt_db_internal internal;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    struct rt_wdb *wdbp = wdb_dbopen(destination.value.dbip, RT_WDB_TYPE_DB_INMEM);
    point_t origin = VINIT_ZERO;

    RT_DB_INTERNAL_INIT(&internal);
    int result = OBJ[ID_JOINT].ft_make(&OBJ[ID_JOINT], &internal, nullptr, origin, 1.0);
    if (result == BRLCAD_OK)
	result = OBJ[ID_JOINT].ft_adjust(&messages, &internal,
	    static_cast<int>(sizeof(arguments) / sizeof(arguments[0])), arguments);
    if (result == BRLCAD_OK)
	result = wdb_put_internal(wdbp, name, &internal, 1.0);
    if (result != BRLCAD_OK) {
	std::string message = "joint setup failed: ";
	message += bu_vls_cstr(&messages);
	rt_db_free_internal(&internal);
	bu_vls_free(&messages);
	throw std::runtime_error(message);
    }
    rt_db_free_internal(&internal);
    bu_vls_free(&messages);
}

void
add_skinned_bot(context &destination)
{
    static const fastf_t vertices[] = {
	0.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 10.0, 0.0
    };
    static const int faces[] = {0, 1, 2};
    static const uint32_t offsets[] = {0, 1, 3, 4};
    static const uint32_t indices[] = {0, 0, 1, 1};
    static const fastf_t weights[] = {1.0, 0.25, 0.75, 1.0};
    struct rt_db_internal internal;
    struct rt_bot_internal *bot;
    struct rt_wdb *wdbp = wdb_dbopen(destination.value.dbip, RT_WDB_TYPE_DB_INMEM);

    RT_DB_INTERNAL_INIT(&internal);
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = RT_BOT_HAS_SKIN;
    bot->num_vertices = 3;
    bot->vertices = static_cast<fastf_t *>(bu_malloc(sizeof(vertices), "test vertices"));
    memcpy(bot->vertices, vertices, sizeof(vertices));
    bot->num_faces = 1;
    bot->faces = static_cast<int *>(bu_malloc(sizeof(faces), "test faces"));
    memcpy(bot->faces, faces, sizeof(faces));
    BU_ALLOC(bot->skin, struct rt_bot_skin);
    bot->skin->joint_count = 2;
    bot->skin->joint_links = static_cast<struct bu_vls *>(bu_calloc(
	2, sizeof(struct bu_vls), "test skin links"));
    bu_vls_init(&bot->skin->joint_links[0]);
    bu_vls_init(&bot->skin->joint_links[1]);
    bu_vls_strcpy(&bot->skin->joint_links[0], "base");
    bu_vls_strcpy(&bot->skin->joint_links[1], "tool");
    bot->skin->inverse_bind_matrices = static_cast<mat_t *>(bu_malloc(
	2 * sizeof(mat_t), "test inverse binds"));
    MAT_IDN(bot->skin->inverse_bind_matrices[0]);
    MAT_IDN(bot->skin->inverse_bind_matrices[1]);
    bot->skin->inverse_bind_matrices[1][3] = -10.0;
    bot->skin->influence_count = 4;
    bot->skin->vertex_offsets = static_cast<uint32_t *>(bu_malloc(
	sizeof(offsets), "test skin offsets"));
    bot->skin->joint_indices = static_cast<uint32_t *>(bu_malloc(
	sizeof(indices), "test skin indices"));
    bot->skin->weights = static_cast<fastf_t *>(bu_malloc(
	sizeof(weights), "test skin weights"));
    memcpy(bot->skin->vertex_offsets, offsets, sizeof(offsets));
    memcpy(bot->skin->joint_indices, indices, sizeof(indices));
    memcpy(bot->skin->weights, weights, sizeof(weights));
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];
    internal.idb_ptr = bot;
    require(rt_bot_skin_validate(bot, nullptr) == BRLCAD_OK &&
	wdb_put_internal(wdbp, "tool.bot", &internal, 1.0) == BRLCAD_OK,
	"skinned BOT setup failed");
    rt_db_free_internal(&internal);
}

void
add_animation(context &destination)
{
    static const fastf_t times[] = {0.0, 1.25};
    static const fastf_t values[] = {
	0.0, 0.0, 0.0,
	0.0, 0.0, 0.0,
	1.0, 2.0, 3.0,
	4.0, 5.0, 6.0,
	12.345678901234567, 0.0, 0.0,
	7.0, 8.0, 9.0
    };
    struct rt_db_internal internal;
    struct rt_wdb *wdbp = wdb_dbopen(destination.value.dbip, RT_WDB_TYPE_DB_INMEM);
    point_t origin = VINIT_ZERO;

    RT_DB_INTERNAL_INIT(&internal);
    require(OBJ[ID_ANIMATION].ft_make(&OBJ[ID_ANIMATION], &internal, nullptr,
	origin, 1.0) == BRLCAD_OK, "animation setup failed");
    auto *animation = static_cast<struct rt_animation_internal *>(internal.idb_ptr);
    animation->channel_count = 1;
    animation->channels = static_cast<struct rt_animation_channel *>(bu_calloc(
	1, sizeof(struct rt_animation_channel), "test animation channel"));
    struct rt_animation_channel &channel = animation->channels[0];
    bu_vls_init(&channel.target_link);
    bu_vls_strcpy(&channel.target_link, "tool");
    channel.target = RT_ANIMATION_TRANSLATION;
    channel.interpolation = RT_ANIMATION_CUBICSPLINE;
    channel.keyframe_count = 2;
    channel.times = static_cast<fastf_t *>(bu_malloc(sizeof(times), "test animation times"));
    channel.values = static_cast<fastf_t *>(bu_malloc(sizeof(values), "test animation values"));
    memcpy(channel.times, times, sizeof(times));
    memcpy(channel.values, values, sizeof(values));
    require(rt_animation_validate(animation, nullptr) == 0 &&
	wdb_put_internal(wdbp, "motion.anim", &internal, 1.0) == BRLCAD_OK,
	"animation setup failed");
    rt_db_free_internal(&internal);
}

const struct rt_animation_internal *
read_animation(context &source, const char *name, struct rt_db_internal &internal)
{
    const struct directory *directory = db_lookup(source.value.dbip, name, LOOKUP_QUIET);
    require(directory && rt_db_get_internal(&internal, directory, source.value.dbip,
	nullptr) >= 0 && internal.idb_type == ID_ANIMATION,
	std::string("missing restored animation: ") + name);
    return static_cast<const struct rt_animation_internal *>(internal.idb_ptr);
}

const struct rt_joint_internal *
read_joint(context &source, const char *name, struct rt_db_internal &internal)
{
    const struct directory *directory = db_lookup(source.value.dbip, name, LOOKUP_QUIET);
    require(directory && rt_db_get_internal(&internal, directory, source.value.dbip, nullptr) >= 0 &&
	internal.idb_type == ID_JOINT, std::string("missing restored joint: ") + name);
    return static_cast<const struct rt_joint_internal *>(internal.idb_ptr);
}

bool
same_double(double first, double second)
{
    uint64_t first_bits;
    uint64_t second_bits;
    static_assert(sizeof(first_bits) == sizeof(first), "unexpected double size");
    memcpy(&first_bits, &first, sizeof(first_bits));
    memcpy(&second_bits, &second, sizeof(second_bits));
    return first_bits == second_bits;
}

bool
near_transform_value(double first, double second)
{
    static constexpr double transform_tolerance = 1.0e-12;
    return std::fabs(first - second) <= transform_tolerance;
}

bool
near_gltf_float(double first, double second)
{
    static constexpr double gltf_float_tolerance = 1.0e-5;
    return std::fabs(first - second) <= gltf_float_tolerance;
}

bool
has_attribute(const tg3_primitive &primitive, const char *name)
{
    for (uint32_t i = 0; i < primitive.attributes_count; ++i) {
	if (tg3_str_equals_cstr(primitive.attributes[i].key, name))
	    return true;
    }
    return false;
}

void
check_gltf(const char *path)
{
    std::ifstream stream(path, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
	std::istreambuf_iterator<char>());
    tg3_model model;
    tg3_error_stack errors;
    tg3_parse_options options;
    tg3_error_stack_init(&errors);
    tg3_parse_options_init(&options);
    tg3_error_code error = tg3_parse_auto(&model, &errors, bytes.data(), bytes.size(),
	"", 0, &options);
    require(error == TG3_OK, "written glTF did not parse");
    require(model.nodes_count == 4 && model.scenes_count == 1 &&
	model.meshes_count == 1 && model.skins_count == 1 &&
	model.animations_count == 1,
	"written glTF has an incorrect articulation graph");

    require(model.scenes[0].nodes_count == 2 && model.scenes[0].nodes[0] == 0,
	"written glTF has an incorrect root node");
    require(model.nodes[0].children_count == 1 && model.nodes[1].children_count == 1,
	"written glTF hierarchy is incomplete");
    require(model.nodes[1].ext.extras && model.nodes[2].ext.extras,
	"written glTF omitted neutral joint metadata");
    require(model.nodes[3].mesh == 0 && model.nodes[3].skin == 0 &&
	model.meshes[0].primitives_count == 1 &&
	has_attribute(model.meshes[0].primitives[0], "JOINTS_0") &&
	has_attribute(model.meshes[0].primitives[0], "WEIGHTS_0"),
	"written glTF omitted the standard skin binding");
    require(model.animations[0].channels_count == 1 &&
	model.animations[0].samplers_count == 1 &&
	tg3_str_equals_cstr(model.animations[0].channels[0].target.path, "translation") &&
	tg3_str_equals_cstr(model.animations[0].samplers[0].interpolation,
	    "CUBICSPLINE") && model.animations[0].ext.extras,
	"written glTF omitted animation channels or exact metadata");
    tg3_model_free(&model);
    tg3_error_stack_free(&errors);
}

void
check_roundtrip(const char *path)
{
    const double slide_position = 5.1234567890123457;
    const double turn_position = 0.12345678901234566;
    context source;
    add_joint(source, "slide.j", "base", "slide",
	"translation limited {0 0 1} -2 8 5.1234567890123457");
    add_joint(source, "turn.j", "slide", "tool",
	"rotation limited {0 1 0} -1 1 0.12345678901234566");
    add_skinned_bot(source);
    add_animation(source);
    require(source.execute(GCV_FILTER_WRITE, path) == 1, "glTF articulation export failed");
    require(source.supports(path), "glTF reader did not recognize written output");
    check_gltf(path);

    context restored;
    require(restored.execute(GCV_FILTER_READ, path) == 1, "glTF articulation import failed");
    struct rt_db_internal slide_internal;
    struct rt_db_internal turn_internal;
    RT_DB_INTERNAL_INIT(&slide_internal);
    RT_DB_INTERNAL_INIT(&turn_internal);
    const struct rt_joint_internal *slide = read_joint(restored, "slide.j", slide_internal);
    const struct rt_joint_internal *turn = read_joint(restored, "turn.j", turn_internal);
    struct rt_db_internal animation_internal;
    RT_DB_INTERNAL_INIT(&animation_internal);
    const struct rt_animation_internal *animation = read_animation(restored,
	"motion.anim", animation_internal);
    require(slide->dof_count == 1 && slide->dofs[0].motion == RT_JOINT_TRANSLATION &&
	(slide->dofs[0].flags & RT_JOINT_DOF_LIMITED) &&
	EQUAL(slide->dofs[0].lower, -2.0) && EQUAL(slide->dofs[0].upper, 8.0) &&
	same_double(slide->dofs[0].current, slide_position),
	"translated joint lost double precision in glTF metadata");
    require(turn->dof_count == 1 && turn->dofs[0].motion == RT_JOINT_ROTATION &&
	(turn->dofs[0].flags & RT_JOINT_DOF_LIMITED) &&
	EQUAL(turn->dofs[0].lower, -1.0) && EQUAL(turn->dofs[0].upper, 1.0) &&
	same_double(turn->dofs[0].current, turn_position),
	"rotated joint lost double precision in glTF metadata");
    struct rt_db_internal bot_internal;
    RT_DB_INTERNAL_INIT(&bot_internal);
    const struct directory *bot_directory = db_lookup(restored.value.dbip,
	"tool.bot", LOOKUP_QUIET);
    require(bot_directory && rt_db_get_internal(&bot_internal, bot_directory,
	restored.value.dbip, nullptr) >= 0 && bot_internal.idb_type == ID_BOT,
	"missing restored skinned BOT");
    const auto *bot = static_cast<const struct rt_bot_internal *>(bot_internal.idb_ptr);
    require(rt_bot_skin_validate(bot, nullptr) == BRLCAD_OK &&
	bot->skin->joint_count == 2 && bot->skin->influence_count == 4 &&
	BU_STR_EQUAL(bu_vls_cstr(&bot->skin->joint_links[1]), "tool") &&
	near_gltf_float(bot->skin->weights[1], 0.25) &&
	near_gltf_float(bot->skin->inverse_bind_matrices[1][3], -10.0),
	"standard glTF skin data did not round-trip within float precision");
    require(animation->channel_count == 1 &&
	animation->channels[0].interpolation == RT_ANIMATION_CUBICSPLINE &&
	same_double(animation->channels[0].values[12], 12.345678901234567),
	"animation lost double precision in glTF metadata");
    rt_db_free_internal(&bot_internal);
    rt_db_free_internal(&animation_internal);
    rt_db_free_internal(&turn_internal);
    rt_db_free_internal(&slide_internal);
}

void
write_transform_fixture(const char *path, const char *transform)
{
    static const char *prefix = R"({
  "asset": {"version": "2.0"},
  "buffers": [{"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA", "byteLength": 36}],
  "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36}],
  "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
  "meshes": [{"name": "triangle", "primitives": [{"attributes": {"POSITION": 0}}]}],
  "nodes": [{"mesh": 0, )";
    static const char *suffix = R"(}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";
    std::ofstream stream(path, std::ios::binary);
    stream << prefix << transform << suffix;
    require(stream.good(), "unable to write glTF transform fixture");
}

void
check_standard_animation_import(const char *path)
{
    static const char *fixture = R"({
  "asset": {"version": "2.0"},
  "buffers": [{"uri": "data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAABvEoM6bxIDO6abRDs=", "byteLength": 32}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 8},
    {"buffer": 0, "byteOffset": 8, "byteLength": 24}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR"},
    {"bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC3"}
  ],
  "nodes": [{"name": "tool"}],
  "animations": [{
    "name": "external.anim",
    "samplers": [{"input": 0, "output": 1, "interpolation": "LINEAR"}],
    "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]
  }],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";
    std::ofstream stream(path, std::ios::binary);
    stream << fixture;
    require(stream.good(), "unable to write standard animation fixture");
    stream.close();

    context imported;
    require(imported.execute(GCV_FILTER_READ, path) == 1,
	"standard glTF animation import failed");
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    const struct rt_animation_internal *animation = read_animation(imported,
	"external.anim", internal);
    require(animation->channel_count == 1 &&
	animation->channels[0].interpolation == RT_ANIMATION_LINEAR &&
	near_gltf_float(animation->channels[0].values[3], 1.0) &&
	near_gltf_float(animation->channels[0].values[4], 2.0) &&
	near_gltf_float(animation->channels[0].values[5], 3.0),
	"standard glTF animation units were not converted to millimeters");
    rt_db_free_internal(&internal);
    struct rt_db_internal joint_internal;
    RT_DB_INTERNAL_INIT(&joint_internal);
    const struct directory *joint_directory = db_lookup(imported.value.dbip,
	"tool.joint", LOOKUP_QUIET);
    require(joint_directory && rt_db_get_internal(&joint_internal,
	joint_directory, imported.value.dbip, nullptr) >= 0 &&
	joint_internal.idb_type == ID_JOINT,
	"standard glTF animation did not create a persistent joint graph");
    const auto *joint = static_cast<const struct rt_joint_internal *>(
	joint_internal.idb_ptr);
    require(BU_STR_EQUAL(bu_vls_cstr(&joint->parent_link), "gltf_world") &&
	BU_STR_EQUAL(bu_vls_cstr(&joint->child_link), "tool"),
	"standard glTF root joint links are incorrect");
    rt_db_free_internal(&joint_internal);
}

void
check_standard_rig_import(const char *path)
{
    static const char *fixture = R"({
  "asset": {"version": "2.0"},
  "buffers": [{"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAEAAAABAAAAAQAAAACAPwAAAAAAAAAAAAAAAAAAAD8AAAA/AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAgQQAAAAAAAAAAAACAPwAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAEEEAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAABAAABAQA==", "byteLength": 256}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 12},
    {"buffer": 0, "byteOffset": 48, "byteLength": 48},
    {"buffer": 0, "byteOffset": 96, "byteLength": 128},
    {"buffer": 0, "byteOffset": 224, "byteLength": 8},
    {"buffer": 0, "byteOffset": 232, "byteLength": 24}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
    {"bufferView": 1, "componentType": 5121, "count": 3, "type": "VEC4"},
    {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4"},
    {"bufferView": 3, "componentType": 5126, "count": 2, "type": "MAT4"},
    {"bufferView": 4, "componentType": 5126, "count": 2, "type": "SCALAR"},
    {"bufferView": 5, "componentType": 5126, "count": 2, "type": "VEC3"}
  ],
  "meshes": [{"name": "rigged_triangle", "primitives": [{"attributes": {"POSITION": 0, "JOINTS_0": 1, "WEIGHTS_0": 2}}]}],
  "nodes": [
    {"name": "base", "children": [1]},
    {"name": "tool", "translation": [1, 0, 0]},
    {"name": "mesh_node", "translation": [10, 0, 0], "mesh": 0, "skin": 0}
  ],
  "skins": [{"inverseBindMatrices": 3, "skeleton": 0, "joints": [0, 1]}],
  "animations": [{
    "name": "rig.anim",
    "samplers": [{"input": 4, "output": 5, "interpolation": "LINEAR"}],
    "channels": [{"sampler": 0, "target": {"node": 1, "path": "translation"}}]
  }],
  "scenes": [{"nodes": [0, 2]}],
  "scene": 0
})";
    std::ofstream stream(path, std::ios::binary);
    stream << fixture;
    require(stream.good(), "unable to write standard rig fixture");
    stream.close();

    context imported;
    require(imported.execute(GCV_FILTER_READ, path) == 1,
	"standard glTF rig import failed");

    struct rt_db_internal bot_internal;
    RT_DB_INTERNAL_INIT(&bot_internal);
    const struct directory *bot_directory = db_lookup(imported.value.dbip,
	"rigged_triangle", LOOKUP_QUIET);
    require(bot_directory && rt_db_get_internal(&bot_internal, bot_directory,
	imported.value.dbip, nullptr) >= 0 && bot_internal.idb_type == ID_BOT,
	"standard glTF rig did not create its mesh");
    const auto *bot = static_cast<const struct rt_bot_internal *>(bot_internal.idb_ptr);
    require(bot->skin && rt_bot_skin_validate(bot, nullptr) == BRLCAD_OK &&
	bot->skin->joint_count == 2 &&
	BU_STR_EQUAL(bu_vls_cstr(&bot->skin->joint_links[0]), "base") &&
	BU_STR_EQUAL(bu_vls_cstr(&bot->skin->joint_links[1]), "tool") &&
	EQUAL(bot->skin->inverse_bind_matrices[0][3], 10000.0) &&
	EQUAL(bot->skin->inverse_bind_matrices[1][3], 9000.0),
	"standard glTF skin links do not match the imported graph");

    struct rt_db_internal animation_internal;
    RT_DB_INTERNAL_INIT(&animation_internal);
    const struct rt_animation_internal *animation = read_animation(imported,
	"rig.anim", animation_internal);
    require(animation->channel_count == 1 &&
	BU_STR_EQUAL(bu_vls_cstr(&animation->channels[0].target_link), "tool"),
	"standard glTF animation target does not match the imported graph");

    struct rt_db_internal base_internal;
    struct rt_db_internal tool_internal;
    RT_DB_INTERNAL_INIT(&base_internal);
    RT_DB_INTERNAL_INIT(&tool_internal);
    const struct directory *base_directory = db_lookup(imported.value.dbip,
	"base.joint", LOOKUP_QUIET);
    const struct directory *tool_directory = db_lookup(imported.value.dbip,
	"tool.joint", LOOKUP_QUIET);
    require(base_directory && tool_directory &&
	rt_db_get_internal(&base_internal, base_directory, imported.value.dbip,
	    nullptr) >= 0 &&
	rt_db_get_internal(&tool_internal, tool_directory, imported.value.dbip,
	    nullptr) >= 0,
	"standard glTF rig did not create its joint graph");
    const auto *base = static_cast<const struct rt_joint_internal *>(base_internal.idb_ptr);
    auto *tool = static_cast<struct rt_joint_internal *>(tool_internal.idb_ptr);
    require(BU_STR_EQUAL(bu_vls_cstr(&base->parent_link), "gltf_world") &&
	BU_STR_EQUAL(bu_vls_cstr(&base->child_link), "base") &&
	BU_STR_EQUAL(bu_vls_cstr(&tool->parent_link), "base") &&
	BU_STR_EQUAL(bu_vls_cstr(&tool->child_link), "tool") &&
	EQUAL(tool->pose_translation[X], 1000.0),
	"standard glTF graph hierarchy or bind pose is incorrect");

    fastf_t sampled_translation[3];
    require(rt_animation_channel_sample(sampled_translation,
	&animation->channels[0], 1.0) == BRLCAD_OK,
	"standard glTF animation could not be sampled");
    VMOVE(tool->pose_translation, sampled_translation);
    const struct rt_joint_internal *joints[] = {base, tool};
    struct rt_joint_eval_options options = RT_JOINT_EVAL_OPTIONS_INIT;
    struct rt_joint_eval *evaluation = nullptr;
    options.root_link = "gltf_world";
    require(rt_joint_eval_create(&evaluation, joints, 2, &options) == BRLCAD_OK,
	"standard glTF joint graph could not be evaluated");
    mat_t joint_matrices[2];
    require(rt_joint_eval_find(joint_matrices[0], evaluation, "base") == BRLCAD_OK &&
	rt_joint_eval_find(joint_matrices[1], evaluation, "tool") == BRLCAD_OK,
	"standard glTF skin links were not found in the evaluated graph");
    struct rt_bot_internal *posed = nullptr;
    require(rt_bot_skin_pose(&posed, bot, joint_matrices, 2, nullptr) == BRLCAD_OK &&
	posed && !posed->skin && !(posed->bot_flags & RT_BOT_HAS_SKIN) &&
	EQUAL(posed->vertices[3], 11000.0) &&
	EQUAL(posed->vertices[4], 1000.0) &&
	EQUAL(posed->vertices[5], 1500.0) &&
	EQUAL(posed->vertices[6], 10000.0) &&
	EQUAL(posed->vertices[7], 3000.0) &&
	EQUAL(posed->vertices[8], 3000.0),
	"standard glTF rig did not produce the expected sampled skin pose");
    rt_bot_internal_free(posed);
    rt_joint_eval_destroy(evaluation);
    rt_db_free_internal(&tool_internal);
    rt_db_free_internal(&base_internal);
    rt_db_free_internal(&animation_internal);
    rt_db_free_internal(&bot_internal);
}

void
check_transform_import(const char *path, const char *transform)
{
    write_transform_fixture(path, transform);
    context imported;
    require(imported.execute(GCV_FILTER_READ, path) == 1, "glTF transform import failed");

    struct rt_db_internal bot_internal;
    RT_DB_INTERNAL_INIT(&bot_internal);
    const struct directory *bot_directory = db_lookup(imported.value.dbip,
	"triangle", LOOKUP_QUIET);
    require(bot_directory && rt_db_get_internal(&bot_internal, bot_directory,
	imported.value.dbip, nullptr) >= 0 && bot_internal.idb_type == ID_BOT,
	"glTF transform fixture did not create its mesh");
    const auto *bot = static_cast<const struct rt_bot_internal *>(bot_internal.idb_ptr);
    require(bot->num_vertices == 3 && EQUAL(bot->vertices[3], 1000.0) &&
	EQUAL(bot->vertices[7], 1000.0), "glTF mesh coordinates were not converted to millimeters");
    rt_db_free_internal(&bot_internal);

    struct rt_db_internal all_internal;
    RT_DB_INTERNAL_INIT(&all_internal);
    const struct directory *all_directory = db_lookup(imported.value.dbip, "all", LOOKUP_QUIET);
    require(all_directory && rt_db_get_internal(&all_internal, all_directory,
	imported.value.dbip, nullptr) >= 0 && all_internal.idb_type == ID_COMBINATION,
	"glTF transform fixture did not create its scene");
    const auto *combination = static_cast<const struct rt_comb_internal *>(all_internal.idb_ptr);
    const union tree *tree = combination->tree;
    require(tree && tree->tr_op == OP_DB_LEAF && tree->tr_l.tl_mat,
	"glTF node transform was not retained");
    const matp_t matrix = tree->tr_l.tl_mat;
    require(near_transform_value(matrix[0], 0.0) &&
	near_transform_value(matrix[1], -3.0) &&
	near_transform_value(matrix[2], 0.0) &&
	near_transform_value(matrix[3], 1000.0) &&
	near_transform_value(matrix[4], 2.0) &&
	near_transform_value(matrix[5], 0.0) &&
	near_transform_value(matrix[6], 0.0) &&
	near_transform_value(matrix[7], 2000.0) &&
	near_transform_value(matrix[10], 4.0) &&
	near_transform_value(matrix[11], 3000.0),
	"glTF TRS transform was imported incorrectly");
    rt_db_free_internal(&all_internal);
}

} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 2)
	return 1;
    std::string transform_path = std::string(argv[1]) + ".transform.gltf";
    std::string animation_path = std::string(argv[1]) + ".animation.gltf";
    std::string rig_path = std::string(argv[1]) + ".rig.gltf";
    bu_file_delete(argv[1]);
    bu_file_delete(transform_path.c_str());
    bu_file_delete(animation_path.c_str());
    bu_file_delete(rig_path.c_str());
    try {
	check_roundtrip(argv[1]);
	check_transform_import(transform_path.c_str(),
	    R"("translation": [1, 2, 3], "rotation": [0, 0, 0.7071067811865476, 0.7071067811865476], "scale": [2, 3, 4])");
	check_transform_import(transform_path.c_str(),
	    R"("matrix": [0, 2, 0, 0, -3, 0, 0, 0, 0, 0, 4, 0, 1, 2, 3, 1])");
	check_standard_animation_import(animation_path.c_str());
	check_standard_rig_import(rig_path.c_str());
	bu_file_delete(argv[1]);
	bu_file_delete(transform_path.c_str());
	bu_file_delete(animation_path.c_str());
	bu_file_delete(rig_path.c_str());
	return 0;
    } catch (const std::exception &error) {
	bu_log("%s\n", error.what());
	bu_file_delete(argv[1]);
	bu_file_delete(transform_path.c_str());
	bu_file_delete(animation_path.c_str());
	bu_file_delete(rig_path.c_str());
	return 1;
    }
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

