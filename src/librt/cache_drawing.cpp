/*                   C A C H E _ D R A W I N G . C P P
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
/** @file cache_drawing.cpp
 *
 * Caching of drawing data
 */

#include "common.h"

#include <string.h>

/* implementation headers */
#include "bu/app.h"
#include "bu/cache.h"
#include "bu/color.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/process.h"
#include "bu/time.h"
#include "bv/lod.h"
#include "rt/db_instance.h"
#include "rt/db_internal.h"

#include "./librt_private.h"

int
db_cache_init(struct db_i *dbip)
{
    if (!dbip || !dbip->i)
	return 0;

    // Check if we've already done init
    if (dbip->i->c)
	return 1;

    // We want the cache to be able to hold all the info we
    // might want to generate about the database for drawing
    // purposes, so set the max file size of the cache to 2x
    // that of the database.  If we have a small database,
    // just use the default cache size - the idea is we don't
    // want to run into a cache limit in normal use, but we
    // also don't want to eat the whole drive if there is
    // some sort of error.
    long long fsize = 2*bu_file_size(dbip->dbi_filename);
    if (fsize < BU_CACHE_DEFAULT_DB_SIZE)
	fsize = 0;

    dbip->i->c = bu_cache_open(dbip->dbi_filename, 1, fsize);

    // Populate cache_geom_uses
    //
    // Note that this is a cache-only operation - the dbip isn't involved
    // except as storage.  That way, garbage collect will be able to clear out
    // old, unused geometry info as well.

    return 1;
}

/* For all named objects, we NEED the following properties cached to do drawing
 * quickly:
 *
 * region_id
 * region_flag
 * color_inherit
 * color_value
 *
 * The majority of objects in a .g will not have an explicit color set on them
 * directly, but without a cache entry we have no way to tell whether we need
 * to get it so all objects will be assigned a default entry in the cache if
 * they are unset.
 *
 * For this function, we are just checking the cache to see if we have what we
 * need - cracking the AVS, if we must do so, will come later.  The first entry
 * we find that is not present means we need to crack the AVS, so at that point
 * we just bail - we'll go ahead and update everything if any entry is missing.
 */
bool
cache_check(struct bu_cache *c, unsigned long long hash)
{
    // Key length is limited, so we don't need to malloc
    // and free a full bu_vls for this.
    static char ckey[BU_CACHE_KEY_MAXLEN];

    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_ID);
    if (!bu_cache_get(NULL, ckey, c)) {
	bu_cache_get_done(c);
	return false;
    }

    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_FLAG);
    if (!bu_cache_get(NULL, ckey, c)) {
	bu_cache_get_done(c);
	return false;
    }

    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_INHERIT_FLAG);
    if (!bu_cache_get(NULL, ckey, c)) {
	bu_cache_get_done(c);
	return false;
    }

    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_COLOR);
    if (!bu_cache_get(NULL, ckey, c)) {
	bu_cache_get_done(c);
	return false;
    }

    bu_cache_get_done(c);
    return true;
}

void
db_cache_draw_init(struct db_i *dbip, int verbose)
{
    if (!dbip || !dbip->i)
	return;

    if (dbip->dbi_wdbp_inmem || !strlen(dbip->dbi_filename)) {
	if (verbose)
	    bu_log("Temporary databases do not support on-disk caches.");
	return;
    }

    dbip->i->draw_init_complete = false;

    // Make sure we're inited
    if (!db_cache_init(dbip)) {
	if (verbose)
	    bu_log("Error: Cache setup failed for %s", dbip->dbi_filename);
	dbip->i->draw_init_complete = true;
	return;
    }

    // BoTs are handled by a separate routine, since they are (relatively
    // speaking) much slower.
    int target_cnt = 0;
    struct directory *dp;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
		target_cnt++;
	}
    }

    // Total target count is known, proceed
    int64_t start = bu_gettime();
    int64_t overall_start = start;
    int64_t elapsed = 0;
    fastf_t seconds = 0.0;
    int completed = 0;
    struct bu_vls keystr = BU_VLS_INIT_ZERO;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;

	    if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
		continue;

	    // If we already have a match, assume it is valid.  Resetting
	    // invalid data in the cache is outside the scope of cache init.
	    unsigned long long user_key = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	    if (cache_check(dbip->i->c, user_key))
		continue;


	    // If the parent thread has decided to close the dbip, we can
	    // stop now.
	    if (dbip->i->shutdown_requested) {
		bu_vls_free(&keystr);
		dbip->i->draw_init_complete = true;
		return;
	    }

	    // Process object
	    if (verbose > 1) {
		bu_log("Processing(%d):  %s\n", completed+1, dp->d_namep);
		start = bu_gettime();
	    }
	    db_cache_draw_update(dbip, dp->d_namep, 0);

	    // Increment completed count
	    completed++;

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
		    bu_log("Drawing cache processing (%g seconds): completed %d of %d BoTs\n", seconds, completed, target_cnt);
		    start = bu_gettime();
		}
	    }
	}
    }
    bu_vls_free(&keystr);

    if (verbose) {
	elapsed = bu_gettime() - overall_start;
	int rseconds = elapsed / 1000000;
	int rminutes = rseconds / 60;
	int rhours = rminutes / 60;
	rminutes = rminutes % 60;
	rseconds = rseconds % 60;
	bu_log("Draw caching complete (Elapsed time: %02d:%02d:%02d)\n", rhours, rminutes, rseconds);
    }

    // All done.
    dbip->i->draw_init_complete = true;
}

void
db_cache_mesh_init(struct db_i *dbip, int verbose)
{

    if (!dbip || !dbip->i)
	return;

    if (dbip->dbi_wdbp_inmem || !strlen(dbip->dbi_filename)) {
	if (verbose)
	    bu_log("Temporary databases do not support on-disk caches.");
	return;
    }

    dbip->i->mesh_init_complete = false;

    // Make sure we're inited
    if (!db_cache_init(dbip)) {
	if (verbose)
	    bu_log("Error: Cache setup failed for %s", dbip->dbi_filename);
	dbip->i->mesh_init_complete = true;
	return;
    }

    int target = 0;
    struct directory *dp;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT)
		target++;
	}
    }

    // Total target count is known, proceed
    int64_t start = bu_gettime();
    int64_t overall_start = start;
    int64_t elapsed = 0;
    fastf_t seconds = 0.0;
    int completed = 0;
    struct bu_vls keystr = BU_VLS_INIT_ZERO;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
		continue;

	    if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
		continue;

	    // If we already have a match, assume it is valid.  Resetting
	    // invalid data in the cache is outside the scope of cache init.
	    unsigned long long user_key = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	    bu_vls_sprintf(&keystr, "%llu", user_key);
	    int in_cache = bu_cache_get(NULL, bu_vls_cstr(&keystr), dbip->i->c);
	    bu_cache_get_done(dbip->i->c);
	    if (in_cache)
		continue;

	    // We're about to do something that might take a while.  If the
	    // parent thread has decided to close the dbip, we should stop now.
	    if (dbip->i->shutdown_requested) {
		bu_vls_free(&keystr);
		dbip->i->mesh_init_complete = true;
		return;
	    }

	    // Process object
	    if (verbose > 1) {
		bu_log("Processing(%d):  %s\n", completed+1, dp->d_namep);
		start = bu_gettime();
	    }
	    db_cache_mesh_update(dbip, dp->d_namep);

	    // TODO - lod_mesh gets the bounding box, but we have other bits to
	    // get as well - this routine is draw prep for BoTs


	    // Increment completed count
	    completed++;

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
		    bu_log("LoD cache processing (%g seconds): completed %d of %d BoTs\n", seconds, completed, target);
		    start = bu_gettime();
		}
	    }
	}
    }
    bu_vls_free(&keystr);

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
    dbip->i->mesh_init_complete = true;
}

int
db_cache_mesh_update(struct db_i *dbip, const char *name)
{
    if (!dbip || !dbip->i || !dbip->i->c)
	return BRLCAD_ERROR;

    // No-op
    if (!name)
	return BRLCAD_OK;

    // If we have existing data, clear it.
    // TODO - do this right... need to keep a map of the name hashes to data
    // hashes where we can tell when a data hash no longer has any name hashes
    // using it, THEN delete all the various pieces associated with it.
    bu_cache_clear(name, dbip->i->c);

    // If this isn't an active BoT, we're done.
    //
    // TODO - eventually, per-object cache updating probably should go in a
    // functab entry - in principle BReps should be doing this, and there's
    // nothing preventing caching facetized representations of other solids if
    // that makes sense.
    //
    // Doing facetization ops behind the scenes to create cached meshes for
    // solids and combs would be ideal but very tricky - facetize uses
    // multiprocess logic in libged to produce meshes while being robust to
    // failure modes like infinite looping.  A more practical middle ground
    // migth be to have the cache support reading in evaluated BoT versions
    // with LoD info if they exist, but skipp creating them - a libged level
    // process could then managing the creation of the meshes for combs and
    // writing the associated LoD data to the cache.
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
    int cret = bv_lod_mesh_cache(dbip->i->c, name, (const point_t *)bot->vertices, bot->num_vertices, NULL, bot->faces, bot->num_faces, 0.66);
    if (!cret) {
	bu_log("Error processing %s - unable to generate LoD data\n", dp->d_namep);
	rt_db_free_internal(&dbintern);
	return BRLCAD_ERROR;
    }

    // Done with BoT
    rt_db_free_internal(&dbintern);

    // Make sure we can retrieve the cached data
    // TODO - may not really be necessary to verify this here once we're
    // working - including during early stages for testing.
    struct bv_lod_mesh *lod = bv_lod_mesh_get(dbip->i->c, name);
    if (!lod) {
	bu_log("Error processing %s - unable to retrieve LoD data\n", dp->d_namep);
	rt_db_free_internal(&dbintern);
	return BRLCAD_ERROR;
    }

    bv_lod_mesh_put(lod);

    return BRLCAD_OK;
}

struct bv_lod_mesh *
db_cache_lod_mesh_get(struct db_i *dbip, const char *name)
{
    if (!dbip || !dbip->i || !dbip->i->c || !name)
	return NULL;

    return bv_lod_mesh_get(dbip->i->c, name);
}

int
db_cache_draw_update(struct db_i *dbip, const char *name, int attr_only)
{
    static char ckey[BU_CACHE_KEY_MAXLEN];

    if (!dbip || !dbip->i || !dbip->i->c)
	return BRLCAD_ERROR;

    struct bu_cache *c = dbip->i->c;

    // No-op
    if (!name)
	return BRLCAD_OK;

    // If we have existing data, clear it.
    // TODO - do this right...
    bu_cache_clear(name, c);

    // BoTs have extra work to do for LoD...
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT)
	bu_log("TODO - LoD init\n");

    // TODO - port dcache.cpp and dbi2_state.cpp setup here.  Probably will
    // go ahead and do the bounding box calc too, which is the primary
    // candidate that could motivate doing the draw init in a separate thread
    // like the mesh LoD init.
    unsigned long long hash = bu_data_hash(name, strlen(name)*sizeof(char));

    // Read the attributes from the database object
    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
    db5_get_attributes(dbip, &c_avs, dp);

    // Region flag.
    int rflag = 0;
    const char *region_flag_str = bu_avs_get(&c_avs, "region");
    if (region_flag_str && (BU_STR_EQUAL(region_flag_str, "R") || BU_STR_EQUAL(region_flag_str, "1")))
	rflag = 1;
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_FLAG);
    if (!bu_cache_write(&rflag, sizeof(int), ckey, c)) {
	bu_log("%s: region_flag cache write failure\n", name);
	return BRLCAD_ERROR;
    }

    // Region id.  For drawing purposes this needs to be a number.
    int region_id = -1;
    const char *region_id_str = bu_avs_get(&c_avs, "region_id");
    if (region_id_str)
	bu_opt_int(NULL, 1, &region_id_str, (void *)&region_id);
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_ID);
    if (!bu_cache_write(&region_id, sizeof(int), ckey, c)) {
	bu_log("%s: region_id cache write failure\n", name);
	return BRLCAD_ERROR;
    }

    // Inherit flag
    int color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_INHERIT_FLAG);
    if (!bu_cache_write(&color_inherit, sizeof(int), ckey, c)) {
	bu_log("%s: color_inherit cache write failure\n", name);
	return BRLCAD_ERROR;
    }


    // Color
    unsigned int colors = UINT_MAX; // Using UINT_MAX to indicate unset
    const char *color_str = bu_avs_get(&c_avs, "color");
    if (!color_str)
	color_str = bu_avs_get(&c_avs, "rgb");
    if (color_str) {
	struct bu_color color;
	bu_opt_color(NULL, 1, &color_str, (void *)&color);
	// Serialize for cache
	int r, g, b;
	bu_color_to_rgb_ints(&color, &r, &g, &b);
	colors = r + (g << 8) + (b << 16);
    }
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_COLOR);
    if (!bu_cache_write(&colors, sizeof(unsigned int), ckey, c)) {
	bu_log("%s: color cache write failure\n", name);
	return BRLCAD_ERROR;
    }

    // If we're only updating the cached attributes (i.e. the
    // calling code knows the geometry wasn't changed) we can
    // stop now.
    if (attr_only)
	return BRLCAD_OK;


    // TODO - small BoTs may be fast enough to do here, LoD and all - another
    // possibility that may simplify the API would be to have the master init
    // function filter based on object size and queue up the big ones for
    // LoD generation after an initial pass to get the bounding boxes (the
    // fastest thing we can do to get SOMETHING we can draw - the drawing
    // routines could have a priority of LoD data, followed by slist, and
    // the bbox as a last resort (may be worth seeing if the oriented bounding
    // box calc would be fast enough to toss into the mix.
    //
    // May be worth adding object timestamp attributes to the mix as well,
    // since that would be the easiest way to trigger a cache rebuild on
    // database load...



    // Bounding Box (and other LoD data) must come from the geometry.
    // Unpack the internal.
    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    struct rt_db_internal *ip = &dbintern;
    int ret = rt_db_get_internal(ip, dp, dbip, NULL, &rt_uniresource);
    if (ret < 0)
	return BRLCAD_ERROR;
    if (ip->idb_minor_type != dp->d_minor_type) {
	bu_log("Error processing %s - mismatch between d_minor_type (%c) and idb_minor_type (%c)\n", dp->d_namep, dp->d_minor_type, ip->idb_minor_type);
	rt_db_free_internal(&dbintern);
	return BRLCAD_ERROR;
    }


    // Done with internal
    rt_db_free_internal(&dbintern);

    // TODO - Make sure we can retrieve the cached data

    return BRLCAD_OK;
}

void
db_cache_clear_entries(struct db_i *dbip, unsigned long long hash)
{
    struct bu_cache *c = dbip->i->c;

    // Key length is limited, so we don't need to malloc
    // and free a full bu_vls for this.
    static char ckey[MAXPATHLEN];

    // Clear attr entries
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_ID);
    bu_cache_clear(ckey, c);
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_FLAG);
    bu_cache_clear(ckey, c);
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_INHERIT_FLAG);
    bu_cache_clear(ckey, c);
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_COLOR);
    bu_cache_clear(ckey, c);


    // TODO - clear per-name geometry info:
    // bounding box, oriented bounding box

    // Look up the geometry key for this name
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_GEOM_KEY);
    void *vdata;
    if (!bu_cache_get(&vdata, ckey, c)) {
	// If we don't have a geometry key, we're done
	return;
    }
    unsigned long long ghash;
    memcpy(&ghash, vdata, sizeof(unsigned long long));
    if (!ghash) {
	// If we don't have a valid geometry key, we're done
	return;
    }

    // hash is being removed, so remove it from the ghash active set
    dbip->i->cache_geom_uses[ghash].erase(hash);

    // If ghash still has active users, we're done
    if (dbip->i->cache_geom_uses[ghash].size()) 
	return;

    // No more users of ghash - clear the data associated with ghash as well.

    // TODO - clear bbox, BoT LoD data - NOTE: bounding box must also be stored under
    // ghash for LoD reuse.

}

void
db_cache_gc(struct db_i *dbip)
{
    if (!dbip)
	return;

    // TODO - get all cache keys, and use something like a region flag
    // filter to collect the set of all name hashes

    // Hash all active dp names in the dbip, and remove those hashes from
    // the name set collected previously

    // Anything remaining is garbage, and gets db_cache_clear_entries
    // called on it.
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
