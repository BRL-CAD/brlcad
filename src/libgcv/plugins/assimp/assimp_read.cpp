#include "common.h"

#include <string>

#include "gcv/api.h"
#include "wdb.h"

// assimp headers - these three are usually needed
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct shader_properties {
    const char *shadername;		            /**< shader name, or NULL */
    const char *shaderargs;		            /**< shader args, or NULL */
    const unsigned char* rgb; 			    /**< NULL => no color */
};
#define SHADER_PROPERTIES_NULL { NULL, NULL, NULL }

struct assimp_read_options
{
    int starting_id;                                /* starting ident id number */
    int const_id;                                   /* Constant ident number (assigned to all regions if non-negative) */
    int mat_code;                                   /* default material code */
    int verbose;                                    /* verbose flag */
};

struct conversion_state
{
    const struct gcv_opts* gcv_options;
    struct assimp_read_options* assimp_read_options;

    std::string input_file;	                    /* name of the input file */
    struct rt_wdb* fd_out;	                    /* Resulting BRL-CAD file */
    struct wmember all;                             /* scene root */

    const C_STRUCT aiScene* scene;                  /* assimp generated scene */
    int id_no;                                      /* region ident number */
    unsigned int dfs;                               /* number of nodes visited */
    unsigned int converted;                         /* number of meshes converted */
};
#define CONVERSION_STATE_NULL { NULL, NULL, "", NULL, WMEMBER_INIT_ZERO, NULL, 0, 0, 0 }

HIDDEN void
aimatrix_to_arr16(C_STRUCT aiMatrix4x4 aimat, fastf_t* ret)
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

HIDDEN struct shader_properties
generate_shader(struct conversion_state* pstate, unsigned int mesh_idx)
{
    struct shader_properties ret = SHADER_PROPERTIES_NULL;

    /* check for material data */
    if (pstate->scene->mNumMaterials) {
        unsigned int mat_idx = pstate->scene->mMeshes[mesh_idx]->mMaterialIndex;
        C_STRUCT aiMaterial* mat = pstate->scene->mMaterials[mat_idx];

        ret.shadername = mat->GetName().data;
        if (ret.shadername[0] == 0)
            ret.shadername = "plastic";

        /* get material data used for phong shading */
        C_STRUCT aiColor3D diff (-1);
        C_STRUCT aiColor3D spec (-1);
        C_STRUCT aiColor3D amb (-1);
        float s = -1.0;
        mat->Get(AI_MATKEY_COLOR_DIFFUSE, diff);
        mat->Get(AI_MATKEY_COLOR_SPECULAR, spec);
        mat->Get(AI_MATKEY_COLOR_AMBIENT, amb);
        mat->Get(AI_MATKEY_SHININESS_STRENGTH, s);

        /* TODO create shader arg string */
        std::string fmt_args = "{";
        if (s >= 0) {
            fmt_args.append("sh ");
            fmt_args.append(std::to_string(s));
        }
        fmt_args.append("}");

        ret.shaderargs = fmt_args.c_str();
    }

    /* set the color of the face using the first vertex color data we 
     * find if such exists
     * NOTE: we make two assumptions:
     * 1) each vertex only has one set of colors
     * 2) each vertex in the triangle is the same color
     */
    C_STRUCT aiColor4D* mesh_color = pstate->scene->mMeshes[mesh_idx]->mColors[0];
    if (mesh_color) {
        unsigned char trgb[3] = { 0 };
        trgb[0] = (unsigned char)(mesh_color->r * 255);
        trgb[1] = (unsigned char)(mesh_color->g * 255);
        trgb[2] = (unsigned char)(mesh_color->b * 255);
        ret.rgb = trgb;
        if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose)
            bu_log("color data (%d, %d, %d)\n", (int)ret.rgb[0], (int)ret.rgb[1], (int)ret.rgb[2]);
    }

    return ret;
}

HIDDEN void
generate_geometry(struct conversion_state* pstate, wmember &region, unsigned int mesh_idx)
{
    C_STRUCT aiMesh* mesh = pstate->scene->mMeshes[mesh_idx];
    int* faces = new int[mesh->mNumFaces * 3];
    double* vertices = new double[mesh->mNumVertices * 3];
    
    /* make sure we are dealing with only triangles 
     * TODO: support polygons by splitting into triangles 
     */
    if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) {
        bu_log("WARNING: unknown primitive in mesh[%d] -- skipping\n", mesh_idx);
        return;
    }

    if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose) {
        bu_log("mesh[%d] num Faces: %d\n", mesh_idx, mesh->mNumFaces);
        bu_log("mesh[%d] num vertices: %d\n\n", mesh_idx, mesh->mNumVertices);
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

    /* add mesh to region list */
    std::string mesh_name = "mesh_" + std::to_string(pstate->converted);
    mk_bot(pstate->fd_out, mesh_name.c_str(), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, mesh->mNumVertices, mesh->mNumFaces, vertices, faces, (fastf_t*)NULL, (struct bu_bitv*)NULL);
    (void)mk_addmember(mesh_name.c_str(), &region.l, NULL, WMOP_UNION);

    /* book keeping to log converted meshes */
    pstate->converted++;
}

HIDDEN void
handle_node(struct conversion_state* pstate, const C_STRUCT aiNode* curr, struct wmember &regions)
{
    /* some files preserve names in the node
     * first check and get rid of slashes
     * if no slashes, check if theres any name data at all
     * otherwise just give the region a generic name
     * */
    const char* region_name = strrchr(curr->mName.data, '/');
    if (!region_name) {
        region_name = curr->mName.data;
    } else if (region_name[0] == '/') {
        region_name = &region_name[1];
    }
    if (region_name[0] == 0)
        sprintf((char*)region_name, "region_%d.r", pstate->dfs);

    if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose) {
        bu_log("Curr node name | dfs index: %s | %d\n", curr->mName.data, pstate->dfs);
        bu_log("Curr node transformation:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n", 
            curr->mTransformation.a1, curr->mTransformation.a2, curr->mTransformation.a3, curr->mTransformation.a4,
            curr->mTransformation.b1, curr->mTransformation.b2, curr->mTransformation.b3, curr->mTransformation.b4,
            curr->mTransformation.c1, curr->mTransformation.c2, curr->mTransformation.c3, curr->mTransformation.c4,
            curr->mTransformation.d1, curr->mTransformation.d2, curr->mTransformation.d3, curr->mTransformation.d4);
        bu_log("Curr node children: %d\n", curr->mNumChildren);
        bu_log("Curr node meshes: %d\n", curr->mNumMeshes);
        bu_log("Region name: %s\n", region_name);
        if (!curr->mNumMeshes)
            bu_log("\n");
    }
    
    /* book keeping for dfs node traversal */
    pstate->dfs++;
    
    /* generate a list to hold this node's meshes */
    struct wmember mesh;
    BU_LIST_INIT(&mesh.l);
    struct shader_properties shader_prop = SHADER_PROPERTIES_NULL;

    /* handle the current nodes meshes if any */
    for (size_t i = 0; i < curr->mNumMeshes; i++) {
        /* each node has an array of mesh indicies which correlates to 
         * the index of mMeshes in scene
         */
        unsigned int mesh_idx = curr->mMeshes[i];
        if (mesh_idx >= pstate->scene->mNumMeshes)
            bu_exit(EXIT_FAILURE, "ERROR: bad mesh index");
        if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose)
            bu_log("      uses mesh %d\n", mesh_idx);
        generate_geometry(pstate, mesh, mesh_idx);

        /* FIXME generate shader and color 
         * this assumes all mesh under a region are using the same material data
         * if they're not, we're just using the last one
         */
        shader_prop = generate_shader(pstate, mesh_idx);
    }
    if (curr->mNumMeshes) {
        /* apply parent transformations */
        fastf_t tra[16];
        aimatrix_to_arr16(curr->mTransformation, tra);

        /* make region with all meshes */
        mk_lrcomb(pstate->fd_out, region_name, &mesh, 1, shader_prop.shadername, shader_prop.shaderargs, shader_prop.rgb, pstate->id_no, 0, pstate->assimp_read_options->mat_code, 100, 0);
        (void)mk_addmember(region_name, &mesh.l, tra, WMOP_UNION);

        if (!pstate->assimp_read_options->const_id)
            pstate->id_no++;
    }


    /* add base region to top level */
    if (!curr->mNumChildren)
        (void)mk_addmember(region_name, &regions.l, NULL, WMOP_UNION);

    /* recursive call all children */
    for (size_t i = 0; i < curr->mNumChildren; i++) {
        handle_node(pstate, curr->mChildren[i], regions);
    }
}

HIDDEN void
convert_input(struct conversion_state* pstate)
{
    /* we are taking one of the postprocessing presets to avoid
     * spelling out 20+ single postprocessing flags here. 
     */
    pstate->scene = aiImportFile(pstate->input_file.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);

    /* we know there is atleast one root node if conversion is successful */
    if (!pstate->scene || !pstate->scene->mRootNode)
        bu_exit(EXIT_FAILURE, "ERROR: bad scene conversion");
        
    if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose) {
        bu_log("Scene num meshes: %d\n", pstate->scene->mNumMeshes);
        bu_log("Scene num materials: %d\n", pstate->scene->mNumMaterials);
        bu_log("Scene num Animations: %d\n", pstate->scene->mNumAnimations);
        bu_log("Scene num Textures: %d\n", pstate->scene->mNumTextures);
        bu_log("Scene num Lights: %d\n", pstate->scene->mNumLights);
        bu_log("Scene num Cameras: %d\n\n", pstate->scene->mNumCameras);
    }

    /* iterate over each top level node
     * TODO FIXME: some formats (uncertain which) have more than one root
     */
    BU_LIST_INIT(&pstate->all.l);
    for (size_t i = 0; i < 1; i++) {
        if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose)
            bu_log("top level node: %zu\n", i);
        handle_node(pstate, &pstate->scene->mRootNode[i], pstate->all);

        /* make a top level 'all.g' for each root node*/
        std::string all = "all_" + std::to_string(i) + ".g";
        mk_lcomb(pstate->fd_out, all.c_str(), &pstate->all, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
    }
}

HIDDEN void
assimp_read_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct assimp_read_options *options_data;

    BU_ALLOC(options_data, struct assimp_read_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(5 * sizeof(struct bu_opt_desc), "options_desc");

    options_data->starting_id = 1000;
    options_data->const_id = 0;
    options_data->mat_code = 1;
    options_data->verbose = 0;

    BU_OPT((*options_desc)[0], NULL, "starting-ident", "number", bu_opt_int, options_data, "specify the starting ident for the regions created");
    BU_OPT((*options_desc)[1], NULL, "constant-ident", NULL, NULL, &options_data->const_id, "specify that the starting ident should remain constant");
    BU_OPT((*options_desc)[2], NULL, "material", "number", bu_opt_int, &options_data->mat_code, "specify the material code that will be assigned to created regions");
    BU_OPT((*options_desc)[3], "v", "verbose", NULL, NULL, &options_data->verbose, "specify for verbose output");
    BU_OPT_NULL((*options_desc)[4]);
}

HIDDEN void
assimp_read_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

HIDDEN int
assimp_read(struct gcv_context *context, const struct gcv_opts* gcv_options, const void *options_data, const char *source_path)
{
    struct conversion_state state = CONVERSION_STATE_NULL;

    state.gcv_options = gcv_options;
    state.assimp_read_options = (struct assimp_read_options*)options_data;
    state.id_no = state.assimp_read_options->starting_id;
    state.input_file = source_path;
    state.fd_out = context->dbip->dbi_wdbp;

    /* check and validate the specied model file extension against ai
     * NOTE: this is likely all a 'can_read' function will need for ai
     * but it requires extension recognition..
     */
    const char* extension = strrchr(source_path, '.');
    if (!extension) {
        bu_exit(EXIT_FAILURE, "ERROR: Please provide a file with a valid extension");
    }
    if (AI_FALSE == aiIsExtensionSupported(extension)) {
        bu_exit(EXIT_FAILURE, "ERROR: The specified model file extension is currently unsupported in Assimp conversion.");
    }

    mk_id_units(state.fd_out, "Conversion using Asset Importer Library (assimp)", "mm");

    convert_input(&state);

    bu_log("Converted ( %d / %d ) meshes ... %.2f%%\n", state.converted, state.scene->mNumMeshes, (float) (state.converted / state.scene->mNumMeshes * 100));

    return 1;
}


HIDDEN int
assimp_can_read(const char* data)
{
    /* TODO FIXME - currently 'can_read' is unused by gcv */
    if (!data)
        return 0;

    return 1;
}


/* filter setup */
extern "C"
{
    static const struct gcv_filter gcv_conv_assimp_read = {
        "Assimp Reader", GCV_FILTER_READ, BU_MIME_MODEL_ASSIMP, assimp_can_read,
        assimp_read_create_opts, assimp_read_free_opts, assimp_read
    };

    extern const struct gcv_filter gcv_conv_assimp_write;
    static const struct gcv_filter * const filters[] = { &gcv_conv_assimp_read, &gcv_conv_assimp_write, NULL };

    const struct gcv_plugin gcv_plugin_info_s = { filters };

    COMPILER_DLLEXPORT const struct gcv_plugin *
    gcv_plugin_info() { return &gcv_plugin_info_s; }
}