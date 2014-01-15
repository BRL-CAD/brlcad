/*                      M E M A L L O C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/memalloc.c
 *
 * Functions -
 * rt_memalloc allocate 'size' of memory from a given map
 * rt_memget allocate 'size' of memory from map at 'place'
 * rt_memfree return 'size' of memory to map at 'place'
 * rt_mempurge free everything on current memory chain
 * rt_memprint print a map
 * rt_memclose
 *
 * The structure of the displaylist memory map chains consists of
 * non-zero count and base address of that many contiguous units.  The
 * addresses are increasing and the list is terminated with the first
 * zero link.
 *
 * rt_memalloc() and rt_memfree() use these tables to allocate displaylist memory.
 *
 * For each Memory Map there exists a queue (coremap).  There also
 * exists a queue of free buffers which are enqueued on to either of
 * the previous queues.  Initially all of the buffers are placed on
 * the `freemap' queue.  Whenever a buffer is freed because of
 * coalescing ends in rt_memfree() or zero size in rt_memalloc() the
 * mapping buffer is taken off from the respective queue and returned
 * to the `freemap' queue.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"


/* XXX not PARALLEL */
/* Allocation/Free spaces */
static struct mem_map *rt_mem_freemap = MAP_NULL;	/* Freelist of buffers */

/* Flags used by `type' in rt_memfree() */
#define M_TMTCH 00001	/* Top match */
#define M_BMTCH 00002	/* Bottom match */
#define M_TOVFL 00004	/* Top overflow */
#define M_BOVFL 00010	/* Bottom overflow */

size_t
rt_memalloc(struct mem_map **pp, register size_t size)
{
    register struct mem_map *prevp = MAP_NULL;
    register struct mem_map *curp;
    size_t addr;

    if (size == 0)
	return 0L;	/* fail */

    for (curp = *pp; curp; curp = (prevp=curp)->m_nxtp) {
	if (curp->m_size >= size)
	    break;
    }

    if (curp == MAP_NULL)
	return 0L;	/* No more space */

    addr = (size_t)curp->m_addr;
    curp->m_addr += (off_t)size;

    /* If the element size goes to zero, put it on the freelist */

    if ((curp->m_size -= size) == 0) {
	if (prevp)
	    prevp->m_nxtp = curp->m_nxtp;
	else
	    *pp = curp->m_nxtp;	/* Click list down at start */
	curp->m_nxtp = rt_mem_freemap;		/* Link it in */
	rt_mem_freemap = curp;			/* Make it the start */
    }

    return addr;
}

struct mem_map *
rt_memalloc_nosplit(struct mem_map **pp, register size_t size)
{
    register struct mem_map *prevp = MAP_NULL;
    register struct mem_map *curp;
    register struct mem_map *best = MAP_NULL, *best_prevp = MAP_NULL;

    if (size == 0)
	return MAP_NULL;	/* fail */

    for (curp = *pp; curp; curp = (prevp=curp)->m_nxtp) {
	if (curp->m_size < size) continue;
	if (curp->m_size == size) {
	    best = curp;
	    best_prevp = prevp;
	    break;
	}
	/* This element has enough size */
	if (best == MAP_NULL || curp->m_size < best->m_size) {
	    best = curp;
	    best_prevp = prevp;
	}
    }
    if (!best)
	return MAP_NULL;	/* No space */

    /* Move this element to free list, return it, unsplit */
    if (best_prevp)
	best_prevp->m_nxtp = best->m_nxtp;
    else
	*pp = best->m_nxtp;	/* Click list down at start */
    best->m_nxtp = rt_mem_freemap;		/* Link it in */
    rt_mem_freemap = best;			/* Make it the start */

    return best;
}

size_t
rt_memget(struct mem_map **pp, register size_t size, off_t place)
{
    register struct mem_map *prevp, *curp;
    size_t addr;

    prevp = MAP_NULL;		/* special for first pass through */
    if (size == 0)
	bu_bomb("rt_memget() size==0\n");

    curp = *pp;
    while (curp) {
	/*
	 * Assumption: We will always be APPENDING to an existing
	 * memory allocation, so we search for a free piece of memory
	 * which begins at 'place', without worrying about ones which
	 * could begin earlier but be long enough to satisfy this
	 * request.
	 */
	if (curp->m_addr == place && curp->m_size >= size)
	    break;
	curp = (prevp=curp)->m_nxtp;
    }

    if (curp == MAP_NULL)
	return 0L;		/* No space here */

    addr = (size_t)curp->m_addr;
    curp->m_addr += (off_t)size;

    /* If the element size goes to zero, put it on the freelist */
    if ((curp->m_size -= size) == 0) {
	if (prevp)
	    prevp->m_nxtp = curp->m_nxtp;
	else
	    *pp = curp->m_nxtp;	/* Click list down at start */
	curp->m_nxtp = rt_mem_freemap;		/* Link it in */
	rt_mem_freemap = curp;			/* Make it the start */
    }
    return addr;
}


/**
 * Returns:	0 Unable to satisfy request
 * <size> Actual size of free block, may be larger
 * than requested size.
 *
 * Comments:
 * Caller is responsible for returning unused portion.
 */
size_t
rt_memget_nosplit(struct mem_map **pp, register size_t size, size_t place)
{
    register struct mem_map *prevp, *curp;

    prevp = MAP_NULL;		/* special for first pass through */
    if (size == 0)
	bu_bomb("rt_memget_nosplit() size==0\n");

    curp = *pp;
    while (curp) {
	/*
	 * Assumption: We will always be APPENDING to an existing
	 * memory allocation, so we search for a free piece of memory
	 * which begins at 'place', without worrying about ones which
	 * could begin earlier but be long enough to satisfy this
	 * request.
	 */
	if (curp->m_addr == (off_t)place && curp->m_size >= size) {
	    size = curp->m_size;
	    /* put this element on the freelist */
	    if (prevp)
		prevp->m_nxtp = curp->m_nxtp;
	    else
		*pp = curp->m_nxtp;	/* Click list down at start */
	    curp->m_nxtp = rt_mem_freemap;		/* Link it in */
	    rt_mem_freemap = curp;			/* Make it the start */
	    return size;		/* actual size found */
	}
	curp = (prevp=curp)->m_nxtp;
    }

    return 0L;		/* No space found */
}


void
rt_memfree(struct mem_map **pp, size_t size, off_t addr)
{
    register int type = 0;
    register struct mem_map *prevp = MAP_NULL;
    register struct mem_map *curp;
    off_t il;
    struct mem_map *tmap;

    if (size == 0)
	return;		/* Nothing to free */

    /* Find the position in the list such that (prevp)<(addr)<(curp) */
    for (curp = *pp; curp; curp = (prevp=curp)->m_nxtp)
	if (addr < curp->m_addr)
	    break;

    /* Make up the `type' variable */

    if (prevp) {
	il = prevp->m_addr + (off_t)prevp->m_size;
	if (il > addr)
	    type |= M_BOVFL;
	if (il == addr)
	    type |= M_BMTCH;
    }
    if (curp) {
	il = addr + (off_t)size;
	if (il > curp->m_addr)
	    type |= M_TOVFL;
	if (il == curp->m_addr)
	    type |= M_TMTCH;
    }

    if (type & (M_TOVFL|M_BOVFL)) {
	bu_log("rt_memfree(addr=%ld, size=%zu) ERROR type=0%o\n",
	       addr, size, type);
	if (prevp)
	    bu_log("prevp: m_addr=%ld, m_size=%zu\n",
		   prevp->m_addr, prevp->m_size);
	if (curp)
	    bu_log("curp: m_addr=%ld, m_size=%zu\n",
		   curp->m_addr, curp->m_size);
	return;
    }

    /*
     * Now we do the surgery:
     * If there are no matches on boundaries we allocate a buffer
     * If there is one match we expand the appropriate buffer
     * If there are two matches we will have a free buffer returned.
     */

    switch (type & (M_BMTCH|M_TMTCH)) {
	case M_TMTCH|M_BMTCH:	/* Deallocate top element and expand bottom */
	    prevp->m_size += size + curp->m_size;
	    prevp->m_nxtp = curp->m_nxtp;
	    curp->m_nxtp = rt_mem_freemap;		/* Link into rt_mem_freemap */
	    rt_mem_freemap = curp;
	    break;

	case M_BMTCH:		/* Expand bottom element */
	    prevp->m_size += size;
	    break;

	case M_TMTCH:		/* Expand top element downward */
	    curp->m_size += size;
	    curp->m_addr -= (off_t)size;
	    break;

	default:		/* No matches; allocate and insert */
	    if ((tmap=rt_mem_freemap) == MAP_NULL)
		BU_ALLOC(tmap, struct mem_map);
	    else
		rt_mem_freemap = rt_mem_freemap->m_nxtp;	/* Click one off */

	    if (prevp)
		prevp->m_nxtp = tmap;
	    else
		*pp = tmap;

	    tmap->m_size = size;
	    tmap->m_addr = addr;
	    tmap->m_nxtp = curp;
    }
}


void
rt_mempurge(struct mem_map **pp)
{
    register struct mem_map *prevp = MAP_NULL;
    register struct mem_map *curp;

    if (*pp == MAP_NULL)
	return;

    /* Find the end of the (busy) list */
    for (curp = *pp; curp; curp = (prevp=curp)->m_nxtp)
	;

    /* Put the whole busy list onto the free list */
    prevp->m_nxtp = rt_mem_freemap;
    rt_mem_freemap = *pp;

    *pp = MAP_NULL;
}


void
rt_memprint(struct mem_map **pp)
{
    register struct mem_map *curp;

    bu_log("rt_memprint(%p):  address, length\n", (void *)*pp);
    for (curp = *pp; curp; curp = curp->m_nxtp)
	bu_log(" a=%ld, l=%.5zu\n", curp->m_addr, curp->m_size);
}


void
rt_memclose(void)
{
    register struct mem_map *mp;

    while ((mp = rt_mem_freemap) != MAP_NULL) {
	rt_mem_freemap = mp->m_nxtp;
	bu_free((char *)mp, "struct mem_map " BU_FLSTR);
    }
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
