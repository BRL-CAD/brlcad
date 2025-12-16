/*                         C A C H E . H
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
/** @file cache.h
 *
 * Routines for managing and accessing a key/value data store.
 *
 * NOTE:  Data backend storage is NOT guaranteed by this API from one BRL-CAD
 * version to the next. Older versions of libbu caches may use different
 * internal storage than newer ones.  Should a backend need to change, we will
 * strive to maintain support for reading in the previous backend when the new
 * one is released to allow for data migration.  However, cache data is
 * ultimately intended to be ephemeral and subject to regeneration - if the
 * maintenance burden of backwards compatibilty becomes too high the decision
 * will be made to drop support for reading older caches.
 *
 * For data that is canonical and not able to be regenerated from other sources
 * (such as geometry or user settings) bu_cache is not an appropriate storage
 * mechanism.
 */

#ifndef BU_CACHE_H
#define BU_CACHE_H 1

#include "common.h"

#include "bu/defines.h"

__BEGIN_DECLS


// Default maximum cache database size.
//
// Generally user code will want to use this, unless they want something larger
// - a cache that gets filled up will typically cause problems for users.
#define BU_CACHE_DEFAULT_DB_SIZE 4294967296

// Due to the backend implementation, cache key strings have a maximum limit.
// Set this to that max - we want to use it for string buffer allocations and
// snprintf.  NOTE: BRL-CAD compiled LMDB instances (which are what will
// pretty much always be used) will match this default, but if a system LMDB
// with a smaller max length is forced in it could be a problem.
//
// Also, BRL-CAD's API requires that keys be null terminated strings with no
// embedded nulls.  (Among other reasons, printing out a list of all keys
// would get gnarly if we allowed something like that to be a valid key.)
#define BU_CACHE_KEY_MAXLEN 511


struct bu_cache_impl;
struct bu_cache_txn;

struct bu_cache {
    struct bu_cache_impl *i;
};

/* Provide a struct data type that functions can use to return everything
 * needed to execute a deferred bu_cache_write operation. */
struct bu_cache_item {
    char key[BU_CACHE_KEY_MAXLEN];
    void *data;
    size_t data_len;
};

/**
 * Opens the specified cache database from the BRL-CAD BU_DIR_CACHE folder.
 * The cache_db string may contain a hierarchical path, but note that all
 * paths will be interpreted as relative to the BU_DIR_CACHE location - no
 * absolute paths can be specified.
 *
 * If the create flag is non-zero, bu_cache_open will create the specified
 * cache_db if it does not already exist.
 *
 * A zero argument to max_cache_size will result in BU_CACHE_DEFAULT_DB_SIZE
 * being used.  The size limit is a limit for the current session, not a
 * perminent upper limit on how large a given cache can grow.
 *
 * returns the bu_cache structure on success, NULL on failure.
 */
BU_EXPORT extern struct bu_cache *bu_cache_open(const char *cache_db, int create, size_t max_cache_size);

/**
 * Closes the bu_cache and frees all associated memory.  Will NOT close
 * if bu_cache_write still has an active txn - in that case, calling
 * code needs to call bu_cache_write_commit or bu_cache_write_abort before
 * bu_cache_close will succeed.  returns BRLCAD_ERROR if close was not
 * successful, else BRLCAD_OK.
 */
BU_EXPORT extern int bu_cache_close(struct bu_cache *c);

/**
 * Erase the specified cache from disk.  Any open instances of the
 * cache should be closed before calling this function.
 */
BU_EXPORT void bu_cache_erase(const char *cache_db);

/**
 * Retrieve data (read-only) from the cache using the specified key.  User
 * should call bu_cache_get_done once their use of data is complete.
 *
 * Returns the size of the retrieved data. If t is non-NULL and empty, a
 * transaction token is returned that can be reused in subsequent calls.
 * If it is non-NULL and non-empty, bu_cache_get treats *t as the current
 * transaction.  Note that when reusing a txn, the caller MUST call
 * bu_cache_get_done to finalize a transaction.
 *
 * If t is NULL, the data returned in data must be freed by the caller.
 */
BU_EXPORT size_t bu_cache_get(void **data, const char *key, struct bu_cache *c, struct bu_cache_txn **t);

/**
 * Data retrieved using bu_cache_get is temporary if we are using transaction
 * tokens - once the user is done either reading it or copying it, libbu needs
 * to be told that the usage of the returned data is complete. */
BU_EXPORT void bu_cache_get_done(struct bu_cache_txn **t);

/**
 * Assign data to the cache using the specified key.
 *
 * If t is NULL, the write is handled immediately as a complete operation, and
 * bu_cache_write immediately validates that the written data can be
 * successfully read.
 *
 * If t is non-NULL, the txn is not finalized and the calling code can queue up
 * more write operations for eventual commit.  This mode is MUCH more
 * performant when lots of rapid writes are needed, but no per-write read-back
 * validation is performed (since the write is not yet finalized.) This mode
 * requires explicit management of when to finalize the operation for the
 * database, and if the user wants to validate that reads successfully return
 * the expected data they will need to do it themselves, but the speedup
 * will almost always be worth it for any non-trivial data input.  If an individual
 * write DOES report failure (return size 0) in this mode, parent code should
 * not try to continue writing - they should call bu_cache_write_abort
 * immediately and handle the failure case, since the commit operation will
 * also ultimately fail.
 *
 * A cache may only have one active write txn at a time - this is enforced
 * internally in the implementation, and a bu_cache_write will fail if it
 * attempts to create a new write when one is already active.
 */
BU_EXPORT size_t bu_cache_write(void *data, size_t dsize, const char *key, struct bu_cache *c, struct bu_cache_txn **t);

/**
 * Like bu_cache_get_done for data retrieval, a series of write operations may
 * be either committed or (if the parent code has changed its mind) aborted.
 * t is the bu_cache_txn returned by bu_cache write's t parameter.
 *
 * returns BRLCAD_OK on success and BRLCAD_ERROR on error.
 */
BU_EXPORT int bu_cache_write_commit(struct bu_cache *c, struct bu_cache_txn **t);

/**
 * If the calling code decides not to commit a series of bu_cache_write calls staged
 * with a non-NULL t param to a bu_cache_write, call bu_cache_write_abort to clear
 * the decks. */
BU_EXPORT void bu_cache_write_abort(struct bu_cache_txn **t);

/**
 * Clear data associated with the specified key from the cache.  Because this is a
 * write operation as far as the database is concerned, it is treated the same way
 * bu_cache_write operations are as far as txn behavior is concerned.  If you have
 * a batch bu_cache_write in progress and want to do a clear, the write txn should
 * be provided to bu_cache_clear as well, and like other writes it will not be
 * finalized until commit is called.
 */
BU_EXPORT void bu_cache_clear(const char *key, struct bu_cache *c, struct bu_cache_txn **t);

/**
 * Get an array of keys present in the cache.  Caller is responsible
 * for freeing the keysv output (recommend using bu_argv_free).
 */
BU_EXPORT int bu_cache_keys(char ***keysv, struct bu_cache *c);


__END_DECLS

#endif /* BU_CACHE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
