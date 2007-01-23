/*                      M E M A L L O C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file memalloc.c
 *
 * Functions -
 *	rt_memalloc	allocate 'size' of memory from a given map
 *	rt_memget	allocate 'size' of memory from map at 'place'
 *	rt_memfree	return 'size' of memory to map at 'place'
 *	rt_mempurge	free everything on current memory chain
 *	rt_memprint	print a map
 *	rt_memclose
 *
 * The structure of the displaylist memory map chains
 * consists of non-zero count and base address of that many contiguous units.
 * The addresses are increasing and the list is terminated with the
 * first zero link.
 *
 * rt_memalloc() and rt_memfree() use these tables to allocate displaylist memory.
 *
 *	For each Memory Map there exists a queue (coremap).
 *	There also exists a queue of free buffers which are enqueued
 *	on to either of the previous queues.  Initially all of the buffers
 *	are placed on the `freemap' queue.  Whenever a buffer is freed
 *	because of coallescing ends in rt_memfree() or zero size in rt_memalloc()
 *	the mapping buffer is taken off from the respective queue and
 *	returned to the `freemap' queue.
 *
 *  Authors -
 *	George E. Toth
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
/** @} */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"


/* XXX not PARALLEL */
/* Allocation/Free spaces */
static struct mem_map *rt_mem_freemap = MAP_NULL;	/* Freelist of buffers */

/* Flags used by `type' in rt_memfree() */
#define	M_TMTCH	00001	/* Top match */
#define	M_BMTCH	00002	/* Bottom match */
#define	M_TOVFL	00004	/* Top overflow */
#define	M_BOVFL	00010	/* Bottom overflow */

/*
 *			R T _ M E M A L L O C
 *
 *	Takes:		& pointer of map,
 *			size.
 *
 *	Returns:	NULL	Error
 *			<addr>	Othewise
 *
 *	Comments:
 *	Algorithm is first fit.
 */
unsigned long
rt_memalloc(struct mem_map **pp, register unsigned int size)
{
	register struct mem_map *prevp = MAP_NULL;
	register struct mem_map *curp;
	unsigned long	addr;

	if( size == 0 )
		return( 0L );	/* fail */

	for( curp = *pp; curp; curp = (prevp=curp)->m_nxtp )  {
		if( curp->m_size >= size )
			break;
	}

	if( curp == MAP_NULL )
		return(0L);		/* No more space */

	addr = curp->m_addr;
	curp->m_addr += size;

	/* If the element size goes to zero, put it on the freelist */

	if( (curp->m_size -= size) == 0 )  {
		if( prevp )
			prevp->m_nxtp = curp->m_nxtp;
		else
			*pp = curp->m_nxtp;	/* Click list down at start */
		curp->m_nxtp = rt_mem_freemap;		/* Link it in */
		rt_mem_freemap = curp;			/* Make it the start */
	}

	return( addr );
}

/*
 *			R T _ M E M A L L O C _ N O S P L I T
 *
 *	Takes:		& pointer of map,
 *			size.
 *
 *	Returns:	NULL	Error
 *			<addr>	Othewise
 *
 *	Comments:
 *	Algorithm is BEST fit.
 */
struct mem_map *
rt_memalloc_nosplit(struct mem_map **pp, register unsigned int size)
{
	register struct mem_map *prevp = MAP_NULL;
	register struct mem_map *curp;
	register struct mem_map *best = MAP_NULL, *best_prevp = MAP_NULL;

	if( size == 0 )
		return MAP_NULL;	/* fail */

	for( curp = *pp; curp; curp = (prevp=curp)->m_nxtp )  {
		if( curp->m_size < size )  continue;
		if( curp->m_size == size )  {
			best = curp;
			best_prevp = prevp;
			break;
		}
		/* This element has enough size */
		if( best == MAP_NULL || curp->m_size < best->m_size )  {
			best = curp;
			best_prevp = prevp;
		}
	}
	if( !best )
		return MAP_NULL;		/* No space */

	/* Move this element to free list, return it, unsplit */
	if( best_prevp )
		best_prevp->m_nxtp = best->m_nxtp;
	else
		*pp = best->m_nxtp;	/* Click list down at start */
	best->m_nxtp = rt_mem_freemap;		/* Link it in */
	rt_mem_freemap = best;			/* Make it the start */

	return best;
}

/*
 *			R T _ M E M G E T
 *
 *	Returns:	NULL	Error
 *			-1	Zero Request
 *			<addr>	Othewise
 *
 *	Comments:
 *	Algorithm is first fit.
 *	Free space can be split
 */
unsigned long
rt_memget(struct mem_map **pp, register unsigned int size, unsigned int place)
{
	register struct mem_map *prevp, *curp;
	unsigned int addr;

	prevp = MAP_NULL;		/* special for first pass through */
	if( size == 0 )
		rt_bomb("rt_memget() size==0\n");

	curp = *pp;
	while( curp )  {
		/*
		 * Assumption:  We will always be APPENDING to an existing
		 * memory allocation, so we search for a free piece of memory
		 * which begins at 'place', without worrying about ones which
		 * could begin earlier but be long enough to satisfy this
		 * request.
		 */
		if( curp->m_addr == place && curp->m_size >= size )
			break;
		curp = (prevp=curp)->m_nxtp;
	}

	if( curp == MAP_NULL )
		return(0L);		/* No space here */

	addr = curp->m_addr;
	curp->m_addr += size;

	/* If the element size goes to zero, put it on the freelist */
	if( (curp->m_size -= size) == 0 )  {
		if( prevp )
			prevp->m_nxtp = curp->m_nxtp;
		else
			*pp = curp->m_nxtp;	/* Click list down at start */
		curp->m_nxtp = rt_mem_freemap;		/* Link it in */
		rt_mem_freemap = curp;			/* Make it the start */
	}
	return( addr );
}

/*
 *			R T _ M E M G E T _ N O S P L I T
 *
 *	Returns:	0	Unable to satisfy request
 *			<size>	Actual size of free block, may be larger
 *				than requested size.
 *
 *
 *	Comments:
 *		Caller is responsible for returning unused portion.
 */
unsigned long
rt_memget_nosplit(struct mem_map **pp, register unsigned int size, unsigned int place)
{
	register struct mem_map *prevp, *curp;

	prevp = MAP_NULL;		/* special for first pass through */
	if( size == 0 )
		bu_bomb("rt_memget_nosplit() size==0\n");

	curp = *pp;
	while( curp )  {
		/*
		 * Assumption:  We will always be APPENDING to an existing
		 * memory allocation, so we search for a free piece of memory
		 * which begins at 'place', without worrying about ones which
		 * could begin earlier but be long enough to satisfy this
		 * request.
		 */
		if( curp->m_addr == place && curp->m_size >= size )  {
			size = curp->m_size;
			/* put this element on the freelist */
			if( prevp )
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

/*
 *			M E M F R E E
 *
 *	Takes:
 *			size,
 *			address.
 *
 *	Comments:
 *	The routine does not check for wrap around when increasing sizes
 *	or changing addresses.  Other wrap-around conditions are flagged.
 */
void
rt_memfree(struct mem_map **pp, unsigned int size, long unsigned int addr)
{
	register int type = 0;
	register struct mem_map *prevp = MAP_NULL;
	register struct mem_map *curp;
	long il;
	struct mem_map *tmap;

	if( size == 0 )
		return;		/* Nothing to free */

	/* Find the position in the list such that (prevp)<(addr)<(curp) */
	for( curp = *pp; curp; curp = (prevp=curp)->m_nxtp )
		if( addr < curp->m_addr )
			break;

	/* Make up the `type' variable */

	if( prevp )  {
		if( (il=prevp->m_addr+prevp->m_size) > addr )
			type |= M_BOVFL;
		if( il == addr )
			type |= M_BMTCH;
	}
	if( curp )  {
		if( (il=addr+size) > curp->m_addr )
			type |= M_TOVFL;
		if( il == curp->m_addr )
			type |= M_TMTCH;
	}

	if( type & (M_TOVFL|M_BOVFL) )  {
		bu_log("rt_memfree(addr=x%x,size=%d)  ERROR type=0%o\n",
			addr, size, type );
		if( prevp )
			bu_log("prevp: m_addr=x%x, m_size=%d\n",
				prevp->m_addr, prevp->m_size );
		if( curp )
			bu_log("curp: m_addr=x%x, m_size=%d\n",
				curp->m_addr, curp->m_size );
		return;
	}

	/*
 	 * Now we do the surgery:
	 * If there are no matches on boundaries we allocate a buffer
	 * If there is one match we expand the appropriate buffer
	 * If there are two matches we will have a free buffer returned.
	 */

	switch( type & (M_BMTCH|M_TMTCH) )  {
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
		curp->m_addr -= size;
		break;

	default:		/* No matches; allocate and insert */
		if( (tmap=rt_mem_freemap) == MAP_NULL )
			tmap = (struct mem_map *)bu_malloc(sizeof(struct mem_map), "struct mem_map " BU_FLSTR);
		else
			rt_mem_freemap = rt_mem_freemap->m_nxtp;	/* Click one off */

		if( prevp )
			prevp->m_nxtp = tmap;
		else
			*pp = tmap;

		tmap->m_size = size;
		tmap->m_addr = addr;
		tmap->m_nxtp = curp;
	}
}

/*
 *			M E M P U R G E
 *
 *  Take everything on the current memory chain, and place it on
 *  the freelist.
 */
void
rt_mempurge(struct mem_map **pp)
{
	register struct mem_map *prevp = MAP_NULL;
	register struct mem_map *curp;

	if( *pp == MAP_NULL )
		return;

	/* Find the end of the (busy) list */
	for( curp = *pp; curp; curp = (prevp=curp)->m_nxtp )
		;

	/* Put the whole busy list onto the free list */
	prevp->m_nxtp = rt_mem_freemap;
	rt_mem_freemap = *pp;

	*pp = MAP_NULL;
}

/*
 *			M E M P R I N T
 *
 *  Print a memory chain.
 */
void
rt_memprint(struct mem_map **pp)
{
	register struct mem_map *curp;

	bu_log("rt_memprint(x%x):  address, length\n", *pp);
	for( curp = *pp; curp; curp = curp->m_nxtp )
		bu_log(" a=x%.8lx, l=%.5d\n", curp->m_addr, curp->m_size );
}

/*
 *			M E M C L O S E
 *
 *  Return all the storage used by the rt_mem_freemap.
 */
void
rt_memclose(void)
{
	register struct mem_map *mp;

	while( (mp = rt_mem_freemap) != MAP_NULL )  {
		rt_mem_freemap = mp->m_nxtp;
		bu_free( (char *)mp, "struct mem_map " BU_FLSTR);
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
