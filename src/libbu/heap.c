/*                          H E A P . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
 * 1-to-BINS byte size allocations are allocated in PAGESIZE chunks as
 * they are requested.  There's minimal penalty for making this
 * arbitrarily large except where there are very few allocations (each
 * size will allocate at least one page).  PAGESIZE should be some
 * multiple larger than BINS.
 *
 * Embedded or memory-constrained environments probably want to set
 * this a lot smaller than the default.
 */
#define BINS 1024

/**
 * This specifies how much memory we should preallocate for each
 * allocation size.  Note that this number should be some multiple of
 * BINS and system page size in order to be useful.  Ideally sized to
 * keep the allocation size with the most requests down to
 * single-digit page counts.  Testing showed a 1M page size was very
 * effective at eliminating allocation overhead.
 *
 * Embedded or memory-constrained environments probably want to set
 * this a lot smaller than the default.
 */
#define PAGESIZE (BINS * 1024)


struct bins {
    /**
     * heaps is an array of memory pages.  they are allocated one at a
     * time so we only have to keep track of the last page.
     */
    char **heaps;

    /**
     * pages is a count of how many heap pages have been allocated so
     * we can quickly jump into the current.
     */
    size_t pages;

    /**
     * used is an array of page sizes for tabulating how much memory
     * each page is currently using (allocate).  not a counter so we
     * avoid a multiply.
     */
    size_t *used;

    /** keep track of our allocation counts for reporting stats */
    size_t alloc;
};

struct cpus {
    /** each allocation size gets a bin for holding memory pages */
    struct bins bin[BINS];

    /** keep track of allocation sizes outside our supported range */
    size_t misses;
};

/**
 * store data in a cpu-specific structure so we can avoid the need for
 * mutex locking entirely.  relies on static zero-initialization.
 */
static struct cpus per_cpu[MAX_PSW] = {{{{0, 0, 0, 0}}, 0}};


static void
heap_print()
{
    static int printed = 0;

    size_t h, i, j;
    size_t misses = 0;
    size_t allocations = 0;
    size_t total_pages = 0;
    size_t total_alloc = 0;
    size_t ncpu = bu_avail_cpus();

    /* this may get registered for atexit() multiple times, so make
     * sure we only do this once
     */
    if (printed++ > 0) {
	return;
    }

    bu_log("=======================\n"
	   "Memory Heap Information\n"
	   "-----------------------\n");

    for (h=0; h < ncpu; h++) {
	for (i=0; i < BINS; i++) {
	    allocations = 0;
	    for (j=0; j < per_cpu[h].bin[i].pages; j++) {
		allocations += per_cpu[h].bin[i].used[j] / (i+1);
	    }
	    if (allocations > 0)
		bu_log("%04zd [%02zd] => %zd\n", i, per_cpu[h].bin[i].pages, allocations);
	    total_pages += per_cpu[h].bin[i].pages;
	    total_alloc += allocations;
	}
	misses += per_cpu[h].misses;
    }
    bu_log("-----------------------\n"
	   "size [pages] => allocs\n"
	   "Heap range: 1-%d bytes\n"
	   "Page size: %d bytes\n"
	   "Pages: %zd (%.2lfMB)\n"
	   "%zd hits, %zd misses\n"
	   "=======================\n", BINS, PAGESIZE, total_pages, (double)(total_pages * PAGESIZE) / (1024.0*1024.0), total_alloc, misses);
}


void *
bu_heap_get(size_t sz)
{
    char *ret;
    register size_t smo = sz-1;
    static int printit = 0;
    int oncpu;
    struct bins *bin;

    /* what thread are we? */
    oncpu = bu_parallel_id();

    if (sz > BINS || sz == 0) {
	per_cpu[oncpu].misses++;

	if (bu_debug) {
	    bu_log("DEBUG: heap size %zd out of range\n", sz);

	    if (bu_debug & (BU_DEBUG_COREDUMP | BU_DEBUG_MEM_CHECK)) {
		bu_bomb("Intentionally bombing due to BU_DEBUG_COREDUMP and BU_DEBUG_MEM_CHECK\n");
	    }
	}
	return bu_calloc(1, sz, "heap calloc");
    }

    bin = &per_cpu[oncpu].bin[smo];
    bin->alloc++;

    /* init */
    if (bin->pages == 0) {

	if (bu_debug && printit == 0) {
	    printit++;
	    atexit(heap_print);
	}

	bin->pages++;
	bin->heaps = (char **)bu_malloc(1 * sizeof(char *), "heap malloc heaps[]");
	bin->heaps[0] = (char *)bu_calloc(1, PAGESIZE, "heap calloc heaps[][0]");
	bin->used = (size_t *)bu_malloc(1 * sizeof(size_t), "heap malloc used[]");
	bin->used[0] = 0;
    }

    /* grow */
    if (bin->used[bin->pages-1]+sz > PAGESIZE) {
	bin->pages++;
	bin->heaps = (char **)bu_realloc(bin->heaps, bin->pages * sizeof(char *), "heap realloc heaps[]");
	bin->heaps[bin->pages-1] = (char *)bu_calloc(1, PAGESIZE, "heap calloc heaps[][]");
	bin->used = (size_t *)bu_realloc(bin->used, bin->pages * sizeof(size_t), "heap realloc used[]");
	bin->used[bin->pages-1] = 0;
    }

    /* give */
    ret = &(bin->heaps[bin->pages-1][bin->used[bin->pages-1]]);
    bin->used[bin->pages-1] += sz;

    return (void *)ret;
}


void
bu_heap_put(void *ptr, size_t sz)
{
    if (sz > BINS || sz == 0) {
	bu_free(ptr, "heap free");
	return;
    }

    /* TODO: actually do something useful :) */

    return;
}


/* test driver application, intended to become part of unit test */
#if 0

int main (int ac, char *av[])
{
    int i;
    void *ptr;
    size_t allocalls = 0;
    size_t freecalls = 0;

    srand(time(0));

    for (i=0; i<1024*1024*50; i++) {
	size_t sz = (((double)rand() / (double)(RAND_MAX-1)) * (double)BINS) + 1;
	bu_log("allocating %d: %zd\n", i, sz);
#ifdef USE_MALLOC
	ptr = malloc(sz);
#else
	ptr = bu_fastalloc(sz);
#endif
	allocalls++;

	if (i%3==0) {
	    bu_log("freeing sz=%zd allocation\n", sz);
#ifdef USE_MALLOC
	    free(ptr);
#else
	    bu_fastfree(ptr);
#endif
	    freecalls++;
	}
	if (i % (1024 * 1024) == 0) {
#ifdef USE_MALLOC
	    free(NULL);
#else
	    bu_fastfree(NULL);
#endif
	    freecalls++;
	}

    }

#ifdef USE_MALLOC
    free(NULL);
#else
    bu_fastfree(NULL);
#endif
    freecalls++;

    bu_log("calls: %zd, free: %zd\n", allocalls, freecalls);

    return 0;
}

#endif


/* sanity */
#if PAGESIZE < BINS
#  error "ERROR: heap page size cannot be smaller than bin size"
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
