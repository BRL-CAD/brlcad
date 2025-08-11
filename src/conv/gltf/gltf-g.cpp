/*                      G L T F - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2025 United States Government as represented by
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

#include "tiny_gltf.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bg/trimesh.h"
#include "wdb.h"

static void
process_mesh(struct rt_wdb *outfp, const tinygltf::Model &model, const tinygltf::Mesh &mesh, int verbosity, size_t cnt)
{
    int numverts = 0;
    int numfaces = 0;

    std::string shape_name = mesh.name;
    if (!shape_name.length())
	shape_name = std::string("gltf_import_") + std::to_string(cnt);

    if (verbosity)
	std::cout << "BoT " << cnt << " name: " << shape_name << "\n";

    // Iterate over mesh primitives, count vertices and triangles
    for (size_t i = 0; i < mesh.primitives.size(); i++) {
	const tinygltf::Primitive &p = mesh.primitives[i];

	// Triangle faces
	{
	    const tinygltf::Accessor &ta = model.accessors[p.indices];
	    const tinygltf::BufferView &tv = model.bufferViews[ta.bufferView];
	    int byte_stride = (tv.byteStride > 1) ? tv.byteStride : 0;

	    switch (ta.componentType) {
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		    byte_stride = (!byte_stride) ? sizeof(short) * 3 : byte_stride;
		    numfaces += tv.byteLength / byte_stride;
		    break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		    byte_stride = (!byte_stride) ? sizeof(unsigned short) * 3 : byte_stride;
		    numfaces += tv.byteLength / byte_stride;
		    break;
		case TINYGLTF_COMPONENT_TYPE_INT:
		    byte_stride = (!byte_stride) ? sizeof(int) * 3 : byte_stride;
		    numfaces += tv.byteLength / byte_stride;
		    break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		    byte_stride = (!byte_stride) ? sizeof(unsigned int) * 3 : byte_stride;
		    numfaces += tv.byteLength / byte_stride;
		    break;
		default:
		    bu_log("Warning:  unsupported triangle indices type: %d\n", ta.componentType);
		    continue;
	    }
	}

	// Vertices
	{
	    std::map<std::string, int>::const_iterator p_it = p.attributes.find(std::string("POSITION"));
	    if (p_it == p.attributes.end()) {
		bu_log("Error - could not find POSITION attribute for mesh %zd\n", i);
		continue;
	    }
	    const tinygltf::Accessor &va = model.accessors[p_it->second];
	    const tinygltf::BufferView &vv = model.bufferViews[va.bufferView];
	    int byte_stride = (vv.byteStride > 1) ? vv.byteStride : 0;

	    switch (va.componentType) {
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
		    byte_stride = (!byte_stride) ? sizeof(float) * 3 : byte_stride;
		    numverts += vv.byteLength / byte_stride;
		    break;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		    byte_stride = (!byte_stride) ? sizeof(double) * 3 : byte_stride;
		    numverts += vv.byteLength / byte_stride;
		    break;
		default:
		    bu_log("Warning - unsupported vertex type: %d\n", va.componentType);
		    continue;
	    }
	}
    }

    // We now know how many faces and vertices are in the mesh - allocate new arrays
    int *o_faces = (int *)bu_calloc(numfaces, sizeof(int) * 3, "faces");
    fastf_t *o_vertices = (fastf_t *)bu_calloc(numverts, sizeof(fastf_t) * 3, "verts");

    // Iterate over vertices, building a mapping (we may be combining multiple primitives
    // into one BoT, so our indexing may change.)
    std::unordered_map<int, std::unordered_map<unsigned int, unsigned int>> prim_vertmap;

    // Iterate again, transcribing information into vertex array and building map.
    int vcnt = 0;
    for (size_t i = 0; i < mesh.primitives.size(); i++) {
	const tinygltf::Primitive &p = mesh.primitives[i];

	std::map<std::string, int>::const_iterator p_it = p.attributes.find(std::string("POSITION"));
	if (p_it == p.attributes.end()) {
	    bu_log("Error - could not find POSITION attribute for mesh %zd\n", i);
	    continue;
	}
	const tinygltf::Accessor &va = model.accessors[p_it->second];
	const tinygltf::BufferView &vv = model.bufferViews[va.bufferView];
	const tinygltf::Buffer &buffer = model.buffers[vv.buffer];
	int byte_stride = (vv.byteStride > 1) ? vv.byteStride : 0;

	switch (va.componentType) {
	    case TINYGLTF_COMPONENT_TYPE_FLOAT:
		byte_stride = (!byte_stride) ? sizeof(float) * 3 : byte_stride;
		if (verbosity > 1)
		    std::cout << "byte stride: " << byte_stride << "\n";
		numverts = vv.byteLength / byte_stride;
		if (verbosity)
		    std::cout << "Vertex count: " << numverts << "\n";
		{
		    float *verts = (float *)(buffer.data.data() + vv.byteOffset + va.byteOffset);
		    for (int j = 0; j < numverts; j++) {
			if (verbosity > 1)
			    bu_log("V: %g %g %g\n", verts[3*j+0], verts[3*j+1], verts[3*j+2]);
			o_vertices[3*vcnt + 0] = verts[3*j+0];
			o_vertices[3*vcnt + 1] = verts[3*j+1];
			o_vertices[3*vcnt + 2] = verts[3*j+2];
			prim_vertmap[i][j] = vcnt;
			vcnt++;
		    }
		}
		break;
	    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		byte_stride = (!byte_stride) ? sizeof(double) * 3 : byte_stride;
		if (verbosity > 1)
		    std::cout << "byte stride: " << byte_stride << "\n";
		numverts = vv.byteLength / byte_stride;
		if (verbosity)
		    std::cout << "Vertex count: " << numverts << "\n";
		{
		    double *verts = (double *)(buffer.data.data() + vv.byteOffset + va.byteOffset);
		    for (int j = 0; j < numverts; j++) {
			if (verbosity > 1)
			    bu_log("V: %g %g %g\n", verts[3*j+0], verts[3*j+1], verts[3*j+2]);
			o_vertices[3*vcnt + 0] = verts[3*j+0];
			o_vertices[3*vcnt + 1] = verts[3*j+1];
			o_vertices[3*vcnt + 2] = verts[3*j+2];
			prim_vertmap[i][j] = vcnt;
			vcnt++;
		    }
		}
		break;
	    default:
		bu_log("Warning - unsupported vertex type: %d\n", va.componentType);
		continue;
	}
    }

    // Iterate a final time, transcribing information into face arrays.
    int fcnt = 0;
    for (size_t i = 0; i < mesh.primitives.size(); i++) {
	const tinygltf::Primitive &p = mesh.primitives[i];

	// Triangle faces
	{
	    const tinygltf::Accessor &ta = model.accessors[p.indices];
	    const tinygltf::BufferView &tv = model.bufferViews[ta.bufferView];
	    const tinygltf::Buffer &buffer = model.buffers[tv.buffer];
	    int byte_stride = (tv.byteStride > 1) ? tv.byteStride : 0;

	    switch (ta.componentType) {
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		    byte_stride = (!byte_stride) ? sizeof(short) * 3 : byte_stride;
		    if (verbosity > 1)
			std::cout << "byte stride: " << byte_stride << "\n";
		    numfaces = tv.byteLength / byte_stride;
		    if (verbosity)
			std::cout << "Face count: " << numfaces << "\n";
		    {
			short *faces = (short *)(buffer.data.data() + tv.byteOffset + ta.byteOffset);
			for (int j = 0; j < numfaces; j++) {
			    if (verbosity > 1)
				bu_log("F: %d %d %d\n", faces[3*j+0], faces[3*j+1], faces[3*j+2]);
			    o_faces[3*fcnt + 0] = prim_vertmap[i][faces[3*j+0]];
			    o_faces[3*fcnt + 1] = prim_vertmap[i][faces[3*j+1]];
			    o_faces[3*fcnt + 2] = prim_vertmap[i][faces[3*j+2]];
			    fcnt++;
			}
		    }
		    break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		    byte_stride = (!byte_stride) ? sizeof(unsigned short) * 3 : byte_stride;
		    if (verbosity > 1)
			std::cout << "byte stride: " << byte_stride << "\n";
		    numfaces = tv.byteLength / byte_stride;
		    if (verbosity)
			std::cout << "Face count: " << numfaces << "\n";
		    {
			unsigned short *faces = (unsigned short *)(buffer.data.data() + tv.byteOffset + ta.byteOffset);
			for (int j = 0; j < numfaces; j++) {
			    if (verbosity > 1)
				bu_log("F: %d %d %d\n", faces[3*j+0], faces[3*j+1], faces[3*j+2]);
			    o_faces[3*fcnt + 0] = prim_vertmap[i][faces[3*j+0]];
			    o_faces[3*fcnt + 1] = prim_vertmap[i][faces[3*j+1]];
			    o_faces[3*fcnt + 2] = prim_vertmap[i][faces[3*j+2]];
			    fcnt++;
			}
		    }
		    break;
		case TINYGLTF_COMPONENT_TYPE_INT:
		    byte_stride = (!byte_stride) ? sizeof(int) * 3 : byte_stride;
		    if (verbosity > 1)
			std::cout << "byte stride: " << byte_stride << "\n";
		    numfaces = tv.byteLength / byte_stride;
		    if (verbosity)
			std::cout << "Face count: " << numfaces << "\n";
		    {
			int *faces = (int *)(buffer.data.data() + tv.byteOffset + ta.byteOffset);
			for (int j = 0; j < numfaces; j++) {
			    if (verbosity > 1)
				bu_log("F: %d %d %d\n", faces[3*j+0], faces[3*j+1], faces[3*j+2]);
			    o_faces[3*fcnt + 0] = prim_vertmap[i][faces[3*j+0]];
			    o_faces[3*fcnt + 1] = prim_vertmap[i][faces[3*j+1]];
			    o_faces[3*fcnt + 2] = prim_vertmap[i][faces[3*j+2]];
			    fcnt++;
			}
		    }
		    break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		    byte_stride = (!byte_stride) ? sizeof(unsigned int) * 3 : byte_stride;
		    if (verbosity > 1)
			std::cout << "byte stride: " << byte_stride << "\n";
		    numfaces = tv.byteLength / byte_stride;
		    if (verbosity)
			std::cout << "Face count: " << numfaces << "\n";
		    {
			unsigned int *faces = (unsigned int *)(buffer.data.data() + tv.byteOffset + ta.byteOffset);
			for (int j = 0; j < numfaces; j++) {
			    if (verbosity > 1)
				bu_log("F: %d %d %d\n", faces[3*j+0], faces[3*j+1], faces[3*j+2]);
			    o_faces[3*fcnt + 0] = prim_vertmap[i][faces[3*j+0]];
			    o_faces[3*fcnt + 1] = prim_vertmap[i][faces[3*j+1]];
			    o_faces[3*fcnt + 2] = prim_vertmap[i][faces[3*j+2]];
			    fcnt++;
			}
		    }
		    break;
		default:
		    bu_log("Warning - unsupported triangle indices type: %d\n", ta.componentType);
		    continue;
	    }
	}
    }

    int type = RT_BOT_SURFACE;
    if (bg_trimesh_manifold_closed(numverts, numfaces, o_vertices, o_faces))
	type = RT_BOT_SOLID;

    mk_bot(outfp, shape_name.c_str(), type, RT_BOT_CCW, 0, numverts, numfaces, o_vertices, o_faces, NULL, NULL);
}

int main(int argc, char **argv)
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

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* Parse options */
    int opt_ret = bu_opt_parse(NULL, argc, (const char**)argv, d);

    if (print_help) {
	char* help = bu_opt_describe(d, NULL);
	bu_log("%s\nOptions:\n%s", usage, help);
	if (help)
	    bu_free(help, "help str");
	bu_vls_free(&output_path);
	return 1;
    }

    /* See what is left */
    argc = opt_ret;
    if (argc < 2 && !bu_vls_strlen(&output_path))
	bu_exit(BRLCAD_ERROR, "Need input glTF filename\n");
    if (argc > 2 || (argc == 2 && bu_vls_strlen(&output_path)))
	bu_exit(BRLCAD_ERROR, "Multiple inputs specified.\n");
    if (argc == 2)
	bu_vls_sprintf(&output_path, "%s", argv[1]);

    /* Start reading */
    tinygltf::Model model;
    std::string err;
    std::string warn;
    std::string input_filename(argv[0]);

    bool read_ascii = true;
    struct bu_vls ext = BU_VLS_INIT_ZERO;
    if (bu_path_component(&ext, argv[0], BU_PATH_EXT))
	if (BU_STR_EQUAL(bu_vls_cstr(&ext), "glb"))
	    read_ascii = false;

    tinygltf::TinyGLTF gltf_ctx;
    gltf_ctx.SetStoreOriginalJSONForExtrasAndExtensions(extensions);


    bool ret = false;
    if (read_ascii) {
	if (verbosity)
	    std::cout << "Reading as ascii glTF\n";
	ret = gltf_ctx.LoadASCIIFromFile(&model, &err, &warn, input_filename);
    } else {
	if (verbosity)
	    std::cout << "Reading as binary glTF (glb)\n";
	ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn, input_filename);
    }

    if (verbosity && !warn.empty())
	std::cerr << "Warn:" << warn.c_str() << "\n";

    if (!err.empty())
	std::cerr << "Error:" << err.c_str() << "\n";

    if (!ret) {
	std::cerr << "Failed to parse glTF\n";
	return BRLCAD_ERROR;
    }


    // Write out to .g file
    // TODO - check if pre-existing output is present
    struct rt_wdb *outfp = wdb_fopen(bu_vls_cstr(&output_path));
    std::string title = "gltf-g import of " + input_filename;
    mk_id(outfp, title.c_str());

    // Iterate over meshes.  TODO - we could probably interpret
    // nodes as comb instances...
    std::cout << "Mesh count: " << model.meshes.size() << "\n";
    for (size_t i = 0; i < model.meshes.size(); i++) {
	tinygltf::Mesh &mesh = model.meshes[i];
	process_mesh(outfp, model, mesh, verbosity, i);
    }

    db_close(outfp->dbip);

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
