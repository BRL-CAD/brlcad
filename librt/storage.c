/*
 *			S T O R A G E . C
 *
 * Ray Tracing program, storage manager.
 *
 *  Functions -
 *	rt_get_seg	Invoked by GET_SEG() macro
 *	rt_get_pt	Invoked by GET_PT() macro
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

/*
 *  			R T _ G E T _ P T
 *  
 *  This routine is called by the GET_PT macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving subroutine call overhead.
 *
 *  Also note that there is a bit of trickery going on here:
 *  the *real* size of pt_solhit[] array is determined at runtime, here.
 *
 *  Each partition structure is separately allocated with bu_malloc(),
 *  so that it can be freed later.  Note that if the desired length
 *  for a new structure does not match the existing length of the first
 *  free structure on the free queue, this routine is also called.
 *  In this case, all wrong size structures are released, and then
 *  some new ones are obtained.
 *
 *  At some time in the future, it may be worth considering a more
 *  intelligent cache algorithm;  for now, let bu_malloc() handle it.
 */
void
rt_get_pt(rtip, res)
struct rt_i		*rtip;
register struct resource *res;
{
	register int			bytes;
	register struct partition	*pp;
	register int			i;

	RT_CHECK_RTI(rtip);
	RT_RESOURCE_CHECK(res);

	if( RT_LIST_FIRST(partition, &res->re_parthead) == PT_NULL )  {
		RT_LIST_INIT( &res->re_parthead );
		res->re_partlen = 0;
	}

	bytes = rtip->rti_pt_bytes;

	/* First, march through the free queue, discarding wrong sizes */
	pp = RT_LIST_FIRST( partition, &res->re_parthead );
	while( RT_LIST_NOT_HEAD( pp, &res->re_parthead ) )  {
		RT_CHECK_PT(pp);
		if( pp->pt_len != bytes )  {
			register struct partition	*nextpp;

			nextpp = pp->pt_forw;
			DEQUEUE_PT( pp );
			bu_free( (genptr_t)pp, "wrong size partition struct");
			res->re_partlen--;
			pp = nextpp;
			continue;
		}
		pp = pp->pt_forw;
	}

	/* Obtain a few new structures of the desired size */
	for( i=10; i>0; i-- )  {
		pp = (struct partition *)bu_malloc(bytes, "struct partition");
		pp->pt_len = bytes;
		pp->pt_magic = PT_MAGIC;
		FREE_PT(pp, res);
		res->re_partlen++;
	}
}
