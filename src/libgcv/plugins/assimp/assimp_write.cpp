#include "common.h"

#include <string>
#include <vector>
#include <unordered_map>

#include "gcv.h"
#include "raytrace.h"

/* assimp headers */
#include <assimp/scene.h>
#include <assimp/Exporter.hpp>

typedef struct assimp_write_options {
    char* format;
    const char* model_workaround;       /* once gcv can specify models to convert, this should go away */
} assimp_write_options_t;

typedef struct assimp_write_state {
    const struct gcv_opts *gcv_options;
    assimp_write_options_t* assimp_write_options;

    struct db_i *dbip;
    struct model *the_model;

    aiScene* scene;                     /* generated scene to be exported */
    std::vector<aiNode*> children;      /* dynamically keeps track of children while walking */
    std::vector<aiMesh*> meshes;        /* dynamically keeps track of bot meshes while walking */
    std::vector<aiMaterial*> mats;      /* dynamically keeps track of materials (shaders) */
    std::unordered_map<std::string, int> mat_names;     /* maps material to vector index */
    assimp_write_state() {
        gcv_options = NULL;
        assimp_write_options = NULL;
        dbip = NULL;
        the_model = NULL;
        scene = NULL;
        children = {};
        meshes = {};
        mats = {};
        mat_names = {};
    }
} assimp_write_state_t;

HIDDEN char* 
strip_at_char(char* str, char token)
{
    char* ret = strrchr(str, token);
    if (ret)
        return &ret[1];

    return NULL;
}

HIDDEN void
nmg_to_assimp(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state* tsp, void *client_data)
{
    assimp_write_state_t* const pstate = (assimp_write_state_t*)client_data;
    struct model* m;
    struct shell *s;
    struct vertex *v;
    struct bu_ptbl verts = BU_PTBL_INIT_ZERO;
    struct bu_ptbl norms = BU_PTBL_INIT_ZERO;
    std::vector<aiFace*> faces = {};
    aiMesh* curr_mesh = new aiMesh();
    aiNode* curr_node = new aiNode();
    size_t numverts = 0;		                /* Number of vertices to output */
    size_t i;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);
    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, &RTG.rtg_vlfree, &pstate->gcv_options->calculational_tolerance);

    /* list all verticies in result */
    nmg_vertex_tabulate(&verts, &r->l.magic, &RTG.rtg_vlfree);

    /* Get number of vertices */
    numverts = BU_PTBL_LEN(&verts);

    /* get list of vertexuse normals */
    nmg_vertexuse_normal_tabulate(&norms, &r->l.magic, &RTG.rtg_vlfree);

    /* Prelim check all vertices */
    for (i = 0; i < numverts; i++) {
	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);
    }

    /* Check triangles, storing vert and face data */
    curr_mesh->mVertices = new aiVector3D[numverts];
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;
		int vert_count=0;
                aiFace* curr_face = new aiFace();
                curr_face->mIndices = new unsigned int[3];

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
                    size_t loc;
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

                    loc = bu_ptbl_locate(&verts, (long *)v);
                    if ((long)loc < 0 || loc >= numverts) {
                        /* TODO I dont think we ever actually get here, but if we do
                         * handle the mismatch without a full stop
                         */
			bu_ptbl_free(&verts);
			bu_log("ERROR: bad vertex location (%zu)!\n", loc);
                        return;
		    }

                    /* store vertex data and match index to face data */
                    curr_face->mIndices[vert_count] = loc;
                    curr_mesh->mVertices[loc].x = v->vg_p->coord[0] * pstate->gcv_options->scale_factor;
                    curr_mesh->mVertices[loc].y = v->vg_p->coord[1] * pstate->gcv_options->scale_factor;
                    curr_mesh->mVertices[loc].z = v->vg_p->coord[2] * pstate->gcv_options->scale_factor;
		    vert_count++;
		}

                // TODO ai handles polygons
		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_log("ERROR: LU is not a triangle -- ");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
                    return;
		} else if (vert_count < 3)      // TODO same here
		    continue;
                
                /* if we have a triangle, we're good to add the face */
                curr_face->mNumIndices = 3;
                /* it may be worth for time complexity allocating faces to a worst 
                 * case 'numverts' up-front rather than using push_back
                 */
                faces.push_back(curr_face);
	    }
	}
    }
    
    /* generate mesh */
    char* region_name = strip_at_char(db_path_to_string(pathp), '/');
    if (!region_name)
        sprintf(region_name, "region_%lu", pstate->meshes.size());
    curr_mesh->mName = aiString(region_name);
    curr_mesh->mPrimitiveTypes |= aiPrimitiveType_TRIANGLE;
    curr_mesh->mNumVertices = numverts;
    curr_mesh->mNumFaces = faces.size();
    
    /* add faces to mesh */
    curr_mesh->mFaces = new aiFace[faces.size()];     /* convert vector to arr */
    for (i = 0; i < faces.size(); i++) {
        curr_mesh->mFaces[i] = *faces[i];
    }

    /* add normals to mesh */
    size_t numnorms = BU_PTBL_LEN(&norms);
    if (numnorms == numverts) {
        curr_mesh->mNormals = new aiVector3D[numnorms];
        for (i = 0; i < numnorms; i++) {
            struct vertexuse_a_plane *va;

            va = (struct vertexuse_a_plane *)BU_PTBL_GET(&norms, i);
            NMG_CK_VERTEXUSE_A_PLANE(va);

            curr_mesh->mNormals[i].x = va->N[0] * pstate->gcv_options->scale_factor;
            curr_mesh->mNormals[i].y = va->N[1] * pstate->gcv_options->scale_factor;
            curr_mesh->mNormals[i].z = va->N[2] * pstate->gcv_options->scale_factor;
        }
    }

    /* set color data at mesh level */
    curr_mesh->mColors[0] = new aiColor4D(1);  /* default color is white */
    if (tsp->ts_mater.ma_color_valid) {
        curr_mesh->mColors[0]->r = tsp->ts_mater.ma_color[0];
        curr_mesh->mColors[0]->g = tsp->ts_mater.ma_color[1];
        curr_mesh->mColors[0]->b = tsp->ts_mater.ma_color[2];
    }

    /* set material data using shader string */
    if (tsp->ts_mater.ma_shader) {
        std::string shader_name(tsp->ts_mater.ma_shader);
        auto it = pstate->mat_names.find(shader_name);
        if (it != pstate->mat_names.end()) {
            curr_mesh->mMaterialIndex = it->second;
        } else {
            /* we create a new material and give it the name of the raw ma_shader
            * NOTE: this hack-ily handles any modifiers (re, sh, etc)
            * by stuffing them into the string rather than setting them properly
            * in the material properties. This works internally because assimp_read.cpp
            * re-sets the shader string but this will NOT work to relate material data
            * to other readers using the file format */
            aiMaterial* mat = new aiMaterial();
        
            aiString* str_to_aistr = new aiString(shader_name);
            mat->AddProperty(str_to_aistr, AI_MATKEY_NAME);
    
            /* TODO set properties the correct way */
        //     int blinn = aiShadingMode_Blinn;
        //     mat->AddProperty<int>(&blinn, 1, AI_MATKEY_SHADING_MODEL);
    
        //     aiColor3D col(final_color->r, final_color->g, final_color->b);
    
        //     mat->AddProperty<aiColor3D>(&col, 1, AI_MATKEY_COLOR_DIFFUSE);
        //     aiTextureType_UNKNOWN for mat_code?
    
            /* add new material to map */
            pstate->mats.push_back(mat);
            curr_mesh->mMaterialIndex = pstate->mats.size();
            pstate->mat_names.emplace(shader_name, pstate->mats.size());
        }
    } else {
        curr_mesh->mMaterialIndex = 0;
    }

    /* generate node */
    aiString name(db_path_to_string(pathp));
    curr_node->mName = name;

    /* set node transformation 
     * NOTE: facetize takes into account transformation, so we set identity matrix 
     */
    aiMatrix4x4 trans;
    curr_node->mTransformation = trans;

    /* the current node will be a child of the scene root and have no children */
    curr_node->mParent = pstate->scene->mRootNode;
    curr_node->mNumChildren = 0;

    /* point the current node mesh index to newly created mesh */
    curr_node->mMeshes = new unsigned int[1];
    curr_node->mMeshes[0] = pstate->meshes.size();
    curr_node->mNumMeshes = 1;

    /* keep track of new mesh and node */
    pstate->children.push_back(curr_node);
    pstate->meshes.push_back(curr_mesh);
}

HIDDEN void
assimp_write_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    assimp_write_options_t* options_data;

    BU_ALLOC(options_data, assimp_write_options_t);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(3 * sizeof(struct bu_opt_desc), "options_desc");

    options_data->format = NULL;
    options_data->model_workaround = NULL;

    BU_OPT((*options_desc)[0], NULL, "format", "string", bu_opt_str, &options_data->format, "specify the output file format");
    BU_OPT((*options_desc)[1], NULL, "model", "string", bu_opt_str, &options_data->model_workaround, "specify a single object to convert");
    BU_OPT_NULL((*options_desc)[2]);
}


HIDDEN void
assimp_write_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

HIDDEN int
assimp_write(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *dest_path)
{
    assimp_write_state_t state;
    struct gcv_region_end_data gcvwriter;
    struct db_tree_state tree_state;
    int ret = 1;

    gcvwriter.write_region = nmg_to_assimp;
    gcvwriter.client_data = &state;

    state.gcv_options = gcv_options;
    state.assimp_write_options = (assimp_write_options_t*)options_data;

    /* check and validate the specied output file type against ai 
     * checks using file extension if no --format is supplied 
     * this is likely all a 'can_write' function would need
     */
    const char* outext = strip_at_char((char*)dest_path, '.');
    if (state.assimp_write_options->format)     /* intentional setting format trumps file extension */
        outext = state.assimp_write_options->format;
    const aiExportFormatDesc* fmt_desc = NULL;
    bool found_fmt = false;
    if (!outext) {
        bu_log("ERROR: Please provide a file with an extension, or specify format with --format\n");
        return 0;
    }
    Assimp::Exporter exporter;
    for (size_t i = 0, end = exporter.GetExportFormatCount(); i < end; ++i) {
        fmt_desc = exporter.GetExportFormatDescription(i);
        if (!bu_strcmp(outext, fmt_desc->fileExtension)) {
            found_fmt = true;
            break;
        }
    }
    if (!found_fmt) {
        bu_log("ERROR: Non-supported file format (%s)\n", outext);
        return 0;
    }

    /* generate aiScene from .g */
    state.scene = new aiScene();
    state.scene->mRootNode = new aiNode();

    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &state.gcv_options->calculational_tolerance;
    tree_state.ts_ttol = &state.gcv_options->tessellation_tolerance;
    tree_state.ts_m = &state.the_model;

    /* make empty NMG model */
    state.the_model = nmg_mm();
    BU_LIST_INIT(&RTG.rtg_vlfree);	/* for vlist macros */
    
    /* Walk indicated tree(s).  Each region will be output separately */
    if (state.assimp_write_options->model_workaround) {
        const char* single[1] = { state.assimp_write_options->model_workaround };
        (void) db_walk_tree(context->dbip, 1, single,
                1,
                &tree_state,
                0,			/* take all regions */
                (gcv_options->tessellation_algorithm == GCV_TESS_MARCHING_CUBES)?gcv_region_end_mc:gcv_region_end,
                (gcv_options->tessellation_algorithm == GCV_TESS_MARCHING_CUBES)?NULL:nmg_booltree_leaf_tess,
                (void *)&gcvwriter);
    } else {
    (void) db_walk_tree(context->dbip, gcv_options->num_objects, (const char **)gcv_options->object_names,
			1,
			&tree_state,
			0,			/* take all regions */
			(gcv_options->tessellation_algorithm == GCV_TESS_MARCHING_CUBES)?gcv_region_end_mc:gcv_region_end,
			(gcv_options->tessellation_algorithm == GCV_TESS_MARCHING_CUBES)?NULL:nmg_booltree_leaf_tess,
			(void *)&gcvwriter);
    }

    /* update the scene */
    aiNode** conv_children = new aiNode*[state.children.size()];
    for (size_t i = 0; i < state.children.size(); i++) {
        conv_children[i] = state.children[i];
    }
    state.scene->mRootNode->addChildren(state.children.size(), conv_children);

    state.scene->mMeshes = new aiMesh*[state.meshes.size()];
    for (size_t i = 0; i < state.meshes.size(); i++) 
        state.scene->mMeshes[i] = state.meshes[i];
    state.scene->mNumMeshes = state.meshes.size();

    /* index 0 assigns 'plastic' to any region without a shader set */
    state.scene->mMaterials = new aiMaterial*[state.mats.size() +1];
    aiMaterial def;
    aiString def_name("plastic");
    def.AddProperty(&def_name, AI_MATKEY_NAME);
    state.scene->mMaterials[0] = &def;
    for (size_t i = 0; i < state.mats.size(); i++)
        state.scene->mMaterials[i+1] = state.mats[i];
    state.scene->mNumMaterials = state.mats.size() + 1;

    aiMetadata meta_null;
    meta_null.mNumProperties = 0;
    state.scene->mMetaData = &meta_null;

    /* export */
    aiReturn airet = exporter.Export(state.scene, fmt_desc->id, dest_path, state.scene->mFlags);
    if (airet != AI_SUCCESS) {
        bu_log("ERROR: %s", exporter.GetErrorString());
        ret = 0;
    }

    /* Release dynamic storage */
    nmg_km(state.the_model);
    rt_vlist_cleanup();

    return ret;
}

/* filter setup */
extern "C"
{
    extern const struct gcv_filter gcv_conv_assimp_write = {
        "Assimp Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_ASSIMP, NULL,
        assimp_write_create_opts, assimp_write_free_opts, assimp_write
    };
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
