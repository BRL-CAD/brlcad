/*                        S O R T . C
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
/* Based on OpenBSD's qsort.c rev. 251672 2013/12/17
 * -
 * Copyright (c) 1992, 1993
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

#include "common.h"

#include <stdlib.h>

#include "bu/sort.h"

#define MIN(a, b) (a) < (b) ? a : b

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define SWAPCODE(TYPE, parmi, parmj, n)     \
    {                                       \
        long i = (n) / sizeof (TYPE); 		\
        TYPE *pi = (TYPE *) (parmi); 		\
        TYPE *pj = (TYPE *) (parmj); 		\
        do {                                \
            TYPE	t = *pi;                \
            *pi++ = *pj;                    \
            *pj++ = t;                      \
        } while (--i > 0);                  \
    }

#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
    es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;


static void
swapfunc(char *a, char *b, int n, int swaptype)
{
    if (swaptype <= 1)
        SWAPCODE(long, a, b, n)
    else
        SWAPCODE(char, a, b, n)
}


#define SWAP(a, b)				\
    if (swaptype == 0) {			\
        long t = *(long *)(a);			\
        *(long *)(a) = *(long *)(b);    \
        *(long *)(b) = t;               \
    } else 					\
        swapfunc(a, b, sizememb, swaptype)

#define VECSWAP(a, b, n) if ((n) > 0) swapfunc(a, b, n, swaptype)

#define CMP(t, x, y) (compare((t), (x), (y)))


static char *
med3(char *a, char *b, char *c, int (*compare)(const void *, const void *, void *), void *thunk)
{
    return CMP(a, b, thunk) < 0 ?
	   (CMP(b, c, thunk) < 0 ? b : (CMP(a, c, thunk) < 0 ? c : a))
	  :(CMP(b, c, thunk) > 0 ? b : (CMP(a, c, thunk) < 0 ? a : c));
}


void
bu_sort(void *array, size_t nummemb, size_t sizememb, int (*compare)(const void *, const void *, void *), void *context)
{
    char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
    int cmp_result;
    int swaptype;
    size_t d, r;
    size_t swap_cnt;

  loop:	SWAPINIT(array, sizememb);
    swap_cnt = 0;
    if (nummemb < 7) {
        for (pm = (char *)array + sizememb; pm < (char *)array + nummemb * sizememb; pm += sizememb)
            for (pl = pm;
                 pl > (char *)array && CMP(pl - sizememb, pl, context) > 0;
                 pl -= sizememb)
                SWAP(pl, pl - sizememb);
        return;
    }
    pm = (char *)array + (nummemb / 2) * sizememb;
    if (nummemb > 7) {
        pl = (char *)array;
        pn = (char *)array + (nummemb - 1) * sizememb;
        if (nummemb > 40) {
            d = (nummemb / 8) * sizememb;
            pl = med3(pl, pl + d, pl + 2 * d, compare, context);
            pm = med3(pm - d, pm, pm + d, compare, context);
            pn = med3(pn - 2 * d, pn - d, pn, compare, context);
        }
        pm = med3(pl, pm, pn, compare, context);
    }
    SWAP((char *)array, pm);
    pa = pb = (char *)array + sizememb;

    pc = pd = (char *)array + (nummemb - 1) * sizememb;
    for (;;) {
        while (pb <= pc && (cmp_result = CMP(pb, array, context)) <= 0) {
            if (cmp_result == 0) {
                swap_cnt = 1;
                SWAP(pa, pb);
                pa += sizememb;
            }
            pb += sizememb;
        }
        while (pb <= pc && (cmp_result = CMP(pc, array, context)) >= 0) {
            if (cmp_result == 0) {
                swap_cnt = 1;
                SWAP(pc, pd);
                pd -= sizememb;
            }
            pc -= sizememb;
        }
        if (pb > pc)
            break;
        SWAP(pb, pc);
        swap_cnt = 1;
        pb += sizememb;
        pc -= sizememb;
    }
    if (swap_cnt == 0) {  /* Switch to insertion sort */
        for (pm = (char *)array + sizememb; pm < (char *)array + nummemb * sizememb; pm += sizememb)
            for (pl = pm;
                 pl > (char *)array && CMP(pl - sizememb, pl, context) > 0;
                 pl -= sizememb)
                SWAP(pl, pl - sizememb);
        return;
    }

    pn = (char *)array + nummemb * sizememb;
    r = MIN(pa - (char *)array, pb - pa);
    VECSWAP((char *)array, pb - r, r);
    r = MIN(pd - pc, (signed) (pn - pd - sizememb));
    VECSWAP(pb, pn - r, r);
    if ((r = pb - pa) > sizememb)
        bu_sort(array, r / sizememb, sizememb, compare, context);
    if ((r = pd - pc) > sizememb) {
        /* Iterate rather than recurse to save stack space */
        array = pn - r;
        nummemb = r / sizememb;
        goto loop;
    }
}
