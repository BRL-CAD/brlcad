/*                          L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup bu_list */
/*@{*/
/** @file ./libbu/list.c
 *
 * @brief Support routines for linked lists.
 *
 *  Generic bu_list routines.
 *
 *  @author	Michael John Muuss
 *  @author	Lee A. Butler
 *
 *  @par Source -
 *  @n	The U. S. Army Research Laboratory
 *  @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char libbu_list_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"


#include <stdio.h>
#include "machine.h"
#include "bu.h"

#include <assert.h>

/**
 *			B U _ L I S T _ N E W
 *
 *	Creates and initializes a bu_list head structure
 */
struct bu_list *
bu_list_new(void)
{
	struct bu_list *new;

	BU_GETSTRUCT( new, bu_list );
	BU_LIST_INIT( new );

	return( new );
}

/**
 *			B U _ L I S T _ P O P
 *
 *	Returns the results of BU_LIST_POP
 */
struct bu_list *
bu_list_pop( struct bu_list *hp )
{
	struct bu_list *p;

	BU_LIST_POP( bu_list, hp, p );

	return( p );
}

/**
 *			B U _ L I S T _ L E N
 *
 *  Returns the number of elements on a bu_list brand linked list.
 */
int
bu_list_len(register const struct bu_list *hd)
{
	register int			count = 0;
	register const struct bu_list	*ep;

	for( BU_LIST_FOR( ep, bu_list, hd ) )  {
		count++;
	}
	return count;
}

/**
 *			B U _ L I S T _ R E V E R S E
 *
 *	Reverses the order of elements in a bu_list linked list.
 */
void
bu_list_reverse(register struct bu_list *hd)
{
	struct bu_list tmp_hd;
	register struct bu_list *ep;

	BU_CK_LIST_HEAD( hd );

	BU_LIST_INIT( &tmp_hd );
	BU_LIST_INSERT_LIST( &tmp_hd, hd );

	while( BU_LIST_WHILE( ep, bu_list, &tmp_hd ) )  {
		BU_LIST_DEQUEUE( ep );
		BU_LIST_APPEND( hd, ep );
	}
}

/**
 *			B U _ L I S T _ F R E E
 *
 *  Given a list of structures allocated with bu_malloc() enrolled
 *  on a bu_list head, walk the list and free the structures.
 *  This routine can only be used when the structures have no interior
 *  pointers.
 */
void
bu_list_free(struct bu_list *hd)
{
	struct bu_list	*p;

	while( BU_LIST_WHILE( p, bu_list, hd ) )  {
		BU_LIST_DEQUEUE( p );
		bu_free( (genptr_t)p, "struct bu_list" );
	}
}

/**
 *			B U _ L I S T _ P A R A L L E L _ A P P E N D
 *
 *  Simple parallel-safe routine for appending a data structure to the end
 *  of a bu_list doubly-linked list.
 *  @par Issues:
 *  	Only one semaphore shared by all list heads.
 *  @n	No portable way to notify waiting thread(s) that are sleeping
 */
void
bu_list_parallel_append(struct bu_list *headp, struct bu_list *itemp)
{
	bu_semaphore_acquire(BU_SEM_LISTS);
	BU_LIST_INSERT( headp, itemp );		/* insert before head = append */
	bu_semaphore_release(BU_SEM_LISTS);
}

/**
 *			B U _ L I S T _ P A R A L L E L _ D E Q U E U E
 *
 *  Simple parallel-safe routine for dequeueing one data structure from
 *  the head of a bu_list doubly-linked list.
 *  If the list is empty, wait until some other thread puts something on
 *  the list.
 *
 *  @par Issues:
 *	No portable way to not spin and burn CPU time while waiting
 *  @n	for something to show up on the list.
 */
struct bu_list *
bu_list_parallel_dequeue(struct bu_list *headp)
{
	for(;;)  {
		register struct bu_list *p;

		bu_semaphore_acquire(BU_SEM_LISTS);
		p = BU_LIST_FIRST(bu_list, headp);
		if( BU_LIST_NOT_HEAD( p, headp ) )  {
			BU_LIST_DEQUEUE(p);
			bu_semaphore_release(BU_SEM_LISTS);
			return p;
		}
		bu_semaphore_release(BU_SEM_LISTS);

		/* List is empty, wait a moment and peek again */
#if (defined(sgi) && defined(mips)) || (defined(__sgi) && defined(__mips))
		sginap(1);
#endif
	}
	/* NOTREACHED */
}

/**
 *			B U _ C K _ L I S T
 *
 *  Generic bu_list doubly-linked list checker.
 */
void
bu_ck_list(const struct bu_list *hd, const char *str)
{
	register const struct bu_list	*cur;
	int	head_count = 0;

	cur = hd;
	do  {
		if( cur->magic == BU_LIST_HEAD_MAGIC )  head_count++;
		if( !cur->forw )  {
			bu_log("bu_ck_list(%s) cur=x%x, cur->forw=x%x, hd=x%x\n",
				str, cur, cur->forw, hd );
			bu_bomb("bu_ck_list() forw\n");
		}
		if( cur->forw->back != cur )  {
			bu_log("bu_ck_list(%s) cur=x%x, cur->forw=x%x, cur->forw->back=x%x, hd=x%x\n",
				str, cur, cur->forw, cur->forw->back, hd );
			bu_bomb("bu_ck_list() forw->back\n");
		}
		if( !cur->back )  {
			bu_log("bu_ck_list(%s) cur=x%x, cur->back=x%x, hd=x%x\n",
				str, cur, cur->back, hd );
			bu_bomb("bu_ck_list() back\n");
		}
		if( cur->back->forw != cur )  {
			bu_log("bu_ck_list(%s) cur=x%x, cur->back=x%x, cur->back->forw=x%x, hd=x%x\n",
				str, cur, cur->back, cur->back->forw, hd );
			bu_bomb("bu_ck_list() back->forw\n");
		}
		cur = cur->forw;
	} while( cur != hd );

	if( head_count != 1 )  {
		bu_log("bu_ck_list(%s) head_count = %d, hd=x%x\n", str, head_count, hd);
		bu_bomb("bu_ck_list() headless!\n");
	}
}

/**
 *			B U _ C K _ L I S T _ M A G I C
 *
 *  bu_list doubly-linked list checker which checks the magic number for
 *	all elements in the linked list
 */
void
bu_ck_list_magic(const struct bu_list *hd, const char *str, const long int magic)
{
	register const struct bu_list	*cur;
	int	head_count = 0;
	int	item = 0;

	cur = hd;
	do  {
		if( cur->magic == BU_LIST_HEAD_MAGIC )  {
			head_count++;
		} else if( cur->magic != magic ) {
			bu_log("bu_ck_list(%s) cur magic=(%s)x%x, cur->forw magic=(%s)x%x, hd magic=(%s)x%x, item=%d\n",
				str, bu_identify_magic(cur->magic), cur->magic,
				bu_identify_magic(cur->forw->magic), cur->forw->magic,
				bu_identify_magic(hd->magic), hd->magic,
				item);
			bu_bomb("bu_ck_list_magic() cur->magic\n");
		}

		if( !cur->forw )  {
			bu_log("bu_ck_list_magic(%s) cur=x%x, cur->forw=x%x, hd=x%x, item=%d\n",
				str, cur, cur->forw, hd, item );
			bu_bomb("bu_ck_list_magic() forw NULL\n");
		}
		if( cur->forw->back != cur )  {
			bu_log("bu_ck_list_magic(%s) cur=x%x, cur->forw=x%x, cur->forw->back=x%x, hd=x%x, item=%d\n",
				str, cur, cur->forw, cur->forw->back, hd, item );
			bu_log(" cur=%s, cur->forw=%s, cur->forw->back=%s\n",
				bu_identify_magic(cur->magic),
				bu_identify_magic(cur->forw->magic),
				bu_identify_magic(cur->forw->back->magic) );
			bu_bomb("bu_ck_list_magic() cur->forw->back != cur\n");
		}
		if( !cur->back )  {
			bu_log("bu_ck_list_magic(%s) cur=x%x, cur->back=x%x, hd=x%x, item=%d\n",
				str, cur, cur->back, hd, item );
			bu_bomb("bu_ck_list_magic() back NULL\n");
		}
		if( cur->back->forw != cur )  {
			bu_log("bu_ck_list_magic(%s) cur=x%x, cur->back=x%x, cur->back->forw=x%x, hd=x%x, item=%d\n",
				str, cur, cur->back, cur->back->forw, hd, item );
			bu_bomb("bu_ck_list_magic() cur->back->forw != cur\n");
		}
		cur = cur->forw;
		item++;
	} while( cur != hd );

	if( head_count != 1 )  {
		bu_log("bu_ck_list_magic(%s) head_count = %d, hd=x%x, items=%d\n", str, head_count, hd, item);
		bu_bomb("bu_ck_list_magic() headless!\n");
	}
}

/* XXX - apparently needed by muves */
struct bu_list *
bu_list_dequeue_next( struct bu_list *hp, struct bu_list *p )
{
	struct bu_list *p2;

	hp = hp;
	p2 = BU_LIST_NEXT( bu_list, p );
	BU_LIST_DEQUEUE( p2 );

	return( p2 );
}

/*@}*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
