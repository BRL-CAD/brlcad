/*                       H E A P . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2025 United States Government as represented by
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
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bu.h"


/* this should match what is in heap.c */
#define HEAP_BINS 512

#define CNTCALLS


/*
 * FIXME: this routine should compare heap with malloc and make sure
 * it's always faster.
 */
int
main(int ac, char *av[])
{
    int i;
    void *ptr;
#ifdef CNTCALLS
    size_t allocalls = 0;
    size_t freecalls = 0;
#endif

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(av[0]);

    if (ac > 1) {
	fprintf(stderr, "Usage: %s\n", av[0]);
	return 1;
    }

    srand(time(0));

    for (i=0; i<1024*1024*10; i++) {
	size_t sz = ((rand() / (double)(RAND_MAX-1)) * (double)HEAP_BINS) + 1;
	/* bu_log("allocating %d: %zd\n", i, sz); */
#ifdef USE_MALLOC
	ptr = malloc(sz);
#else
	ptr = bu_heap_get(sz);
#endif
#ifdef CNTCALLS
	allocalls++;
#endif
#ifdef USE_MALLOC
	free(ptr);
#else
	bu_heap_put(ptr, sz);
#endif
#ifdef CNTCALLS
	freecalls++;
#endif
    }

#ifdef USE_MALLOC
    free(NULL);
#else
    bu_heap_put(NULL, 1);
#endif
    freecalls++;

#ifdef CNTCALLS
    bu_log("calls: %zd, free: %zd\n", allocalls, freecalls);
#endif

    return 0;
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
