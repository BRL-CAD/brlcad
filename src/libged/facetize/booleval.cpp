/*                     B O O L E V A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/facetize/booleval.cpp
 *
 * The core evaluation logic of the facetize command.
 */

#include "common.h"

#include <map>
#include <set>
#include <vector>

#include <string.h>

#include "manifold/manifold.h"
#ifdef USE_ASSETIMPORT
#include "manifold/meshIO.h"
#endif


#include "bu/app.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

static union tree *
facetize_region_end(struct db_tree_state *tsp,
	const struct db_full_path *pathp,
	union tree *curtree,
	void *client_data)
{
    union tree **facetize_tree;

    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);

    struct _ged_facetize_state *s = (struct _ged_facetize_state *)client_data;
    facetize_tree = &s->facetize_tree;

    if (curtree->tr_op == OP_NOP) return curtree;

    if (*facetize_tree) {
	union tree *tr;
	BU_ALLOC(tr, union tree);
	RT_TREE_INIT(tr);
	tr->tr_op = OP_UNION;
	tr->tr_b.tb_regionp = REGION_NULL;
	tr->tr_b.tb_left = *facetize_tree;
	tr->tr_b.tb_right = curtree;
	*facetize_tree = tr;
    } else {
	*facetize_tree = curtree;
    }

    /* Tree has been saved, and will be freed later */
    return TREE_NULL;
}

struct manifold_mesh {
    int num_vertices;
    int num_faces;
    fastf_t *vertices;
    unsigned int *faces;
};


static
struct manifold_mesh *
bot_to_mmesh(struct rt_bot_internal *bot)
{
    struct manifold_mesh *omesh = NULL;
    BU_GET(omesh, struct manifold_mesh);
    omesh->num_vertices = bot->num_vertices;
    omesh->num_faces = bot->num_faces;
    omesh->faces = (unsigned int *)bu_calloc(bot->num_faces * 3, sizeof(unsigned int), "faces array");
    for (size_t i = 0; i < bot->num_faces * 3; i++) {
	omesh->faces[i] = bot->faces[i];
    }
    omesh->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "vert array");
    for (size_t i = 0; i < bot->num_vertices * 3; i++) {
	omesh->vertices[i] = bot->vertices[i];
    }
    return omesh;
}

static
struct manifold_mesh *
nmg_to_mmesh(struct nmgregion *r)
{
    if (!r)
	return NULL;

    return NULL;
}

static
struct nmgregion *
mmesh_to_nmg(struct manifold_mesh *m)
{
    if (!m)
	return NULL;

    return NULL;
}

long
bool_meshes(
	double **o_coords, int *o_ccnt, unsigned int **o_tris, int *o_tricnt,
	manifold::OpType b_op,
	double *a_coords, int a_ccnt, unsigned int *a_tris, int a_tricnt,
	double *b_coords, int b_ccnt, unsigned int *b_tris, int b_tricnt,
#ifdef USE_ASSETIMPORT
	const char *lname, const char *rname)
#else
	const char *UNUSED(lname), const char *UNUSED(rname))
#endif
{
    if (!o_coords || !o_ccnt || !o_tris || !o_tricnt)
	return -1;

    manifold::Mesh a_mesh;
    for (int i = 0; i < a_ccnt; i++)
	a_mesh.vertPos.push_back(glm::vec3(a_coords[3*i], a_coords[3*i+1], a_coords[3*i+2]));
    for (int i = 0; i < a_tricnt; i++)
	a_mesh.triVerts.push_back(glm::vec3(a_tris[3*i], a_tris[3*i+1], a_tris[3*i+2]));
    manifold::Mesh b_mesh;
    for (int i = 0; i < b_ccnt; i++)
	b_mesh.vertPos.push_back(glm::vec3(b_coords[3*i], b_coords[3*i+1], b_coords[3*i+2]));
    for (int i = 0; i < b_tricnt; i++)
	b_mesh.triVerts.push_back(glm::vec3(b_tris[3*i], b_tris[3*i+1], b_tris[3*i+2]));

    manifold::Manifold a_manifold(a_mesh);
    if (a_manifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - a invalid\n");
	return -1;
    }
    manifold::Manifold b_manifold(b_mesh);
    if (b_manifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - b invalid\n");
	return -1;
    }

    manifold::Manifold *result = new manifold::Manifold();
    try {
	*result = a_manifold.Boolean(b_manifold, b_op);
	if (result->Status() != manifold::Manifold::Error::NoError) {
	    bu_log("Error - bool result invalid\n");
	    return -1;
	}
    } catch (...) {
	bu_log("Manifold boolean library threw failure\n");
#ifdef USE_ASSETIMPORT
	const char *evar = getenv("GED_MANIFOLD_DEBUG");
	// write out the failing inputs to files to aid in debugging
	if (evar && strlen(evar)) {
	    std::cerr << "Manifold op: " << (int)b_op << "\n";
	    manifold::ExportMesh(std::string(lname)+std::string(".glb"), a_manifold.GetMesh(), {});
	    manifold::ExportMesh(std::string(rname)+std::string(".glb"), b_manifold.GetMesh(), {});
	    bu_exit(EXIT_FAILURE, "Exiting to avoid overwriting debug outputs from Manifold boolean failure.");
	}
#endif
	return -1;
    }
    manifold::Mesh rmesh = result->GetMesh();
    delete result;

    (*o_coords) = (double *)calloc(rmesh.vertPos.size()*3, sizeof(double));
    (*o_tris) = (unsigned int *)calloc(rmesh.triVerts.size()*3, sizeof(unsigned int));
    for (size_t i = 0; i < rmesh.vertPos.size(); i++) {
	(*o_coords)[3*i] = rmesh.vertPos[i].x;
	(*o_coords)[3*i+1] = rmesh.vertPos[i].y;
	(*o_coords)[3*i+2] = rmesh.vertPos[i].z;
    }
    for (size_t i = 0; i < rmesh.triVerts.size(); i++) {
	(*o_tris)[3*i] = rmesh.triVerts[i].x;
	(*o_tris)[3*i+1] = rmesh.triVerts[i].y;
	(*o_tris)[3*i+2] = rmesh.triVerts[i].z;
    }
    *o_ccnt = (int)rmesh.vertPos.size();
    *o_tricnt = (int)rmesh.triVerts.size();


    int not_solid = bg_trimesh_solid2(*o_ccnt, *o_tricnt, (fastf_t *)*o_coords, (int *)*o_tris, NULL);
    if (not_solid) {
	*o_ccnt = 0;
	*o_tricnt = 0;
	bu_free(*o_coords, "coords");
	*o_coords = NULL;
	bu_free(*o_tris, "tris");
	*o_tris = NULL;
	bu_log("Error: Manifold boolean succeeded, but result reports as non-solid: failure.");
	return -1;
    }

    return rmesh.triVerts.size();
}

int
_ged_manifold_do_bool(
        union tree *tp, union tree *tl, union tree *tr,
        int op, struct bu_list *UNUSED(vlfree), const struct bn_tol *tol, void *data)
{
    // If we're planning to write out an NMG primitive, we can't stage through
    // Manifold since it operates on triangles and we would lose any higher order
    // polygon faces.
    //if (flags & FACETIZE_NMG) {
//
 //   }

    struct rt_bot_internal *lbot = NULL;
    struct rt_bot_internal *rbot = NULL;
    struct manifold_mesh *lmesh = NULL;
    struct manifold_mesh *rmesh = NULL;
    struct manifold_mesh *omesh = NULL;
    struct _ged_facetize_state *s = (struct _ged_facetize_state *)data;
    struct rt_wdb *wdbp = wdb_dbopen(s->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    // Uncomment to see debugging output
    //_ged_facetize_log_default(s);

    // Translate op for MANIFOLD
    manifold::OpType manifold_op = manifold::OpType::Add;
    switch (op) {
	case OP_UNION:
	    manifold_op = manifold::OpType::Add;
	    break;
	case OP_INTERSECT:
	    manifold_op = manifold::OpType::Intersect;
	    break;
	case OP_SUBTRACT:
	    manifold_op = manifold::OpType::Subtract;
	    break;
	default:
	    manifold_op = manifold::OpType::Add;
    };

    // We're either working with the results of CSG NMG tessellations,
    // or we have prior manifold_mesh results - figure out which.
    // If it's the NMG results, we need to make manifold_meshes for
    // processing.
    if (tl->tr_d.td_r) {
	if (!BU_SETJUMP) {
	    /* try */
	    lbot = (struct rt_bot_internal *)nmg_mdl_to_bot(tl->tr_d.td_r->m_p, &RTG.rtg_vlfree, tol);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    lbot = NULL;
	    lmesh = NULL;
	} BU_UNSETJUMP;

	if (lbot) {
	    lmesh = bot_to_mmesh(lbot);
	    // We created this locally if it wasn't originally a BoT - clean up
	    if (lbot->vertices)
		bu_free(lbot->vertices, "verts");
	    if (lbot->faces)
		bu_free(lbot->faces, "faces");
	    BU_FREE(lbot, struct rt_bot_internal);
	    lbot = NULL;
	}
    } else {
	lmesh = (struct manifold_mesh *)tl->tr_d.td_d;
    }
    if (tr->tr_d.td_r) {

	if (!BU_SETJUMP) {
	    /* try */
	    rbot = (struct rt_bot_internal *)nmg_mdl_to_bot(tr->tr_d.td_r->m_p, &RTG.rtg_vlfree, tol);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    rbot = NULL;
	    rmesh = NULL;
	} BU_UNSETJUMP;

	if (rbot) {
	    rmesh = bot_to_mmesh(rbot);
	    // We created this locally if it wasn't originally a BoT - clean up
	    if (rbot->vertices)
		bu_free(rbot->vertices, "verts");
	    if (rbot->faces)
		bu_free(rbot->faces, "faces");
	    BU_FREE(rbot, struct rt_bot_internal);
	    rbot = NULL;
	}

    } else {
	rmesh = (struct manifold_mesh *)tr->tr_d.td_d;
    }
    int failed = 0;
    if (!lmesh || !rmesh) {
	failed = 1;
    } else {
	BU_GET(omesh, struct manifold_mesh);
	int mret = bool_meshes(
		(double **)&omesh->vertices, &omesh->num_vertices, &omesh->faces, &omesh->num_faces,
		manifold_op,
		(double *)lmesh->vertices, lmesh->num_vertices, lmesh->faces, lmesh->num_faces,
		(double *)rmesh->vertices, rmesh->num_vertices, rmesh->faces, rmesh->num_faces,
		tl->tr_d.td_name,
		tr->tr_d.td_name
		);

	failed = (mret < 0) ? 1 : 0;

	if (failed) {
	    // TODO - we should be able to try an NMG boolean op here as a
	    // fallback, but we may need to translate one or both of the inputs
	    // into NMG
	    int nmg_op = NMG_BOOL_ADD;
	    switch (op) {
		case OP_UNION:
		    nmg_op = NMG_BOOL_ADD;
		    break;
		case OP_INTERSECT:
		    nmg_op = NMG_BOOL_ISECT;
		    break;
		case OP_SUBTRACT:
		    nmg_op = NMG_BOOL_SUB;
		    break;
		default:
		    nmg_op = NMG_BOOL_ADD;
	    }

	    failed = 0;
	    struct nmgregion *td_l = (tl->tr_d.td_r) ? tl->tr_d.td_r : NULL;
	    struct nmgregion *td_r = (tr->tr_d.td_r) ? tr->tr_d.td_r : NULL;
	    if (!td_l)
		td_l = mmesh_to_nmg(lmesh);
	    if (!td_r)
		td_r = mmesh_to_nmg(rmesh);

	    if (!td_l || !td_r) {
		failed = 1;
	    }

	    if (!failed) {
		struct nmgregion *reg;
		if (!BU_SETJUMP) {
		    /* try */

		    /* move operands into the same model */
		    if (td_l->m_p != td_r->m_p)
			nmg_merge_models(td_l->m_p, td_r->m_p);
		    reg = nmg_do_bool(td_l, td_r, nmg_op, &RTG.rtg_vlfree, &wdbp->wdb_tol);
		    if (reg) {
			nmg_r_radial_check(reg, &RTG.rtg_vlfree, &wdbp->wdb_tol);
		    }
		    omesh = nmg_to_mmesh(reg);
		} else {
		    /* catch */
		    BU_UNSETJUMP;
		    return 1;
		} BU_UNSETJUMP;
	    }

	    bu_free(omesh->faces, "faces");
	    bu_free(omesh->vertices, "vertices");
	    BU_PUT(omesh, struct manifold_mesh);
	    omesh = NULL;
	}
    }

    // Memory cleanup
    if (tl->tr_d.td_r) {
	nmg_km(tl->tr_d.td_r->m_p);
	tl->tr_d.td_r = NULL;
    }
    if (tr->tr_d.td_r) {
	nmg_km(tr->tr_d.td_r->m_p);
	tr->tr_d.td_r = NULL;
    }
    if (tl->tr_d.td_d) {
	struct manifold_mesh *km = (struct manifold_mesh *)tl->tr_d.td_d;
	bu_free(km->faces, "faces");
	bu_free(km->vertices, "vertices");
	BU_PUT(km, struct manifold_mesh);
	tl->tr_d.td_d = NULL;
    }
    if (tr->tr_d.td_d) {
	struct manifold_mesh *km = (struct manifold_mesh *)tr->tr_d.td_d;
	bu_free(km->faces, "faces");
	bu_free(km->vertices, "vertices");
	BU_PUT(km, struct manifold_mesh);
	tr->tr_d.td_d = NULL;
    }

    tp->tr_d.td_r = NULL;

    if (failed) {
	tp->tr_d.td_d = NULL;
	return -1;
    }

    tp->tr_op = OP_TESS;
    tp->tr_d.td_d = omesh;
    return 0;
}

static int
_try_facetize(struct manifold_mesh **omesh, struct model **omodel, struct _ged_facetize_state *s, int argc, const char **argv)
{
    struct ged *gedp = s->gedp;
    int i, failed;
    union tree *ftree = NULL;
    struct model *nmg_model = nmg_mm();

    _ged_facetize_log_nmg(s);

    struct db_tree_state init_state;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    db_init_db_tree_state(&init_state, gedp->dbip, wdbp->wdb_resp);
    /* Establish tolerances */
    init_state.ts_ttol = &wdbp->wdb_ttol;
    init_state.ts_tol = &wdbp->wdb_tol;
    init_state.ts_m = &nmg_model;
    s->facetize_tree = (union tree *)0;

    if (!BU_SETJUMP) {
	/* try */
	i = db_walk_tree(gedp->dbip, argc, (const char **)argv,
			 1,
			&init_state,
			 0,			/* take all regions */
			 facetize_region_end,
			 rt_booltree_leaf_tess,
			 (void *)s
			);
    } else {
	/* catch */
	BU_UNSETJUMP;
	_ged_facetize_log_default(s);
	return -1;
    } BU_UNSETJUMP;

    if (i < 0) {
	/* Destroy NMG */
	_ged_facetize_log_default(s);
	return -1;
    }

    if (s->facetize_tree) {

	if (s->make_nmg) {
	    // We're processing as an NMG, so we can't use Manifold - it works
	    // only on triangles, and we would lose any higher order polygon
	    // faces from the NMG primitives.  Use an old school nmg_boolean
	    if (!BU_SETJUMP) {
		/* try */
		failed = nmg_boolean(s->facetize_tree, nmg_model, &RTG.rtg_vlfree, &wdbp->wdb_tol, &rt_uniresource);
	    } else {
		/* catch */
		BU_UNSETJUMP;
		_ged_facetize_log_default(s);
		return failed;
	    } BU_UNSETJUMP;

	    if (!failed && s->facetize_tree) {
		NMG_CK_REGION(s->facetize_tree->tr_d.td_r);
		s->facetize_tree->tr_d.td_r = (struct nmgregion *)NULL;
	    }

	    if (s->facetize_tree) {
		db_free_tree(s->facetize_tree, &rt_uniresource);
	    }

	    *omodel = (failed) ? NULL : nmg_model;
	    return failed;
	}

	// We're not making an NMG - use Manifold
	ftree = rt_booltree_evaluate(s->facetize_tree, &RTG.rtg_vlfree, &wdbp->wdb_tol, &rt_uniresource, &_ged_manifold_do_bool, 0, (void *)s);
	if (!ftree) {
	    _ged_facetize_log_default(s);
	    return -1;
	}
	if (ftree->tr_d.td_d) {
	    *omesh = (struct manifold_mesh *)ftree->tr_d.td_d;
	    _ged_facetize_log_default(s);
	    return 0;
	} else if (ftree->tr_d.td_r) {
	    // If we started with or end up with an NMG mesh but are using
	    // Manifold to make a BoT, we need to translate the NMG for the
	    // next stage.
	    struct rt_bot_internal *bot = NULL;
	    if (!BU_SETJUMP) {
		/* try */
		bot = (struct rt_bot_internal *)nmg_mdl_to_bot(ftree->tr_d.td_r->m_p, &RTG.rtg_vlfree, &wdbp->wdb_tol);
	    } else {
		/* catch */
		BU_UNSETJUMP;
		bot = NULL;
		omesh = NULL;
	    } BU_UNSETJUMP;

	    if (bot) {
		*omesh = bot_to_mmesh(bot);
		// We created this locally if it wasn't originally a BoT - clean up
		if (bot->vertices)
		    bu_free(bot->vertices, "verts");
		if (bot->faces)
		    bu_free(bot->faces, "faces");
		BU_FREE(bot, struct rt_bot_internal);
		bot = NULL;
		_ged_facetize_log_default(s);
		return 0;
	    } else {
		_ged_facetize_log_default(s);
		return 1;
	    }
	}
    }

    // TODO - We don't have a tree - whether that's an error or not depends
    _ged_facetize_log_default(s);
    return 0;
}

int
_ged_facetize_booleval(struct _ged_facetize_state *s, int argc, const char **argv, const char *newname)
{
    // Depending on the options, the tree walk with produce either
    // a manifold_mesh or an NMG model.
    struct manifold_mesh *mesh = NULL;
    struct model *nmg_model = NULL;

    int ret = BRLCAD_ERROR;
    if (!s || !argc || !argv || !newname)
	goto ged_manifold_obj_memfree;

    ret = _try_facetize(&mesh, &nmg_model, s, argc, argv);

    // If we have a manifold_mesh, write it out as a bot
    if (mesh && mesh->num_faces > 0) {
	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->bot_flags = 0;
	bot->num_vertices = mesh->num_vertices;
	bot->num_faces = mesh->num_faces;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->num_normals = 0;
	bot->num_face_normals = 0;
	bot->normals = NULL;
	bot->face_normals = NULL;
	bot->vertices = (fastf_t *)bu_calloc(mesh->num_vertices * 3, sizeof(fastf_t), "vertices");
	for (int i = 0; i < mesh->num_vertices * 3; i++) {
	    bot->vertices[i] = mesh->vertices[i];
	}
	bot->faces = (int *)bu_calloc(mesh->num_faces * 3, sizeof(int), "faces");
	for (int i = 0; i < mesh->num_faces * 3; i++) {
	    bot->faces[i] = mesh->faces[i];
	}

	ret = _ged_facetize_write_bot(s, bot, newname);
	goto ged_manifold_obj_memfree;
    }

    // If we have an NMG, write it out
    if (nmg_model) {
	ret = _ged_facetize_write_nmg(s, nmg_model, newname);
	goto ged_manifold_obj_memfree;
    }

ged_manifold_obj_memfree:
    if (mesh) {
	if (mesh->vertices)
	    bu_free(mesh->vertices, "verts");
	if (mesh->faces)
	    bu_free(mesh->faces, "faces");
	BU_FREE(mesh, struct manifold_mesh);
    }
    if (!s->quiet && ret != BRLCAD_OK) {
	bu_log("MANIFOLD: failed to generate %s\n", newname);
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

