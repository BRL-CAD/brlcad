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
	int interleaved;
};

struct conversion_state
{
	const struct gcv_opts *gcv_options;
	struct gltf_read_options *gltf_read_options;

	std::string input_file;	/* name of the input file */
	FILE *fd_in;		/* input file */
	struct rt_wdb *fd_out;	/* Resulting BRL-CAD file */

	int num_vert_values;
	int num_face_values;
	int num_norm_values;
};

static void testingStringIntMap(const std::map<std::string, int> &m, int &pos) {
	std::map<std::string, int>::const_iterator it(m.begin());
	std::map<std::string, int>::const_iterator itEnd(m.end());
	for (; it != itEnd; it++) {
		//std::cout << Indent(indent) << it->first << ": " << it->second << std::endl;
		if (it->first == "POSITION")
		{
			pos = it->second;
			std::cout << "BOT position: " << pos << std::endl;
		}

	}
}

void set_array_sizes(const tinygltf::Model &model, int &numvert, int &numfaces, int &numnorm)
{
	{

		int indices_pos = model.meshes[0].primitives[0].indices;

		//assume 1 accessor, 1 bufferview
		const tinygltf::Accessor &accessor = model.accessors[indices_pos];
		const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
		//unsigned char to short int arr
		const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
			accessor.byteOffset;
		const int byte_stride = accessor.ByteStride(bufferView);
		//const size_t count = accessor.count;

		unsigned short* indices = (unsigned short*)dataPtr;
		//int faces[bufferView.byteLength / byte_stride] ;
		//numfaces = bufferView.byteLength / byte_stride;
		numfaces = accessor.count;
	}

	{
		int bot_pos = 0;
		testingStringIntMap(model.meshes[0].primitives[0].attributes, bot_pos);
		//assume 1 accessor, 1 bufferview
		const tinygltf::Accessor &accessor = model.accessors[bot_pos];
		const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
		//unsigned char to short int arr
		const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
			accessor.byteOffset;
		const int byte_stride = accessor.ByteStride(bufferView);
		//const size_t count = accessor.count;

		float* positions = (float*)dataPtr;
		//change double to fast_f for mk_bot
		//double vertices[(bufferView.byteLength / byte_stride) * 3] ;
		//numvert = (bufferView.byteLength / byte_stride) * 3;
		numvert = accessor.count * 3;
	}

	{
		//hardcoding norm to be accessor 1
		const tinygltf::Accessor &accessor = model.accessors[1];
		const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
		//unsigned char to short int arr
		const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
			accessor.byteOffset;
		const int byte_stride = accessor.ByteStride(bufferView);
		//const size_t count = accessor.count;

		float* positions = (float*)dataPtr;
		//change double to fast_f for mk_bot
		//double vertices[(bufferView.byteLength / byte_stride) * 3] ;
		//numnorm = (bufferView.byteLength / byte_stride) * 3;
		numnorm = accessor.count * 3;
	}
	return;
}

void
insert_vector_faces(const tinygltf::Model &model, double vertices[], int faces[], double normals[])
{
	{

		int indices_pos = model.meshes[0].primitives[0].indices;
		for (int i = 0; i < 3; i++)
		{
			//std::cout << model.accessors[i].bufferView << std::endl;
		}

		//assume 1 accessor, 1 bufferview
		const tinygltf::Accessor &accessor = model.accessors[indices_pos];
		const tinygltf::BufferView &bufferView = model.bufferViews[indices_pos];
		const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
		//unsigned char to short int arr
		const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
			accessor.byteOffset;
		const int byte_stride = accessor.ByteStride(bufferView);
		//const size_t count = accessor.count;

		unsigned short* indices = (unsigned short*)dataPtr;
		//int faces[bufferView.byteLength / byte_stride] ;
		//int numfaces = bufferView.byteLength / byte_stride;
		int numfaces = accessor.count;

		for (long unsigned int i = 0; i < numfaces; i++)
		{
			//std::cout << "i : " << i << " = " << indices[i] << std::endl;
			faces[i] = indices[i];

		}

		//std::cout << "Number of Faces: " << numfaces << std::endl;
	}

	{
		int bot_pos = 0;
		testingStringIntMap(model.meshes[0].primitives[0].attributes, bot_pos);

		//assume 1 accessor, 1 bufferview
		const tinygltf::Accessor &accessor = model.accessors[bot_pos];
		const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
		//unsigned char to short int arr
		const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
			accessor.byteOffset;
		const int byte_stride = accessor.ByteStride(bufferView);
		//const size_t count = accessor.count;

		float* positions = (float*)dataPtr;
		//change double to fast_f for mk_bot
		//double vertices[(bufferView.byteLength / byte_stride) * 3] ;
		//int numvert = (bufferView.byteLength / byte_stride) * 3;
		int numvert = accessor.count * 3;
		std::cout << bufferView.byteLength / byte_stride << std::endl;

		for (long unsigned int i = 0; i < accessor.count; i++)
		{
			//std::cout << "i : " << i << " = " << positions[i * 3] << " , "
			//	<< positions[(i * 3) + 1] << " , " << positions[(i * 3) + 2] << std::endl;

			vertices[i * 3] = positions[i * 3];
			vertices[(i * 3) + 1] = positions[(i * 3) + 1];
			vertices[(i * 3) + 2] = positions[(i * 3) + 2];


		}
		//std::cout << "Number of vertices: " << numvert << std::endl;
		//::cout << "Stride Count : " << byte_stride << std::endl;

	}

	{
		//hardcoding norm to be accessor 1
		const tinygltf::Accessor &accessor = model.accessors[1];
		const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
		//unsigned char to short int arr
		const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
			accessor.byteOffset;
		const int byte_stride = accessor.ByteStride(bufferView);
		//const size_t count = accessor.count;

		float* positions = (float*)dataPtr;
		//change double to fast_f for mk_bot
		//double vertices[(bufferView.byteLength / byte_stride) * 3] ;
		//int numnorm = (bufferView.byteLength / byte_stride) * 3;
		int numnorm = accessor.count * 3;

		for (long unsigned int i = 0; i < accessor.count; i++)
		{
			//std::cout << "i : " << i << " = " << positions[i * 3] << " , "
			//	<< positions[(i * 3) + 1] << " , " << positions[(i * 3) + 2] << std::endl;

			normals[i * 3] = positions[i * 3];
			normals[(i * 3) + 1] = positions[(i * 3) + 1];
			normals[(i * 3) + 2] = positions[(i * 3) + 2];


		}
		//std::cout << "Number of vertices: " << numnorm << std::endl;
		//std::cout << "Stride Count : " << byte_stride << std::endl;
	}
	return;
}


void
output_to_bot(struct rt_wdb *outfp, unsigned char bot_output_mode, char bot_orientation)
{
	
	/*
	int ret = 0;
	ret = mk_bot(outfp, //out file path
	bu_vls_addr(gfi->primitive_name), //mesh name
	ti.bot_mode, // RT_BOT_PLATE, RT_BOT_PLATE_NOCOS, RT_BOT_SOLID, RT_BOT_SURFACE
	bot_orientation, //??
	0, //?
	ti.bot_num_vertices,
	ti.bot_num_faces,
	ti.bot_vertices,
	ti.bot_faces,
	ti.bot_thickness,
	ti.bot_face_mode);*/
}

void
convert_from_gltf(struct conversion_state *state, tinygltf::Model &model)
{
	const tinygltf::Scene &scene = model.scenes[0];
	tinygltf::Node node;
	for (int i = 0; i < scene.nodes.size(); i++)
	{
		node = model.nodes[scene.nodes[i]];
		std::cout << node.children[0] << std::endl;

	}

	set_array_sizes(model, state->num_vert_values, state->num_face_values, state->num_norm_values);
	int *faces = new int[state->num_face_values];
	double *vertices = new double[state->num_vert_values * 3];
	double *normals = new double[state->num_norm_values * 3];
	insert_vector_faces(model, vertices, faces, normals);
	
	rt_wdb *outfp = state->fd_out;
	std::string title = "gltf conversion from" + state->input_file;
	mk_id(outfp, title.c_str());
	mk_bot(outfp, "Mesh", RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, state->num_vert_values / 3, state->num_face_values / 3, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);
}

HIDDEN int
gltf_read(struct gcv_context *context, const struct gcv_opts *UNUSED(gcv_options), const void *options_data, const char *source_path)
{
	/*
	bool store_original_json_for_extras_and_extensions = false;
	if (argc < 2)
	{
	printf("Needs input.gltf\n");
	exit(1);
	}
	bu_setprogname(argv[0]);
	if (argc > 2) {
	store_original_json_for_extras_and_extensions = true;
	}
	*/

	//struct conversion_state state;
	//memset(&state, 0, sizeof(state));

	//state.gltf_read_options = (struct gtlf_read_options *)options_data;
	//std::cout << "Interleaved:" << test.interleaved << std::endl;
	
	tinygltf::Model model;
	std::string err;
	std::string warn;
	std::string input_filename(source_path);
	std::string ext = GetFilePathExtension(input_filename);

	bool ret = false;

	// getting undefined reference here for the class
	tinygltf::TinyGLTF gltf_ctx;

	//gltf_ctx.SetStoreOriginalJSONForExtrasAndExtensions(
	//	store_original_json_for_extras_and_extensions);


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

	//state.gcv_options = gcv_options;
	//state.stl_read_options = (struct stl_read_options *)options_data;
	//state.id_no = state.stl_read_options->starting_id;
	state.input_file = input_filename;
	state.fd_out = context->dbip->dbi_wdbp;

	convert_from_gltf(&state, model);

	return 1;
}


HIDDEN int
gltf_can_read(const char *UNUSED(data))
{
	/* FIXME */
	printf("123ecksdee");
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