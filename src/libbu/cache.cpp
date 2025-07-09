/*                       C A C H E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2025 United States Government as represented by
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
 * TODO - unit test that verifies read/write is successful and pushes
 * the limits of database size to see what the failure mode looks like.
 */

#include "common.h"

#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

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

struct bu_cache_impl {
    MDB_env *env;
    MDB_dbi dbi;
    MDB_txn *write_txn;  // Blocking, only need one
    MDB_dbi write_dbi;
    struct bu_vls *fname;
};

struct bu_cache *
bu_cache_open(const char *cache_db, int create, size_t max_cache_size)
{
    size_t msize;

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
	bu_path_component(&ctmp, cache_db, BU_PATH_BASENAME);
	bu_path_component(&cdbd, cache_db, BU_PATH_DIRNAME);
	while (bu_vls_strlen(&ctmp) && !BU_STR_EQUAL(bu_vls_cstr(&ctmp), ".") &&
	       (!(bu_vls_strlen(&ctmp) == 1 && bu_vls_cstr(&ctmp)[0] == BU_DIR_SEPARATOR))) {
	    dirs.push_back(std::string(bu_vls_cstr(&ctmp)));
	    bu_path_component(&ctmp, bu_vls_cstr(&cdbd), BU_PATH_BASENAME);
	    bu_path_component(&cdbd, bu_vls_cstr(&cdbd), BU_PATH_DIRNAME);
	}
	bu_dir(cdb, MAXPATHLEN, BU_DIR_CACHE, NULL);
	bu_vls_sprintf(&ctmp, "%s", cdb);
	for (long long i = dirs.size() - 1; i >= 0; i--) {
	    bu_vls_printf(&ctmp, "%c%s", BU_DIR_SEPARATOR, dirs[i].c_str());
	    if (!bu_file_exists(bu_vls_cstr(&ctmp), NULL))
		bu_mkdir((char *)bu_vls_cstr(&ctmp));
	}
	bu_vls_free(&ctmp);
	bu_vls_free(&cdbd);

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

    // mapsize is supposed to be a multiple of the OS page size - assuming
    // 4096 bytes for now, but maybe should add some code to actually
    // get the active page size...
    msize = (max_cache_size) ? (max_cache_size / 4096) * 4096 : BU_CACHE_DEFAULT_DB_SIZE;
    if (mdb_env_set_mapsize(c->i->env, msize))
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

    // Do a sync to make sure everything is on disk
    mdb_env_sync(c->i->env, 1);

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
bu_cache_get(void **data, const char *key, struct bu_cache *c, struct bu_cache_txn **t)
{
    if (!c || !key)
	return 0;

    int mkeysize = mdb_env_get_maxkeysize(c->i->env);
    if (mkeysize <= 0 || strlen(key) > (size_t)mkeysize)
	return 0;

    MDB_val mdb_key;
    MDB_val mdb_data[2];

    MDB_txn *txn = (t && *t) ? (MDB_txn *)*t : NULL;
    if (!txn) {
	mdb_txn_begin(c->i->env, NULL, MDB_RDONLY, &txn);
	mdb_dbi_open(txn, NULL, 0, &c->i->dbi);
	if (t)
	    *t = (struct bu_cache_txn *)txn;
    }
    mdb_key.mv_size = (strlen(key)+1)*sizeof(char);
    mdb_key.mv_data = (void *)key;
    int rc = mdb_get(txn, c->i->dbi, &mdb_key, &mdb_data[0]);
    if (rc) {
	if (data)
	    (*data) = NULL;
	if (!t)
	    mdb_txn_abort(txn);
	return 0;
    }
    if (!t) {
	// We're not persisting the transaction token - copy the data
	// before we finish up
	size_t mdatasize = mdb_data[0].mv_size;
	void *dcpy = bu_malloc(mdatasize, "data copy");
	memcpy(dcpy, mdb_data[0].mv_data, mdatasize);
	(*data) = dcpy;

	// Finalize and return
	mdb_txn_abort(txn);
	return mdatasize;
    } else {
	// We're persisting the transaction, so we avoid making a persistent
	// copy of the data
	if (data)
	    (*data) = mdb_data[0].mv_data;
	return mdb_data[0].mv_size;
    }
}

void
bu_cache_get_done(struct bu_cache_txn *t)
{
    if (!t)
	return;

    mdb_txn_abort((MDB_txn *)t);
}

size_t
bu_cache_write(void *data, size_t dsize, const char *key, struct bu_cache *c)
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
    mdb_key.mv_size = (strlen(key)+1)*sizeof(char);
    mdb_key.mv_data = (void *)key;
    mdb_data[0].mv_size = dsize;
    mdb_data[0].mv_data = data;
    mdb_data[1].mv_size = 0;
    mdb_data[1].mv_data = NULL;
    int rc = mdb_put(c->i->write_txn, c->i->write_dbi, &mdb_key, mdb_data, 0);
    mdb_txn_commit(c->i->write_txn);
    mdb_env_sync(c->i->env, 0);
    c->i->write_txn = NULL;
    // If we were unsuccessful, return 0;
    if (rc)
	return 0;

    // Make sure we can can read back what we wrote
    mdb_txn_begin(c->i->env, NULL, MDB_RDONLY, &c->i->write_txn);
    mdb_dbi_open(c->i->write_txn, NULL, 0, &c->i->write_dbi);
    rc = mdb_get(c->i->write_txn, c->i->write_dbi, &mdb_key, &mdb_data[0]);
    if (rc) {
	mdb_txn_abort(c->i->write_txn);
	c->i->write_txn = NULL;
	return 0;
    }
    if (mdb_data[0].mv_size != dsize) {
	mdb_txn_abort(c->i->write_txn);
	c->i->write_txn = NULL;
	return 0;
    }
    if (memcmp(mdb_data[0].mv_data, data, mdb_data[0].mv_size)) {
	mdb_txn_abort(c->i->write_txn);
	c->i->write_txn = NULL;
	return 0;
    }

    size_t ret = mdb_data[0].mv_size;
    mdb_txn_abort(c->i->write_txn);
    c->i->write_txn = NULL;

    return ret;
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
    mdb_key.mv_size = (strlen(key)+1)*sizeof(char);
    mdb_key.mv_data = (void *)key;
    mdb_del(c->i->write_txn, c->i->write_dbi, &mdb_key, NULL);
    mdb_txn_commit(c->i->write_txn);
    mdb_env_sync(c->i->env, 0);
}

int
bu_cache_keys(char ***keysv, struct bu_cache *c)
{
    if (!c || !keysv)
	return 0;

    struct bu_vls keystr = BU_VLS_INIT_ZERO;
    std::set<std::string> keys;
    MDB_val mdb_key, mdb_data;
    MDB_cursor *cursor;

    MDB_txn *txn;
    mdb_txn_begin(c->i->env, NULL, MDB_RDONLY, &txn);
    mdb_dbi_open(txn, NULL, 0, &c->i->dbi);
    int rc = mdb_cursor_open(txn, c->i->dbi, &cursor);
    if (rc) {
	mdb_txn_abort(txn);
	return 0;
    }
    rc = mdb_cursor_get(cursor, &mdb_key, &mdb_data, MDB_FIRST);
    if (rc) {
	mdb_cursor_close(cursor);
	mdb_txn_abort(txn);
	return 0;
    }

    bu_vls_strncpy(&keystr, (const char *)mdb_key.mv_data, mdb_key.mv_size-1);
    keys.insert(std::string(bu_vls_cstr(&keystr)));

    while (!mdb_cursor_get(cursor, &mdb_key, &mdb_data, MDB_NEXT)) {
	bu_vls_strncpy(&keystr, (const char *)mdb_key.mv_data, mdb_key.mv_size-1);
	keys.insert(std::string(bu_vls_cstr(&keystr)));
    }
    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);

    char **kv = (char **)bu_calloc(keys.size(), sizeof(const char *), "keys array");
    std::set<std::string>::iterator s_it;
    int kvi = 0;
    for (s_it = keys.begin(); s_it != keys.end(); ++s_it) {
	char *s = bu_strdup((*s_it).c_str());
	kv[kvi] = s;
	kvi++;
    }
    *keysv = kv;

    return kvi;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
