/* OpenBSD: qsort.c    v1.18 2017/05/30 14:54:09 */
/* OpenBSD: heapsort.c v1.11 2017/05/20 12:48:56 */
/*-
 * Copyright (c) 1991, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Note - the purpose of this file is to guarantee a consistent quick sort for
 * libbu applications.  We base this on OpenBSD's implementation, adding a
 * context that can be passed through the sorting calls to comparison
 * functions.  OpenBSD's code uses heapsort under the hood, so that
 * implementation is also included here. */

/* TODO - incorporate tests defined by OpenBSD into our libbu unit tests:
 * https://github.com/openbsd/src/tree/master/regress/lib/libc/qsort */

#include "common.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include "bu/sort.h"

/***********
 * heapsort
 ***********/

/*
 * Swap two areas of size number of bytes.  Although qsort permits random
 * blocks of memory to be sorted, sorting pointers is almost certainly the
 * common case (and, were it not, could easily be made so).  Regardless, it
 * isn't worth optimizing; the SWAP's get sped up by the cache, and pointer
 * arithmetic gets lost in the time required for comparison function calls.
 */
#define	SWAP(a, b, count, size, tmp) { \
    count = size; \
    do { \
	tmp = *a; \
	*a++ = *b; \
	*b++ = tmp; \
    } while (--count); \
}

/* Copy one block of size size to another. */
#define COPY(a, b, count, size, tmp1, tmp2) { \
    count = size; \
    tmp1 = a; \
    tmp2 = b; \
    do { \
	*tmp1++ = *tmp2++; \
    } while (--count); \
}

/*
 * Build the list into a heap, where a heap is defined such that for
 * the records K1 ... KN, Kj/2 >= Kj for 1 <= j/2 <= j <= N.
 *
 * There are two cases.  If j == nmemb, select largest of Ki and Kj.  If
 * j < nmemb, select largest of Ki, Kj and Kj+1.
 */
#define CREATE(initval, nmemb, par_i, child_i, par, child, size, count, tmp, ctx) { \
    for (par_i = initval; (child_i = par_i * 2) <= nmemb; \
	    par_i = child_i) { \
	child = base + child_i * size; \
	if (child_i < nmemb && compar(child, child + size, ctx) < 0) { \
	    child += size; \
	    ++child_i; \
	} \
	par = base + par_i * size; \
	if (compar(child, par, ctx) <= 0) \
	break; \
	SWAP(par, child, count, size, tmp); \
    } \
}

/*
 * Select the top of the heap and 'heapify'.  Since by far the most expensive
 * action is the call to the compar function, a considerable optimization
 * in the average case can be achieved due to the fact that k, the displaced
 * element, is usually quite small, so it would be preferable to first
 * heapify, always maintaining the invariant that the larger child is copied
 * over its parent's record.
 *
 * Then, starting from the *bottom* of the heap, finding k's correct place,
 * again maintaining the invariant.  As a result of the invariant no element
 * is 'lost' when k is assigned its correct place in the heap.
 *
 * The time savings from this optimization are on the order of 15-20% for the
 * average case. See Knuth, Vol. 3, page 158, problem 18.
 *
 */
#define HEAP_SELECT(par_i, child_i, nmemb, par, child, size, k, count, tmp1, tmp2, ctx) { \
    for (par_i = 1; (child_i = par_i * 2) <= nmemb; par_i = child_i) { \
	child = base + child_i * size; \
	if (child_i < nmemb && compar(child, child + size, ctx) < 0) { \
	    child += size; \
	    ++child_i; \
	} \
	par = base + par_i * size; \
	COPY(par, child, count, size, tmp1, tmp2); \
    } \
    for (;;) { \
	child_i = par_i; \
	par_i = child_i / 2; \
	child = base + child_i * size; \
	par = base + par_i * size; \
	if (child_i == 1 || compar(k, par, ctx) < 0) { \
	    COPY(child, k, count, size, tmp1, tmp2); \
	    break; \
	} \
	COPY(child, par, count, size, tmp1, tmp2); \
    } \
}

/*
 * Heapsort -- Knuth, Vol. 3, page 145.  Runs in O (N lg N), both average
 * and worst.  While heapsort is faster than the worst case of quicksort,
 * the BSD quicksort does median selection so that the chance of finding
 * a data set that will trigger the worst case is nonexistent.  Heapsort's
 * only advantage over quicksort is that it requires little additional memory.
 */
static int
heapsort(void *vbase, size_t nmemb, size_t size,
	int (*compar)(const void *, const void *, void *), void *context)
{
    size_t cnt, i, j, l;
    char tmp, *tmp1, *tmp2;
    char *base, *k, *p, *t;

    if (nmemb <= 1)
	return (0);

    if (!size) {
	errno = EINVAL;
	return (-1);
    }

    if ((k = (char *)malloc(size)) == NULL)
	return (-1);

    /*
     * Items are numbered from 1 to nmemb, so offset from size bytes
     * below the starting address.
     */
    base = (char *)vbase - size;

    for (l = nmemb / 2 + 1; --l;)
	CREATE(l, nmemb, i, j, t, p, size, cnt, tmp, context);

    /*
     * For each element of the heap, save the largest element into its
     * final slot, save the displaced element (k), then recreate the
     * heap.
     */
    while (nmemb > 1) {
	COPY(k, base + nmemb * size, cnt, size, tmp1, tmp2);
	COPY(base + nmemb * size, base + size, cnt, size, tmp1, tmp2);
	--nmemb;
	HEAP_SELECT(i, j, nmemb, t, p, size, k, cnt, tmp1, tmp2, context);
    }
    free(k);
    return (0);
}


/********
 * qsort
 ********/

static char	*med3(char *, char *, char *, int (*)(const void *, const void *, void *), void *);
static void	 swapfunc(char *, char *, size_t, int);

#ifndef min
#  define min(a, b)	(a) < (b) ? a : b
#endif

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 *
 * This version differs from Bentley & McIlroy in the following ways:
 *   1. The partition value is swapped into a[0] instead of being
 *	stored out of line.
 *
 *   2. The swap function can swap 32-bit aligned elements on 64-bit
 *	platforms instead of swapping them as byte-aligned.
 *
 *   3. It uses David Musser's introsort algorithm to fall back to
 *	heapsort(3) when the recursion depth reaches 2*lg(n + 1).
 *	This avoids quicksort's quadratic behavior for pathological
 *	input without appreciably changing the average run time.
 *
 *   4. Tail recursion is eliminated when sorting the larger of two
 *	subpartitions to save stack space.
 */
#define SWAPTYPE_BYTEV	1
#define SWAPTYPE_INTV	2
#define SWAPTYPE_LONGV	3
#define SWAPTYPE_INT	4
#define SWAPTYPE_LONG	5

#define TYPE_ALIGNED(TYPE, a, es)			\
    ((uintptr_t)a % sizeof(TYPE) == 0 && es % sizeof(TYPE) == 0)

#define swapcode(TYPE, parmi, parmj, n) { 		\
    size_t i = (n) / sizeof (TYPE); 		\
    TYPE *pi = (TYPE *) (parmi); 		\
    TYPE *pj = (TYPE *) (parmj); 		\
    do {                                \
	TYPE	t = *pi;                \
	*pi++ = *pj;                    \
	*pj++ = t;                      \
    } while (--i > 0);                  \
}

static void
swapfunc(char *a, char *b, size_t n, int swaptype)
{
    switch (swaptype) {
	case SWAPTYPE_INT:
	case SWAPTYPE_INTV:
	    swapcode(int, a, b, n);
	    break;
	case SWAPTYPE_LONG:
	case SWAPTYPE_LONGV:
	    swapcode(long, a, b, n);
	    break;
	default:
	    swapcode(char, a, b, n);
	    break;
    }
}

#define swap(a, b)	do {				\
    switch (swaptype) {				\
	case SWAPTYPE_INT: {				\
			       int t = *(int *)(a);			\
			       *(int *)(a) = *(int *)(b);		\
			       *(int *)(b) = t;			\
			       break;					\
			   }						\
	case SWAPTYPE_LONG: {				\
				long t = *(long *)(a);			\
				*(long *)(a) = *(long *)(b);		\
				*(long *)(b) = t;			\
				break;					\
			    }						\
	default:					\
							swapfunc(a, b, es, swaptype);		\
    } \
} while (0)

#define vecswap(a, b, n) 	if ((n) > 0) swapfunc(a, b, n, swaptype)

static char *
med3(char *a, char *b, char *c, int (*cmp)(const void *, const void *, void *), void *context)
{
    return cmp(a, b, context) < 0 ?
	(cmp(b, c, context) < 0 ? b : (cmp(a, c, context) < 0 ? c : a ))
	:(cmp(b, c, context) > 0 ? b : (cmp(a, c, context) < 0 ? a : c ));
}

static void
introsort(char *a, size_t n, size_t es, size_t maxdepth, int swaptype,
	int (*cmp)(const void *, const void *, void *), void *context)
{
    char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
    int cmp_result;
    size_t r, s;

loop:	if (n < 7) {
	    for (pm = a + es; pm < a + n * es; pm += es)
		for (pl = pm; pl > a && cmp(pl - es, pl, context) > 0;
			pl -= es)
		    swap(pl, pl - es);
	    return;
	}
	if (maxdepth == 0) {
	    if (heapsort(a, n, es, cmp, context) == 0)
		return;
	}
	maxdepth--;
	pm = a + (n / 2) * es;
	if (n > 7) {
	    pl = a;
	    pn = a + (n - 1) * es;
	    if (n > 40) {
		s = (n / 8) * es;
		pl = med3(pl, pl + s, pl + 2 * s, cmp, context);
		pm = med3(pm - s, pm, pm + s, cmp, context);
		pn = med3(pn - 2 * s, pn - s, pn, cmp, context);
	    }
	    pm = med3(pl, pm, pn, cmp, context);
	}
	swap(a, pm);
	pa = pb = a + es;
	pc = pd = a + (n - 1) * es;
	for (;;) {
	    while (pb <= pc && (cmp_result = cmp(pb, a, context)) <= 0) {
		if (cmp_result == 0) {
		    swap(pa, pb);
		    pa += es;
		}
		pb += es;
	    }
	    while (pb <= pc && (cmp_result = cmp(pc, a, context)) >= 0) {
		if (cmp_result == 0) {
		    swap(pc, pd);
		    pd -= es;
		}
		pc -= es;
	    }
	    if (pb > pc)
		break;
	    swap(pb, pc);
	    pb += es;
	    pc -= es;
	}

	pn = a + n * es;
	r = min(pa - a, pb - pa);
	vecswap(a, pb - r, r);
	r = min((long unsigned int)pd - (long unsigned int)pc, pn - pd - es);
	vecswap(pb, pn - r, r);
	/*
	 * To save stack space we sort the smaller side of the partition first
	 * using recursion and eliminate tail recursion for the larger side.
	 */
	r = pb - pa;
	s = pd - pc;
	if (r < s) {
	    /* Recurse for 1st side, iterate for 2nd side. */
	    if (s > es) {
		if (r > es) {
		    introsort(a, r / es, es, maxdepth,
			    swaptype, cmp, context);
		}
		a = pn - s;
		n = s / es;
		goto loop;
	    }
	} else {
	    /* Recurse for 2nd side, iterate for 1st side. */
	    if (r > es) {
		if (s > es) {
		    introsort(pn - s, s / es, es, maxdepth,
			    swaptype, cmp, context);
		}
		n = r / es;
		goto loop;
	    }
	}
}

void
bu_sort(void *a, size_t n, size_t es, int (*cmp)(const void *, const void *, void *), void *context)
{
    size_t i, maxdepth = 0;
    int swaptype;

    /* Approximate 2*ceil(lg(n + 1)) */
    for (i = n; i > 0; i >>= 1)
	maxdepth++;
    maxdepth *= 2;

    if (TYPE_ALIGNED(long, a, es))
	swaptype = es == sizeof(long) ? SWAPTYPE_LONG : SWAPTYPE_LONGV;
    else if (sizeof(int) != sizeof(long) && TYPE_ALIGNED(int, a, es))
	swaptype = es == sizeof(int) ? SWAPTYPE_INT : SWAPTYPE_INTV;
    else
	swaptype = SWAPTYPE_BYTEV;

    introsort((char *)a, n, es, maxdepth, swaptype, cmp, context);

}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
