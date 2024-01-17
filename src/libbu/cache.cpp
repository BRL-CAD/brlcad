/*                       C A C H E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2023 United States Government as represented by
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
/** @file cache.c
 *
 * Routines for creating and manipulating a key/value cache database.
 *
 */

#include "common.h"

#include <cstring>
#include <string>
#include <thread>
#include <vector>

#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"

#include "lmdb.h"

#include "bio.h"
#include "bu/app.h"
#include "bu/cache.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"

// Maximum database size.
#define CACHE_MAX_DB_SIZE 4294967296

struct bu_cache_impl {
    MDB_env *env;
    MDB_txn *txn; // TODO - do we need to support more than one of these for parallel reading...
    MDB_dbi dbi;
    MDB_txn *write_txn;  // Blocking, only need one
    MDB_dbi write_dbi;
    struct bu_vls *fname;
};

struct bu_cache *
bu_cache_open(const char *cache_db, int create)
{
    if (!cache_db)
	return NULL;

    char cdb[MAXPATHLEN];
    bu_dir(cdb, MAXPATHLEN, BU_DIR_CACHE, cache_db, NULL);

    if (!bu_file_exists(cdb, NULL)) {
	// If the cache isn't already present and we're not being told to create it
	// in that situation, we need to bail
	if (!create)
	    return NULL;

	// Ensure the necessary top level dirs are present
	bu_dir(cdb, MAXPATHLEN, BU_DIR_CACHE, NULL);
	if (!bu_file_exists(cdb, NULL))
	    bu_mkdir(cdb);

	// Break cache_db up into component directories with bu_path_component,
	// making sure each one exists
	std::vector<std::string> dirs;
	struct bu_vls cdbd = BU_VLS_INIT_ZERO;
	struct bu_vls ctmp = BU_VLS_INIT_ZERO;
	bu_path_component(&cdbd, cache_db, BU_PATH_DIRNAME);
	while (bu_vls_strlen(&cdbd)) {
	    bu_path_component(&ctmp, bu_vls_cstr(&cdbd), BU_PATH_BASENAME);
	    dirs.push_back(std::string(bu_vls_cstr(&cdbd)));
	    bu_vls_sprintf(&ctmp, "%s", bu_vls_cstr(&cdbd));
	    bu_path_component(&cdbd, bu_vls_cstr(&ctmp), BU_PATH_DIRNAME);
	}
	bu_dir(cdb, MAXPATHLEN, BU_DIR_CACHE, NULL);
	bu_vls_sprintf(&ctmp, "%s", cdb);
	for (long long i = dirs.size() - 1; i >= 0; i--) {
	    bu_vls_printf(&ctmp, "/%s", dirs[i].c_str());
	    if (!bu_file_exists(bu_vls_cstr(&ctmp), NULL))
		bu_mkdir((char *)bu_vls_cstr(&ctmp));
	}

	bu_dir(cdb, MAXPATHLEN, BU_DIR_CACHE, cache_db, NULL);
    }

    struct bu_cache *c = NULL;
    BU_GET(c, struct bu_cache);
    BU_GET(c->i, struct bu_cache_impl);
    BU_GET(c->i->fname, struct bu_vls);
    bu_vls_init(c->i->fname);
    bu_vls_sprintf(c->i->fname, "%s", cdb);

    // Base maximum readers on an estimate of how many threads
    // we might want to fire off
    size_t mreaders = std::thread::hardware_concurrency();
    if (!mreaders)
	mreaders = 1;
    int ncpus = bu_avail_cpus();
    if (ncpus > 0 && (size_t)ncpus > mreaders)
	mreaders = (size_t)ncpus + 2;

    // Set up LMDB environments
    if (mdb_env_create(&c->i->env))
	goto bu_context_fail;
    if (mdb_env_set_maxreaders(c->i->env, mreaders))
	goto bu_context_close_fail;
    if (mdb_env_set_mapsize(c->i->env, CACHE_MAX_DB_SIZE))
	goto bu_context_close_fail;

    // Need to call mdb_env_sync() at appropriate points.
    if (mdb_env_open(c->i->env, cdb, MDB_NOSYNC, 0664))
	goto bu_context_close_fail;

    // Success - return the context
    return c;

bu_context_close_fail:
    mdb_env_close(c->i->env);
bu_context_fail:
    bu_vls_free(c->i->fname);
    BU_PUT(c->i->fname, struct bu_vls);
    BU_PUT(c->i, struct bu_cache_impl);
    BU_PUT(c, struct bu_cache);
    return NULL;
}

void
bu_cache_close(struct bu_cache *c)
{
    if (!c)
	return;

    mdb_env_close(c->i->env);
    bu_vls_free(c->i->fname);
    BU_PUT(c->i->fname, struct bu_vls);
    BU_PUT(c->i, struct bu_cache_impl);
    BU_PUT(c, struct bu_cache);
}

void
bu_cache_erase(const char *cache_db)
{
    if (!cache_db)
	return;

    char cdb[MAXPATHLEN];
    bu_dir(cdb, MAXPATHLEN, BU_DIR_CACHE, cache_db, NULL);
    bu_dirclear(cdb);
}

size_t
bu_cache_get(void **data, const char *key, struct bu_cache *c)
{
    if (!data || !c || !key)
	return 0;

    int mkeysize = mdb_env_get_maxkeysize(c->i->env);
    if (mkeysize <= 0 || strlen(key) > (size_t)mkeysize)
	return 0;

    MDB_val mdb_key;
    MDB_val mdb_data[2];

    if (!c->i->txn) {
	mdb_txn_begin(c->i->env, NULL, 0, &c->i->txn);
	mdb_dbi_open(c->i->txn, NULL, 0, &c->i->dbi);
    }
    mdb_key.mv_size = strlen(key)*sizeof(char);
    mdb_key.mv_data = (void *)key;
    int rc = mdb_get(c->i->txn, c->i->dbi, &mdb_key, &mdb_data[0]);
    if (rc) {
	(*data) = NULL;
	return 0;
    }
    (*data) = mdb_data[0].mv_data;

    return mdb_data[0].mv_size;
}

void
bu_cache_get_done(const char *key, struct bu_cache *c)
{
    if (!key || !c)
	return;

    mdb_txn_commit(c->i->txn);
    c->i->txn = NULL;
}

size_t
bu_cache_write(void *data, size_t dsize, char *key, struct bu_cache *c)
{
    if (!data || !key || !c)
	return 0;

    int mkeysize = mdb_env_get_maxkeysize(c->i->env);
    if (mkeysize <= 0 || strlen(key) > (size_t)mkeysize)
	return 0;

    // Prepare inputs for writing
    MDB_val mdb_key;
    MDB_val mdb_data[2];

    // Write out key/value to LMDB database, where the key is the hash
    // and the value is the serialized LoD data
    mdb_txn_begin(c->i->env, NULL, 0, &c->i->write_txn);
    mdb_dbi_open(c->i->write_txn, NULL, 0, &c->i->write_dbi);
    mdb_key.mv_size = strlen(key)*sizeof(char);
    mdb_key.mv_data = (void *)key;
    mdb_data[0].mv_size = dsize;
    mdb_data[0].mv_data = data;
    mdb_data[1].mv_size = 0;
    mdb_data[1].mv_data = NULL;
    mdb_put(c->i->write_txn, c->i->write_dbi, &mdb_key, mdb_data, 0);
    mdb_txn_commit(c->i->write_txn);
    return dsize;
}

void
bu_cache_clear(const char *key, struct bu_cache *c)
{
    if (!key || !c)
	return;

    // Construct lookup key
    MDB_val mdb_key;

    mdb_txn_begin(c->i->env, NULL, 0, &c->i->write_txn);
    mdb_dbi_open(c->i->write_txn, NULL, 0, &c->i->write_dbi);
    mdb_key.mv_size = strlen(key)*sizeof(char);
    mdb_key.mv_data = (void *)key;
    mdb_del(c->i->write_txn, c->i->write_dbi, &mdb_key, NULL);
    mdb_txn_commit(c->i->write_txn);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
