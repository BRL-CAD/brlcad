/*                          H E A P . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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

#include "bu.h"


/**
 * This number specifies the range of byte sizes to support for fast
 * memory allocations.  Any request outside this range will get passed
 * to bu_calloc().
 *
 * 1-to-HEAP_BINS byte size allocations are allocated in PAGESIZE chunks as
 * they are requested.  There's minimal penalty for making this
 * arbitrarily large except where there are very few allocations (each
 * size will allocate at least one page).  PAGESIZE should be some
 * multiple larger than HEAP_BINS.
 *
 * Embedded or memory-constrained environments probably want to set
 * this a lot smaller than the default.
 */
#define HEAP_BINS 256

/**
 * This specifies how much memory we should preallocate for each
 * allocation size.  Note that this number should be some multiple of
 * HEAP_BINS and system page size in order to be useful.  Ideally sized to
 * keep the allocation size with the most requests down to
 * single-digit page counts.  Testing showed a 1M page size was very
 * effective at eliminating allocation overhead.
 *
 * Embedded or memory-constrained environments probably want to set
 * this a lot smaller than the default.
 */
#define HEAP_PAGESIZE (HEAP_BINS * 256)


struct heap {
    /**
     * pages is an array of memory pages.  they are allocated one at a
     * time so we only have to keep track of the last page.
     */
    char **pages;

    /**
     * a count of how many heap pages have been allocated so we can
     * quickly jump into the current.
     */
    size_t count;

    /**
     * given tabulates how much memory the current page has been
     * allocated to callers.  not a counter to avoid a multiply.
     */
    size_t given;
};

struct cpus {
    /** each allocation size gets a bin for holding memory pages */
    struct heap heap[HEAP_BINS];

    /** keep track of allocation sizes outside our supported range */
    size_t misses;
};

/**
 * store data in a cpu-specific structure so we can avoid the need for
 * mutex locking entirely.  relies on static zero-initialization.
 */
static struct cpus per_cpu[MAX_PSW] = {{{{0, 0, 0}}, 0}};


bu_heap_func_t
bu_heap_log(bu_heap_func_t log)
{
    static bu_heap_func_t heap_log = (bu_heap_func_t)&bu_log;

    if (log)
	heap_log = log;

    return heap_log;
}


static void
heap_print(void)
{
    static int printed = 0;

    size_t h, i;
    size_t allocs = 0;
    size_t misses = 0;
    size_t got = 0;
    size_t total_pages = 0;
    size_t ncpu = bu_avail_cpus();

    bu_heap_func_t log = bu_heap_log(NULL);

    struct bu_vls str = BU_VLS_INIT_ZERO;

    /* this may get atexit()-registered multiple times, so make sure
     * we only do this once
     */
    if (printed++ > 0) {
	return;
    }

    log("=======================\n"
	"Memory Heap Information\n"
	"-----------------------\n", NULL);

    for (h=0; h < ncpu; h++) {
	for (i=0; i < HEAP_BINS; i++) {

	    /* capacity across all pages */
	    got = per_cpu[h].heap[i].count * (HEAP_PAGESIZE/(i+1));

	    if (got > 0) {
		/* last page is partial */
		got -= (HEAP_PAGESIZE - per_cpu[h].heap[i].given)/(i+1);
		bu_vls_sprintf(&str, "%04zu [%02zu] => %zu\n", i, per_cpu[h].heap[i].count, got);
		log(bu_vls_addr(&str), NULL);
		allocs += got;
	    }
	    total_pages += per_cpu[h].heap[i].count;
	}
	misses += per_cpu[h].misses;
    }
    bu_vls_sprintf(&str, "-----------------------\n"
		   "size [pages] => count\n"
		   "Heap range: 1-%d bytes\n"
		   "Page size: %d bytes\n"
		   "Pages: %zu (%.2lfMB)\n"
		   "%zu allocs, %zu misses\n"
		   "=======================\n",
		   HEAP_BINS,
		   HEAP_PAGESIZE,
		   total_pages,
		   (double)(total_pages * HEAP_PAGESIZE) / (1024.0*1024.0),
		   allocs,
		   misses);
    log(bu_vls_addr(&str), NULL);
    bu_vls_free(&str);
}


void *
bu_heap_get(size_t sz)
{
    char *ret;
    register size_t smo = sz-1;
    static int registered = 0;
    int oncpu;
    struct heap *heap;

    /* what thread are we? */
    oncpu = bu_parallel_id();

#ifdef DEBUG
    if (sz > HEAP_BINS || sz == 0) {
	per_cpu[oncpu].misses++;

	if (bu_debug) {
	    bu_log("DEBUG: heap size %zd out of range\n", sz);

	    if (bu_debug & (BU_DEBUG_COREDUMP | BU_DEBUG_MEM_CHECK)) {
		bu_bomb("Intentionally bombing due to BU_DEBUG_COREDUMP and BU_DEBUG_MEM_CHECK\n");
	    }
	}
	return bu_calloc(1, sz, "heap calloc");
    }
#endif

    heap = &per_cpu[oncpu].heap[smo];

    /* init */
    if (heap->count == 0) {

	if (registered++ == 0) {
	    ret = getenv("BU_HEAP_PRINT");
	    if ((++registered == 2) && (ret && atoi(ret) > 0)) {
		atexit(heap_print);
	    }
	}

	heap->count++;
	heap->pages = (char **)bu_malloc(1 * sizeof(char *), "heap malloc pages[]");
	heap->pages[0] = (char *)bu_calloc(1, HEAP_PAGESIZE, "heap calloc pages[][0]");
	heap->given = 0;
    }

    /* grow */
    if (heap->given+sz > HEAP_PAGESIZE) {
	heap->count++;
	heap->pages = (char **)bu_realloc(heap->pages, heap->count * sizeof(char *), "heap realloc pages[]");
	heap->pages[heap->count-1] = (char *)bu_calloc(1, HEAP_PAGESIZE, "heap calloc pages[][]");
	heap->given = 0;
    }

    /* give */
    ret = &(heap->pages[heap->count-1][heap->given]);
    heap->given += sz;

    return (void *)ret;
}


void
bu_heap_put(void *ptr, size_t sz)
{
    if (sz > HEAP_BINS || sz == 0) {
	bu_free(ptr, "heap free");
	return;
    }

    /* TODO: actually do something useful :) */

    return;
}


/* sanity */
#if HEAP_PAGESIZE < HEAP_BINS
#  error "ERROR: heap page size cannot be smaller than bin range"
#endif


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
