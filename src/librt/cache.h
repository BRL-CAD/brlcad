/*                         C A C H E . H
 * BRL-CAD
 *
 * Copyright (c) 2016-2018 United States Government as represented by
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


struct rt_cache;

struct rt_cache *rt_cache_open(void);
void rt_cache_close(struct rt_cache *cache);

/* rt_cache_prep() is thread-safe */
int rt_cache_prep(struct rt_cache *cache, struct soltab *stp,
		  struct rt_db_internal *ip);


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
