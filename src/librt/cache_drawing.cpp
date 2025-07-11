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

#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>

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
#include "bg/pnts.h"
#include "bv/lod.h"
#include "rt/db_instance.h"
#include "rt/db_internal.h"

#include "./librt_private.h"

class GeomData {
    public:
	GeomData(struct bu_cache *dc, int64_t t, const char *n);
	~GeomData();

	int64_t stime;
	struct bu_cache *c;
	char *name;
	unsigned long long user_hash = 0;
};

GeomData::GeomData(struct bu_cache *dc, int64_t t, const char *n)
{
    c = dc;
    stime = t;
    name = bu_strdup(n);
}


GeomData::~GeomData()
{
    bu_free(name, "name");
}

int64_t cache_timestamp(const char *ckey, struct bu_cache *c)
{
    void *vdata;
    int64_t ctimestmp = 0;
    struct bu_cache_txn *txn = NULL;
    if (bu_cache_get(&vdata, ckey, c, &txn))
	memcpy(&ctimestmp, vdata, sizeof(int64_t));
    bu_cache_get_done(&txn);
    return ctimestmp;
}

int
cache_update_attrs(struct bu_cache *c, const char *name, unsigned long long hash, struct bu_attribute_value_set *c_avs, int64_t stime)
{
    static char ckey[BU_CACHE_KEY_MAXLEN];
    static char tkey[BU_CACHE_KEY_MAXLEN];

    if (!c || !c_avs || !hash)
	return BRLCAD_ERROR;

    if (stime)
	snprintf(tkey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_TIMESTAMP);

    // Region flag.
    int rflag = 0;
    const char *region_flag_str = bu_avs_get(c_avs, "region");
    if (region_flag_str && (BU_STR_EQUAL(region_flag_str, "R") || BU_STR_EQUAL(region_flag_str, "1")))
	rflag = 1;
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_FLAG);

    // Before writing cache, check for updated timestamp - if we have a non-zero stime and the cache
    // time is newer, we're aborting
    if (stime && cache_timestamp(tkey, c) > stime)
	return BRLCAD_ERROR;

    bu_semaphore_acquire(BU_SEM_CACHE);
    if (!bu_cache_write(&rflag, sizeof(int), ckey, c)) {
	bu_semaphore_release(BU_SEM_CACHE);
	bu_log("%s: region_flag cache write failure\n", name);
	return BRLCAD_ERROR;
    }
    bu_semaphore_release(BU_SEM_CACHE);

    // Region id.  For drawing purposes this needs to be a number.
    int region_id = -1;
    const char *region_id_str = bu_avs_get(c_avs, "region_id");
    if (region_id_str)
	bu_opt_int(NULL, 1, &region_id_str, (void *)&region_id);
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_ID);

    // Before writing cache, check for updated timestamp - if we have a non-zero stime and the cache
    // time is newer, we're aborting
    if (stime && cache_timestamp(tkey, c) > stime)
	return BRLCAD_ERROR;

    bu_semaphore_acquire(BU_SEM_CACHE);
    if (!bu_cache_write(&region_id, sizeof(int), ckey, c)) {
	bu_semaphore_release(BU_SEM_CACHE);
	bu_log("%s: region_id cache write failure\n", name);
	return BRLCAD_ERROR;
    }
    bu_semaphore_release(BU_SEM_CACHE);

    // Inherit flag
    int color_inherit = (BU_STR_EQUAL(bu_avs_get(c_avs, "inherit"), "1")) ? 1 : 0;
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_INHERIT_FLAG);

    // Before writing cache, check for updated timestamp - if we have a non-zero stime and the cache
    // time is newer, we're aborting
    if (stime && cache_timestamp(tkey, c) > stime)
	return BRLCAD_ERROR;

    bu_semaphore_acquire(BU_SEM_CACHE);
    if (!bu_cache_write(&color_inherit, sizeof(int), ckey, c)) {
	bu_semaphore_release(BU_SEM_CACHE);
	bu_log("%s: color_inherit cache write failure\n", name);
	return BRLCAD_ERROR;
    }
    bu_semaphore_release(BU_SEM_CACHE);

    // Color
    unsigned int colors = UINT_MAX; // Using UINT_MAX to indicate unset
    const char *color_str = bu_avs_get(c_avs, "color");
    if (!color_str)
	color_str = bu_avs_get(c_avs, "rgb");
    if (color_str) {
	struct bu_color color;
	bu_opt_color(NULL, 1, &color_str, (void *)&color);
	// Serialize for cache
	int r, g, b;
	bu_color_to_rgb_ints(&color, &r, &g, &b);
	colors = r + (g << 8) + (b << 16);
    }
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_COLOR);

    // Before writing cache, check for updated timestamp - if we have a non-zero stime and the cache
    // time is newer, we're aborting
    if (stime && cache_timestamp(tkey, c) > stime)
	return BRLCAD_ERROR;

    bu_semaphore_acquire(BU_SEM_CACHE);
    if (!bu_cache_write(&colors, sizeof(unsigned int), ckey, c)) {
	bu_semaphore_release(BU_SEM_CACHE);
	bu_log("%s: color cache write failure\n", name);
	return BRLCAD_ERROR;
    }
    bu_semaphore_release(BU_SEM_CACHE);

    return BRLCAD_OK;
}

int
cache_mesh_update(struct bu_cache *c, struct rt_db_internal *ip, const char *name)
{
    if (!c || !ip || !name)
	return BRLCAD_ERROR;

    if (ip->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
	return BRLCAD_OK;

    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);
    //bu_log("Caching LoD for %s\n", name);

    // Generate and write new data
    bu_semaphore_acquire(BU_SEM_CACHE);
    int cret = bv_lod_mesh_cache(c, name, (const point_t *)bot->vertices, bot->num_vertices, NULL, bot->faces, bot->num_faces, 0.66);
    if (!cret) {
	bu_semaphore_release(BU_SEM_CACHE);
	bu_log("Error processing %s - unable to generate LoD data\n", name);
	return BRLCAD_ERROR;
    }
    bu_semaphore_release(BU_SEM_CACHE);

    // Make sure we can retrieve the cached data
    // TODO - may not really be necessary to verify this here once we're
    // working - including during early stages for testing.
    struct bv_lod_mesh *lod = bv_lod_mesh_get(c, name);
    if (!lod) {
	bu_log("Error processing %s - unable to retrieve LoD data\n", name);
	return BRLCAD_ERROR;
    }
    bv_lod_mesh_put(lod);

    return BRLCAD_OK;
}

// aabb, obb and especially LoD can take longer for very large database
// objects.  We also want each individual op to complete as quickly as possible
// - the sooner we have at least an aabb, the sooner we can draw SOMETHING on
// the screen.
//
// Current idea is to have three queues in three threads, with the aabb getting
// dps from the draw_init routine, cracking them, and doing the aabb before
// handing off the internal data to obb.  obb will in turn do its calculation
// before handing off to the LoD routine.
//
// LoD, which is potentially the slowest of the ops, will do its thing and then
// free the internal.
//
// That way, we only have to crack the internal once, the aabb and obb won't
// have to wait on the LoD, and it can all happen automatically without
// freezing the GUI.

void
aabb_calc(std::shared_ptr<ProcessDrawData> p)
{
    struct rt_db_internal *i = NULL;
    char ckey[BU_CACHE_KEY_MAXLEN];
    char tkey[BU_CACHE_KEY_MAXLEN];

    // Check what's in the pipeline
    p.get()->m_aabb.lock();
    bool aabb_empty = p.get()->q_aabb.empty();
    p.get()->m_aabb.unlock();

    while (!aabb_empty || !p.get()->init_done) {
	if (aabb_empty) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    // Check what's in the pipeline
	    p.get()->m_aabb.lock();
	    aabb_empty = p.get()->q_aabb.empty();
	    p.get()->m_aabb.unlock();
	    continue;
	}
	p.get()->m_aabb.lock();
	i = p.get()->q_aabb.front();
	p.get()->q_aabb.pop();
	p.get()->m_aabb.unlock();

	GeomData *gd = (GeomData *)i->idb_uptr;
	snprintf(tkey, BU_CACHE_KEY_MAXLEN, "%llu:%s", gd->user_hash, CACHE_TIMESTAMP);
	int64_t ctime = cache_timestamp(tkey, p.get()->dbip->i->c);

	if (ctime > gd->stime || p.get()->shutdown) {
	    if (ctime > gd->stime)
		bu_log("Newer timestamp found, aborting aabb init\n");
	    if (p.get()->shutdown)
		bu_log("Shutting down, aborting aabb init\n");
	    delete gd;
	    rt_db_free_internal(i);
	    BU_PUT(i, struct rt_db_internal);

	    // Check what's in the pipeline
	    p.get()->m_aabb.lock();
	    aabb_empty = p.get()->q_aabb.empty();
	    p.get()->m_aabb.unlock();

	    continue;
	}

	// Do aabb calc
	if (i->idb_meth->ft_bbox) {
	    const struct bn_tol btol = BN_TOL_INIT_TOL;
	    point_t bb[2];
	    if (!(i->idb_meth->ft_bbox(i, &bb[0], &bb[1], &btol) < 0)) {
		snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", gd->user_hash, CACHE_AABB);
		bu_semaphore_acquire(BU_SEM_CACHE);
		if (!bu_cache_write(&bb, 2*sizeof(point_t), ckey, p.get()->dbip->i->c)) {
		    bu_log("%s: aabb cache write failure\n", gd->name);
		}
		bu_semaphore_release(BU_SEM_CACHE);
	    } else {
		bu_log("%s: aabb calc failure\n", gd->name);
	    }
	}

	// Queue up for OBB calc
	p.get()->m_obb.lock();
	p.get()->q_obb.push(i);
	p.get()->m_obb.unlock();

	// Check what's in the pipeline
	p.get()->m_aabb.lock();
	aabb_empty = p.get()->q_aabb.empty();
	p.get()->m_aabb.unlock();
    }
}

void
obb_calc(std::shared_ptr<ProcessDrawData> p)
{
    struct rt_db_internal *i = NULL;
    char ckey[BU_CACHE_KEY_MAXLEN];
    char tkey[BU_CACHE_KEY_MAXLEN];

    // Check what's in the pipeline
    p.get()->m_aabb.lock();
    bool aabb_empty = p.get()->q_aabb.empty();
    p.get()->m_aabb.unlock();
    p.get()->m_obb.lock();
    bool obb_empty = p.get()->q_obb.empty();
    p.get()->m_obb.unlock();

    while (!aabb_empty || !obb_empty || !p.get()->init_done) {
	if (obb_empty) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));

	    // Check what's in the pipeline
	    p.get()->m_aabb.lock();
	    aabb_empty = p.get()->q_aabb.empty();
	    p.get()->m_aabb.unlock();
	    p.get()->m_obb.lock();
	    obb_empty = p.get()->q_obb.empty();
	    p.get()->m_obb.unlock();

	    continue;
	}
	p.get()->m_obb.lock();
	i = p.get()->q_obb.front();
	p.get()->q_obb.pop();
	p.get()->m_obb.unlock();

	GeomData *gd = (GeomData *)i->idb_uptr;
	snprintf(tkey, BU_CACHE_KEY_MAXLEN, "%llu:%s", gd->user_hash, CACHE_TIMESTAMP);
	int64_t ctime = cache_timestamp(tkey, p.get()->dbip->i->c);

	if (ctime > gd->stime || p.get()->shutdown) {
	    if (ctime > gd->stime)
		bu_log("Newer timestamp found, aborting obb init\n");
	    if (p.get()->shutdown)
		bu_log("Shutting down, aborting obb init\n");
	    delete gd;
	    rt_db_free_internal(i);
	    BU_PUT(i, struct rt_db_internal);

	    // Check what's in the pipeline.  Since we're shutting down,
	    // only the local queue is of concern.
	    p.get()->m_obb.lock();
	    obb_empty = p.get()->q_obb.empty();
	    p.get()->m_obb.unlock();

	    continue;
	}

	// Do obb calc
	if (i->idb_meth->ft_oriented_bbox) {
	    double btol = BN_TOL_DIST;
	    struct rt_arb_internal arb;
	    arb.magic = RT_ARB_INTERNAL_MAGIC;
	    if (!(i->idb_meth->ft_oriented_bbox(&arb, i, btol) < 0)) {
		vect_t obb[4] = {VINIT_ZERO};
		bg_pnts_obb(&obb[0], &obb[1], &obb[2], &obb[3], (const point_t *)arb.pt, 8);
		bool cfailure = false;
		for (int k = 0; k < 4; k++) {
		    if (VNEAR_ZERO(obb[k], VUNITIZE_TOL)) {
			bu_log("%s: obb calculation failure\n", gd->name);
			cfailure = true;
		    }
		}
		if (!cfailure){
		    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", gd->user_hash, CACHE_OBB);
		    bu_semaphore_acquire(BU_SEM_CACHE);
		    if (!bu_cache_write(&obb, 4*sizeof(vect_t), ckey, p.get()->dbip->i->c)) {
			bu_log("%s: obb cache write failure\n", gd->name);
		    }
		    bu_semaphore_release(BU_SEM_CACHE);
		} else {
		    bg_pnts_obb(&obb[0], &obb[1], &obb[2], &obb[3], (const point_t *)arb.pt, 8);
		}
	    } else {
		bu_log("%s: obb calc failure\n", gd->name);
	    }
	}

	// Queue up for LoD calc
	p.get()->m_lod.lock();
	p.get()->q_lod.push(i);
	p.get()->m_lod.unlock();

	// Check what's in the pipeline
	p.get()->m_aabb.lock();
	aabb_empty = p.get()->q_aabb.empty();
	p.get()->m_aabb.unlock();
	p.get()->m_obb.lock();
	obb_empty = p.get()->q_obb.empty();
	p.get()->m_obb.unlock();
    }
}

void
lod_calc(std::shared_ptr<ProcessDrawData> p)
{
    struct rt_db_internal *i = NULL;
    char tkey[BU_CACHE_KEY_MAXLEN];

    // Check what's in the pipeline
    p.get()->m_aabb.lock();
    bool aabb_empty = p.get()->q_aabb.empty();
    p.get()->m_aabb.unlock();
    p.get()->m_obb.lock();
    bool obb_empty = p.get()->q_obb.empty();
    p.get()->m_obb.unlock();
    p.get()->m_lod.lock();
    bool lod_empty = p.get()->q_lod.empty();
    p.get()->m_lod.unlock();

    while (!aabb_empty || !obb_empty || !lod_empty || !p.get()->init_done) {
	if (lod_empty) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));

	    // Check what's in the pipeline
	    p.get()->m_aabb.lock();
	    aabb_empty = p.get()->q_aabb.empty();
	    p.get()->m_aabb.unlock();
	    p.get()->m_obb.lock();
	    obb_empty = p.get()->q_obb.empty();
	    p.get()->m_obb.unlock();
	    p.get()->m_lod.lock();
	    lod_empty = p.get()->q_lod.empty();
	    p.get()->m_lod.unlock();

	    continue;
	}
	p.get()->m_lod.lock();
	i = p.get()->q_lod.front();
	p.get()->q_lod.pop();
	p.get()->m_lod.unlock();

	GeomData *gd = (GeomData *)i->idb_uptr;
	snprintf(tkey, BU_CACHE_KEY_MAXLEN, "%llu:%s", gd->user_hash, CACHE_TIMESTAMP);
	int64_t ctime = cache_timestamp(tkey, p.get()->dbip->i->c);

	if (ctime > gd->stime || p.get()->shutdown) {
	    if (ctime > gd->stime)
		bu_log("Newer timestamp found, aborting LoD init\n");
	    if (p.get()->shutdown)
		bu_log("Shutting down, aborting Lod init\n");
	    delete gd;
	    rt_db_free_internal(i);
	    BU_PUT(i, struct rt_db_internal);
	    // We didn't actually complete the processing of i, so don't increment
	    if (!p.get()->shutdown)
		p.get()->dbip->i->p_completed++;

	    // Check what's in the pipeline.  Since we're shutting down,
	    // only the local queue is of concern.
	    p.get()->m_lod.lock();
	    lod_empty = p.get()->q_lod.empty();
	    p.get()->m_lod.unlock();

	    continue;
	}

	// Do LoD setup
	cache_mesh_update(gd->c, i, gd->name);

	// This is the last step - free internal
	delete gd;
	rt_db_free_internal(i);
	BU_PUT(i, struct rt_db_internal);
	p.get()->dbip->i->p_completed++;

	// Check what's in the pipeline
	p.get()->m_aabb.lock();
	aabb_empty = p.get()->q_aabb.empty();
	p.get()->m_aabb.unlock();
	p.get()->m_obb.lock();
	obb_empty = p.get()->q_obb.empty();
	p.get()->m_obb.unlock();
	p.get()->m_lod.lock();
	lod_empty = p.get()->q_lod.empty();
	p.get()->m_lod.unlock();

    }
}

int
db_cache_init(struct db_i *dbip, int verbose)
{
    static char ckey[BU_CACHE_KEY_MAXLEN];

    if (!dbip || !dbip->i)
	return BRLCAD_OK;

    dbip->i->c = NULL;
    if (!dbip->dbi_filename || !strlen(dbip->dbi_filename)) {
	dbip->i->init_complete = true;
	if (verbose)
	    bu_log("inmem databases do not support on-disk caches.");
	return BRLCAD_OK;
    }

    // We want the cache to be able to hold all the info we might want to
    // generate about the database for drawing purposes, so set the max file
    // size of the cache to 2x that of the database.  If we have a small
    // database, just use the default cache size - the idea is we don't want to
    // run into a cache limit in normal use, but we also don't want to eat the
    // whole drive if there is some sort of error.
    long long fsize = 2*bu_file_size(dbip->dbi_filename);
    if (fsize < BU_CACHE_DEFAULT_DB_SIZE)
	fsize = 0;
    dbip->i->c = bu_cache_open(dbip->dbi_filename, 1, fsize);
    if (!dbip->i->c) {
	dbip->i->init_complete = true;
	return BRLCAD_ERROR;
    }
    struct bu_cache *c = dbip->i->c;
    int64_t init_start = bu_gettime();

    dbip->i->p = std::make_shared<ProcessDrawData>();
    dbip->i->p.get()->dbip = dbip;
    dbip->i->p_completed = 0;

    // Collect the set of directory pointers we need to process.
    std::unordered_set<struct directory *> &to_init = *dbip->i->to_init;
    struct directory *dp;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
		continue;
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
		continue;
	    to_init.insert(dp);
	}
    }
    size_t target_cnt = to_init.size();

    // Set up processing threads.
    std::thread t_aabb(aabb_calc, dbip->i->p);
    std::thread t_obb(obb_calc, dbip->i->p);
    std::thread t_lod(lod_calc, dbip->i->p);

    int64_t overall_start = bu_gettime();
    int64_t elapsed = 0;
    fastf_t seconds = 0.0;
    struct bu_vls keystr = BU_VLS_INIT_ZERO;

    /* Because we're supposed to be doing init in a thread, there is always a
     * chance another thread is working on the same database object.  To avoid
     * trouble, we check for a timestamp associated with the dp at all stages
     * where we plan to write data to the cache.  If at any point the timestamp
     * in the cache is newer than our start timestamp for the dp, we immediately
     * abandon processing that dp and leave it up to whatever other thread is
     * working with the dp. */
    bu_semaphore_acquire(BU_SEM_CACHE);
    dp = (to_init.size() > 0) ? *to_init.begin() : NULL;
    bool valid_dp = false;
    while (dp && !valid_dp) {
	int64_t dp_start = bu_gettime();
	unsigned long long hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_TIMESTAMP);
	if (cache_timestamp(ckey, c) > dp_start)
	    dp = NULL;
	if (dp) {
	    valid_dp = true;
	    to_init.erase(dp);
	} else {
	    dp = (to_init.size() > 0) ? *to_init.begin() : NULL;
	}
    }
    bu_semaphore_release(BU_SEM_CACHE);

    while (dp) {
	if (UNLIKELY(init_start < bu_file_timestamp(dbip->dbi_filename))) {
	    // File changed before initial cache generation completed - but librt
	    // I/O routines didn't populate to_init - external edit?
	    // Abort and start over - we know nothing about what happened...
	    bu_semaphore_acquire(BU_SEM_CACHE);
	    to_init.clear();
	    bu_cache_close(dbip->i->c);
	    bu_semaphore_release(BU_SEM_CACHE);
	    return db_cache_init(dbip, verbose);
	}

	// If the parent thread has decided to close the dbip, we can
	// stop now.
	if (dbip->i->shutdown_requested) {
	    bu_vls_free(&keystr);

	    // Tell the geom threads we're shutting down
	    dbip->i->p.get()->shutdown = true;

	    // Wait for the aabb, obb, lod threads to finish up
	    t_aabb.join();
	    t_obb.join();
	    t_lod.join();

	    dbip->i->init_complete = true;
	    return BRLCAD_OK;
	}

	// Commence processing.
	int64_t dp_start = bu_gettime();

	if (verbose > 1) {
	    bu_log("Processing(%zd):  %s\n", dbip->i->p_completed+1, dp->d_namep);
	}

	// Immediately pull the necessary data from the dp/disk into memory
	// (This makes us independent to any librt changes to the struct
	// directory data.)

	// Set up data container
	GeomData *gd = new GeomData(c, dp_start, dp->d_namep);

	// Read the attributes from the database object
	struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
	bu_semaphore_acquire(BU_SEM_CACHE);
	if (db5_get_attributes(dbip, &c_avs, dp) < 0) {
	    bu_semaphore_release(BU_SEM_CACHE);
	    delete gd;
	    continue;
	}
	bu_semaphore_release(BU_SEM_CACHE);

	// Read the geometry from the database object
	struct rt_db_internal *ip;
	BU_GET(ip, struct rt_db_internal);
	RT_DB_INTERNAL_INIT(ip);
	bu_semaphore_acquire(BU_SEM_CACHE);
	int ret = rt_db_get_internal(ip, dp, dbip, NULL, &rt_uniresource);
	if (ret < 0) {
	    bu_semaphore_release(BU_SEM_CACHE);
	    BU_PUT(ip, struct rt_db_internal);
	    delete gd;
	    continue;
	}
	bu_semaphore_release(BU_SEM_CACHE);
	if (ip->idb_minor_type != dp->d_minor_type) {
	    bu_log("Error processing %s - mismatch between d_minor_type (%c) and idb_minor_type (%c)\n", dp->d_namep, dp->d_minor_type, ip->idb_minor_type);
	    rt_db_free_internal(ip);
	    BU_PUT(ip, struct rt_db_internal);
	    continue;
	}

	unsigned long long user_key = bu_data_hash(gd->name, strlen(gd->name)*sizeof(char));
	gd->user_hash = user_key;
	snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", user_key, CACHE_TIMESTAMP);
	if (cache_timestamp(ckey, c)) {
	    // If we already have a populated cache, assume it is valid.
	    // Validating and resetting preexisting data in the cache against the
	    // .g is outside the scope of the init routine.
	    dp = (to_init.size() > 0) ? *to_init.begin() : NULL;
	    if (dp)
		to_init.erase(dp);
	    bu_avs_free(&c_avs);
	    rt_db_free_internal(ip);
	    BU_PUT(ip, struct rt_db_internal);
	    delete gd;
	    continue;
	}

	// First order of business is to write the timestamp
	bu_semaphore_acquire(BU_SEM_CACHE);
	if (!bu_cache_write(&dp_start, sizeof(int64_t), ckey, c)) {
	    bu_log("%s: timestamp cache write failure\n", dp->d_namep);
	    bu_cache_write(&dp_start, sizeof(int64_t), ckey, c);
	    bu_semaphore_release(BU_SEM_CACHE);
	    bu_avs_free(&c_avs);
	    rt_db_free_internal(ip);
	    BU_PUT(ip, struct rt_db_internal);
	    delete gd;
	    continue;
	}
	bu_semaphore_release(BU_SEM_CACHE);

	// Get the properties we need from attributes
	if (cache_update_attrs(c, gd->name, user_key, &c_avs, gd->stime) != BRLCAD_OK) {
	    bu_semaphore_release(BU_SEM_CACHE);
	    bu_avs_free(&c_avs);
	    rt_db_free_internal(ip);
	    BU_PUT(ip, struct rt_db_internal);
	    delete gd;
	    continue;
	}
	bu_avs_free(&c_avs);

	// Kick off the geometry processing
	ip->idb_uptr = gd;
	dbip->i->p.get()->m_aabb.lock();
	dbip->i->p.get()->q_aabb.push(ip);
	dbip->i->p.get()->m_aabb.unlock();

	bu_semaphore_acquire(BU_SEM_CACHE);
	dp = (to_init.size() > 0) ? *to_init.begin() : NULL;
	if (dp)
	    to_init.erase(dp);
	bu_semaphore_release(BU_SEM_CACHE);

	// If the user has requested it, let them know what is happening
	if (verbose && seconds > 5.0) {
	    elapsed = bu_gettime() - overall_start;
	    seconds = elapsed / 1000000.0;
	    size_t completed = dbip->i->p_completed;
	    bu_log("Drawing cache processing (%g seconds): completed %zd of %zd BoTs\n", seconds, completed, target_cnt);
	}

    }

    bu_vls_free(&keystr);

    // Wait for the aabb, obb, lod threads to finish up
    dbip->i->p.get()->init_done = true;
    t_aabb.join();
    t_obb.join();
    t_lod.join();

    if (verbose) {
	elapsed = bu_gettime() - overall_start;
	int rseconds = elapsed / 1000000;
	int rminutes = rseconds / 60;
	int rhours = rminutes / 60;
	rminutes = rminutes % 60;
	rseconds = rseconds % 60;
	bu_log("Draw caching complete (Elapsed time: %02d:%02d:%02d)\n", rhours, rminutes, rseconds);
    }

    // TODO - Populate cache_geom_uses
    //
    // Note that this is a cache-only operation - the dbip isn't involved
    // except as storage.  That way, garbage collect will be able to clear out
    // old, unused geometry info as well.

    dbip->i->init_complete = true;
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
db_cache_update(struct db_i *dbip, const char *name, int attr_only)
{
    static char ckey[BU_CACHE_KEY_MAXLEN];

    if (!dbip || !dbip->i || !dbip->i->c) {
	bu_log("db_cache_update: invalid dbip input\n");
	return BRLCAD_ERROR;
    }

    struct bu_cache *c = dbip->i->c;

    // No-op
    if (!name)
	return BRLCAD_OK;

    // If we have existing data, clear it.
    // TODO - do this right...
    //bu_cache_clear(name, c);

    // Do the lookup
    struct directory *dp = db_lookup(dbip, name, LOOKUP_NOISY);
    if (dp == RT_DIR_NULL) {
	bu_log("db_cache_update: db_lookup failed for %s\n", name);
	return BRLCAD_ERROR;
    }

    // Get the name hash
    unsigned long long hash = bu_data_hash(name, strlen(name)*sizeof(char));

    // First order of business is to write the timestamp.  Unlike
    // db_cache_init, if we call this routine the presumption is we have the
    // most current info.
    int64_t dp_start = bu_gettime();
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_TIMESTAMP);
    bu_semaphore_acquire(BU_SEM_CACHE);
    if (!bu_cache_write(&dp_start, sizeof(int64_t), ckey, c)) {
	bu_semaphore_release(BU_SEM_CACHE);
	bu_log("%s: timestamp cache write failure\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    bu_semaphore_release(BU_SEM_CACHE);

    // Read the attributes from the database object
    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
    if (db5_get_attributes(dbip, &c_avs, dp) < 0) {
	bu_log("%s: avs lookup failure\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    if (cache_update_attrs(dbip->i->c, name, hash, &c_avs, 0) != BRLCAD_OK) {
	bu_avs_free(&c_avs);
	bu_log("%s: cache_update_attrs failure\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    bu_avs_free(&c_avs);

    // If we're only updating the cached attributes (i.e. the
    // calling code knows the geometry wasn't changed) we can
    // stop now.
    if (attr_only)
	return BRLCAD_OK;

    // Remaining steps require geometry info
    struct rt_db_internal *ip;
    BU_GET(ip, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(ip);
    int ret = rt_db_get_internal(ip, dp, dbip, NULL, &rt_uniresource);
    if (ret < 0) {
	bu_log("db_cache_update: rt_get_internal failed for %s\n", name);
	return BRLCAD_ERROR;
    }
    if (ip->idb_minor_type != dp->d_minor_type) {
	bu_log("Error processing %s - mismatch between d_minor_type (%c) and idb_minor_type (%c)\n", dp->d_namep, dp->d_minor_type, ip->idb_minor_type);
	rt_db_free_internal(ip);
	BU_PUT(ip, struct rt_db_internal);
	return BRLCAD_ERROR;
    }

    // Do aabb calc
    if (ip->idb_meth->ft_bbox) {
	const struct bn_tol btol = BN_TOL_INIT_TOL;
	point_t bb[2];
	if (!(ip->idb_meth->ft_bbox(ip, &bb[0], &bb[1], &btol) < 0)) {
	    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_AABB);
	    bu_semaphore_acquire(BU_SEM_CACHE);
	    if (!bu_cache_write(&bb, 2*sizeof(point_t), ckey, c)) {
		bu_log("%s: aabb cache write failure\n", name);
	    }
	    bu_semaphore_release(BU_SEM_CACHE);
	}
    }

    // Do obb calc
    if (ip->idb_meth->ft_oriented_bbox) {
	double btol = BN_TOL_DIST;
	struct rt_arb_internal arb;
	arb.magic = RT_ARB_INTERNAL_MAGIC;
	if (!(ip->idb_meth->ft_oriented_bbox(&arb, ip, btol) < 0)) {
	    vect_t obb[4] = {VINIT_ZERO};
	    bg_pnts_obb(&obb[0], &obb[1], &obb[2], &obb[3], (const point_t *)arb.pt, 8);
	    bool cfailure = false;
	    for (int k = 0; k < 4; k++) {
		if (VNEAR_ZERO(obb[k], VUNITIZE_TOL)) {
		    bu_log("%s: obb calculation failure\n", name);
		    cfailure = true;
		}
	    }
	    if (!cfailure){
		snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_OBB);
		bu_semaphore_acquire(BU_SEM_CACHE);
		if (!bu_cache_write(&obb, 4*sizeof(vect_t), ckey, c)) {
		    bu_log("%s: obb cache write failure\n", name);
		}
		bu_semaphore_release(BU_SEM_CACHE);
	    }
	}
    }

    if (cache_mesh_update(dbip->i->c, ip, dp->d_namep) != BRLCAD_OK)
	bu_log("%s: cache_mesh_update failure\n", dp->d_namep);

    rt_db_free_internal(ip);
    BU_PUT(ip, struct rt_db_internal);

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
    struct bu_cache_txn *txn = NULL;
    void *vdata = NULL;
    if (!bu_cache_get(&vdata, ckey, c, &txn)) {
	// If we don't have a geometry key, we're done
	bu_cache_get_done(&txn);
	return;
    }
    unsigned long long ghash;
    memcpy(&ghash, vdata, sizeof(unsigned long long));
    bu_cache_get_done(&txn);
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
