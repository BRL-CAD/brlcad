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
static char RCSid[] = "@(#)$Header$ (ARL)";
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
