#include "common.h"

#include <string>
#include <unordered_map>

#include "gcv/api.h"
#include "wdb.h"

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
        delete name;
        delete args;
        delete rgb;
    }
} shader_properties_t;

typedef struct assimp_read_options {
    int starting_id;                                /* starting ident id number */
    int const_id;                                   /* Constant ident number (assigned to all regions if non-negative) */
    int mat_code;                                   /* default material code */
    int verbose;                                    /* verbose flag */
    char* format;                                   /* output file format */
} assimp_read_options_t;

typedef struct assimp_read_state {
    const struct gcv_opts* gcv_options;             /* global options */
    assimp_read_options_t* assimp_read_options;     /* internal options */

    std::string input_file;	                    /* name of the input file */
    struct rt_wdb* fd_out;	                    /* Resulting BRL-CAD file */
    struct wmember all;                             /* scene root */

    const aiScene* scene;                           /* assimp generated scene */
    int id_no;                                      /* region ident number */
    unsigned int dfs;                               /* number of nodes visited */
    unsigned int converted;                         /* number of meshes converted */
    assimp_read_state() {
        gcv_options = NULL;
        assimp_read_options = NULL;
        input_file = "";
        fd_out = NULL;
        all = WMEMBER_INIT_ZERO;
        scene = NULL;
        id_no = 0;
        dfs = 0;
        converted = 0;
    }
    ~assimp_read_state() {}
} assimp_read_state_t;

HIDDEN void
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

HIDDEN shader_properties_t*
generate_shader(assimp_read_state_t* pstate, unsigned int mesh_idx)
{
    shader_properties_t* ret = new shader_properties_t();

    /* check for material data */
    if (pstate->scene->HasMaterials()) {
        unsigned int mat_idx = pstate->scene->mMeshes[mesh_idx]->mMaterialIndex;
        aiMaterial* mat = pstate->scene->mMaterials[mat_idx];

        /* for brlcad shaders, a space means we have additional shader params
         * in the form "shader_name param1=x .."
         */
        std::string fmt = mat->GetName().data;
        size_t space = fmt.find(" ");

        if (space != std::string::npos) {
            ret->name = new char[space +1];
            ret->args = new char[fmt.size() - space];
            bu_strlcpy(ret->name, fmt.substr(0, space).c_str(), space +1);
            bu_strlcpy(ret->args, fmt.substr(space+1).c_str(), fmt.size() - space);
        } else {
            ret->name = new char[fmt.size() +1];
            bu_strlcpy(ret->name, fmt.c_str(), fmt.size() +1);
        }

        /* TODO create shader arg string from set material data, not just name */
        /* get material data used for phong shading */
        aiColor3D diff (-1);
        aiColor3D spec (-1);
        aiColor3D amb (-1);
        float s = -1.0;
        mat->Get(AI_MATKEY_COLOR_DIFFUSE, diff);
        mat->Get(AI_MATKEY_COLOR_SPECULAR, spec);
        mat->Get(AI_MATKEY_COLOR_AMBIENT, amb);
        mat->Get(AI_MATKEY_SHININESS_STRENGTH, s);
    }

    /* set the color of the face using the first vertex color data we 
     * find if such exists
     * NOTE: we make two assumptions:
     * 1) each vertex only has one set of colors
     * 2) each vertex in the triangle is the same color
     */
    aiColor4D* mesh_color = pstate->scene->mMeshes[mesh_idx]->mColors[0];
    if (mesh_color) {
        ret->rgb = new unsigned char[3];
        ret->rgb[0] = (unsigned char)(mesh_color->r * 255);
        ret->rgb[1] = (unsigned char)(mesh_color->g * 255);
        ret->rgb[2] = (unsigned char)(mesh_color->b * 255);
        if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose)
            bu_log("color data (%d, %d, %d)\n", (int)ret->rgb[0], (int)ret->rgb[1], (int)ret->rgb[2]);
    }

    return ret;
}

HIDDEN std::string
generate_unique_name(const char* curr_name, unsigned int def_idx, bool is_mesh)
{
    static std::unordered_map<std::string, int>used_names; /* used names in db */

    std::string name(curr_name);
    std::string prefix("region");
    std::string suffix(".r");

    /* get last instance of '/' and keep everything after it */
    const char* trim = strrchr(curr_name, '/');
    if (trim)
        name = ++trim;

    if (is_mesh) {
        prefix = "mesh";
        suffix = ".MESH";

        /* get rid of .r if it is at end of name but regions have no enforced pattern
         * to discern them from underlying meshes when importing names. 
         * add '.MESH' at end to reduce collisions */
        size_t dotr = name.find(".r\0");
        if (dotr != std::string::npos)
            name = name.substr(0, dotr);

        name.append(suffix);
    }

    /* if curr_name is empty, give a generic name */
    if (!name.size())
        name = prefix + "_" + std::to_string(def_idx) + suffix;

    /* check for name collisions */
    auto it = used_names.find(name);
    if (it != used_names.end())
        name.append("_DUP" + std::to_string(it->second++));
    used_names.emplace(name, 0);

    return name;
}

HIDDEN void
generate_geometry(assimp_read_state_t* pstate, wmember &region, unsigned int mesh_idx)
{
    aiMesh* mesh = pstate->scene->mMeshes[mesh_idx];
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

    /* add mesh to region list */
    std::string mesh_name = generate_unique_name(pstate->scene->mMeshes[mesh_idx]->mName.data, pstate->converted, 1);
    mk_bot(pstate->fd_out, mesh_name.c_str(), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, mesh->mNumVertices, mesh->mNumFaces, vertices, faces, (fastf_t*)NULL, (struct bu_bitv*)NULL);
    (void)mk_addmember(mesh_name.c_str(), &region.l, NULL, WMOP_UNION);

    /* book keeping to log converted meshes */
    pstate->converted++;

    /* cleanup memory */
    delete[] faces;
    delete[] vertices;
}

HIDDEN void
handle_node(assimp_read_state_t* pstate, aiNode* curr, struct wmember &regions)
{
    shader_properties_t shader_prop;
    std::string region_name = generate_unique_name(curr->mName.data, pstate->dfs, 0);

    if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose) {
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
        if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose)
            bu_log("      uses mesh %d\n", mesh_idx);

        generate_geometry(pstate, mesh, mesh_idx);

        /* FIXME generate shader and color 
         * this assumes all mesh under a region are using the same material data.
         * if they're not, we're just using the last one
         */
        shader_prop = *generate_shader(pstate, mesh_idx);
    }
    if (curr->mNumMeshes) {
        /* apply parent transformations */
        fastf_t tra[16];
        aimatrix_to_arr16(curr->mTransformation, tra);

        /* make region with all meshes */
        mk_lrcomb(pstate->fd_out, region_name.c_str(), &mesh, 1, shader_prop.name, shader_prop.args, shader_prop.rgb, pstate->id_no, 0, pstate->assimp_read_options->mat_code, 100, 0);
        (void)mk_addmember(region_name.c_str(), &mesh.l, tra, WMOP_UNION);

        if (!pstate->assimp_read_options->const_id)
            pstate->id_no++;
    }

    /* when we're done adding children, add to top level */
    if (!curr->mNumChildren)
        (void)mk_addmember(region_name.c_str(), &regions.l, NULL, WMOP_UNION);

    /* recursive call all children */
    for (size_t i = 0; i < curr->mNumChildren; i++) {
        handle_node(pstate, curr->mChildren[i], regions);
    }
}

HIDDEN int
convert_input(assimp_read_state_t* pstate)
{
    /* we are taking one of the postprocessing presets to have
     * max quality with reasonable render times. But, we must keep 
     * seemingly redundant materials as we use the names for BRLCAD shaders
     */
    pstate->scene = aiImportFile(pstate->input_file.c_str(), aiProcessPreset_TargetRealtime_MaxQuality & ~aiProcess_RemoveRedundantMaterials);

    if (!pstate->scene) {
        bu_log("ERROR: bad scene conversion\n");
        return 0;
    }
        
    if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose) {
        bu_log("Scene num meshes: %d\n", pstate->scene->mNumMeshes);
        bu_log("Scene num materials: %d\n", pstate->scene->mNumMaterials);
        bu_log("Scene num Animations: %d\n", pstate->scene->mNumAnimations);
        bu_log("Scene num Textures: %d\n", pstate->scene->mNumTextures);
        bu_log("Scene num Lights: %d\n", pstate->scene->mNumLights);
        bu_log("Scene num Cameras: %d\n\n", pstate->scene->mNumCameras);
    }

    /* create around the root node */
    BU_LIST_INIT(&pstate->all.l);
    if (pstate->gcv_options->verbosity_level || pstate->assimp_read_options->verbose)
        bu_log("-- root node --\n");
    handle_node(pstate, &pstate->scene->mRootNode[0], pstate->all);

    /* make a top level 'all.g' */
    mk_lcomb(pstate->fd_out, "all.g", &pstate->all, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);

    /* TODO FIXME: bad generation logs extra converted and mesh */
    bu_log("Converted ( %d / %d ) meshes ... %.2f%%\n", pstate->converted, pstate->scene->mNumMeshes, (float)pstate->converted / (float)pstate->scene->mNumMeshes * 100.0);

    return 1;
}

HIDDEN void
assimp_read_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    assimp_read_options_t* options_data;

    BU_ALLOC(options_data, assimp_read_options_t);
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

HIDDEN void
assimp_read_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

HIDDEN int
assimp_read(struct gcv_context *context, const struct gcv_opts* gcv_options, const void *options_data, const char *source_path)
{
    assimp_read_state_t state;

    state.gcv_options = gcv_options;
    state.assimp_read_options = (assimp_read_options_t*)options_data;
    state.id_no = state.assimp_read_options->starting_id;
    state.input_file = source_path;
    state.fd_out = context->dbip->dbi_wdbp;

    /* check and validate the specied input file type against ai
     * checks using file extension if no --format is supplied
     * this is likely all a 'can_read' function would need
     */
    const char* extension = strrchr(source_path, '.');
    if (state.assimp_read_options->format)       /* intentional setting format trumps file extension */
        extension = state.assimp_read_options->format;
    if (!extension) {
        bu_log("ERROR: Please provide a file with a valid extension, or specify format with --format\n");
        return 0;
    }
    if (AI_FALSE == aiIsExtensionSupported(extension)) {
        bu_log("ERROR: The specified model file type is currently unsupported in Assimp conversion.\n");
        return 0;
    }

    mk_id_units(state.fd_out, "Conversion using Asset Importer Library (assimp)", "mm");

    return convert_input(&state);
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
