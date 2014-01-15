/*                        D B _ D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file db_diff.c
 *
 * Routines to determine the differences between two BRL-CAD databases.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "bio.h"

#include "tcl.h"

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "db_diff.h"


HIDDEN int
db_diff_external(const struct bu_external *ext1, const struct bu_external *ext2)
{
    if (ext1->ext_nbytes != ext2->ext_nbytes) {
	return 1;
    }
    if (memcmp((void *)ext1->ext_buf, (void *)ext2->ext_buf, ext1->ext_nbytes)) {
	return 1;
    }
    return 0;
}

int
db_diff(const struct db_i *dbip1,
	const struct db_i *dbip2,
	int (*add_func)(const struct db_i *left, const struct db_i *right, const struct directory *added, void *data),
	int (*del_func)(const struct db_i *left, const struct db_i *right, const struct directory *removed, void *data),
	int (*chgd_func)(const struct db_i *left, const struct db_i *right, const struct directory *before, const struct directory *after, void *data),
	int (*unch_func)(const struct db_i *left, const struct db_i *right, const struct directory *unchanged, void *data),
	void *client_data)
{
    int ret = 0;
    int error = 0;
    struct directory *dp1, *dp2;
    struct bu_external ext1, ext2;

    /* look at all objects in this database */
    FOR_ALL_DIRECTORY_START(dp1, dbip1) {
	int this_diff = 0;

	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp1->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    continue;
	}

	/* check if this object exists in the other database */
	if ((dp2 = db_lookup(dbip2, dp1->d_namep, 0)) == RT_DIR_NULL) {
	    this_diff++;
	    if (del_func(dbip1, dbip2, dp1, client_data)) error--;
	    continue;
	}

	/* Check if the objects differ */
	if (db_get_external(&ext1, dp1, dbip1) || db_get_external(&ext2, dp2, dbip2)) {
	    bu_log("Error getting bu_external form when comparing %s and %s\n", dp1->d_namep, dp2->d_namep);
	    error--;
	    continue;
	}

	if (db_diff_external(&ext1, &ext2)) {
	    this_diff++;
	    if (chgd_func(dbip1, dbip2, dp1, dp2, client_data)) error--;
	} else {
	    if (unch_func(dbip1, dbip2, dp1, client_data)) error--;
	}

	ret += this_diff;

    } FOR_ALL_DIRECTORY_END;

    /* now look for objects in the other database that aren't here */
    FOR_ALL_DIRECTORY_START(dp2, dbip2) {
	int this_diff = 0;

	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp2->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    continue;
	}

	/* check if this object exists in the other database */
	if ((dp1 = db_lookup(dbip1, dp2->d_namep, 0)) == RT_DIR_NULL) {
	    this_diff++;
	    if (add_func(dbip1, dbip2, dp1, client_data)) error--;
	}

	ret += this_diff;

    } FOR_ALL_DIRECTORY_END;

    if (!error) {
    	return ret;
    } else {
	return error;
    }
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
