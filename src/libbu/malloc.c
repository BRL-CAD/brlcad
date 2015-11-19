/*                        M A L L O C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
#ifdef HAVE_SYS_IPC_H
#  include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_SHM_H
#  include <sys/shm.h>
#endif
#include "errno.h"
#include "bio.h"

#include "bu/debug.h"
#include "bu/list.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"

/**
 * this controls whether to semaphore protect malloc calls
 *
 * FIXME: we really don't need to do this any more, especially if
 * compiling against glibc+pthreads.  Windows, however, needs might
 * need some build flag encouragement.
 */
#define MALLOC_NOT_MP_SAFE 1


/**
 * used by the memory allocation routines going through alloc() to
 * indicate whether allocated memory should be zero'd.
 */
typedef enum {
    MALLOC,
    CALLOC
} alloc_t;


#define MDB_MAGIC 0x12348969
struct memdebug {
    uint32_t magic;		/* corruption can be everywhere */
    void *mdb_addr;
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
 * Add another entry to the memory debug table
 */
HIDDEN void
memdebug_add(void *ptr, size_t cnt, const char *str)
{
    register struct memdebug *mp = NULL;

top:
    bu_semaphore_acquire(BU_SEM_MALLOC);

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
	    if (mp->mdb_len > 0) continue;
	    mp->magic = MDB_MAGIC;
	    mp->mdb_addr = ptr;
	    mp->mdb_len = cnt;
	    mp->mdb_str = str;
	    bu_memdebug_lowat = mp-1;
	    bu_semaphore_release(BU_SEM_MALLOC);
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
	    bu_bomb("memdebug_add() malloc failure\n");
    } else {
	size_t old_len = bu_memdebug_len;
	bu_memdebug_len *= 16;
	bu_memdebug = (struct memdebug *)realloc((char *)bu_memdebug, sizeof(struct memdebug) * bu_memdebug_len);
	if (bu_memdebug == (struct memdebug *)0)
	    bu_bomb("memdebug_add() malloc failure\n");
	memset((char *)&bu_memdebug[old_len], 0,
	       (bu_memdebug_len-old_len) * sizeof(struct memdebug));
    }
    bu_semaphore_release(BU_SEM_MALLOC);

    goto top;
}


/**
 * Check an entry against the memory debug table, based upon its address.
 */
HIDDEN struct memdebug *
memdebug_check(register void *ptr, const char *str)
{
    register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
    register uint32_t *ip;

    if (bu_memdebug == (struct memdebug *)0) {
	bu_semaphore_acquire(BU_SEM_MALLOC);
	fprintf(stderr, "memdebug_check(%p, %s) no memdebug table yet\n",
		ptr, str);
	bu_semaphore_release(BU_SEM_MALLOC);
	return MEMDEBUG_NULL;
    }
    for (; mp >= bu_memdebug; mp--) {
	if (!mp->magic) continue;
	if (mp->magic != MDB_MAGIC) bu_bomb("memdebug_check() malloc tracing table corrupted!\n");
	if (mp->mdb_len == 0) continue;
	if (mp->mdb_addr != ptr) continue;
	ip = (uint32_t *)((char *)ptr+mp->mdb_len-sizeof(uint32_t));
	if (*ip != MDB_MAGIC) {
	    bu_semaphore_acquire(BU_SEM_MALLOC);
	    fprintf(stderr, "ERROR memdebug_check(%p, %s) %s, barrier word corrupted!\nbarrier at %p was=x%lx s/b=x%x, len=%llu\n",
		    ptr, str, mp->mdb_str,
		    (void *)ip, (unsigned long)*ip, MDB_MAGIC, (unsigned long long)mp->mdb_len);
	    bu_semaphore_release(BU_SEM_MALLOC);
	    bu_bomb("memdebug_check() memory corruption\n");
	}
	return mp;		/* OK */
    }
    return MEMDEBUG_NULL;
}


extern int bu_bomb_failsafe_init(void);

/**
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
HIDDEN void *
alloc(alloc_t type, size_t cnt, size_t sz, const char *str)
{
    void *ptr = 0;
    register size_t size = sz;
    const size_t MINSIZE = sizeof(uint32_t) > sizeof(intptr_t) ? sizeof(uint32_t) : sizeof(intptr_t);

    static int failsafe_init = 0;

    const char *nul = "(null)";
    if (!str)
	str = nul;

    /* bu_bomb hook to recover from memory problems */
    if (UNLIKELY(!failsafe_init)) {
	failsafe_init = bu_bomb_failsafe_init();
    }

    if (UNLIKELY(cnt == 0 || sz == 0)) {
	fprintf(stderr, "ERROR: alloc size=0 (cnt=%llu, sz=%llu) %s\n",
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

    /* if we're debugging, we need a slightly larger allocation size
     * for debug tracking.
     */
    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK)) {
	/* Pad, plus full uint32_t for magic number */
	size = (size + 2*sizeof(uint32_t) - 1) & (~(sizeof(uint32_t) - 1));
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	size = (size + 2*sizeof(struct memqdebug) - 1) & (~(sizeof(struct memqdebug) - 1));
    }

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_MALLOC);
#endif

/* align allocations to what address multiple */
#define ALIGN 8

    switch (type) {
	case MALLOC:
#ifdef HAVE_POSIX_MEMALIGN
	    if (posix_memalign(&ptr, ALIGN, cnt*size))
		ptr = NULL;
#else
	    ptr = malloc(cnt*size);
#endif
	    break;
	case CALLOC:
#ifdef HAVE_POSIX_MEMALIGN
	    if (posix_memalign(&ptr, ALIGN, cnt*size))
		ptr = NULL;
	    else
		memset(ptr, 0, cnt*size);
#else
	    ptr = calloc(cnt, size);
#endif
	    break;
	default:
	    bu_bomb("ERROR: alloc with unknown type\n");
    }

    if (UNLIKELY(ptr==NULL || bu_debug&BU_DEBUG_MEM_LOG)) {
	fprintf(stderr, "NULL malloc(%llu) %s\n", (unsigned long long)(cnt*size), str);
    }

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_release(BU_SEM_MALLOC);
#endif

    if (UNLIKELY(ptr==(char *)0)) {
	fprintf(stderr, "bu_malloc: Insufficient memory available\n");
	bu_bomb("bu_malloc: malloc failure");
    }
    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK)) {
	memdebug_add(ptr, cnt*size, str);

	/* Install a barrier word at the end of the dynamic arena */
	/* Correct location depends on 'cnt*size' being rounded up, above */

	*((uint32_t *)(((char *)ptr) + (cnt*size) - sizeof(uint32_t))) = MDB_MAGIC;
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	struct memqdebug *mp = (struct memqdebug *)ptr;
	ptr = (void *)(((struct memqdebug *)ptr)+1);
	mp->m.magic = MDB_MAGIC;
	mp->m.mdb_addr = ptr;
	mp->m.mdb_len = cnt*size;
	mp->m.mdb_str = str;
	bu_semaphore_acquire(BU_SEM_MALLOC);
	if (bu_memq == BU_LIST_NULL) {
	    bu_memq = &bu_memqhd;
	    BU_LIST_INIT(bu_memq);
	}
	BU_LIST_APPEND(bu_memq, &(mp->q));
	BU_LIST_MAGIC_SET(&(mp->q), MDB_MAGIC);
	bu_semaphore_release(BU_SEM_MALLOC);
    }
    bu_n_malloc++;
    return ptr;
}


void *
bu_malloc(size_t size, const char *str)
{
    return alloc(MALLOC, 1, size, str);
}


void *
bu_calloc(size_t nelem, size_t elsize, const char *str)
{
    return alloc(CALLOC, nelem, elsize, str);
}


void
bu_free(void *ptr, const char *str)
{
    const char *nul = "(null)";
    if (!str)
	str = nul;

    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_LOG)) {
	fprintf(stderr, "%p free          %s\n", ptr, str);
    }

    if (UNLIKELY(ptr == (char *)0 || ptr == (char *)(-1L))) {
	fprintf(stderr, "%p free ERROR %s\n", ptr, str);
	return;
    }
    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK)) {
	struct memdebug *mp;
	mp = memdebug_check(ptr, str);
	if (UNLIKELY(mp == MEMDEBUG_NULL)) {
	    fprintf(stderr, "ERROR bu_free(%p, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n", ptr, str);
	} else {
	    mp->mdb_len = 0;	/* successful delete */
	}
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	struct memqdebug *mp = ((struct memqdebug *)ptr)-1;
	if (UNLIKELY(!BU_LIST_MAGIC_EQUAL(&(mp->q), MDB_MAGIC))) {
	    fprintf(stderr, "ERROR bu_free(%p, %s) pointer bad, or not allocated with bu_malloc!  Ignored.\n", ptr, str);
	} else {
	    ptr = (void *)mp;
	    bu_semaphore_acquire(BU_SEM_MALLOC);
	    BU_LIST_DEQUEUE(&(mp->q));
	    bu_semaphore_release(BU_SEM_MALLOC);
	}
    }

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_MALLOC);
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
    bu_semaphore_release(BU_SEM_MALLOC);
#endif
}


void *
bu_realloc(register void *ptr, size_t siz, const char *str)
{
    struct memdebug *mp=NULL;
    void *original_ptr;
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
     * passing to bu_free().  Do that so we can maintain our LIBBU
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
	mp = memdebug_check(ptr, str);
	if (UNLIKELY(mp == MEMDEBUG_NULL)) {
	    fprintf(stderr, "%p realloc%6d %s ** barrier check failure\n",
		    ptr, (int)siz, str);
	}
	/* Pad, plus full uint32_t for magic number */
	siz = (siz+2*sizeof(uint32_t)-1)&(~(sizeof(uint32_t)-1));
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	struct memqdebug *mqp = ((struct memqdebug *)ptr)-1;

	siz = (siz + 2*sizeof(struct memqdebug) - 1)
	    &(~(sizeof(struct memqdebug)-1));

	if (UNLIKELY(!BU_LIST_MAGIC_EQUAL(&(mqp->q), MDB_MAGIC))) {
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
	ptr = (void *)mqp;
	BU_LIST_DEQUEUE(&(mqp->q));
    }

    original_ptr = ptr;

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_MALLOC);
#endif
    ptr = realloc(ptr, siz);
#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_release(BU_SEM_MALLOC);
#endif

    /* If realloc returns NULL then it failed to allocate the
     * requested memory and we need to bomb.
     */
    if (UNLIKELY(!ptr)) {
	fprintf(stderr, "bu_realloc(): unable to allocate requested memory of size %ld, %s\n", (long int)siz, str);
	bu_bomb("bu_realloc(): unable to allocate requested memory.\n");
    }

    if (UNLIKELY(ptr==(char *)0 || bu_debug&BU_DEBUG_MEM_LOG)) {
	if (ptr == original_ptr) {
	    fprintf(stderr, "%p realloc%6d %s [grew in place]\n",
		    ptr, (int)siz, str);
	} else {
	    /* Not using %p to print original_ptr's pointer value here in order
	     * to avoid clang static analyzer complaining about use of memory
	     * after memory is freed.  If/when clang can recognize that this is
	     * not an actual problem, switch it back to the %p form */
	    fprintf(stderr, "%p realloc%6d %s [moved from 0x%lx]\n",
		    ptr, (int)siz, str, (uintptr_t)original_ptr);
	}
    }

    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK && ptr)) {
	/* Even if ptr didn't change, need to update siz & barrier */
	bu_semaphore_acquire(BU_SEM_MALLOC);

	/* If memdebug_check returned MEMDEBUG_NULL, we can't do these
	 * assignments.  In that situation the error condition was already
	 * reported earlier, so just skip the assignments here and continue */
	if (mp != MEMDEBUG_NULL) {
	    mp->mdb_addr = ptr;
	    mp->mdb_len = siz;
	}

	/* Install a barrier word at the new end of the dynamic
	 * arena. Correct location depends on 'siz' being rounded up,
	 * above.
	 */
	*((uint32_t *)(((char *)ptr)+siz-sizeof(uint32_t))) = MDB_MAGIC;
	bu_semaphore_release(BU_SEM_MALLOC);
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK && ptr)) {
	struct memqdebug *mqp;
	bu_semaphore_acquire(BU_SEM_MALLOC);
	mqp = (struct memqdebug *)ptr;
	ptr = (void *)(((struct memqdebug *)ptr)+1);
	mqp->m.magic = MDB_MAGIC;
	mqp->m.mdb_addr = ptr;
	mqp->m.mdb_len = siz;
	mqp->m.mdb_str = str;
	BU_ASSERT(bu_memq != BU_LIST_NULL);
	BU_LIST_APPEND(bu_memq, &(mqp->q));
	BU_LIST_MAGIC_SET(&(mqp->q), MDB_MAGIC);
	bu_semaphore_release(BU_SEM_MALLOC);
    }
    bu_n_realloc++;
    return ptr;
}

/* bu_reallocarray is based off of OpenBSD's reallocarray.c, v 1.3 2015/09/13
 *
 * Copyright (c) 2008 Otto Moerbeek <otto@drijf.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW	((size_t)1 << (sizeof(size_t) * 4))
void *
bu_reallocarray(register void *optr, size_t nmemb, size_t size, const char *str)
{
    if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) && nmemb > 0 && SIZE_MAX / nmemb < size) {
	return NULL;
    }
    return bu_realloc(optr, size * nmemb, str);
}


void
bu_prmem(const char *str)
{
    register struct memdebug *mp;
    register struct memqdebug *mqp;
    register uint32_t *ip;
    register size_t count = 0;

    fprintf(stderr, "\nbu_prmem(): dynamic memory use (%s)\n", str);
    if (UNLIKELY((bu_debug&(BU_DEBUG_MEM_CHECK|BU_DEBUG_MEM_QCHECK)) == 0)) {
	fprintf(stderr, "\tMemory debugging is now OFF\n");
    }
    fprintf(stderr, " Address Length Purpose\n");
    if (bu_memdebug_len > 0) {
	mp = &bu_memdebug[bu_memdebug_len-1];
	for (; mp >= bu_memdebug; mp--) {
	    if (!mp->magic) continue;
	    if (mp->magic != MDB_MAGIC) bu_bomb("memdebug_check() malloc tracing table corrupted!\n");
	    if (mp->mdb_len == 0) continue;

	    count++;
	    ip = (uint32_t *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(uint32_t));
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
			(unsigned long)*ip, MDB_MAGIC);
	    }
	}
    }


    if (bu_memq != BU_LIST_NULL) {
	fprintf(stderr, "memdebug queue\n Address Length Purpose\n");
	BU_LIST_EACH(bu_memq, mqp, struct memqdebug) {
	    if (!BU_LIST_MAGIC_EQUAL(&(mqp->q), MDB_MAGIC)
		|| !BU_LIST_MAGIC_EQUAL(&(mqp->m), MDB_MAGIC))
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
bu_ck_malloc_ptr(void *ptr, const char *str)
{
    register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
    register uint32_t *ip;


    if (UNLIKELY(ptr == (char *)NULL)) {
	fprintf(stderr, "bu_ck_malloc_ptr(%p, %s) null pointer\n\n", ptr, str);
	bu_bomb("Goodbye");
    }

    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_CHECK)) {
	if (bu_memdebug == (struct memdebug *)0) {
	    fprintf(stderr, "bu_ck_malloc_ptr(%p, %s) no memdebug table yet\n",
		    ptr, str);
	    /* warning only -- the program is just getting started */
	    return;
	}

	for (; mp >= bu_memdebug; mp--) {
	    if (!mp->magic) continue;
	    if (mp->magic != MDB_MAGIC) bu_bomb("bu_ck_malloc_ptr() malloc tracing table corrupted!\n");
	    if (mp->mdb_len == 0 || mp->mdb_addr != ptr) continue;

	    /* Found the relevant entry */
	    ip = (uint32_t *)(((char *)ptr)+mp->mdb_len-sizeof(uint32_t));
	    if (*ip != MDB_MAGIC) {
		fprintf(stderr, "ERROR bu_ck_malloc_ptr(%p, %s) barrier word corrupted! was=x%lx s/b=x%x\n",
			ptr, str, (unsigned long)*ip, MDB_MAGIC);
		bu_bomb("bu_ck_malloc_ptr\n");
	    }
	    return;		/* OK */
	}
	fprintf(stderr, "WARNING: bu_ck_malloc_ptr(%p, %s)\
	pointer not in table of allocated memory.\n", ptr, str);
    } else if (UNLIKELY(bu_debug&BU_DEBUG_MEM_QCHECK)) {
	struct memqdebug *mqp = (struct memqdebug *)ptr;
	if (UNLIKELY(!BU_LIST_MAGIC_EQUAL(&(mqp->q), MDB_MAGIC) || mqp->m.magic != MDB_MAGIC)) {
	    fprintf(stderr, "WARNING: bu_ck_malloc_ptr(%p, %s)"
		    " memory corrupted.\n", ptr, str);
	}
    }
}


int
bu_mem_barriercheck(void)
{
    register struct memdebug *mp = &bu_memdebug[bu_memdebug_len-1];
    register uint32_t *ip;

    if (UNLIKELY(bu_memdebug == (struct memdebug *)0)) {
	fprintf(stderr, "bu_mem_barriercheck() no memdebug table yet\n");
	return 0;
    }
    bu_semaphore_acquire(BU_SEM_MALLOC);
    for (; mp >= bu_memdebug; mp--) {
	if (!mp->magic) continue;
	if (mp->magic != MDB_MAGIC) {
	    bu_semaphore_release(BU_SEM_MALLOC);
	    fprintf(stderr, "  mp->magic = x%lx, s/b=x%x\n", (unsigned long)(mp->magic), MDB_MAGIC);
	    bu_bomb("bu_mem_barriercheck() malloc tracing table corrupted!\n");
	}
	if (mp->mdb_len == 0) continue;
	ip = (uint32_t *)(((char *)mp->mdb_addr)+mp->mdb_len-sizeof(uint32_t));
	if (*ip != MDB_MAGIC) {
	    bu_semaphore_release(BU_SEM_MALLOC);
	    fprintf(stderr, "ERROR bu_mem_barriercheck(%p, len=%llu) barrier word corrupted!\n\tbarrier at %p was=x%lx s/b=x%x %s\n",
		    mp->mdb_addr, (unsigned long long)mp->mdb_len,
		    (void *)ip, (unsigned long)*ip, MDB_MAGIC, mp->mdb_str);
	    return -1;	/* FAIL */
	}
    }
    bu_semaphore_release(BU_SEM_MALLOC);
    return 0;			/* OK */
}


int
#ifdef HAVE_SYS_SHM_H
bu_shmget(int *shmid, char **shared_memory, int key, size_t size)
#else
bu_shmget(int *UNUSED(shmid), char **UNUSED(shared_memory), int UNUSED(key), size_t UNUSED(size))
#endif
{
    int ret = 1;
#ifdef HAVE_SYS_SHM_H
    int shmsize;
    long psize = sysconf(_SC_PAGESIZE);
    int flags = IPC_CREAT|0666;

    ret = 0;
    errno = 0;

    /*
       make more portable
       shmsize = (size + getpagesize()-1) & ~(getpagesize()-1);
       */
    shmsize = (size + psize - 1) & ~(psize - 1);

    /* First try to attach to an existing one */
    if (((*shmid) = shmget(key, shmsize, 0)) < 0) {
	/* No existing one, create a new one */
	if (((*shmid) = shmget(key, shmsize, flags)) < 0) {
	    printf("bu_shmget failed, errno=%d\n", errno);
	    return 1;
	}
	ret = -1;
    }

    /* WWW this is unnecessary in this version? */
    /* Open the segment Read/Write */
    /* This gets mapped to a high address on some platforms, so no problem. */
    if (((*shared_memory) = (char *)shmat((*shmid), 0, 0)) == (char *)(-1L)) {
	printf("errno=%d\n", errno);
	return 1;
    }

#endif
    return ret;
}


struct bu_pool *
bu_pool_create(size_t block_size)
{
    struct bu_pool *pool;

    pool = (struct bu_pool*)bu_malloc(sizeof(struct bu_pool), "bu_pool_create");
    pool->block_size = block_size;
    pool->block_pos = 0;
    pool->alloc_size = 0;
    pool->block = NULL;
    return pool;
}

void *
bu_pool_alloc(struct bu_pool *pool, size_t nelem, size_t elsize)
{
    const size_t n_bytes = nelem * elsize;
    void *ret;

    if (pool->block_pos + n_bytes > pool->alloc_size) {
    	pool->alloc_size += (n_bytes < pool->block_size ? pool->block_size : n_bytes);
	pool->block = (uint8_t*)bu_realloc(pool->block, pool->alloc_size, "bu_pool_alloc");
    }

    ret = pool->block + pool->block_pos;
    pool->block_pos += n_bytes;
    return ret;
}

void
bu_pool_delete(struct bu_pool *pool)
{
    bu_free(pool->block, "bu_pool_delete");
    bu_free(pool, "bu_pool_delete");
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
