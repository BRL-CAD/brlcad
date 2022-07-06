#include "common.h"

#include <string>
#include <vector>
#include <unordered_map>

#include "gcv.h"
#include "raytrace.h"

/* assimp headers */
#include <assimp/scene.h>
#include <assimp/Exporter.hpp>

struct assimp_write_options
{
    char* format;
};

struct conversion_state
{
    const struct gcv_opts *gcv_options;
    const struct assimp_write_options *assimp_write_options;

    struct db_i *dbip;
    struct model *the_model;

    /* TODO move scene out of here? all its being used for in walk is the root node */
    aiScene* scene;                     /* generated scene to be exported */
    std::vector<aiNode*> children;      /* dynamically keeps track of children while walking */
    std::vector<aiMesh*> meshes;        /* dynamically keeps track of bot meshes while walking */
    
};
#define CONVERSION_STATE_NULL { NULL, NULL, NULL, NULL, NULL, {}, {} }

/* uses strrchr to find last instance of 'token' and returns what remains excluding
 * the char. returns NULL if the token does not exist within the string 
 */
HIDDEN char* 
strip_at_char(char* str, char token) {
    char* ret = strrchr(str, token);
    if (ret)
        return &ret[1];

    return NULL;
}

HIDDEN void
nmg_to_assimp(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id), float color[3], void *client_data)
{
    struct conversion_state * const pstate = (struct conversion_state *)client_data;
    struct model* m;
    struct shell *s;
    struct vertex *v;
    struct bu_ptbl verts = BU_PTBL_INIT_ZERO;
    struct bu_ptbl norms = BU_PTBL_INIT_ZERO;
    size_t numverts = 0;		                /* Number of vertices to output */
    aiVector3D* final_vertices = NULL;                  /* puts 'verts' into usable arr for ai */
    std::vector<aiFace*> faces = {};
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

    /* Check vertices */
    for (i = 0; i < numverts; i++) {
	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);
    }

    /* Check triangles, storing vert and face data */
    final_vertices = new aiVector3D[numverts];
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
                unsigned int* face = new unsigned int[3];

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
                    if (loc < 0 || loc >= numverts) {
                        /* TODO I dont think we ever actually get here, but if we do
                         * handle the mismatch without a full stop
                         */
			bu_ptbl_free(&verts);
			bu_exit(1, "ERROR: bad vertex location (%zu)!", loc);
		    }

                    /* store vertex data and match index to face data */
                    face[vert_count] = loc;
                    final_vertices[loc].x = v->vg_p->coord[0] * pstate->gcv_options->scale_factor;
                    final_vertices[loc].y = v->vg_p->coord[1] * pstate->gcv_options->scale_factor;
                    final_vertices[loc].z = v->vg_p->coord[2] * pstate->gcv_options->scale_factor;
		    vert_count++;
		}

                // TODO ai handles polygons
		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle\n");
		} else if (vert_count < 3)      // TODO same here
		    continue;
                
                /* create a new face and add to vector */
                aiFace* curr_face = new aiFace();
                curr_face->mNumIndices = 3;
                curr_face->mIndices = face;
                /* it may be worth for time complexity allocating faces to a worst 
                 * case 'numverts' up-front rather than using push_back
                 */
                faces.push_back(curr_face);
	    }
	}
    }
    
    /* GENERATE MESH */
    aiMesh* curr_mesh = new aiMesh();

    char* region_name = strip_at_char(db_path_to_string(pathp), '/');
    if (!region_name)
        sprintf(region_name, "region_%lu", pstate->meshes.size());
    curr_mesh->mName = aiString(region_name);
    curr_mesh->mPrimitiveTypes |= aiPrimitiveType_TRIANGLE;
    curr_mesh->mNumVertices = numverts;
    curr_mesh->mNumFaces = faces.size();

    curr_mesh->mVertices = final_vertices;
    
    /* add faces */
    aiFace* final_faces = new aiFace[faces.size()];     /* convert vector to arr */
    for (i = 0; i < faces.size(); i++) {
        final_faces[i] = *faces[i];
    }
    curr_mesh->mFaces = final_faces;

    /* add normals */
    size_t numnorms = BU_PTBL_LEN(&norms);
    if (numnorms == numverts) {
        aiVector3D* final_normals = new aiVector3D[numnorms];
        for (i = 0; i < numnorms; i++) {
            struct vertexuse_a_plane *va;

            va = (struct vertexuse_a_plane *)BU_PTBL_GET(&norms, i);
            NMG_CK_VERTEXUSE_A_PLANE(va);

            aiVector3D curr;
            curr.x = va->N[0] * pstate->gcv_options->scale_factor;
            curr.y = va->N[1] * pstate->gcv_options->scale_factor;
            curr.z = va->N[2] * pstate->gcv_options->scale_factor;
            final_normals[i] = curr;
        }
        curr_mesh->mNormals = final_normals;
    }

    /* TODO generate material and color data */
    curr_mesh->mMaterialIndex = 0;

    aiColor4D* final_color = new aiColor4D();
    final_color->r = color[0];
    final_color->g = color[1];
    final_color->b = color[2];
    curr_mesh->mColors[0] = final_color;

    /* GENERATE NODE */
    aiNode* curr_node = new aiNode();

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
    curr_node->mNumMeshes = 1;
    unsigned int* temp_mMeshes = new unsigned int[1];
    temp_mMeshes[0] = pstate->meshes.size();
    curr_node->mMeshes = temp_mMeshes;

    // TODO cleanup memory

    /* keep track of new mesh and node */
    pstate->children.push_back(curr_node);
    pstate->meshes.push_back(curr_mesh);
}

HIDDEN void
assimp_write_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct assimp_write_options *options_data;

    BU_ALLOC(options_data, struct assimp_write_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(2 * sizeof(struct bu_opt_desc), "options_desc");

    options_data->format = NULL;

    BU_OPT((*options_desc)[0], NULL, "format", "string", bu_opt_str, &options_data->format, "specify the output file format");
    BU_OPT_NULL((*options_desc)[1]);
}


HIDDEN void
assimp_write_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

HIDDEN int
assimp_write(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *dest_path)
{
    struct conversion_state state = CONVERSION_STATE_NULL;
    struct gcv_region_end_data gcvwriter;
    struct db_tree_state tree_state;
    int ret = 1;

    gcvwriter.write_region = nmg_to_assimp;
    gcvwriter.client_data = &state;

    state.gcv_options = gcv_options;
    state.assimp_write_options = (struct assimp_write_options*)options_data;
    state.dbip = context->dbip;

    /* check and validate the specied output file type against ai 
     * checks using file extension if no --format is supplied 
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
        if (!strcmp(outext, fmt_desc->fileExtension)) {
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
    (void) db_walk_tree(state.dbip, gcv_options->num_objects, (const char **)gcv_options->object_names,
			1,
			&tree_state,
			0,			/* take all regions */
			(gcv_options->tessellation_algorithm == GCV_TESS_MARCHING_CUBES)?gcv_region_end_mc:gcv_region_end,
			(gcv_options->tessellation_algorithm == GCV_TESS_MARCHING_CUBES)?NULL:nmg_booltree_leaf_tess,
			(void *)&gcvwriter);

    /* update the scene */
    aiNode** conv_children = new aiNode*[state.children.size()];
    for (size_t i = 0; i < state.children.size(); i++) {
        conv_children[i] = state.children[i];
    }
    state.scene->mRootNode->addChildren(state.children.size(), conv_children);

    aiMesh** conv_meshes = new aiMesh*[state.meshes.size()];
    for (size_t i = 0; i < state.meshes.size(); i++) 
        conv_meshes[i] = state.meshes[i];
    state.scene->mMeshes = conv_meshes;
    state.scene->mNumMeshes = state.meshes.size();

    /* TODO FIXME: create better materials using shader data of db */
    aiMaterial** conv_mats = new aiMaterial*[1];
    state.scene->mMaterials = conv_mats;
    aiMaterial* temp = new aiMaterial();
    conv_mats[0] = temp;

    state.scene->mNumMaterials = 1;

    /* export */
    aiReturn airet = exporter.Export(state.scene, fmt_desc->id, dest_path, state.scene->mFlags);
    if (airet != AI_SUCCESS) {
        bu_log("ERROR: %s", exporter.GetErrorString());
        ret = 0;
    }

    /* Release dynamic storage */
    for (size_t i = 0; i < state.children.size(); i++)
        delete state.children[i];
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
