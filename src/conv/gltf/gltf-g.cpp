/*                      G L T F - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2026 United States Government as represented by
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

#include <climits>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "vmath.h"
#include "bu/app.h"
#include "bu/arg.h"
#include "bg/trimesh.h"
#include "wdb.h"

#include "gltf_read_util.h"

struct gltf_args {
    int print_help;
    long verbosity;
    const char *output_path;
    int extensions;
};

static const struct bu_arg_option gltf_options[] = {
    BU_ARG_FLAG("h", "help", struct gltf_args, print_help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_COUNTING_FLAG("v", "verbosity", struct gltf_args, verbosity,
	BU_ARG_VALUE_LONG, "Increase verbosity level"),
    BU_ARG_VALUE("o", "output", struct gltf_args, output_path,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "file", "Set output filename"),
    BU_ARG_FLAG("E", "extensions", struct gltf_args, extensions,
	"Store original JSON for extensions"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand gltf_operands[] = {
    BU_ARG_OPERAND("input_file", BU_ARG_VALUE_FILE, 1, 1, "Input glTF file", NULL),
    BU_ARG_OPERAND("output_file.g", BU_ARG_VALUE_FILE, 0, 1, "Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema gltf_schema = BU_ARG_SCHEMA(
    "gltf-g", "Convert glTF geometry to a BRL-CAD database", gltf_options,
    gltf_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static bool
process_mesh(struct rt_wdb *outfp, const tg3_model &model, const tg3_mesh &mesh,
    int verbosity, uint32_t mesh_number)
{
    std::string shape_name = brlcad_gltf::to_string(mesh.name);
    if (shape_name.empty())
	shape_name = "gltf_import_" + std::to_string(mesh_number);

    if (verbosity)
	std::cout << "BoT " << mesh_number << " name: " << shape_name << "\n";

    std::vector<fastf_t> vertices;
    std::vector<int> faces;
    for (uint32_t primitive_number = 0; primitive_number < mesh.primitives_count;
	++primitive_number) {
	const tg3_primitive &primitive = mesh.primitives[primitive_number];
	if (primitive.mode != TG3_MODE_TRIANGLES) {
	    bu_log("Error: glTF primitive %u is not a triangle mesh\n", primitive_number);
	    return false;
	}

	int32_t position_index = brlcad_gltf::find_attribute(primitive, "POSITION");
	brlcad_gltf::accessor_view positions;
	if (position_index == TG3_INDEX_NONE ||
	    !brlcad_gltf::get_accessor_view(model, position_index, positions) ||
	    positions.accessor->type != TG3_TYPE_VEC3 ||
	    !brlcad_gltf::is_supported_position_component(
		positions.accessor->component_type) ||
	    positions.accessor->count > std::numeric_limits<size_t>::max()) {
	    bu_log("Error: glTF primitive %u has invalid POSITION data\n", primitive_number);
	    return false;
	}
	if (positions.accessor->component_type == TG3_COMPONENT_TYPE_DOUBLE)
	    brlcad_gltf::warn_legacy_double_position(mesh_number, primitive_number);

	size_t primitive_vertex_count = static_cast<size_t>(positions.accessor->count);
	size_t first_vertex = vertices.size() / 3;
	if (primitive_vertex_count > static_cast<size_t>(INT_MAX) - first_vertex ||
	    primitive_vertex_count > (vertices.max_size() - vertices.size()) / 3) {
	    bu_log("Error: glTF mesh %u has too many vertices\n", mesh_number);
	    return false;
	}
	vertices.resize(vertices.size() + primitive_vertex_count * 3);

	int32_t component_size = tg3_component_size(positions.accessor->component_type);
	for (size_t vertex = 0; vertex < primitive_vertex_count; ++vertex) {
	    for (size_t axis = 0; axis < 3; ++axis) {
		double coordinate;
		if (!brlcad_gltf::read_coordinate(
			positions.data + vertex * positions.stride + axis * component_size,
			positions.accessor->component_type, coordinate))
		    return false;
		vertices[(first_vertex + vertex) * 3 + axis] =
		    static_cast<fastf_t>(coordinate);
	    }
	}

	brlcad_gltf::accessor_view indices;
	const tg3_accessor *index_accessor = nullptr;
	size_t index_count = primitive_vertex_count;
	if (primitive.indices != TG3_INDEX_NONE) {
	    if (!brlcad_gltf::get_accessor_view(model, primitive.indices, indices) ||
		indices.accessor->type != TG3_TYPE_SCALAR ||
		indices.accessor->count > std::numeric_limits<size_t>::max()) {
		bu_log("Error: glTF primitive %u has invalid index data\n", primitive_number);
		return false;
	    }
	    index_accessor = indices.accessor;
	    index_count = static_cast<size_t>(index_accessor->count);
	}
	if (index_count == 0 || index_count % 3 != 0 ||
	    index_count > faces.max_size() - faces.size()) {
	    bu_log("Error: glTF primitive %u does not contain complete triangles\n",
		primitive_number);
	    return false;
	}

	size_t first_index = faces.size();
	faces.resize(first_index + index_count);
	for (size_t i = 0; i < index_count; ++i) {
	    size_t index = i;
	    if (index_accessor &&
		!brlcad_gltf::read_index(indices.data + i * indices.stride,
		    index_accessor->component_type, index)) {
		bu_log("Error: glTF primitive %u uses an unsupported index type\n",
		    primitive_number);
		return false;
	    }
	    if (index >= primitive_vertex_count) {
		bu_log("Error: glTF primitive %u contains an invalid vertex index\n",
		    primitive_number);
		return false;
	    }
	    faces[first_index + i] = static_cast<int>(first_vertex + index);
	}
    }

    size_t vertex_count = vertices.size() / 3;
    size_t face_count = faces.size() / 3;
    if (vertex_count == 0 || face_count == 0) {
	bu_log("Error: glTF mesh %u contains no triangles\n", mesh_number);
	return false;
    }

    int type = RT_BOT_SURFACE;
    if (bg_trimesh_manifold_closed(vertex_count, face_count, vertices.data(), faces.data()))
	type = RT_BOT_SOLID;

    return mk_bot(outfp, shape_name.c_str(), type, RT_BOT_CCW, 0,
	vertex_count, face_count, vertices.data(), faces.data(), NULL, NULL) >= 0;
}


int
main(int argc, char **argv)
{
    const struct bu_arg_grammar grammar = {&gltf_schema, NULL, NULL};
    struct gltf_args args = {};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int schema_status = bu_arg_introspect(&msg, &grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (schema_status != 0) {
	bu_flog(schema_status > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return schema_status > 0 ? BRLCAD_OK : BRLCAD_ERROR;
    }

    if (argc > 0) {
	--argc;
	++argv;
    }
    int operand_index = bu_arg_schema_parse_complete(&gltf_schema, &args, &msg,
	argc, (const char **)argv);
    if (operand_index < 0) {
	bu_exit(BRLCAD_ERROR, "%s\n", bu_vls_cstr(&msg));
    }
    bu_vls_free(&msg);

    if (args.print_help) {
	char *help = bu_arg_schema_help(&gltf_schema, "gltf-g");
	if (help)
	    bu_log("%s", help);
	if (help)
	    bu_free(help, "help str");
	return 0;
    }

    argc -= operand_index;
    argv += operand_index;
    if (argc < 2 && !args.output_path)
	bu_exit(BRLCAD_ERROR, "Need output BRL-CAD filename\n");
    if (argc == 2 && args.output_path)
	bu_exit(BRLCAD_ERROR, "Multiple inputs specified.\n");
    const char *output_path = args.output_path ? args.output_path : argv[1];

    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;
    brlcad_gltf::confined_file_reader reader(argv[0]);
    tg3_error_code parse_result = reader.parse(model.get(), errors.get(), args.extensions != 0);
    brlcad_gltf::log_errors(*errors.get());
    if (parse_result != TG3_OK) {
	bu_log("Failed to parse glTF input %s (error %d)\n", argv[0],
	    static_cast<int>(parse_result));
	return BRLCAD_ERROR;
    }

    struct rt_wdb *outfp = wdb_fopen(output_path);
    if (!outfp) {
	bu_log("Unable to open output file %s\n", output_path);
	return BRLCAD_ERROR;
    }
    std::string title = "gltf-g import of " + std::string(argv[0]);
    mk_id(outfp, title.c_str());

    if (args.verbosity)
	std::cout << "Mesh count: " << model->meshes_count << "\n";
    for (uint32_t i = 0; i < model->meshes_count; ++i) {
	if (!process_mesh(outfp, *model.get(), model->meshes[i], args.verbosity, i)) {
	    db_close(outfp->dbip);
	    return BRLCAD_ERROR;
	}
    }

    db_close(outfp->dbip);
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
