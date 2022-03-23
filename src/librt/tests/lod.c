/*                           L O D . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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
#include "bu/time.h"
#include "bu/units.h"
#include "bg.h"
#include "bg/lod.h"
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

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    db_update_nref(dbip, &rt_uniresource);

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
    }

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;

    // Unpack bot
    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	bu_exit(1, "ERROR: %s internal get failed\n", argv[2]);

    struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    if (!bot->num_faces)
	bu_exit(1, "ERROR: %s - no faces found\n", argv[2]);

    struct bg_mesh_lod *mlod = NULL;
    mlod = bg_mesh_lod_create((const point_t *)bot->vertices, bot->num_vertices, bot->faces, bot->num_faces);
    if (!mlod)
	bu_exit(1, "ERROR: %s - lod creation failed\n", argv[2]);


    // TODO Set up initial view

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Initialization time: %f seconds\n", seconds);

    start = bu_gettime();

    // TODO - lod testing
    // struct bu_list elist;
    int ecnt = bg_lod_elist(NULL, NULL, mlod);

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("lod, view 1 edge cnt(%f sec): %d\n", seconds, ecnt);

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
