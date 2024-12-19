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
 * Logic implementing the per-region facetize mode.
 *
 * Note:  we deliberately manage this somewhat differently from the "convert
 * everything to one BoT" case to minimize the number of subprocesses we have
 * to launch.  For very large models, if we just treat each region like its a
 * complete conversion, we may end up launching too many subprocesses and run
 * into operating system limitations.
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
    int ret = BRLCAD_OK;
    struct db_i *dbip = s->dbip;
    struct bu_list *vlfree = &rt_vlfree;

    /* Used the libged tolerances */
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    s->tol = &(wdbp->wdb_ttol);

    if (!argc) return BRLCAD_ERROR;

    struct directory **dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    int newobjcnt = _ged_sort_existing_objs(dbip, argc, argv, dpa);
    if (newobjcnt != 1) {
	if (!newobjcnt)
	    bu_vls_printf(s->gedp->ged_result_str, "Need non-existent output comb name.");
	if (newobjcnt)
	    bu_vls_printf(s->gedp->ged_result_str, "More than one non-existent object specified in region processing mode, aborting.");
	return BRLCAD_ERROR;
    }

    const char *oname = argv[argc-1];
    argc--;

    // Before we go any further, see if we actually have regions in the
    // specified input(s).
    const char *active_regions = "( -type r ! -below -type r )";
    struct bu_ptbl *ar = NULL;
    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, active_regions, argc, dpa, dbip, NULL, NULL, NULL) < 0) {
	if (s->verbosity >= 0) {
	    bu_log("regions.cpp:%d Problem searching for active regions - aborting.\n", __LINE__);
	}
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }

    // If we have none, just treat this as a normal facetize operation.
    if (!BU_PTBL_LEN(ar)) {
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");

	/* If we're doing an NMG output, use the old-school libnmg booleval */
	if (s->make_nmg) {
	    if (!s->in_place) {
		ret = _ged_facetize_nmgeval(s, argc, argv, oname);
		bu_free(dpa, "dpa");
		return ret;
	    } else {
		for (int i = 0; i < argc; i++) {
		    const char *av[2];
		    av[0] = argv[i];
		    av[1] = NULL;
		    ret = _ged_facetize_nmgeval(s, 1, av, argv[i]);
		    if (ret == BRLCAD_ERROR) {
			bu_free(dpa, "dpa");
			return ret;
		    }
		}
		bu_free(dpa, "dpa");
		return ret;
	    }
	}

	// If we're not doing NMG, use the Manifold booleval
	if (!s->in_place) {
	    ret = _ged_facetize_booleval(s, argc, dpa, oname, false, false);
	} else {
	    for (int i = 0; i < argc; i++) {
		struct directory *idpa[2];
		idpa[0] = dpa[i];
		idpa[1] = NULL;
		ret = _ged_facetize_booleval(s, 1, (struct directory **)idpa, argv[i], false, false);
		if (ret == BRLCAD_ERROR) {
		    bu_free(dpa, "dpa");
		    return ret;
		}
	    }
	}

	// Report on the primitive processing
	facetize_primitives_summary(s);

	// After collecting info for summary, we can now clean up working files
	bu_dirclear(s->wdir);

	// Cleanup
	bu_free(dpa, "dpa");
	return ret;
    }

    // We've got something warranting region processiong. For the working file
    // setup, we need to check the solids to see if any supporting files need
    // to be copied
    const char *active_solids = "! -type comb";
    struct bu_ptbl *as = NULL;
    BU_ALLOC(as, struct bu_ptbl);
    if (db_search(as, DB_SEARCH_RETURN_UNIQ_DP, active_solids, argc, dpa, dbip, NULL, NULL, NULL) < 0) {
	if (s->verbosity >= 0) {
	    bu_log("regions.cpp:%d Problem searching for active solids - aborting.\n", __LINE__);
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

    char kfname[MAXPATHLEN];
    char tmpwfile[MAXPATHLEN];

    /* Figure out the working .g filename */
    struct bu_vls wfilename = BU_VLS_INIT_ZERO;
    // Hash the path string and construct
    unsigned long long hash_num = bu_data_hash((void *)bu_vls_cstr(s->bname), bu_vls_strlen(s->bname));
    bu_vls_sprintf(&wfilename, "facetize_regions_%s_%llu", bu_vls_cstr(s->bname), hash_num);

    // Have filename, get a location in the cache directory
    bu_dir(tmpwfile, MAXPATHLEN, s->wdir, bu_vls_cstr(&wfilename), NULL);
    bu_vls_sprintf(s->wfile, "%s", tmpwfile);
    bu_vls_printf(&wfilename, "_keep");
    bu_dir(kfname, MAXPATHLEN, s->wdir, bu_vls_cstr(&wfilename), NULL);
    bu_vls_free(&wfilename);

    // Set up working file.  We will reuse this for each region->bot conversion.
    // We pass in the list of all active solids so any necessary supporting data
    // files also get copied over.
    if (_ged_facetize_working_file_setup(s, as) != BRLCAD_OK) {
	if (s->verbosity >= 0) {
	    bu_log("regions.cpp:%d Failed to set up working file - aborting.\n", __LINE__);
	}
	bu_ptbl_free(as);
	bu_free(as, "as table");
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    // We need all the solids converted
    if (!s->make_nmg && !s->nmg_booleval) {
	if (_ged_facetize_leaves_tri(s, dbip, as)) {
	    if (s->verbosity >= 0) {
		bu_log("regions.cpp:%d Failed to tessellate all solids - aborting.\n", __LINE__);
	    }
	    bu_ptbl_free(as);
	    bu_free(as, "as table");
	    bu_ptbl_free(ar);
	    bu_free(ar, "ar table");
	    bu_free(dpa, "free dpa");
	    return BRLCAD_ERROR;
	}
    }

    // Done with solids table
    bu_ptbl_free(as);
    bu_free(as, "as table");

    // If we're going to be doing NMG outputs or NMG booleans,
    // we'll need to have a facetize_state container that has
    // info for the working .g, rather than the parent.  Set
    // up accordingly.
    struct _ged_facetize_state nmg_wstate;
    nmg_wstate.verbosity = s->verbosity;
    nmg_wstate.no_empty = s->no_empty;
    nmg_wstate.make_nmg = s->make_nmg;
    nmg_wstate.nonovlp_brep = s->nonovlp_brep;
    nmg_wstate.no_fixup= s->no_fixup;
    nmg_wstate.wdir = s->wdir;
    nmg_wstate.wfile = s->wfile;
    nmg_wstate.bname = s->bname;
    nmg_wstate.log_file = s->log_file;
    nmg_wstate.lfile = s->lfile;
    nmg_wstate.regions = s->regions;
    nmg_wstate.resume = s->resume;
    nmg_wstate.in_place = s->in_place;
    nmg_wstate.nmg_booleval = s->nmg_booleval;
    nmg_wstate.max_time = s->max_time;
    nmg_wstate.max_pnts = s->max_pnts;
    nmg_wstate.prefix = s->prefix;
    nmg_wstate.suffix = s->suffix;
    nmg_wstate.tol = s->tol;
    nmg_wstate.nonovlp_threshold = s->nonovlp_threshold;
    nmg_wstate.solid_suffix = s->solid_suffix;
    nmg_wstate.dbip = NULL;

    // If we have any solids in the hierarchies with only combs above them,
    // they are "implicit" regions and must be facetized individually. For
    // the non-NMG case we've already handled all primitives as part of the
    // normal pipeline, but for NMG we need to handle them specifically
    const char *implicit_regions = "( ! -below -type r ! -type comb )";
    struct bu_ptbl *ir = NULL;
    BU_ALLOC(ir, struct bu_ptbl);
    if (db_search(ir, DB_SEARCH_RETURN_UNIQ_DP, implicit_regions, argc, dpa, dbip, NULL, NULL, NULL) < 0) {
	if (s->verbosity >= 0) {
	    bu_log("Problem searching for implicit regions - aborting.\n");
	}
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	if (nmg_wstate.dbip)
	    db_close(nmg_wstate.dbip);
	return BRLCAD_OK;
    }
    if (BU_PTBL_LEN(ir)) {
	if (s->make_nmg || s->nmg_booleval) {
	    for (size_t i = 0; i < BU_PTBL_LEN(ir); i++) {
		struct directory *idp = (struct directory *)BU_PTBL_GET(ir, i);
		char *obj_name = bu_strdup(idp->d_namep);
		struct db_i *wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
		db_dirbuild(wdbip);
		db_update_nref(wdbip, &rt_uniresource);
		nmg_wstate.dbip = wdbip;
		int nret = _ged_facetize_nmgeval(s, 1, (const char **)&obj_name, obj_name);
		if (nret != BRLCAD_OK) {
		    if (s->verbosity >= 0)
			bu_log("regions.cpp:%d Failed to process %s.\n", __LINE__, obj_name);
		    bu_ptbl_free(ar);
		    bu_free(ar, "ar table");
		    bu_free(obj_name, "obj_name");
		    bu_free(dpa, "free dpa");
		    return BRLCAD_ERROR;
		}
		bu_free(obj_name, "obj_name");
		db_close(wdbip);
	    }
	}
    }
    bu_ptbl_free(ir);
    bu_free(ir, "ir table");


    // For proper regions, we're reducing the CSG tree to a single BoT or NMG and
    // swapping that solid in under the region comb
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    for (size_t i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *dpw[2] = {NULL};
	dpw[0] = (struct directory *)BU_PTBL_GET(ar, i);

	facetize_log(s, 0, "Processing %s\n", dpw[0]->d_namep);

	// Get a name for the region's output BoT
	if (s->make_nmg) {
	    bu_vls_sprintf(&bname, "%s.nmg", dpw[0]->d_namep);
	} else {
	    bu_vls_sprintf(&bname, "%s.bot", dpw[0]->d_namep);
	}

	struct db_i *wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
	wdbip->dbi_read_only = 1;
	db_dirbuild(wdbip);
	db_update_nref(wdbip, &rt_uniresource);
	struct directory *dcheck = db_lookup(wdbip, bu_vls_cstr(&bname), LOOKUP_QUIET);
	if (dcheck != RT_DIR_NULL)
	    bu_vls_incr(&bname, NULL, NULL, &_db_uniq_test, (void *)wdbip);
	wdbip->dbi_read_only = 0;
	nmg_wstate.dbip = wdbip;

	int bret = BRLCAD_OK;
	if (s->make_nmg || s->nmg_booleval) {
	    char *obj_name = bu_strdup(dpw[0]->d_namep);
	    bret = _ged_facetize_nmgeval(&nmg_wstate, 1, (const char **)&obj_name, bu_vls_cstr(&bname));
	    bu_free(obj_name, "obj_name");
	    db_close(wdbip);
	} else {
	    // Need wdbp in the next two stages for tolerances
	    struct rt_wdb *wwdbp = wdb_dbopen(wdbip, RT_WDB_TYPE_DB_DEFAULT);
	    char *obj_name = bu_strdup(dpw[0]->d_namep);
	    bret = _ged_facetize_booleval_tri(s, wdbip, wwdbp, 1, (const char **)&obj_name, bu_vls_cstr(&bname), vlfree, 1);
	    bu_free(obj_name, "obj_name");
	    db_close(wdbip);
	}

	if (bret != BRLCAD_OK) {
	    if (s->verbosity >= 0)
		bu_log("regions.cpp:%d Failed to generate %s.\n", __LINE__, bu_vls_cstr(&bname));
	    bu_ptbl_free(ar);
	    bu_free(ar, "ar table");
	    bu_vls_free(&bname);
	    bu_free(dpa, "free dpa");
	    return BRLCAD_ERROR;
	}

	// Replace the region's comb tree with the new solid
	wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
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
	db_close(wdbip);
    }
    bu_vls_free(&bname);

    // Report on the primitive processing
    if (!s->make_nmg && !s->nmg_booleval)
	facetize_primitives_summary(s);

    // keep active regions into .g copy
    struct ged *wgedp = ged_open("db", bu_vls_cstr(s->wfile), 1);
    if (!wgedp) {
	if (s->verbosity >= 0) {
	    bu_log("regions.cpp:%d unable to retrieve working data - FAIL\n", __LINE__);
	}
    	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }
    const char **av = (const char **)bu_calloc(argc+10, sizeof(const char *), "av");
    av[0] = "keep";
    av[1] = kfname;
    for (int i = 0; i < argc; i++) {
	av[i+2] = argv[i];
    }
    av[argc+2] = NULL;
    ged_exec_keep(wgedp, argc+2, av);
    ged_close(wgedp);

    /* Capture the current tops list.  If we're not doing an in-place overwrite, we
     * need to know what the new top level objects are for the assembly of the
     * final comb. */
    struct directory **tlist = NULL;
    size_t tcnt = db_ls(dbip, DB_LS_TOPS, NULL, &tlist);
    std::set<std::string> otops;
    for (size_t i = 0; i < tcnt; i++) {
	otops.insert(std::string(tlist[i]->d_namep));
    }
    bu_free(tlist, "tlist");
    tlist = NULL;

    /* The user may have specified a naming preference - if so, honor it */
    struct bu_vls prefix_str = BU_VLS_INIT_ZERO;
    struct bu_vls suffix_str = BU_VLS_INIT_ZERO;
    const char *affix = NULL;
    int use_prefix = 1;
    if (bu_vls_strlen(s->prefix)) {
	bu_vls_sprintf(&prefix_str, "%s", bu_vls_cstr(s->prefix));
    } else {
	bu_vls_sprintf(&prefix_str, "facetize_");
    }

    if (bu_vls_strlen(s->suffix)) {
	bu_vls_sprintf(&suffix_str, "%s", bu_vls_cstr(s->suffix));
	use_prefix = 0;
    }
    affix = (use_prefix) ? bu_vls_cstr(&prefix_str) : bu_vls_cstr(&suffix_str);

    // dbconcat output .g into original .g - either using -O to overwrite
    // or allowing dbconcat to suffix the names depending on whether we're
    // in-place or not.
    av[0] = "dbconcat";
    av[1] = (s->in_place) ? "-O" : "-L";
    av[2] = (use_prefix) ? "-p" : "-s";
    av[3] = kfname;
    av[4] = affix;
    av[5] = NULL;
    ged_exec_dbconcat(s->gedp, 5, av);
    bu_free(av, "av");

    bu_vls_free(&prefix_str);
    bu_vls_free(&suffix_str);

    /* Done importing stuff - update nref. */
    db_update_nref(dbip, &rt_uniresource);


    /* Capture the new tops list. */
    tcnt = db_ls(dbip, DB_LS_TOPS, NULL, &tlist);
    std::set<std::string> ntops;
    for (size_t i = 0; i < tcnt; i++) {
	ntops.insert(std::string(tlist[i]->d_namep));
    }
    bu_free(tlist, "tlist");


    /* Find the new top level objects from dbconcat */
    std::set<std::string> new_tobjs;
    std::set_difference(ntops.begin(), ntops.end(), otops.begin(), otops.end(), std::inserter(new_tobjs, new_tobjs.begin()));

    /* Check to see if oname ended up being created in the
     * dbconcat.  If it was, rename it. */
    struct directory *cdp  = db_lookup(dbip, oname, LOOKUP_QUIET);
    if (cdp != RT_DIR_NULL) {
	// Find a new name
	struct bu_vls nname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&nname, "%s_0", oname);
	cdp  = db_lookup(dbip, bu_vls_cstr(&nname), LOOKUP_QUIET);
	if (cdp != RT_DIR_NULL) {
	    if (bu_vls_incr(&nname, NULL, NULL, &_db_uniq_test, (void *)dbip) < 0) {
		if (s->verbosity >= 0) {
		    bu_log("regions.cpp:%d unable to generate name - FAIL\n", __LINE__);
		}
		bu_vls_free(&nname);
		return BRLCAD_ERROR;
	    }
	}
	const char *mav[4];
	mav[0] = "mvall";
	mav[1] = oname;
	mav[2] = bu_vls_cstr(&nname);
	mav[3] = NULL;
	ged_exec_mvall(s->gedp, 3, mav);
	new_tobjs.erase(std::string(oname));
	new_tobjs.insert(std::string(bu_vls_cstr(&nname)));
	bu_vls_free(&nname);
    }

    /* Make a new comb to hold the output */
    struct wmember wcomb;
    BU_LIST_INIT(&wcomb.l);
    struct rt_wdb *cwdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
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
    bu_dirclear(s->wdir);
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

