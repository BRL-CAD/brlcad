/*                      D C A C H E . C P P
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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
/** @addtogroup ged_db*/
/** @{ */
/** @file libged/dcache.cpp
 *
 * Manage cached drawing data.
 */

#include "common.h"

#include <algorithm>
#include <map>
#include <thread>
#include <fstream>
#include <sstream>

extern "C" {
#include "lmdb.h"
}

#include "./alphanum.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/hash.h"
#include "bu/path.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "bv/lod.h"
#include "raytrace.h"
#include "ged/defines.h"
#include "ged/view.h"
#include "./ged_private.h"

// Subdirectory in BRL-CAD cache to Dbi state data
#define DBI_CACHEDIR ".Dbi"

// Maximum database size.
#define CACHE_MAX_DB_SIZE 4294967296

// Define what format of the cache is current - if it doesn't match, we need
// to wipe and redo.
#define CACHE_CURRENT_FORMAT 1

struct ged_draw_cache {
    MDB_env *env;
    MDB_txn *txn;
    MDB_dbi dbi;
    struct bu_vls *fname;
};

void
dbi_cache_clear(struct ged_draw_cache *c)
{
    if (!c)
	return;
    char dir[MAXPATHLEN];
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, DBI_CACHEDIR, bu_vls_cstr(c->fname));
    bu_dirclear((const char *)dir);
}

struct ged_draw_cache *
dbi_cache_open(const char *name)
{
    // Hash the input filename to generate a key for uniqueness
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s", bu_path_normalize(name));

    unsigned long long hash = bu_data_hash(bu_vls_cstr(&fname), bu_vls_strlen(&fname)*sizeof(char));
    bu_path_component(&fname, bu_path_normalize(name), BU_PATH_BASENAME_EXTLESS);
    bu_vls_printf(&fname, "_%llu", hash);

    // Set up the container
    struct ged_draw_cache *c;
    BU_GET(c, struct ged_draw_cache);
    BU_GET(c->fname, struct bu_vls);
    bu_vls_init(c->fname);
    bu_vls_sprintf(c->fname, "%s", bu_vls_cstr(&fname));

    // Base maximum readers on an estimate of how many threads
    // we might want to fire off
    size_t mreaders = std::thread::hardware_concurrency();
    if (!mreaders)
	mreaders = 1;
    int ncpus = bu_avail_cpus();
    if (ncpus > 0 && (size_t)ncpus > mreaders)
	mreaders = (size_t)ncpus + 2;

    // Set up LMDB environments
    if (mdb_env_create(&c->env))
	goto ged_context_fail;
    if (mdb_env_set_maxreaders(c->env, mreaders))
	goto ged_context_close_fail;
    if (mdb_env_set_mapsize(c->env, CACHE_MAX_DB_SIZE))
	goto ged_context_close_fail;

    // Ensure the necessary top level dirs are present
    char dir[MAXPATHLEN];
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, NULL);
    if (!bu_file_exists(dir, NULL))
	bu_mkdir(dir);
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, DBI_CACHEDIR, NULL);
    if (!bu_file_exists(dir, NULL)) {
	bu_mkdir(dir);
    }
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, DBI_CACHEDIR, "format", NULL);
    if (!bu_file_exists(dir, NULL)) {
	// Note a format, so we can detect if what's there isn't compatible
	// with what this logic expects (in anticipation of future changes
	// to the on-disk format).
	FILE *fp = fopen(dir, "w");
	if (!fp)
	    goto ged_context_close_fail;
	fprintf(fp, "%d\n", CACHE_CURRENT_FORMAT);
	fclose(fp);
    } else {
	std::ifstream format_file(dir);
	size_t disk_format_version = 0;
	format_file >> disk_format_version;
	format_file.close();
	if (disk_format_version != CACHE_CURRENT_FORMAT) {
	    bu_log("Old GED drawing info cache (%zd) found - clearing\n", disk_format_version);
	    dbi_cache_clear(c);
	    mdb_env_close(c->env);
	    bu_vls_free(&fname);
	    bu_vls_free(c->fname);
	    BU_PUT(c->fname, struct bu_vls);
	    BU_PUT(c, struct ged_draw_cache);
	    return dbi_cache_open(name);
	}
	FILE *fp = fopen(dir, "w");
	if (!fp)
	    goto ged_context_close_fail;
	fprintf(fp, "%d\n", CACHE_CURRENT_FORMAT);
	fclose(fp);
    }

      // Create the specific LMDB cache dir, if not already present
      bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, DBI_CACHEDIR, bu_vls_cstr(&fname), NULL);
      if (!bu_file_exists(dir, NULL))
          bu_mkdir(dir);

      // Need to call mdb_env_sync() at appropriate points.
      if (mdb_env_open(c->env, dir, MDB_NOSYNC, 0664))
	  goto ged_context_close_fail;

      // Success - return the context
      return c;

      // If something went wrong, clean up and return NULL
  ged_context_close_fail:
      mdb_env_close(c->env);
  ged_context_fail:
      bu_vls_free(&fname);
      bu_vls_free(c->fname);
      BU_PUT(c->fname, struct bu_vls);
      BU_PUT(c, struct ged_draw_cache);
      return NULL;
}

void
dbi_cache_close(struct ged_draw_cache *c)
{
    if (!c)
	return;
    mdb_env_close(c->env);
    bu_vls_free(c->fname);
    BU_PUT(c->fname, struct bu_vls);
    BU_PUT(c, struct ged_draw_cache);
}

void
cache_write(struct ged_draw_cache *c, unsigned long long hash, const char *component, std::stringstream &s)
{
    if (!c || hash == 0 || !component)
	return;

    // Prepare inputs for writing
    MDB_val mdb_key;
    MDB_val mdb_data[2];
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);
    std::string buffer = s.str();

    // As implemented this shouldn't be necessary, since all our keys are below
    // the default size limit (511)
    //if (keystr.length()*sizeof(char) > mdb_env_get_maxkeysize(c->i->lod_env))
    //  return false;

    // Write out key/value to LMDB database, where the key is the hash
    // and the value is the serialized LoD data
    char *keycstr = bu_strdup(keystr.c_str());
    void *bdata = bu_calloc(buffer.length()+1, sizeof(char), "bdata");
    memcpy(bdata, buffer.data(), buffer.length()*sizeof(char));
    mdb_txn_begin(c->env, NULL, 0, &c->txn);
    mdb_dbi_open(c->txn, NULL, 0, &c->dbi);
    mdb_key.mv_size = keystr.length()*sizeof(char);
    mdb_key.mv_data = (void *)keycstr;
    mdb_data[0].mv_size = buffer.length()*sizeof(char);
    mdb_data[0].mv_data = bdata;
    mdb_data[1].mv_size = 0;
    mdb_data[1].mv_data = NULL;
    mdb_put(c->txn, c->dbi, &mdb_key, mdb_data, 0);
    mdb_txn_commit(c->txn);
    bu_free(keycstr, "keycstr");
    bu_free(bdata, "buffer data");
}

size_t
cache_get(struct ged_draw_cache *c, void **data, unsigned long long hash, const char *component)
{
    if (!c || !data || hash == 0 || !component)
	return 0;

    // Construct lookup key
    MDB_val mdb_key;
    MDB_val mdb_data[2];
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);

    // As implemented this shouldn't be necessary, since all our keys are below
    // the default size limit (511)
    //if (keystr.length()*sizeof(char) > mdb_env_get_maxkeysize(c->env))
    //  return 0;
    char *keycstr = bu_strdup(keystr.c_str());
    mdb_txn_begin(c->env, NULL, 0, &c->txn);
    mdb_dbi_open(c->txn, NULL, 0, &c->dbi);
    mdb_key.mv_size = keystr.length()*sizeof(char);
    mdb_key.mv_data = (void *)keycstr;
    int rc = mdb_get(c->txn, c->dbi, &mdb_key, &mdb_data[0]);
    if (rc) {
	bu_free(keycstr, "keycstr");
	(*data) = NULL;
	return 0;
    }
    bu_free(keycstr, "keycstr");
    (*data) = mdb_data[0].mv_data;

    return mdb_data[0].mv_size;
}

void
cache_del(struct ged_draw_cache *c, unsigned long long hash, const char *component)
{
    if (!c || hash == 0 || !component)
	return;

    // Construct lookup key
    MDB_val mdb_key;
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);

    mdb_txn_begin(c->env, NULL, 0, &c->txn);
    mdb_dbi_open(c->txn, NULL, 0, &c->dbi);
    mdb_key.mv_size = keystr.length()*sizeof(char);
    mdb_key.mv_data = (void *)keystr.c_str();
    mdb_del(c->txn, c->dbi, &mdb_key, NULL);
    mdb_txn_commit(c->txn);
}

void
cache_done(struct ged_draw_cache *c)
{
    if (!c)
	return;
    mdb_txn_commit(c->txn);
}

/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
