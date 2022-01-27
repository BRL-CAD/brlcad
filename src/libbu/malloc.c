/*                        M A L L O C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
#include <limits.h>
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
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/exit.h"
#include "bu/log.h"

/* strict c89 doesn't declare posix_memalign */
#ifndef HAVE_DECL_POSIX_MEMALIGN
extern int posix_memalign(void **, size_t, size_t);
#endif

int BU_SEM_MALLOC;


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


/* non-published globals */
extern const char bu_vls_message[];
extern const char bu_strdup_message[];

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

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_MALLOC);
#endif

/* align allocations to what address multiple */
#define ALIGNMENT 8

    switch (type) {
	case MALLOC:
#ifdef HAVE_POSIX_MEMALIGN
	    if (posix_memalign(&ptr, ALIGNMENT, cnt*size))
		ptr = NULL;
#else
	    ptr = malloc(cnt*size);
#endif
	    break;
	case CALLOC:
#ifdef HAVE_POSIX_MEMALIGN
	    if (posix_memalign(&ptr, ALIGNMENT, cnt*size))
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

    bu_n_malloc++;

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_release(BU_SEM_MALLOC);
#endif

    if (UNLIKELY(ptr==(char *)0)) {
	fprintf(stderr, "bu_malloc: Insufficient memory available\n");
	bu_bomb("bu_malloc: malloc failure");
    }

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

    /* silently ignore NULL pointers */
    if (UNLIKELY(ptr == (char *)0)) {
	return;
    }

    /* noisily report "marked" pointers */
    if (ptr == (char *)(-1L)) {
	fprintf(stderr, "%p free ERROR %s\n", ptr, str);
	return;
    }

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_MALLOC);
#endif

    /* Here we intentionally wipe out the first four bytes before the
     * actual free() as a basic memory safeguard.  While we're not
     * guaranteed anything after free(), some implementations leave
     * the zapped value intact and it can help with debugging.
     */
    *((uint32_t *)ptr) = 0xFFFFFFFF;	/* zappo! */

    free(ptr);
    bu_n_free++;

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_release(BU_SEM_MALLOC);
#endif
}


void *
bu_realloc(register void *ptr, size_t siz, const char *str)
{
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

#if defined(MALLOC_NOT_MP_SAFE)
    bu_semaphore_acquire(BU_SEM_MALLOC);
#endif

    ptr = realloc(ptr, siz);
    bu_n_realloc++;

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

    return ptr;
}


void
bu_prmem(const char *str)
{
    fprintf(stderr, "bu_prmem: no op\n");
    fprintf(stderr, "bu_prmem: str=%s\n", str);
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
    if (UNLIKELY(ptr == (char *)NULL)) {
	fprintf(stderr, "bu_ck_malloc_ptr(%p, %s) null pointer\n\n", ptr, str);
	bu_bomb("Goodbye");
    }
}


int
bu_mem_barriercheck(void)
{
    return 0;
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
    size_t shmsize;
    size_t psize;
    int flags = IPC_CREAT | 0666;

#ifdef _SC_PAGESIZE
    psize = sysconf(_SC_PAGESIZE);
#else
    psize = BU_PAGE_SIZE;
#endif

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
	    perror("bu_shmget");
	    return 1;
	}
	ret = -1;
    }

    /* WWW this is unnecessary in this version? */
    /* Open the segment Read/Write */
    /* This gets mapped to a high address on some platforms, so no problem. */
    if (((*shared_memory) = (char *)shmat((*shmid), 0, 0)) == (char *)(-1L)) {
	printf("bu_shmget failed, errno=%d\n", errno);
	perror("shmat");
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
