/*
 *			S T O R A G E . C
 *
 * Ray Tracing program, storage manager.
 *
 *  Functions -
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "raytrace.h"
#include "debug.h"


/*
 *  malloc/Free wrappers, for debugging
 */
char *
vmalloc(cnt, str)
unsigned int cnt;
char *str;
{
	register char *ptr;
	extern char *malloc();

	RES_ACQUIRE( &res_malloc );		/* lock */
	ptr = malloc(cnt);
	RES_RELEASE( &res_malloc );		/* unlock */

	if( ptr==(char *)0 || debug&DEBUG_MEM )
		fprintf(stderr,"%x=malloc(%d) %s\n", ptr, cnt, str);
	return(ptr);
}

void
vfree(ptr,str)
char *ptr;
{
	extern void free();

	RES_ACQUIRE( &res_malloc );		/* lock */
	free(ptr);
	RES_RELEASE( &res_malloc );		/* unlock */
	if(debug&DEBUG_MEM) fprintf(stderr,"%x freed %s\n", ptr, str);
	*((int *)ptr) = -1;	/* zappo! */
}

/*
 *  			G E T _ S E G
 *  
 *  This routine is called by the GET_SEG macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.  When this routine is called,
 *  the seg resource must already be locked.
 *  malloc() locking is done in vmalloc.
 */
void
get_seg()  {
	register char *cp;
	register struct seg *sp;
	register int bytes;

	bytes = 64*sizeof(struct seg);
	if( (cp = vmalloc(bytes, "get_seg")) == (char *)0 )  {
		fprintf(stderr,"get_seg: malloc failure\n");
		exit(17);
	}
	sp = (struct seg *)cp;
	while( bytes >= sizeof(struct seg) )  {
		sp->seg_next = FreeSeg;
		FreeSeg = sp++;
		bytes -= sizeof(struct seg);
	}
}

/*
 *  			G E T _ P T
 *  
 *  This routine is called by the GET_PT macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.  When this routine is called,
 *  the partition resource must already be locked.
 *  malloc() locking is done in vmalloc.
 */
void
get_pt()  {
	register char *cp;
	register struct partition *pp;
	register int bytes;

	bytes = 64*sizeof(struct partition);
	if( (cp = vmalloc(bytes, "get_pt")) == (char *)0 )  {
		fprintf(stderr,"get_pt: malloc failure\n");
		exit(17);
	}
	pp = (struct partition *)cp;
	while( bytes >= sizeof(struct partition) )  {
		pp->pt_forw = FreePart;
		FreePart = pp++;
		bytes -= sizeof(struct partition);
	}
}
