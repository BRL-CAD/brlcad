/*                      M E M A L L O C . C
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file memalloc.c
 *
 * Functions -
 *	memalloc	allocate 'size' of memory from a given map
 *	memget		allocate 'size' of memory from map at 'place'
 *	memfree		return 'size' of memory to map at 'place'
 *	mempurge	free everything on current memory chain
 *	memprint	print a map
 *
 * The structure of the displaylist memory map chains
 * consists of non-zero count and base address of that many contiguous units.
 * The addresses are increasing and the list is terminated with the
 * first zero link.
 *
 * memalloc() and memfree() use these tables to allocate displaylist memory.
 *
 *	For each Memory Map there exists a queue (coremap).
 *	There also exists a queue of free buffers which are enqueued
 *	on to either of the previous queues.  Initially all of the buffers
 *	are placed on the `freemap' queue.  Whenever a buffer is freed
 *	because of coallescing ends in memfree() or zero size in memalloc()
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
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "./mged_dm.h"		/* for struct mem_map */

extern char	*malloc();

/* Allocation/Free spaces */
static struct mem_map *freemap = MAP_NULL;	/* Freelist of buffers */

/* Flags used by `type' in memfree() */
#define	M_TMTCH	00001	/* Top match */
#define	M_BMTCH	00002	/* Bottom match */
#define	M_TOVFL	00004	/* Top overflow */
#define	M_BOVFL	00010	/* Bottom overflow */

/*
 *			M E M A L L O C
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
memalloc( pp, size )
struct mem_map **pp;
register unsigned size;
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

	if( curp == MAP_NULL )  {
		return(0L);		/* No more space */
	}

	addr = curp->m_addr;
	curp->m_addr += size;

	/* If the element size goes to zero, put it on the freelist */

	if( (curp->m_size -= size) == 0 )  {
		if( prevp )
			prevp->m_nxtp = curp->m_nxtp;
		else
			*pp = curp->m_nxtp;	/* Click list down at start */
		curp->m_nxtp = freemap;		/* Link it in */
		freemap = curp;			/* Make it the start */
	}

	return( addr );
}

/*
 *			M E M G E T
 *
 *	Returns:	NULL	Error
 *			-1	Zero Request
 *			<addr>	Othewise
 *
 *	Comments:
 *	Algorithm is first fit.
 */
unsigned long
memget( pp, size, place )
struct mem_map **pp;
register unsigned int size;
unsigned int place;
{
	register struct mem_map *prevp, *curp;
	unsigned int addr;

	prevp = MAP_NULL;		/* special for first pass through */
	if( size == 0 )
		return( -1 );	/* Anything non-zero */

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
		curp->m_nxtp = freemap;		/* Link it in */
		freemap = curp;			/* Make it the start */
	}
	return( addr );
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
memfree( pp, size, addr )
struct mem_map **pp;
unsigned size;
unsigned long addr;
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
		(void)printf("mfree(addr=%d,size=%d)  error type=0%o\n",
			addr, size, type );
		if( prevp )
			(void)printf("prevp: m_addr=%d, m_size=%d\n",
				prevp->m_addr, prevp->m_size );
		if( curp )
			(void)printf("curp: m_addr=%d, m_size=%d\n",
				curp->m_addr, curp->m_size );
		(void)printf("display memory dropped, continuing\n");
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
		curp->m_nxtp = freemap;		/* Link into freemap */
		freemap = curp;
		break;

	case M_BMTCH:		/* Expand bottom element */
		prevp->m_size += size;
		break;

	case M_TMTCH:		/* Expand top element downward */
		curp->m_size += size;
		curp->m_addr -= size;
		break;

	default:		/* No matches; allocate and insert */
		if( (tmap=freemap) == MAP_NULL )
			tmap = (struct mem_map *)malloc(sizeof(struct mem_map));
		else
			freemap = freemap->m_nxtp;	/* Click one off */

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
mempurge( pp )
struct mem_map **pp;
{
	register struct mem_map *prevp = MAP_NULL;
	register struct mem_map *curp;

	if( *pp == MAP_NULL )
		return;

	/* Find the end of the (busy) list */
	for( curp = *pp; curp; curp = (prevp=curp)->m_nxtp )
		;

	/* Put the whole busy list onto the free list */
	prevp->m_nxtp = freemap;
	freemap = *pp;

	*pp = MAP_NULL;
}

/*
 *			M E M P R I N T
 *
 *  Print a memory chain.
 */
void
memprint( pp )
struct mem_map **pp;
{
	register struct mem_map *curp;

	curp = *pp;
	for( curp = *pp; curp; curp = curp->m_nxtp )
		(void)printf(" %ld, len=%d\n", curp->m_addr, curp->m_size );
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
