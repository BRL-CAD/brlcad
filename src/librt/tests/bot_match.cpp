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

int
main(int argc, char *argv[])
{
    int64_t start, elapsed;
    fastf_t seconds;

    bu_setprogname(argv[0]);

    if (argc != 2) {
	bu_exit(1, "Usage: %s file.g", argv[0]);
    }

    struct db_i *dbip = db_open(argv[1], DB_OPEN_READONLY);
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    db_update_nref(dbip, &rt_uniresource);

    start = bu_gettime();

    // Find all BoT objects in the .g database
    std::unordered_set<struct directory *> bots;
    struct bu_ptbl bot_objs = BU_PTBL_INIT_ZERO;
    const char *bot_search = "-type bot";
    (void)db_search(&bot_objs, DB_SEARCH_RETURN_UNIQ_DP, bot_search, 0, NULL, dbip, NULL, NULL, NULL);
    size_t bot_cnt = BU_PTBL_LEN(&bot_objs);
    for(size_t i = 0; i < bot_cnt; i++) {
        struct directory *dp = (struct directory *)BU_PTBL_GET(&bot_objs, i);
        bots.insert(dp);
    }
    db_search_free(&bot_objs);

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

    // If we have BoTs, we're going to be building up sets
    std::unordered_map<struct directory *, std::unordered_set<struct directory *>> bot_groups;
    std::unordered_set<struct directory *> clear_dps;
    size_t bots_processed = 0;
    while (!bots.empty()) {
	// Pop the next BoT off the set
	struct directory *wdp = *bots.begin();
	bots.erase(bots.begin());
	bots_processed++;
	bu_log("Processing %s (%zd of %zd)\n", wdp->d_namep, bots_processed, bot_cnt);

	// Make a pca version of the BoT
	struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
	rt_db_get_internal(&intern, wdp, dbip, NULL, &rt_uniresource);
	struct rt_bot_internal *orig_bot = (struct rt_bot_internal *)(intern.idb_ptr);
	struct rt_bot_internal *pca_orig_bot = pca_bot(orig_bot);
	point_t *ovp = (point_t *)pca_orig_bot->vertices;

	// For each of the remaining BoTs, see if its PCA version matches pca_g
	for (d_it = bots.begin(); d_it != bots.end(); ++d_it) {
	    struct directory *cdp = *d_it;

	    // Can we trivially rule it out?
	    if (bot_face_cnts[cdp] != orig_bot->num_faces || bot_vert_cnts[cdp] != orig_bot->num_vertices)
		continue;

	    // Passes the trivial checks - time for PCA and bg_trimesh_diff
	    //bu_log("  checking %s\n", cdp->d_namep);
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

	    // Free up the internal memory for cdp
	    if (cbot != pca_cbot)
		rt_bot_internal_free(pca_cbot);
	    rt_db_free_internal(&cintern);

	    // If there is no difference per bg_trimesh_diff, we found a PCA
	    // match - record it
	    if (!is_diff) {
		bot_groups[wdp].insert(cdp);
		clear_dps.insert(cdp);
		bots_processed++;
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

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Matching check complete (%f sec) - %zd match groups found\n",seconds, bot_groups.size());

    // Print any groups found
    std::unordered_map<struct directory *, std::unordered_set<struct directory *>>::iterator bg_it;
    for(bg_it = bot_groups.begin(); bg_it != bot_groups.end(); ++bg_it) {
	if (!bg_it->second.size())
	    continue;
	bu_log("%s:\n", bg_it->first->d_namep);
	for (d_it = bg_it->second.begin(); d_it != bg_it->second.end(); ++d_it) {
	    bu_log("\t%s\n", (*d_it)->d_namep);
	}
    }

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
