/*                      D B _ A L L O C . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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
/** @addtogroup db4 */
/** @{ */
/** @file librt/db_alloc.c
 *
 * v4 granule allocation routines
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "raytrace.h"


/**
 * D B _ A L L O C
 *
 * Find a block of database storage of "count" granules.
 *
 * Returns:
 * 0 OK
 * non-0 failure
 */
int
db_alloc(register struct db_i *dbip, register struct directory *dp, size_t count)
{
    size_t len;
    union record rec;

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);
    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_alloc(%s) x%x, x%x, count=%zu\n",
				    dp->d_namep, dbip, dp, count);
    if (count <= 0) {
	bu_log("db_alloc(0)\n");
	return -1;
    }

    if (dp->d_flags & RT_DIR_INMEM) {
	if (dp->d_un.ptr) {
	    dp->d_un.ptr = bu_realloc(dp->d_un.ptr,
				      count * sizeof(union record), "db_alloc() d_un.ptr");
	} else {
	    dp->d_un.ptr = bu_malloc(count * sizeof(union record), "db_alloc() d_un.ptr");
	}
	dp->d_len = count;
	return 0;
    }

    if (dbip->dbi_read_only) {
	bu_log("db_alloc on READ-ONLY file\n");
	return -1;
    }
    while (1) {
	len = rt_memalloc(&(dbip->dbi_freep), (unsigned)count);
	if (len == 0L) {
	    /* No contiguous free block, append to file */
	    if ((dp->d_addr = dbip->dbi_eof) == RT_DIR_PHONY_ADDR) {
		bu_log("db_alloc: bad EOF\n");
		return -1;
	    }
	    dp->d_len = count;
	    dbip->dbi_eof += (off_t)(count * sizeof(union record));
	    dbip->dbi_nrec += count;
	    break;
	}
	dp->d_addr = (off_t)(len * sizeof(union record));
	dp->d_len = count;
	if (db_get(dbip, dp, &rec, 0, 1) < 0)
	    return -1;
	if (rec.u_id != ID_FREE) {
	    bu_log("db_alloc():  len %ld non-FREE (id %d), skipping\n",
		   len, rec.u_id);
	    continue;
	}
    }

    /* Clear out ALL the granules, for safety */
    return db_zapper(dbip, dp, 0);
}


/**
 * D B _ D E L R E C
 *
 * Delete a specific record from database entry
 * No longer supported.
 */
int
db_delrec(struct db_i *dbip, register struct directory *dp, int recnum)
{

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);
    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_delrec(%s) x%x, x%x, recnum=%d\n",
				    dp->d_namep, dbip, dp, recnum);

    bu_log("ERROR db_delrec() is no longer supported.  Use combination import/export routines.\n");
    return -1;
}


/**
 * D B _ D E L E T E
 *
 * Delete the indicated database record(s).
 * Arrange to write "free storage" database markers in its place,
 * positively erasing what had been there before.
 *
 * Returns:
 * 0 on success
 * non-zero on failure
 */
int
db_delete(struct db_i *dbip, struct directory *dp)
{
    int i = 0;

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);
    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_delete(%s) x%x, x%x\n",
				    dp->d_namep, dbip, dp);

    if (dp->d_flags & RT_DIR_INMEM) {
	bu_free(dp->d_un.ptr, "db_delete d_un.ptr");
	dp->d_un.ptr = NULL;
	dp->d_len = 0;
	return 0;
    }

    if (db_version(dbip) == 4) {
	i = db_zapper(dbip, dp, 0);
	rt_memfree(&(dbip->dbi_freep), (unsigned)dp->d_len, dp->d_addr/(sizeof(union record)));
    } else if (db_version(dbip) == 5) {
	i = db5_write_free(dbip, dp, dp->d_len);
	rt_memfree(&(dbip->dbi_freep), dp->d_len, dp->d_addr);
    } else {
	bu_bomb("db_delete() unsupported database version\n");
    }

    dp->d_len = 0;
    dp->d_addr = RT_DIR_PHONY_ADDR;
    return i;
}


/**
 * D B _ Z A P P E R
 *
 * Using a single call to db_put(), write multiple zeroed records out,
 * all with u_id field set to ID_FREE.
 * This will zap all records from "start" to the end of this entry.
 *
 * Returns:
 * 0 on success (from db_put())
 * non-zero on failure
 */
int
db_zapper(struct db_i *dbip, struct directory *dp, size_t start)
{
    union record *rp;
    size_t i;
    size_t todo;
    int ret;

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);
    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_zapper(%s) x%x, x%x, start=%zu\n",
				    dp->d_namep, dbip, dp, start);

    if (dp->d_flags & RT_DIR_INMEM) bu_bomb("db_zapper() called on RT_DIR_INMEM object\n");

    if (dbip->dbi_read_only)
	return -1;

    BU_ASSERT_LONG(dbip->dbi_version, ==, 4);

    if (dp->d_len < start)
	return -1;

    if ((todo = dp->d_len - start) == 0)
	return 0;		/* OK -- trivial */

    rp = (union record *)bu_malloc(todo * sizeof(union record), "db_zapper buf");
    memset((char *)rp, 0, todo * sizeof(union record));

    for (i=0; i < todo; i++)
	rp[i].u_id = ID_FREE;
    ret = db_put(dbip, dp, rp, (off_t)start, todo);
    bu_free((char *)rp, "db_zapper buf");
    return ret;
}


void
db_alloc_directory_block(struct resource *resp)
{
    struct directory *dp;
    size_t bytes;

    RT_CK_RESOURCE(resp);
    BU_CK_PTBL(&resp->re_directory_blocks);

    BU_ASSERT_PTR(resp->re_directory_hd, ==, NULL);

    /* Get a BIG block */
    bytes = (size_t)bu_malloc_len_roundup(1024*sizeof(struct directory));
    dp = (struct directory *)bu_malloc(bytes, "re_directory_blocks from db_alloc_directory_block() " BU_FLSTR);

    /* Record storage for later */
    bu_ptbl_ins(&resp->re_directory_blocks, (long *)dp);

    while (bytes >= sizeof(struct directory)) {
	dp->d_magic = RT_DIR_MAGIC;
	dp->d_forw = resp->re_directory_hd;
	resp->re_directory_hd = dp;
	dp++;
	bytes -= sizeof(struct directory);
    }
}


void
rt_alloc_seg_block(register struct resource *res)
{
    register struct seg *sp;
    size_t bytes;

    RT_CK_RESOURCE(res);

    if (!BU_LIST_IS_INITIALIZED(&res->re_seg)) {
	BU_LIST_INIT(&(res->re_seg));
	bu_ptbl_init(&res->re_seg_blocks, 64, "re_seg_blocks ptbl");
    }
    bytes = bu_malloc_len_roundup(64*sizeof(struct seg));
    sp = (struct seg *)bu_malloc(bytes, "rt_alloc_seg_block()");
    bu_ptbl_ins(&res->re_seg_blocks, (long *)sp);
    while (bytes >= sizeof(struct seg)) {
	sp->l.magic = RT_SEG_MAGIC;
	BU_LIST_INSERT(&(res->re_seg), &(sp->l));
	res->re_seglen++;
	sp++;
	bytes -= sizeof(struct seg);
    }
}


/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
