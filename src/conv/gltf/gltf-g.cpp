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

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "vmath.h"
#include "bn/mat.h"
#include "bu/app.h"
#include "bu/opt.h"
#include "bg/trimesh.h"
#include "wdb.h"

#include "gltf_read_util.h"

static bool
process_mesh(struct rt_wdb *outfp, const tg3_model &model, uint32_t mesh_index,
    uint32_t object_index, const mat_t transform, int verbosity)
{
    const tg3_mesh &mesh = model.meshes[mesh_index];
    std::string shape_name = brlcad_gltf::to_string(mesh.name);
    if (shape_name.empty())
	shape_name = "gltf_import_" + std::to_string(mesh_index);
    shape_name = brlcad_gltf::unique_object_name(outfp->dbip, shape_name, object_index);

    if (verbosity)
	std::cout << "BoT " << mesh_index << " name: " << shape_name << "\n";

    std::vector<fastf_t> vertices;
    std::vector<int> faces;
    for (uint32_t i = 0; i < mesh.primitives_count; ++i) {
	if (!brlcad_gltf::append_triangle_primitive(model, mesh_index, i, vertices, faces))
	    return false;
    }

    size_t vertex_count = vertices.size() / ELEMENTS_PER_POINT;
    size_t face_count = faces.size() / brlcad_gltf::TRIANGLE_VERTICES;
    if (vertex_count == 0 || face_count == 0) {
	bu_log("Error: glTF mesh %u contains no triangles\n", mesh_index);
	return false;
    }

    if (transform) {
	for (size_t i = 0; i < vertices.size(); i += ELEMENTS_PER_POINT) {
	    point_t point;
	    MAT4X3PNT(point, transform, &vertices[i]);
	    VMOVE(&vertices[i], point);
	}

	/* Baking a reflection into the vertices also reverses the winding. */
	if (bn_mat_det3(transform) < 0.0) {
	    for (size_t i = 0; i < faces.size(); i += brlcad_gltf::TRIANGLE_VERTICES)
		std::swap(faces[i + 1], faces[i + 2]);
	}
    }

    int type = RT_BOT_SURFACE;
    if (bg_trimesh_manifold_closed(vertex_count, face_count, vertices.data(), faces.data()))
	type = RT_BOT_SOLID;

    if (mk_bot(outfp, shape_name.c_str(), type, RT_BOT_CCW, 0,
	    vertex_count, face_count, vertices.data(), faces.data(), NULL, NULL) < 0) {
	bu_log("Unable to write glTF mesh %u as %s\n", mesh_index, shape_name.c_str());
	return false;
    }
    return true;
}


static bool
handle_node(struct rt_wdb *outfp, const tg3_model &model, int32_t node_index,
    const mat_t parent_transform, std::set<int32_t> &active_nodes, int verbosity)
{
    brlcad_gltf::active_node active(model, node_index, active_nodes);
    const tg3_node *node = active.get();
    if (!node)
	return false;

    mat_t local_transform, transform;
    brlcad_gltf::node_transform(local_transform, *node);
    bn_mat_mul(transform, parent_transform, local_transform);

    if (node->mesh != TG3_INDEX_NONE) {
	if (node->mesh < 0 || static_cast<uint32_t>(node->mesh) >= model.meshes_count) {
	    bu_log("glTF node %d has invalid mesh index %d\n", node_index, node->mesh);
	    return false;
	}
	if (!process_mesh(outfp, model, node->mesh, node_index, transform, verbosity))
	    return false;
    }

    for (uint32_t i = 0; i < node->children_count; ++i) {
	if (!handle_node(outfp, model, node->children[i], transform, active_nodes, verbosity))
	    return false;
    }
    return true;
}


static bool
convert_from_gltf(struct rt_wdb *outfp, const tg3_model &model, int verbosity)
{
    if (model.scenes_count == 0) {
	/* A glTF asset can be a mesh library with no node placements. */
	for (uint32_t i = 0; i < model.meshes_count; ++i) {
	    if (!process_mesh(outfp, model, i, i, NULL, verbosity))
		return false;
	}
	return true;
    }

    const tg3_scene *scene = brlcad_gltf::default_scene(model);
    if (!scene)
	return false;

    mat_t identity;
    MAT_IDN(identity);
    std::set<int32_t> active_nodes;
    for (uint32_t i = 0; i < scene->nodes_count; ++i) {
	if (!handle_node(outfp, model, scene->nodes[i], identity, active_nodes, verbosity))
	    return false;
    }
    return true;
}


int
main(int argc, char **argv)
{
    const char * const usage = "Usage: gltf-g [options] input_file [output_file.g]\n";
    int verbosity = 0;
    int print_help = 0;
    int extensions = 0;
    struct bu_vls output_path = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[6];
    BU_OPT(d[0], "h", "help",       "", NULL,              &print_help,   "Print help and exit");
    BU_OPT(d[1], "?", "",           "", NULL,              &print_help,   "");
    BU_OPT(d[2], "v", "verbosity",  "", &bu_opt_incr_long, &verbosity,    "Increase verbosity level");
    BU_OPT(d[3], "o", "output",     "", &bu_opt_str,       &output_path,  "Set output filename");
    BU_OPT(d[4], "E", "extensions", "", NULL,              &extensions,   "Store original JSON for extensions");
    BU_OPT_NULL(d[5]);

    bu_setprogname(argv[0]);

    if (argc > 0) {
	--argc;
	++argv;
    }
    int opt_ret = bu_opt_parse(NULL, argc, (const char **)argv, d);

    if (print_help) {
	char *help = bu_opt_describe(d, NULL);
	bu_log("%s\nOptions:\n%s", usage, help);
	if (help)
	    bu_free(help, "help str");
	bu_vls_free(&output_path);
	return BRLCAD_OK;
    }

    argc = opt_ret;
    if (argc < 2 && !bu_vls_strlen(&output_path))
	bu_exit(BRLCAD_ERROR, "Need input glTF filename\n");
    if (argc > 2 || (argc == 2 && bu_vls_strlen(&output_path)))
	bu_exit(BRLCAD_ERROR, "Multiple inputs specified.\n");
    if (argc == 2)
	bu_vls_sprintf(&output_path, "%s", argv[1]);

    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;
    brlcad_gltf::confined_file_reader reader(argv[0]);
    tg3_error_code parse_result = reader.parse(model.get(), errors.get(), extensions != 0);
    brlcad_gltf::log_errors(*errors.get());
    if (parse_result != TG3_OK) {
	bu_log("Failed to parse glTF input %s (error %d)\n", argv[0],
	    static_cast<int>(parse_result));
	bu_vls_free(&output_path);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *outfp = wdb_fopen(bu_vls_cstr(&output_path));
    if (!outfp) {
	bu_log("Unable to open output file %s\n", bu_vls_cstr(&output_path));
	bu_vls_free(&output_path);
	return BRLCAD_ERROR;
    }
    std::string title = "gltf-g import of " + std::string(argv[0]);

    if (verbosity)
	std::cout << "Mesh count: " << model->meshes_count << "\n";
    bool converted = mk_id(outfp, title.c_str()) >= 0 &&
	convert_from_gltf(outfp, *model.get(), verbosity);

    db_close(outfp->dbip);
    bu_vls_free(&output_path);
    return converted ? BRLCAD_OK : BRLCAD_ERROR;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
