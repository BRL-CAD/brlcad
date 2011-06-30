/*                          T I R E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file shapes/tire.c
 *
 * Tire Generator
 *
 * Program to create basic tire shapes.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "wdb.h"
#include "ged.h"

#define DEFAULT_TIRE_FILENAME "tire.g"


int main(int ac, char *av[])
{
    struct rt_wdb *db_fp = NULL;
    const char *filename = NULL;
    struct ged ged;
    int ret;

    filename = DEFAULT_TIRE_FILENAME;

    /* Just using "tire.g" for now. */
    if (!bu_file_exists(filename)) {
	db_fp = wdb_fopen(filename);
    } else {
	bu_exit(1, "ERROR - refusing to overwrite existing [%s] file.", filename);
    }
    mk_id(db_fp, "Tire");

    GED_INIT(&ged, db_fp);

    /* create the tire */
    ret = ged_tire(&ged, ac, (const char **)av);

    /* Close database */
    wdb_close(db_fp);

    if (ret) {
	unlink(filename);
	bu_log("%s", bu_vls_addr(ged.ged_result_str));
	ged_free(&ged);
	return 1;
    }

    /* release our ged instance memory */
    ged_free(&ged);

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
