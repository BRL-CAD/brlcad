/*                   C A C H E _ L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2016-2025 United States Government as represented by
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
/** @file cache_lod.cpp
 *
 * Caching of LoD drawing data
 */

#include "common.h"

#include <string.h>

/* implementation headers */
#include "bu/app.h"
#include "bu/file.h"
#include "bu/path.h"
#include "bu/process.h"
#include "bu/time.h"
#include "rt/db_instance.h"

#include "./librt_private.h"

void
db_mesh_lod_init(struct db_i *dbip, int verbose) {

    if (!dbip || !dbip->i)
	return;

    if (dbip->dbi_wdbp_inmem || !strlen(dbip->dbi_filename)) {
	if (verbose)
	    bu_log("Temporary databases do not support on-disk caches.");
	return;
    }

    dbip->i->init_complete = false;

    if (!dbip->i->mesh_c)
	dbip->i->mesh_c = bv_mesh_lod_context_create(dbip->dbi_filename);

    if (!dbip->i->mesh_c) {
	if (verbose)
	    bu_log("Error: LoD cache setup failed for %s", dbip->dbi_filename);
	dbip->i->init_complete = true;
	return;
    }

    int mesh_c_target = 0;
    struct directory *dp;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT)
		mesh_c_target++;
	}
    }

    // Total target count is known, proceed
    int64_t start = bu_gettime();
    int64_t overall_start = start;
    int64_t elapsed = 0;
    fastf_t seconds = 0.0;
    int mesh_c_completed = 0;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
		continue;

	    // If we already have a match, assume it is valid.  Resetting
	    // invalid data in the cache is outside the scope of cache init.
	    unsigned long long key = bv_mesh_lod_key_get(dbip->i->mesh_c, dp->d_namep);
	    if (key)
		continue;

	    // We're about to do something that might take a while.  If the
	    // parent thread has decided to close the dbip, we should stop now.
	    if (dbip->i->shutdown_requested) {
		dbip->i->init_complete = true;
		return;
	    }

	    // Process object
	    if (verbose > 1) {
		bu_log("Processing(%d):  %s\n", mesh_c_completed+1, dp->d_namep);
		start = bu_gettime();
	    }
	    db_mesh_lod_update(dbip, dp->d_namep);

	    // Increment completed count
	    mesh_c_completed++;

	    // If the user has requested it, let them know what is happening
	    if (verbose) {
		if (verbose > 1) {
		    elapsed = bu_gettime() - start;
		    seconds = elapsed / 1000000.0;
		    bu_log("Completed. (%g seconds)", seconds);
		}

		if (seconds > 5.0) {
		    elapsed = bu_gettime() - overall_start;
		    seconds = elapsed / 1000000.0;
		    bu_log("LoD cache processing (%g seconds): completed %d of %d BoTs\n", seconds, mesh_c_completed, mesh_c_target);
		    start = bu_gettime();
		}
	    }
	}
    }

    if (verbose) {
	elapsed = bu_gettime() - overall_start;
	int rseconds = elapsed / 1000000;
	int rminutes = rseconds / 60;
	int rhours = rminutes / 60;
	rminutes = rminutes % 60;
	rseconds = rseconds % 60;
	bu_log("Mesh LoD caching complete (Elapsed time: %02d:%02d:%02d)\n", rhours, rminutes, rseconds);
    }

    // All done.
    dbip->i->init_complete = true;
}

void
db_mesh_lod_clear(struct db_i *dbip)
{
    if (!dbip || !dbip->i || dbip->i->mesh_c)
	return;

    bv_mesh_lod_clear_cache(dbip->i->mesh_c, 0);
}

int
db_mesh_lod_update(struct db_i *dbip, const char *name)
{
    if (!dbip || !dbip->i || dbip->i->mesh_c)
	return BRLCAD_ERROR;

    // No-op
    if (!name)
	return BRLCAD_OK;

    // If we have existing data, clear it.
    unsigned long long key = bv_mesh_lod_key_get(dbip->i->mesh_c, name);
    if (key) {
	bv_mesh_lod_clear_cache(dbip->i->mesh_c, key);
	bv_mesh_lod_key_put(dbip->i->mesh_c, name, 0);
    }

    // If this isn't an active BoT, we're done.
    //
    // (TODO - eventually, per-object cache updating probably should go in a
    // functab entry - in principle BReps should be doing this, and there's
    // nothing preventing caching facetized representations of other solids if
    // that makes sense.
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
	return BRLCAD_OK;

    // Unpack the data for LoD generation
    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    struct rt_db_internal *ip = &dbintern;
    int ret = rt_db_get_internal(ip, dp, dbip, NULL, &rt_uniresource);
    if (ret < 0)
	return BRLCAD_ERROR;
    if (ip->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_log("Error processing %s - mismatch between d_minor_type (%c) and idb_minor_type (%c)\n", dp->d_namep, dp->d_minor_type, ip->idb_minor_type);
	rt_db_free_internal(&dbintern);
	return BRLCAD_ERROR;
    }
    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    // Generate and write new data
    key = bv_mesh_lod_cache(dbip->i->mesh_c, (const point_t *)bot->vertices, bot->num_vertices, NULL, bot->faces, bot->num_faces, 0, 0.66);
    if (!key) {
	bu_log("Error processing %s - unable to generate LoD data\n", dp->d_namep);
	rt_db_free_internal(&dbintern);
	return BRLCAD_ERROR;
    }
    bv_mesh_lod_key_put(dbip->i->mesh_c, dp->d_namep, key);

    // Done with BoT
    rt_db_free_internal(&dbintern);

    // Make sure we can retrieve the cached data
    // TODO - may not really be necessary to verify this here once we're
    // working - including during early stages for testing.
    struct bv_mesh_lod *lod = bv_mesh_lod_create(dbip->i->mesh_c, key);
    if (!lod) {
	bu_log("Error processing %s - unable to retrieve LoD data\n", dp->d_namep);
	rt_db_free_internal(&dbintern);
	return BRLCAD_ERROR;
    }

    bv_mesh_lod_destroy(lod);

    return BRLCAD_OK;
}

struct bv_mesh_lod *
db_mesh_lod_get(struct db_i *dbip, const char *name)
{
    if (!dbip || !name)
	return NULL;

    struct bv_mesh_lod *lod = NULL;

    unsigned long long key = bv_mesh_lod_key_get(dbip->i->mesh_c, name);
    if (key)
	lod = bv_mesh_lod_create(dbip->i->mesh_c, key);

    return lod;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
