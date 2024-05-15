/*                       D B _ O P E N . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2024 United States Government as represented by
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
/** @addtogroup dbio */
/** @{ */
/** @file librt/db_open.c
 *
 * Routines for opening, creating, and replicating BRL-CAD geometry
 * database files.  BRL-CAD geometry database files are managed in a
 * given application through a "database instance" (dbi).  An
 * application may maintain multiple database instances open to a
 * given geometry database file.
 *
 */

#include "common.h"

#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"

#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/app.h"
#include "vmath.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "wdb.h"


#ifndef SEEK_SET
#  define SEEK_SET 0
#endif

#define DEFAULT_DB_TITLE "Untitled BRL-CAD Database"


struct db_i *
db_open(const char *name, const char *mode)
{
    static int sem_uses = 0;
    extern int RT_SEM_WORKER;
    extern int RT_SEM_RESULTS;
    extern int RT_SEM_MODEL;
    extern int RT_SEM_TREE0;
    extern int RT_SEM_TREE1;
    extern int RT_SEM_TREE2;
    extern int RT_SEM_TREE3;
    register struct db_i *dbip = DBI_NULL;
    register int i;
    char **argv;

    if (!sem_uses)
	sem_uses = bu_semaphore_register("LIBRT_SEM_USES");

    if (!RT_SEM_WORKER)
	RT_SEM_WORKER = bu_semaphore_register("RT_SEM_WORKER");
    if (!RT_SEM_RESULTS)
	RT_SEM_RESULTS = bu_semaphore_register("RT_SEM_RESULTS");
    if (!RT_SEM_MODEL)
	RT_SEM_MODEL = bu_semaphore_register("RT_SEM_MODEL");
    if (!RT_SEM_TREE0)
	RT_SEM_TREE0 = bu_semaphore_register("RT_SEM_TREE0");
    if (!RT_SEM_TREE1)
	RT_SEM_TREE1 = bu_semaphore_register("RT_SEM_TREE1");
    if (!RT_SEM_TREE2)
	RT_SEM_TREE2 = bu_semaphore_register("RT_SEM_TREE2");
    if (!RT_SEM_TREE3)
	RT_SEM_TREE3 = bu_semaphore_register("RT_SEM_TREE3");

    if (name == NULL)
	return DBI_NULL;

    if (RT_G_DEBUG & RT_DEBUG_DB) {
	bu_log("db_open(%s, %s)\n", name, mode);
    }

    if (mode && mode[0] == 'r' && mode[1] == '\0') {
	/* Read-only mode */

	struct bu_mapped_file *mfp;

	mfp = bu_open_mapped_file(name, "db_i");
	if (mfp == NULL) {
	    if (RT_G_DEBUG & RT_DEBUG_DB) {
		bu_log("db_open(%s) FAILED, unable to open as a mapped file\n", name);
	    }
	    return DBI_NULL;
	}

	/* Is this a re-use of a previously mapped file? */
	if (mfp->apbuf) {
	    dbip = (struct db_i *)mfp->apbuf;
	    RT_CK_DBI(dbip);
	    bu_semaphore_acquire(sem_uses);
	    dbip->dbi_uses++;
	    bu_semaphore_release(sem_uses);

	    /*
	     * decrement the mapped file reference counter by 1,
	     * references are already counted in dbip->dbi_uses
	     */
	    bu_close_mapped_file(mfp);

	    if (RT_G_DEBUG & RT_DEBUG_DB) {
		bu_log("db_open(%s) dbip=%p: reused previously mapped file\n", name, (void *)dbip);
	    }

	    return dbip;
	}

	BU_ALLOC(dbip, struct db_i);
	dbip->dbi_mf = mfp;
	dbip->dbi_eof = (b_off_t)mfp->buflen;
	dbip->dbi_inmem = mfp->buf;
	dbip->dbi_mf->apbuf = (void *)dbip;

	/* Do this too, so we can seek around on the file */
	if ((dbip->dbi_fp = fopen(name, "rb")) == NULL) {
	    if (RT_G_DEBUG & RT_DEBUG_DB) {
		bu_log("db_open(%s) FAILED, unable to open file for reading\n", name);
	    }
	    bu_free((char *)dbip, "struct db_i");
	    return DBI_NULL;
	}

	dbip->dbi_use_comb_instance_ids = 0;
	const char *need_comb_inst = getenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS");
	if (BU_STR_EQUAL(need_comb_inst, "1")) {
	    dbip->dbi_use_comb_instance_ids = 1;
	}

	dbip->dbi_read_only = 1;
    } else {
	/* Read-write mode */

	BU_ALLOC(dbip, struct db_i);
	dbip->dbi_eof = (b_off_t)-1L;

	if ((dbip->dbi_fp = fopen(name, "r+b")) == NULL) {
	    if (RT_G_DEBUG & RT_DEBUG_DB) {
		bu_log("db_open(%s) FAILED, unable to open file for reading/writing\n", name);
	    }
	    bu_free((char *)dbip, "struct db_i");
	    return DBI_NULL;
	}

	dbip->dbi_use_comb_instance_ids = 0;
	const char *need_comb_inst = getenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS");
	if (BU_STR_EQUAL(need_comb_inst, "1")) {
	    dbip->dbi_use_comb_instance_ids = 1;
	}

	dbip->dbi_read_only = 0;
    }

    /* Initialize fields */
    for (i = 0; i < RT_DBNHASH; i++)
	dbip->dbi_Head[i] = RT_DIR_NULL;

    dbip->dbi_local2base = 1.0;		/* mm */
    dbip->dbi_base2local = 1.0;
    dbip->dbi_title = (char *)0;
    dbip->dbi_uses = 1;

    /* FIXME: At some point, expand argv search paths with
     * getenv("BRLCAD_FILE_PATH") paths
     */

    /* intentionally acquiring dynamic memory here since we set
     * dbip->dbi_filepath to argv.  arg values and array memory are
     * released during db_close.
     */
    char *nname = bu_file_realpath(name, NULL);
    argv = (char **)bu_malloc(3 * sizeof(char *), "dbi_filepath[3]");
    argv[0] = bu_strdup(".");
    argv[1] = bu_path_dirname(nname);
    argv[2] = NULL;
    dbip->dbi_filepath = argv;
    bu_free(nname, "realpath");

#if !defined(_WIN32) || defined(__CYGWIN__)
    /* If not a full path */
    if (argv[1][0] != '/') {
	struct bu_vls fullpath = BU_VLS_INIT_ZERO;

	bu_free((void *)argv[1], "db_open: argv[1]");
	argv[1] = bu_getcwd((char *)NULL, (size_t)MAXPATHLEN);

	/* Something went wrong and we didn't get the CWD. So,
	 * free up any memory allocated here and return DBI_NULL */
	if (BU_STR_EQUAL(argv[1], ".") || BU_STR_EMPTY(argv[1])) {
	    bu_close_mapped_file(dbip->dbi_mf);
	    bu_free_mapped_files(0);
	    dbip->dbi_mf = (struct bu_mapped_file *)NULL;

	    if (dbip->dbi_fp) {
		fclose(dbip->dbi_fp);
	    }

	    bu_free((void *)argv[0], "db_open: argv[0]");
	    bu_free((void *)argv, "db_open: argv");
	    bu_free((char *)dbip, "struct db_i");

	    return DBI_NULL;
	}

	bu_vls_printf(&fullpath, "%s/%s", argv[1], name);
	dbip->dbi_filename = bu_strdup(bu_vls_addr(&fullpath));
	bu_vls_free(&fullpath);
    } else {
	/* Record the filename and file path */
	dbip->dbi_filename = bu_strdup(name);
    }
#else
    /* Record the filename and file path */
    dbip->dbi_filename = bu_strdup(name);
#endif

    bu_ptbl_init(&dbip->dbi_clients, 128, "dbi_clients[]");
    bu_ptbl_init(&dbip->dbi_changed_clbks , 8, "dbi_changed_clbks]");
    bu_ptbl_init(&dbip->dbi_update_nref_clbks, 8, "dbi_update_nref_clbks");

    dbip->dbi_use_comb_instance_ids = 0;
    const char *need_comb_inst = getenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS");
    if (BU_STR_EQUAL(need_comb_inst, "1")) {
	dbip->dbi_use_comb_instance_ids = 1;
    }

    dbip->dbi_magic = DBI_MAGIC;		/* Now it's valid */

    /* determine version */
    dbip->dbi_version = 0; /* make db_version() calculate */
    dbip->dbi_version = db_version(dbip);

    if (dbip->dbi_version < 5) {
	if (rt_db_flip_endian(dbip)) {
	    if (dbip->dbi_version > 0) {
		/* version is stored as -4 to denote the flip */
		dbip->dbi_version *= -1;
	    }
	    dbip->dbi_read_only = 1;
	    bu_log("WARNING: Binary-incompatible v4 geometry database detected.\n");
	    bu_log(" Endianness flipped.  Converting to READ ONLY.\n");
	}
    }

    if (RT_G_DEBUG & RT_DEBUG_DB) {
	bu_log("db_open(%s) dbip=%p version=%d\n", dbip->dbi_filename, (void *)dbip, dbip->dbi_version);
    }

    /* Initialize tolerances */

    /* Set up the four possible wdb containers. */
    if (rt_uniresource.re_magic != RESOURCE_MAGIC)
	rt_init_resource(&rt_uniresource, 0, NULL);

    BU_ALLOC(dbip->dbi_wdbp, struct rt_wdb);
    wdb_init(dbip->dbi_wdbp, dbip, RT_WDB_TYPE_DB_DISK);
    dbip->dbi_wdbp->wdb_resp = &rt_uniresource;
    BN_TOL_INIT_SET_TOL(&dbip->dbi_wdbp->wdb_tol);
    BG_TESS_TOL_INIT_SET_TOL(&dbip->dbi_wdbp->wdb_ttol);

    BU_ALLOC(dbip->dbi_wdbp_a, struct rt_wdb);
    wdb_init(dbip->dbi_wdbp_a, dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY);
    dbip->dbi_wdbp_a->wdb_resp = &rt_uniresource;
    BN_TOL_INIT_SET_TOL(&dbip->dbi_wdbp_a->wdb_tol);
    BG_TESS_TOL_INIT_SET_TOL(&dbip->dbi_wdbp_a->wdb_ttol);

    BU_ALLOC(dbip->dbi_wdbp_inmem, struct rt_wdb);
    wdb_init(dbip->dbi_wdbp_inmem, dbip, RT_WDB_TYPE_DB_INMEM);
    dbip->dbi_wdbp_inmem->wdb_resp = &rt_uniresource;
    BN_TOL_INIT_SET_TOL(&dbip->dbi_wdbp_inmem->wdb_tol);
    BG_TESS_TOL_INIT_SET_TOL(&dbip->dbi_wdbp_inmem->wdb_ttol);

    BU_ALLOC(dbip->dbi_wdbp_inmem_a, struct rt_wdb);
    wdb_init(dbip->dbi_wdbp_inmem_a, dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);
    dbip->dbi_wdbp_inmem_a->wdb_resp = &rt_uniresource;
    BN_TOL_INIT_SET_TOL(&dbip->dbi_wdbp_inmem_a->wdb_tol);
    BG_TESS_TOL_INIT_SET_TOL(&dbip->dbi_wdbp_inmem_a->wdb_ttol);

    return dbip;
}

struct db_i *
db_create(const char *name, int version)
{
    FILE *fp;
    struct db_i *dbip;
    int result;

    if (name == NULL) return DBI_NULL;

    if (RT_G_DEBUG & RT_DEBUG_DB)
	bu_log("db_create(%s, %d)\n", name, version);

    fp = fopen(name, "w+b");

    if (fp == NULL) {
	perror(name);
	return DBI_NULL;
    }

    if (version == 5) {
	result = db5_fwrite_ident(fp, DEFAULT_DB_TITLE, 1.0);
    } else if (version == 4) {
	result = db_fwrite_ident(fp, DEFAULT_DB_TITLE, 1.0);
    } else {
	bu_log("WARNING: db_create() was provided an unrecognized version number: %d\n", version);
	result = db5_fwrite_ident(fp, DEFAULT_DB_TITLE, 1.0);
    }

    (void)fclose(fp);

    if (result < 0)
	return DBI_NULL;

    if ((dbip = db_open(name, DB_OPEN_READWRITE)) == DBI_NULL)
	return DBI_NULL;

    /* Do a quick scan to determine version, find _GLOBAL, etc. */
    if (db_dirbuild(dbip) < 0)
	return DBI_NULL;

    return dbip;
}


void
db_close_client(struct db_i *dbip, long int *client)
{
    if (!dbip)
	return;

    RT_CK_DBI(dbip);

    if (client) {
	(void)bu_ptbl_rm(&dbip->dbi_clients, client);
    }

    db_close(dbip);
}


void
db_close(register struct db_i *dbip)
{
    register int i;
    register struct directory *dp, *nextdp;
    static int sem_uses = 0;
    if (!sem_uses)
	sem_uses = bu_semaphore_register("LIBRT_SEM_USES");

    if (!dbip)
	return;

    RT_CK_DBI(dbip);
    if (RT_G_DEBUG&RT_DEBUG_DB) bu_log("db_close(%s) %p uses=%d\n",
				    dbip->dbi_filename, (void *)dbip, dbip->dbi_uses);

    bu_semaphore_acquire(sem_uses);
    if ((--dbip->dbi_uses) > 0) {
	bu_semaphore_release(sem_uses);
	/* others are still using this database */
	return;
    }
    bu_semaphore_release(sem_uses);

    /* Free wdbp containers */
    if (dbip->dbi_wdbp) {
	BU_LIST_DEQUEUE(&dbip->dbi_wdbp->l);
	BU_LIST_MAGIC_SET(&dbip->dbi_wdbp->l, 0);
	bu_vls_free(&dbip->dbi_wdbp->wdb_name);
	bu_vls_free(&dbip->dbi_wdbp->wdb_prestr);
	dbip->dbi_wdbp->type = 0;
	dbip->dbi_wdbp->wdb_resp = NULL;
	dbip->dbi_wdbp->wdb_interp = NULL;
	bu_free((void *)dbip->dbi_wdbp, "struct rt_wdb");
	dbip->dbi_wdbp = NULL;
    }
    dbip->dbi_wdbp = NULL;

    if (dbip->dbi_wdbp_a) {
	BU_LIST_DEQUEUE(&dbip->dbi_wdbp_a->l);
	BU_LIST_MAGIC_SET(&dbip->dbi_wdbp_a->l, 0);
	bu_vls_free(&dbip->dbi_wdbp_a->wdb_name);
	bu_vls_free(&dbip->dbi_wdbp_a->wdb_prestr);
	dbip->dbi_wdbp_a->type = 0;
	dbip->dbi_wdbp_a->wdb_resp = NULL;
	dbip->dbi_wdbp_a->wdb_interp = NULL;
	bu_free((void *)dbip->dbi_wdbp_a, "struct rt_wdb");
	dbip->dbi_wdbp_a = NULL;
    }
    dbip->dbi_wdbp_a = NULL;

    if (dbip->dbi_wdbp_inmem) {
	BU_LIST_DEQUEUE(&dbip->dbi_wdbp_inmem->l);
	BU_LIST_MAGIC_SET(&dbip->dbi_wdbp_inmem->l, 0);
	bu_vls_free(&dbip->dbi_wdbp_inmem->wdb_name);
	bu_vls_free(&dbip->dbi_wdbp_inmem->wdb_prestr);
	dbip->dbi_wdbp_inmem->type = 0;
	dbip->dbi_wdbp_inmem->wdb_resp = NULL;
	dbip->dbi_wdbp_inmem->wdb_interp = NULL;
	bu_free((void *)dbip->dbi_wdbp_inmem, "struct rt_wdb");
	dbip->dbi_wdbp_inmem = NULL;
    }
    dbip->dbi_wdbp_inmem = NULL;

    if (dbip->dbi_wdbp_inmem_a) {
	BU_LIST_DEQUEUE(&dbip->dbi_wdbp_inmem_a->l);
	BU_LIST_MAGIC_SET(&dbip->dbi_wdbp_inmem_a->l, 0);
	bu_vls_free(&dbip->dbi_wdbp_inmem_a->wdb_name);
	bu_vls_free(&dbip->dbi_wdbp_inmem_a->wdb_prestr);
	dbip->dbi_wdbp_inmem_a->type = 0;
	dbip->dbi_wdbp_inmem_a->wdb_resp = NULL;
	dbip->dbi_wdbp_inmem_a->wdb_interp = NULL;
	bu_free((void *)dbip->dbi_wdbp_inmem_a, "struct rt_wdb");
	dbip->dbi_wdbp_inmem_a = NULL;
    }
    dbip->dbi_wdbp_inmem_a = NULL;

    /* ready to free the database -- use count is now zero */

    /* free up any mapped files */
    bu_close_mapped_file(dbip->dbi_mf);
    bu_free_mapped_files(0);
    dbip->dbi_mf = (struct bu_mapped_file *)NULL;

    /* try to ensure/encourage that the file is written out */
    db_sync(dbip);

    if (dbip->dbi_fp) {
	fclose(dbip->dbi_fp);
    }

    if (dbip->dbi_title)
	bu_free(dbip->dbi_title, "dbi_title");
    if (dbip->dbi_filename)
	bu_free(dbip->dbi_filename, "dbi_filename");

    db_free_anim(dbip);
    rt_color_free();		/* Free MaterHead list */

    /* Release map of database holes */
    rt_mempurge(&(dbip->dbi_freep));
    rt_memclose();

    dbip->dbi_inmem = NULL;		/* sanity */

    bu_ptbl_free(&dbip->dbi_clients);
    if (BU_PTBL_IS_INITIALIZED(&dbip->dbi_changed_clbks))
	bu_ptbl_free(&dbip->dbi_changed_clbks);
    if (BU_PTBL_IS_INITIALIZED(&dbip->dbi_update_nref_clbks))
	bu_ptbl_free(&dbip->dbi_update_nref_clbks);

    /* Free all directory entries */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL;) {
	    RT_CK_DIR(dp);
	    nextdp = dp->d_forw;
	    RT_DIR_FREE_NAMEP(dp);	/* frees d_namep */

	    if ((dp->d_flags & RT_DIR_INMEM) && (dp->d_un.ptr != NULL)) {
		bu_free(dp->d_un.ptr, "db_close d_un.ptr");
		dp->d_un.ptr = NULL;
		dp->d_len    = 0;
	    }

	    /* Put 'dp' back on the freelist */
	    dp->d_forw = rt_uniresource.re_directory_hd;
	    rt_uniresource.re_directory_hd = dp;

	    /* null'ing the forward pointer here is a huge
	     * memory leak as it causes the loss of all
	     * nodes on the freelist except the first.
	     * (so don't do it)
	     */

	    dp = nextdp;
	}
	dbip->dbi_Head[i] = RT_DIR_NULL;	/* sanity*/
    }

    if (dbip->dbi_filepath != NULL) {
	bu_argv_free(2, dbip->dbi_filepath);
	dbip->dbi_filepath = NULL; /* sanity */
    }

    dbip->dbi_magic = (uint32_t)0x10101010;
    bu_free((char *)dbip, "struct db_i");
}

int
db_filetype(const char *fname)
{
    // If we can't open it, there's no way to tell if it's
    // a .g file
    if (!fname)
	return -1;
    FILE *fp = fopen(fname, "rb");
    if (!fp)
	return -1;

    // Get the file header - if we can't do that, it's not
    // a .g file
    unsigned char header[8];
    if (fread(header, sizeof(header), 1, fp) != 1) {
	fclose(fp);
	return -1;
    }
    fclose(fp);

    // Have a header - decide based on the header contents
    if (db5_header_is_valid(header))
	return 5;
    // Have encountered a leading 'I' character in some .tif
    // files, so check for 'v' and '4' as well.
    if (header[0] == 'I' && header[2] == 'v' && header[3] == '4')
	return 4;

    // Could not recognize
    return -1;
}

int
db_dump(struct rt_wdb *wdbp, struct db_i *dbip)
/* output */
/* input */
{
    register int i;
    register struct directory *dp;
    struct bu_external ext;

    RT_CK_DBI(dbip);
    RT_CK_WDB(wdbp);

    /* just in case since we don't actually handle it below */
    if (db_version(dbip) != db_version(wdbp->dbip)) {
	bu_log("Internal Error: dumping a v%d database into a v%d database is untested\n", db_version(dbip), db_version(wdbp->dbip));
	return -1;
    }

    //struct directory *out_global = db_lookup(wdbp->dbip, "_GLOBAL", LOOKUP_QUIET);

    /* Output all directory entries */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    RT_CK_DIR(dp);
	    //if (out_global && BU_STR_EQUAL(dp->d_namep, "_GLOBAL")) {
		//bu_log("db_dump() - in append-only mode, and target db already has a _GLOBAL object");
		//continue;
	    //}
	    /* XXX Need to go to internal form, if database versions don't match */
	    if (db_get_external(&ext, dp, dbip) < 0) {
		bu_log("db_dump() read failed on %s, skipping\n", dp->d_namep);
		continue;
	    }
	    if (wdb_export_external(wdbp, &ext, dp->d_namep, dp->d_flags & ~(RT_DIR_INMEM), dp->d_minor_type) < 0) {
		bu_log("db_dump() write failed on %s, aborting\n", dp->d_namep);
		bu_free_external(&ext);
		return -1;
	    }
	    bu_free_external(&ext);
	}
    }
    return 0;
}

struct db_i *
db_clone_dbi(struct db_i *dbip, long int *client)
{
    static int sem_uses = 0;
    if (!sem_uses)
	sem_uses = bu_semaphore_register("LIBRT_SEM_USES");

    RT_CK_DBI(dbip);

    bu_semaphore_acquire(sem_uses);
    dbip->dbi_uses++;
    bu_semaphore_release(sem_uses);
    if (client) {
	bu_ptbl_ins_unique(&dbip->dbi_clients, client);
    }
    return dbip;
}

void
db_sync(struct db_i *dbip)
{
    RT_CK_DBI(dbip);

    bu_semaphore_acquire(BU_SEM_SYSCALL);

    /* make sure we have something to do */
    if (!dbip->dbi_fp) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	return;
    }

    /* flush the file */
    (void)fflush(dbip->dbi_fp);

#if defined(HAVE_FSYNC)
    /* make sure it's written out */
    (void)fsync(fileno(dbip->dbi_fp));
#else
#  if defined(HAVE_SYNC)
    /* try the whole filesystem if sans fsync() */
    sync();
#  endif
#endif

    bu_semaphore_release(BU_SEM_SYSCALL);
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
