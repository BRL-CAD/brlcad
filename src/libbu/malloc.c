/*                        M A L L O C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"

/** this controls whether to semaphore protect malloc calls */
#define MALLOC_NOT_MP_SAFE 1


/**
 * used by the memory allocation routines passed to bu_alloc by
 * default to indicate whether allocated memory should be zero'd
 * first.
 */
typedef enum {
    MALLOC,
    CALLOC
} alloc_t;


#define MDB_MAGIC 0x12348969
struct memdebug {
    long magic;		/* corruption can be everywhere */
    genptr_t mdb_addr;
    const char *mdb_str;
    int mdb_len;
};
static struct memdebug *bu_memdebug = (struct memdebug *)NULL;
static struct memdebug *bu_memdebug_lowat = (struct memdebug *)NULL;
static size_t bu_memdebug_len = 0;
#define MEMDEBUG_NULL ((struct memdebug *)0)

struct memqdebug {
    struct bu_list q;
    struct memdebug m;
};

static struct bu_list *bu_memq = BU_LIST_NULL;
static struct bu_list bu_memqhd;
#define MEMQDEBUG_NULL ((struct memqdebug *)0)

/* non-published globals */
extern const char bu_vls_message[];
extern const char bu_strdup_message[];


/**
 * _ B U _ M E M D E B U G _ A D D
 *
 * Add another entry to the memory debug table
 */
HIDDEN void
_bu_memdebug_add(genptr_t ptr, unsigned int cnt, const char *str)
{
    register struct memdebug *mp = NULL;

 top:
    bu_semaphore_acquire(BU_SEM_SYSCALL);

    if (bu_memdebug) {
	mp = &bu_memdebug[bu_memdebug_len-1];
	if (bu_memdebug_lowat > bu_memdebug
	    && bu_memdebug_lowat < mp)
	{
	    mp = bu_memdebug_lowat;
	} else {
	    bu_memdebug_lowat = mp;
	}
    }

 again:
    if (bu_memdebug) {
	for (; mp >= bu_memdebug; mp--) {
	    /* Search for an empty slot */
	    if (mp->mdb_len > 0)  continue;
	    mp->magic = MDB_MAGIC;
	    mp->mdb_addr = ptr;
	    mp->mdb_len = cnt;
	    mp->mdb_str = str;
	    bu_memdebug_lowat = mp-1;
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    return;
	}
	/* Didn't find a slot.  If started in middle, go again */
	mp = &bu_memdebug[bu_memdebug_len-1];
	if (bu_memdebug_lowat != mp) {
	    bu_memdebug_lowat = mp;
	    goto again;
	}
    }

    /* Need to make more slots */
    if (bu_memdebug_len <= 0) {
	bu_memdebug_len = 5120-2;
	bu_memdebug = (struct memdebug *)calloc(
	    bu_memdebug_len, sizeof(struct memdebug));
	if (bu_memdebug == (struct memdebug *)0)
	    bu_bomb("_bu_memdebug_add() malloc failure\n");
    } else {
	size_t old_len = bu_memdebug_len;
	bu_memdebug_len *= 16;
	bu_memdebug = (struct memdebug *)realloc((char *)bu_memdebug, sizeof(struct memdebug) * bu_memdebug_len);
	if (bu_memdebug == (struct memdebug *)0)
	    bu_bomb("_bu_memdebug_add() malloc failure\n");
	memset((char *)&bu_memdebug[old_len], 0,
	       (bu_memdebug_len-old_len) * sizeof(struct memdebug));
    }
    bu_semaphore_release(BU_SEM_SYSCALL);

    goto top;
}


/**
 * _ B U _ M E M D E B U G _ C H E C K
 *
 * Check an entry against the memory debug table, based upon it's address.
 */
HIDDEN struct memdebug *
_bu_memdebug_check(register genptr_t ptr, const char *str)
{
    register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
    register long *ip;

    if (bu_memdebug == (struct memdebug *)0) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "_bu_memdebug_check(x%lx, %s)  no memdebug table yet\n",
		(long)ptr, str);
	bu_semaphore_release(BU_SEM_SYSCALL);
	return MEMDEBUG_NULL;
    }
    for (; mp >= bu_memdebug; mp--) {
	if (!mp->magic)  continue;
	if (mp->magic != MDB_MAGIC)  bu_bomb("_bu_memdebug_check() malloc tracing table corrupted!\n");
	if (mp->mdb_len <= 0)  continue;
	if (mp->mdb_addr != ptr)  continue;
	ip = (long *)((char *)ptr+mp->mdb_len-sizeof(long));
	if (*ip != MDB_MAGIC) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    fprintf(stderr, "ERROR _bu_memdebug_check(x%lx, %s) %s, barrier word corrupted!\nbarrier at x%lx was=x%lx s/b=x%x, len=%d\n",
		    (long)ptr, str, mp->mdb_str,
		    (long)ip, *ip, MDB_MAGIC, mp->mdb_len);
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    bu_bomb("_bu_memdebug_check() memory corruption\n");
	}
	return (mp);		/* OK */
    }
    return MEMDEBUG_NULL;
}


/**
 * _ B U _ A L L O C
 *
 * This routine only returns on successful allocation.  We promise
 * never to return a NULL pointer; caller doesn't have to check.
 *
 * Requesting allocation of zero bytes is considered a irrecoverable
 * mistake in order to fulfill the non-NULL promise.
 *
 * Failure results in bu_bomb() being called.
 *
 * type is 0 for malloc, 1 for calloc
 */
HIDDEN genptr_t
_bu_alloc(alloc_t type, unsigned int cnt, unsigned int sz, const char *str)
{
    register genptr_t ptr = 0;
    register unsigned long int size = cnt * sz;

    extern int bu_bomb_failsafe_init();
    static int failsafe_init = 0;

    /* bu_bomb hook to recover from memory problems */
    if (!failsafe_init) {
	failsafe_init = bu_bomb_failsafe_init();
    }

    if (size == 0) {
	fprintf(stderr, "ERROR: _bu_alloc size=0 (cnt=%d, sz=%d) %s\n", cnt, sz, str);
	bu_bomb("ERROR: bu_malloc(0)\n");
    }

    if (size < sizeof(int)) {
	size = sizeof(int);
    }

    if (bu_debug&BU_DEBUG_MEM_CHECK) {
	/* Pad, plus full int for magic number */
	size = (size+2*sizeof(long)-1)&(~(sizeof(long)-1));
    } else if (bu_debug&BU_DEBUG_MEM_QCHECK) {
	size = (size+2*sizeof(struct memqdebug)-1)
	    &(~(sizeof(struct memqdebug)-1));
    }

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_SYSCALL);
#endif

    switch (type) {
	case MALLOC:
	    ptr = malloc(size);
	    break;
	case CALLOC:
	    /* if we're debugging, we need a slightly larger
	     * allocation size for debug tracking.
	     */
	    if (bu_debug&(BU_DEBUG_MEM_CHECK|BU_DEBUG_MEM_QCHECK)) {
		ptr = malloc(size);
		memset(ptr, 0, size);
	    } else {
		ptr = calloc(cnt, sz);
	    }
	    break;
	default:
	    bu_bomb("ERROR: _bu_alloc with unknown type\n");
    }

    if (ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG) {
	fprintf(stderr, "%8lx malloc%7ld %s\n", (long)ptr, size, str);
    }
#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_release(BU_SEM_SYSCALL);
#endif

    if (ptr==(char *)0) {
	fprintf(stderr, "bu_malloc: Insufficient memory available\n");
	bu_bomb("bu_malloc: malloc failure");
    }
    if (bu_debug&BU_DEBUG_MEM_CHECK) {
	_bu_memdebug_add(ptr, size, str);

	/* Install a barrier word at the end of the dynamic arena */
	/* Correct location depends on 'size' being rounded up, above */

	*((long *)(((char *)ptr)+size-sizeof(long))) = MDB_MAGIC;
    } else if (bu_debug&BU_DEBUG_MEM_QCHECK) {
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
	BU_LIST_APPEND(bu_memq, &(mp->q));
	BU_LIST_MAGIC_SET(&(mp->q), MDB_MAGIC);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    bu_n_malloc++;
    return (ptr);
}


genptr_t
bu_malloc(size_t size, const char *str)
{
    return _bu_alloc(MALLOC, 1, size, str);
}


genptr_t
bu_calloc(unsigned int nelem, size_t elsize, const char *str)
{
    return _bu_alloc(CALLOC, nelem, elsize, str);
}


void
bu_free(genptr_t ptr, const char *str)
{
    if (bu_debug&BU_DEBUG_MEM_LOG) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "%8lx free          %s\n", (long)ptr, str);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (ptr == (char *)0 || ptr == (char *)(-1L)) {
	fprintf(stderr, "%8lx free ERROR %s\n", (long)ptr, str);
	return;
    }
    if (bu_debug&BU_DEBUG_MEM_CHECK) {
	struct memdebug *mp;
	if ((mp = _bu_memdebug_check(ptr, str)) == MEMDEBUG_NULL) {
	    fprintf(stderr, "ERROR bu_free(x%lx, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n", (long)ptr, str);
	} else {
	    mp->mdb_len = 0;	/* successful delete */
	}
    } else if (bu_debug&BU_DEBUG_MEM_QCHECK) {
	struct memqdebug *mp = ((struct memqdebug *)ptr)-1;
	if (BU_LIST_MAGIC_WRONG(&(mp->q), MDB_MAGIC)) {
	    fprintf(stderr, "ERROR bu_free(x%lx, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n", (long)ptr, str);
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


genptr_t
bu_realloc(register genptr_t ptr, size_t cnt, const char *str)
{
    struct memdebug *mp=NULL;
    genptr_t original_ptr;

    if (!ptr) {
	/* This is so we are compatible with system realloc.  It seems
	 * like an odd behaviour, but some non-BRL-CAD code relies on
	 * this.
	 */
	return bu_malloc(cnt, str);
    }

    if (bu_debug&BU_DEBUG_MEM_CHECK) {
	mp = _bu_memdebug_check(ptr, str);
	if (mp == MEMDEBUG_NULL) {
	    fprintf(stderr, "%8lx realloc%6d %s ** barrier check failure\n",
		    (long)ptr, (int)cnt, str);
	}
	/* Pad, plus full long for magic number */
	cnt = (cnt+2*sizeof(long)-1)&(~(sizeof(long)-1));
    } else if (bu_debug&BU_DEBUG_MEM_QCHECK) {
	struct memqdebug *mqp = ((struct memqdebug *)ptr)-1;

	cnt = (cnt + 2*sizeof(struct memqdebug) - 1)
	    &(~(sizeof(struct memqdebug)-1));

	if (BU_LIST_MAGIC_WRONG(&(mqp->q), MDB_MAGIC)) {
	    fprintf(stderr, "ERROR bu_realloc(x%lx, %s) pointer bad, "
		    "or not allocated with bu_malloc!  Ignored.\n",
		    (long)ptr, str);
	    /*
	     * Since we're ignoring this, atleast return the pointer
	     * that was passed in. We should probably return NULL.
	     */
	    return ptr;
	}
	ptr = (genptr_t)mqp;
	BU_LIST_DEQUEUE(&(mqp->q));
    }

    if (cnt == 0) {
	fprintf(stderr, "ERROR: bu_realloc cnt=0 (ptr=%p) %s\n", ptr, str);
	bu_bomb("ERROR: bu_realloc(0)\n");
    }

    original_ptr = ptr;

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_SYSCALL);
#endif
    ptr = realloc(ptr, cnt);
#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_release(BU_SEM_SYSCALL);
#endif

    if (ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	if (ptr == original_ptr) {
	    fprintf(stderr, "%8lx realloc%6d %s [grew in place]\n",
		    (long)ptr, (int)cnt, str);
	} else {
	    fprintf(stderr, "%8lx realloc%6d %s [moved from %8lx]\n",
		    (long)ptr, (int)cnt, str, (long unsigned int)original_ptr);
	}

	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (ptr==(char *)0 && cnt > 0) {
	fprintf(stderr, "bu_realloc: Insufficient memory available\n");
	bu_bomb("bu_realloc: malloc failure");
    }
    if (bu_debug&BU_DEBUG_MEM_CHECK && ptr) {
	/* Even if ptr didn't change, need to update cnt & barrier */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	mp->mdb_addr = ptr;
	mp->mdb_len = cnt;

	/* Install a barrier word at the new end of the dynamic
	 * arena. Correct location depends on 'cnt' being rounded up,
	 * above.
	 */
	*((long *)(((char *)ptr)+cnt-sizeof(long))) = MDB_MAGIC;
	bu_semaphore_release(BU_SEM_SYSCALL);
    } else if (bu_debug&BU_DEBUG_MEM_QCHECK && ptr) {
	struct memqdebug *mqp;
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	mqp = (struct memqdebug *)ptr;
	ptr = (genptr_t)(((struct memqdebug *)ptr)+1);
	mqp->m.magic = MDB_MAGIC;
	mqp->m.mdb_addr = ptr;
	mqp->m.mdb_len = cnt;
	mqp->m.mdb_str = str;
	BU_ASSERT(bu_memq != BU_LIST_NULL);
	BU_LIST_APPEND(bu_memq, &(mqp->q));
	BU_LIST_MAGIC_SET(&(mqp->q), MDB_MAGIC);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    bu_n_realloc++;
    return (ptr);
}


void
bu_prmem(const char *str)
{
    register struct memdebug *mp;
    register struct memqdebug *mqp;
    register long *ip;
    register size_t count = 0;

    fprintf(stderr, "\nbu_prmem(): dynamic memory use (%s)\n", str);
    if ((bu_debug&(BU_DEBUG_MEM_CHECK|BU_DEBUG_MEM_QCHECK)) == 0) {
	fprintf(stderr, "\tMemory debugging is now OFF\n");
    }
#if 0
    fprintf(stderr, "\t%ld slots in memdebug table (not # of allocs)\n Address Length Purpose\n",
	    (long)bu_memdebug_len);
#else
    fprintf(stderr, " Address Length Purpose\n");
#endif
    if (bu_memdebug_len > 0) {
	mp = &bu_memdebug[bu_memdebug_len-1];
	for (; mp >= bu_memdebug; mp--) {
	    if (!mp->magic)  continue;
	    if (mp->magic != MDB_MAGIC)  bu_bomb("_bu_memdebug_check() malloc tracing table corrupted!\n");
	    if (mp->mdb_len <= 0)  continue;

	    count++;
	    ip = (long *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(long));
	    if (mp->mdb_str == bu_strdup_message) {
		fprintf(stderr, "%8lx %6d bu_strdup: \"%s\"\n",
			(long)(mp->mdb_addr), mp->mdb_len,
			((char *)mp->mdb_addr));
	    } else if (mp->mdb_str == bu_vls_message) {
		fprintf(stderr, "%8lx %6d bu_vls: \"%s\"\n",
			(long)(mp->mdb_addr), mp->mdb_len,
			((char *)mp->mdb_addr));
	    } else {
		fprintf(stderr, "%8lx %6d %s\n",
			(long)(mp->mdb_addr), mp->mdb_len,
			mp->mdb_str);
	    }
	    if (*ip != MDB_MAGIC) {
		fprintf(stderr, "\tCorrupted end marker was=x%lx\ts/b=x%x\n",
			*ip, MDB_MAGIC);
	    }
	}
    }


    if (bu_memq != BU_LIST_NULL) {
	fprintf(stderr, "memdebug queue\n Address Length Purpose\n");
	BU_LIST_EACH(bu_memq, mqp, struct memqdebug) {
	    if (BU_LIST_MAGIC_WRONG(&(mqp->q), MDB_MAGIC)
		|| BU_LIST_MAGIC_WRONG(&(mqp->m), MDB_MAGIC))
		bu_bomb("bu_prmem() malloc tracing queue corrupted!\n");
	    if (mqp->m.mdb_str == bu_strdup_message) {
		fprintf(stderr, "%8lx %6d bu_strdup: \"%s\"\n",
			(long)(mqp->m.mdb_addr), mqp->m.mdb_len,
			((char *)mqp->m.mdb_addr));
	    } else if (mqp->m.mdb_str == bu_vls_message) {
		fprintf(stderr, "%8lx %6d bu_vls: \"%s\"\n",
			(long)(mqp->m.mdb_addr), mqp->m.mdb_len,
			((char *)mqp->m.mdb_addr));
	    } else {
		fprintf(stderr, "%8lx %6d %s\n",
			(long)(mqp->m.mdb_addr), mqp->m.mdb_len,
			mqp->m.mdb_str);
	    }
	}
    }

    fprintf(stderr, "%lu allocation entries\n", (unsigned long)count);
}


int
bu_malloc_len_roundup(register int nbytes)
{
#if !defined(HAVE_CALTECH_MALLOC)
    return (nbytes);
#else
    static int pagesz;
    register int n;
    register int amt;

    if (pagesz == 0)
	pagesz = getpagesize();

#define OVERHEAD (4*sizeof(unsigned char) + \
			2*sizeof(unsigned short) + \
			sizeof(unsigned int))
    n = pagesz - OVERHEAD;
    if (nbytes <= n)
	return (n);
    amt = pagesz;

    while (nbytes > amt + n) {
	amt <<= 1;
    }
    return (amt-OVERHEAD-sizeof(int));
#endif
}


void
bu_ck_malloc_ptr(genptr_t ptr, const char *str)
{
    register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
    register long *ip;


    if (ptr == (char *)NULL) {
	fprintf(stderr, "bu_ck_malloc_ptr(x%lx, %s) null pointer\n\n", (long)ptr, str);
	bu_bomb("Goodbye");
    }

    if (bu_debug&BU_DEBUG_MEM_CHECK) {
	if (bu_memdebug == (struct memdebug *)0) {
	    fprintf(stderr, "bu_ck_malloc_ptr(x%lx, %s)  no memdebug table yet\n",
		    (long)ptr, str);
	    /* warning only -- the program is just getting started */
	    return;
	}

	for (; mp >= bu_memdebug; mp--) {
	    if (!mp->magic)  continue;
	    if (mp->magic != MDB_MAGIC)  bu_bomb("bu_ck_malloc_ptr() malloc tracing table corrupted!\n");
	    if (mp->mdb_len <= 0 || mp->mdb_addr != ptr)  continue;

	    /* Found the relevant entry */
	    ip = (long *)(((char *)ptr)+mp->mdb_len-sizeof(long));
	    if (*ip != MDB_MAGIC) {
		fprintf(stderr, "ERROR bu_ck_malloc_ptr(x%lx, %s) barrier word corrupted! was=x%lx s/b=x%x\n",
			(long)ptr, str, (long)*ip, MDB_MAGIC);
		bu_bomb("bu_ck_malloc_ptr\n");
	    }
	    return;		/* OK */
	}
	fprintf(stderr, "WARNING: bu_ck_malloc_ptr(x%lx, %s)\
	pointer not in table of allocated memory.\n", (long)ptr, str);
    } else if (bu_debug&BU_DEBUG_MEM_QCHECK) {
	struct memqdebug *mqp = (struct memqdebug *)ptr;
	if (BU_LIST_MAGIC_WRONG(&(mqp->q), MDB_MAGIC) || mqp->m.magic != MDB_MAGIC) {
	    fprintf(stderr, "WARNING: bu_ck_malloc_ptr(x%lx, %s)"
		    " memory corrupted.\n", (long)ptr, str);
	}
    }
}


int
bu_mem_barriercheck(void)
{
    register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
    register long *ip;

    if (bu_memdebug == (struct memdebug *)0) {
	fprintf(stderr, "bu_mem_barriercheck()  no memdebug table yet\n");
	return 0;
    }
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    for (; mp >= bu_memdebug; mp--) {
	if (!mp->magic)  continue;
	if (mp->magic != MDB_MAGIC) {
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    fprintf(stderr, "  mp->magic = x%lx, s/b=x%x\n", (long)(mp->magic), MDB_MAGIC);
	    bu_bomb("bu_mem_barriercheck() malloc tracing table corrupted!\n");
	}
	if (mp->mdb_len <= 0)  continue;
	ip = (long *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(long));
	if (*ip != MDB_MAGIC) {
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    fprintf(stderr, "ERROR bu_mem_barriercheck(x%lx, len=%d) barrier word corrupted!\n\tbarrier at x%lx was=x%lx s/b=x%x %s\n",
		    (long)mp->mdb_addr, mp->mdb_len,
		    (long)ip, *ip, MDB_MAGIC, mp->mdb_str);
	    return -1;	/* FAIL */
	}
    }
    bu_semaphore_release(BU_SEM_SYSCALL);
    return 0;			/* OK */
}

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
