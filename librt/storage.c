/*
 *			S T O R A G E . C
 *
 * Ray Tracing program, storage manager.
 *
 *  Functions -
 *	rt_malloc	Allocate storage, with visibility & checking
 *	rt_free		Similarly, free storage
 *	rt_realloc	Reallocate storage, with visibility & checking
 *	rt_calloc	Allocate zero'ed storage
 *	rt_prmem	When debugging, print memory map
 *	rt_strdup	Duplicate a string in dynamic memory
 *	rt_get_seg	Invoked by GET_SEG() macro
 *	rt_get_pt	Invoked by GET_PT() macro
 *	rt_byte_roundup	Optimize sizing of malloc() requests
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSstorage[] = "@(#)$Header$";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"

#ifdef BSD
# include <strings.h>
#else
# include <string.h>
#endif

/** #define MEMDEBUG 1 **/

#ifdef MEMDEBUG
#define MDB_SIZE	500
#define MDB_MAGIC	0x12348969
struct memdebug {
	char	*mdb_addr;
	char	*mdb_str;
	int	mdb_len;
} rt_mdb[MDB_SIZE];
#endif /* MEMDEBUG */

/*
 *			R T _ M A L L O C
 */
char *
rt_malloc(cnt, str)
unsigned int cnt;
char *str;
{
	register char *ptr;

#ifdef MEMDEBUG
	cnt = (cnt+2*sizeof(int)-1)&(~(sizeof(int)-1));
#endif /* MEMDEBUG */
	if( rt_g.rtg_parallel )  {
		RES_ACQUIRE( &rt_g.res_syscall );		/* lock */
	}
	ptr = malloc(cnt);
	if( rt_g.rtg_parallel ) {
		RES_RELEASE( &rt_g.res_syscall );		/* unlock */
	}

	if( ptr==(char *)0 || rt_g.debug&DEBUG_MEM )
		rt_log("%7x malloc%6d %s\n", ptr, cnt, str);
	if( ptr==(char *)0 )  {
		rt_log("rt_malloc: Insufficient memory available, sbrk(0)=x%x\n", sbrk(0));
		rt_bomb("rt_malloc: malloc failure");
	}
#ifdef MEMDEBUG
	{
		register struct memdebug *mp = rt_mdb;
		for( ; mp < &rt_mdb[MDB_SIZE]; mp++ )  {
			if( mp->mdb_len > 0 )  continue;
			mp->mdb_addr = ptr;
			mp->mdb_len = cnt;
			mp->mdb_str = str;
			goto ok;
		}
		rt_log("rt_malloc:  memdebug overflow\n");
	}
ok:	;
	{
		register int *ip = (int *)(ptr+cnt-sizeof(int));
		*ip = MDB_MAGIC;
	}
#endif /* MEMDEBUG */
	return(ptr);
}

/*
 *			R T _ F R E E
 */
void
rt_free(ptr,str)
char	*ptr;
char	*str;
{
#ifdef MEMDEBUG
	{
		register struct memdebug *mp = rt_mdb;
		for( ; mp < &rt_mdb[MDB_SIZE]; mp++ )  {
			if( mp->mdb_len <= 0 )  continue;
			if( mp->mdb_addr != ptr )  continue;
			{
				register int *ip = (int *)(ptr+mp->mdb_len-sizeof(int));
				if( *ip != MDB_MAGIC )  {
					rt_log("ERROR rt_free(x%x, %s) corrupted! x%x!=x%x\n", ptr, str, *ip, MDB_MAGIC);
					return;
				}
			}
			mp->mdb_len = 0;	/* successful free */
			goto ok;
		}
		rt_log("ERROR rt_free(x%x, %s) bad pointer!\n", ptr, str);
		return;
	}
ok:	;
#endif /* MEMDEBUG */

	if(rt_g.debug&DEBUG_MEM) rt_log("%7x free %s\n", ptr, str);
	if(ptr == (char *)0)  {
		rt_log("%7x free ERROR %s\n", ptr, str);
		return;
	}
	if( rt_g.rtg_parallel ) {
		RES_ACQUIRE( &rt_g.res_syscall );		/* lock */
	}
	*((int *)ptr) = -1;	/* zappo! */
	free(ptr);
	if( rt_g.rtg_parallel ) {
		RES_RELEASE( &rt_g.res_syscall );		/* unlock */
	}
}

/*
 *			R T _ R E A L L O C
 */
char *
rt_realloc(ptr, cnt, str)
register char *ptr;
unsigned int cnt;
char *str;
{
#ifdef MEMDEBUG
	register char *savedptr;

	savedptr = ptr;
	cnt = (cnt+2*sizeof(int)-1)&(~(sizeof(int)-1));
#endif /* MEMDEBUG */

	if( rt_g.rtg_parallel ) {
		RES_ACQUIRE( &rt_g.res_syscall );		/* lock */
	}
	ptr = realloc(ptr,cnt);
	if( rt_g.rtg_parallel ) {
		RES_RELEASE( &rt_g.res_syscall );		/* unlock */
	}

	if( ptr==(char *)0 || rt_g.debug&DEBUG_MEM )
		rt_log("%7x realloc%6d %s\n", ptr, cnt, str);
	if( ptr==(char *)0 )  {
		rt_log("rt_realloc: Insufficient memory available, using %d\n", sbrk(0));
		rt_bomb("rt_realloc: malloc failure");
	}
#ifdef MEMDEBUG
	if( ptr != savedptr )
	{
		/* replace old entry with new one */
		register struct memdebug *mp = rt_mdb;
		for( ; mp < &rt_mdb[MDB_SIZE]; mp++ )  {
			if( mp->mdb_len > 0 && (mp->mdb_addr == savedptr) ) {
				mp->mdb_addr = ptr;
				mp->mdb_len = cnt;
				mp->mdb_str = str;
				goto ok;
			}
		}
		rt_log("rt_realloc: old entry not found!\n");
	}
ok:	;
	{
		register int *ip = (int *)(ptr+cnt-sizeof(int));
		*ip = MDB_MAGIC;
	}
#endif /* MEMDEBUG */
	return(ptr);
}

/*
 *			R T _ C A L L O C
 */
char *
rt_calloc( nelem, elsize, str )
unsigned int	nelem;
unsigned int	elsize;
char		*str;
{
	unsigned	len;
	char		*ret;

	ret = rt_malloc( (len = nelem*elsize), str );
#ifdef SYSV
	(void)memset( ret, '\0', len );
#else
	bzero( ret, len );
#endif
	return(ret);
}

/*
 *			R T _ P R M E M
 * 
 *  Print map of memory currently in use.
 */
void
rt_prmem(str)
char *str;
{
#ifdef MEMDEBUG
	register struct memdebug *mp = rt_mdb;
	register int *ip;

	rt_log("\nRT memory use\t\t%s\n", str);
	for( ; mp < &rt_mdb[MDB_SIZE]; mp++ )  {
		if( mp->mdb_len <= 0 )  continue;
		ip = (int *)(mp->mdb_addr+mp->mdb_len-sizeof(int));
		rt_log("%7x %5x %s %s\n",
			mp->mdb_addr, mp->mdb_len, mp->mdb_str,
			*ip!=MDB_MAGIC ? "-BAD-" : "" );
		if( *ip != MDB_MAGIC )
			rt_log("\t%x\t%x\n", *ip, MDB_MAGIC);
	}
#endif /* MEMDEBUG */
}

/*
 *			R T _ S T R D U P
 *
 * Given a string, allocate enough memory to hold it using rt_malloc(),
 * duplicate the strings, returns a pointer to the new string.
 */
char *
rt_strdup( cp )
register char *cp;
{
	register char	*base;
	register int	len;

	if(rt_g.debug&DEBUG_MEM) rt_log("rt_strdup(%s) x%x\n", cp, cp);

	len = strlen( cp )+2;
	if( (base = rt_malloc( len, "rt_strdup" )) == (char *)0 )
		rt_bomb("rt_strdup:  unable to allocate memory");

#ifdef BSD
	bcopy( cp, base, len );
#else
	memcpy( base, cp, len );
#endif
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
rt_get_seg(res)
register struct resource *res;
{
	register char *cp;
	register struct seg *sp;
	register int bytes;

	bytes = rt_byte_roundup(64*sizeof(struct seg));
	if( (cp = rt_malloc(bytes, "rt_get_seg")) == (char *)0 )  {
		rt_bomb("rt_get_seg: malloc failure\n");
	}
	sp = (struct seg *)cp;
	while( bytes >= sizeof(struct seg) )  {
		sp->seg_next = res->re_seg;
		res->re_seg = sp++;
		res->re_seglen++;
		bytes -= sizeof(struct seg);
	}
}

/*
 *  			R T _ G E T _ P T
 *  
 *  This routine is called by the GET_PT macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.
 *
 *  Also note that there is a bit of trickery going on here:
 *  the *real* size of pt_solhit[] array is determined at runtime, here.
 *  XXX WARNING:  If multiple models are being used, we need to cope
 *  XXX with their differing size requirements, nworkers * N(rt_i)!
 */
void
rt_get_pt(rtip, res)
struct rt_i		*rtip;
register struct resource *res;
{
	register char *cp;
	register int bytes;
	register int size;		/* size of structure to really get */

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_get_pt:  bad rtip\n");

	size = rtip->rti_pt_bytes;
	size = (size + sizeof(bitv_t)-1) & (~(sizeof(bitv_t)-1));
	if( size <= 0 )
		rt_bomb("rt_get_pt: bad size");
	bytes = rt_byte_roundup(64*size);
	if( (cp = rt_malloc(bytes, "rt_get_pt")) == (char *)0 )
		rt_bomb("rt_get_pt: malloc failure\n");

	while( bytes >= size )  {
		((struct partition *)cp)->pt_forw = res->re_part;
		res->re_part = (struct partition *)cp;
		res->re_partlen++;
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

#ifdef SYSV
	return(nbytes);
#else
	if (pagesz == 0)
		pagesz = getpagesize();

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
#endif
}
