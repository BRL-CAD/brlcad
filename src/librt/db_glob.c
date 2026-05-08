/*                       D B _ G L O B . C
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
 * information.
 */
/** @file librt/db_glob.c
 *
 * Geometry-database backend for bu_glob(): implements gl_opendir,
 * gl_readdir, gl_closedir, gl_lstat and gl_stat callbacks that let
 * bu_glob() enumerate objects stored in a BRL-CAD .g file, and
 * provides the public db_path_glob() entry point.
 *
 * Namespace model
 * ---------------
 * An empty path ("") represents the flat root: opendir("") enumerates
 * ALL objects in the database by name, regardless of hierarchy.  This
 * preserves the existing ged-glob behaviour.  A non-empty path such as
 * "vehicle1" or "vehicle1/wheel1" is treated as a combination path;
 * opendir opens the last component as a combination and enumerates its
 * immediate children (deduplicated by name).
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>

#include "bio.h"

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bu/glob.h"

#include "vmath.h"
#include "rt/db4.h"
#include "rt/defines.h"
#include "rt/db_instance.h"
#include "rt/directory.h"
#include "rt/search.h"
#include "raytrace.h"
#include "librt_private.h"


/* -----------------------------------------------------------------------
 * Directory handle for the geometry-database backend
 * ----------------------------------------------------------------------- */

struct _db_glob_dir {
    struct db_i      *dbip;
    struct directory **entries;  /* array of dp pointers to iterate */
    int               nentries;
    int               idx;       /* current read position */
};


/* Collect all OP_DB_LEAF entries from a combination tree, deduplicated
 * by directory pointer, into a resizable array. */
static void
_db_glob_collect_tree(const union tree *tp,
	struct db_i *dbip,
	struct directory ***arr,
	int *narr,
	int *alloc)
{
    int i;

    if (!tp)
	return;
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    _db_glob_collect_tree(tp->tr_b.tb_left,  dbip, arr, narr, alloc);
	    _db_glob_collect_tree(tp->tr_b.tb_right, dbip, arr, narr, alloc);
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    _db_glob_collect_tree(tp->tr_b.tb_left, dbip, arr, narr, alloc);
	    break;
	case OP_DB_LEAF: {
			     struct directory *dp;

			     dp = db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET);
			     if (dp == RT_DIR_NULL)
				 break;

			     /* Deduplicate by directory pointer */
			     for (i = 0; i < *narr; i++) {
				 if ((*arr)[i] == dp)
				     break;
			     }
			     if (i < *narr)
				 break;  /* already present */

			     if (*narr >= *alloc) {
				 *alloc = *alloc ? *alloc * 2 : 16;
				 *arr = (struct directory **)bu_realloc(*arr,
					 (size_t)(*alloc) * sizeof(struct directory *),
					 "_db_glob children");
			     }
			     (*arr)[(*narr)++] = dp;
			     break;
			 }
	default:
			 break;
    }
}


/* -----------------------------------------------------------------------
 * bu_glob callbacks
 * ----------------------------------------------------------------------- */

static void *
_db_glob_opendir(const char *path, void *data)
{
    struct db_i *dbip = (struct db_i *)data;
    struct _db_glob_dir *dctx;
    struct directory **entries = NULL;
    int nentries = 0;
    int alloc = 0;

    RT_CK_DBI(dbip);

    BU_ALLOC(dctx, struct _db_glob_dir);
    dctx->dbip = dbip;

    if (!path || *path == '\0') {
	/* Flat root: enumerate every object in the database. */
	struct directory *dp;
	FOR_ALL_DIRECTORY_START(dp, dbip)
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	if (dp->d_flags & RT_DIR_HIDDEN)
	    continue;

	if (nentries >= alloc) {
	    alloc = alloc ? alloc * 2 : 64;
	    entries = (struct directory **)bu_realloc(entries,
		    (size_t)alloc * sizeof(struct directory *),
		    "_db_glob root entries");
	}
	entries[nentries++] = dp;
	FOR_ALL_DIRECTORY_END;

    } else {
	/* Combination path: open the last component as a comb. */
	const char *slash = strrchr(path, '/');
	const char *objname = slash ? slash + 1 : path;
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;

	dp = db_lookup(dbip, objname, LOOKUP_QUIET);
	if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB)) {
	    dctx->entries  = NULL;
	    dctx->nentries = 0;
	    dctx->idx      = 0;
	    return (void *)dctx;
	}

	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, bn_mat_identity) < 0) {
	    dctx->entries  = NULL;
	    dctx->nentries = 0;
	    dctx->idx      = 0;
	    return (void *)dctx;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	_db_glob_collect_tree(comb->tree, dbip, &entries, &nentries, &alloc);

	rt_db_free_internal(&intern);
    }

    dctx->entries  = entries;
    dctx->nentries = nentries;
    dctx->idx      = 0;
    return (void *)dctx;
}


static int
_db_glob_readdir(struct bu_dirent *de, void *handle)
{
    struct _db_glob_dir *dctx = (struct _db_glob_dir *)handle;

    if (!dctx || dctx->idx >= dctx->nentries)
	return 1;

    bu_vls_sprintf(de->name, "%s", dctx->entries[dctx->idx]->d_namep);
    dctx->idx++;
    return 0;
}


static void
_db_glob_closedir(void *handle)
{
    struct _db_glob_dir *dctx = (struct _db_glob_dir *)handle;
    if (!dctx)
	return;
    if (dctx->entries)
	bu_free(dctx->entries, "_db_glob entries");
    bu_free(dctx, "_db_glob_dir");
}


static int
_db_glob_lstat(const char *path, struct bu_stat *sb, void *data)
{
    struct db_i *dbip = (struct db_i *)data;
    const char *slash;
    const char *objname;
    struct directory *dp;

    RT_CK_DBI(dbip);

    /* Use the last component of the path as the object name */
    slash   = strrchr(path, '/');
    objname = slash ? slash + 1 : path;

    if (!objname || *objname == '\0')
	return -1;

    dp = db_lookup(dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return -1;

    sb->is_dir = (dp->d_flags & RT_DIR_COMB) ? 1 : 0;
    sb->size   = 0;
    return 0;
}


/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

int
db_path_glob(struct bu_glob_context *gp,
	const char *pattern,
	int flags,
	const struct db_i *dbip)
{
    if (!gp || !pattern || !dbip)
	return -1;
    RT_CK_DBI(dbip);

    gp->gl_opendir  = _db_glob_opendir;
    gp->gl_readdir  = _db_glob_readdir;
    gp->gl_closedir = _db_glob_closedir;
    gp->gl_lstat    = _db_glob_lstat;
    gp->gl_stat     = _db_glob_lstat; /* no symlinks in .g files */
    gp->data        = (void *)dbip;

    return bu_glob(pattern, flags, gp);
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
