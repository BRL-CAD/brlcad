/*               T R I _ B O O L E V A L . C P P
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
/** @file libged/facetize/tri_booleval.cpp
 *
 * The core evaluation logic of the facetize command when targeting
 * triangle output.
 */

#include "common.h"

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>

#include <string.h>

#include "manifold/manifold.h"
#ifdef USE_ASSETIMPORT
#include "manifold/meshIO.h"
#endif

#include "bu/app.h"
#include "bu/path.h"
#include "bu/time.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

// Translate flags to ged_tessellate opts.  These need to match the options
// used by that executable to specify algorithms.
static const char *
method_opt(int *method_flags, struct directory *dp)
{
    switch (dp->d_minor_type) {
	case ID_DSP:
	    // DSP primitives need to avoid the methodology, since NMG
	    // doesn't seem to work very well
	    // TODO - revisit this if we get a better NMG facetize, perhaps
	    // using http://mgarland.org/software/terra.html
	    *method_flags = *method_flags & ~(FACETIZE_METHOD_NMG);
	    break;
	default:
	    break;
    }

    // NMG is best, when it works
    static const char *nmg_opt = "--nmg";
    if (*method_flags & FACETIZE_METHOD_NMG) {
	*method_flags = *method_flags & ~(FACETIZE_METHOD_NMG);
	return nmg_opt;
    }

    // CM is currently the best bet fallback
    static const char *cm_opt = "--cm";
    if (*method_flags & FACETIZE_METHOD_CONTINUATION) {
	*method_flags = *method_flags & ~(FACETIZE_METHOD_CONTINUATION);
	return cm_opt;
    }

    // SPSR via point sampling is currently our only option for a non-manifold
    // input
    static const char *spsr_opt = "--spsr";
    if (*method_flags & FACETIZE_METHOD_SPSR) {
	*method_flags = *method_flags & ~(FACETIZE_METHOD_SPSR);
	return spsr_opt;
    }

    // If we've exhausted the methods available, no option is available
    return NULL;
}

static int
bot_to_manifold(void **out, struct db_tree_state *tsp, struct rt_db_internal *ip)
{
    if (!out || !tsp || !ip)
	return BRLCAD_ERROR;

    // By this point all leaves should be bots
    if (ip->idb_minor_type != ID_BOT)
	return BRLCAD_ERROR;

    struct rt_bot_internal *nbot = (struct rt_bot_internal *)ip->idb_ptr;

    manifold::Mesh bot_mesh;
    for (size_t j = 0; j < nbot->num_vertices ; j++)
	bot_mesh.vertPos.push_back(glm::vec3(nbot->vertices[3*j], nbot->vertices[3*j+1], nbot->vertices[3*j+2]));
    for (size_t j = 0; j < nbot->num_faces; j++)
	bot_mesh.triVerts.push_back(glm::vec3(nbot->faces[3*j], nbot->faces[3*j+1], nbot->faces[3*j+2]));

    manifold::Manifold bot_manifold = manifold::Manifold(bot_mesh);
    if (bot_manifold.Status() != manifold::Manifold::Error::NoError) {
	// Urk - we got a mesh, but it's no good for a Manifold(??)
	return BRLCAD_ERROR;
    }

    // Passed - return the manifold
    (*out) = new manifold::Manifold(bot_manifold);
    return 0;
}

// Customized version of rt_booltree_leaf_tess for Manifold processing
static union tree *
_booltree_leaf_tess(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *UNUSED(data))
{
    int ts_status = 0;
    union tree *curtree;
    struct directory *dp;

    if (!tsp || !pathp || !ip)
	return TREE_NULL;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (tsp->ts_m)
	NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    BG_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_RESOURCE(tsp->ts_resp);


    void *odata = NULL;
    ts_status = bot_to_manifold(&odata, tsp, ip);
    if (ts_status < 0) {
	// If we failed, return TREE_NULL
	return TREE_NULL;
    }

    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = NULL;
    curtree->tr_d.td_d = odata;

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK)
	bu_log("_booltree_leaf_tess(%s) OK\n", dp->d_namep);

    return curtree;
}


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

static int
manifold_do_bool(
        union tree *tp, union tree *tl, union tree *tr,
        int op, struct bu_list *UNUSED(vlfree), const struct bn_tol *UNUSED(tol), void *data)
{
    struct _ged_facetize_state *s = (struct _ged_facetize_state *)data;
    if (!s)
	return -1;

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

    // By this point we should have prepared our Manifold inputs - now
    // it's a question of doing the evaluation

    // We're either working with the results of CSG NMG tessellations,
    // or we have prior manifold_mesh results - figure out which.
    // If it's the NMG results, we need to make manifold_meshes for
    // processing.
    manifold::Manifold *lm = (manifold::Manifold *)tl->tr_d.td_d;
    manifold::Manifold *rm = (manifold::Manifold *)tr->tr_d.td_d;
    manifold::Manifold *result = NULL;
    int failed = 0;
    if (!lm || lm->Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - left manifold invalid\n");
	lm = NULL;
	failed = 1;
    }
    if (!rm || rm->Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - right manifold invalid\n");
	rm = NULL;
	failed = 1;
    }
    if (!failed) {
	manifold::Manifold bool_out;
	try {
	    bool_out = lm->Boolean(*rm, manifold_op);
	    if (bool_out.Status() != manifold::Manifold::Error::NoError) {
		bu_log("Error - bool result invalid\n");
		failed = 1;
	    }
	} catch (...) {
	    bu_log("Manifold boolean library threw failure\n");
#ifdef USE_ASSETIMPORT
	    const char *evar = getenv("GED_MANIFOLD_DEBUG");
	    // write out the failing inputs to files to aid in debugging
	    if (evar && strlen(evar)) {
		std::cerr << "Manifold op: " << (int)manifold_op << "\n";
		manifold::ExportMesh(std::string(tl->tr_d.td_name)+std::string(".glb"), lm->GetMesh(), {});
		manifold::ExportMesh(std::string(tr->tr_d.td_name)+std::string(".glb"), rm->GetMesh(), {});
		bu_exit(EXIT_FAILURE, "Exiting to avoid overwriting debug outputs from Manifold boolean failure.");
	    }
#endif
	    failed = 1;
	}

#if 0
	// If we're debugging and need to capture glb for a "successful" case, these can be uncommented
	manifold::ExportMesh(std::string(tl->tr_d.td_name)+std::string(".glb"), lm->GetMesh(), {});
	manifold::ExportMesh(std::string(tr->tr_d.td_name)+std::string(".glb"), rm->GetMesh(), {});
	manifold::ExportMesh(std::string(tl->tr_d.td_name)+std::to_string(op)+std::string(tr->tr_d.td_name)+std::string(".glb"), bool_out.GetMesh(), {});
#endif

	if (!failed)
	    result = new manifold::Manifold(bool_out);
    }

    if (failed) {
	// TODO - we may want to try an NMG boolean op (or possibly
	// even geogram) here as a fallback
#if 0
	manifold::Mesh lmesh = lm->GetMesh();
	Mesh -> NMG
	manifold::Mesh rmesh = rm->GetMesh();
	Mesh -> NMG
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
	    result = nmg_to_manifold(reg);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    return 1;
	} BU_UNSETJUMP;
#endif
    }

    // Memory cleanup
    if (tl->tr_d.td_d) {
	manifold::Manifold *m = (manifold::Manifold *)tl->tr_d.td_d;
	delete m;
	tl->tr_d.td_d = NULL;
    }
    if (tr->tr_d.td_d) {
	manifold::Manifold *m = (manifold::Manifold *)tr->tr_d.td_d;
	delete m;
	tr->tr_d.td_d = NULL;
    }

    if (failed) {
	tp->tr_d.td_d = NULL;
	return -1;
    }

    tp->tr_op = OP_TESS;
    tp->tr_d.td_d = (void *)result;
    return 0;
}

int
_ged_facetize_booleval(struct _ged_facetize_state *s, int argc, struct directory **dpa, const char *newname)
{
    if (!s)
	return BRLCAD_ERROR;

    if (!argc || !dpa)
	return BRLCAD_ERROR;

    struct ged *gedp = s->gedp;

    /* First stage is to process the primitive instances */
    const char *sfilter = "! -type comb";
    struct bu_ptbl leaf_dps = BU_PTBL_INIT_ZERO;
    if (db_search(&leaf_dps, DB_SEARCH_RETURN_UNIQ_DP, sfilter, argc, dpa, gedp->dbip, NULL) < 0) {
	// Empty input - nothing to facetize;
	return BRLCAD_ERROR;
    }

    /* OK, we have work to do. Figure out the working .g filename */
    struct bu_vls wfilename = BU_VLS_INIT_ZERO;
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    struct bu_vls dname = BU_VLS_INIT_ZERO;
    bu_path_component(&bname, gedp->dbip->dbi_filename, BU_PATH_BASENAME);
    bu_path_component(&dname, gedp->dbip->dbi_filename, BU_PATH_DIRNAME);
    unsigned long long hash_num = bu_str_hash(bu_vls_cstr(&dname));
    bu_vls_free(&dname);
    // TODO - Hash the path to the .g file - the filename and path in combination should be unique, so we
    // can avoid collisions between .g files with the same name in different directories
    bu_vls_sprintf(&wfilename, "%lld_%s_tess.g", hash_num, bu_vls_cstr(&bname));
    bu_vls_free(&bname);
    char wfile[MAXPATHLEN];
    bu_dir(wfile, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&wfilename), NULL);
    bu_vls_free(&wfilename);

    /* TODO - If we're resuming, see if we already have an appropriately named
     * .g file.  If we don't, or we're not resuming, make the working copy. */
    // Copy current .g file to a temp file name for processing
    std::ifstream orig_file(gedp->dbip->dbi_filename, std::ios::binary);
    std::ofstream work_file(wfile, std::ios::binary);
    if (!orig_file.is_open() || !work_file.is_open())
	return BRLCAD_ERROR;
    work_file << orig_file.rdbuf();
    orig_file.close();
    work_file.close();

    // TODO - Sort out dp objects by d_len and type

    // Build up the path to the ged_tessellate executable
    char tess_exec[MAXPATHLEN];
    bu_dir(tess_exec, MAXPATHLEN, BU_DIR_BIN, "ged_tessellate", BU_DIR_EXT, NULL);

    // Call ged_tessellate to produce evaluated solids - TODO batch
    // into groupings that can be fed to ged_tessellate for parallel
    // processing, with fallback to individual if parallel fails
    // Build up the command to run
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct bu_vls abs_str = BU_VLS_INIT_ZERO;
    struct bu_vls rel_str = BU_VLS_INIT_ZERO;
    struct bu_vls norm_str = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&abs_str, "%0.17f", wdbp->wdb_ttol.abs);
    bu_vls_sprintf(&rel_str, "%0.17f", wdbp->wdb_ttol.rel);
    bu_vls_sprintf(&norm_str, "%0.17f", wdbp->wdb_ttol.norm);
    int method_flags = s->method_flags;
    const char *tess_cmd[MAXPATHLEN] = {NULL};
    tess_cmd[ 0] = tess_exec;
    tess_cmd[ 1] = "--tol-abs";
    tess_cmd[ 2] = bu_vls_cstr(&abs_str);
    tess_cmd[ 3] = "--tol-rel";
    tess_cmd[ 4] = bu_vls_cstr(&rel_str);
    tess_cmd[ 5] = "--tol-norm";
    tess_cmd[ 6] = bu_vls_cstr(&norm_str);
    tess_cmd[ 7] = NULL;
    tess_cmd[ 8] = "-O";
    tess_cmd[ 9] = wfile;

    for (size_t i = 0; i < BU_PTBL_LEN(&leaf_dps); i++) {
	struct directory *ldp = (struct directory *)BU_PTBL_GET(&leaf_dps, i);
	tess_cmd[10] = ldp->d_namep;
	// There are a number of methods that can be tried.  We try them in priority
	// order, timing out if one of them goes too long.
	int rc = -1;
	method_flags = s->method_flags;
	tess_cmd[7] = method_opt(&method_flags, ldp);
	while (tess_cmd[7]) {
	    int aborted = 0, timeout = 0;
	    int64_t start = bu_gettime();
	    int64_t elapsed = 0;
	    fastf_t seconds = 0.0;
	    struct bu_process *p = NULL;
	    bu_process_exec(&p, tess_cmd[0], 11, tess_cmd, 0, 0);
	    while (!timeout && p && (bu_process_pid(p) != -1)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		elapsed = bu_gettime() - start;
		seconds = elapsed / 1000000.0;
		if (seconds > s->max_time) {
		    bu_terminate(bu_process_pid(p));
		    timeout = 1;
		}
	    }
	    int w_rc = bu_process_wait(&aborted, p, 0);
	    rc = (timeout) ? -1 : w_rc;

	    if (rc == BRLCAD_OK)
		break;

	    tess_cmd[7] = method_opt(&method_flags, ldp);
	}

	if (rc != BRLCAD_OK) {
	    // If we tried all the methods and didn't get any successes, we have an
	    // error.  We can either terminate the whole conversion based on this
	    // failure, or ignore this object and process the remainder of the
	    // tree.  The latter will be incorrect if this object was supposed to
	    // materially contribute to the final object shape, but for some
	    // applications like visualization there are cases where "something is
	    // better than nothing" applies
	    return -1;
	}
    }

    // Open working .g copy with BoTs replacing CSG solids and perform
    // the tree walk to set up Manifold data.  Using the working copy
    // means we will be getting the correct triangle data for each solid
    const char **av = (const char **)bu_calloc(argc+1, sizeof(char *), "av");
    for (int i = 0; i < argc; i++) {
	av[i] = dpa[i]->d_namep;
    }
    struct db_i *wdbip = db_open(wfile, DB_OPEN_READONLY);
    if (!wdbip)
	return BRLCAD_ERROR;
    if (db_dirbuild(wdbip) < 0)
	return BRLCAD_ERROR;
    db_update_nref(wdbip, &rt_uniresource);
    struct rt_wdb *wwdbp = wdb_dbopen(wdbip, RT_WDB_TYPE_DB_DEFAULT);

    /* Second stage is to instance the BoTs generated by stage 1 and
     * prepare Manifold inputs for evaluation */
    {
	struct db_tree_state init_state;
	db_init_db_tree_state(&init_state, wdbip, wwdbp->wdb_resp);
	/* Establish tolerances */
	init_state.ts_ttol = &wwdbp->wdb_ttol;
	init_state.ts_tol = &wwdbp->wdb_tol;
	init_state.ts_m = NULL;
	s->facetize_tree = (union tree *)0;
	int i = 0;
	if (!BU_SETJUMP) {
	    /* try */
	    i = db_walk_tree(wdbip, argc, av,
		    1,
		    &init_state,
		    0,			/* take all regions */
		    facetize_region_end,
		    _booltree_leaf_tess,
		    (void *)s
		    );
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    i = -1;
	} BU_UNSETJUMP;

	if (i < 0) {
	    return -1;
	}
    }
    bu_free(av, "av");

    // TODO - We don't have a tree - whether that's an error or not depends
    if (!s->facetize_tree) {
	return 0;
    }

    // Third stage is to execute the boolean operations
    union tree *ftree = rt_booltree_evaluate(s->facetize_tree, &RTG.rtg_vlfree, &wwdbp->wdb_tol, &rt_uniresource, &manifold_do_bool, 0, (void *)s);
    if (!ftree) {
	return -1;
    }

    // By this point, we should be done with the temporary copy
    // TODO - this needs to be cleaned up in other error cases
    db_close(wdbip);
    bu_file_delete(wfile);

    if (ftree->tr_d.td_d) {
	manifold::Manifold *om = (manifold::Manifold *)ftree->tr_d.td_d;
	manifold::Mesh rmesh = om->GetMesh();
	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->bot_flags = 0;
	bot->num_vertices = (int)rmesh.vertPos.size();
	bot->num_faces = (int)rmesh.triVerts.size();
	bot->vertices = (double *)calloc(rmesh.vertPos.size()*3, sizeof(double));;
	bot->faces = (int *)calloc(rmesh.triVerts.size()*3, sizeof(int));
	for (size_t j = 0; j < rmesh.vertPos.size(); j++) {
	    bot->vertices[3*j] = rmesh.vertPos[j].x;
	    bot->vertices[3*j+1] = rmesh.vertPos[j].y;
	    bot->vertices[3*j+2] = rmesh.vertPos[j].z;
	}
	for (size_t j = 0; j < rmesh.triVerts.size(); j++) {
	    bot->faces[3*j] = rmesh.triVerts[j].x;
	    bot->faces[3*j+1] = rmesh.triVerts[j].y;
	    bot->faces[3*j+2] = rmesh.triVerts[j].z;
	}
	delete om;
	ftree->tr_d.td_d = NULL;

	// If we have a manifold_mesh, write it out as a bot
	return _ged_facetize_write_bot(s, bot, newname);
    }

    if (!s->quiet) {
	bu_log("FACETIZE: failed to generate %s\n", newname);
    }

    return -1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

