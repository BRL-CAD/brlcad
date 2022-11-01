/*                      G L T F - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tinygltf/tiny_gltf.h"

#include "vmath.h"
#include "bu/app.h"
#include "wdb.h"


static std::string
Indent(const int indent) {
    std::string s;
    for (int i = 0; i < indent; i++) {
	s += "  ";
    }

    return s;
}


static void
testingStringIntMap(const std::map<std::string, int> &m, int &pos) {
    std::map<std::string, int>::const_iterator it(m.begin());
    std::map<std::string, int>::const_iterator itEnd(m.end());
    for (; it != itEnd; it++) {
	//std::cout << Indent(indent) << it->first << ": " << it->second << std::endl;
	if(it->first =="POSITION") {
	    pos = it->second;
	    std::cout << "BOT position: " << pos << std::endl;
	}

    }
}


static void
getshapename(const tinygltf::Model &model, std::string &name)
{

    std::cout << Indent(1) << "name     : " << model.meshes[0].name << std::endl;
    std::string temp = model.meshes[0].name;
    name.append(model.meshes[0].name);


}


static void
gathershapeinfo(const tinygltf::Model &model, int &numvert, int &numfaces)
{
    //buffer if needed
    std::cout << "Mesh size: " << model.meshes.size() << std::endl;

    {
	int indices_pos = model.meshes[0].primitives[0].indices;

	//assume 1 accessor, 1 bufferview
	const tinygltf::Accessor &accessor = model.accessors[indices_pos];
	const tinygltf::BufferView &bufferView = model.bufferViews[indices_pos];
	//assume 1 accessor, 1 bufferview
	const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
	//unsigned char to short int arr
	const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
	    accessor.byteOffset;
	const int byte_stride = accessor.ByteStride(bufferView);
	//const size_t count = accessor.count;

	unsigned short* indices = (unsigned short*) dataPtr;
	int *faces = new int[bufferView.byteLength / byte_stride];
	//int faces[bufferView.byteLength / byte_stride] ;
	numfaces = (int)bufferView.byteLength / byte_stride;

	for(long unsigned int i = 0; i < bufferView.byteLength / byte_stride; i++) {
	    //std::cout << "i : " << i  << " = " << indices[i] << std::endl;
	    faces[i] = indices[i];

	}

	std::cout << "Number of Faces: " << numfaces << std::endl;
	delete[] faces;
    }

    {
	int bot_pos = 0;
	testingStringIntMap(model.meshes[0].primitives[0].attributes, bot_pos);

	//assume 1 accessor, 1 bufferview
	const tinygltf::Accessor &accessor = model.accessors[bot_pos];
	const tinygltf::BufferView &bufferView = model.bufferViews[bot_pos];
	const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
	//unsigned char to short int arr
	const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
	    accessor.byteOffset;
	const int byte_stride = accessor.ByteStride(bufferView);
	//const size_t count = accessor.count;

	float* positions = (float*) dataPtr;
	//change double to fast_f for mk_bot
	double *vertices = new double[(bufferView.byteLength / byte_stride) * 3];
	//double vertices[(bufferView.byteLength / byte_stride) * 3] ;
	numvert = (bufferView.byteLength / byte_stride) * 3;

	for(long unsigned int i = 0; i < bufferView.byteLength / byte_stride; i++) {
	    std::cout << "i : " << i  << " = " << positions[i*3] << " , "
		      << positions[(i*3) + 1] << " , " << positions[(i*3) + 2] << std::endl;

	    vertices[i*3] = positions[i*3];
	    vertices[(i*3)+1] = positions[(i*3)+1];
	    vertices[(i*3)+2] = positions[(i*3)+2];
	}

	std::cout << "Number of vertices: " << numvert << std::endl;
	std:: cout << "Stride Count : " << byte_stride << std::endl;

	delete[] vertices;
    }
    return ;

}


static void
insertvectorfaces(const tinygltf::Model &model, double vertices[], int faces[])
{
    {
	int indices_pos = model.meshes[0].primitives[0].indices;

	//assume 1 accessor, 1 bufferview
	const tinygltf::Accessor &accessor = model.accessors[indices_pos];
	const tinygltf::BufferView &bufferView = model.bufferViews[indices_pos];
	const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
	//unsigned char to short int arr
	const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
	    accessor.byteOffset;
	const int byte_stride = accessor.ByteStride(bufferView);
	//const size_t count = accessor.count;

	unsigned short* indices = (unsigned short*) dataPtr;
	//int faces[bufferView.byteLength / byte_stride] ;
	int numfaces = (int)bufferView.byteLength / byte_stride;

	for(long unsigned int i = 0; i < bufferView.byteLength / byte_stride; i++) {
	    //std::cout << "i : " << i  << " = " << indices[i] << std::endl;
	    faces[i] = indices[i];
	}

	std::cout << "Number of Faces: " << numfaces << std::endl;
    }

    {
	int bot_pos = 0;
	testingStringIntMap(model.meshes[0].primitives[0].attributes, bot_pos);

	//assume 1 accessor, 1 bufferview
	const tinygltf::Accessor &accessor = model.accessors[bot_pos];
	const tinygltf::BufferView &bufferView = model.bufferViews[bot_pos];
	const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
	//unsigned char to short int arr
	const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
	    accessor.byteOffset;
	const int byte_stride = accessor.ByteStride(bufferView);
	//const size_t count = accessor.count;

	float* positions = (float*) dataPtr;
	//change double to fast_f for mk_bot
	//double vertices[(bufferView.byteLength / byte_stride) * 3] ;
	int numvert = ((int)bufferView.byteLength / byte_stride) * 3;

	for(long unsigned int i = 0; i < bufferView.byteLength / byte_stride; i++) {
	    std::cout << "i : " << i  << " = " << positions[i*3] << " , "
		      << positions[(i*3) + 1] << " , " << positions[(i*3) + 2] << std::endl;

	    vertices[i*3] = positions[i*3];
	    vertices[(i*3)+1] = positions[(i*3)+1];
	    vertices[(i*3)+2] = positions[(i*3)+2];
	}

	std::cout << "Number of vertices: " << numvert << std::endl;
	std:: cout << "Stride Count : " << byte_stride << std::endl;

    }
    return ;

}


static std::string GetFilePathExtension(const std::string &FileName)
{
    if (FileName.find_last_of(".") != std::string::npos) {
	return FileName.substr(FileName.find_last_of(".") + 1);
    }
    return "";
}


int main(int argc, char **argv)
{
    bool store_original_json_for_extras_and_extensions = false;

    if (argc < 2) {
	printf("Needs input.gltf\n");
	exit(1);
    }

    bu_setprogname(argv[0]);

    if (argc > 2) {
	store_original_json_for_extras_and_extensions = true;
    }

    tinygltf::Model model;
    std::string err;
    std::string warn;
    std::string input_filename(argv[1]);
    std::string ext = GetFilePathExtension(input_filename);


    bool ret = false;

    // getting undefined reference here for the class
    tinygltf::TinyGLTF gltf_ctx;

    gltf_ctx.SetStoreOriginalJSONForExtrasAndExtensions(
	store_original_json_for_extras_and_extensions);


    if (ext.compare("glb") == 0) {
	std::cout << "File type: binary glTF" << std::endl;
	// assume binary glTF.
	ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn,
					  input_filename);
    } else {
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


    std::string shape_name;

    getshapename(model, shape_name);

    std::cout << "Shape name: " << shape_name << std::endl;

    //array of vectors, edges, faces

    int num_vert_values;
    int num_face_values;

    gathershapeinfo(model, num_vert_values, num_face_values);

    std::cout << "Num vert : " << num_vert_values / 3 << std::endl;
    std::cout << "Num faces: " << num_face_values / 3 << std::endl;

    int *faces = new int[(size_t)num_face_values*3];
    double *vertices = new double[(size_t)num_vert_values*3];
    //int faces[num_faces];
    //double vertices[num_vert_values];

    insertvectorfaces(model, vertices, faces);

    for(int i = 0; i < num_vert_values; i = i + 3) {
	//std::cout << vertices[i] << ", " << vertices[i+1] << ", " << vertices[i+2] << std::endl;
    }

    for(int i = 0; i < num_face_values; i = i + 3) {
	//std::cout << faces[i] << ", " << faces[i+1] << ", " << faces[i+2] << std::endl;
    }

    // mk_Bot needs file, name, num vert, num face, verts, faces
    struct rt_wdb *outfp = NULL;

    //need to fix this. replace test.g with new
    std::string gfile;
    gfile = shape_name + ".g";

    outfp = wdb_fopen(gfile.c_str());

    std::string title = "gltf conversion from" + input_filename;

    mk_id(outfp, title.c_str());

    mk_bot(outfp, shape_name.c_str(), RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, num_vert_values / 3, num_face_values / 3, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);

    wdb_close(outfp);

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
