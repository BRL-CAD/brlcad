/*                         C A C H E . H
 * BRL-CAD
 *
 * Copyright (c) 2018-2024 United States Government as represented by
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
 */

#ifndef BU_CACHE_H
#define BU_CACHE_H 1

#include "common.h"

#include "bu/defines.h"

__BEGIN_DECLS

struct bu_cache_impl;

struct bu_cache {
    struct bu_cache_impl *i;
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
 * returns the bu_cache structure on success, NULL on failure.
 */
BU_EXPORT extern struct bu_cache *bu_cache_open(const char *cache_db, int create);

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
 * Returns the size of the retrieved data.
 */
BU_EXPORT size_t bu_cache_get(void **data, const char *key, struct bu_cache *c);

/**
 * Data retrieved using bu_cache_get is temporary - once the user is done
 * either reading it or copying it, libbu needs to be told that the usage of
 * the returned data is complete. */
BU_EXPORT void bu_cache_get_done(const char *key, struct bu_cache *c);

/**
 * Assign data to the cache using the specified key
 */
BU_EXPORT size_t bu_cache_write(void **data, const char *key, struct bu_cache *c);

/**
 * Clear data associated with the specified key from the cache
 */
BU_EXPORT void bu_cache_clear(const char *key, struct bu_cache *c);


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
