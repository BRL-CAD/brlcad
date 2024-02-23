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
    int newobj_cnt = 0;
    int ret = BRLCAD_OK;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->dbip;

    /* Used the libged tolerances */
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    s->tol = &(wdbp->wdb_ttol);

    if (!argc) return BRLCAD_ERROR;

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
    if (_ged_validate_objs_list(s, argc, argv, newobj_cnt) == BRLCAD_ERROR) {
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    const char *active_regions = "( -type r ! -below -type r ) -or ( ! -below -type r ! -type comb )";
    struct bu_ptbl *ar = NULL;
    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, active_regions, newobj_cnt, dpa, dbip, NULL) < 0) {
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
	bu_free(dpa, "free dpa");
	return BRLCAD_OK;
    }

    /* First stage is to process the primitive instances - get the primitives from the regions */
    bu_free(dpa, "dpa");
    dpa = (struct directory **)bu_calloc(BU_PTBL_LEN(ar), sizeof(struct directory *), "dp array");
    for (size_t i = 0; i < BU_PTBL_LEN(ar); i++) {
	dpa[i] = (struct directory *)BU_PTBL_GET(ar, i);
    }

    // For the working file setup, we need to check the solids to see if any
    // supporting files need to be copied
    const char *active_solids = "! -type comb";
    struct bu_ptbl *as = NULL;
    BU_ALLOC(as, struct bu_ptbl);
    if (db_search(as, DB_SEARCH_RETURN_UNIQ_DP, active_solids, BU_PTBL_LEN(ar), dpa, dbip, NULL) < 0) {
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

    bool have_failure = false;
    for (size_t i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *dpw[2] = {NULL};
	dpw[0] = (struct directory *)BU_PTBL_GET(ar, i);
	bu_vls_sprintf(&bname, "%s.bot", dpw[0]->d_namep);
	bu_vls_incr(&bname, NULL, NULL, &_db_uniq_test, (void *)gedp);
	if (_ged_facetize_booleval(s, 1, dpw, bu_vls_cstr(&bname), wdir, wfile) == BRLCAD_OK) {
	    // TODO - replace the region's comb tree with the new BoT
	} else {
	    have_failure = true;
	}
    }

    // If any of the regions failed, don't keep or dbconcat - the operation
    // wasn't a full success.
    if (have_failure) {
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
	bu_free(dpa, "dpa array");
	return BRLCAD_ERROR;
    }

    // TODO - keep active regions into .g copy
    // TODO - dbconcat kept .g into original .g - either using -O to overwrite
    // or allowing dbconcat to suffix the names depending on whether we're
    // in-place or not.


    /* Done changing stuff - update nref. */
    db_update_nref(dbip, &rt_uniresource);

    bu_ptbl_free(ar);
    bu_free(ar, "ar table");
    bu_free(dpa, "dpa array");
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

