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

CacheItem::CacheItem()
{
    erase_op = false;
    data_len = 0;
    data = NULL;
}

CacheItem::CacheItem(const char *dkey, void *dval, size_t dlen)
{
    snprintf(key, BU_CACHE_KEY_MAXLEN, "%s", dkey);
    erase_op = (!dval || !dlen) ? true : false;
    data_len = dlen;
    if (dval && data_len) {
	data = bu_malloc(data_len, "cache data");
	memcpy(data, dval, data_len);
    } else {
	data = NULL;
    }
}

CacheItem::CacheItem(const CacheItem &o)
{
    erase_op = o.erase_op;
    data_len = o.data_len;
    snprintf(key, BU_CACHE_KEY_MAXLEN, "%s", o.key);
    if (o.data) {
	data = bu_malloc(o.data_len, "cache data");
	memcpy(data, o.data, o.data_len);
    }
}

CacheItem& CacheItem::operator=(const CacheItem& o)
{
    if (this == &o)
        return *this;

    bu_free(data, "free prev data");
    data = NULL;

    erase_op = o.erase_op;
    data_len = o.data_len;
    snprintf(key, BU_CACHE_KEY_MAXLEN, "%s", o.key);
    if (o.data) {
	data = bu_malloc(o.data_len, "cache data");
	memcpy(data, o.data, o.data_len);
    }
    return *this;
}

CacheItem::~CacheItem()
{
    bu_free(data, "free data");
    data = NULL;
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
attr_calc(std::shared_ptr<ProcessDrawData> p)
{
    char ckey[BU_CACHE_KEY_MAXLEN];

    // Until we close this database, monitor for inputs to process.
    while (!p.get()->shutdown) {
	if (!p.get()->q_attr.size_approx()) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    continue;
	}

	std::string name;
	if (!p.get()->q_attr.try_dequeue(name))
	    continue;

	// Got a name - look up the attributes and internal
	struct directory *dp = db_lookup(p.get()->dbip, name.c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL)
	    continue;
	struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
	if (db5_get_attributes(p.get()->dbip, &c_avs, dp) < 0)
	    continue;
	struct rt_db_internal *ip;
	BU_GET(ip, struct rt_db_internal);
	RT_DB_INTERNAL_INIT(ip);
	int ret = rt_db_get_internal(ip, dp, p.get()->dbip, NULL, &rt_uniresource);
	if (ret < 0) {
	    BU_PUT(ip, struct rt_db_internal);
	    bu_avs_free(&c_avs);
	    continue;
	}
	ip->idb_uptr = bu_strdup(name.c_str());

	// We will not be directly writing to the cache from this thread - instead
	// we will queue up CacheItem entries to be written from the q_write thread.
	unsigned long long hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));


	// Region flag.
	int rflag = 0;
	const char *region_flag_str = bu_avs_get(&c_avs, "region");
	if (region_flag_str && (BU_STR_EQUAL(region_flag_str, "R") || BU_STR_EQUAL(region_flag_str, "1")))
	    rflag = 1;
	snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_FLAG);
	CacheItem ritem(ckey, &rflag, sizeof(int));
	p.get()->q_write.enqueue(ritem);
	bu_log("enqueue: %s:%s\n", dp->d_namep, ckey);

	// Region id.  For drawing purposes this needs to be a number.
	int region_id = -1;
	const char *region_id_str = bu_avs_get(&c_avs, "region_id");
	if (region_id_str)
	    bu_opt_int(NULL, 1, &region_id_str, (void *)&region_id);
	snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_REGION_ID);
	CacheItem reg_item(ckey, &region_id, sizeof(int));
	p.get()->q_write.enqueue(reg_item);
	bu_log("enqueue: %s:%s\n", dp->d_namep, ckey);

	// Inherit flag
	int color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;
	snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_INHERIT_FLAG);
	CacheItem inherit_item(ckey, &color_inherit, sizeof(int));
	p.get()->q_write.enqueue(inherit_item);
	bu_log("enqueue: %s:%s\n", dp->d_namep, ckey);

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
	CacheItem colors_item(ckey, &colors, sizeof(unsigned int));
	p.get()->q_write.enqueue(colors_item);
	bu_log("enqueue: %s:%s\n", dp->d_namep, ckey);

	// Done with attribute data
	bu_avs_free(&c_avs);

	// Hand ip off to the aabb queue
	p.get()->q_aabb.enqueue(ip);
    }

    p.get()->thread_cnt--;
}

void
aabb_calc(std::shared_ptr<ProcessDrawData> p)
{
    char ckey[BU_CACHE_KEY_MAXLEN];

    // Until we close this database, monitor for inputs to process.
    while (!p.get()->shutdown) {
	if (!p.get()->q_aabb.size_approx()) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    continue;
	}

	struct rt_db_internal *ip = NULL;
	if (!p.get()->q_aabb.try_dequeue(ip))
	    continue;

	const char *name = (const char *)ip->idb_uptr;
	unsigned long long hash = bu_data_hash(name, strlen(name)*sizeof(char));
	snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_AABB);

	// Do aabb calc
	if (ip->idb_meth->ft_bbox) {
	    const struct bn_tol btol = BN_TOL_INIT_TOL;
	    point_t bb[2];
	    if (!(ip->idb_meth->ft_bbox(ip, &bb[0], &bb[1], &btol) < 0)) {
		CacheItem aabb_item(ckey, &bb, 2*sizeof(point_t));
		p.get()->q_write.enqueue(aabb_item);
		bu_log("enqueue: %s\n", ckey);
	    } else {
		// If we couldn't get a bbox, clear out any old entry
		CacheItem aabb_item(ckey, NULL, 0);
		p.get()->q_write.enqueue(aabb_item);
		bu_log("enqueue: %s\n", ckey);
		bu_log("%s: aabb calc failure\n", name);
	    }
	} else {
	    // If a named item changed type, we may need to clear
	    // out an old bbox entry
	    CacheItem aabb_item(ckey, NULL, 0);
	    p.get()->q_write.enqueue(aabb_item);
	    bu_log("enqueue: %s\n", ckey);
	}

	// Hand ip off to the obb queue
	p.get()->q_obb.enqueue(ip);
    }

    p.get()->thread_cnt--;
}

void
obb_calc(std::shared_ptr<ProcessDrawData> p)
{
    char ckey[BU_CACHE_KEY_MAXLEN];

    // Until we close this database, monitor for inputs to process.
    while (!p.get()->shutdown) {
	if (!p.get()->q_obb.size_approx()) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    continue;
	}

	struct rt_db_internal *ip = NULL;
	if (!p.get()->q_obb.try_dequeue(ip))
	    continue;

	const char *name = (const char *)ip->idb_uptr;
	unsigned long long hash = bu_data_hash(name, strlen(name)*sizeof(char));
	snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_OBB);

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
		    CacheItem obb_item(ckey, &obb, 4*sizeof(vect_t));
		    p.get()->q_write.enqueue(obb_item);
		    bu_log("enqueue: %s\n", ckey);
		} else {
		    // If calculation failed, clear out any old data
		    CacheItem obb_item(ckey, NULL, 0);
		    p.get()->q_write.enqueue(obb_item);
		    bu_log("enqueue: %s\n", ckey);

		    bu_log("%s: obb calc failure\n", name);
		    //bg_pnts_obb(&obb[0], &obb[1], &obb[2], &obb[3], (const point_t *)arb.pt, 8);
		}
	    } else {
		// If calculation failed, clear out any old data
		CacheItem obb_item(ckey, NULL, 0);
		p.get()->q_write.enqueue(obb_item);
		bu_log("enqueue: %s\n", ckey);

		bu_log("%s: obb calc failure\n", name);
	    }
	} else {
	    // If a named item changed type, we may need to clear out an old
	    // oriented bbox entry
	    CacheItem obb_item(ckey, NULL, 0);
	    p.get()->q_write.enqueue(obb_item);
	    bu_log("enqueue: %s\n", ckey);
	}

	// Hand ip off to the LoD queue
	p.get()->q_lod.enqueue(ip);
    }

    p.get()->thread_cnt--;
}

void
lod_calc(std::shared_ptr<ProcessDrawData> p)
{
    // Until we close this database, monitor for inputs to process.
    while (!p.get()->shutdown) {
	if (!p.get()->q_lod.size_approx()) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    continue;
	}

	struct rt_db_internal *ip = NULL;
	if (!p.get()->q_lod.try_dequeue(ip))
	    continue;

	const char *name = (const char *)ip->idb_uptr;

	// Do LoD core ops
	struct bu_ptbl cache_items = BU_PTBL_INIT_ZERO;
	if (ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
	    RT_BOT_CK_MAGIC(bot);
	    int cret = bv_lod_mesh_gen(&cache_items, name,
		    (const point_t *)bot->vertices, bot->num_vertices, NULL,
		    bot->faces, bot->num_faces, 0.66);
	    if (!cret) {
		// If LoD failed, clear out old entries
		bv_lod_clear_gen(&cache_items, name, p.get()->dbip->i->c);
	    }
	} else {
	    // We may need to clean up old LoD if an object's type changed
	    bv_lod_clear_gen(&cache_items, name, p.get()->dbip->i->c);
	}

	// Do the actual cache ops
	for (size_t i = 0; i < BU_PTBL_LEN(&cache_items); i++) {
	    struct bu_cache_item *itm = (struct bu_cache_item *)BU_PTBL_GET(&cache_items, i);
	    CacheItem lod_item(itm->key, itm->data, itm->data_len);
	    bu_log("enqueue: %s\n", itm->key);
	    bu_free(itm->data, "bu_cache_item data");
	    bu_free(itm, "bu_cache_item");
	    p.get()->q_write.enqueue(lod_item);
	}

	// LoD calc is the last use of rt_db_internal - free
	bu_free(ip->idb_uptr, "name uptr");
	rt_db_free_internal(ip);
	BU_PUT(ip, struct rt_db_internal);
    }

    p.get()->thread_cnt--;
}

void
q_write(std::shared_ptr<ProcessDrawData> p)
{
    // Until we close this database, monitor for cache items to write.
    while (!p.get()->shutdown) {
	if (!p.get()->q_write.size_approx()) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    continue;
	}

	CacheItem item;
	if (!p.get()->q_write.try_dequeue(item))
	    continue;

	// If this is an erase op, do the erasing
	if (item.erase_op) {
	    bu_cache_clear(item.key, p.get()->dbip->i->c);
	    continue;
	}

	// TODO - see if we need to make copies of item data in order
	// to safely pass to bu_cache_write...
	bu_log("Writing: %s\n", item.key);

	// Try a few times in case the first write doesn't succeed
	int tcnt = 0;
	while (tcnt < 10 && !bu_cache_write(item.data, item.data_len, item.key, p.get()->dbip->i->c)) {
	    bu_log("%s: cache write failure\n", item.key);
	    tcnt++;
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	if (tcnt == 10)
	    bu_log("ERROR:  %s: completely failed to write\n", item.key);
    }

    p.get()->thread_cnt--;
}

#if 0
// TODO - see if we can do this from various threads via env vars
// If the user has requested it, let them know what is happening
if (verbose && seconds > 5.0) {
    elapsed = bu_gettime() - overall_start;
    seconds = elapsed / 1000000.0;
    bu_log("Drawing cache processing (%g seconds): completed %zd of %zd BoTs\n", seconds, completed, target_cnt);
}
#endif

// db_cache_update calls must be made from the same
// thread as this function
int
db_cache_start(struct db_i *dbip, int verbose)
{
    static char ckey[BU_CACHE_KEY_MAXLEN];

    if (!dbip || !dbip->i)
	return BRLCAD_ERROR;

    dbip->i->c = NULL;
    if (!dbip->dbi_filename || !strlen(dbip->dbi_filename)) {
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
	return BRLCAD_ERROR;
    }

    dbip->i->p = std::make_shared<ProcessDrawData>();
    dbip->i->p.get()->dbip = dbip;


    // Set up processing threads.  These will run until the
    // database is closed
    std::thread t_attr(attr_calc, dbip->i->p);
    std::thread t_aabb(aabb_calc, dbip->i->p);
    std::thread t_obb(obb_calc, dbip->i->p);
    std::thread t_lod(lod_calc, dbip->i->p);
    std::thread t_write(q_write, dbip->i->p);
    dbip->i->p.get()->thread_cnt = 5;

    // These threads will run until the database is closed.
    t_attr.detach();
    t_aabb.detach();
    t_obb.detach();
    t_lod.detach();
    t_write.detach();

    // Enqueue the initial set of directory pointers we need to process.
    struct directory *dp;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
		continue;
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
		continue;

	    // TODO - look up timestamp in cache - if present, no further
	    // processing to do.
	    unsigned long long hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_TIMESTAMP);
	    if (bu_cache_get(NULL, ckey, dbip->i->c, NULL))
		continue;

	    // No pre-existing entry.  First order of business is to write the timestamp.
	    int64_t dp_start = bu_gettime();
	    CacheItem ts_item(ckey, &dp_start, sizeof(int64_t));
	    dbip->i->p.get()->q_write.enqueue(ts_item);

	    // Queue up for attr processing.
	    dbip->i->p.get()->q_attr.enqueue(std::string(dp->d_namep));
	}
    }

    // TODO - need to monitor timestamp state to kick off a new init if
    // someone other than us touches the.g file
#if 0
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
#endif

#if 0
    // If the user has requested it, let them know what is happening
    if (verbose && seconds > 5.0) {
	elapsed = bu_gettime() - overall_start;
	seconds = elapsed / 1000000.0;
	bu_log("Drawing cache processing (%g seconds): completed %zd of %zd BoTs\n", seconds, completed, target_cnt);
    }
#endif

    // TODO - Populate cache_geom_uses
    //
    // Note that this is a cache-only operation - the dbip isn't involved
    // except as storage.  That way, garbage collect will be able to clear out
    // old, unused geometry info as well.
    //
    // Might need to have this done by the garbage collect op itself, and have
    // that op wait until the cache queue system reports clear.  Should also
    // lock cache ops when we're mid gc so nobody can queue up until the gc
    // is done.

    return BRLCAD_OK;
}

struct bv_lod_mesh *
db_cache_lod_mesh_get(struct db_i *dbip, const char *name)
{
    if (!dbip || !dbip->i || !dbip->i->c || !name)
	return NULL;

    return bv_lod_mesh_get(dbip->i->c, name);
}

// Must be called from the same thread as db_cache_start
int
db_cache_update(struct db_i *dbip, const char *name)
{
    static char ckey[BU_CACHE_KEY_MAXLEN];

    // no name == no-op
    if (!name)
	return BRLCAD_OK;

    if (!dbip || !dbip->i)
	return BRLCAD_ERROR;

    // db_cache_update is a no-op for inmmem databases
    if (!dbip->dbi_filename || !strlen(dbip->dbi_filename))
	return BRLCAD_OK;

    if (!dbip->i->c) {
	bu_log("db_cache_update: no dbip cache defined\n");
	return BRLCAD_ERROR;
    }

    // Do the lookup
    struct directory *dp = db_lookup(dbip, name, LOOKUP_NOISY);
    if (dp == RT_DIR_NULL) {
	bu_log("db_cache_update: db_lookup failed for %s\n", name);
	return BRLCAD_ERROR;
    }
    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
	return BRLCAD_OK;
    if (dp->d_addr == RT_DIR_PHONY_ADDR)
	return BRLCAD_OK;

    // Update timestamp in cache.  As long as db_cache_update is only
    // called from the same thread as db_cache_start, the timestamp
    // should always end up being the latest version (which is what
    // we want.
    unsigned long long hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, CACHE_TIMESTAMP);
    int64_t dp_start = bu_gettime();
    CacheItem ts_item(ckey, &dp_start, sizeof(int64_t));
    dbip->i->p.get()->q_write.enqueue(ts_item);


    // Enqueue to process
    dbip->i->p.get()->q_attr.enqueue(std::string(dp->d_namep));

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

// Return 1 if there is processing going on, else 0
int
db_cache_processing(struct db_i *dbip)
{
    if (!dbip)
	return 0;

    // Check queues
    if (dbip->i->p.get()->q_attr.size_approx())
	return 1;

    if (dbip->i->p.get()->q_aabb.size_approx())
	return 1;

    if (dbip->i->p.get()->q_obb.size_approx())
	return 1;

    if (dbip->i->p.get()->q_lod.size_approx())
	return 1;

    if (dbip->i->p.get()->q_write.size_approx())
	return 1;

    return 0;
}


// TODO - for garbage collect, get all cache keys, and use something like a
// region flag filter to collect the set of all name hashes

// Hash all active dp names in the dbip, and remove those hashes from
// the name set collected previously

// Anything remaining is garbage, and gets db_cache_clear_entries
// called on it.

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
