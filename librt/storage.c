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
static char RCSstorage[] = "@(#)$Header$";
#endif

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/raytrace.h"
#include "debug.h"


/*
 *			R T _ M A L L O C
 */
char *
rt_malloc(cnt, str)
unsigned int cnt;
char *str;
{
	register char *ptr;
	extern char *malloc();

	RES_ACQUIRE( &rt_g.res_malloc );		/* lock */
	ptr = malloc(cnt);
	RES_RELEASE( &rt_g.res_malloc );		/* unlock */

	if( ptr==(char *)0 || rt_g.debug&DEBUG_MEM )
		rt_log("%x=malloc(%d) %s\n", ptr, cnt, str);
	if( ptr==(char *)0 )
		rt_bomb("rt_malloc: malloc failure");
	return(ptr);
}

/*
 *			R T _ F R E E
 */
void
rt_free(ptr,str)
char *ptr;
{
	RES_ACQUIRE( &rt_g.res_malloc );		/* lock */
	*((int *)ptr) = -1;	/* zappo! */
	free(ptr);
	RES_RELEASE( &rt_g.res_malloc );		/* unlock */
	if(rt_g.debug&DEBUG_MEM) rt_log("%x freed %s\n", ptr, str);
}

/*
 *			R T _ S T R D U P
 *
 * Given a string, allocate enough memory to hold it using malloc(),
 * duplicate the strings, returns a pointer to the new string.
 */
char *
rt_strdup( cp )
register char *cp;
{
	register char	*base;
	register char	*current;

	RES_ACQUIRE( &rt_g.res_malloc );		/* lock */
	if( (base = malloc( strlen(cp)+1 )) == (char *)0 )
		rt_bomb("rt_strdup:  unable to allocate memory");
	RES_RELEASE( &rt_g.res_malloc );		/* unlock */

	current = base;
	do  {
		*current++ = *cp;
	}  while( *cp++ != '\0' );

	return(base);
}

/*
 *  			R T _ G E T _ S E G
 *  
 *  This routine is called by the GET_SEG macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.  When this routine is called,
 *  the seg resource must already be locked.
 *  malloc() locking is done in rt_malloc.
 */
void
rt_get_seg()  {
	register char *cp;
	register struct seg *sp;
	register int bytes;

	bytes = rt_byte_roundup(64*sizeof(struct seg));
	if( (cp = rt_malloc(bytes, "rt_get_seg")) == (char *)0 )  {
		rt_log("rt_get_seg: malloc failure\n");
		exit(17);
	}
	sp = (struct seg *)cp;
	while( bytes >= sizeof(struct seg) )  {
		sp->seg_next = rt_g.FreeSeg;
		rt_g.FreeSeg = sp++;
		bytes -= sizeof(struct seg);
	}
}

/*
 *  			R T _ G E T _ P T
 *  
 *  This routine is called by the GET_PT macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.  When this routine is called,
 *  the partition resource must already be locked.
 *  malloc() locking is done in rt_malloc.
 *
 *  Also note that there is a bit of trickery going on here:
 *  the *real* size of pt_solhit[] array is determined at runtime, here.
 */
void
rt_get_pt()  {
	register char *cp;
	register int bytes;
	register int size;		/* size of structure to really get */

	size = PT_BYTES;		/* depends on rt_i.nsolids */
	size = (size + sizeof(long) -1) & (~(sizeof(long)-1));
	bytes = rt_byte_roundup(64*size);
	if( (cp = rt_malloc(bytes, "rt_get_pt")) == (char *)0 )  {
		rt_log("rt_get_pt: malloc failure\n");
		exit(17);
	}
	while( bytes >= size )  {
		((struct partition *)cp)->pt_forw = rt_g.FreePart;
		rt_g.FreePart = (struct partition *)cp;
		cp += size;
		bytes -= size;
	}
}

/*
 *  			R T _ B Y T E _ R O U N D U P
 *  
 *  On systems with the CalTech malloc(), the amount of storage
 *  ACTUALLY ALLOCATED is the amount requested rounded UP to the
 *  nearest power of two.  For structures which are acquired and
 *  released often, this works well, but for structures which will
 *  remain unchanged for the duration of the program, this wastes
 *  as much as 50% of the address space (and usually memory as well).
 *  Here, we round up a byte size to the nearest power of two,
 *  leaving off the malloc header, so as to ask for storage without
 *  wasting any.
 *  
 *  On systems with the traditional malloc(), this strategy will just
 *  consume the memory in somewhat larger chunks, but overall little
 *  unused memory will be consumed.
 */
int
rt_byte_roundup(nbytes)
register int nbytes;
{
	static int pagesz;
	register int n;
	register int amt;

	if (pagesz == 0)
		pagesz = 1024;		/* getpagesize(); */

#define OVERHEAD	(4*sizeof(unsigned char) + \
			2*sizeof(unsigned short) + \
			sizeof(unsigned int) )
	n = pagesz - OVERHEAD;
	if (nbytes <= n)
		return(n);
	amt = pagesz;

	while (nbytes > amt + n) {
		amt <<= 1;
	}
	return(amt-OVERHEAD-sizeof(int));
}
