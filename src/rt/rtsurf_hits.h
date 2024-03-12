/*                   R T S U R F _ H I T S . H
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/**
 *
 * Keep track of rtsurf hits separately using C++ containers.
 *
 */

#ifndef RTSURF_HITS_H
#define RTSURF_HITS_H

#include "common.h"

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * initialize a new context for tracking hit counters.
 */
void*
rtsurf_context_create(void);


/**
 * destroy a tracking context releasing all memory.
 */
void
rtsurf_context_destroy(void* context);


/**
 * reset the hit stats
 */
void
rtsurf_context_reset(void* context);


/**
 * count a hit on a given region of a specified material.
 */
void
rtsurf_register_hit(void* context, const char *region, int materialId);


/**
 * count a shotline sampled
 */
void
rtsurf_register_line(void* context);


/**
 * retrieve the counter for a given region or material ID.
 *
 * specify -1 or NULL to not retrieve a hit count.  hit counts are
 * returned in the provided regionHits or materialHits counters if
 * non-NULL.
 */
void
rtsurf_get_hits(void* context, const char *region, int materialId, size_t *regionHits, size_t *materialHits, size_t *lines);


/**
 * iterate over all registered region IDs
 */
void
rtsurf_iterate_regions(void* context, void (*callback)(const char *region, size_t hits, size_t lines, void* data), void* data);


/**
 * iterate over all registered material IDs
 */
void
rtsurf_iterate_materials(void* context, void (*callback)(int materialId, size_t hits, size_t lines, void* data), void* data);


/**
 * iterate over all combination groups above regions
 */
void
rtsurf_iterate_groups(void* context, void (*callback)(const char *assembly, size_t hits, size_t lines, void* data), void* data);


#ifdef __cplusplus
}
#endif

#endif /* RTSURF_HITS_H */


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
