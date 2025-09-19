/*                 C A C H E _ D R A W I N G . C P P
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

#include <set>
#include <thread>
#include <vector>

#include "vmath.h"
#include "bu/app.h"
#include "bu/cache.h"
#include "bu/color.h"
#include "bu/env.h"
#include "bu/opt.h"
#include "bu/time.h"
#include "bu/units.h"
#include "bg.h"
#include "bv/lod.h"
#include "raytrace.h"
#include "../librt_private.h" // so we can inspect cache

//#define LOD_TIMING
//#define LOD_TIMING_PER_LEVEL

// Exercise the LoD structs in the cache (TODO - validate data somehow...)
static bool
do_lod(struct db_i *dbip, struct directory *dp)
{
    // For the moment at least, only BoTs have cached LoD data
    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
	return true;

    struct bv_lod_mesh *mlod = db_cache_lod_mesh_get(dbip, dp->d_namep);
    if (!mlod) {
	bu_log("%s - no mesh LoD available\n", dp->d_namep);
	return false;
    }

    struct bv_scene_obj *s = bv_obj_get(NULL);
    s->draw_data = (void *)mlod;

    for (int i = 0; i < 16; i++) {
	bv_lod_level(s, i, 0);
    }

    bv_lod_mesh_put(mlod);
    bv_obj_put(s);
    return true;
}


// For this step, we read the data from both the dp and the cache and compare it
// (for attributes).  In the case of geometry data, we either recalculate it and
// compare it (bboxes) or exercise it (LoD).
static bool
validate_dp(struct db_i *dbip, struct directory *dp)
{
    bool valid = true;
    struct bu_cache_txn *txn = NULL;
    void *cdata = NULL;
    char ckey[BU_CACHE_KEY_MAXLEN];

    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
    if (db5_get_attributes(dbip, &c_avs, dp) < 0) {
	bu_log("ERROR - unable to get attributes: %s\n", dp->d_namep);
	return false;
    }

    unsigned long long user_hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));

    // Region flag
    const char *region_flag_str = bu_avs_get(&c_avs, "region");
    int db_rflag = (region_flag_str && (BU_STR_EQUAL(region_flag_str, "R") || BU_STR_EQUAL(region_flag_str, "1"))) ? 1 : 0;
    int cache_rflag = 0;
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", user_hash, CACHE_REGION_FLAG);
    int cret = bu_cache_get(&cdata, ckey, dbip->i->c, &txn);
    if (cret == sizeof(int)) {
	memcpy(&cache_rflag, cdata, sizeof(int));
	if (cache_rflag < 0 || cache_rflag > 1) {
	    bu_log("Got what should be impossible region flag value: %s %s: %d\n", ckey, dp->d_namep, cache_rflag);
	    valid = false;
	}
	if (db_rflag != cache_rflag) {
	    bu_log("Cache(%d) vs. db(%d) region flags mismatch for %s\n", cache_rflag, db_rflag, dp->d_namep);
	    valid = false;
	}
    } else {
	bu_log("Cache miss for region flag on %s (%d)\n", dp->d_namep, cret);
	valid = false;
    }

    // Region id.
    int db_region_id = -1;
    const char *region_id_str = bu_avs_get(&c_avs, "region_id");
    if (region_id_str)
	bu_opt_int(NULL, 1, &region_id_str, (void *)&db_region_id);
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", user_hash, CACHE_REGION_ID);
    int cache_region_id = -1;
    if (bu_cache_get(&cdata, ckey, dbip->i->c, &txn)) {
	memcpy(&cache_region_id, cdata, sizeof(int));
	if (db_region_id != cache_region_id) {
	    bu_log("Cache(%d) vs. db(%d) region_id mismatch for %s\n", cache_region_id, db_region_id, dp->d_namep);
	    valid = false;
	}
    } else {
    	bu_log("Cache miss for region id on %s\n", dp->d_namep);
	valid = false;
    }

    // Inherit flag
    int db_color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", user_hash, CACHE_INHERIT_FLAG);
    int cache_color_inherit = 0;
    if (bu_cache_get(&cdata, ckey, dbip->i->c, &txn)) {
	memcpy(&cache_color_inherit, cdata, sizeof(int));
	if (db_color_inherit != cache_color_inherit) {
	    bu_log("Cache(%d) vs. db(%d) color_inherit flag mismatch for %s\n", cache_color_inherit, db_color_inherit, dp->d_namep);
	    valid = false;
	}
    } else {
    	bu_log("Cache miss for inherit flag on %s\n", dp->d_namep);
	valid = false;
    }

    // Color
    unsigned int db_colors = UINT_MAX; // Using UINT_MAX to indicate unset
    const char *color_str = bu_avs_get(&c_avs, "color");
    if (!color_str)
	color_str = bu_avs_get(&c_avs, "rgb");
    if (color_str) {
	struct bu_color color;
	bu_opt_color(NULL, 1, &color_str, (void *)&color);
	// Serialize for cache
	int r, g, b;
	bu_color_to_rgb_ints(&color, &r, &g, &b);
	db_colors = r + (g << 8) + (b << 16);
    }
    unsigned int cache_colors = UINT_MAX; // Using UINT_MAX to indicate unset
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", user_hash, CACHE_COLOR);
    if (bu_cache_get(&cdata, ckey, dbip->i->c, &txn)) {
	memcpy(&cache_colors, cdata, sizeof(unsigned int));
	if (db_colors!= cache_colors) {
	    bu_log("Cache(%d) vs. db(%d) color mismatch for %s\n", cache_colors, db_colors, dp->d_namep);
	    valid = false;
	}
    } else {
	bu_log("Cache miss for color on %s\n", dp->d_namep);
	valid = false;
    }

    bu_cache_get_done(&txn);
    bu_avs_free(&c_avs);

    // If we've already failed, don't bother going further
    if (!valid)
	return false;

    // For bounding boxes we need the internal to validate
    const struct bn_tol btol = BN_TOL_INIT_TOL;
    double bdtol = BN_TOL_DIST;
    struct rt_db_internal *ip;
    BU_GET(ip, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(ip);
    int ret = rt_db_get_internal(ip, dp, dbip, NULL, &rt_uniresource);
    if (ret < 0) {
	BU_PUT(ip, struct rt_db_internal);
	bu_log("Failed to get internal for %s\n", dp->d_namep);
	return false;
    }

    // Check Axis-Aligned Bounding Box, if primitive supports it
    if (ip->idb_meth->ft_bbox) {
	point_t db_aabb[2] = {VINIT_ZERO};
	if (ip->idb_meth->ft_bbox(ip, &db_aabb[0], &db_aabb[1], &btol) < 0) {
	    valid = false;
	} else {
	    point_t cache_aabb[2] = {VINIT_ZERO};
	    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", user_hash, CACHE_AABB);
	    if (bu_cache_get(&cdata, ckey, dbip->i->c, &txn)) {
		memcpy(&cache_aabb, cdata, 2*sizeof(point_t));
		if (!VNEAR_EQUAL(db_aabb[0], cache_aabb[0], VUNITIZE_TOL) || !VNEAR_EQUAL(db_aabb[1], cache_aabb[1], VUNITIZE_TOL)) {
		    bu_log("Cache aabb vs. db aabb mismatch for %s\n", dp->d_namep);
		    bu_log("   db_aabb: (%0.15f %0.15f %0.15f) -> (%0.15f %0.15f %0.15f)\n", V3ARGS(db_aabb[0]), V3ARGS(db_aabb[1]));
		    bu_log("cache_aabb: (%0.15f %0.15f %0.15f) -> (%0.15f %0.15f %0.15f)\n", V3ARGS(cache_aabb[0]), V3ARGS(cache_aabb[1]));
		    valid = false;
		}
	    } else {
		bu_log("Cache miss for aabb on %s\n", dp->d_namep);
		bu_cache_get_done(&txn);
		rt_db_free_internal(ip);
		BU_PUT(ip, struct rt_db_internal);
		return false;
	    }
	}
    }

    // Check Oriented Bounding Box, if primitive supports it
    if (ip->idb_meth->ft_oriented_bbox) {
	struct rt_arb_internal arb;
	arb.magic = RT_ARB_INTERNAL_MAGIC;
	if (ip->idb_meth->ft_oriented_bbox(&arb, ip, bdtol) < 0) {
	    valid = false;
	} else {
	    point_t db_obb[4] = {VINIT_ZERO};
	    bg_pnts_obb(&db_obb[0], &db_obb[1], &db_obb[2], &db_obb[3], (const point_t *)arb.pt, 8);

	    point_t cache_obb[4] = {VINIT_ZERO};
	    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu:%s", user_hash, CACHE_OBB);
	    if (bu_cache_get(&cdata, ckey, dbip->i->c, &txn)) {
		memcpy(&cache_obb, cdata, 4*sizeof(point_t));
		bool obbvalid = true;
		for (int j = 0; j < 4; j++) {
		    if (!VNEAR_EQUAL(db_obb[j], cache_obb[j], VUNITIZE_TOL)) {
			if (j > 0) {
			    // Check for flipped vector, which is equivalent for an OBB
			    vect_t vflip;
			    VMOVE(vflip, cache_obb[j]);
			    VSCALE(vflip, vflip, -1);
			    if (!VNEAR_EQUAL(db_obb[j], vflip, VUNITIZE_TOL))
				obbvalid = false;
			} else {
			    obbvalid = false;
			}
		    }
		}
		if (!obbvalid) {
		    bu_log("Cache obb vs. db obb mismatch for %s\n", dp->d_namep);
		    //bu_log("   db_obb: (%0.15f %0.15f %0.15f) -> (%0.15f %0.15f %0.15f) (%0.15f %0.15f %0.15f) (%0.15f %0.15f %0.15f)\n", V3ARGS(db_obb[0]), V3ARGS(db_obb[1]), V3ARGS(db_obb[2]), V3ARGS(db_obb[3]));
		    //bu_log("cache_obb: (%0.15f %0.15f %0.15f) -> (%0.15f %0.15f %0.15f) (%0.15f %0.15f %0.15f) (%0.15f %0.15f %0.15f)\n", V3ARGS(cache_obb[0]), V3ARGS(cache_obb[1]), V3ARGS(cache_obb[2]), V3ARGS(cache_obb[3]));
		    valid = false;
		}
	    } else {
		bu_log("Cache miss for obb on %s\n", dp->d_namep);
		bu_cache_get_done(&txn);
		rt_db_free_internal(ip);
		BU_PUT(ip, struct rt_db_internal);
		return false;
	    }
	}
    }

    bu_cache_get_done(&txn);
    rt_db_free_internal(ip);
    BU_PUT(ip, struct rt_db_internal);

    if (!valid)
	return false;

    // Level of Detail
    if (!do_lod(dbip, dp)) {
	bu_log("do_lod failure for %s\n", dp->d_namep);
	valid = false;
    }

    return valid;
}

// For this step, we read the data from the cache and trigger an update if we
// don't find it.  Idea is to roughly simulate what would happen if someone
// started editing a database while the initial cache build was still ongoing.
void
work_dp(struct db_i *dbip, struct directory *dp, std::atomic<int> &miss_counter)
{
    if (!dbip || !dp)
	return;

    if (!validate_dp(dbip, dp)) {
	bu_log("Validation failed for %s, asking for update\n", dp->d_namep);
	db_cache_update(dbip, dp->d_namep);
	miss_counter.fetch_add(1);
    }
}

bool
single_threaded_read_check(struct db_i *dbip)
{
    int64_t start, elapsed;
    fastf_t seconds;

    bu_log("Starting single threaded read check.\n");

    start = bu_gettime();

    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
		continue;
	    validate_dp(dbip, dp);
	}
    }
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Single threaded validation time: %f seconds\n", seconds);

    return true;
}

bool
multi_threaded_read_check(struct db_i *dbip)
{
    int64_t start, elapsed;
    fastf_t seconds;

    start = bu_gettime();

    bu_log("Starting multithreaded read check.\n");

    std::atomic<int> miss_counter = 0;

    std::set<struct directory *> dpq;
    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
		continue;
	    dpq.insert(dp);
	}
    }
    std::vector<std::thread> wthreads;
    while (!dpq.empty()) {
	while (wthreads.size() < 6 && !dpq.empty()) {
	    struct directory *dp = *dpq.begin();
	    dpq.erase(dp);
	    wthreads.emplace_back(work_dp, dbip, dp, std::ref(miss_counter));
	}
	for (std::thread& t : wthreads) {
	    if (t.joinable())
		t.join();
	}
	wthreads.clear();
    }

    // As long as we have misses reported, repeat the process
    while (miss_counter) {
	miss_counter = 0;
	for (int i = 0; i < RT_DBNHASH; i++) {
	    struct directory *dp;
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (dp->d_addr == RT_DIR_PHONY_ADDR)
		    continue;
		if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
		    continue;
		dpq.insert(dp);
	    }
	}
	while (!dpq.empty()) {
	    while (wthreads.size() < 6 && !dpq.empty()) {
		struct directory *dp = *dpq.begin();
		dpq.erase(dp);
		wthreads.emplace_back(work_dp, dbip, dp, std::ref(miss_counter));
	    }
	    for (std::thread& t : wthreads) {
		if (t.joinable())
		    t.join();
	    }
	    wthreads.clear();
	}
    }

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Multithreaded validation time: %f seconds\n", seconds);

    return true;
}


int
main(int argc, char *argv[])
{
    int64_t start, elapsed;
    fastf_t seconds;
    struct db_i *dbip;

    bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_exit(1, "Usage: %s file.g [object]", argv[0]);
    }


    // Set up to use a local cache
    char cache_dir[MAXPATHLEN] = {0};
    bu_dir(cache_dir, MAXPATHLEN, BU_DIR_CURR, "librt_lod_cache", NULL);
    bu_setenv("BU_DIR_CACHE", cache_dir, 1);
    bu_dirclear(cache_dir);
    bu_mkdir(cache_dir);

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    db_update_nref(dbip, &rt_uniresource);

    // Initialize cache
    start = bu_gettime();
    db_cache_start(dbip, 1);

    // Wait until librt claims there is no more processing going on
    bu_log("Waiting for full cache initialization...\n");
    while (db_cache_processing(dbip)) {
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    bu_log("Waiting for full cache initialization... done.\n");

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Total cache initialization time: %f seconds\n", seconds);

    single_threaded_read_check(dbip);
    multi_threaded_read_check(dbip);

    db_close(dbip);
    bu_dirclear(cache_dir);

    // Pause so we can see what the basic setup did
    bu_log("Basic test complete.\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Reset
    bu_mkdir(cache_dir);
    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    db_update_nref(dbip, &rt_uniresource);


    // Now we get rugged.  This part of the test attempts to simulate a
    // contentious environment where the app is trying to read from the cache
    // while it is in the process of building.  We already know the properties
    // of the cache behavior in a clean setup from the above test - this one
    // is intended to simulate what might happen if someone opens a large .g
    // file and immediately tries to draw up the whole thing while the cache
    // is still in the process of building.
    bu_log("Deliberately not waiting for cache init to finish - starting app work immediately.\n");
    db_cache_start(dbip, 1);

    multi_threaded_read_check(dbip);

    // After that's over, check again
    while (db_cache_processing(dbip)) {
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    bu_log("Final validation check\n");
    single_threaded_read_check(dbip);

    db_close(dbip);
    bu_dirclear(cache_dir);

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
