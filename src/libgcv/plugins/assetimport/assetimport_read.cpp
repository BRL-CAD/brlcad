/*               A S S E T I M P O R T _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2023 United States Government as represented by
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
/** @file assetimport_read.cpp
 *
 * GCV logic for importing geometry using the Open Asset Import Library:
 * https://github.com/assimp/assimp 
 *
 */

#include "common.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "gcv/api.h"
#include "wdb.h"
#include "bg/trimesh.h"

/* assimp headers */
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

typedef struct shader_properties {
    char *name;		                            /* shader name, or NULL */
    char *args;		                            /* shader args, or NULL */
    unsigned char* rgb; 			    /* NULL => no color */
    shader_properties() {
	name = NULL;
	args = NULL;
	rgb = NULL;
    }
    ~shader_properties() {
	delete[] name;
	delete[] args;
	delete[] rgb;
    }
} shader_properties_t;

typedef struct assetimport_read_options {
    int starting_id;                                /* starting ident id number */
    int const_id;                                   /* Constant ident number (assigned to all regions if non-negative) */
    int mat_code;                                   /* default material code */
    int verbose;                                    /* verbose flag */
    char* format;                                   /* output file format */
} assetimport_read_options_t;

typedef struct assetimport_read_state {
    const struct gcv_opts* gcv_options;             /* global options */
    assetimport_read_options_t* assetimport_read_options;     /* internal options */

    std::string input_file;	                    /* name of the input file */
    struct rt_wdb* fd_out;	                    /* Resulting BRL-CAD file */
    struct wmember all;                             /* scene root */

    const aiScene* scene;                           /* assetimport generated scene */
    int id_no;                                      /* region ident number */
    unsigned int dfs;                               /* number of nodes visited */
    unsigned int converted;                         /* number of meshes converted */
    assetimport_read_state() {
	gcv_options = NULL;
	assetimport_read_options = NULL;
	input_file = "";
	fd_out = NULL;
	all = WMEMBER_INIT_ZERO;
	scene = NULL;
	id_no = 0;
	dfs = 0;
	converted = 0;
    }
    ~assetimport_read_state() {}
} assetimport_read_state_t;

static void
aimatrix_to_arr16(aiMatrix4x4 aimat, fastf_t* ret)
{
    ret[0 ] = aimat.a1;
    ret[1 ] = aimat.a2;
    ret[2 ] = aimat.a3;
    ret[3 ] = aimat.a4;
    ret[4 ] = aimat.b1;
    ret[5 ] = aimat.b2;
    ret[6 ] = aimat.b3;
    ret[7 ] = aimat.b4;
    ret[8 ] = aimat.c1;
    ret[9 ] = aimat.c2;
    ret[10] = aimat.c3;
    ret[11] = aimat.c4;
    ret[12] = aimat.d1;
    ret[13] = aimat.d2;
    ret[14] = aimat.d3;
    ret[15] = aimat.d4;
}

/* known shader names */
std::unordered_set<std::string>brlcad_shaders { "bump", "bwtexture", "camo", "checker", 
                                                "cloud", "envmap", "fbmbump", "fire", 
                                                "glass", "gravel", "light", "marble", 
                                                "mirror", "plastic", "rtrans", "scloud", 
                                                "spm", "stack", "stxt", "texture", "turbump", 
                                                "wood" };

/* checks if in_name exists as a known shader keyword
 * if it is, we assume any remaining string after a space are shader args
 *
 * returns 1 if shader properties are found and set in ret
 * returns 0 otherwise
 */
static int
check_brlcad_shader(std::string in_name, shader_properties_t* ret)
{
    std::string name, args = "";
    size_t space = in_name.find(" ");

    /* for brlcad shaders a space means we have additional shader params
     * in the form "shader_name param1=x .."
     */
    if (space != std::string::npos) {
        name = in_name.substr(0, space);
        args = in_name.substr(space+1);
    } else {
        name = in_name;
    }

    if (brlcad_shaders.find(name) != brlcad_shaders.end()) {
        ret->name = new char[name.size() + 1];
        bu_strlcpy(ret->name, name.c_str(), name.size() + 1);
        if (args != "") {
            ret->args = new char[args.size() + 1];
            bu_strlcpy(ret->args, args.c_str(), args.size() + 1);
        }
        return 1;
    }

    return 0;
}

static shader_properties_t*
generate_shader(assetimport_read_state_t* pstate, unsigned int mesh_idx)
{
    aiColor3D* mesh_color = (aiColor3D*)pstate->scene->mMeshes[mesh_idx]->mColors[0];
    if (!pstate->scene->HasMaterials() && !mesh_color)
	return NULL;

    shader_properties_t* ret = new shader_properties_t();

    /* check for material data */
    if (pstate->scene->HasMaterials()) {
	unsigned int mat_idx = pstate->scene->mMeshes[mesh_idx]->mMaterialIndex;
	aiMaterial* mat = pstate->scene->mMaterials[mat_idx];

        /* when shader names are written from a .g we use the raw name+args as the shader name
         * otherwise try to build up the shader manually 
         */
        if (!check_brlcad_shader(mat->GetName().data, ret)) {
	    std::string name = "plastic";
	    ret->name = new char[name.size() + 1];
	    bu_strlcpy(ret->name, name.c_str(), name.size() + 1);

	    /* brlcad 'plastic' shader defaults */
	    float tr = 0.0;		/* transparency */
	    float re = 0.0;		/* mirror reflectance */
	    float sp = 0.7;		/* specular reflectivity */
	    float di = 0.3;		/* diffuse reflectivity */
	    float ri = 1.0;		/* refractive index */
	    float ex = 0.0;		/* extinction */
	    float sh = 10.0;	        /* shininess */
	    float em = 0.0;		/* emission */

	    /* gets value if key exists in material, otherwise leaves default */
            /* NOTE: assetimport seems to favor MATKEY_OPACITY over MATKEY_TRANSPARENCYFACTOR
             * so we use 1-opacity for transparency
             */
	    mat->Get(AI_MATKEY_OPACITY, tr);
	    mat->Get(AI_MATKEY_REFLECTIVITY, re);
	    mat->Get(AI_MATKEY_SPECULAR_FACTOR, sp);
	    mat->Get(AI_MATKEY_SHININESS_STRENGTH, di);
	    mat->Get(AI_MATKEY_REFRACTI, ri);
	    mat->Get(AI_MATKEY_ANISOTROPY_FACTOR, ex);
	    mat->Get(AI_MATKEY_SHININESS, sh);
	    mat->Get(AI_MATKEY_EMISSIVE_INTENSITY, em);

	    /* format values into args string */
	    std::string args =
	        "{ tr " + std::to_string(1-tr) +
	         " re " + std::to_string(re) +
	         " sp " + std::to_string(sp) +
	         " di " + std::to_string(di) +
	         " ri " + std::to_string(ri) +
	         " ex " + std::to_string(ex) +
	         " sh " + std::to_string(sh) +
	         " em " + std::to_string(em) +
	         " }";
	    ret->args = new char[args.size() + 1];
	    bu_strlcpy(ret->args, args.c_str(), args.size() + 1);
        }

	/* check for vertex colors, otherwise try to use diffuse color */
        if (!mesh_color) {
            aiColor3D diff(1);	/* diffuse color -> defaults to white */
            mat->Get(AI_MATKEY_COLOR_DIFFUSE, diff);
            mesh_color = &diff;
        }
    }

    /* set the color of the face using the first vertex color data we
     * find if such exists. If not, try to use the material diffuse color.
     * NOTE: we make two assumptions when using the vertex[0] color:
     * 1) each vertex only has one set of colors
     * 2) each vertex in the triangle is the same color
     */
    if (mesh_color) {
	ret->rgb = new unsigned char[3];
	ret->rgb[0] = (unsigned char)(mesh_color->r * 255);
	ret->rgb[1] = (unsigned char)(mesh_color->g * 255);
	ret->rgb[2] = (unsigned char)(mesh_color->b * 255);
	if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose)
	    bu_log("color data (%d, %d, %d)\n", (int)ret->rgb[0], (int)ret->rgb[1], (int)ret->rgb[2]);
    }

    return ret;
}

static std::string
generate_unique_name(const char* curr_name, const char* def_name, bool is_mesh)
{
    static std::unordered_map<std::string, int>used_names; /* used names in db */

    std::string name(curr_name);
    std::string suffix = is_mesh ? ".s" : ".r";

    /* get last instance of '/' and keep everything after it */
    const char* trim = strrchr(curr_name, '/');
    if (trim)
	name = ++trim;

    /* strip .r at end if it exists */
    size_t dotr = name.find(".r\0");
    if (dotr != std::string::npos)
        name = name.substr(0, dotr);

    /* if curr_name is empty, give a generic name */
    if (!name.size())
	name = def_name;

    /* cleanup name - remove spaces, slashes and non-standard characters */
    bu_vls scrub = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&scrub, "%s", name.c_str());
    bu_vls_simplify(&scrub, nullptr, " _\0", " _\0");
    name = scrub.vls_len > 0 ? std::string(bu_vls_cstr(&scrub)) : def_name;   /* somemtimes we scrub out the entire name */

    /* check for name collisions */
    auto handle = used_names.find(name);
    if (handle != used_names.end()) {
        while (used_names.find(name + "_" + std::to_string(++handle->second)) != used_names.end())
            ;
        name.append("_" + std::to_string(handle->second));
    }

    used_names.emplace(name, 0);
    name.append(suffix);

    return name;
}

static void
generate_geometry(assetimport_read_state_t* pstate, wmember &region, unsigned int mesh_idx)
{
    aiMesh* mesh = pstate->scene->mMeshes[mesh_idx];
    /* sanity check: importer handles triangulation but make sure
       we are dealing with only triangles */
    if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) {
	bu_log("WARNING: unknown primitive in mesh[%d] -- skipping\n", mesh_idx);
	return;
    }

    int* faces = new int[mesh->mNumFaces * 3];
    double* vertices = new double[mesh->mNumVertices * 3];
    fastf_t* normals = NULL;
    fastf_t* thickness = NULL;
    bu_bitv* bitv = NULL;

    if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose) {
	bu_log("mesh[%d] num Faces: %d\n", mesh_idx, mesh->mNumFaces);
	bu_log("mesh[%d] num vertices: %d\n", mesh_idx, mesh->mNumVertices);
    }

    /* add all faces */
    for (size_t i = 0; i < mesh->mNumFaces; i++) {
	faces[i * 3   ] = mesh->mFaces[i].mIndices[0];
	faces[i * 3 +1] = mesh->mFaces[i].mIndices[1];
	faces[i * 3 +2] = mesh->mFaces[i].mIndices[2];
    }

    /* add all vertices */
    for (size_t i = 0; i < mesh->mNumVertices; i++) {
	vertices[i * 3   ] = mesh->mVertices[i].x * pstate->gcv_options->scale_factor;
	vertices[i * 3 +1] = mesh->mVertices[i].y * pstate->gcv_options->scale_factor;
	vertices[i * 3 +2] = mesh->mVertices[i].z * pstate->gcv_options->scale_factor;
    }

    /* check bot mode */
    unsigned char orientation = RT_BOT_CCW; /* default ccw https://assimp.sourceforge.net/lib_html/data.html*/
    unsigned char mode = RT_BOT_SURFACE;
    {
	rt_bot_internal bot;
	std::memset(&bot, 0, sizeof(bot));
	bot.magic = RT_BOT_INTERNAL_MAGIC;
	bot.orientation = orientation;
	bot.num_vertices = mesh->mNumVertices;
	bot.num_faces = mesh->mNumFaces;
	bot.vertices = vertices;
	bot.faces = faces;
	/* TODO this generates a lot of false plate modes */
	mode = bg_trimesh_solid((int)bot.num_vertices, (int)bot.num_faces, bot.vertices, bot.faces, NULL) ? RT_BOT_PLATE : RT_BOT_SOLID;
    }

    if (mode == RT_BOT_PLATE) {
	const fastf_t plate_thickness = 1.0;
	bitv = bu_bitv_new(mesh->mNumFaces * 3);
	thickness = new fastf_t[mesh->mNumFaces * 3] {plate_thickness};
    }

    /* add mesh to region list */
    std::string mesh_name = generate_unique_name(pstate->scene->mMeshes[mesh_idx]->mName.data, pstate->gcv_options->default_name, 1);
    /* check for normals */
    if (mesh->HasNormals()) {
	normals = new fastf_t[mesh->mNumVertices * 3];
	for (size_t i = 0; i < mesh->mNumVertices; i++) {
	    aiVector3D normalized = mesh->mNormals[i].Normalize();
	    normals[i * 3   ] = normalized.x;
	    normals[i * 3 +1] = normalized.y;
	    normals[i * 3 +2] = normalized.z;
	}

	mk_bot_w_normals(pstate->fd_out, mesh_name.c_str(), mode, orientation, 0, mesh->mNumVertices, mesh->mNumFaces, vertices, faces, thickness, bitv, mesh->mNumVertices, normals, faces);
    } else {
	mk_bot(pstate->fd_out, mesh_name.c_str(), mode, orientation, 0, mesh->mNumVertices, mesh->mNumFaces, vertices, faces, thickness, bitv);
    }
    (void)mk_addmember(mesh_name.c_str(), &region.l, NULL, WMOP_UNION);

    /* book keeping to log converted meshes */
    pstate->converted++;

    /* cleanup memory */
    delete[] faces;
    delete[] vertices;
    delete[] normals;
    delete[] thickness;
    if (bitv)
        bu_bitv_free(bitv);
}

static void
handle_node(assetimport_read_state_t* pstate, aiNode* curr, struct wmember &regions)
{
    shader_properties_t *shader_prop = NULL;
    std::string region_name = generate_unique_name(curr->mName.data, pstate->gcv_options->default_name, 0);

    if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose) {
	bu_log("\nCurr node name | dfs index: %s | %d\n", curr->mName.data, pstate->dfs);
	bu_log("Curr node transformation:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n", 
		curr->mTransformation.a1, curr->mTransformation.a2, curr->mTransformation.a3, curr->mTransformation.a4,
		curr->mTransformation.b1, curr->mTransformation.b2, curr->mTransformation.b3, curr->mTransformation.b4,
		curr->mTransformation.c1, curr->mTransformation.c2, curr->mTransformation.c3, curr->mTransformation.c4,
		curr->mTransformation.d1, curr->mTransformation.d2, curr->mTransformation.d3, curr->mTransformation.d4);
	bu_log("Curr node children: %d\n", curr->mNumChildren);
	bu_log("Curr node meshes: %d\n", curr->mNumMeshes);
	bu_log("Region name: %s\n", region_name.c_str());
	if (!curr->mNumMeshes)
	    bu_log("\n");
    }

    /* book keeping for dfs node traversal */
    pstate->dfs++;

    /* generate a list to hold this node's meshes */
    struct wmember mesh;
    BU_LIST_INIT(&mesh.l);

    /* handle the current nodes meshes if any */
    for (size_t i = 0; i < curr->mNumMeshes; i++) {
	unsigned int mesh_idx = curr->mMeshes[i];\
				if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose)
				    bu_log("      uses mesh %d\n", mesh_idx);

	generate_geometry(pstate, mesh, mesh_idx);

	/* FIXME generate shader and color 
	 * this assumes all mesh under a region are using the same material data.
	 * if they're not, we're just using the last one
	 */
	if (shader_prop)
	    delete shader_prop;
	shader_prop = generate_shader(pstate, mesh_idx);
    }
    if (curr->mNumMeshes) {
	/* apply parent transformations */
	fastf_t tra[16];
	aimatrix_to_arr16(curr->mTransformation, tra);

	/* make region with all meshes */
	mk_lrcomb(pstate->fd_out, region_name.c_str(), &mesh, 1, shader_prop->name, shader_prop->args, shader_prop->rgb, pstate->id_no, 0, pstate->assetimport_read_options->mat_code, 100, 0);
	(void)mk_addmember(region_name.c_str(), &mesh.l, tra, WMOP_UNION);

	if (!pstate->assetimport_read_options->const_id)
	    pstate->id_no++;
    }

    /* when we're done adding children, add to top level */
    if (!curr->mNumChildren) {
	/* apply child's transformations */
	fastf_t tra[16];
	aimatrix_to_arr16(curr->mTransformation, tra);
	(void)mk_addmember(region_name.c_str(), &regions.l, tra, WMOP_UNION);
    }

    /* recursive call all children */
    for (size_t i = 0; i < curr->mNumChildren; i++) {
	/* skip children with no meshes under them (cameras, etc) */
	if (curr->mChildren[i]->mNumChildren || curr->mChildren[i]->mNumMeshes)
	    handle_node(pstate, curr->mChildren[i], regions);
    }

    if (shader_prop)
	delete shader_prop;
}

static int
convert_input(assetimport_read_state_t* pstate)
{
    /* have importer remove points and lines as we can't do anything with them */
    aiPropertyStore* props = aiCreatePropertyStore();
    aiSetImportPropertyInteger(props, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
    /* we are taking one of the postprocessing presets to have
     * max quality with reasonable render times. But, we must keep 
     * seemingly redundant materials as we use the names for BRLCAD shaders
     */
    unsigned int import_flags = aiProcessPreset_TargetRealtime_MaxQuality & ~aiProcess_RemoveRedundantMaterials;
    pstate->scene = aiImportFileExWithProperties(pstate->input_file.c_str(),
						 import_flags,
						 NULL,
						 props);
    
    if (!pstate->scene) {
	bu_log("ERROR: bad scene conversion\n");
	return 0;
    }

    if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose) {
	bu_log("Scene num meshes: %d\n", pstate->scene->mNumMeshes);
	bu_log("Scene num materials: %d\n", pstate->scene->mNumMaterials);
	bu_log("Scene num Animations: %d\n", pstate->scene->mNumAnimations);
	bu_log("Scene num Textures: %d\n", pstate->scene->mNumTextures);
	bu_log("Scene num Lights: %d\n", pstate->scene->mNumLights);
	bu_log("Scene num Cameras: %d\n\n", pstate->scene->mNumCameras);
    }

    /* create around the root node */
    BU_LIST_INIT(&pstate->all.l);
    if (pstate->gcv_options->verbosity_level || pstate->assetimport_read_options->verbose)
	bu_log("-- root node --\n");
    handle_node(pstate, &pstate->scene->mRootNode[0], pstate->all);

    /* make a top level 'all.g' */
    mk_lcomb(pstate->fd_out, "all.g", &pstate->all, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);

    /* import handles triangulation and scrubs extra shapes - this should *in theory* always report 100% */
    bu_log("Converted ( %d / %d ) meshes ... %.2f%%\n", pstate->converted, pstate->scene->mNumMeshes, (float)pstate->converted / (float)pstate->scene->mNumMeshes * 100.0);

    /* cleanup */
    aiReleaseImport(pstate->scene);
    aiReleasePropertyStore(props);

    return 1;
}

static void
assetimport_read_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    assetimport_read_options_t* options_data;

    BU_ALLOC(options_data, assetimport_read_options_t);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(6 * sizeof(struct bu_opt_desc), "options_desc");

    options_data->starting_id = 1000;
    options_data->const_id = 0;
    options_data->mat_code = 1;
    options_data->verbose = 0;
    options_data->format = NULL;

    BU_OPT((*options_desc)[0], NULL, "starting-ident", "number", bu_opt_int, options_data, "specify the starting ident for the regions created");
    BU_OPT((*options_desc)[1], NULL, "constant-ident", NULL, NULL, &options_data->const_id, "specify that the starting ident should remain constant");
    BU_OPT((*options_desc)[2], NULL, "material", "number", bu_opt_int, &options_data->mat_code, "specify the material code that will be assigned to created regions");
    BU_OPT((*options_desc)[3], "v", "verbose", NULL, NULL, &options_data->verbose, "specify for verbose output");
    BU_OPT((*options_desc)[4], NULL, "format", NULL, bu_opt_str, &options_data->format, "specify the output file format");
    BU_OPT_NULL((*options_desc)[5]);
}

static void
assetimport_read_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

static int
assetimport_read(struct gcv_context *context, const struct gcv_opts* gcv_options, const void *options_data, const char *source_path)
{
    assetimport_read_state_t state;

    state.gcv_options = gcv_options;
    state.assetimport_read_options = (assetimport_read_options_t*)options_data;
    state.id_no = state.assetimport_read_options->starting_id;
    state.input_file = source_path;
    struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    state.fd_out = wdbp;

    /* check and validate the specied input file type against ai
     * checks using file extension if no --format is supplied
     * this is likely all a 'can_read' function would need
     */
    const char* extension = strrchr(source_path, '.');
    if (state.assetimport_read_options->format)       /* intentional setting format trumps file extension */
	extension = state.assetimport_read_options->format;
    if (!extension) {
	bu_log("ERROR: Please provide a file with a valid extension, or specify format with --format\n");
	return 0;
    }
    if (AI_FALSE == aiIsExtensionSupported(extension)) {
	bu_log("ERROR: The specified model file type is currently unsupported in assetimport conversion.\n");
	return 0;
    }

    mk_id_units(state.fd_out, "Conversion using Asset Importer Library (assetimport)", "mm");

    int ret = convert_input(&state);
    return ret;
}


static int
assetimport_can_read(const char* data)
{
    /* TODO FIXME - currently 'can_read' is unused by gcv */
    if (!data)
	return 0;

    return 1;
}


/* filter setup */
extern "C"
{
    static const struct gcv_filter gcv_conv_assetimport_read = {
	"Assetimport Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_ASSETIMPORT, assetimport_can_read,
	assetimport_read_create_opts, assetimport_read_free_opts, assetimport_read
    };

    extern const struct gcv_filter gcv_conv_assetimport_write;
    static const struct gcv_filter * const filters[] = { &gcv_conv_assetimport_read, &gcv_conv_assetimport_write, NULL };

    const struct gcv_plugin gcv_plugin_info_s = { filters };

    COMPILER_DLLEXPORT const struct gcv_plugin *
	gcv_plugin_info() { return &gcv_plugin_info_s; }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
