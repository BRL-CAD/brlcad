/*                   G L T F _ R E A D . C P P
* BRL-CAD
*
* Copyright (c) 2002-2022 United States Government as represented by
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

#include <stdlib.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT

#include "tinygltf_headers/tiny_gltf.h"

#include "bu/cv.h"
#include "bu/getopt.h"
#include "bu/units.h"
#include "gcv/api.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

using namespace tinygltf;

struct gltf_read_options
{
};

struct conversion_state
{
	const struct gcv_opts *gcv_options;
	struct gltf_read_options *gltf_read_options;

	std::string input_file;	/* name of the input file */
	FILE *fd_in;		/* input file */
	struct rt_wdb *fd_out;	/* Resulting BRL-CAD file */

	struct wmember scene;

	int num_vert_values;
	int num_face_values;
	int num_norm_values;

	int bot_fcurr; //current face
	int id_no; //????
	int mat_code;
};

static void testingStringIntMap(const std::map<std::string, int> &m, int &pos) {
	std::map<std::string, int>::const_iterator it(m.begin());
	std::map<std::string, int>::const_iterator itEnd(m.end());
	for (; it != itEnd; it++) {
		//std::cout << Indent(indent) << it->first << ": " << it->second << std::endl;
		if (it->first == "POSITION")
		{
			pos = it->second;
			//std::cout << "BOT position: " << pos << std::endl;
		}

	}
}

void
generate_geometry(struct conversion_state *state, tinygltf::Model &model, int mesh_number, std::string shape_name, wmember &region)
{
	tinygltf::Mesh mesh = model.meshes[mesh_number];
	//for each mesh primitive
	for (int j = 0; j < mesh.primitives.size(); j++)
	{
		//get face data
		int indices_pos = mesh.primitives[j].indices;

		tinygltf::Accessor accessor = model.accessors.at(indices_pos);
		tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
		tinygltf::Buffer buffer = model.buffers[bufferView.buffer];
		//unsigned char to short int arr
		const unsigned char * dataPtr3 = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

		const int byte_stride3 = accessor.ByteStride(bufferView);
		//const size_t count = accessor.count;

		unsigned short* indices = (unsigned short*)dataPtr3;
		//int faces[bufferView.byteLength / byte_stride] ;
		//int numfaces = bufferView.byteLength / byte_stride;
		int numfaces = accessor.count;
		int *faces = new int[numfaces];
		for (long unsigned int i = 0; i < numfaces; i++)
		{
			faces[i] = indices[i];
		}
		//get vertex data
		int bot_pos = 0;
		testingStringIntMap(mesh.primitives[j].attributes, bot_pos);
		accessor = model.accessors[bot_pos];
		bufferView = model.bufferViews[accessor.bufferView];
		buffer = model.buffers[bufferView.buffer];
		//unsigned char to short int arr
		const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
			accessor.byteOffset;
		int byte_stride = accessor.ByteStride(bufferView);
		//const size_t count = accessor.count;

		float* positions = (float*)dataPtr;
		int numvert = accessor.count * 3;
		double* vertices = new double[numvert];
		for (long unsigned int i = 0; i < accessor.count; i++)
		{
			vertices[i * 3] = positions[i * 3];
			vertices[(i * 3) + 1] = positions[(i * 3) + 1];
			vertices[(i * 3) + 2] = positions[(i * 3) + 2];
		}
		//make shape and add to region
		mk_bot(state->fd_out, (shape_name + std::to_string(j)).c_str(), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, numvert / 3, numfaces / 3, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);
		(void)mk_addmember((char*)(shape_name + std::to_string(j)).c_str(), &region.l, NULL, WMOP_UNION);
	}
}

void
handle_node(struct conversion_state *state, tinygltf::Model &model, int node_index, struct wmember &regions)
{
	tinygltf::Node node = model.nodes[node_index];
	if (node.children.empty() && node.mesh == -1)
	{
		//trapping nodes without meshes or children to avoid errors
		//Fix me
		return;
	}
	std::string region_name = "Region_" +std::to_string(node_index);
	struct wmember region;
	//generate a list to hold this node's children nodes
	BU_LIST_INIT(&region.l);
	int mesh_number = node.mesh;
	//if the node is a leaf, therefore has a mesh
	if (mesh_number != -1)
	{
		tinygltf::Mesh mesh = model.meshes[node.mesh];
		std::string shape_name = model.meshes[mesh_number].name;
		if (mesh.name.empty())
		{
			shape_name = "shape" + std::to_string(node_index);
		}
		state->bot_fcurr = 0;
		state->id_no = 0;
		generate_geometry(state, model, mesh_number, shape_name, region);
		//make a combination between the region and the mesh
		mk_lrcomb(state->fd_out, region_name.c_str(), &region, 1, (char *)NULL, (char *)NULL, NULL, state->id_no, 0, 0, 100, 0);
	}
	//account for translation of children
	if (!node.translation.empty())
	{
		fastf_t arr[16] = { 0 };
		arr[0] = arr[5] = arr[10] = arr[15] = 1;
		arr[3] = (fastf_t)node.translation[0];
		arr[7] = (fastf_t)node.translation[1];
		arr[11] = (fastf_t)node.translation[2];
		(void)mk_addmember(region_name.c_str(), &regions.l, arr, WMOP_UNION);
	}
	//account for child transformation matrix
	else if (!node.matrix.empty())
	{
		fastf_t *arr = new fastf_t[16];
		for (int i = 0; i < 16; i++)
		{
			arr[i] = (fastf_t)node.matrix[i];
		}
		(void)mk_addmember(region_name.c_str(), &regions.l, arr, WMOP_UNION);
	}
	else
	{
		(void)mk_addmember(region_name.c_str(), &regions.l, NULL, WMOP_UNION);
	}
	//for each child node
	for (int i = 0; i < node.children.size(); i++)
	{
		handle_node(state, model, node.children[i], region);
	}
	//if the node had children, make them all into a region
	if (node.children.size() > 0)
	{
		mk_lrcomb(state->fd_out, region_name.c_str(), &region, 0, (char *)NULL, (char *)NULL, NULL, state->id_no, 0, 0, 100, 0);
	}
}

void
convert_from_gltf(struct conversion_state *state, tinygltf::Model &model)
{
	//assume file has one scene
	const tinygltf::Scene &scene = model.scenes[0];
	tinygltf::Node node;
	//for each top level scene node
	for (int i = 0; i < scene.nodes.size(); i++)
	{
		node = model.nodes[scene.nodes[i]];
		handle_node(state, model, scene.nodes[i], state->scene);

	}
}

HIDDEN int
gltf_read(struct gcv_context *context, const struct gcv_opts *UNUSED(gcv_options), const void *options_data, const char *source_path)
{

	tinygltf::Model model;
	std::string err;
	std::string warn;
	std::string input_filename(source_path);
	std::string ext = GetFilePathExtension(input_filename);

	bool ret = false;

	// getting undefined reference here for the class
	tinygltf::TinyGLTF gltf_ctx;

	if (ext.compare("glb") == 0) {
		std::cout << "File type: binary glTF" << std::endl;
		// assume binary glTF.
		ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn,
			input_filename);
	}
	else {
		std::cout << "File type: ASCII glTF" << std::endl;
		// assume ascii glTF.
		ret =
			gltf_ctx.LoadASCIIFromFile(&model, &err, &warn, input_filename.c_str());
	}

	if (!warn.empty()) {
		printf("Warn: %s\n", warn.c_str());
	}

	if (!err.empty()) {
		printf("Err: %s\n", err.c_str());
	}

	if (!ret) {
		printf("Failed to parse glTF\n");
		return -1;
	}


	struct conversion_state state;

	memset(&state, 0, sizeof(state));
	
	//generate list to hold scene geometry
	BU_LIST_INIT(&state.scene.l);
	state.input_file = input_filename;
	state.fd_out = context->dbip->dbi_wdbp;

	std::string title = "gltf conversion from" + state.input_file;
	//set geometry title
	mk_id(state.fd_out, title.c_str());

	convert_from_gltf(&state, model);

	//combine all top level regions
	mk_lcomb(context->dbip->dbi_wdbp, "all", &state.scene, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
	
	return 1;
}


HIDDEN int
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