/*
 *			H T B L . C
 *
 *  Support for variable length arrays of "struct hit".
 *  Patterned after the libbu/ptbl.c idea.
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
static const char librt_htbl_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#include <string.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"

/*
 *			R T _ H T B L _ I N I T
 */
void
rt_htbl_init(struct rt_htbl *b, int len, const char *str)
              	   
   		    		/* initial len. */
          	     
{
	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("rt_htbl_init(%8x, len=%d, %s)\n", b, len, str);
	BU_LIST_INIT(&b->l);
	b->l.magic = RT_HTBL_MAGIC;
	if( len <= 0 )  len = 64;
	b->blen = len;
	b->hits = (struct hit *)bu_calloc(b->blen, sizeof(struct hit), str);
	b->end = 0;
}

/*
 *			R T _ H T B L _ R E S E T
 *
 *  Reset the table to have no elements, but retain any existing storage.
 */
void
rt_htbl_reset(struct rt_htbl *b)
{
	RT_CK_HTBL(b);
	b->end = 0;
	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("rt_htbl_reset(%8x)\n", b);
}

/*
 *			R T _ H T B L _ F R E E
 *
 *  Deallocate dynamic hit buffer
 *  and render unusable without a subsequent rt_htbl_init().
 */
void
rt_htbl_free(struct rt_htbl *b)
{
	RT_CK_HTBL(b);

	bu_free((genptr_t)b->hits, "rt_htbl.hits[]");
	bzero((char *)b, sizeof(struct rt_htbl));	/* sanity */

	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("rt_htbl_free(%8x)\n", b);
}

/*
 *			R T _ H T B L _ G E T
 *
 *  Allocate another hit structure, extending the array if necessary.
 */
struct hit *
rt_htbl_get(struct rt_htbl *b)
{
	RT_CK_HTBL(b);

	if( b->end >= b->blen )  {
		/* Increase size of array */
		b->hits = (struct hit *)bu_realloc( (char *)b->hits,
			sizeof(struct hit) * (b->blen *= 4),
			"rt_htbl.hits[]" );
	}

	/* There is sufficient room */
	return &b->hits[b->end++];
}
