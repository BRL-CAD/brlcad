/*                        M A L L O C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup malloc */
/** @{ */
/**
 * @file malloc.c
 *
 * @brief
 *  Parallel-protected debugging-enhanced wrapper around system malloc().
 *
 *  The bu_malloc() routines can't use bu_log() because that uses
 *  the bu_vls() routines which depend on bu_malloc().  So it goes direct
 *  to stderr, semaphore protected.
 *
 * @par  Functions
 *	bu_malloc	Allocate storage, with visibility & checking
 * @n	bu_free		Similarly, free storage
 * @n	bu_realloc	Reallocate storage, with visibility & checking
 * @n	bu_calloc	Allocate zero'ed storage
 * @n	bu_prmem	When debugging, print memory map
 * @n	bu_strdup_body	Duplicate a string in dynamic memory
 * @n	bu_malloc_len_roundup	Optimize sizing of malloc() requests
 * @n	bu_free_array	free elements of an array
 *
 *
 *  @author	Michael John Muuss
 *  @author      Christopher Sean Morrison
 *
 * @par  Source -
 *	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char RCSmalloc[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"

/** this variable controls the libbu debug level */
int	bu_debug = 0;


/** used by the memory allocation routines passed to bu_alloc by
 * default to indicate whether allocated memory should be zero'd
 * first.
 */
typedef enum {
  MALLOC,
  CALLOC
} alloc_t;


/* These counters are not semaphore-protected, and thus are only estimates */
long	bu_n_malloc = 0;
long	bu_n_free = 0;
long	bu_n_realloc = 0;

#define MDB_MAGIC	0x12348969
struct memdebug {
	long		magic;		/* corruption can be everywhere */
	genptr_t	mdb_addr;
	const char	*mdb_str;
	int		mdb_len;
};
static struct memdebug	*bu_memdebug = (struct memdebug *)NULL;
static struct memdebug	*bu_memdebug_lowat = (struct memdebug *)NULL;
static size_t		bu_memdebug_len = 0;
#define MEMDEBUG_NULL	((struct memdebug *)0)

struct memqdebug {
	struct bu_list	q;
	struct memdebug	m;
};

static struct bu_list *bu_memq = BU_LIST_NULL;
static struct bu_list bu_memqhd;
#define MEMQDEBUG_NULL	((struct memqdebug *)0)

const char bu_strdup_message[] = "bu_strdup string";
extern const char bu_vls_message[];	/* from vls.c */


#ifdef _WIN32
char *sbrk(i)
{
	return( (char *)0 );
}
#endif


/**
 *			B U _ M E M D E B U G _ A D D
 *
 *  Add another entry to the memory debug table
 */
HIDDEN void
bu_memdebug_add(char *ptr, unsigned int cnt, const char *str)
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
		if( bu_memdebug == (struct memdebug *)0 )
			bu_bomb("bu_memdebug_add() malloc failure\n");
	} else {
		size_t	old_len = bu_memdebug_len;
		bu_memdebug_len *= 16;
		bu_memdebug = (struct memdebug *)realloc(
			(char *)bu_memdebug,
			sizeof(struct memdebug) * bu_memdebug_len );
		if( bu_memdebug == (struct memdebug *)0 )
			bu_bomb("bu_memdebug_add() malloc failure\n");
		bzero( (char *)&bu_memdebug[old_len],
			(bu_memdebug_len-old_len) * sizeof(struct memdebug) );
	}
	bu_semaphore_release( BU_SEM_SYSCALL );

	goto top;
}

/**
 *			B U _ M E M D E B U G _ C H E C K
 *
 *  Check an entry against the memory debug table, based upon it's address.
 */
HIDDEN struct memdebug *
bu_memdebug_check(register char *ptr, const char *str)
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


/**
 *			B U _ A L L O C
 *
 *  This routine only returns on successful allocation.
 *  We promise never to return a NULL pointer; caller doesn't have to check.
 *
 *  Requesting allocation of zero bytes is considered a irrecoverable
 *  mistake in order to fulfill the non-NULL promise.
 *
 *  Failure results in bu_bomb() being called.
 *
 *  type is 0 for malloc, 1 for calloc
 */
static genptr_t
bu_alloc(alloc_t type, unsigned int cnt, unsigned int sz, const char *str)
{
	register genptr_t ptr;
	register unsigned long int size = cnt * sz;

	if( size == 0 )  {
		fprintf(stderr,"ERROR: bu_alloc size=0 (cnt=%d, sz=%d) %s\n", cnt, sz, str );
		bu_bomb("ERROR: bu_malloc(0)\n");
	}

	if( size < sizeof( int ) ) {
		size = sizeof( int );
	}

	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		/* Pad, plus full int for magic number */
		size = (size+2*sizeof(long)-1)&(~(sizeof(long)-1));
	} else if (bu_debug&BU_DEBUG_MEM_QCHECK ) {
		size = (size+2*sizeof(struct memqdebug)-1)
			&(~(sizeof(struct memqdebug)-1));
	}

#if defined(MALLOC_NOT_MP_SAFE)
	bu_semaphore_acquire( BU_SEM_SYSCALL );
#endif

	switch (type) {
	  case MALLOC:
	    ptr = malloc(size);
	    break;
	  case CALLOC:
#if defined(HAVE_CALLOC)
	      /* if we're debugging, we need a slightly larger
	       * allocation size for debug tracking.
	       */
	      if( bu_debug&(BU_DEBUG_MEM_CHECK|BU_DEBUG_MEM_QCHECK) )  {
		  ptr = malloc(size);
		  bzero(ptr, size);
	      } else {
		  ptr = calloc(cnt, sz);
	      }
#else
	    ptr = malloc(size);
	    bzero(ptr, size);
#endif
	    break;
	  default:
	    bu_bomb("ERROR: bu_alloc with unknown type\n");
	}

	if( ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG )  {
		fprintf(stderr, "%8lx malloc%7ld %s\n", (long)ptr, size, str);
	}
#if defined(MALLOC_NOT_MP_SAFE)
	bu_semaphore_release( BU_SEM_SYSCALL );
#endif

	if( ptr==(char *)0 )  {
		fprintf(stderr,"bu_malloc: Insufficient memory available, sbrk(0)=x%lx\n", (long)sbrk(0));
		bu_bomb("bu_malloc: malloc failure");
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		bu_memdebug_add( ptr, size, str );

		/* Install a barrier word at the end of the dynamic arena */
		/* Correct location depends on 'size' being rounded up, above */

		*((long *)(((char *)ptr)+size-sizeof(long))) = MDB_MAGIC;
	} else if (bu_debug&BU_DEBUG_MEM_QCHECK ) {
		struct memqdebug *mp = (struct memqdebug *)ptr;
		ptr = (genptr_t)(((struct memqdebug *)ptr)+1);
		mp->m.magic = MDB_MAGIC;
		mp->m.mdb_addr = ptr;
		mp->m.mdb_len = size;
		mp->m.mdb_str = str;
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		if (bu_memq == BU_LIST_NULL) {
			bu_memq = &bu_memqhd;
			BU_LIST_INIT(bu_memq);
		}
		BU_LIST_APPEND(bu_memq,&(mp->q));
		BU_LIST_MAGIC_SET(&(mp->q),MDB_MAGIC);
		bu_semaphore_release(BU_SEM_SYSCALL);
	}
	bu_n_malloc++;
	return(ptr);
}

/**
 *			B U _ M A L L O C
 *
 *  This routine only returns on successful allocation.
 *  We promise never to return a NULL pointer; caller doesn't have to check.
 *  Failure results in bu_bomb() being called.
w */
genptr_t
bu_malloc(size_t size, const char *str)
{
  return bu_alloc(MALLOC, 1, size, str);
}


/**
 *			B U _ C A L L O C
 *
 *  This routine only returns on successful allocation.
 *  We promise never to return a NULL pointer; caller doesn't have to check.
 *  Failure results in bu_bomb() being called.
 */
genptr_t
bu_calloc(unsigned int nelem, size_t elsize, const char *str)
{
  return bu_alloc(CALLOC, nelem, elsize, str);
}


/*
 *			B U _ F R E E
 */
void
bu_free(genptr_t ptr, const char *str)
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
		    fprintf(stderr,"ERROR bu_free(x%lx, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n", (long)ptr, str);
		} else {
		    mp->mdb_len = 0;	/* successful delete */
		}
	} else if (bu_debug&BU_DEBUG_MEM_QCHECK ) {
		struct memqdebug *mp = ((struct memqdebug *)ptr)-1;
		if (BU_LIST_MAGIC_WRONG(&(mp->q),MDB_MAGIC)) {
		    fprintf(stderr,"ERROR bu_free(x%lx, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n", (long)ptr, str);
		} else {
		    ptr = (genptr_t)mp;
		    bu_semaphore_acquire(BU_SEM_SYSCALL);
		    BU_LIST_DEQUEUE(&(mp->q));
		    bu_semaphore_release(BU_SEM_SYSCALL);
		}
	}

#if defined(MALLOC_NOT_MP_SAFE)
	bu_semaphore_acquire(BU_SEM_SYSCALL);
#endif
/* Windows does not like */
#ifndef _WIN32
	*((int *)ptr) = -1;	/* zappo! */
#endif
	free(ptr);
#if defined(MALLOC_NOT_MP_SAFE)
	bu_semaphore_release(BU_SEM_SYSCALL);
#endif
	bu_n_free++;
}

/**
 *			B U _ R E A L L O C
 *
 *  bu_malloc()/bu_free() compatible wrapper for realloc().
 *
 *  While the string 'str' is provided for the log messages, don't
 *  disturb the mdb_str value, so that this storage allocation can be
 *  tracked back to it's original creator.
 */
genptr_t
bu_realloc(register genptr_t ptr, size_t cnt, const char *str)
{
	struct memdebug		*mp=NULL;
	char	*original_ptr;

	if ( ! ptr ) {
	    /* This is so we are compatible with system realloc.
	     * It seems like an odd behaviour, but some non-BRL-CAD
	     * code relies on this.
	     */
	    return bu_malloc(cnt, str);
	}

	if( bu_debug&BU_DEBUG_MEM_CHECK )  {
		if( ptr && (mp = bu_memdebug_check( ptr, str )) == MEMDEBUG_NULL )  {
			fprintf(stderr,"%8lx realloc%6d %s ** barrier check failure\n",
				(long)ptr, cnt, str );
		}
		/* Pad, plus full long for magic number */
		cnt = (cnt+2*sizeof(long)-1)&(~(sizeof(long)-1));
	} else if ( bu_debug&BU_DEBUG_MEM_QCHECK ) {
		struct memqdebug *mp = ((struct memqdebug *)ptr)-1;

		cnt = (cnt + 2*sizeof(struct memqdebug) - 1)
		    &(~(sizeof(struct memqdebug)-1));

		if (BU_LIST_MAGIC_WRONG(&(mp->q),MDB_MAGIC)) {
			fprintf(stderr,"ERROR bu_realloc(x%lx, %s) pointer bad, "
				"or not allocated with bu_malloc!  Ignored.\n",
				(long)ptr, str);
			/*
			 * Since we're ignoring this, atleast return
			 * the pointer that was passed in. We should
			 * probably return NULL.
			 */
			return ptr;
		}
		ptr = (genptr_t)mp;
		BU_LIST_DEQUEUE(&(mp->q));
	}

	original_ptr = ptr;

#if defined(MALLOC_NOT_MP_SAFE)
	bu_semaphore_acquire(BU_SEM_SYSCALL);
#endif
	ptr = realloc(ptr,cnt);
#if defined(MALLOC_NOT_MP_SAFE)
	bu_semaphore_release(BU_SEM_SYSCALL);
#endif

	if( ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG )  {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		if (ptr == original_ptr) {
			fprintf(stderr,"%8lx realloc%6d %s [grew in place]\n",
				   (long)ptr,       cnt, str );
		} else {
			fprintf(stderr,"%8lx realloc%6d %s [moved from %8lx]\n",
				   (long)ptr,       cnt, str, original_ptr);
		}

		bu_semaphore_release(BU_SEM_SYSCALL);
	}
	if( ptr==(char *)0 && cnt > 0 )  {
		fprintf(stderr,"bu_realloc: Insufficient memory available, sbrk(0)=x%lx\n", (long)sbrk(0));
		bu_bomb("bu_realloc: malloc failure");
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK && ptr )  {
		/* Even if ptr didn't change, need to update cnt & barrier */
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		mp->mdb_addr = ptr;
		mp->mdb_len = cnt;

		/* Install a barrier word at the new end of the dynamic arena */
		/* Correct location depends on 'cnt' being rounded up, above */
		*((long *)(((char *)ptr)+cnt-sizeof(long))) = MDB_MAGIC;
		bu_semaphore_release(BU_SEM_SYSCALL);
	} else if ( bu_debug&BU_DEBUG_MEM_QCHECK && ptr ) {
		struct memqdebug *mp;
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		mp = (struct memqdebug *)ptr;
		ptr = (genptr_t)(((struct memqdebug *)ptr)+1);
		mp->m.magic = MDB_MAGIC;
		mp->m.mdb_addr = ptr;
		mp->m.mdb_len = cnt;
		mp->m.mdb_str = str;
		BU_ASSERT(bu_memq != BU_LIST_NULL);
		BU_LIST_APPEND(bu_memq,&(mp->q));
		BU_LIST_MAGIC_SET(&(mp->q),MDB_MAGIC);
		bu_semaphore_release(BU_SEM_SYSCALL);
	}
	bu_n_realloc++;
	return(ptr);
}


/**
 *			B U _ P R M E M
 *
 *  Print map of memory currently in use.
 */
void
bu_prmem(const char *str)
{
    register struct memdebug *mp;
    register struct memqdebug *mqp;
    register long *ip;
    register size_t count = 0;

    fprintf(stderr,"\nbu_prmem(): dynamic memory use (%s)\n", str);
    if( (bu_debug&(BU_DEBUG_MEM_CHECK|BU_DEBUG_MEM_QCHECK)) == 0 )  {
	fprintf(stderr,"\tMemory debugging is now OFF\n");
    }
#if 0
    fprintf(stderr,"\t%ld slots in memdebug table (not # of allocs)\n Address Length Purpose\n",
	    (long)bu_memdebug_len);
#else
    fprintf(stderr," Address Length Purpose\n");
#endif
    if( bu_memdebug_len > 0 )  {
	mp = &bu_memdebug[bu_memdebug_len-1];
	for( ; mp >= bu_memdebug; mp-- )  {
	    if( !mp->magic )  continue;
	    if( mp->magic != MDB_MAGIC )  bu_bomb("bu_memdebug_check() malloc tracing table corrupted!\n");
	    if( mp->mdb_len <= 0 )  continue;

	    count++;
	    ip = (long *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(long));
	    if( mp->mdb_str == bu_strdup_message )  {
		fprintf(stderr,"%8lx %6d bu_strdup: \"%s\"\n",
			(long)(mp->mdb_addr), mp->mdb_len,
			((char *)mp->mdb_addr) );
	    } else if( mp->mdb_str == bu_vls_message )  {
		fprintf(stderr,"%8lx %6d bu_vls: \"%s\"\n",
			(long)(mp->mdb_addr), mp->mdb_len,
			((char *)mp->mdb_addr) );
	    } else {
		fprintf(stderr,"%8lx %6d %s\n",
			(long)(mp->mdb_addr), mp->mdb_len,
			mp->mdb_str);
	    }
	    if( *ip != MDB_MAGIC )  {
		fprintf(stderr,"\tCorrupted end marker was=x%lx\ts/b=x%x\n",
			*ip, MDB_MAGIC);
	    }
	}
    }


    if (bu_memq != BU_LIST_NULL)  {
	fprintf(stderr,"memdebug queue\n Address Length Purpose\n");
	BU_LIST_EACH(bu_memq, mqp, struct memqdebug) {
	    if (BU_LIST_MAGIC_WRONG(&(mqp->q),MDB_MAGIC)
		|| BU_LIST_MAGIC_WRONG(&(mqp->m),MDB_MAGIC))
		bu_bomb("bu_prmem() malloc tracing queue corrupted!\n");
	    if( mqp->m.mdb_str == bu_strdup_message )  {
		fprintf(stderr,"%8lx %6d bu_strdup: \"%s\"\n",
			(long)(mqp->m.mdb_addr), mqp->m.mdb_len,
			((char *)mqp->m.mdb_addr) );
	    } else if( mqp->m.mdb_str == bu_vls_message )  {
		fprintf(stderr,"%8lx %6d bu_vls: \"%s\"\n",
			(long)(mqp->m.mdb_addr), mqp->m.mdb_len,
			((char *)mqp->m.mdb_addr) );
	    } else {
		fprintf(stderr,"%8lx %6d %s\n",
			(long)(mqp->m.mdb_addr), mqp->m.mdb_len,
			mqp->m.mdb_str);
	    }
	}
    }

    fprintf(stderr, "%lu allocation entries\n", count);


}

/**
 *			B U _ S T R D U P
 *
 * Given a string, allocate enough memory to hold it using bu_malloc(),
 * duplicate the strings, returns a pointer to the new string.
 */
#if 0
char *
bu_strdup(register const char *cp)
{
	return bu_strdupm(cp, bu_strdup_message);
}
#endif
char *
bu_strdupm(register const char *cp, const char *label)
{
	register char	*base;
	register size_t	len;

	len = strlen( cp )+1;
	base = bu_malloc( len, label);

	if(bu_debug&BU_DEBUG_MEM_LOG) {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		fprintf(stderr, "%8lx strdup%7ld \"%s\"\n", (long)base, (long)len, cp );
		bu_semaphore_release(BU_SEM_SYSCALL);
	}

	memcpy( base, cp, len );
	return(base);
}

/**
 *			B U _ D I R N A M E
 *
 *  Given a filesystem pathname, return a pointer to a dynamic string
 *  which is the parent directory of that file/directory.
 *
 *	/usr/dir/file	/usr/dir
 * @n	/usr/dir/	/usr
 * @n	/usr/file	/usr
 * @n	/usr/		/
 * @n	/usr		/
 * @n	/		/
 * @n	.		.
 * @n	..		.
 * @n	usr		.
 * @n	a/b		a
 * @n	a/		.
 * @n	../a/b		../a
 */
char *
bu_dirname(const char *cp)
{
	char	*ret;
	char	*slash;
	int	len;

	/* Special cases */
	if( cp == NULL )  return bu_strdup(".");
	if( strcmp( cp, "/" ) == 0 )
		return bu_strdup("/");
	if( strcmp( cp, "." ) == 0 ||
	    strcmp( cp, ".." ) == 0 ||
	    strrchr(cp, '/') == NULL )
		return bu_strdup(".");

	/* Make a duplicate copy of the string, and shorten it in place */
	ret = bu_strdup(cp);

	/* A trailing slash doesn't count */
	len = strlen(ret);
	if( ret[len-1] == '/' )  ret[len-1] = '\0';

	/* If no slashes remain, return "." */
	if( (slash = strrchr(ret, '/')) == NULL )  {
		bu_free( ret, "bu_dirname" );
		return bu_strdup(".");
	}

	/* Remove trailing slash, unless it's at front */
	if( slash == ret )
		ret[1] = '\0';		/* ret == "/" */
	else
		*slash = '\0';

	return ret;
}

/**
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
bu_malloc_len_roundup(register int nbytes)
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

/**
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
bu_ck_malloc_ptr(genptr_t ptr, const char *str)
{
	register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
	register long	*ip;


	if (ptr == (char *)NULL) {
		fprintf(stderr,"bu_ck_malloc_ptr(x%lx, %s) null pointer\n\n", (long)ptr, str);
		bu_bomb("Goodbye");
	}

	if (bu_debug&BU_DEBUG_MEM_CHECK) {
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
	} else if (bu_debug&BU_DEBUG_MEM_QCHECK) {
		struct memqdebug *mp = (struct memqdebug *)ptr;
		if (BU_LIST_MAGIC_WRONG(&(mp->q),MDB_MAGIC)
		    || mp->m.magic != MDB_MAGIC) {
			fprintf(stderr,"WARNING: bu_ck_malloc_ptr(x%lx, %s)"
				" memory corrupted.\n", (long)ptr, str);
		}
	}
}

/**
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
bu_mem_barriercheck(void)
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


/** b u _ f r e e _ a r r a y
 *
 * free up to argc elements of memory allocated to an array without
 * free'ing the array itself.
 */
void bu_free_array(int argc, char *argv[], const char *str)
{
  int count = 0;

  if (!argv || argc <= 0) {
    return;
  }

  while (count < argc) {
    if (argv[count]) {
      bu_free(argv[count], str);
      argv[count] = NULL;
    }
    count++;
  }

  return;
}

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
