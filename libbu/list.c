/*
 *			L I S T . C
 *
 *  Generic bu_list routines
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char libbu_list_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#include "machine.h"
#include "bu.h"

/*
 *			B U _ L I S T _ L E N
 *
 *  Returns the number of elements on a bu_list brand linked list.
 */
int
bu_list_len( hd )
register CONST struct bu_list	*hd;
{
	register int			count = 0;
	register CONST struct bu_list	*ep;

	for( BU_LIST_FOR( ep, bu_list, hd ) )  {
		count++;
	}
	return count;
}

/*
 *			B U _ L I S T _ R E V E R S E
 *
 *	Reverses the order of elements in a bu_list linked list.
 */
void
bu_list_reverse( hd )
register struct bu_list   *hd;
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

/*
 *			B U _ L I S T _ F R E E
 *
 *  Given a list of structures allocated with bu_malloc() enrolled
 *  on a bu_list head, walk the list and free the structures.
 *  This routine can only be used when the structures have no interior
 *  pointers.
 */
void
bu_list_free(hd)
struct bu_list	*hd;
{
	struct bu_list	*p;

	while( BU_LIST_WHILE( p, bu_list, hd ) )  {
		BU_LIST_DEQUEUE( p );
		bu_free( (genptr_t)p, "struct bu_list" );
	}
}

/*
 *			B U _ L I S T _ P A R A L L E L _ A P P E N D
 *
 *  Simple parallel-safe routine for appending a data structure to the end
 *  of a bu_list doubly-linked list.
 *  Issues:
 *	Only one semaphore shared by all list heads.
 *	No portable way to notify waiting thread(s) that are sleeping
 */
void
bu_list_parallel_append( headp, itemp )
struct bu_list	*headp;
struct bu_list	*itemp;
{
	bu_semaphore_acquire(BU_SEM_LISTS);
	BU_LIST_INSERT( headp, itemp );		/* insert before head = append */
	bu_semaphore_release(BU_SEM_LISTS);
}

/*
 *			B U _ L I S T _ P A R A L L E L _ D E Q U E U E
 *
 *  Simple parallel-safe routine for dequeueing one data structure from
 *  the head of a bu_list doubly-linked list.
 *  If the list is empty, wait until some other thread puts something on
 *  the list.
 *
 *  Issues:
 *	No portable way to not spin and burn CPU time while waiting
 *	for something to show up on the list.
 */
struct bu_list *
bu_list_parallel_dequeue( headp )
struct bu_list	*headp;
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

/*
 *			B U _ C K _ L I S T
 *
 *  Generic bu_list doubly-linked list checker.
 */
void
bu_ck_list( hd, str )
CONST struct bu_list	*hd;
CONST char		*str;
{
	register CONST struct bu_list	*cur;
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

/*
 *			B U _ C K _ L I S T _ M A G I C
 *
 *  bu_list doubly-linked list checker which checks the magic number for
 *	all elements in the linked list
 */
void
bu_ck_list_magic( hd, str, magic )
CONST struct bu_list	*hd;
CONST char		*str;
CONST long		magic;
{
	register CONST struct bu_list	*cur;
	int	head_count = 0;

	cur = hd;
	do  {
		if( cur->magic == BU_LIST_HEAD_MAGIC )  {
			head_count++;
		} else if( cur->magic != magic ) {
			bu_log("nmg_ck_list(%s) cur magic=(%s)x%x, cur->forw magic=(%s)x%x, hd magic=(%s)x%x\n",
				str, bu_identify_magic(cur->magic), cur->magic,
				bu_identify_magic(cur->forw->magic), cur->forw->magic,
				bu_identify_magic(hd->magic), hd->magic);
			bu_bomb("nmg_ck_list_magic() cur->magic\n");
		}

		if( !cur->forw )  {
			bu_log("nmg_ck_list_magic(%s) cur=x%x, cur->forw=x%x, hd=x%x\n",
				str, cur, cur->forw, hd );
			bu_bomb("nmg_ck_list_magic() forw\n");
		}
		if( cur->forw->back != cur )  {
			bu_log("nmg_ck_list_magic(%s) cur=x%x, cur->forw=x%x, cur->forw->back=x%x, hd=x%x\n",
				str, cur, cur->forw, cur->forw->back, hd );
			bu_log(" cur=%s, cur->forw=%s, cur->forw->back=%s\n",
				bu_identify_magic(cur->magic),
				bu_identify_magic(cur->forw->magic),
				bu_identify_magic(cur->forw->back->magic) );
			bu_bomb("nmg_ck_list_magic() forw->back\n");
		}
		if( !cur->back )  {
			bu_log("nmg_ck_list_magic(%s) cur=x%x, cur->back=x%x, hd=x%x\n",
				str, cur, cur->back, hd );
			bu_bomb("nmg_ck_list_magic() back\n");
		}
		if( cur->back->forw != cur )  {
			bu_log("nmg_ck_list_magic(%s) cur=x%x, cur->back=x%x, cur->back->forw=x%x, hd=x%x\n",
				str, cur, cur->back, cur->back->forw, hd );
			bu_bomb("nmg_ck_list_magic() back->forw\n");
		}
		cur = cur->forw;
	} while( cur != hd );

	if( head_count != 1 )  {
		bu_log("nmg_ck_list_magic(%s) head_count = %d, hd=x%x\n", str, head_count, hd);
		bu_bomb("nmg_ck_list_magic() headless!\n");
	}
}
