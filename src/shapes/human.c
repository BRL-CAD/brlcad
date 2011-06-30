/*                          H U M A N . C
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
/** @file shapes/human.c
 *
 * Generator for human models based on height, and other stuff eventually
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "wdb.h"
#include "ged.h"

#define DEFAULT_FILENAME "human.g"

int main(int ac, char *av[])
{
    struct rt_wdb *db_fp = NULL;
    struct ged ged;
    int ret;
    const char *filename = NULL;

    filename = DEFAULT_FILENAME;

    db_fp = wdb_fopen(filename);

    mk_id_units(db_fp, "Human Model", "in");

    GED_INIT(&ged, db_fp);
    bu_log("Building\n");
    ret = ged_human(&ged, ac, (const char **)av);
    bu_log("Finished Building\n");
    wdb_close(db_fp);

    if (ret) {
	bu_exit(1, "%s", bu_vls_addr(ged.ged_result_str));
    }
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
