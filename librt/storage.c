/*
 *			S T O R A G E . C
 *
 * Ray Tracing program, storage manager.
 *
 *  Functions -
 *	rt_get_seg	Invoked by GET_SEG() macro
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSstorage[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "bu.h"
#include "./debug.h"

/*
 *  			R T _ G E T _ S E G
 *  
 *  This routine is called by the GET_SEG macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.  When this routine is called,
 *  the seg resource must already be locked.
 *  malloc() locking is done in bu_malloc.
 */
void
rt_get_seg(res)
register struct resource	*res;
{
	register struct seg	*sp;
	register int		bytes;

	RT_RESOURCE_CHECK(res);

	if( res->re_seg.l.forw == RT_LIST_NULL )  {
		RT_LIST_INIT( &(res->re_seg.l) );
	}
	bytes = bu_malloc_len_roundup(64*sizeof(struct seg));
	sp = (struct seg *)bu_malloc(bytes, "rt_get_seg()");
	while( bytes >= sizeof(struct seg) )  {
		sp->l.magic = RT_SEG_MAGIC;
		RT_LIST_INSERT(&(res->re_seg.l), &(sp->l));
		res->re_seglen++;
		sp++;
		bytes -= sizeof(struct seg);
	}
}
