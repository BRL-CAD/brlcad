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
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSstorage[] = "@(#)$Header$";
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
#include "./debug.h"

#define MDB_MAGIC	0x12348969
struct memdebug {
	char		*mdb_addr;
	CONST char	*mdb_str;
	int		mdb_len;
};
static struct memdebug	*rt_memdebug;
static int		rt_memdebug_len = 0;
#define MEMDEBUG_NULL	((struct memdebug *)0)

/*
 *			R T _ M E M D E B U G _ A D D
 *
 *  Add another entry to the memory debug table
 */
HIDDEN void
rt_memdebug_add( ptr, cnt, str )
char		*ptr;
unsigned int	cnt;
CONST char	*str;
{
	register struct memdebug *mp;
top:
	if( rt_g.rtg_parallel )  {
		RES_ACQUIRE( &rt_g.res_syscall );		/* lock */
	}
	if( rt_memdebug )  {
		mp = &rt_memdebug[rt_memdebug_len-1];
		for( ; mp >= rt_memdebug; mp-- )  {
			/* Search for an empty slot */
			if( mp->mdb_len > 0 )  continue;
			mp->mdb_addr = ptr;
			mp->mdb_len = cnt;
			mp->mdb_str = str;
			if( rt_g.rtg_parallel ) {
				RES_RELEASE( &rt_g.res_syscall ); /* unlock */
			}
			return;
		}
	}

	/* Need to make more slots */
	if( rt_memdebug_len <= 0 )  {
		rt_memdebug_len = 510;
		rt_memdebug = (struct memdebug *)calloc(
			rt_memdebug_len, sizeof(struct memdebug) );
	} else {
		int	old_len = rt_memdebug_len;
		rt_memdebug_len *= 4;
		rt_memdebug = (struct memdebug *)realloc(
			(char *)rt_memdebug,
			sizeof(struct memdebug) * rt_memdebug_len );
		bzero( (char *)&rt_memdebug[old_len],
			(rt_memdebug_len-old_len) * sizeof(struct memdebug) );
	}
	if( rt_g.rtg_parallel ) {
		RES_RELEASE( &rt_g.res_syscall );		/* unlock */
	}
	if( rt_memdebug == (struct memdebug *)0 )
		rt_bomb("rt_memdebug_add() malloc failure\n");
	goto top;
}

/*
 *			R T _ M E M D E B U G _ C H E C K
 *
 *  Check an entry against the memory debug table, based upon it's address.
 */
HIDDEN struct memdebug *
rt_memdebug_check( ptr, str )
register char	*ptr;
CONST char	*str;
{
	register struct memdebug *mp = &rt_memdebug[rt_memdebug_len-1];
	register long	*ip;

	if( rt_memdebug == (struct memdebug *)0 )  {
		rt_log("rt_memdebug_check(x%x, %s)  no memdebug table yet\n",
			ptr, str);
		return MEMDEBUG_NULL;
	}
	for( ; mp >= rt_memdebug; mp-- )  {
		if( mp->mdb_len <= 0 )  continue;
		if( mp->mdb_addr != ptr )  continue;
		ip = (long *)(ptr+mp->mdb_len-sizeof(long));
		if( *ip != MDB_MAGIC )  {
			rt_log("ERROR rt_memdebug_check(x%x, %s) %s, barrier word corrupted!\nbarrier at x%x was=x%x s/b=x%x, len=%d\n",
				ptr, str, mp->mdb_str,
				ip, *ip, MDB_MAGIC, mp->mdb_len);
			rt_bomb("rt_memdebug_check() memory corruption\n");
		}
		return(mp);		/* OK */
	}
	return MEMDEBUG_NULL;
}

/*
 *			R T _ M E M _ B A R R I E R C H E C K
 *
 *  Check *all* entries in the memory debug table for barrier word
 *  corruption.
 *  Intended to be called periodicly through an application during debugging.
 *
 *  Returns -
 *	-1	something is wrong
 *	 0	all is OK;
 */
int
rt_mem_barriercheck()
{
	register struct memdebug *mp = &rt_memdebug[rt_memdebug_len-1];
	register long	*ip;

	if( rt_memdebug == (struct memdebug *)0 )  {
		rt_log("rt_mem_barriercheck()  no memdebug table yet\n");
		return 0;
	}
	for( ; mp >= rt_memdebug; mp-- )  {
		if( mp->mdb_len <= 0 )  continue;
		ip = (long *)(mp->mdb_addr+mp->mdb_len-sizeof(long));
		if( *ip != MDB_MAGIC )  {
			rt_log("ERROR rt_mem_barriercheck(x%x, len=%d) barrier word corrupted!\nbarrier at x%x was=x%x s/b=x%x\n",
				mp->mdb_addr, mp->mdb_len,
				ip, *ip, MDB_MAGIC);
			return -1;	/* FAIL */
		}
	}
	return 0;			/* OK */
}

/*
 *			R T _ M E M D E B U G _ M O V E
 *
 *  realloc() has moved to a new memory block.
 *  Update our notion as well.
 */
HIDDEN void
rt_memdebug_move( old_ptr, new_ptr, new_cnt, new_str )
char	*old_ptr;
char	*new_ptr;
int	new_cnt;
CONST char	*new_str;
{
	register struct memdebug *mp = &rt_memdebug[rt_memdebug_len-1];

	if( rt_memdebug == (struct memdebug *)0 )  {
		rt_log("rt_memdebug_move(x%x, x%x, %d., %s)  no memdebug table yet\n",
			old_ptr, new_ptr, new_cnt, new_str);
		return;
	}
	for( ; mp >= rt_memdebug; mp-- )  {
		if( mp->mdb_len > 0 && (mp->mdb_addr == old_ptr) ) {
			mp->mdb_addr = new_ptr;
			mp->mdb_len = new_cnt;
			mp->mdb_str = new_str;
			return;
		}
	}
	rt_log("rt_memdebug_move(): old memdebug entry not found!\n");
	rt_log(" old_ptr=x%x, new_ptr=x%x, new_cnt=%d., new_str=%s\n",
		old_ptr, new_ptr, new_cnt, new_str );
}

/*
 *			R T _ M A L L O C
 *
 *  This routine only returns on successful allocation.
 *  Failure results in rt_bomb() being called.
 */
char *
rt_malloc(cnt, str)
unsigned int	cnt;
CONST char	*str;
{
	register char *ptr;

	if( cnt == 0 )  {
		rt_log("ERROR: rt_malloc count=0 %s\n", str );
		rt_bomb("ERROR: rt_malloc(0)\n");
	}
	if( rt_g.debug&DEBUG_MEM_FULL )  {
		/* Pad, plus full int for magic number */
		cnt = (cnt+2*sizeof(long)-1)&(~(sizeof(long)-1));
	}
	if( rt_g.rtg_parallel )  {
		RES_ACQUIRE( &rt_g.res_syscall );		/* lock */
	}
	ptr = malloc(cnt);
	if( rt_g.rtg_parallel ) {
		RES_RELEASE( &rt_g.res_syscall );		/* unlock */
	}

	if( ptr==(char *)0 || rt_g.debug&DEBUG_MEM )
		rt_log("%8x malloc%6d %s\n", ptr, cnt, str);
	if( ptr==(char *)0 )  {
		rt_log("rt_malloc: Insufficient memory available, sbrk(0)=x%x\n", sbrk(0));
		rt_bomb("rt_malloc: malloc failure");
	}
	if( rt_g.debug&DEBUG_MEM_FULL )  {
		rt_memdebug_add( ptr, cnt, str );

		/* Install a barrier word at the end of the dynamic arena */
		/* Correct location depends on 'cnt' being rounded up, above */

		*((long *)(ptr+cnt-sizeof(long))) = MDB_MAGIC;
	}
	return(ptr);
}

/*
 *			R T _ F R E E
 */
void
rt_free(ptr,str)
char		*ptr;
CONST char	*str;
{
	if(rt_g.debug&DEBUG_MEM) rt_log("%8x free %s\n", ptr, str);
	if(ptr == (char *)0 || ptr == (char *)(-1L) )  {
		rt_log("%8x free ERROR %s\n", ptr, str);
		return;
	}
	if( rt_g.debug&DEBUG_MEM_FULL )  {
		struct memdebug	*mp;
		if( (mp = rt_memdebug_check( ptr, str )) == MEMDEBUG_NULL )  {
			rt_log("ERROR rt_free(x%x, %s) pointer bad, or not allocated with rt_malloc!  Ignored.\n",
				ptr, str);
			return;
		} else {
			mp->mdb_len = 0;	/* successful delete */
		}
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
register char	*ptr;
unsigned int	cnt;
CONST char	*str;
{
	char	*original_ptr = ptr;

	if( rt_g.debug&DEBUG_MEM_FULL )  {
		if( rt_memdebug_check( ptr, str ) == MEMDEBUG_NULL )  {
			rt_log("%8x realloc%6d %s ** barrier check failure\n",
				ptr, cnt, str );
		}
		/* Pad, plus full int for magic number */
		cnt = (cnt+2*sizeof(long)-1)&(~(sizeof(long)-1));
	}

	if( rt_g.rtg_parallel ) {
		RES_ACQUIRE( &rt_g.res_syscall );		/* lock */
	}
	ptr = realloc(ptr,cnt);
	if( rt_g.rtg_parallel ) {
		RES_RELEASE( &rt_g.res_syscall );		/* unlock */
	}

	if( ptr==(char *)0 || rt_g.debug&DEBUG_MEM )  {
		rt_log("%8x realloc%6d %s %s\n", ptr, cnt, str,
			ptr == original_ptr ? "[grew in place]" : "[moved]" );
	}
	if( ptr==(char *)0 )  {
		rt_log("rt_realloc: Insufficient memory available, sbrk(0)=x%x\n", sbrk(0));
		rt_bomb("rt_realloc: malloc failure");
	}
	if( rt_g.debug&DEBUG_MEM_FULL )  {
		/* Even if ptr didn't change, need to update cnt & barrier */
		rt_memdebug_move( original_ptr, ptr, cnt, str );

		/* Install a barrier word at the end of the dynamic arena */
		/* Correct location depends on 'cnt' being rounded up, above */
		*((long *)(ptr+cnt-sizeof(long))) = MDB_MAGIC;
	}
	return(ptr);
}

/*
 *			R T _ C A L L O C
 */
char *
rt_calloc( nelem, elsize, str )
unsigned int	nelem;
unsigned int	elsize;
CONST char	*str;
{
	unsigned	len;
	char		*ret;

	ret = rt_malloc( (len = nelem*elsize), str );
	bzero( ret, len );
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
	register struct memdebug *mp;
	register int *ip;

	rt_log("\nrt_prmem(): LIBRT memory use (%s)\n", str);
	if( (rt_g.debug&DEBUG_MEM_FULL) == 0 )  {
		rt_log("\tMemory debugging is now OFF\n");
	}
	rt_log("\t%d elements in memdebug table\n Address Length Purpose\n",
		rt_memdebug_len);
	if( rt_memdebug_len <= 0 )  return;

	mp = &rt_memdebug[rt_memdebug_len-1];
	for( ; mp >= rt_memdebug; mp-- )  {
		if( mp->mdb_len <= 0 )  continue;
		ip = (int *)(mp->mdb_addr+mp->mdb_len-sizeof(int));
		rt_log("%8x %6x %s %s\n",
			mp->mdb_addr, mp->mdb_len, mp->mdb_str,
			*ip!=MDB_MAGIC ? "-BAD-" : "" );
		if( *ip != MDB_MAGIC )
			rt_log("\t%x\t%x\n", *ip, MDB_MAGIC);
	}
}

/*
 *			R T _ S T R D U P
 *
 * Given a string, allocate enough memory to hold it using rt_malloc(),
 * duplicate the strings, returns a pointer to the new string.
 */
char *
rt_strdup( cp )
register CONST char *cp;
{
	register char	*base;
	register int	len;

	if(rt_g.debug&DEBUG_MEM) rt_log("rt_strdup(%s) x%x\n", cp, cp);

	len = strlen( cp )+2;
	if( (base = rt_malloc( len, "rt_strdup duplicate string" )) == (char *)0 )
		rt_bomb("rt_strdup:  unable to allocate memory");

	memcpy( base, cp, len );
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
register struct resource	*res;
{
	register struct seg	*sp;
	register int		bytes;

	RT_RESOURCE_CHECK(res);

	if( res->re_seg.l.forw == RT_LIST_NULL )  {
		RT_LIST_INIT( &(res->re_seg.l) );
	}
	bytes = rt_byte_roundup(64*sizeof(struct seg));
	sp = (struct seg *)rt_malloc(bytes, "rt_get_seg()");
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
 *  Each partition structure is separately allocated with rt_malloc(),
 *  so that it can be freed later.  Note that if the desired length
 *  for a new structure does not match the existing length of the first
 *  free structure on the free queue, this routine is also called.
 *  In this case, all wrong size structures are released, and then
 *  some new ones are obtained.
 *
 *  At some time in the future, it may be worth considering a more
 *  intelligent cache algorithm;  for now, let rt_malloc() handle it.
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
			rt_free( (char *)pp, "wrong size partition struct");
			res->re_partlen--;
			pp = nextpp;
			continue;
		}
		pp = pp->pt_forw;
	}

	/* Obtain a few new structures of the desired size */
	for( i=10; i>0; i-- )  {
		pp = (struct partition *)rt_malloc(bytes, "struct partition");
		pp->pt_len = bytes;
		pp->pt_magic = PT_MAGIC;
		FREE_PT(pp, res);
		res->re_partlen++;
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
#if !defined(HAVE_CALTECH_MALLOC)
	return(nbytes);
#else
	static int pagesz;
	register int n;
	register int amt;

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

/*	R T _ C K _ M A L L O C _ P T R
 *
 *	Check the magic number stored with memory allocated with rt_malloc
 *	when DEBUG_MEM_FULL is set.
 *
 *	return:
 *		0	pointer good or DEBUG_MEM_FULL not set
 *		other	memory corrupted.
 */
void
rt_ck_malloc_ptr( ptr, str )
char	*ptr;
char	*str;
{
	register struct memdebug *mp = &rt_memdebug[rt_memdebug_len-1];
	register long	*ip;


	/* if memory debugging isn't turned on, we have no way
	 * of knowing if the pointer is good or not
	 */
	if ((rt_g.debug&DEBUG_MEM_FULL) == 0) return;


	if (ptr == (char *)NULL) {
		rt_log("rt_ck_malloc_ptr(x%x, %s) null pointer\n\n", ptr, str);
		rt_bomb("Goodbye");
	}

	if( rt_memdebug == (struct memdebug *)0 )  {
		rt_log("rt_ck_malloc_ptr(x%x, %s)  no memdebug table yet\n",
			ptr, str);
		/* warning only -- the program is just getting started */
		return;
	}

	for( ; mp >= rt_memdebug; mp-- )  {
		if( mp->mdb_len <= 0 || mp->mdb_addr != ptr )  continue;

		ip = (long *)(ptr+mp->mdb_len-sizeof(long));
		if( *ip != MDB_MAGIC )  {
			rt_log("ERROR rt_ck_malloc_ptr(x%x, %s) barrier word corrupted! was=x%x s/b=x%x\n",
				ptr, str, *ip, MDB_MAGIC);
			rt_bomb("rt_ck_malloc_ptr\n");
		}
		return;		/* OK */
	}
	rt_log("ERROR rt_ck_malloc_ptr(x%x, %s)\n\
	pointer not in table of allocated memory.\n", ptr, str);

	rt_bomb("rt_ck_malloc_ptr\n");
}
