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
    require(model.nodes_count == 3 && model.scenes_count == 1,
	"written glTF has an incorrect articulation graph");
    require(model.scenes[0].nodes_count == 1 && model.scenes[0].nodes[0] == 0,
	"written glTF has an incorrect root node");
    require(model.nodes[0].children_count == 1 && model.nodes[1].children_count == 1,
	"written glTF hierarchy is incomplete");
    require(model.nodes[1].ext.extras && model.nodes[2].ext.extras,
	"written glTF omitted neutral joint metadata");
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
    require(source.execute(GCV_FILTER_WRITE, path) == 1, "glTF articulation export failed");
    check_gltf(path);

    context restored;
    require(restored.execute(GCV_FILTER_READ, path) == 1, "glTF articulation import failed");
    struct rt_db_internal slide_internal;
    struct rt_db_internal turn_internal;
    RT_DB_INTERNAL_INIT(&slide_internal);
    RT_DB_INTERNAL_INIT(&turn_internal);
    const struct rt_joint_internal *slide = read_joint(restored, "slide.j", slide_internal);
    const struct rt_joint_internal *turn = read_joint(restored, "turn.j", turn_internal);
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
check_transform_import(const char *path, const char *transform)
{
    write_transform_fixture(path, transform);
    context imported;
    require(imported.execute(GCV_FILTER_READ, path) == 1, "glTF transform import failed");

    struct rt_db_internal bot_internal;
    RT_DB_INTERNAL_INIT(&bot_internal);
    const struct directory *bot_directory = db_lookup(imported.value.dbip,
	"triangle0", LOOKUP_QUIET);
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
    bu_file_delete(argv[1]);
    bu_file_delete(transform_path.c_str());
    try {
	check_roundtrip(argv[1]);
	check_transform_import(transform_path.c_str(),
	    R"("translation": [1, 2, 3], "rotation": [0, 0, 0.7071067811865476, 0.7071067811865476], "scale": [2, 3, 4])");
	check_transform_import(transform_path.c_str(),
	    R"("matrix": [0, 2, 0, 0, -3, 0, 0, 0, 0, 0, 4, 0, 1, 2, 3, 1])");
	bu_file_delete(argv[1]);
	bu_file_delete(transform_path.c_str());
	return 0;
    } catch (const std::exception &error) {
	bu_log("%s\n", error.what());
	bu_file_delete(argv[1]);
	bu_file_delete(transform_path.c_str());
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

