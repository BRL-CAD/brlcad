/*                 L C O M B _ L A R G E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
 * details.
 */
/** @file librt/tests/lcomb_large.c
 *
 * Verify that a large union combination can be written without a
 * stack-depth failure in the combination serializer.
 */

#include "common.h"

#include <stdlib.h>

#include "bu/app.h"
#include "bu/file.h"
#include "raytrace.h"
#include "wdb.h"


#define LARGE_COMB_MEMBER_COUNT 65536


int
main(int argc, char *argv[])
{
    char gfile[MAXPATHLEN] = {0};
    struct wmember members;
    struct rt_wdb *wdbp;
    FILE *fp;

    bu_setprogname(argv[0]);
    if (argc != 1) {
	bu_exit(EXIT_FAILURE, "Usage: %s\n", argv[0]);
    }

    fp = bu_temp_file(gfile, sizeof(gfile));
    if (!fp) {
	bu_exit(EXIT_FAILURE, "Unable to create temporary database\n");
    }
    fclose(fp);
    bu_file_delete(gfile);

    wdbp = wdb_fopen(gfile);
    if (!wdbp) {
	bu_exit(EXIT_FAILURE, "Unable to open temporary database\n");
    }

    BU_LIST_INIT(&members.l);
    for (size_t i = 0; i < LARGE_COMB_MEMBER_COUNT; i++) {
	if (!mk_addmember("member.s", &members.l, NULL, WMOP_UNION)) {
	    wdb_close(wdbp);
	    bu_file_delete(gfile);
	    bu_exit(EXIT_FAILURE, "Unable to add combination member\n");
	}
    }

    if (mk_lcomb(wdbp, "large.c", &members, 0, NULL, NULL, NULL, 0)) {
	wdb_close(wdbp);
	bu_file_delete(gfile);
	bu_exit(EXIT_FAILURE, "Unable to write large combination\n");
    }

    wdb_close(wdbp);
    bu_file_delete(gfile);
    return EXIT_SUCCESS;
}
