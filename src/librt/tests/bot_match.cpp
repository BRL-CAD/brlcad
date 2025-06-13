/*                    B O T _ M A T C H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2025 United States Government as represented by
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

#include "common.h"

#include <unordered_set>
#include <unordered_map>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/time.h"
#include "bu/units.h"
#include "bg/pca.h"
#include "bg/trimesh.h"
#include "raytrace.h"

struct rt_bot_internal *
pca_bot(struct rt_bot_internal *bot)
{
    point_t *vp = (point_t *)bot->vertices;
    point_t center;
    vect_t xaxis, yaxis, zaxis;

    if (bg_pca(&center, &xaxis, &yaxis, &zaxis, bot->num_vertices, vp) != BRLCAD_OK)
        return bot;

    mat_t R, T, RT;
    // Rotation
    MAT_IDN(R);
    VMOVE(&R[0], xaxis);
    VMOVE(&R[4], yaxis);
    VMOVE(&R[8], zaxis);
    // Translation
    MAT_IDN(T);
    MAT_DELTAS_VEC_NEG(T, center);
    // Combine
    bn_mat_mul(RT, R, T);

    struct rt_bot_internal *moved_bot = rt_bot_dup(bot);
    for (size_t i = 0; i < moved_bot->num_vertices; i++) {
	vect_t v1;
	VMOVE(v1, &moved_bot->vertices[i*3]);
	vect_t v2;
	MAT4X3PNT(v2, RT, v1);
	VMOVE(&moved_bot->vertices[i*3], v2);
    }

    return moved_bot;
}

struct hash_results {
    std::unordered_map<struct directory *, unsigned long long> dp_hash;
    std::unordered_map<unsigned long long, struct directory *> hash_dp;
    std::unordered_map<unsigned long long, std::unordered_set<struct directory *>> bot_groups_hashed;
};

// Test hash method
void
test_hash(struct hash_results *h, struct db_i *dbip, std::vector<struct directory *> &bots)
{
    if (!h || !dbip)
	return;

    int64_t start = bu_gettime();
    int64_t msgtime = bu_gettime();

    // Iterate over all BoTs
    for(size_t i = 0; i < bots.size(); i++) {
        struct directory *wdp = bots[i];

	if (bu_gettime() - msgtime > 5000000.0) {
	    bu_log("Processing %s (%zd of %zd)\n", wdp->d_namep, i, bots.size());
	    msgtime = bu_gettime();
	}

	// Make a PCA version of the BoT
	struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
	rt_db_get_internal(&intern, wdp, dbip, NULL, &rt_uniresource);
	struct rt_bot_internal *orig_bot = (struct rt_bot_internal *)(intern.idb_ptr);
	struct rt_bot_internal *pca_nbot = pca_bot(orig_bot);

	// Hash the PCA version
	point_t *ovp = (point_t *)pca_nbot->vertices;
	unsigned long long ohash = bg_trimesh_hash(
		pca_nbot->faces, pca_nbot->num_faces, ovp, pca_nbot->num_vertices,
		VUNITIZE_TOL
		);

	// Done with PCA bot
	rt_bot_internal_free(pca_nbot);
	rt_db_free_internal(&intern);

	// Record the results
	h->dp_hash[wdp] = ohash;
	h->hash_dp[ohash] = wdp;
	h->bot_groups_hashed[ohash].insert(wdp);
    }

    std::unordered_map<unsigned long long, std::unordered_set<struct directory *>>::iterator b_it;
    size_t gcnt = 0;
    size_t gobjs = 0;
    for (b_it = h->bot_groups_hashed.begin(); b_it != h->bot_groups_hashed.end(); ++b_it) {
	// Anything by itself in a set didn't form any groups
	if (b_it->second.size() == 1)
	    continue;

	// OK, found a non-trivial group
	gcnt++;
	gobjs += b_it->second.size();
    }

    int64_t elapsed = bu_gettime() - start;
    fastf_t seconds = elapsed / 1000000.0;

    bu_log("Hashing complete (%f sec) - %zd match groups found, %zd of %zd BoTs are part of a group.\n", seconds, gcnt, gobjs, bots.size());
}

struct diff_results {
    std::unordered_map<struct directory *, std::unordered_set<struct directory *>> bot_groups;
};

// Test diff method
void
test_diff(struct diff_results *d, struct db_i *dbip, std::vector<struct directory *> &vbots, struct hash_results *h)
{
    if (!d || !dbip)
	return;

    int64_t start = bu_gettime();

    // Build a set of BoTs
    std::unordered_set<struct directory *> bots;
    for(size_t i = 0; i < vbots.size(); i++) {
        bots.insert(vbots[i]);
    }

    // We can't afford to load everything into memory and leave it there, and most
    // BoTs won't be matches trivially based on counts, so make a single up front pass
    // to collect face and vert counts once.  This will let us avoid a lot of I/O
    // during the comparisons.
    std::unordered_map<struct directory *, size_t> bot_vert_cnts;
    std::unordered_map<struct directory *, size_t> bot_face_cnts;
    std::unordered_set<struct directory *>::iterator d_it;
    for (d_it = bots.begin(); d_it != bots.end(); ++d_it) {
	struct directory *dp = *d_it;
	struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
	rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
	struct rt_bot_internal *bot = (struct rt_bot_internal *)(intern.idb_ptr);
	bot_vert_cnts[dp] = bot->num_vertices;
	bot_face_cnts[dp] = bot->num_faces;
	rt_db_free_internal(&intern);
    }

    // Work through the set of BoTs looking for matches
    std::unordered_set<struct directory *> clear_dps;
    size_t bots_processed = 0;
    int64_t msgtime = bu_gettime();
    while (!bots.empty()) {
	// Pop the next BoT off the set
	struct directory *wdp = *bots.begin();
	unsigned long long ohash = h->dp_hash[wdp];
	bots.erase(bots.begin());
	bots_processed++;

	if (bu_gettime() - msgtime > 5000000.0) {
	    bu_log("Processing %s (%zd of %zd)\n", wdp->d_namep, bots_processed, vbots.size());
	    msgtime = bu_gettime();
	}

	// Make a pca version of the BoT
	struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
	rt_db_get_internal(&intern, wdp, dbip, NULL, &rt_uniresource);
	struct rt_bot_internal *orig_bot = (struct rt_bot_internal *)(intern.idb_ptr);
	struct rt_bot_internal *pca_orig_bot = pca_bot(orig_bot);
	point_t *ovp = (point_t *)pca_orig_bot->vertices;

	// For each of the remaining BoTs, see if its PCA version matches pca_g
	for (d_it = bots.begin(); d_it != bots.end(); ++d_it) {

	    struct directory *cdp = *d_it;
	    unsigned long long chash = h->dp_hash[cdp];

	    // Can we trivially rule it out?
	    if (bot_face_cnts[cdp] != orig_bot->num_faces || bot_vert_cnts[cdp] != orig_bot->num_vertices) {
		// Did the hashes think these matched?
		if (ohash == chash)
		    bu_log("WARNING!  BoTs %s and %s are different, but their hashes match! (%llu)\n", wdp->d_namep, cdp->d_namep, chash);
		continue;
	    }

	    // Passes the trivial checks - time for PCA and bg_trimesh_diff
	    struct rt_db_internal cintern = RT_DB_INTERNAL_INIT_ZERO;
	    rt_db_get_internal(&cintern, cdp, dbip, NULL, &rt_uniresource);
	    struct rt_bot_internal *cbot = (struct rt_bot_internal *)(cintern.idb_ptr);
	    struct rt_bot_internal *pca_cbot = pca_bot(cbot);
	    point_t *cvp = (point_t *)pca_cbot->vertices;

	    int is_diff = bg_trimesh_diff(
		    pca_orig_bot->faces, pca_orig_bot->num_faces, ovp, pca_orig_bot->num_vertices,
		    pca_cbot->faces, pca_cbot->num_faces, cvp, pca_cbot->num_vertices,
		    VUNITIZE_TOL
		    );

	    // We have our diff answer - free up the internal memory for cdp
	    if (cbot != pca_cbot)
		rt_bot_internal_free(pca_cbot);
	    rt_db_free_internal(&cintern);

	    // If there is no difference per bg_trimesh_diff, we found a PCA
	    // match - record it
	    if (!is_diff) {
		d->bot_groups[wdp].insert(cdp);
		clear_dps.insert(cdp);
		bots_processed++;
		// Difference found - did the hashes think these didn't match?
		if (ohash != chash)
		    bu_log("WARNING!  BoTs %s and %s are the same, but their hashes differ! (%llu and %llu)\n", wdp->d_namep, cdp->d_namep, ohash, chash);

	    } else {
		// Difference found - did the hashes think these matched?
		if (ohash == chash)
		    bu_log("WARNING!  BoTs %s and %s are different, but their hashes match! (%llu)\n", wdp->d_namep, cdp->d_namep, chash);
	    }
	}

	// Free up the internal memory for wdp
	if (orig_bot != pca_orig_bot)
	    rt_bot_internal_free(pca_orig_bot);
	rt_db_free_internal(&intern);

	// Remove anything we found a match for from the working bots set
	for (d_it = clear_dps.begin(); d_it != clear_dps.end(); ++d_it)
	    bots.erase(*d_it);
	clear_dps.clear();
    }

    int64_t elapsed = bu_gettime() - start;
    fastf_t seconds = elapsed / 1000000.0;

    // Get a count of all objects that would up part of some group
    std::unordered_map<struct directory *, std::unordered_set<struct directory *>>::iterator bg_it;
    size_t gobjs = 0;
    for (bg_it = d->bot_groups.begin(); bg_it != d->bot_groups.end(); ++bg_it) {
	gobjs++;  // Account for the key dp
	gobjs += bg_it->second.size(); // Add the matches
    }

    bu_log("Diff matching check complete (%f sec) - %zd match groups found, %zd of %zd BoTs are part of a group.\n",seconds, d->bot_groups.size(), gobjs, vbots.size());
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    if (argc != 2) {
	bu_exit(1, "Usage: %s file.g", argv[0]);
    }

    int64_t start = bu_gettime();

    struct db_i *dbip = db_open(argv[1], DB_OPEN_READONLY);
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    db_update_nref(dbip, &rt_uniresource);

    int64_t elapsed = bu_gettime() - start;
    fastf_t seconds = elapsed / 1000000.0;

    bu_log("Setup time: %g seconds\n", seconds);

    // Find all BoT objects in the .g database
    std::vector<struct directory *> bots;
    start = bu_gettime();
#if 0
    // NOTE - several seconds slower than just directly
    // iterating over the hashes on a large model...
    struct bu_ptbl bot_objs = BU_PTBL_INIT_ZERO;
    const char *bot_search = "-type bot";
    (void)db_search(&bot_objs, DB_SEARCH_RETURN_UNIQ_DP|DB_SEARCH_FLAT, bot_search, 0, NULL, dbip, NULL, NULL, NULL);
    for (size_t i = 0; i < BU_PTBL_LEN(&bot_objs); i++)
	bots.push_back((struct directory *)BU_PTBL_GET(&bot_objs, i));
    db_search_free(&bot_objs);
#else
    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
		continue;
	    bots.push_back(dp);
	}
    }
#endif
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;

    bu_log("Initial BoT search time: %g seconds\n", seconds);


    struct hash_results h;
    test_hash(&h, dbip, bots);

    struct diff_results d;
    test_diff(&d, dbip, bots, &h);

#if 0
    // Print any groups found
    for(bg_it = bot_groups.begin(); bg_it != bot_groups.end(); ++bg_it) {
	if (!bg_it->second.size())
	    continue;
	bu_log("%s:\n", bg_it->first->d_namep);
	for (d_it = bg_it->second.begin(); d_it != bg_it->second.end(); ++d_it) {
	    bu_log("\t%s\n", (*d_it)->d_namep);
	}
    }
#endif

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
