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
/** @file heap.c
 *
 * Brief description
 *
 */

#include "common.h"

#include "bu.h"


#define BINS 64
#define PAGESIZE (BINS * 1024)

/** heaps is an array of lists containing pages of memory binned per requested allocation size */
static char **heaps[BINS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/** pages is an array of counts for how many heap pages have been allocated per each heaps[] size */
static size_t pages[BINS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/** used is an array of lists to count how much memory has been used (allocated) per page */
static size_t *used[BINS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static size_t alloc[BINS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static size_t misses = 0;


void
bu_heap_print()
{
    size_t i, j;
    size_t allocations = 0;
    size_t total_pages = 0;

    bu_log("=======================\n"
	   "Memory Heap Information\n"
	   "-----------------------\n");

    for (i=0; i < BINS; i++) {
	allocations = 0;
	for (j=0; j < pages[i]; j++) {
	    allocations += used[i][j] / (i+1);
	}
	if (allocations > 0)
	    bu_log("%04ld [%02ld] => %ld\n", i, pages[i], allocations);
	total_pages += pages[i];
    }
    bu_log("-----------------------\n"
	   "size [pages] => allocs\n"
	   "Heap range: 1-%ld bytes\n"
	   "Page size: %ld bytes\n"
	   "Pages: %ld (%.2lfMB)\n"
	   "=======================\n", BINS, PAGESIZE, total_pages, (double)(total_pages * PAGESIZE) / (1024.0*1024.0));
}


void *
bu_heap_get(size_t sz)
{
    char *ret;
    register size_t smo = sz-1;
    static int printit = 0;

    if (sz > BINS || sz == 0) {
	misses++;
	return bu_calloc(1, sz, "heap calloc");
    }

    alloc[smo]++;

    /* init */
    if (!pages[smo]) {

	if (printit==0) {
	    atexit(bu_heap_print);
	    printit++;
	}

	pages[smo]++;
	heaps[smo] = (char **)bu_malloc(1 * sizeof(char *), "heap malloc heaps[]");
	heaps[smo][0] = (char *)bu_calloc(1, PAGESIZE, "heap calloc heaps[][0]");
	used[smo] = (size_t *)bu_malloc(1 * sizeof(size_t), "heap malloc used[]");
	used[smo][0] = 0;
    }

    /* grow */
    if (used[smo][pages[smo]-1]+sz > PAGESIZE) {
	pages[smo]++;
	heaps[smo] = (char **)bu_realloc(heaps[smo], pages[smo] * sizeof(char *), "heap realloc heaps[]");
	heaps[smo][pages[smo]-1] = (char *)bu_calloc(1, PAGESIZE, "heap calloc heaps[][]");
	used[smo] = (size_t *)bu_realloc(used[smo], pages[smo] * sizeof(size_t), "heap realloc used[]");
	used[smo][pages[smo]-1] = 0;
    }

    /* give */
    ret = &(heaps[smo][pages[smo]-1][used[smo][pages[smo]-1]]);
    used[smo][pages[smo]-1] += sz;

    return (void *)ret;
}


void
bu_heap_put(void *ptr, size_t sz)
{
    if (sz > BINS || sz == 0) {
	bu_free(ptr, "heap free");
	return;
    }

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
	printf("allocating %d: %ld\n", i, sz);
#ifdef USE_MALLOC
	ptr = malloc(sz);
#else
	ptr = bu_fastalloc(sz);
#endif
	allocalls++;

	if (i%3==0) {
	    printf("freeing sz=%ld allocation\n", sz);
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

    printf("calls: %ld, free: %ld\n", allocalls, freecalls);

    return 0;
}


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
