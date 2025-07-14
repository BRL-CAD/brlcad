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
// snprintf.
#define BU_CACHE_KEY_MAXLEN 511

// If we need to be certain cache operations are complete before proceeding,
// or nobody else will start a cache operation until we're done, use this
// semaphore.
BU_EXPORT extern int BU_SEM_CACHE;

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
 * Closes the bu_cache and frees all associated memory.
 */
BU_EXPORT extern void bu_cache_close(struct bu_cache *c);

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
 * transaction.  To finalize a transaction after multiple get calls, use
 * bu_cache_get_done.
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
 * Assign data to the cache using the specified key
 */
BU_EXPORT size_t bu_cache_write(void *data, size_t dsize, const char *key, struct bu_cache *c);

/**
 * Clear data associated with the specified key from the cache
 */
BU_EXPORT void bu_cache_clear(const char *key, struct bu_cache *c);

/**
 * Get an array of keys present in the cache
 */
BU_EXPORT int bu_cache_keys(char ***keysv, struct bu_cache *c);



// It is common convention within the BRL-CAD code to use a ":#[#...]" suffix
// after a hash string to denote a particular type of data when creating a hash
// key.  Most of these are internal, but in a few cases the type of hashed data
// is common to many different libraries.  Therefore, for convenience, we define
// a few of them here and document the expected storage to enable data reuse.

/* Axis Aligned Bounding Box - stored as an array of two points:
 * bbmin[0]bbmin[1][bbmin[2][bbmax[0]bbmax[1]bbmax[2] */
#define CACHE_AABB "aabb"

/* An inversion matrix is intended to take data defined at the origin after a
 * Principle Component Analysis and restore it to its original position.
 * Stored as an array of 16 fastf_t values per the vmath.h mat_t convention. */
#define CACHE_INV_MAT "im"

/* Oriented Bounding Box - stored as an array of a center point
 * c and three extent vectors v1, v2, v3:
 * c[0]c[1]c[2]v1[0]v1[1]v1[2]v2[0]v2[1]v2[2]v3[0]v3[1]v3[2] */
#define CACHE_OBB "obb"

/* Timestamp (microseconds since UNIX epoch) stored as a single int64_t */
#define CACHE_TIMESTAMP "timestmp"





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
