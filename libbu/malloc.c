/*
 *			M A L L O C . C
 *
 * Parallel-protected debugging-enhanced wrapper around system malloc().
 *
 *  Functions -
 *	bu_malloc	Allocate storage, with visibility & checking
 *	bu_free		Similarly, free storage
 *	bu_realloc	Reallocate storage, with visibility & checking
 *	bu_calloc	Allocate zero'ed storage
 *	bu_prmem	When debugging, print memory map
 *	bu_strdup	Duplicate a string in dynamic memory
 *	bu_malloc_len_roundup	Optimize sizing of malloc() requests
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
static char RCSmalloc[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"

int	bu_debug = 0;

#define MDB_MAGIC	0x12348969
struct memdebug {
	genptr_t	mdb_addr;
	CONST char	*mdb_str;
	int		mdb_len;
};
static struct memdebug	*bu_memdebug;
static int		bu_memdebug_len = 0;
#define MEMDEBUG_NULL	((struct memdebug *)0)

/*
 *			B U _ M E M D E B U G _ A D D
 *
 *  Add another entry to the memory debug table
 */
HIDDEN void
bu_memdebug_add( ptr, cnt, str )
char		*ptr;
unsigned int	cnt;
CONST char	*str;
{
	register struct memdebug *mp;
top:
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	if( bu_memdebug )  {
		mp = &bu_memdebug[bu_memdebug_len-1];
		for( ; mp >= bu_memdebug; mp-- )  {
			/* Search for an empty slot */
			if( mp->mdb_len > 0 )  continue;
			mp->mdb_addr = ptr;
			mp->mdb_len = cnt;
			mp->mdb_str = str;
			bu_semaphore_release( BU_SEM_SYSCALL );
			return;
		}
	}

	/* Need to make more slots */
	if( bu_memdebug_len <= 0 )  {
		bu_memdebug_len = 510;
		bu_memdebug = (struct memdebug *)calloc(
			bu_memdebug_len, sizeof(struct memdebug) );
	} else {
		int	old_len = bu_memdebug_len;
		bu_memdebug_len *= 16;
		bu_memdebug = (struct memdebug *)realloc(
			(char *)bu_memdebug,
			sizeof(struct memdebug) * bu_memdebug_len );
		bzero( (char *)&bu_memdebug[old_len],
			(bu_memdebug_len-old_len) * sizeof(struct memdebug) );
	}
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( bu_memdebug == (struct memdebug *)0 )
		bu_bomb("bu_memdebug_add() malloc failure\n");
	goto top;
}

/*
 *			B U _ M E M D E B U G _ C H E C K
 *
 *  Check an entry against the memory debug table, based upon it's address.
 */
HIDDEN struct memdebug *
bu_memdebug_check( ptr, str )
register char	*ptr;
CONST char	*str;
{
	register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
	register long	*ip;

	if( bu_memdebug == (struct memdebug *)0 )  {
		bu_log("bu_memdebug_check(x%x, %s)  no memdebug table yet\n",
			ptr, str);
		return MEMDEBUG_NULL;
	}
	for( ; mp >= bu_memdebug; mp-- )  {
		if( mp->mdb_len <= 0 )  continue;
		if( mp->mdb_addr != ptr )  continue;
		ip = (long *)(ptr+mp->mdb_len-sizeof(long));
		if( *ip != MDB_MAGIC )  {
			bu_log("ERROR bu_memdebug_check(x%x, %s) %s, barrier word corrupted!\nbarrier at x%x was=x%x s/b=x%x, len=%d\n",
				ptr, str, mp->mdb_str,
				ip, *ip, MDB_MAGIC, mp->mdb_len);
			bu_bomb("bu_memdebug_check() memory corruption\n");
		}
		return(mp);		/* OK */
	}
	return MEMDEBUG_NULL;
}

/*
 *			B U _ M E M D E B U G _ M O V E
 *
 *  realloc() has moved to a new memory block.
 *  Update our notion as well.
 */
HIDDEN void
bu_memdebug_move( old_ptr, new_ptr, new_cnt, new_str )
char	*old_ptr;
char	*new_ptr;
int	new_cnt;
CONST char	*new_str;
{
	register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];

	if( bu_memdebug == (struct memdebug *)0 )  {
		bu_log("bu_memdebug_move(x%x, x%x, %d., %s)  no memdebug table yet\n",
			old_ptr, new_ptr, new_cnt, new_str);
		return;
	}
	for( ; mp >= bu_memdebug; mp-- )  {
		if( mp->mdb_len > 0 && (mp->mdb_addr == old_ptr) ) {
			mp->mdb_addr = new_ptr;
			mp->mdb_len = new_cnt;
			mp->mdb_str = new_str;
			return;
		}
	}
	bu_log("bu_memdebug_move(): old memdebug entry not found!\n");
	bu_log(" old_ptr=x%x, new_ptr=x%x, new_cnt=%d., new_str=%s\n",
		old_ptr, new_ptr, new_cnt, new_str );
}

/*
 *			B U _ M A L L O C
 *
 *  This routine only returns on successful allocation.
 *  Failure results in bu_bomb() being called.
 */
genptr_t
bu_malloc(cnt, str)
unsigned int	cnt;
CONST char	*str;
{
	register genptr_t ptr;

	if( cnt == 0 )  {
		bu_log("ERROR: bu_malloc count=0 %s\n", str );
		bu_bomb("ERROR: bu_malloc(0)\n");
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		/* Pad, plus full int for magic number */
		cnt = (cnt+2*sizeof(long)-1)&(~(sizeof(long)-1));
	}

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	ptr = malloc(cnt);
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG )
		bu_log("%8x malloc%6d %s\n", ptr, cnt, str);
	if( ptr==(char *)0 )  {
		bu_log("bu_malloc: Insufficient memory available, sbrk(0)=x%x\n", sbrk(0));
		bu_bomb("bu_malloc: malloc failure");
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		bu_memdebug_add( ptr, cnt, str );

		/* Install a barrier word at the end of the dynamic arena */
		/* Correct location depends on 'cnt' being rounded up, above */

		*((long *)(((char *)ptr)+cnt-sizeof(long))) = MDB_MAGIC;
	}
	return(ptr);
}

/*
 *			B U _ F R E E
 */
void
bu_free(ptr,str)
genptr_t	ptr;
CONST char	*str;
{
	if(bu_debug&BU_DEBUG_MEM_LOG) bu_log("%8x free %s\n", ptr, str);
	if(ptr == (char *)0 || ptr == (char *)(-1L) )  {
		bu_log("%8x free ERROR %s\n", ptr, str);
		return;
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		struct memdebug	*mp;
		if( (mp = bu_memdebug_check( ptr, str )) == MEMDEBUG_NULL )  {
			bu_log("ERROR bu_free(x%x, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n",
				ptr, str);
			return;
		} else {
			mp->mdb_len = 0;	/* successful delete */
		}
	}

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	*((int *)ptr) = -1;	/* zappo! */
	free(ptr);
	bu_semaphore_release(BU_SEM_SYSCALL);
}

/*
 *			B U _ R E A L L O C
 */
genptr_t
bu_realloc(ptr, cnt, str)
register genptr_t	ptr;
unsigned int		cnt;
CONST char		*str;
{
	char	*original_ptr = ptr;

	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		if( bu_memdebug_check( ptr, str ) == MEMDEBUG_NULL )  {
			bu_log("%8x realloc%6d %s ** barrier check failure\n",
				ptr, cnt, str );
		}
		/* Pad, plus full int for magic number */
		cnt = (cnt+2*sizeof(long)-1)&(~(sizeof(long)-1));
	}

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	ptr = realloc(ptr,cnt);
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG )  {
		bu_log("%8x realloc%6d %s %s\n", ptr, cnt, str,
			ptr == original_ptr ? "[grew in place]" : "[moved]" );
	}
	if( ptr==(char *)0 )  {
		bu_log("bu_realloc: Insufficient memory available, sbrk(0)=x%x\n", sbrk(0));
		bu_bomb("bu_realloc: malloc failure");
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		/* Even if ptr didn't change, need to update cnt & barrier */
		bu_memdebug_move( original_ptr, ptr, cnt, str );

		/* Install a barrier word at the end of the dynamic arena */
		/* Correct location depends on 'cnt' being rounded up, above */
		*((long *)(((char *)ptr)+cnt-sizeof(long))) = MDB_MAGIC;
	}
	return(ptr);
}

/*
 *			B U _ C A L L O C
 */
genptr_t
bu_calloc( nelem, elsize, str )
unsigned int	nelem;
unsigned int	elsize;
CONST char	*str;
{
	unsigned	len;
	genptr_t	ret;

	ret = bu_malloc( (len = nelem*elsize), str );
	bzero( ret, len );
	return ret;
}

/*
 *			B U _ P R M E M
 * 
 *  Print map of memory currently in use.
 */
void
bu_prmem(str)
CONST char *str;
{
	register struct memdebug *mp;
	register int *ip;

	bu_log("\nbu_prmem(): dynamic memory use (%s)\n", str);
	if( (bu_debug&BU_DEBUG_MEM_CHECK) == 0 )  {
		bu_log("\tMemory debugging is now OFF\n");
	}
	bu_log("\t%d elements in memdebug table\n Address Length Purpose\n",
		bu_memdebug_len);
	if( bu_memdebug_len <= 0 )  return;

	mp = &bu_memdebug[bu_memdebug_len-1];
	for( ; mp >= bu_memdebug; mp-- )  {
		if( mp->mdb_len <= 0 )  continue;
		ip = (int *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(int));
		bu_log("%8x %6x %s %s\n",
			mp->mdb_addr, mp->mdb_len, mp->mdb_str,
			*ip!=MDB_MAGIC ? "-BAD-" : "" );
		if( *ip != MDB_MAGIC )
			bu_log("\t%x\t%x\n", *ip, MDB_MAGIC);
	}
}

/*
 *			B U _ S T R D U P
 *
 * Given a string, allocate enough memory to hold it using bu_malloc(),
 * duplicate the strings, returns a pointer to the new string.
 */
char *
bu_strdup( cp )
register CONST char *cp;
{
	register char	*base;
	register int	len;

	if(bu_debug&BU_DEBUG_MEM_LOG) bu_log("bu_strdup(%s) x%x\n", cp, cp);

	len = strlen( cp )+2;
	if( (base = bu_malloc( len, "bu_strdup duplicate string" )) == (char *)0 )
		bu_bomb("bu_strdup:  unable to allocate memory");

	memcpy( base, cp, len );
	return(base);
}

/*
 *  			B U _ M A L L O C _ L E N _ R O U N D U P
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
bu_malloc_len_roundup(nbytes)
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

/*
 *			B U _ C K _ M A L L O C _ P T R
 *
 *	For a given pointer allocated by bu_malloc(),
 *	Check the magic number stored after the allocation area
 *	when BU_DEBUG_MEM_CHECK is set.
 *
 *	This is the individual version of bu_mem_barriercheck().
 *
 *	returns if pointer good or BU_DEBUG_MEM_CHECK not set,
 *	bombs if memory is corrupted.
 */
void
bu_ck_malloc_ptr( ptr, str )
genptr_t	ptr;
CONST char	*str;
{
	register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
	register long	*ip;


	/* if memory debugging isn't turned on, we have no way
	 * of knowing if the pointer is good or not
	 */
	if ((bu_debug&BU_DEBUG_MEM_CHECK) == 0) return;


	if (ptr == (char *)NULL) {
		bu_log("bu_ck_malloc_ptr(x%x, %s) null pointer\n\n", ptr, str);
		bu_bomb("Goodbye");
	}

	if( bu_memdebug == (struct memdebug *)0 )  {
		bu_log("bu_ck_malloc_ptr(x%x, %s)  no memdebug table yet\n",
			ptr, str);
		/* warning only -- the program is just getting started */
		return;
	}

	for( ; mp >= bu_memdebug; mp-- )  {
		if( mp->mdb_len <= 0 || mp->mdb_addr != ptr )  continue;

		ip = (long *)(((char *)ptr)+mp->mdb_len-sizeof(long));
		if( *ip != MDB_MAGIC )  {
			bu_log("ERROR bu_ck_malloc_ptr(x%x, %s) barrier word corrupted! was=x%x s/b=x%x\n",
				ptr, str, *ip, MDB_MAGIC);
			bu_bomb("bu_ck_malloc_ptr\n");
		}
		return;		/* OK */
	}
	bu_log("WARNING: bu_ck_malloc_ptr(x%x, %s)\
	pointer not in table of allocated memory.\n", ptr, str);
}

/*
 *			B U _ M E M _ B A R R I E R C H E C K
 *
 *  Check *all* entries in the memory debug table for barrier word
 *  corruption.
 *  Intended to be called periodicly through an application during debugging.
 *
 *  This is the bulk version of bu_ck_malloc_ptr()
 *
 *  Returns -
 *	-1	something is wrong
 *	 0	all is OK;
 */
int
bu_mem_barriercheck()
{
	register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
	register long	*ip;

	if( bu_memdebug == (struct memdebug *)0 )  {
		bu_log("bu_mem_barriercheck()  no memdebug table yet\n");
		return 0;
	}
	for( ; mp >= bu_memdebug; mp-- )  {
		if( mp->mdb_len <= 0 )  continue;
		ip = (long *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(long));
		if( *ip != MDB_MAGIC )  {
			bu_log("ERROR bu_mem_barriercheck(x%x, len=%d) barrier word corrupted!\nbarrier at x%x was=x%x s/b=x%x\n",
				mp->mdb_addr, mp->mdb_len,
				ip, *ip, MDB_MAGIC);
			return -1;	/* FAIL */
		}
	}
	return 0;			/* OK */
}
