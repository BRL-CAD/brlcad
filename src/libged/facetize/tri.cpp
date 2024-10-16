/*                         T R I . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
 * Triangle centric boolean evaluation logic using Manifold library.
 */

#include "common.h"

#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <queue>

#include <string.h>

#include "manifold/manifold.h"
#ifdef USE_ASSETIMPORT
#include "manifold/meshIO.h"
#endif

#include "bu/app.h"
#include "bu/path.h"
#include "bu/snooze.h"
#include "bu/time.h"
#include "../ged_private.h"
#include "./ged_facetize.h"
#include "./tess_opts.h"
#include "./subprocess.h"

static int
bot_to_manifold(void **out, struct db_tree_state *tsp, struct rt_db_internal *ip, int flip)
{
    if (!out || !tsp || !ip)
	return BRLCAD_ERROR;

    // By this point all leaves should be bots
    if (ip->idb_minor_type != ID_BOT)
	return BRLCAD_ERROR;

    struct rt_bot_internal *nbot = (struct rt_bot_internal *)ip->idb_ptr;

    if (!nbot->num_vertices) {
	// Trivial case
        (*out) = new manifold::Manifold();
	return 0;
    }

    if (flip) {
	switch (nbot->orientation) {
	    case RT_BOT_CCW:
		nbot->orientation = RT_BOT_CW;
		break;
	    default:
		nbot->orientation = RT_BOT_CCW;
	}
    }

    if (nbot->num_vertices < 3)
	return BRLCAD_ERROR;

    manifold::MeshGL64 bot_mesh;
    for (size_t j = 0; j < nbot->num_vertices*3 ; j++)
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), nbot->vertices[j]);
    if (nbot->orientation == RT_BOT_CW) {
	for (size_t j = 0; j < nbot->num_faces; j++) {
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+0]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+2]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+1]);
	}
    } else {
	for (size_t j = 0; j < nbot->num_faces; j++) {
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+0]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+1]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), nbot->faces[3*j+2]);
	}
    }

    manifold::Manifold bot_manifold = manifold::Manifold(bot_mesh);
    if (bot_manifold.Status() != manifold::Manifold::Error::NoError) {
	// Urk - we got a mesh, but it's no good for a Manifold(??)
	return BRLCAD_ERROR;
    }

    // Passed - return the manifold
    (*out) = new manifold::Manifold(bot_manifold);
    return 0;
}

// We need to see if the matrix is turning the BoT inside out.  Make a
// test face, with a setup that will report non-flipping with an IDN
// matrix, and see what the currently active matrix does to it.
static int bot_flipped(mat_t *m)
{
    point_t oorigin = {-0.4, 0.5, 0.4};
    point_t othit = {-0.301, 0.581, 0.28};
    point_t ov[3] = {{0, 1, 1}, {-1, 1, 0}, {0, 0, 0}};

    point_t origin, thit;
    point_t v[3];

    for (int i = 0; i < 3; i++)
	MAT4X3PNT(v[i], *m, ov[i]);
    MAT4X3PNT(origin, *m, oorigin);
    MAT4X3PNT(thit, *m, othit);

    vect_t raydir;
    VSUB2(raydir, thit, origin);

    vect_t edges[2];
    VSUB2(edges[0], v[1], v[0]);
    VSUB2(edges[1], v[2], v[1]);

    vect_t ecross;
    VCROSS(ecross, edges[0], edges[1]);

    fastf_t vedot = VDOT(ecross, raydir);
    if (vedot > 0)
	return 1;

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

    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = NULL;
    curtree->tr_d.td_d = NULL;
    curtree->tr_d.td_i = NULL;

    // Infinite half spaces get special handling in the boolean evaluation
    if (ip->idb_minor_type == ID_HALF) {
	struct rt_db_internal *hintern;
	BU_GET(hintern, struct rt_db_internal);
	RT_DB_INTERNAL_INIT(hintern);
	hintern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	hintern->idb_type = ID_HALF;
	hintern->idb_meth = &OBJ[ID_HALF];
	struct rt_half_internal *hf_cp;
	BU_GET(hf_cp, struct rt_half_internal);
	hintern->idb_ptr = (void *)hf_cp;

	struct rt_half_internal *hf_ip= (struct rt_half_internal *)ip->idb_ptr;
	hf_cp->magic = hf_ip->magic;
	HMOVE(hf_cp->eqn, hf_ip->eqn);
	curtree->tr_d.td_i = hintern;
	return curtree;
    }

    // Anything else that's not a BoT is a no-op for booleans
    if (ip->idb_minor_type != ID_BOT)
	return curtree;

    // Observed in Goliath example model with SKTRACKdrivewheel2.c comb - due
    // to the values in ts_mat, the BoT ends up inside-out when read in.
    int flip = bot_flipped(&tsp->ts_mat);

    void *odata = NULL;
    ts_status = bot_to_manifold(&odata, tsp, ip, flip);
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
    curtree->tr_d.td_i = NULL;

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

    // If we have a left half space, bail - that's not well defined for producing
    // a Manifold closed volume
    if (tl->tr_d.td_i) {
	bu_log("Error - internal pointer on left boolean input\n");
	return -1;
    }

    // By this point we should have prepared our Manifold inputs - now
    // it's a question of doing the evaluation.
    manifold::Manifold *lm = (manifold::Manifold *)tl->tr_d.td_d;
    manifold::Manifold *rm = (manifold::Manifold *)tr->tr_d.td_d;
    manifold::Manifold *result = NULL;
    int failed = 0;
    bool delete_left = false;
    // On the right we can either have a Manifold, or a half space.  If it's
    // the latter, we need special handling.
    bool delete_right = false;
    if (tr->tr_d.td_i) {
	if (tr->tr_d.td_i->idb_minor_type != ID_HALF) {
	    return -1;
	}
	if (!lm) {
	    lm = new manifold::Manifold();
	    delete_left = true;
	}
	struct rt_half_internal *hf_ip= (struct rt_half_internal *)tr->tr_d.td_i->idb_ptr;
	if (manifold_op != manifold::OpType::Add) {

	    // Intersections and Subtractions with half spaces are handled
	    // by Manifold routines
	    vect_t pn;
	    pn[0] = hf_ip->eqn[0];
	    pn[1] = hf_ip->eqn[1];
	    pn[2] = hf_ip->eqn[2];
	    if (op == OP_INTERSECT)
		VSCALE(pn, pn, -1);
	    manifold::Manifold trimmed = lm->TrimByPlane(linalg::vec<double, 3>(pn[0], pn[1], pn[2]), hf_ip->eqn[3]);
	    result = new manifold::Manifold(trimmed);
	}

	BU_PUT(hf_ip, struct rt_half_internal);
	BU_PUT(tr->tr_d.td_i, struct rt_db_internal);
	tr->tr_d.td_i = NULL;
    }

    // Anything not already set up or handled is a no-op
    if (!lm) {
	lm = new manifold::Manifold();
	delete_left = true;
    }
    if (!rm) {
	rm = new manifold::Manifold();
	delete_right = true;
    }

    if (!result) {

	// Before we try a boolean, validate that our inputs satisfy Manifold's
	// criteria.
	if (!lm || lm->Status() != manifold::Manifold::Error::NoError) {
	    facetize_log(s, 0, "Error - left manifold invalid: %s\n", tl->tr_d.td_name);
	    lm = NULL;
	    failed = 1;
	}
	if (!rm || rm->Status() != manifold::Manifold::Error::NoError) {
	    facetize_log(s, 0, "Error - right manifold invalid: %s\n", tr->tr_d.td_name);
	    rm = NULL;
	    failed = 1;
	}

	if (!failed) {
	    // We should have valid inputs - proceed
	    facetize_log(s, 1, "Trying boolean op:  %s, %s\n", tl->tr_d.td_name, tr->tr_d.td_name);

	    manifold::Manifold bool_out;
	    try {
		bool_out = lm->Boolean(*rm, manifold_op);
		if (bool_out.Status() != manifold::Manifold::Error::NoError) {
		    facetize_log(s, 0, "Error - bool result invalid:\n\t%s\n\t%s\n", tl->tr_d.td_name, tr->tr_d.td_name);
		    failed = 1;
		}
	    } catch (...) {
		facetize_log(s, 0, "Manifold boolean library threw failure\n");
#ifdef USE_ASSETIMPORT
		const char *evar = getenv("GED_MANIFOLD_DEBUG");
		// write out the failing inputs to files to aid in debugging
		if (evar && strlen(evar)) {
		    std::cerr << "Manifold op: " << (int)manifold_op << "\n";
		    manifold::ExportMesh(std::string(tl->tr_d.td_name)+std::string(".glb"), lm->GetMeshGL(), {});
		    manifold::ExportMesh(std::string(tr->tr_d.td_name)+std::string(".glb"), rm->GetMeshGL(), {});
		    bu_exit(EXIT_FAILURE, "Exiting to avoid overwriting debug outputs from Manifold boolean failure.");
		}
#endif
		failed = 1;
	    }

#if 0
	    // If we're debugging and need to capture glb for a "successful" case, these can be uncommented
	    manifold::ExportMesh(std::string(tl->tr_d.td_name)+std::string(".glb"), lm->GetMeshGL(), {});
	    manifold::ExportMesh(std::string(tr->tr_d.td_name)+std::string(".glb"), rm->GetMeshGL(), {});
	    manifold::ExportMesh(std::string("out-") + std::string(tl->tr_d.td_name)+std::to_string(op)+std::string(tr->tr_d.td_name)+std::string(".glb"), bool_out.GetMesh(), {});
#endif

	    if (!failed)
		result = new manifold::Manifold(bool_out);
	}
    }

    // Memory cleanup
    if (delete_left)
	delete lm;
    if (delete_right)
	delete rm;

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

std::vector<std::string>
tess_avail_methods()
{

    // Build up the path to the ged_exec executable
    char tess_exec[MAXPATHLEN];
    bu_dir(tess_exec, MAXPATHLEN, BU_DIR_BIN, "ged_exec", BU_DIR_EXT, NULL);

    const char *tess_cmd[MAXPATHLEN] = {NULL};
    tess_cmd[ 0] = tess_exec;
    tess_cmd[ 1] = "facetize_process";
    tess_cmd[ 2] = "--list-methods";
    tess_cmd[ 3] = NULL;

    struct bu_process* p;
    bu_process_create(&p, tess_cmd, BU_PROCESS_HIDE_WINDOW);

    char mraw[MAXPATHLEN] = {'\0'};
    int read_res = bu_process_read_n(p, BU_PROCESS_STDOUT, MAXPATHLEN, mraw);

    if (bu_process_wait_n(&p, 0) || (read_res <= 0)) {
	// wait error or read error
	bu_log("%s %s - wait or read error\n", tess_cmd[0], tess_cmd[1]);
	std::vector<std::string> empty;
	return empty;
    }

    std::string mstr = std::string((const char *)mraw);
    std::stringstream mstream(mstr);
    std::string m;
    std::vector<std::string> methods;
    while (std::getline(mstream, m, ' ')) {
	methods.push_back(m);
    }

    return methods;
}

int
tess_run(struct _ged_facetize_state *s, const char **tess_cmd, int tess_cmd_cnt, fastf_t max_time, int ocnt)
{
    if (!s || !tess_cmd || !tess_cmd[3])
	return BRLCAD_ERROR;

    std::string wfile(tess_cmd[3]);
    std::string wfilebak = wfile + std::string(".bak");
    {
	// Before the run, prepare a backup file
	std::ifstream workfile(wfile, std::ios::binary);
	std::ofstream bakfile(wfilebak, std::ios::binary);
	if (!workfile.is_open() || !bakfile.is_open()) {
	    bu_log("Unable to create backup file %s\n", wfilebak.c_str());
	    return BRLCAD_ERROR;
	}
	bakfile << workfile.rdbuf();
	workfile.close();
	bakfile.close();
    }

    // Record the actual command being use to trigger the subprocess
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    for (int i = 0; i < tess_cmd_cnt ; i++)
	bu_vls_printf(&cmd, "%s ", tess_cmd[i]);
    facetize_log(s, 2, "%s\n", bu_vls_cstr(&cmd));
    bu_vls_free(&cmd);

    // If we're not being verbose, just report how many objects we're working on
    if (ocnt == 1)
	facetize_log(s, 0, "Attempting to triangulate %s...", tess_cmd[tess_cmd_cnt-ocnt]);
    if (ocnt > 1)
	facetize_log(s, 0, "Attempting to triangulate %d solids...", ocnt);

    int64_t start = bu_gettime();
    int64_t elapsed = 0;
    fastf_t seconds = 0.0;
    tess_cmd[tess_cmd_cnt] = NULL; // Make sure we're NULL terminated
    struct subprocess_s p;
    if (subprocess_create(tess_cmd, subprocess_option_no_window|subprocess_option_enable_async|subprocess_option_inherit_environment, &p)) {
	// Unable to create subprocess??
	facetize_log(s, 0, " FAILED.\n");
	facetize_log(s, 0, "Unable to create subprocess\n");

	return BRLCAD_ERROR;
    }
    while (subprocess_alive(&p)) {
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	elapsed = bu_gettime() - start;
	seconds = elapsed / 1000000.0;

	// Check for and pass along intermediate output
	char curr_out[MAXPATHLEN*10] = {'\0'};
	subprocess_read_stdout(&p, curr_out, MAXPATHLEN*10);
	if (strlen(curr_out))
	    facetize_log(s, 1, "%s", curr_out);
	char curr_err[MAXPATHLEN*10] = {'\0'};
	subprocess_read_stderr(&p, curr_err, MAXPATHLEN*10);
	if (strlen(curr_err))
	    facetize_log(s, 1, "%s", curr_err);

	if (seconds > max_time) {
	    // if we timeout, cleanup and return error
	    subprocess_terminate(&p);

	    facetize_log(s, 0, " FAILED.\n");

	    facetize_log(s, 0, "tess_run subprocess killed %g %g\n", seconds, max_time);
	    if (s->verbosity >= 0) {
		char mraw[MAXPATHLEN*10] = {'\0'};
		subprocess_read_stdout(&p, mraw, MAXPATHLEN*10);
		if (strlen(mraw))
		    facetize_log(s, 0, "%s\n", mraw);
		char mraw2[MAXPATHLEN*10] = {'\0'};
		subprocess_read_stderr(&p, mraw2, MAXPATHLEN*10);
		if (strlen(mraw2))
		    facetize_log(s, 0, "%s\n", mraw2);
	    }
	    subprocess_destroy(&p);

	    // Because we had to kill the process, there's no way of knowing
	    // whether we interrupted I/O in a state that could result in a
	    // corrupted .g file.  Restore the pre-run state of the .g file -
	    // we may have to redo some work, but this at least ensures we
	    // won't have strange garbage corrupting subsequent processing.
	    std::ifstream bakfile(wfilebak, std::ios::binary);
	    std::ofstream workfile(wfile, std::ios::binary);
	    if (!workfile.is_open() || !bakfile.is_open())
		return BRLCAD_ERROR;
	    workfile << bakfile.rdbuf();
	    workfile.close();
	    bakfile.close();


	    return BRLCAD_ERROR;
	}
    }
    int w_rc;
    if (subprocess_join(&p, &w_rc)) {
	// Unable to join??
	facetize_log(s, 0, " FAILED.\n");
	facetize_log(s, 0, "tess_run subprocess unable to join\n");
	if (s->verbosity >= 0) {
	    char mraw[MAXPATHLEN*10] = {'\0'};
	    subprocess_read_stdout(&p, mraw, MAXPATHLEN*10);
	    if (strlen(mraw))
		facetize_log(s, 0, "%s\n", mraw);
	    char mraw2[MAXPATHLEN*10] = {'\0'};
	    subprocess_read_stderr(&p, mraw2, MAXPATHLEN*10);
	    if (strlen(mraw2))
		facetize_log(s, 0, "%s\n", mraw2);
	}
	return BRLCAD_ERROR;
    }

    bu_file_delete(wfilebak.c_str());

    if (s->verbosity >= 0) {
	char mraw[MAXPATHLEN*10] = {'\0'};
	subprocess_read_stdout(&p, mraw, MAXPATHLEN*10);
	if (strlen(mraw))
	    facetize_log(s, 0, "%s\n", mraw);
	char mraw2[MAXPATHLEN*10] = {'\0'};
	subprocess_read_stderr(&p, mraw2, MAXPATHLEN*10);
	if (strlen(mraw2))
	    facetize_log(s, 0, "%s\n", mraw2);
    }

    // Needed to clean up file handles
    subprocess_destroy(&p);

    if (w_rc == BRLCAD_OK) {
	facetize_log(s, 0, " Success.\n");
    } else {
	facetize_log(s, 0, " FAILED.\n");
    }

    return (w_rc ? BRLCAD_ERROR : BRLCAD_OK);
}

int
bisect_run(struct _ged_facetize_state *s, std::vector<struct directory *> &bad_dps, std::vector<struct directory *> &inputs, const char **orig_cmd, int cmd_cnt, fastf_t max_time, int ocnt);

int
bisect_failing_inputs(struct _ged_facetize_state *s, std::vector<struct directory *> &bad_dps, std::vector<struct directory *> &inputs, const char **orig_cmd, int cmd_cnt, fastf_t max_time)
{
    std::vector<struct directory *> left_inputs;
    std::vector<struct directory *> right_inputs;
    for (size_t i = 0; i < inputs.size()/2; i++)
	left_inputs.push_back(inputs[i]);
    for (size_t i =  inputs.size()/2; i < inputs.size(); i++)
	right_inputs.push_back(inputs[i]);

    int lret = bisect_run(s, bad_dps, left_inputs, orig_cmd, cmd_cnt, max_time, left_inputs.size());
    int rret = bisect_run(s, bad_dps, right_inputs, orig_cmd, cmd_cnt, max_time, right_inputs.size());
    return lret + rret;
}

int
bisect_run(struct _ged_facetize_state *s, std::vector<struct directory *> &bad_dps, std::vector<struct directory *> &inputs, const char **orig_cmd, int cmd_cnt, fastf_t max_time, int ocnt)
{
    const char *tess_cmd[MAXPATHLEN] = {NULL};
    // The initial part of the re-run is the same.
    for (int i = 0; i < cmd_cnt; i++) {
	tess_cmd[i] = orig_cmd[i];
    }
    for (size_t i = 0; i < inputs.size(); i++) {
	tess_cmd[cmd_cnt+i] = inputs[i]->d_namep;
    }

    int ret = tess_run(s, tess_cmd, cmd_cnt+inputs.size(), max_time, ocnt);
    if (ret) {
	if (inputs.size() > 1) {
	    return bisect_failing_inputs(s, bad_dps, inputs, tess_cmd, cmd_cnt, max_time);
	}
	bad_dps.push_back(inputs[0]);
	return 1;
    }
    return 0;
}



class DpCompare
{
    public:
	bool operator()(struct directory *dp1, struct directory *dp2) {
	    // C++ priority queues return the largest element, but
	    // we want to start with the smaller elements - so we
	    // invert the large/small reporting
	    return (dp1->d_len > dp2->d_len);
	}
};

#define CMD_LEN_MAX 8000

int
_ged_facetize_leaves_tri(struct _ged_facetize_state *s, struct db_i *dbip, struct bu_ptbl *leaf_dps)
{
    // Sort dp objects by d_len using a priority queue
    std::priority_queue<struct directory *, std::vector<struct directory *>, DpCompare> pq;
    std::queue<struct directory *> q_dsp;
    std::priority_queue<struct directory *, std::vector<struct directory *>, DpCompare> q_pbot;
    for (size_t i = 0; i < BU_PTBL_LEN(leaf_dps); i++) {
	struct directory *ldp = (struct directory *)BU_PTBL_GET(leaf_dps, i);

	// If this isn't a proper BRL-CAD object, tessellation is a no-op
	if (ldp->d_major_type != DB5_MAJORTYPE_BRLCAD)
	    continue;

	// ID_DSP objects, for the moment, need to avoid NMG processing - set
	// up to handle separately
	if (ldp->d_minor_type == ID_DSP) {
	    q_dsp.push(ldp);
	    continue;
	}

	// Plate mode bots only have a realistic chance of being handled by
	// the plate to vol conversion method, but they can be quite slow
	// and will run into max-time limitations if they are large.  Separate
	// the large ones out - we will treat their handling like a fallback method and
	// be more tolerant of time
	if (ldp->d_minor_type == ID_BOT) {
	    struct rt_db_internal intern;
	    RT_DB_INTERNAL_INIT(&intern);
	    if (rt_db_get_internal(&intern, ldp, dbip, NULL, &rt_uniresource) < 0) {
		pq.push(ldp);
		continue;
	    }
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)(intern.idb_ptr);
	    int propVal = (int)rt_bot_propget(bot, "type");
	    // Plate mode BoTs need an explicit volume representation
	    if (propVal == RT_BOT_PLATE || propVal == RT_BOT_PLATE_NOCOS) {
		q_pbot.push(ldp);
		continue;
	    }
	}

	// Standard case
	pq.push(ldp);
    }

    if (pq.empty() && q_dsp.empty() && q_pbot.empty()) {
	bu_log("Note: no viable objects for tessellation found.\n");
	return BRLCAD_OK;
    }

    // Build up the path to the ged_exec executable
    char tess_exec[MAXPATHLEN];
    bu_dir(tess_exec, MAXPATHLEN, BU_DIR_BIN, "ged_exec", BU_DIR_EXT, NULL);

    // Set up a priority order of methods to try when processing primitives.
    std::vector<std::string> avail_methods = tess_avail_methods();
    if (avail_methods.size() == 0) {
	bu_log("No methods for tessellation found.\n");
	bu_dirclear(s->wdir);
	return BRLCAD_ERROR;
    }

    method_options_t *mo = (method_options_t*)s->method_opts;
    std::queue<std::string> method_flags;
    std::queue<std::string> method_flags_bak;
    for (size_t i = 0; i < mo->methods.size(); i++) {
	std::string cmethod = mo->methods[i];
	if (std::find(avail_methods.begin(), avail_methods.end(), cmethod) != avail_methods.end()) {
	    method_flags.push(cmethod);
	} else {
	    bu_log("Warning: user requested %s tessellation method not found.\n", cmethod.c_str());
	}
    }

    if (mo->methods.size() && !method_flags.size()) {
	bu_log("Error: all user requested tessellation methods unsupported.\n");
	bu_dirclear(s->wdir);
	return BRLCAD_ERROR;
    }

    if (!method_flags.size() && avail_methods.size()) {
	for (size_t i = 0; i < avail_methods.size(); i++) {
	    method_flags.push(avail_methods[i]);
	}
    }

    method_flags_bak = method_flags;

    // We want the subprocess to be using the same cache directory
    // as the parent
    char lcache[MAXPATHLEN] = {0};
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CACHE, NULL);


    // Call ged_exec to produce evaluated solids.
    // First step is to build up the command to run
    std::vector<std::string> failed_dps;
    std::string mstrpp;
    int l_max_time;
    struct bu_vls method_str = BU_VLS_INIT_ZERO;
    struct bu_vls method_opts_str = BU_VLS_INIT_ZERO;
    const char *tess_cmd[MAXPATHLEN] = {NULL};
    int method_ind = 5;
    int method_opt_ind = 7;
    tess_cmd[ 0] = tess_exec;
    tess_cmd[ 1] = "facetize_process";
    tess_cmd[ 2] = "-O";
    tess_cmd[ 3] = bu_vls_cstr(s->wfile);
    tess_cmd[ 4] = "--methods";
    tess_cmd[ 5] = NULL;
    tess_cmd[ 6] = "--method-opts";
    tess_cmd[ 7] = NULL;
    tess_cmd[ 8] = "--cache-dir";
    tess_cmd[ 9] = lcache;
    int cmd_fixed_cnt = 10;
    while (!pq.empty()) {
	int obj_cnt = 0;

	// Starting a new round of object processing - reset method flags
	method_flags = method_flags_bak;

	// There are a number of methods that can be tried.  We try them in priority
	// order, timing out if one of them goes too long.
	mstrpp = method_flags.front();
	method_flags.pop();
	bu_vls_sprintf(&method_str, "%s", mstrpp.c_str());
	tess_cmd[method_ind] = bu_vls_cstr(&method_str);
	// Each method has its own default (or possibly user set) time limit
	l_max_time = mo->max_time[mstrpp];
	// Get defined options for this particular method
	bu_vls_sprintf(&method_opts_str, "%s", mo->method_optstr(mstrpp, dbip).c_str());
	tess_cmd[method_opt_ind] = bu_vls_cstr(&method_opts_str);

	std::vector<struct directory *> dps;
	std::vector<struct directory *> bad_dps;
	struct bu_vls cmd = BU_VLS_INIT_ZERO;
	for (int i = 0; i < cmd_fixed_cnt; i++)
	    bu_vls_printf(&cmd, "%s ", tess_cmd[i]);
	while (bu_vls_strlen(&cmd) < CMD_LEN_MAX) {
	    if (pq.empty() || cmd_fixed_cnt+dps.size() == MAXPATHLEN)
		break;
	    struct directory *ldp = pq.top();
	    if ((bu_vls_strlen(&cmd) + strlen(ldp->d_namep)) > CMD_LEN_MAX) {
		// This would be too long -  we've listed all we can
		break;
	    }
	    obj_cnt++;
	    pq.pop();
	    dps.push_back(ldp);
	    bu_vls_printf(&cmd, "%s ", ldp->d_namep);
	}
	bu_vls_free(&cmd);

	// We have the list of objects to feed the process - now, trigger
	// the runs with as many methods as it takes to facetize all the
	// primitives
	int err_cnt = 0;
	while (bu_vls_strlen(&method_str)) {
	    if (BU_STR_EQUAL(bu_vls_cstr(&method_str), "NMG")) {
		err_cnt = bisect_run(s, bad_dps, dps, tess_cmd, cmd_fixed_cnt, l_max_time, obj_cnt);
	    } else {
		// If we're in fallback territory, process individually rather
		// than doing the bisect - at least for now, those methods are
		// much more expensive and likely to fail as compared to NMG.
		for (size_t i = 0; i < dps.size(); i++) {
		    tess_cmd[cmd_fixed_cnt] = dps[i]->d_namep;
		    int tess_ret = tess_run(s, tess_cmd, cmd_fixed_cnt + 1, l_max_time, 1);
		    if (tess_ret != BRLCAD_OK) {
			bad_dps.push_back(dps[i]);
			err_cnt++;
		    }
		}
	    }

	    // If we dealt successfully with everything, we're done
	    if (!err_cnt)
		break;

	    if (method_flags.size()) {
		// If we still have available methods to try, go another round
		err_cnt = 0;
		mstrpp = method_flags.front();
		method_flags.pop();
		bu_vls_sprintf(&method_str, "%s", mstrpp.c_str());
		tess_cmd[method_ind] = bu_vls_cstr(&method_str);
		// Each method has its own default (or possibly user set) time limit
		l_max_time = mo->max_time[mstrpp];
		// Get defined options for this particular method
		bu_vls_sprintf(&method_opts_str, "\"%s\"", mo->method_optstr(mstrpp, dbip).c_str());
		tess_cmd[method_opt_ind] = bu_vls_cstr(&method_opts_str);
		dps = bad_dps;
		bad_dps.clear();
	    } else {
		// All done - nothing left to try
		bu_vls_trunc(&method_str, 0);
		tess_cmd[method_ind] = NULL;
	    }
	}

	if (err_cnt || bad_dps.size() > 0) {
	    // If we tried all the active methods and still had failures, we have an
	    // error.  We'll keep trying to process all the leaves, since we want to
	    // get a full picture of what the issues with the conversion are, but
	    // we need to record these as a full-on failure.
	    for (size_t i = 0; i < bad_dps.size(); i++)
		failed_dps.push_back(std::string(bad_dps[i]->d_namep));
	}
    }

    while (!q_dsp.empty()) {
	bu_vls_sprintf(&method_str, "CM");
	tess_cmd[method_ind] = bu_vls_cstr(&method_str);
	mstrpp = std::string("CM");
	l_max_time = mo->max_time[mstrpp];
	bu_vls_sprintf(&method_opts_str, "\"%s\"", mo->method_optstr(mstrpp, dbip).c_str());
	tess_cmd[method_opt_ind] = bu_vls_cstr(&method_opts_str);
	std::vector<struct directory *> dps;
	struct bu_vls cmd = BU_VLS_INIT_ZERO;
	for (int i = 0; i < cmd_fixed_cnt; i++)
	    bu_vls_printf(&cmd, "%s ", tess_cmd[i]);
	while (bu_vls_strlen(&cmd) < CMD_LEN_MAX) {
	    if (q_dsp.empty() || cmd_fixed_cnt+dps.size() == MAXPATHLEN)
		break;
	    struct directory *ldp = q_dsp.front();
	    if ((bu_vls_strlen(&cmd) + strlen(ldp->d_namep)) > CMD_LEN_MAX) {
		// This would be too long -  we've listed all we can
		break;
	    }
	    q_dsp.pop();
	    dps.push_back(ldp);
	    bu_vls_printf(&cmd, "%s ", ldp->d_namep);
	}
	bu_vls_free(&cmd);

	// We have the list of objects to feed the process - now, trigger
	// the runs with as many methods as it takes to facetize all the
	// primitives
	for (size_t i = 0; i < dps.size(); i++) {
	    tess_cmd[cmd_fixed_cnt] = dps[i]->d_namep;
	    int err_cnt = tess_run(s, tess_cmd, cmd_fixed_cnt + 1, l_max_time, 1);
	    if (err_cnt)
		failed_dps.push_back(std::string(dps[i]->d_namep));
	}
    }

    while (!q_pbot.empty()) {
	bu_vls_sprintf(&method_str, "NMG");
	tess_cmd[method_ind] = bu_vls_cstr(&method_str);
	mstrpp = std::string("NMG");
	l_max_time = mo->plate_max_time;
	bu_vls_sprintf(&method_opts_str, "\"%s\"", mo->method_optstr(mstrpp, dbip).c_str());
	tess_cmd[method_opt_ind] = bu_vls_cstr(&method_opts_str);


	std::vector<struct directory *> dps;
	std::vector<struct directory *> bad_dps;
	struct bu_vls cmd = BU_VLS_INIT_ZERO;
	for (int i = 0; i < cmd_fixed_cnt; i++)
	    bu_vls_printf(&cmd, "%s ", tess_cmd[i]);
	int obj_cnt = 0;
	while (bu_vls_strlen(&cmd) < CMD_LEN_MAX) {
	    if (q_pbot.empty() || cmd_fixed_cnt+dps.size() == MAXPATHLEN)
		break;
	    struct directory *ldp = q_pbot.top();
	    if ((bu_vls_strlen(&cmd) + strlen(ldp->d_namep)) > CMD_LEN_MAX) {
		// This would be too long -  we've listed all we can
		break;
	    }
	    obj_cnt++;
	    q_pbot.pop();
	    dps.push_back(ldp);
	    bu_vls_printf(&cmd, "%s ", ldp->d_namep);
	}
	bu_vls_free(&cmd);


	int err_cnt = bisect_run(s, bad_dps, dps, tess_cmd, cmd_fixed_cnt, l_max_time * dps.size(), obj_cnt);
	if (err_cnt) {
	    // If we couldn't handle the plate mode conversion, we can't do the
	    // boolean evaluation
	    facetize_log(s, 0, "Plate mode conversion wasn't able to complete\n");
	    return BRLCAD_ERROR;
	}
    }

    if (failed_dps.size()) {
	// As the parent process, we can know when we've run out of options
       // to try.  If we get there, flag the solid in the working copy so
       // the summary knows to report it.
       struct db_i *cdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
       if (cdbip) {
           db_dirbuild(cdbip);
           db_update_nref(cdbip, &rt_uniresource);
           for (size_t i = 0; i < failed_dps.size(); i++) {
	       struct directory *dp = db_lookup(cdbip, failed_dps[i].c_str(), LOOKUP_QUIET);
	       if (!dp)
		   continue;
               struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
               db5_get_attributes(cdbip, &avs, dp);
               (void)bu_avs_add(&avs, FACETIZE_METHOD_ATTR, "FAIL");
               (void)db5_update_attributes(dp, &avs, cdbip);
           }
           db_close(cdbip);
       }
       return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_ged_facetize_booleval_tri(struct _ged_facetize_state *s, struct db_i *dbip, struct rt_wdb *wdbp, int argc, const char **argv, const char *oname, bool output_to_working)
{
    union tree *ftree;
    if (!dbip || !wdbp || !argv || !oname)
	return BRLCAD_ERROR;

    if (s->verbosity == 0) {
	if (argc == 1) {
	    bu_log("%s: evaluating booleans...\n", argv[0]);
	} else {
	    bu_log("Evaluating booleans for the trees of %d input objects...\n", argc);
	}
    }

    // Unlike the -r flag processing regions, where each individual region
    // processed is semantically a single solid , there is no guarantee in
    // general that the output is representing a single, well behaved solid.
    // Consequently, thin volumes and close faces may be expected features and
    // it's more problematic to do the fixup check.  However, if we were given
    // a single primitive or region, those outputs should satisfy the fixup
    // criteria.
    bool do_fixup = false;
    if (argc == 1 && !s->no_fixup) {
	struct directory *dp = db_lookup(dbip, argv[0], LOOKUP_QUIET);
	if ((dp->d_flags & RT_DIR_REGION) || (dp->d_flags & RT_DIR_SOLID))
	    do_fixup = true;
    }

    // If we don't have inputs that can be fed to db_walk_tree it will produce
    // an error, which we don't want.  What we do want in such a case - where
    // there are NO valid walking candidates - is to indicate that there wasn't
    // a logic failure.  That means we need an empty bot to be generated - i.e.
    // we don't want to trigger the db_walk_tree error path.
    int ac = 0;
    const char **av = (const char **)bu_calloc(argc, sizeof(const char *), "av");
    for (int i = 0; i < argc; i++) {
	struct directory *dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	if (dp->d_flags & RT_DIR_COMB || dp->d_flags & RT_DIR_SOLID) {
	    av[ac] = argv[i];
	    ac++;
	}
    }

    if (ac) {
	s->error_flag = 0;
	struct db_tree_state init_state;
	db_init_db_tree_state(&init_state, dbip, wdbp->wdb_resp);
	/* Establish tolerances */
	init_state.ts_ttol = &wdbp->wdb_ttol;
	init_state.ts_tol = &wdbp->wdb_tol;
	init_state.ts_m = NULL;
	s->facetize_tree = (union tree *)0;
	int i = 0;
	if (!BU_SETJUMP) {
	    /* try */
	    i = db_walk_tree(dbip, argc, argv,
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

	// Something went wrong - not just empty geometry, but an actual error.
	// Do not generate a BoT, empty or otherwise.
	if (i < 0 || s->error_flag) {
	    bu_free(av, "av");
	    facetize_log(s, 0, "FAILED.\n");
	    return BRLCAD_ERROR;
	}
    }
    bu_free(av, "av");

    struct db_i *odbip = (output_to_working) ? dbip : s->dbip;

    // We don't have a tree - unless we've been told not to, prepare an empty BoT
    if (!s->facetize_tree && !s->no_empty) {
	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->bot_flags = 0;
	bot->num_vertices = 0;
	bot->num_faces = 0;
	bot->vertices = NULL;
	bot->faces = NULL;
	if (_ged_facetize_write_bot(odbip, bot, oname, s->verbosity) != BRLCAD_OK) {
	    facetize_log(s, 0, "FAILED.\n");
	    return BRLCAD_ERROR;
	}
	facetize_log(s, 0, " Success.\n");
	return BRLCAD_OK;
    }

    // Third stage is to execute the boolean operations
    ftree = rt_booltree_evaluate(s->facetize_tree, &RTG.rtg_vlfree, &wdbp->wdb_tol, &rt_uniresource, &manifold_do_bool, 0, (void *)s);
    if (!ftree) {
	return BRLCAD_ERROR;
    }

    if (ftree->tr_d.td_d) {
	manifold::Manifold *om = (manifold::Manifold *)ftree->tr_d.td_d;
	manifold::MeshGL64 rmesh = om->GetMeshGL64();
	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->bot_flags = 0;
	bot->num_vertices = (int)rmesh.vertProperties.size()/3;
	bot->num_faces = (int)rmesh.triVerts.size()/3;
	bot->vertices = (double *)calloc(rmesh.vertProperties.size(), sizeof(double));
	bot->faces = (int *)calloc(rmesh.triVerts.size(), sizeof(int));
	for (size_t j = 0; j < rmesh.vertProperties.size(); j++)
	    bot->vertices[j] = rmesh.vertProperties[j];
	for (size_t j = 0; j < rmesh.triVerts.size(); j++)
	    bot->faces[j] = rmesh.triVerts[j];
	delete om;
	ftree->tr_d.td_d = NULL;

	// If we have a manifold_mesh, write it out as a bot
	if (_ged_facetize_write_bot(odbip, bot, oname, s->verbosity) != BRLCAD_OK) {
	    facetize_log(s, 0, "FAILED.\n");
	    return BRLCAD_ERROR;
	}
    } else {
	// Evaluation didn't produce a tree - unless we've been told not to,
	// prepare an empty BoT
	if (!s->no_empty) {
	    struct rt_bot_internal *bot;
	    BU_GET(bot, struct rt_bot_internal);
	    bot->magic = RT_BOT_INTERNAL_MAGIC;
	    bot->mode = RT_BOT_SOLID;
	    bot->orientation = RT_BOT_CCW;
	    bot->thickness = NULL;
	    bot->face_mode = (struct bu_bitv *)NULL;
	    bot->bot_flags = 0;
	    bot->num_vertices = 0;
	    bot->num_faces = 0;
	    bot->vertices = NULL;
	    bot->faces = NULL;
	    if (_ged_facetize_write_bot(odbip, bot, oname, s->verbosity) != BRLCAD_OK) {
		facetize_log(s, 0, "FAILED.\n");
		return BRLCAD_ERROR;
	    }
	    facetize_log(s, 0, "Success.\n");
	    return BRLCAD_OK;
	}
    }

    // If we meet the conditions, apply the fixup logic
    if (do_fixup) {
	struct directory *dp = db_lookup(dbip, argv[0], LOOKUP_QUIET);
	if ((dp->d_flags & RT_DIR_REGION) || (!(dp->d_flags & RT_DIR_COMB))) {
	    struct directory *bot_dp = db_lookup(odbip, oname, LOOKUP_QUIET);
	    struct rt_bot_internal *nbot = bot_fixup(s, odbip, bot_dp, oname);
	    if (nbot) {
		// Write out new version of BoT
		db_delete(odbip, bot_dp);
		db_dirdelete(odbip, bot_dp);
		if (_ged_facetize_write_bot(odbip, nbot, oname, s->verbosity) != BRLCAD_OK) {
		    facetize_log(s, 0, "FAILED.\n");
		}
	    }
	}
    }

    facetize_log(s, 0, "Success.\n");
    return BRLCAD_OK;
}

int
_ged_facetize_booleval(struct _ged_facetize_state *s, int argc, struct directory **dpa, const char *oname, bool output_to_working, bool cleanup)
{
    int ret = BRLCAD_OK;

    if (!s)
	return BRLCAD_ERROR;

    if (!argc || !dpa)
	return BRLCAD_ERROR;

    struct db_i *dbip = s->dbip;
    struct rt_wdb *wwdbp;

    /* First stage is to process the primitive instances.  We include points in
     * this even though they do not define a volume in order to allow for the
     * possibility of applying the alternative pnt based reconstruction methods
     * to their data. */
    const char *sfilter = "-type shape -or -type pnts";
    struct bu_ptbl leaf_dps = BU_PTBL_INIT_ZERO;
    if (db_search(&leaf_dps, DB_SEARCH_RETURN_UNIQ_DP, sfilter, argc, dpa, dbip, NULL) < 0) {
	// Empty input - nothing to facetize.
	return BRLCAD_OK;
    }

    /* OK, we have work to do. Set up a working copy of the .g file. */
    if (_ged_facetize_working_file_setup(s, &leaf_dps) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (_ged_facetize_leaves_tri(s, dbip, &leaf_dps))
	return BRLCAD_ERROR;

    // Re-open working .g copy after BoTs have replaced CSG solids and perform
    // the tree walk to set up Manifold data.
    struct db_i *wdbip = db_open(bu_vls_cstr(s->wfile), (output_to_working) ? DB_OPEN_READWRITE :  DB_OPEN_READONLY);
    if (!wdbip) {
	bu_dirclear(s->wdir);
	return BRLCAD_ERROR;
    }
    if (db_dirbuild(wdbip) < 0)
	return BRLCAD_ERROR;

    db_update_nref(wdbip, &rt_uniresource);

    // Need wdbp in the next two stages for tolerances
    wwdbp = wdb_dbopen(wdbip, RT_WDB_TYPE_DB_DEFAULT);

    /* Second stage is to prepare Manifold versions of the instances of the BoT
     * obj conversions generated by stage 1.  This is where matrix placement
     * is handled. */
    // Prepare argc/argv array for db_walk_tree
    const char **av = (const char **)bu_calloc(argc+1, sizeof(char *), "av");
    for (int i = 0; i < argc; i++) {
	av[i] = dpa[i]->d_namep;
    }

    if (_ged_facetize_booleval_tri(s, wdbip, wwdbp, argc, av, oname, output_to_working) != BRLCAD_OK) {
	if (s->verbosity >= 0) {
	    bu_log("FACETIZE: failed to generate %s\n", oname);
	}
    }

    bu_free(av, "av");
    db_close(wdbip);

    if (cleanup)
	bu_dirclear(s->wdir);

    bu_ptbl_free(&leaf_dps);

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

