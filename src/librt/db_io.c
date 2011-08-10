/*                         D B _ I O . C
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
/** @file librt/db_io.c
 *
 * v4 database read/write I/O routines
 *
 */

#include "common.h"

#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "raytrace.h"


/**
 * D B _ R E A D
 *
 * Reads 'count' bytes at file offset 'offset' into buffer at 'addr'.
 * A wrapper for the UNIX read() sys-call that takes into account
 * syscall semaphores, stdio-only machines, and in-memory buffering.
 *
 * Returns -
 * 0 OK
 * -1 FAILURE
 */
HIDDEN int
db_read(const struct db_i *dbip, genptr_t addr, size_t count, off_t offset)
/* byte count */
/* byte offset from start of file */
{
    size_t got;
    int ret;

    RT_CK_DBI(dbip);
    if (RT_G_DEBUG&DEBUG_DB) {
	bu_log("db_read(dbip=x%x, addr=x%x, count=%zu., offset=x%x)\n",
	       dbip, addr, count, offset);
    }
    if (count == 0) {
	return -1;
    }
    if (offset+count > (size_t)dbip->dbi_eof) {
	/* Attempt to read off the end of the file */
	bu_log("db_read(%s) ERROR offset=%d, count=%zu, dbi_eof=%d\n",
	       dbip->dbi_filename,
	       offset, count, dbip->dbi_eof);
	return -1;
    }
    if (dbip->dbi_inmem) {
	memcpy(addr, ((char *)dbip->dbi_inmem) + offset, count);
	return 0;
    }
    bu_semaphore_acquire(BU_SEM_SYSCALL);

    ret = (int)fseek(dbip->dbi_fp, (long)offset, 0);
    if (ret)
	bu_bomb("db_read: fseek error\n");
    got = (size_t)fread(addr, 1, count, dbip->dbi_fp);

    bu_semaphore_release(BU_SEM_SYSCALL);

    if (got != count) {
	perror(dbip->dbi_filename);
	bu_log("db_read(%s):  read error.  Wanted %zu, got %zu bytes\n", dbip->dbi_filename, count, got);
	return -1;
    }
    return 0;			/* OK */
}


/**
 * D B _ G E T M R E C
 *
 * Retrieve all records in the database pertaining to an object, and
 * place them in malloc()'ed storage, which the caller is responsible
 * for free()'ing.
 *
 * This loads the combination into a local record buffer.
 * This is in external v4 format.
 *
 * Returns -
 * union record * - OK
 * (union record *)0 - FAILURE
 */
union record *
db_getmrec(const struct db_i *dbip, const struct directory *dp)
{
    union record *where;

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);

    if (dbip->dbi_version > 4) {
	/* can't get an mrec on a v5 */
	return (union record *)NULL;
    }

    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_getmrec(%s) x%x, x%x\n",
				    dp->d_namep, dbip, dp);

    if (dp->d_addr == RT_DIR_PHONY_ADDR)
	return (union record *)0;	/* was dummy DB entry */
    where = (union record *)bu_malloc(
	dp->d_len * sizeof(union record),
	"db_getmrec record[]");

    if (dp->d_flags & RT_DIR_INMEM) {
	memcpy((char *)where, dp->d_un.ptr, dp->d_len * sizeof(union record));
	return where;
    }

    if (db_read(dbip, (char *)where,
		dp->d_len * sizeof(union record),
		dp->d_addr) < 0) {
	bu_free((genptr_t)where, "db_getmrec record[]");
	return (union record *)0;	/* VERY BAD */
    }
    return where;
}


/**
 * D B _ G E T
 *
 * Retrieve 'len' records from the database, "offset" granules into
 * this entry.
 *
 * Returns -
 * 0 OK
 * -1 FAILURE
 */
int
db_get(const struct db_i *dbip, const struct directory *dp, union record *where, off_t offset, size_t len)
{

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);
    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_get(%s) x%x, x%x x%x off=%d len=%zu\n",
				    dp->d_namep, dbip, dp, where, offset, len);

    if (dp->d_addr == RT_DIR_PHONY_ADDR) {
	where->u_id = '\0';	/* undefined id */
	return -1;
    }
    if (offset < 0 || len+(size_t)offset > dp->d_len) {
	bu_log("db_get(%s):  xfer %d..%x exceeds 0..%zu\n",
	       dp->d_namep, offset, offset+len, dp->d_len);
	where->u_id = '\0';	/* undefined id */
	return -1;
    }

    if (dp->d_flags & RT_DIR_INMEM) {
	memcpy((char *)where,
	       ((char *)dp->d_un.ptr) + offset * sizeof(union record),
	       len * sizeof(union record));
	return 0;		/* OK */
    }

    if (db_read(dbip, (char *)where, len * sizeof(union record),
		dp->d_addr + offset * sizeof(union record)) < 0) {
	where->u_id = '\0';	/* undefined id */
	return -1;
    }
    return 0;			/* OK */
}


/**
 * D B _ W R I T E
 *
 * Writes 'count' bytes into at file offset 'offset' from buffer at
 * 'addr'.  A wrapper for the UNIX write() sys-call that takes into
 * account syscall semaphores, stdio-only machines, and in-memory
 * buffering.
 *
 * Returns -
 * 0 OK
 * -1 FAILURE
 */
/* should be HIDDEN */
int
db_write(struct db_i *dbip, const genptr_t addr, size_t count, off_t offset)
{
    register size_t got;

    RT_CK_DBI(dbip);
    if (RT_G_DEBUG&DEBUG_DB) {
	bu_log("db_write(dbip=x%x, addr=x%x, count=%zu., offset=x%x)\n",
	       dbip, addr, count, offset);
    }
    if (dbip->dbi_read_only) {
	bu_log("db_write(%s):  READ-ONLY file\n",
	       dbip->dbi_filename);
	return -1;
    }
    if (count == 0) {
	return -1;
    }
    if (dbip->dbi_inmem) {
	bu_log("db_write() in memory?\n");
	return -1;
    }
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    bu_suspend_interrupts();

    (void)fseek(dbip->dbi_fp, offset, 0);
    got = fwrite(addr, 1, count, dbip->dbi_fp);
    fflush(dbip->dbi_fp);

    bu_restore_interrupts();
    bu_semaphore_release(BU_SEM_SYSCALL);
    if (got != count) {
	perror("db_write");
	bu_log("db_write(%s):  write error.  Wanted %zu, got %zu bytes.\nFile forced read-only.\n",
	       dbip->dbi_filename, count, got);
	dbip->dbi_read_only = 1;
	return -1;
    }
    return 0;			/* OK */
}


/**
 * D B _ P U T
 *
 * Store 'len' records to the database, "offset" granules into this
 * entry.
 *
 * Returns:
 * 0 OK
 * non-0 FAILURE
 */
int
db_put(struct db_i *dbip, const struct directory *dp, union record *where, off_t offset, size_t len)
{
    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);

    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_put(%s) x%x, x%x x%x off=%d len=%zu\n",
				    dp->d_namep, dbip, dp, where, offset, len);

    if ((len+(size_t)offset) > dp->d_len) {
	bu_log("db_put(%s):  xfer %d..%x exceeds 0..%zu\n",
	       dp->d_namep, offset, offset+len, dp->d_len);
	return -1;
    }

    if (dp->d_flags & RT_DIR_INMEM) {
	memcpy(((char *)dp->d_un.ptr) + offset * sizeof(union record),
	       (char *)where,
	       len * sizeof(union record));
	return 0;		/* OK */
    }

    if (dbip->dbi_read_only) {
	bu_log("db_put(%s):  READ-ONLY file\n",
	       dbip->dbi_filename);
	return -1;
    }

    if (db_write(dbip, (char *)where, len * sizeof(union record),
		 dp->d_addr + offset * sizeof(union record)) < 0) {
	return -1;
    }
    return 0;
}


/**
 * D B _ G E T _ E X T E R N A L
 *
 * Obtains a object from the database, leaving it in external (on-disk)
 * format.
 *
 * The bu_external structure represented by 'ep' is initialized here,
 * the caller need not pre-initialize it.  On error, 'ep' is left
 * un-initialized and need not be freed, to simplify error recovery.
 * On success, the caller is responsible for calling
 * bu_free_external(ep);
 *
 * Returns -
 * -1 error
 * 0 success
 */
int
db_get_external(register struct bu_external *ep, const struct directory *dp, const struct db_i *dbip)
{
    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);
    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_get_external(%s) ep=x%x, dbip=x%x, dp=x%x\n",
				    dp->d_namep, ep, dbip, dp);

    if ((dp->d_flags & RT_DIR_INMEM) == 0 && dp->d_addr == RT_DIR_PHONY_ADDR)
	return -1;		/* was dummy DB entry */

    BU_EXTERNAL_INIT(ep);
    if (dbip->dbi_version < 5)
	ep->ext_nbytes = dp->d_len * sizeof(union record);
    else
	ep->ext_nbytes = dp->d_len;
    ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "db_get_ext ext_buf");

    if (dp->d_flags & RT_DIR_INMEM) {
	memcpy((char *)ep->ext_buf, dp->d_un.ptr, ep->ext_nbytes);
	return 0;
    }

    if (db_read(dbip, (char *)ep->ext_buf, ep->ext_nbytes, dp->d_addr) < 0) {
	bu_free(ep->ext_buf, "db_get_ext ext_buf");
	ep->ext_buf = (genptr_t)NULL;
	ep->ext_nbytes = 0;
	return -1;	/* VERY BAD */
    }
    return 0;
}


/**
 * D B _ P U T _ E X T E R N A L
 *
 * Given that caller already has an external representation of the
 * database object, update it to have a new name (taken from
 * dp->d_namep) in that external representation, and write the new
 * object into the database, obtaining different storage if the size
 * has changed.
 *
 * Caller is responsible for freeing memory of external
 * representation, using bu_free_external().
 *
 * This routine is used to efficiently support MGED's "cp" and "keep"
 * commands, which don't need to import objects just to rename and
 * copy them.
 *
 * Returns -
 * <0 error
 * 0 success
 */
int
db_put_external(struct bu_external *ep, struct directory *dp, struct db_i *dbip)
{

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);
    BU_CK_EXTERNAL(ep);
    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_put_external(%s) ep=x%x, dbip=x%x, dp=x%x\n",
				    dp->d_namep, ep, dbip, dp);


    if (dbip->dbi_read_only) {
	bu_log("db_put_external(%s):  READ-ONLY file\n",
	       dbip->dbi_filename);
	return -1;
    }

    if (db_version(dbip) == 5)
	return db_put_external5(ep, dp, dbip);

    if (db_version(dbip) < 5) {
	size_t ngran;

	ngran = (ep->ext_nbytes+sizeof(union record)-1)/sizeof(union record);
	if (ngran != dp->d_len) {
	    if (dp->d_addr != RT_DIR_PHONY_ADDR) {
		if (db_delete(dbip, dp))
		    return -2;
	    }
	    if (db_alloc(dbip, dp, ngran)) {
		return -3;
	    }
	}
	/* Sanity check */
	if (ngran != dp->d_len) {
	    bu_log("db_put_external(%s) ngran=%zu != dp->d_len %zu\n",
		   dp->d_namep, ngran, dp->d_len);
	    bu_bomb("db_io.c: db_put_external()");
	}

	db_wrap_v4_external(ep, dp->d_namep);
    } else
	bu_bomb("db_put_external(): unknown dbi_version\n");

    if (dp->d_flags & RT_DIR_INMEM) {
	memcpy(dp->d_un.ptr, (char *)ep->ext_buf, ep->ext_nbytes);
	return 0;
    }

    if (db_write(dbip, (char *)ep->ext_buf, ep->ext_nbytes, dp->d_addr) < 0) {
	return -1;
    }
    return 0;
}


/**
 * D B _ F W R I T E _ E X T E R N A L
 *
 * Add name from dp->d_namep to external representation of solid, and
 * write it into a file.
 *
 * Caller is responsible for freeing memory of external
 * representation, using bu_free_external().
 *
 * The 'name' field of the external representation is modified to
 * contain the desired name.  The 'ep' parameter cannot be const.
 *
 * THIS ROUTINE ONLY SUPPORTS WRITING V4 GEOMETRY.
 *
 * Returns -
 * <0 error
 * 0 OK
 *
 * NOTE: Callers of this should be using wdb_export_external()
 * instead.
 */
int
db_fwrite_external(FILE *fp, const char *name, struct bu_external *ep)
{

    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_fwrite_external(%s) ep=x%x\n", name, ep);

    BU_CK_EXTERNAL(ep);

    db_wrap_v4_external(ep, name);

    return bu_fwrite_external(fp, ep);
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
