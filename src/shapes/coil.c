/*                          C O I L . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2013 United States Government as represented by
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
/** @file shapes/coil.c
 *
 * Coil Generator
 *
 * Program to create coils using the pipe primitive.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"


#define DEFAULT_COIL_FILENAME "coil.g"

int
main(int ac, char *av[])
{

    struct rt_wdb *db_fp = NULL;
    struct ged ged;

    /* make sure file doesn't already exist and opens for writing */
    if (bu_file_exists(DEFAULT_COIL_FILENAME, NULL))
	bu_exit(2, "%s: refusing to overwrite pre-existing file %s\n", av[0], DEFAULT_COIL_FILENAME);

    db_fp = wdb_fopen(DEFAULT_COIL_FILENAME);
    if (!db_fp)
	bu_exit(2, "%s: unable to open %s for writing\n", av[0], DEFAULT_COIL_FILENAME);

    /* do it. */
    mk_id(db_fp, "coil");
    GED_INIT(&ged, db_fp);
    if (ged_coil(&ged, ac, (const char**)av) == GED_ERROR) {
	/* Close database */
	wdb_close(db_fp);
	/* Creation failed - remove file */
	bu_file_delete(DEFAULT_COIL_FILENAME);
	bu_log("%s\n", bu_vls_addr(ged.ged_result_str));
	return -1;
    } else {
	/* Close database */
	wdb_close(db_fp);
    }
    bu_log("%s\n", bu_vls_addr(ged.ged_result_str));
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
