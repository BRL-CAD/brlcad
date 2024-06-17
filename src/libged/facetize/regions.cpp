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
	if (s->verbosity >= 0) {
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
	if (s->verbosity >= 0) {
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
	if (s->verbosity >= 0) {
	    bu_log("Problem searching for implicit regions - aborting.\n");
	}
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }
    bool have_failure = false;
    if (BU_PTBL_LEN(ir)) {

	if (_ged_facetize_leaves_tri(s, dbip, ir) != BRLCAD_OK) {
	    have_failure = true;
	}
    }
    bu_ptbl_free(ir);
    bu_free(ir, "ir table");

    // For proper regions, we're reducing the CSG tree to a single BoT and
    // swapping that BoT in under the region comb
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    for (size_t i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *dpw[2] = {NULL};
	dpw[0] = (struct directory *)BU_PTBL_GET(ar, i);

	facetize_log(s, 0, "Processing %s\n", dpw[0]->d_namep);

	// Get a name for the region's output BoT
	bu_vls_sprintf(&bname, "%s.bot", dpw[0]->d_namep);
	struct db_i *cdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READONLY);
	db_dirbuild(cdbip);
	db_update_nref(cdbip, &rt_uniresource);
	struct directory *dcheck = db_lookup(cdbip, bu_vls_cstr(&bname), LOOKUP_QUIET);
	if (dcheck != RT_DIR_NULL)
	    bu_vls_incr(&bname, NULL, NULL, &_db_uniq_test, (void *)cdbip);
	db_close(cdbip);

	if (_ged_facetize_booleval(s, 1, dpw, bu_vls_cstr(&bname), true, false) == BRLCAD_OK) {
	    // Replace the region's comb tree with the new BoT
	    struct db_i *wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
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

	} else {
	    have_failure = true;
	}
    }
    bu_vls_free(&bname);

    {
	struct db_i *cdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READONLY);
	if (cdbip) {
	    db_dirbuild(cdbip);
	    db_update_nref(cdbip, &rt_uniresource);
	    std::map<std::string, std::set<std::string>> method_sets;
	    method_scan(&method_sets, cdbip);
	    std::map<std::string, std::set<std::string>>::iterator m_it;
	    for (m_it = method_sets.begin(); m_it != method_sets.end(); ++m_it) {
		if (m_it->first == std::string("REPAIR")) {
		    bu_log("%zd BoT(s) closed to form manifolds using 'bot repair'%s\n",
			    m_it->second.size(), (s->verbosity > 0) ? ":" : ".");
		} else if (m_it->first == std::string("PLATE")) {
		    bu_log("%zd plate mode BoT(s) extruded to form manifold volumes%s\n",
			    m_it->second.size(), (s->verbosity > 0) ? ":" : ".");
		} else if (m_it->first == std::string("FAIL")) {
		    bu_log("%zd object(s) failed to facetize%s\n",
			    m_it->second.size(), (s->verbosity > 0) ? ":" : ".");
		} else {
		    bu_log("Facetized %zd object(s) using the %s method%s\n",
			    m_it->second.size(), m_it->first.c_str(), (s->verbosity > 0 && m_it->first != std::string("NMG")) ? ":" : ".");
		}
		if (s->verbosity > 0) {
		    // If we used NMG to facetize, that's considered normal - don't
		    // bother listing those primitives
		    if (m_it->first == std::string("NMG"))
			continue;
		    std::set<std::string>::iterator s_it;
		    for (s_it = m_it->second.begin(); s_it != m_it->second.end(); ++s_it) {
			bu_log("\t%s\n", (*s_it).c_str());
		    }
		}
	    }
	}
	db_close(cdbip);
    }

    // TODO - need to have an option to shotline through the new triangle faces and compare
    // with the old CSG shotline results to try and spot any gross problems with the new
    // mesh.  The nature of facetization means the shotlines won't be 1-1 identical, but
    // if we have a shot from a triangle with LoS in the CSG and none in the BoT, that's
    // a definite problem.  If the BoT is manifold we'll still return it, but with a warning
    // that there may be a problem.

    // If any of the regions failed (implicit or explicit), don't keep or
    // dbconcat - the operation wasn't a full success.
    if (have_failure) {
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    // keep active regions into .g copy
    struct ged *wgedp = ged_open("db", bu_vls_cstr(s->wfile), 1);
    if (!wgedp) {
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
    ged_exec(s->gedp, 5, av);
    bu_free(av, "av");

    bu_vls_free(&prefix_str);
    bu_vls_free(&suffix_str);

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

    /* Check to see if oname ended up being created in the
     * dbconcat.  If it was, rename it. */
    struct directory *cdp  = db_lookup(s->gedp->dbip, oname, LOOKUP_QUIET);
    if (cdp != RT_DIR_NULL) {
	// Find a new name
	struct bu_vls nname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&nname, "%s_0", oname);
	cdp  = db_lookup(s->gedp->dbip, bu_vls_cstr(&nname), LOOKUP_QUIET);
	if (cdp != RT_DIR_NULL) {
	    if (bu_vls_incr(&nname, NULL, NULL, &_db_uniq_test, (void *)s->gedp->dbip) < 0) {
		bu_vls_free(&nname);
		return BRLCAD_ERROR;
	    }
	}
	const char *mav[4];
	mav[0] = "mvall";
	mav[1] = oname;
	mav[2] = bu_vls_cstr(&nname);
	mav[3] = NULL;
	ged_exec(s->gedp, 3, mav);
	new_tobjs.erase(std::string(oname));
	new_tobjs.insert(std::string(bu_vls_cstr(&nname)));
	bu_vls_free(&nname);
    }

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

