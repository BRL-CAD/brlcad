/*                     R E G I O N S . C P P
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
/** @file libged/facetize.cpp
 *
 * The facetize command.
 *
 */

#include "common.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include <string.h>

#include "bu/app.h"
#include "bu/path.h"
#include "bu/env.h"
#include "rt/search.h"
#include "raytrace.h"
#include "wdb.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

int
_ged_facetize_regions(struct _ged_facetize_state *s, int argc, const char **argv)
{
    struct ged *gedp = s->gedp;
    int ret = BRLCAD_OK;
    struct db_i *dbip = gedp->dbip;

    /* Used the libged tolerances */
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    s->tol = &(wdbp->wdb_ttol);

    if (!argc) return BRLCAD_ERROR;

    struct directory **dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    int newobjcnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
    if (newobjcnt != 1) {
	if (!newobjcnt)
	    bu_vls_printf(gedp->ged_result_str, "Need non-existent output comb name.");
	if (newobjcnt)
	    bu_vls_printf(gedp->ged_result_str, "More than one non-existent object specified in region processing mode, aborting.");
	return BRLCAD_ERROR;
    }

    const char *oname = argv[argc-1];
    argc--;

    const char *active_regions = "( -type r ! -below -type r )";
    struct bu_ptbl *ar = NULL;
    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, active_regions, argc, dpa, dbip, NULL) < 0) {
	if (s->verbosity) {
	    bu_log("Problem searching for active regions - aborting.\n");
	}
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }
    if (!BU_PTBL_LEN(ar)) {
	/* No active regions (unlikely but technically possible), nothing to do */
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "dpa");
	return BRLCAD_OK;
    }

    // For the working file setup, we need to check the solids to see if any
    // supporting files need to be copied
    const char *active_solids = "! -type comb";
    struct bu_ptbl *as = NULL;
    BU_ALLOC(as, struct bu_ptbl);
    if (db_search(as, DB_SEARCH_RETURN_UNIQ_DP, active_solids, argc, dpa, dbip, NULL) < 0) {
	if (s->verbosity) {
	    bu_log("Problem searching for active solids - aborting.\n");
	}
	bu_ptbl_free(as);
	bu_free(as, "as table");
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }
    if (!BU_PTBL_LEN(as)) {
	/* No active solids (unlikely but technically possible), nothing to do */
	bu_ptbl_free(as);
	bu_free(as, "as table");
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }

    char *wdir= (char *)bu_calloc(MAXPATHLEN, sizeof(char), "wfile");
    char *wfile = (char *)bu_calloc(MAXPATHLEN, sizeof(char), "wfile");
    char kfname[MAXPATHLEN];

    /* Figure out the working .g filename */
    struct bu_vls wfilename = BU_VLS_INIT_ZERO;
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    struct bu_vls dname = BU_VLS_INIT_ZERO;
    char rfname[MAXPATHLEN];
    bu_file_realpath(dbip->dbi_filename, rfname);

    // Get the root filename, so we can give the working file a relatable name
    bu_path_component(&bname, rfname, BU_PATH_BASENAME);

    // Hash the path string and construct
    unsigned long long hash_num = bu_data_hash((void *)bu_vls_cstr(&bname), bu_vls_strlen(&bname));
    bu_vls_sprintf(&dname, "facetize_regions_%llu", hash_num);
    bu_vls_sprintf(&wfilename, "facetize_regions_%s", bu_vls_cstr(&bname));
    bu_vls_free(&bname);

    // Have filename, get a location in the cache directory
    bu_dir(wdir, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&dname), NULL);
    bu_dir(wfile, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&dname), bu_vls_cstr(&wfilename), NULL);
    bu_vls_printf(&wfilename, "_keep");
    bu_dir(kfname, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&dname), bu_vls_cstr(&wfilename), NULL);
    bu_vls_free(&dname);
    bu_vls_free(&wfilename);

    // Set up working file.  We will reuse this for each region->bot conversion.
    // We pass in the list of all active solids so any necessary supporting data
    // files also get copied over.
    if (_ged_facetize_working_file_setup(&wfile, &wdir, dbip, as, s->resume) != BRLCAD_OK) {
	bu_ptbl_free(as);
	bu_free(as, "as table");
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    // Done with solids table
    bu_ptbl_free(as);
    bu_free(as, "as table");


    // If we have any solids in the hierarchies with only combs above them,
    // they are "implicit" regions and must be facetized individually.
    const char *implicit_regions = "( ! -below -type r ! -type comb )";
    struct bu_ptbl *ir = NULL;
    BU_ALLOC(ir, struct bu_ptbl);
    if (db_search(ir, DB_SEARCH_RETURN_UNIQ_DP, implicit_regions, argc, dpa, dbip, NULL) < 0) {
	if (s->verbosity) {
	    bu_log("Problem searching for implicit regions - aborting.\n");
	}
	bu_ptbl_free(ar);
	bu_free(wdir, "wdir");
	bu_free(wfile, "wfile");
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }
    bool have_failure = false;
    if (BU_PTBL_LEN(ir)) {

	if (_ged_facetize_leaves_tri(s, wfile, wdir, dbip, ir) != BRLCAD_OK) {
	    have_failure = true;
	}
    }
    bu_ptbl_free(ir);
    bu_free(ir, "ir table");

    // For proper regions, we're reducing the CSG tree to a single BoT and
    // swapping that BoT in under the region comb
    for (size_t i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *dpw[2] = {NULL};
	dpw[0] = (struct directory *)BU_PTBL_GET(ar, i);

	// Get a name for the region's output BoT
	bu_vls_sprintf(&bname, "%s.bot", dpw[0]->d_namep);
	struct db_i *cdbip = db_open(wfile, DB_OPEN_READONLY);
	db_dirbuild(cdbip);
	db_update_nref(cdbip, &rt_uniresource);
	struct directory *dcheck = db_lookup(cdbip, bu_vls_cstr(&bname), LOOKUP_QUIET);
	if (dcheck != RT_DIR_NULL)
	    bu_vls_incr(&bname, NULL, NULL, &_db_uniq_test, (void *)cdbip);
	db_close(cdbip);

	if (_ged_facetize_booleval(s, 1, dpw, bu_vls_cstr(&bname), wdir, wfile, true) == BRLCAD_OK) {
	    // Replace the region's comb tree with the new BoT
	    struct db_i *wdbip = db_open(wfile, DB_OPEN_READWRITE);
	    db_dirbuild(wdbip);
	    db_update_nref(wdbip, &rt_uniresource);
	    struct directory *wdp = db_lookup(wdbip, dpw[0]->d_namep, LOOKUP_QUIET);
	    struct rt_db_internal intern;
	    struct rt_comb_internal *comb;
	    rt_db_get_internal(&intern, wdp, wdbip, NULL, &rt_uniresource);
	    comb = (struct rt_comb_internal *)(&intern)->idb_ptr;
	    RT_CK_COMB(comb);
	    db_free_tree(comb->tree, &rt_uniresource);
	    union tree *tp;
	    struct rt_tree_array *tree_list;
	    BU_GET(tree_list, struct rt_tree_array);
	    tree_list[0].tl_op = OP_UNION;
	    BU_GET(tp, union tree);
	    RT_TREE_INIT(tp);
	    tree_list[0].tl_tree = tp;
	    tp->tr_l.tl_op = OP_DB_LEAF;
	    tp->tr_l.tl_name = bu_strdup(bu_vls_cstr(&bname));
	    tp->tr_l.tl_mat = NULL;
	    comb->tree = (union tree *)db_mkgift_tree(tree_list, 1, &rt_uniresource);
	    struct rt_wdb *wwdbp = wdb_dbopen(wdbip, RT_WDB_TYPE_DB_DEFAULT);
	    wdb_put_internal(wwdbp, wdp->d_namep, &intern, 1.0);
	    db_update_nref(wdbip, &rt_uniresource);


	    // We've written out the evaluated BoT, but there is a chance we have degenerately
	    // thin volumes where we had coplanar interactions.  Check with the raytracer, and
	    // if we find such a case try to remove the degenerate faces and produce a new mesh.
	    struct directory *bot_dp = db_lookup(wdbip, bu_vls_cstr(&bname), LOOKUP_QUIET);
	    if (!bot_dp) {
		db_close(wdbip);
		continue;
	    }
	    struct rt_db_internal bot_intern;
	    RT_DB_INTERNAL_INIT(&bot_intern);
	    if (rt_db_get_internal(&bot_intern, bot_dp, wdbip, NULL, &rt_uniresource) < 0) {
		db_close(wdbip);
		continue;
	    }
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)(bot_intern.idb_ptr);
	    struct rt_i *rtip = rt_new_rti(wdbip);
	    rt_gettree(rtip, bu_vls_cstr(&bname));
	    rt_prep(rtip);
	    struct bu_ptbl tfaces = BU_PTBL_INIT_ZERO;
	    int have_thin_faces = rt_bot_thin_check(&tfaces, bot, rtip, VUNITIZE_TOL, 1);
	    rt_free_rti(rtip);

	    // If we have work to do, we'll need a new BoT
	    struct rt_bot_internal *nbot = NULL;

	    if (have_thin_faces) {

		// First order of business - get the problematic faces out of
		// the mesh
		nbot = rt_bot_remove_faces(&tfaces, bot);

		// If we took away manifoldness removing faces (likely) we need
		// to try and rebuild it.
		if (nbot && nbot->num_faces) {
		    struct rt_bot_internal *rbot = NULL;
		    struct rt_bot_repair_info rs = RT_BOT_REPAIR_INFO_INIT;
		    rs.max_hole_area_percent = 30;
		    int repair_result = rt_bot_repair(&rbot, nbot, &rs);
		    if (repair_result < 0) {
			bu_log("%s removed %zd thin faces, but repair failed.  Retaining manifold result.\n", bu_vls_cstr(&bname), BU_PTBL_LEN(&tfaces));
			// The repair didn't succeed.  That means we weren't able
			// to produce a manifold mesh after removing the thin
			// triangles.  In that situation, we return the manifold
			// result we do have, thin triangles or not, rather than
			// produce something invalid.
			rt_bot_internal_free(nbot);
			BU_PUT(nbot, struct rt_bot_internal);
			nbot = NULL;
		    } else {
			// If we produced a new repaired mesh, replace nbot with
			// the final result.
			if (rbot && rbot != nbot) {
			    rt_bot_internal_free(nbot);
			    BU_PUT(nbot, struct rt_bot_internal);
			    nbot = rbot;
			}
		    }
		}
	    }
	    bu_ptbl_free(&tfaces);

	    // Done with bot_intern
	    rt_db_free_internal(&bot_intern);

	    if (nbot) {
		// Write out new version of BoT
		db_delete(wdbip, bot_dp);
		db_dirdelete(wdbip, bot_dp);
		if (_ged_facetize_write_bot(wdbip, nbot, bu_vls_cstr(&bname), s->verbosity) != BRLCAD_OK) {
		    bu_log("Error writing out finalized version of %s\n", bu_vls_cstr(&bname));
		}
	    }

	    db_close(wdbip);

	} else {
	    have_failure = true;
	}
    }

    // If any of the regions failed (implicit or explicit), don't keep or
    // dbconcat - the operation wasn't a full success.
    if (have_failure) {
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	bu_free(wdir, "wdir");
	bu_free(wfile, "wfile");
	return BRLCAD_ERROR;
    }

    // keep active regions into .g copy
    struct ged *wgedp = ged_open("db", wfile, 1);
    if (!wgedp) {
    	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	bu_free(wdir, "wdir");
	bu_free(wfile, "wfile");
	return BRLCAD_ERROR;
    }
    const char **av = (const char **)bu_calloc(argc+10, sizeof(const char *), "av");
    av[0] = "keep";
    av[1] = kfname;
    for (int i = 0; i < argc; i++) {
	av[i+2] = argv[i];
    }
    av[argc+2] = NULL;
    ged_exec(wgedp, argc+2, av);
    ged_close(wgedp);

    /* Capture the current tops list.  If we're not doing an in-place overwrite, we
     * need to know what the new top level objects are for the assembly of the
     * final comb. */
    struct directory **tlist = NULL;
    size_t tcnt = db_ls(s->gedp->dbip, DB_LS_TOPS, NULL, &tlist);
    std::set<std::string> otops;
    for (size_t i = 0; i < tcnt; i++) {
	otops.insert(std::string(tlist[i]->d_namep));
    }
    bu_free(tlist, "tlist");
    tlist = NULL;


    // dbconcat output .g into original .g - either using -O to overwrite
    // or allowing dbconcat to suffix the names depending on whether we're
    // in-place or not.
    av[0] = "dbconcat";
    av[1] = (s->in_place) ? "-O" : "-L";
    av[2] = kfname;
    av[3] = "facetize_"; // TODO - customize
    av[4] = NULL;
    ged_exec(s->gedp, 4, av);
    bu_free(av, "av");

    /* Done importing stuff - update nref. */
    db_update_nref(dbip, &rt_uniresource);


    /* Capture the new tops list. */
    tcnt = db_ls(s->gedp->dbip, DB_LS_TOPS, NULL, &tlist);
    std::set<std::string> ntops;
    for (size_t i = 0; i < tcnt; i++) {
	ntops.insert(std::string(tlist[i]->d_namep));
    }
    bu_free(tlist, "tlist");


    /* Find the new top level objects from dbconcat */
    std::set<std::string> new_tobjs;
    std::set_difference(ntops.begin(), ntops.end(), otops.begin(), otops.end(), std::inserter(new_tobjs, new_tobjs.begin()));

    /* Make a new comb to hold the output */
    struct wmember wcomb;
    BU_LIST_INIT(&wcomb.l);
    struct rt_wdb *cwdbp = wdb_dbopen(s->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    std::set<std::string>::iterator s_it;
    for (s_it = new_tobjs.begin(); s_it != new_tobjs.end(); ++s_it) {
	(void)mk_addmember(s_it->c_str(), &(wcomb.l), NULL, DB_OP_UNION);
    }
    mk_lcomb(cwdbp, oname, &wcomb, 0, NULL, NULL, NULL, 0);

    /* Done importing stuff - update nref. */
    db_update_nref(dbip, &rt_uniresource);

    bu_ptbl_free(ar);
    bu_free(ar, "ar table");
    bu_free(dpa, "free dpa");
    bu_dirclear(wdir);
    bu_free(wdir, "wdir");
    bu_free(wfile, "wfile");
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

