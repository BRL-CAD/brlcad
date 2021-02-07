/*                          M E M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @file rt/mem.h
 *
 */

#ifndef RT_MEM_H
#define RT_MEM_H

#include "common.h"
#include "vmath.h"
#include "rt/defines.h"

__BEGIN_DECLS

/**
 * These structures are used to manage internal resource maps.
 * Typically these maps describe some kind of memory or file space.
 */
struct mem_map {
    struct mem_map *m_nxtp;     /**< @brief Linking pointer to next element */
    size_t m_size;              /**< @brief Size of this free element */
    b_off_t m_addr;               /**< @brief Address of start of this element */
};
#define MAP_NULL        ((struct mem_map *) 0)

/* memalloc.c -- non PARALLEL routines */

/**
 * Takes:               & pointer of map,
 * size.
 *
 * Returns:     NULL Error
 * address Otherwise
 *
 * Comments:
 * Algorithm is first fit.
 */
RT_EXPORT extern size_t rt_memalloc(struct mem_map **pp,
				    size_t size);

/**
 * Takes:               & pointer of map,
 * size.
 *
 * Returns:     NULL Error
 * address Otherwise
 *
 * Comments:
 * Algorithm is BEST fit.
 */
RT_EXPORT extern struct mem_map * rt_memalloc_nosplit(struct mem_map **pp,
						      size_t size);

/**
 * Takes:
 * size,
 * address.
 *
 * Comments:
 * The routine does not check for wrap around when increasing sizes
 * or changing addresses.  Other wrap-around conditions are flagged.
 */
RT_EXPORT extern void rt_memfree(struct mem_map **pp,
				 size_t size,
				 b_off_t addr);

/**
 * Take everything on the current memory chain, and place it on the
 * freelist.
 */
RT_EXPORT extern void rt_mempurge(struct mem_map **pp);

/**
 * Return all the storage used by the rt_mem_freemap.
 */
RT_EXPORT extern void rt_memclose(void);


__END_DECLS

#endif /* RT_MEM_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
