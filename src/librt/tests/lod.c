/*                           L O D . C
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

int
main(int argc, char *argv[])
{
    int64_t start, elapsed;
    fastf_t seconds;
    struct db_i *dbip;
    struct directory *dp;

    bu_setprogname(argv[0]);

    if (argc != 3) {
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
    db_cache_init(dbip);

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
    }

    int key = db_lod_mesh_update(dbip, dp->d_namep);
    if (key != BRLCAD_OK)
	bu_exit(1, "ERROR: %s - db_lod_mesh_update failed\n", dp->d_namep);

    struct bv_lod_mesh *mlod = db_lod_mesh_get(dbip, dp->d_namep);
    if (!mlod)
	bu_exit(1, "ERROR: %s - db_lod_mesh_get failed\n", dp->d_namep);

    struct bv_scene_obj *s = bv_obj_get(NULL);
    s->draw_data = (void *)mlod;

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Initialization time: %f seconds\n", seconds);

    // We have our LoD container - now exercise it
    start = bu_gettime();

    for (int i = 0; i < 16; i++) {
	int64_t lstart = bu_gettime();
	bv_lod_level(s, i, 0);
	int64_t lelapsed = bu_gettime() - lstart;
	seconds = lelapsed / 1000000.0;
	bu_log("Level %d time: %f seconds\n", i, seconds);
    }

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Overall LoD level setting time: %f sec\n", seconds);

    bv_lod_mesh_destroy(mlod);
    bv_obj_put(s);

    db_close(dbip);
    bu_dirclear(cache_dir);

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
