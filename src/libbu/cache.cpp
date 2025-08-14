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
 */

#include "common.h"

#include <chrono>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "lmdb.h"

#include "bio.h"
#include "bu/app.h"
#include "bu/cache.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"

static size_t
os_page_size()
{
#if defined(HAVE_WINDOWS_H)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
#elif defined(HAVE_SYSCONF_AVPHYS)
    long sz = sysconf(_SC_PAGESIZE);
    if (sz <= 0) sz = 4096; // fallback
    return (size_t)sz;
#else
    return 4096; // fallback
#endif
}

struct bu_cache_impl {
    MDB_env *env;
    MDB_dbi dbi;
    struct bu_vls *fname;
    int write_txn_active; // 0 = no active write txn, 1 = active
    std::mutex write_txn_mutex;
};

struct bu_cache_txn {
    MDB_txn *txn;
    struct bu_cache *cache;
};

struct bu_cache *
bu_cache_open(const char *cache_db, int create, size_t max_cache_size)
{
    size_t page_size;
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
    new (&c->i->write_txn_mutex) std::mutex; // Placement-new to construct the mutex
    BU_GET(c->i->fname, struct bu_vls);
    bu_vls_init(c->i->fname);
    bu_vls_sprintf(c->i->fname, "%s", cdb);
    c->i->write_txn_active = 0;

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

    // mapsize is supposed to be a multiple of the OS page size
    page_size = os_page_size();
    msize = (max_cache_size) ? (max_cache_size / page_size) * page_size : BU_CACHE_DEFAULT_DB_SIZE;
    if (mdb_env_set_mapsize(c->i->env, msize))
	goto bu_context_close_fail;

    // Need to call mdb_env_sync() at appropriate points.
    if (mdb_env_open(c->i->env, cdb, MDB_NOSYNC, 0664))
	goto bu_context_close_fail;

    // Do the initial dbi setup.  Opening with a write transaction
    // so we can create a non-existent database and get the proper
    // setup for writes (which is supported by the libbu API calls.)
    MDB_txn *txn;
    if (mdb_txn_begin(c->i->env, NULL, 0, &txn) != 0) // begin write txn
	goto bu_context_close_fail;
    if (mdb_dbi_open(txn, NULL, 0, &c->i->dbi) != 0) // open unnamed db
    {
	mdb_txn_abort(txn);
	goto bu_context_close_fail;
    }
    if (mdb_txn_commit(txn)) {
	goto bu_context_close_fail;
    }

    // Success - return the context
    return c;

bu_context_close_fail:
    mdb_env_close(c->i->env);
bu_context_fail:
    bu_vls_free(c->i->fname);
    BU_PUT(c->i->fname, struct bu_vls);
    c->i->write_txn_mutex.~mutex(); // Call destructor before freeing
    BU_PUT(c->i, struct bu_cache_impl);
    BU_PUT(c, struct bu_cache);
    return NULL;
}

int
bu_cache_close(struct bu_cache *c)
{
    if (!c)
	return BRLCAD_OK;

    {
	std::lock_guard<std::mutex> guard(c->i->write_txn_mutex); // lock
	if (c->i->write_txn_active) {
	    bu_log("Error - attempted to close cache with a write transaction active - commit or abort write before close\n");
	    return BRLCAD_ERROR;
	}
    }

    // Do a sync to make sure everything is on disk
    mdb_env_sync(c->i->env, 1);

    mdb_dbi_close(c->i->env, c->i->dbi);
    mdb_env_close(c->i->env);
    bu_vls_free(c->i->fname);
    BU_PUT(c->i->fname, struct bu_vls);
    BU_PUT(c->i, struct bu_cache_impl);
    BU_PUT(c, struct bu_cache);

    return BRLCAD_OK;
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

static
MDB_txn *
cache_get_read_txn(struct bu_cache *c, struct bu_cache_txn **t)
{
    if (UNLIKELY(!c))
	return NULL;

    MDB_txn *txn = (t && *t) ? (*t)->txn : NULL;
    if (txn)
	return txn;

    int txn_ret = mdb_txn_begin(c->i->env, NULL, MDB_RDONLY, &txn);
    if (txn_ret == MDB_MAP_RESIZED) {
	mdb_env_set_mapsize(c->i->env, 0);
	txn_ret = mdb_txn_begin(c->i->env, NULL, MDB_RDONLY, &txn);
    }

    // Start timeout clock
    auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(5);

    while (txn_ret == MDB_READERS_FULL) {
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	txn_ret = mdb_txn_begin(c->i->env, NULL, MDB_RDONLY, &txn);

	// Check if timeout exceeded
	auto now = std::chrono::steady_clock::now();
	if (now - start > timeout) {
	    // Timed out
	    bu_log("Error - txn acquisition timed out!\n");
	    return NULL;
	}
    }

    if (txn_ret != 0) {
	bu_log("Error - txn acquisition failed!: %d\n", txn_ret);
	return NULL;
    }

    if (t) {
	struct bu_cache_txn *ctxn;
	BU_GET(ctxn, struct bu_cache_txn);
	ctxn->txn = txn;
	ctxn->cache = c;
	*t = ctxn;
    }

    return txn;
}

size_t
bu_cache_get(void **data, const char *key, struct bu_cache *c, struct bu_cache_txn **t)
{
    if (!c || !key || !strlen(key))
	return 0;

    int mkeysize = (mdb_env_get_maxkeysize(c->i->env) < BU_CACHE_KEY_MAXLEN) ? mdb_env_get_maxkeysize(c->i->env) : BU_CACHE_KEY_MAXLEN;
    if (mkeysize <= 0)
	return 0;
    if (strlen(key) > (size_t)mkeysize) {
	if (mdb_env_get_maxkeysize(c->i->env) >= BU_CACHE_KEY_MAXLEN) {
	    bu_log("bu_cache_get called with a key string larger than the supported length (%d):\n%s\n", BU_CACHE_KEY_MAXLEN, key);
	    return 0;
	} else {
	    bu_log("ERROR: LMDB backend compiled with max key length (%d) smaller than that supported by libbu API (%d). key should be valid per libbu API but backend support is inadequate (needs to be fixed):\n%s\n", mdb_env_get_maxkeysize(c->i->env), BU_CACHE_KEY_MAXLEN, key);
	    return 0;
	}
    }

    MDB_val mdb_key;
    MDB_val mdb_data[2];

    MDB_txn *txn = cache_get_read_txn(c, t);
    if (!txn)
	return 0;
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
	if (mdatasize && data) {
	    void *dcpy = bu_malloc(mdatasize, "data copy");
	    memcpy(dcpy, mdb_data[0].mv_data, mdatasize);
	    (*data) = dcpy;
	}

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
bu_cache_get_done(struct bu_cache_txn **t)
{
    if (!t || !*t)
	return;

    struct bu_cache_txn *ctxn = *t;
    mdb_txn_abort(ctxn->txn);
    ctxn->cache = NULL;
    BU_PUT(ctxn, struct bu_cache_txn);
    *t = NULL;
}

static
MDB_txn *
cache_get_write_txn(struct bu_cache *c, struct bu_cache_txn **t)
{
    if (UNLIKELY(!c))
	return NULL;

    // We can't have multiple write txns per cache - lock to prevent
    // multiple threads trying this at the same time
    std::lock_guard<std::mutex> guard(c->i->write_txn_mutex);

    // If we already have a write txn and we're trying to start another
    // one, that's a no-no
    if ((!t || !*t) && c->i->write_txn_active) {
        bu_log("Error: Attempt to start a second write transaction on the same cache.\n");
        return NULL;
    }

    MDB_txn *txn = (t && *t) ? (*t)->txn : NULL;
    if (txn)
	return txn;

    int txn_ret = mdb_txn_begin(c->i->env, NULL, 0, &txn);
    if (txn_ret) {
	bu_log("bu_cache_write: couldn't begin txn: %d\n", txn_ret);
	if (txn_ret == MDB_MAP_RESIZED)
	    mdb_env_set_mapsize(c->i->env, 0);
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	txn_ret = mdb_txn_begin(c->i->env, NULL, 0, &txn);
    }

    if (txn_ret != 0) {
	bu_log("Error - txn acquisition failed!: %d\n", txn_ret);
	return NULL;
    }

    // We have an active write txn - let the cache know
    c->i->write_txn_active = 1;

    if (t) {
	struct bu_cache_txn *ctxn;
	BU_GET(ctxn, struct bu_cache_txn);
	ctxn->txn = txn;
	ctxn->cache = c;
	*t = ctxn;
    }

    return txn;
}

size_t
bu_cache_write(void *data, size_t dsize, const char *key, struct bu_cache *c, struct bu_cache_txn **t)
{
    if (!data || !key || !c || !strlen(key))
	return 0;

    int rc = 0;
    int mkeysize = mdb_env_get_maxkeysize(c->i->env);
    if (mkeysize <= 0 || strlen(key) > (size_t)mkeysize)
	return 0;

    // Prepare inputs for writing
    MDB_val mdb_key;
    MDB_val mdb_data[2];

    // Write out key/value to LMDB database, where the key is the hash
    // and the value is the serialized LoD data
    MDB_txn *txn = cache_get_write_txn(c, t);
    if (!txn)
	return 0;

    mdb_key.mv_size = (strlen(key)+1)*sizeof(char);
    mdb_key.mv_data = (void *)key;
    mdb_data[0].mv_size = dsize;
    mdb_data[0].mv_data = data;
    mdb_data[1].mv_size = 0;
    mdb_data[1].mv_data = NULL;
    // Use logical OR to collect errors
    rc |= mdb_put(txn, c->i->dbi, &mdb_key, mdb_data, 0);

    if (t) {
	// We're doing multiple writes, so at this point we're done one way or
	// the other.  If we were unsuccessful, return 0;
	if (rc)
	    return 0;

	// Put succeeded - return dsize
	return dsize;
    }


    // Not doing multiple writes - proceed
    rc |= mdb_txn_commit(txn);
    txn = NULL;
    mdb_env_sync(c->i->env, 0);

    // No longer have an active write txn - let the cache know
    c->i->write_txn_active = 0;

    // If we were unsuccessful, return 0;
    if (rc)
	return 0;

    // Since we're only writing out this one value, we can also make sure we can
    // read back what we wrote to validate that we succeeded.
    txn = cache_get_read_txn(c, NULL);
    if (!txn)
	return 0;
    rc = mdb_get(txn, c->i->dbi, &mdb_key, &mdb_data[0]);
    if (rc) {
	mdb_txn_abort(txn);
	return 0;
    }
    if (mdb_data[0].mv_size != dsize) {
	mdb_txn_abort(txn);
	return 0;
    }
    if (memcmp(mdb_data[0].mv_data, data, mdb_data[0].mv_size)) {
	mdb_txn_abort(txn);
	return 0;
    }

    size_t ret = mdb_data[0].mv_size;
    mdb_txn_abort(txn);

    return ret;
}

int
bu_cache_write_commit(struct bu_cache *c, struct bu_cache_txn **t)
{
    if (!c || !t || !(*t) || !(*t)->txn)
	return BRLCAD_ERROR;

    struct bu_cache_txn *ctxn = *t;
    int rc = 0;
    {
	std::lock_guard<std::mutex> guard(c->i->write_txn_mutex); // lock
	rc = mdb_txn_commit(ctxn->txn);
	// No longer have an active write txn - let the cache know
	ctxn->cache->i->write_txn_active = 0;
    }
    mdb_env_sync(c->i->env, 0);
    ctxn->cache = NULL;
    BU_PUT(ctxn, struct bu_cache_txn);
    *t = NULL;

    return (rc) ? BRLCAD_ERROR : BRLCAD_OK;
}

void
bu_cache_write_abort(struct bu_cache_txn **t)
{
    if (!t || !(*t) || !(*t)->txn)
	return;

    struct bu_cache_txn *ctxn = *t;
    {
	std::lock_guard<std::mutex> guard(ctxn->cache->i->write_txn_mutex); // lock
	mdb_txn_abort(ctxn->txn);
	// No longer have an active write txn - let the cache know
	ctxn->cache->i->write_txn_active = 0;
    }

    ctxn->cache = NULL;
    BU_PUT(ctxn, struct bu_cache_txn);
    *t = NULL;
}

void
bu_cache_clear(const char *key, struct bu_cache *c, struct bu_cache_txn **t)
{
    if (!key || !c)
	return;

    // Construct lookup key

    MDB_txn *txn = cache_get_write_txn(c, t);
    if (!txn)
	return;

    MDB_val mdb_key;
    mdb_key.mv_size = (strlen(key)+1)*sizeof(char);
    mdb_key.mv_data = (void *)key;
    mdb_del(txn, c->i->dbi, &mdb_key, NULL);

    if (t) {
	// We're doing multiple writes, so at this point we're done
	return;
    }

    int rc = 0;
    {
	std::lock_guard<std::mutex> guard(c->i->write_txn_mutex); // lock
	rc = mdb_txn_commit(txn);
	// No longer have an active write txn - let the cache know
	c->i->write_txn_active = 0;
    }
    if (rc && rc != MDB_NOTFOUND)
	bu_log("Clear operation for %s failed\n", key);
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

    MDB_txn *txn = cache_get_read_txn(c, NULL);
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
