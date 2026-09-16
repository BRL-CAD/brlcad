/*                     T E S T _ H E A P . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
