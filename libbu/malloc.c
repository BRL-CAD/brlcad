/*
 *			M A L L O C . C
 *
 *  Parallel-protected debugging-enhanced wrapper around system malloc().
 *
 *  The bu_malloc() routines can't use bu_log() because that uses
 *  the bu_vls() routines which depend on bu_malloc().  So it goes direct
 *  to stderr, semaphore protected.
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
	long		magic;		/* corruption can be everywhere */
	genptr_t	mdb_addr;
	CONST char	*mdb_str;
	int		mdb_len;
};
static struct memdebug	*bu_memdebug = (struct memdebug *)NULL;
static struct memdebug	*bu_memdebug_lowat = (struct memdebug *)NULL;
static size_t		bu_memdebug_len = 0;
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
		if( bu_memdebug_lowat > bu_memdebug &&
		    bu_memdebug_lowat < mp )  {
		    	mp = bu_memdebug_lowat;
		} else {
			bu_memdebug_lowat = mp;
		}
again:
		for( ; mp >= bu_memdebug; mp-- )  {
			/* Search for an empty slot */
			if( mp->mdb_len > 0 )  continue;
			mp->magic = MDB_MAGIC;
			mp->mdb_addr = ptr;
			mp->mdb_len = cnt;
			mp->mdb_str = str;
			bu_memdebug_lowat = mp-1;
			bu_semaphore_release( BU_SEM_SYSCALL );
			return;
		}
		/* Didn't find a slot.  If started in middle, go again */
		mp = &bu_memdebug[bu_memdebug_len-1];
		if( bu_memdebug_lowat != mp )  {
			bu_memdebug_lowat = mp;
			goto again;
		}
	}

	/* Need to make more slots */
	if( bu_memdebug_len <= 0 )  {
		bu_memdebug_len = 5120-2;
		bu_memdebug = (struct memdebug *)calloc(
			bu_memdebug_len, sizeof(struct memdebug) );
	} else {
		size_t	old_len = bu_memdebug_len;
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
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		fprintf(stderr,"bu_memdebug_check(x%lx, %s)  no memdebug table yet\n",
			(long)ptr, str);
		bu_semaphore_release(BU_SEM_SYSCALL);
		return MEMDEBUG_NULL;
	}
	for( ; mp >= bu_memdebug; mp-- )  {
		if( !mp->magic )  continue;
		if( mp->magic != MDB_MAGIC )  bu_bomb("bu_memdebug_check() malloc tracing table corrupted!\n");
		if( mp->mdb_len <= 0 )  continue;
		if( mp->mdb_addr != ptr )  continue;
		ip = (long *)(ptr+mp->mdb_len-sizeof(long));
		if( *ip != MDB_MAGIC )  {
			bu_semaphore_acquire(BU_SEM_SYSCALL);
			fprintf(stderr,"ERROR bu_memdebug_check(x%lx, %s) %s, barrier word corrupted!\nbarrier at x%lx was=x%lx s/b=x%x, len=%d\n",
				(long)ptr, str, mp->mdb_str,
				(long)ip, *ip, MDB_MAGIC, mp->mdb_len);
			bu_semaphore_release(BU_SEM_SYSCALL);
			bu_bomb("bu_memdebug_check() memory corruption\n");
		}
		return(mp);		/* OK */
	}
	return MEMDEBUG_NULL;
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
		fprintf(stderr,"ERROR: bu_malloc count=0 %s\n", str );
		bu_bomb("ERROR: bu_malloc(0)\n");
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		/* Pad, plus full int for magic number */
		cnt = (cnt+2*sizeof(long)-1)&(~(sizeof(long)-1));
	}

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	ptr = malloc(cnt);
	if( ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG )  {
		fprintf(stderr, "%8lx malloc%7d %s\n", (long)ptr, cnt, str);
	}
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( ptr==(char *)0 )  {
		fprintf(stderr,"bu_malloc: Insufficient memory available, sbrk(0)=x%lx\n", (long)sbrk(0));
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
	if(bu_debug&BU_DEBUG_MEM_LOG) {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		fprintf(stderr, "%8lx free          %s\n", (long)ptr, str);
		bu_semaphore_release(BU_SEM_SYSCALL);
	}
	if(ptr == (char *)0 || ptr == (char *)(-1L) )  {
		fprintf(stderr,"%8lx free ERROR %s\n", (long)ptr, str);
		return;
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		struct memdebug	*mp;
		if( (mp = bu_memdebug_check( ptr, str )) == MEMDEBUG_NULL )  {
			fprintf(stderr,"ERROR bu_free(x%lx, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n",
				(long)ptr, str);
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
 *
 *  bu_malloc()/bu_free() compatible wrapper for realloc().
 *
 *  While the string 'str' is provided for the log messages, don't
 *  disturb the mdb_str value, so that this storage allocation can be
 *  tracked back to it's original creator.
 */
genptr_t
bu_realloc(ptr, cnt, str)
register genptr_t	ptr;
unsigned int		cnt;
CONST char		*str;
{
	struct memdebug		*mp;
	char	*original_ptr = ptr;

	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		if( (mp = bu_memdebug_check( ptr, str )) == MEMDEBUG_NULL )  {
			fprintf(stderr,"%8lx realloc%6d %s ** barrier check failure\n",
				(long)ptr, cnt, str );
		}
		/* Pad, plus full int for magic number */
		cnt = (cnt+2*sizeof(long)-1)&(~(sizeof(long)-1));
	}

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	ptr = realloc(ptr,cnt);
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG )  {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		fprintf(stderr,"%8lx realloc%6d %s %s\n", (long)ptr, cnt, str,
			ptr == original_ptr ? "[grew in place]" : "[moved]" );
		bu_semaphore_release(BU_SEM_SYSCALL);
	}
	if( ptr==(char *)0 )  {
		fprintf(stderr,"bu_realloc: Insufficient memory available, sbrk(0)=x%lx\n", (long)sbrk(0));
		bu_bomb("bu_realloc: malloc failure");
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		/* Even if ptr didn't change, need to update cnt & barrier */
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		mp->mdb_addr = ptr;
		mp->mdb_len = cnt;

		/* Install a barrier word at the new end of the dynamic arena */
		/* Correct location depends on 'cnt' being rounded up, above */
		*((long *)(((char *)ptr)+cnt-sizeof(long))) = MDB_MAGIC;
		bu_semaphore_release(BU_SEM_SYSCALL);
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
	register long *ip;

	fprintf(stderr,"\nbu_prmem(): dynamic memory use (%s)\n", str);
	if( (bu_debug&BU_DEBUG_MEM_CHECK) == 0 )  {
		fprintf(stderr,"\tMemory debugging is now OFF\n");
	}
	fprintf(stderr,"\t%ld elements in memdebug table\n Address Length Purpose\n",
		(long)bu_memdebug_len);
	if( bu_memdebug_len <= 0 )  return;

	mp = &bu_memdebug[bu_memdebug_len-1];
	for( ; mp >= bu_memdebug; mp-- )  {
		if( !mp->magic )  continue;
		if( mp->magic != MDB_MAGIC )  bu_bomb("bu_memdebug_check() malloc tracing table corrupted!\n");
		if( mp->mdb_len <= 0 )  continue;
		ip = (long *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(int));
		fprintf(stderr,"%8lx %6d %s %s\n",
			(long)(mp->mdb_addr), mp->mdb_len, mp->mdb_str,
			*ip!=MDB_MAGIC ? "-BAD-" : "" );
		if( *ip != MDB_MAGIC )
			fprintf(stderr,"\t%lx\t%x\n", *ip, MDB_MAGIC);
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

	if(bu_debug&BU_DEBUG_MEM_LOG) {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		fprintf(stderr,"bu_strdup(%s) x%lx\n", cp, (long)cp);
		bu_semaphore_release(BU_SEM_SYSCALL);
	}

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
		fprintf(stderr,"bu_ck_malloc_ptr(x%lx, %s) null pointer\n\n", (long)ptr, str);
		bu_bomb("Goodbye");
	}

	if( bu_memdebug == (struct memdebug *)0 )  {
		fprintf(stderr,"bu_ck_malloc_ptr(x%lx, %s)  no memdebug table yet\n",
			(long)ptr, str);
		/* warning only -- the program is just getting started */
		return;
	}

	for( ; mp >= bu_memdebug; mp-- )  {
		if( !mp->magic )  continue;
		if( mp->magic != MDB_MAGIC )  bu_bomb("bu_ck_malloc_ptr() malloc tracing table corrupted!\n");
		if( mp->mdb_len <= 0 || mp->mdb_addr != ptr )  continue;

		/* Found the relevant entry */
		ip = (long *)(((char *)ptr)+mp->mdb_len-sizeof(long));
		if( *ip != MDB_MAGIC )  {
			fprintf(stderr,"ERROR bu_ck_malloc_ptr(x%lx, %s) barrier word corrupted! was=x%lx s/b=x%x\n",
				(long)ptr, str, (long)*ip, MDB_MAGIC);
			bu_bomb("bu_ck_malloc_ptr\n");
		}
		return;		/* OK */
	}
	fprintf(stderr,"WARNING: bu_ck_malloc_ptr(x%lx, %s)\
	pointer not in table of allocated memory.\n", (long)ptr, str);
}

/*
 *			B U _ M E M _ B A R R I E R C H E C K
 *
 *  Check *all* entries in the memory debug table for barrier word
 *  corruption.
 *  Intended to be called periodicly through an application during debugging.
 *  Has to run single-threaded, to prevent table mutation.
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
		fprintf(stderr,"bu_mem_barriercheck()  no memdebug table yet\n");
		return 0;
	}
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	for( ; mp >= bu_memdebug; mp-- )  {
		if( !mp->magic )  continue;
		if( mp->magic != MDB_MAGIC )  {
			bu_semaphore_release( BU_SEM_SYSCALL );
			fprintf(stderr,"  mp->magic = x%lx, s/b=x%x\n", (long)(mp->magic), MDB_MAGIC );
			bu_bomb("bu_mem_barriercheck() malloc tracing table corrupted!\n");
		}
		if( mp->mdb_len <= 0 )  continue;
		ip = (long *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(long));
		if( *ip != MDB_MAGIC )  {
			bu_semaphore_release( BU_SEM_SYSCALL );
			fprintf(stderr,"ERROR bu_mem_barriercheck(x%lx, len=%d) barrier word corrupted!\n\tbarrier at x%lx was=x%lx s/b=x%x %s\n",
				(long)mp->mdb_addr, mp->mdb_len,
				(long)ip, *ip, MDB_MAGIC, mp->mdb_str);
			return -1;	/* FAIL */
		}
	}
	bu_semaphore_release( BU_SEM_SYSCALL );
	return 0;			/* OK */
}
