/*                     C A C H E _ L O D . C
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
/** @file cache.c
 *
 * Caching of LoD drawing data
 */

#include "common.h"

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

    if (!dbip || !dbip->i || dbip->i->mesh_c)
	return;

    int64_t start, elapsed, overall_start;
    fastf_t seconds;
    dbip->i->mesh_c_completed = 0;
    dbip->i->mesh_c_target = 0;
    struct directory *dp;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT)
		dbip->i->mesh_c_target++;
	}
    }

    // Total target count is known, proceed
    start = bu_gettime();
    overall_start = start;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
		continue;

	    // If we already have a match, assume it is valid.  Resetting
	    // invalid data in the cache is outside the scope of db_cache_init.
	    unsigned long long key = bv_mesh_lod_key_get(dbip->i->mesh_c, dp->d_namep);
	    if (key)
		continue;

	    if (verbose > 1)
		bu_log("Processing(%d):  %s\n", dbip->i->mesh_c_completed+1, dp->d_namep);

	    // Process object
	    db_mesh_lod_update(dbip, dp);

	    // Increment completed count
	    dbip->i->mesh_c_completed++;

	    elapsed = bu_gettime() - start;
	    seconds = elapsed / 1000000.0;

	    if (verbose > 1)
		bu_log("Completed. (%g seconds)", seconds);

	    if (seconds > 5.0) {
		if (verbose) {
		    elapsed = bu_gettime() - overall_start;
		    seconds = elapsed / 1000000.0;
		    bu_log("LoD cache processing (%g seconds): completed %d of %d BoTs\n", seconds, dbip->i->mesh_c_completed, dbip->i->mesh_c_target);
		}
		start = bu_gettime();
	    }
	}
    }

    elapsed = bu_gettime() - overall_start;
    int rseconds = elapsed / 1000000;
    int rminutes = rseconds / 60;
    int rhours = rminutes / 60;
    rminutes = rminutes % 60;
    rseconds = rseconds % 60;
    bu_log("Mesh LoD caching complete (Elapsed time: %02d:%02d:%02d)\n", rhours, rminutes, rseconds);
}

void
db_mesh_lod_clear(struct db_i *dbip)
{
    if (!dbip || !dbip->i || dbip->i->mesh_c)
	return;

    bv_mesh_lod_clear_cache(dbip->i->mesh_c, 0);
}

int
db_mesh_lod_update(struct db_i *dbip, struct directory *dp)
{
    if (!dbip || !dbip->i || dbip->i->mesh_c)
	return BRLCAD_ERROR;

    // No-op if this isn't a BoT
    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
	return BRLCAD_OK;

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

    // If we have existing data, clear it - we're assuming whatever is there is
    // invalid if we're calling this function.
    unsigned long long key = bv_mesh_lod_key_get(dbip->i->mesh_c, dp->d_namep);
    if (key)
	bv_mesh_lod_clear_cache(dbip->i->mesh_c, key);

    // Cache new data
    key = bv_mesh_lod_cache(dbip->i->mesh_c, (const point_t *)bot->vertices, bot->num_vertices, NULL, bot->faces, bot->num_faces, 0, 0.66);
    if (!key) {
	bu_log("Error processing %s - unable to generate LoD data\n", dp->d_namep);
	rt_db_free_internal(&dbintern);
	return BRLCAD_ERROR;
    }
    bv_mesh_lod_key_put(dbip->i->mesh_c, dp->d_namep, key);
    rt_db_free_internal(&dbintern);

    // Make sure we can retrieve the cached data
    struct bv_mesh_lod *lod = bv_mesh_lod_create(dbip->i->mesh_c, key);
    if (!lod) {
	bu_log("Error processing %s - unable to retrieve LoD data\n", dp->d_namep);
	rt_db_free_internal(&dbintern);
	return BRLCAD_ERROR;
    }

    bv_mesh_lod_destroy(lod);

    return BRLCAD_OK;
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
