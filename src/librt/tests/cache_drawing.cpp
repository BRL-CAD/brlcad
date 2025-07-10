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

#include "vmath.h"
#include "bu/app.h"
#include "bu/env.h"
#include "bu/time.h"
#include "bu/units.h"
#include "bg.h"
#include "bv/lod.h"
#include "raytrace.h"

//#define LOD_TIMING
//#define LOD_TIMING_PER_LEVEL

static void
do_lod(struct db_i *dbip, struct directory *dp)
{
#ifdef LOD_TIMING
    int64_t start, elapsed;
    fastf_t seconds;
    start = bu_gettime();
#endif
#ifdef LOD_TIMING_PER_LEVEL
    int64_t lstart, lelapsed;
#endif

    int key = db_cache_update(dbip, dp->d_namep, 0);
    if (key != BRLCAD_OK) {
	bu_log("ERROR: %s - db_cache_mesh_update failed\n", dp->d_namep);
	db_cache_update(dbip, dp->d_namep, 0);
	return;
    }

    struct bv_lod_mesh *mlod = db_cache_lod_mesh_get(dbip, dp->d_namep);
    if (!mlod) {
	bu_log("ERROR: %s - db_cache_lod_mesh_get failed\n", dp->d_namep);
	return;
    }

    struct bv_scene_obj *s = bv_obj_get(NULL);
    s->draw_data = (void *)mlod;

#ifdef LOD_TIMING
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("%s: initialization time: %f seconds\n", dp->d_namep, seconds);
#endif

    // We have our LoD container - now exercise it
#ifdef LOD_TIMING
    start = bu_gettime();
#endif

    for (int i = 0; i < 16; i++) {
#ifdef LOD_TIMING_PER_LEVEL
	lstart = bu_gettime();
#endif
	bv_lod_level(s, i, 0);
#ifdef LOD_TIMING_PER_LEVEL
	lelapsed = bu_gettime() - lstart;
	seconds = lelapsed / 1000000.0;
	bu_log("%s: level %d time: %f seconds\n", dp->d_namep, i, seconds);
#endif
    }

#ifdef LOD_TIMING
    //elapsed = bu_gettime() - start;
    //seconds = elapsed / 1000000.0;
    //bu_log("%s: total LoD level setting time: %f sec\n", dp->d_namep, seconds);
#endif

    bv_lod_mesh_put(mlod);
    bv_obj_put(s);
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

    start = bu_gettime();

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
    db_cache_init(dbip, 1);

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Total cache initialization time: %f seconds\n", seconds);

    // TODO - validate properties in the cache

    // Exercise the LoD structs in the cache (TODO - validate data somehow...)
    start = bu_gettime();

    if (argc < 3) {
	for (int i = 0; i < RT_DBNHASH; i++) {
	    struct directory *dp;
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (dp->d_addr == RT_DIR_PHONY_ADDR)
		    continue;
		if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
		    continue;
		do_lod(dbip, dp);
	    }
	}
    } else {

	struct directory *dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
	}
	do_lod(dbip, dp);
    }

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("LoD validation time: %f seconds\n", seconds);

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
