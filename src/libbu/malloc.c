/*                        M A L L O C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
    size_t mdb_len;
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
static struct bu_list bu_memqhd = BU_LIST_INIT_ZERO;
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
_bu_memdebug_add(genptr_t ptr, size_t cnt, const char *str)
{
    register struct memdebug *mp = NULL;

top:
    bu_semaphore_acquire(BU_SEM_SYSCALL);

    if (LIKELY(bu_memdebug != NULL)) {
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
    if (LIKELY(bu_memdebug != NULL)) {
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
 * Check an entry against the memory debug table, based upon its address.
 */
HIDDEN struct memdebug *
_bu_memdebug_check(register genptr_t ptr, const char *str)
{
    register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
    register long *ip;

    if (bu_memdebug == (struct memdebug *)0) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "_bu_memdebug_check(%p, %s)  no memdebug table yet\n",
		ptr, str);
	bu_semaphore_release(BU_SEM_SYSCALL);
	return MEMDEBUG_NULL;
    }
    for (; mp >= bu_memdebug; mp--) {
	if (!mp->magic)  continue;
	if (mp->magic != MDB_MAGIC)  bu_bomb("_bu_memdebug_check() malloc tracing table corrupted!\n");
	if (mp->mdb_len == 0)  continue;
	if (mp->mdb_addr != ptr)  continue;
	ip = (long *)((char *)ptr+mp->mdb_len-sizeof(long));
	if (*ip != MDB_MAGIC) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    fprintf(stderr, "ERROR _bu_memdebug_check(%p, %s) %s, barrier word corrupted!\nbarrier at %p was=x%lx s/b=x%x, len=%llu\n",
		    ptr, str, mp->mdb_str,
		    (void *)ip, *ip, MDB_MAGIC, (unsigned long long)mp->mdb_len);
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    bu_bomb("_bu_memdebug_check() memory corruption\n");
	}
	return mp;		/* OK */
    }
    return MEMDEBUG_NULL;
}


extern int bu_bomb_failsafe_init();

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
_bu_alloc(alloc_t type, size_t cnt, size_t sz, const char *str)
{
    register genptr_t ptr = 0;
    register size_t size = sz;
    const size_t MINSIZE = sizeof(uint32_t) > sizeof(intptr_t) ? sizeof(uint32_t) : sizeof(intptr_t);

    static int failsafe_init = 0;

    /* bu_bomb hook to recover from memory problems */
    if (UNLIKELY(!failsafe_init)) {
	failsafe_init = bu_bomb_failsafe_init();
    }

    if (UNLIKELY(cnt == 0 || sz == 0)) {
	fprintf(stderr, "ERROR: _bu_alloc size=0 (cnt=%llu, sz=%llu) %s\n",
		(unsigned long long)cnt, (unsigned long long)sz, str);
	bu_bomb("ERROR: bu_malloc(0)\n");
    }

    /* minimum allocation size, always big enough to stash a pointer.
     * that said, if you're anywhere near this size, you're probably
     * doing something wrong.
     */
    if (UNLIKELY(size < MINSIZE)) {
	size = MINSIZE;
    }

    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK)) {
	/* Pad, plus full int for magic number */
	size = (size + 2*sizeof(long) - 1) & (~(sizeof(long) - 1));
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	size = (size + 2*sizeof(struct memqdebug) - 1) & (~(sizeof(struct memqdebug) - 1));
    }

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_SYSCALL);
#endif

    switch (type) {
	case MALLOC:
	    ptr = malloc(cnt*size);
	    break;
	case CALLOC:
	    /* if we're debugging, we need a slightly larger
	     * allocation size for debug tracking.
	     */
	    if (UNLIKELY(bu_debug&(BU_DEBUG_MEM_CHECK|BU_DEBUG_MEM_QCHECK))) {
		ptr = malloc(cnt*size);
		memset(ptr, 0, cnt*size);
	    } else {
		ptr = calloc(cnt, size);
	    }
	    break;
	default:
	    bu_bomb("ERROR: _bu_alloc with unknown type\n");
    }

    if (UNLIKELY(ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG)) {
	fprintf(stderr, "NULL malloc(%llu) %s\n", (unsigned long long)(cnt*size), str);
    }
#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_release(BU_SEM_SYSCALL);
#endif

    if (UNLIKELY(ptr==(char *)0)) {
	fprintf(stderr, "bu_malloc: Insufficient memory available\n");
	bu_bomb("bu_malloc: malloc failure");
    }
    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK)) {
	_bu_memdebug_add(ptr, cnt*size, str);

	/* Install a barrier word at the end of the dynamic arena */
	/* Correct location depends on 'cnt*size' being rounded up, above */

	*((long *)(((char *)ptr) + (cnt*size) - sizeof(long))) = MDB_MAGIC;
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	struct memqdebug *mp = (struct memqdebug *)ptr;
	ptr = (genptr_t)(((struct memqdebug *)ptr)+1);
	mp->m.magic = MDB_MAGIC;
	mp->m.mdb_addr = ptr;
	mp->m.mdb_len = cnt*size;
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
    return ptr;
}


genptr_t
bu_malloc(size_t size, const char *str)
{
    return _bu_alloc(MALLOC, 1, size, str);
}


genptr_t
bu_calloc(size_t nelem, size_t elsize, const char *str)
{
    return _bu_alloc(CALLOC, nelem, elsize, str);
}


void
bu_free(genptr_t ptr, const char *str)
{
    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_LOG)) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "%p free          %s\n", ptr, str);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (UNLIKELY(ptr == (char *)0 || ptr == (char *)(-1L))) {
	fprintf(stderr, "%p free ERROR %s\n", ptr, str);
	return;
    }
    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK)) {
	struct memdebug *mp;
	mp = _bu_memdebug_check(ptr, str);
	if (UNLIKELY(mp == MEMDEBUG_NULL)) {
	    fprintf(stderr, "ERROR bu_free(%p, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n", ptr, str);
	} else {
	    mp->mdb_len = 0;	/* successful delete */
	}
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	struct memqdebug *mp = ((struct memqdebug *)ptr)-1;
	if (UNLIKELY(BU_LIST_MAGIC_WRONG(&(mp->q), MDB_MAGIC))) {
	    fprintf(stderr, "ERROR bu_free(%p, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n", ptr, str);
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

    /* Here we wipe out the first four bytes before the actual free()
     * as a basic memory safeguard.  This should wipe out any magic
     * number in structures and provide a distinct memory signature if
     * the address happens to be accessed via some other pointer or
     * the program crashes.  While we're not guaranteed anything after
     * free(), some implementations leave the zapped value intact.
     */
    *((uint32_t *)ptr) = 0xFFFFFFFF;	/* zappo! */

    free(ptr);
#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_release(BU_SEM_SYSCALL);
#endif
    bu_n_free++;
}


genptr_t
bu_realloc(register genptr_t ptr, size_t siz, const char *str)
{
    struct memdebug *mp=NULL;
    genptr_t original_ptr;
    const size_t MINSIZE = sizeof(uint32_t) > sizeof(intptr_t) ? sizeof(uint32_t) : sizeof(intptr_t);

    /* If bu_realloc receives a NULL pointer and zero size then bomb
     * because the behavior of realloc is undefined for these inputs.
     */
    if (UNLIKELY(ptr == NULL && siz == 0)) {
	bu_bomb("bu_realloc(): invalid input, NULL pointer and zero size\n");
    }

    /* If bu_realloc receives a NULL pointer and non-zero size then
     * allocate new memory.
     */
    if (UNLIKELY(ptr == NULL && siz > 0)) {
	return bu_malloc(siz, str);
    }

    /* If bu_realloc receives a non-NULL pointer and zero size then
     * free the memory.  Instead of returning NULL, though, the
     * standard says we can return a small allocation suitable for
     * passing to bu_free().  Do that so we can maintain are LIBBU
     * guarantee of worry-free memory management.
     */
    if (UNLIKELY(ptr != NULL && siz == 0)) {
	bu_free(ptr, str);
	return bu_malloc(MINSIZE, str);
    }

    /* If the new allocation size is smaller than the minimum size
     * to store a pointer then set the size to this minimum size.
     * This is necessary so that the function bu_free can place a
     * value in the memory before it is freed. The size allocated
     * needs to be large enough to hold this value.
     */
    if (UNLIKELY(siz < MINSIZE)) {
        siz = MINSIZE;
    }

    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK)) {
	mp = _bu_memdebug_check(ptr, str);
	if (UNLIKELY(mp == MEMDEBUG_NULL)) {
	    fprintf(stderr, "%p realloc%6d %s ** barrier check failure\n",
		    ptr, (int)siz, str);
	}
	/* Pad, plus full long for magic number */
	siz = (siz+2*sizeof(long)-1)&(~(sizeof(long)-1));
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	struct memqdebug *mqp = ((struct memqdebug *)ptr)-1;

	siz = (siz + 2*sizeof(struct memqdebug) - 1)
	    &(~(sizeof(struct memqdebug)-1));

	if (UNLIKELY(BU_LIST_MAGIC_WRONG(&(mqp->q), MDB_MAGIC))) {
	    fprintf(stderr, "ERROR bu_realloc(%p, %s) pointer bad, "
		    "or not allocated with bu_malloc!  Ignored.\n",
		    ptr, str);
	    /*
	     * Since we're ignoring this, at least return the pointer
	     * that was passed in.  Standard says behavior is
	     * undefined when reallocating memory not allocated with
	     * the matching allocation routines (i.e., bu_malloc() or
	     * bu_calloc() in our situation), so we are fair game to
	     * just return the pointer we were given.
	     */
	    return ptr;
	}
	ptr = (genptr_t)mqp;
	BU_LIST_DEQUEUE(&(mqp->q));
    }

    original_ptr = ptr;

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_SYSCALL);
#endif
    ptr = realloc(ptr, siz);
#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_release(BU_SEM_SYSCALL);
#endif

    /* If realloc returns NULL then it failed to allocate the
     * requested memory and we need to bomb.
     */
    if (UNLIKELY(!ptr)) {
	fprintf(stderr, "bu_realloc(): unable to allocate requested memory of size %ld, %s\n", (long int)siz, str);
	bu_bomb("bu_realloc(): unable to allocate requested memory.\n");
    }

    if (UNLIKELY(ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG)) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	if (ptr == original_ptr) {
	    fprintf(stderr, "%p realloc%6d %s [grew in place]\n",
		    ptr, (int)siz, str);
	} else {
	    fprintf(stderr, "%p realloc%6d %s [moved from %p]\n",
		    ptr, (int)siz, str, original_ptr);
	}

	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK && ptr)) {
	/* Even if ptr didn't change, need to update siz & barrier */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	mp->mdb_addr = ptr;
	mp->mdb_len = siz;

	/* Install a barrier word at the new end of the dynamic
	 * arena. Correct location depends on 'siz' being rounded up,
	 * above.
	 */
	*((long *)(((char *)ptr)+siz-sizeof(long))) = MDB_MAGIC;
	bu_semaphore_release(BU_SEM_SYSCALL);
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK && ptr)) {
	struct memqdebug *mqp;
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	mqp = (struct memqdebug *)ptr;
	ptr = (genptr_t)(((struct memqdebug *)ptr)+1);
	mqp->m.magic = MDB_MAGIC;
	mqp->m.mdb_addr = ptr;
	mqp->m.mdb_len = siz;
	mqp->m.mdb_str = str;
	BU_ASSERT(bu_memq != BU_LIST_NULL);
	BU_LIST_APPEND(bu_memq, &(mqp->q));
	BU_LIST_MAGIC_SET(&(mqp->q), MDB_MAGIC);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    bu_n_realloc++;
    return ptr;
}


void
bu_prmem(const char *str)
{
    register struct memdebug *mp;
    register struct memqdebug *mqp;
    register long *ip;
    register size_t count = 0;

    fprintf(stderr, "\nbu_prmem(): dynamic memory use (%s)\n", str);
    if (UNLIKELY((bu_debug&(BU_DEBUG_MEM_CHECK|BU_DEBUG_MEM_QCHECK)) == 0)) {
	fprintf(stderr, "\tMemory debugging is now OFF\n");
    }
    fprintf(stderr, " Address Length Purpose\n");
    if (bu_memdebug_len > 0) {
	mp = &bu_memdebug[bu_memdebug_len-1];
	for (; mp >= bu_memdebug; mp--) {
	    if (!mp->magic)  continue;
	    if (mp->magic != MDB_MAGIC)  bu_bomb("_bu_memdebug_check() malloc tracing table corrupted!\n");
	    if (mp->mdb_len == 0)  continue;

	    count++;
	    ip = (long *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(long));
	    if (mp->mdb_str == bu_strdup_message) {
		fprintf(stderr, "%p %llu bu_strdup: \"%s\"\n",
			mp->mdb_addr, (unsigned long long)mp->mdb_len,
			((char *)mp->mdb_addr));
	    } else if (mp->mdb_str == bu_vls_message) {
		fprintf(stderr, "%p %llu bu_vls: \"%s\"\n",
			mp->mdb_addr, (unsigned long long)mp->mdb_len,
			((char *)mp->mdb_addr));
	    } else {
		fprintf(stderr, "%p %llu %s\n",
			mp->mdb_addr, (unsigned long long)mp->mdb_len,
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
		fprintf(stderr, "%p %llu bu_strdup: \"%s\"\n",
			mqp->m.mdb_addr, (unsigned long long)mqp->m.mdb_len,
			((char *)mqp->m.mdb_addr));
	    } else if (mqp->m.mdb_str == bu_vls_message) {
		fprintf(stderr, "%p %llu bu_vls: \"%s\"\n",
			mqp->m.mdb_addr, (unsigned long long)mqp->m.mdb_len,
			((char *)mqp->m.mdb_addr));
	    } else {
		fprintf(stderr, "%p %llu %s\n",
			mqp->m.mdb_addr, (unsigned long long)mqp->m.mdb_len,
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
    return nbytes;
#else
    static int pagesz;
    register int n;
    register int amt;

    if (pagesz == 0)
	pagesz = getpagesize();

#define OVERHEAD (4*sizeof(unsigned char) +	\
		  2*sizeof(unsigned short) +	\
		  sizeof(unsigned int))
    n = pagesz - OVERHEAD;
    if (nbytes <= n)
	return n;
    amt = pagesz;

    while (nbytes > amt + n) {
	amt <<= 1;
    }
    return amt-OVERHEAD-sizeof(int);
#endif
}


void
bu_ck_malloc_ptr(genptr_t ptr, const char *str)
{
    register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
    register long *ip;


    if (UNLIKELY(ptr == (char *)NULL)) {
	fprintf(stderr, "bu_ck_malloc_ptr(%p, %s) null pointer\n\n", ptr, str);
	bu_bomb("Goodbye");
    }

    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK)) {
	if (bu_memdebug == (struct memdebug *)0) {
	    fprintf(stderr, "bu_ck_malloc_ptr(%p, %s)  no memdebug table yet\n",
		    ptr, str);
	    /* warning only -- the program is just getting started */
	    return;
	}

	for (; mp >= bu_memdebug; mp--) {
	    if (!mp->magic)  continue;
	    if (mp->magic != MDB_MAGIC)  bu_bomb("bu_ck_malloc_ptr() malloc tracing table corrupted!\n");
	    if (mp->mdb_len == 0 || mp->mdb_addr != ptr)  continue;

	    /* Found the relevant entry */
	    ip = (long *)(((char *)ptr)+mp->mdb_len-sizeof(long));
	    if (*ip != MDB_MAGIC) {
		fprintf(stderr, "ERROR bu_ck_malloc_ptr(%p, %s) barrier word corrupted! was=x%lx s/b=x%x\n",
			ptr, str, (long)*ip, MDB_MAGIC);
		bu_bomb("bu_ck_malloc_ptr\n");
	    }
	    return;		/* OK */
	}
	fprintf(stderr, "WARNING: bu_ck_malloc_ptr(%p, %s)\
	pointer not in table of allocated memory.\n", ptr, str);
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	struct memqdebug *mqp = (struct memqdebug *)ptr;
	if (UNLIKELY(BU_LIST_MAGIC_WRONG(&(mqp->q), MDB_MAGIC) || mqp->m.magic != MDB_MAGIC)) {
	    fprintf(stderr, "WARNING: bu_ck_malloc_ptr(%p, %s)"
		    " memory corrupted.\n", ptr, str);
	}
    }
}


int
bu_mem_barriercheck(void)
{
    register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
    register long *ip;

    if (UNLIKELY(bu_memdebug == (struct memdebug *)0)) {
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
	if (mp->mdb_len == 0)  continue;
	ip = (long *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(long));
	if (*ip != MDB_MAGIC) {
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    fprintf(stderr, "ERROR bu_mem_barriercheck(%p, len=%llu) barrier word corrupted!\n\tbarrier at %p was=x%lx s/b=x%x %s\n",
		    mp->mdb_addr, (unsigned long long)mp->mdb_len,
		    (void *)ip, *ip, MDB_MAGIC, mp->mdb_str);
	    return -1;	/* FAIL */
	}
    }
    bu_semaphore_release(BU_SEM_SYSCALL);
    return 0;			/* OK */
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
