/*                     M A L L O C . H
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

/** @file malloc.h
 *
 */
#ifndef BU_MALLOC_H
#define BU_MALLOC_H

#include "common.h"
#include <stddef.h> /* for size_t */

#include "bu/defines.h"
#include "bu/magic.h"

__BEGIN_DECLS

/** @addtogroup malloc */
/** @{ */
/** @file libbu/malloc.c
 *
 * @brief
 * Parallel-protected debugging-enhanced wrapper around system malloc().
 *
 * Provides a parallel-safe interface to the system memory allocator
 * with standardized error checking, optional memory-use logging, and
 * optional run-time pointer and memory corruption testing.
 *
 * The bu_*alloc() routines can't use bu_log() because that uses the
 * bu_vls() routines which depend on bu_malloc().  So it goes direct
 * to stderr, semaphore protected.
 *
 */

BU_EXPORT extern long bu_n_malloc;
BU_EXPORT extern long bu_n_free;
BU_EXPORT extern long bu_n_realloc;

/**
 * This routine only returns on successful allocation.  We promise
 * never to return a NULL pointer; caller doesn't have to check.
 * Allocation failure results in bu_bomb() being called.
 */
BU_EXPORT extern void *bu_malloc(size_t siz,
				    const char *str);

/**
 * This routine only returns on successful allocation.
 * We promise never to return a NULL pointer; caller doesn't have to check.
 * Failure results in bu_bomb() being called.
 */
BU_EXPORT extern void *bu_calloc(size_t nelem,
				    size_t elsize,
				    const char *str);

BU_EXPORT extern void bu_free(void *ptr,
			      const char *str);

/**
 * bu_malloc()/bu_free() compatible wrapper for realloc().
 *
 * this routine mimics the C99 standard behavior of realloc() except
 * that NULL will never be returned.  it will bomb if siz is zero and
 * ptr is NULL.  it will return a minimum allocation suitable for
 * bu_free() if siz is zero and ptr is non-NULL.
 *
 * While the string 'str' is provided for the log messages, don't
 * disturb the str value, so that this storage allocation can be
 * tracked back to its original creator.
 */
BU_EXPORT extern void *bu_realloc(void *ptr,
				     size_t siz,
				     const char *str);

/**
 * Print map of memory currently in use.
 */
BU_EXPORT extern void bu_prmem(const char *str);

/**
 * On systems with the CalTech malloc(), the amount of storage
 * ACTUALLY ALLOCATED is the amount requested rounded UP to the
 * nearest power of two.  For structures which are acquired and
 * released often, this works well, but for structures which will
 * remain unchanged for the duration of the program, this wastes as
 * much as 50% of the address space (and usually memory as well).
 * Here, we round up a byte size to the nearest power of two, leaving
 * off the malloc header, so as to ask for storage without wasting
 * any.
 *
 * On systems with the traditional malloc(), this strategy will just
 * consume the memory in somewhat larger chunks, but overall little
 * unused memory will be consumed.
 */
BU_EXPORT extern int bu_malloc_len_roundup(int nbytes);

/**
 * For a given pointer allocated by bu_malloc(), bu_calloc(), or
 * BU_ALLOC() check the magic number stored after the allocation area
 * when BU_DEBUG_MEM_CHECK is set.
 *
 * This is the individual version of bu_mem_barriercheck().
 *
 * returns if pointer good or BU_DEBUG_MEM_CHECK not set, bombs if
 * memory is corrupted.
 */
BU_EXPORT extern void bu_ck_malloc_ptr(void *ptr, const char *str);

/**
 * Check *all* entries in the memory debug table for barrier word
 * corruption.  Intended to be called periodically through an
 * application during debugging.  Has to run single-threaded, to
 * prevent table mutation.
 *
 * This is the bulk version of bu_ck_malloc_ptr()
 *
 * Returns -
 *  -1	something is wrong
 *   0	all is OK;
 */
BU_EXPORT extern int bu_mem_barriercheck(void);

/**
 * really fast heap-based memory allocation intended for "small"
 * allocation sizes (e.g., single structs).
 *
 * the implementation allocates chunks of memory ('pages') in order to
 * substantially reduce calls to system malloc.  it has a nice
 * property of having O(1) constant time complexity and profiles
 * significantly faster than system malloc().
 *
 * release memory with bu_heap_put() only.
 */
BU_EXPORT extern void *bu_heap_get(size_t sz);

/**
 * counterpart to bu_heap_get() for releasing fast heap-based memory
 * allocations.
 *
 * the implementation may do nothing, relying on free-on-exit, or may
 * mark deallocations for reuse.  pass a NULL pointer and zero size to
 * force compaction of any unused memory.
 */
BU_EXPORT extern void bu_heap_put(void *ptr, size_t sz);

/**
 * Convenience typedef for the printf()-style callback function used
 * during application exit to print summary statistics.
 */
typedef int (*bu_heap_func_t)(const char *, ...);

/**
 * This function registers and returns the current printing function
 * that will be used during application exit (via an atexit() handler)
 * if the BU_HEAP_PRINT environment variable is set.  Statistics on
 * calls to bu_heap_get() and bu_heap_put() will be logged.  If log is
 * NULL, the currently set function will remain unchanged and will be
 * returned.
 */
BU_EXPORT extern bu_heap_func_t bu_heap_log(bu_heap_func_t log);


/** @} */

__END_DECLS

#endif  /* BU_MALLOC_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
