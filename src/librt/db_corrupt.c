/*                    D B _ C O R R U P T . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file librt/db_corrupt.c
 *
 * Routines to detect whether a geometry database file seems to be
 * corrupt or invalid due to flipped endian encoding.
 *
 */

#include "common.h"

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"

#include "./librt_private.h"


struct counter {
    size_t found;
    size_t fixed;
};


static int
db_corrupt_handler(struct db_i *dbip, const char *name, off_t offset, size_t size, int UNUSED(type), genptr_t data)
{
    struct counter *cnt = (struct counter *)data;
    struct bu_external ext;
    union record *rp = NULL;
    mat_t diskmat = MAT_INIT_ZERO;
    size_t nodecount;
    size_t j;
    int ret;

    RT_CK_DBI(dbip);

    /* get into position */
    ret = fseek(dbip->dbi_fp, (long)offset, 0);
    if (ret) {
	bu_log("Database seek failure, unable to seek [%s]\n", name);
	return 0;
    }

    /* set up our buffer */
    BU_EXTERNAL_INIT(&ext);
    ext.ext_nbytes = size * sizeof(union record);
    ext.ext_buf = (genptr_t)bu_calloc(ext.ext_nbytes, 1, "db_corrupt");

    /* read into the buffer */
    j = (size_t)fread(ext.ext_buf, 1, ext.ext_nbytes, dbip->dbi_fp);
    if (j != ext.ext_nbytes) {
	bu_log("Database read failure, unable to read [%s]\n", name);
	bu_free_external(&ext);
	return 0;
    }

    rp = (union record *)ext.ext_buf;
    if (rp[0].u_id != ID_COMB) {
	/* skip non-combination */
	bu_free_external(&ext);
	return 0;
    }

    /* iterate over combination members, looking for bad matrices */
    nodecount = ext.ext_nbytes/sizeof(union record) - 1;
    for (j=0; j<nodecount; j++) {

	/* try without flipping */
	flip_mat_dbmat(diskmat, rp[j+1].M.m_mat, 0);
	if ((bn_mat_ck(name, diskmat) < 0)
	    || fabs(diskmat[0]) > 1 || fabs(diskmat[1]) > 1 || fabs(diskmat[2]) > 1
	    || fabs(diskmat[4]) > 1 || fabs(diskmat[5]) > 1 || fabs(diskmat[6]) > 1
	    || fabs(diskmat[8]) > 1 || fabs(diskmat[9]) > 1 || fabs(diskmat[10]) > 1)
	{
	    /* corruption detected */
	    cnt->found++;

	    /* invalid, so try flipped */
	    flip_mat_dbmat(diskmat, rp[j+1].M.m_mat, 1);
	    if ((bn_mat_ck(name, diskmat) < 0)
		|| fabs(diskmat[0]) > 1 || fabs(diskmat[1]) > 1 || fabs(diskmat[2]) > 1
		|| fabs(diskmat[4]) > 1 || fabs(diskmat[5]) > 1 || fabs(diskmat[6]) > 1
		|| fabs(diskmat[8]) > 1 || fabs(diskmat[9]) > 1 || fabs(diskmat[10]) > 1)
	    {
		bu_log("WARNING: Invalid matrix detected within %s\n", name);
	    } else {
		/* flipping helped */
		cnt->fixed++;
	    }

	    break;
	}
    }

    bu_free_external(&ext);

    return 0;
}


int
rt_db_flip_endian(struct db_i *dbip)
{
    struct counter cnt = {0, 0};
    char *v4flip;

    RT_CK_DBI(dbip);

    /* does not apply to v5 geometry database */
    if (db_version(dbip) > 4)
	return 0;

    /* provide the user some means to override this automatic behavior */
    v4flip = getenv("LIBRT_V4FLIP");
    if (v4flip)
	return bu_str_true(v4flip);

    /* iterate over all database objects looking for signs of
     * corruption keeping a tally of whether fliping the record fixed
     * the problem.
     */
    db_scan(dbip, db_corrupt_handler, 0, &cnt);

    /* it has to help more than it hurts */
    if (cnt.found > 0 && (double)cnt.fixed > ((double)cnt.found / 2.0)) {
	if (cnt.fixed != cnt.found)
	    bu_log("%zu of %zu objects were NOT fixed by flipping endian interpretation.  Manual inspection and repair required.\n", cnt.found - cnt.fixed, cnt.found);
	return 1;
    }

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
