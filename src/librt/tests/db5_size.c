/*                      D B 5 _ S I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2016 United States Government as represented by
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
#include "bu/units.h"
#include "raytrace.h"
#include "../librt_private.h"

#define HSIZE(buf, var) \
        (void)bu_humanize_number(buf, 5, (int64_t)var, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);

int
main(int argc, char *argv[])
{
    long fsize;
    struct db_i *dbip;
    struct directory *dp;
    char hlen[6] = { '\0' };

    if (argc != 3) {
	bu_exit(1, "Usage: %s file.g [object]", argv[0]);
    }

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

    fsize = db5_size(dbip, dp, 0);
    HSIZE(hlen, fsize);
    bu_log("obj, no attributes: %ld (%s)\n", fsize, hlen);
    fsize = db5_size(dbip, dp, DB_SIZE_ATTR);
    HSIZE(hlen, fsize);
    bu_log("obj, attributes: %ld (%s)\n", fsize, hlen);
    fsize = db5_size(dbip, dp, DB_SIZE_KEEP);
    HSIZE(hlen, fsize);
    bu_log("keep, no attributes: %ld (%s)\n", fsize, hlen);
    fsize = db5_size(dbip, dp, DB_SIZE_KEEP|DB_SIZE_ATTR);
    HSIZE(hlen, fsize);
    bu_log("keep, with attributes: %ld (%s)\n", fsize, hlen);
    fsize = db5_size(dbip, dp, DB_SIZE_XPUSH);
    HSIZE(hlen, fsize);
    bu_log("xpush, no attributes: %ld (%s)\n", fsize, hlen);
    fsize = db5_size(dbip, dp, DB_SIZE_XPUSH|DB_SIZE_ATTR);
    HSIZE(hlen, fsize);
    bu_log("xpush, with attributes: %ld (%s)\n", fsize, hlen);

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
