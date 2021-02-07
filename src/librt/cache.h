/*                         C A C H E . H
 * BRL-CAD
 *
 * Copyright (c) 2016-2021 United States Government as represented by
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
 * Caching of prep data
 *
 */


#ifndef LIBRT_CACHE_H
#define LIBRT_CACHE_H


#include "common.h"

#include "rt/db_internal.h"
#include "rt/soltab.h"


__BEGIN_DECLS

/*
 * TODO:
 *
 * - put an rt_cache in dbip or rtip so you can have cached and
 *   uncached databases open at the same time, instead of as tree
 *   walker data
 *
 * - make rt_cache_prep an implementation detail in prep
 *
 * - rt_cache_open(dir) instead of relying on LIBRT_CACHE global
 *
 * - move LIBRT_CACHE environment variable access into caller
 *   locations instead of from within rt_cache_open
 *
 * - use sem_open/sem_wait/sem_post for cross-application coordination
 *   of cache writing
 *
 */

/**
 * cache handle, opaque type
 */
struct rt_cache;

/**
 * opens and returns a cache handle if the cache is not disabled.
 *
 * cache may be disabled by setting LIBRT_CACHE to a non-empty false
 * value (e.g., LIBRT_CACHE="off").
 */
struct rt_cache *rt_cache_open(void);

/**
 * closes a cache handle if valid and open.
 */
void rt_cache_close(struct rt_cache *cache);

/**
 * ensures a solid is prepared, generating or reading prep data from
 * cache when available.
 *
 * if a cache is provided, the particular solid is looked up (and
 * cached) based on its matrix and export data.  if prep cache data
 * exists, it is read and used.  if prep cache data does not exist for
 * that object, rt_obj_prep() is run.
 *
 * typically, this means an object will prep via rt_obj_prep() the
 * first time it is encountered, and read from cache on subsequnt
 * calls.
 */
int rt_cache_prep(struct rt_cache *cache, struct soltab *stp, struct rt_db_internal *ip);


__END_DECLS


#endif


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
