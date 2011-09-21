/*                          P T B L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include <stdio.h>
#include <string.h>

#include "bu.h"


void
bu_ptbl_init(struct bu_ptbl *b, size_t len, const char *str)
{
    if (UNLIKELY(bu_debug & BU_DEBUG_PTBL))
	bu_log("bu_ptbl_init(%p, len=%zu, %s)\n", (void *)b, len, str);

    BU_LIST_INIT_MAGIC(&b->l, BU_PTBL_MAGIC);

    if (UNLIKELY(len <= (size_t)0))
	len = 64;

    b->blen = len;
    b->buffer = (long **)bu_calloc(b->blen, sizeof(long *), str);
    b->end = 0;
}


void
bu_ptbl_reset(struct bu_ptbl *b)
{
    BU_CK_PTBL(b);

    if (UNLIKELY(bu_debug & BU_DEBUG_PTBL))
	bu_log("bu_ptbl_reset(%p)\n", (void *)b);
    b->end = 0;
    memset((char *)b->buffer, 0, b->blen*sizeof(long *));	/* no peeking */
}


int
bu_ptbl_ins(struct bu_ptbl *b, long int *p)
{
    register int i;

    BU_CK_PTBL(b);

    if (UNLIKELY(bu_debug & BU_DEBUG_PTBL))
	bu_log("bu_ptbl_ins(%p, %p)\n", (void *)b, (void *)p);

    if (b->blen == 0)
	bu_ptbl_init(b, 64, "bu_ptbl_ins() buffer");

    if ((size_t)b->end >= b->blen) {
	b->buffer = (long **)bu_realloc((char *)b->buffer,
					sizeof(p)*(b->blen *= 4),
					"bu_ptbl.buffer[] (ins)");
    }

    i=b->end++;
    b->buffer[i] = p;
    return i;
}


int
bu_ptbl_locate(const struct bu_ptbl *b, const long int *p)
{
    register int k;
    register const long **pp;

    BU_CK_PTBL(b);

    pp = (const long **)b->buffer;
    for (k = b->end-1; k >= 0; k--)
	if (pp[k] == p) return k;

    return -1;
}


void
bu_ptbl_zero(struct bu_ptbl *b, const long int *p)
{
    register int k;
    register const long **pp;

    BU_CK_PTBL(b);

    pp = (const long **)b->buffer;
    for (k = b->end-1; k >= 0; k--) {
	if (pp[k] == p) {
	    pp[k] = (long *)0;
	}
    }
}


int
bu_ptbl_ins_unique(struct bu_ptbl *b, long int *p)
{
    register int k;
    register long **pp;

    BU_CK_PTBL(b);

    pp = b->buffer;

    /* search for existing */
    for (k = b->end-1; k >= 0; k--) {
	if (pp[k] == p) {
	    return k;
	}
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PTBL))
	bu_log("bu_ptbl_ins_unique(%p, %p)\n", (void *)b, (void *)p);

    if (b->blen <= 0 || (size_t)b->end >= b->blen) {
	/* Table needs to grow */
	bu_ptbl_ins(b, p);
	return -1;	/* To signal that it was added */
    }

    b->buffer[k=b->end++] = p;
    return -1;		/* To signal that it was added */
}


int
bu_ptbl_rm(struct bu_ptbl *b, const long int *p)
{
    register int end, j, k, l;
    register long **pp;
    int ndel = 0;

    BU_CK_PTBL(b);

    end = b->end;
    pp = b->buffer;

    for (l = b->end-1; l >= 0; --l) {
	if (pp[l] == p) {
	    /* delete consecutive occurrence(s) of p */
	    ndel++;

	    j=l+1;
	    while (l >= 1 && pp[l-1] == p) --l, ndel++;
	    /* pp[l] through pp[j-1] match p */

	    end -= j - l;
	    for (k=l; j < b->end;)
		b->buffer[k++] = b->buffer[j++];
	    b->end = end;
	}
    }
    if (UNLIKELY(bu_debug & BU_DEBUG_PTBL))
	bu_log("bu_ptbl_rm(%p, %p) ndel=%d\n", (void *)b, (void *)p, ndel);
    return ndel;
}


void
bu_ptbl_cat(struct bu_ptbl *dest, const struct bu_ptbl *src)
{
    BU_CK_PTBL(dest);
    BU_CK_PTBL(src);

    if (UNLIKELY(bu_debug & BU_DEBUG_PTBL))
	bu_log("bu_ptbl_cat(%p, %p)\n", (void *)dest, (void *)src);

    if ((dest->blen - dest->end) < (size_t)src->end) {
	dest->blen = (dest->blen + src->end) * 2 + 8;
	dest->buffer = (long **)bu_realloc((char *)dest->buffer,
					   dest->blen * sizeof(long *),
					   "bu_ptbl.buffer[] (cat)");
    }
    memcpy((char *)&dest->buffer[dest->end], (char *)src->buffer, src->end*sizeof(long *));
    dest->end += src->end;
}


void
bu_ptbl_cat_uniq(struct bu_ptbl *dest, const struct bu_ptbl *src)
{
    register long **p;

    BU_CK_PTBL(dest);
    BU_CK_PTBL(src);

    if (UNLIKELY(bu_debug & BU_DEBUG_PTBL))
	bu_log("bu_ptbl_cat_uniq(%p, %p)\n", (void *)dest, (void *)src);

    /* Assume the worst, ensure sufficient space to add all 'src' items */
    if ((dest->blen - dest->end) < (size_t)src->end) {
	dest->buffer = (long **)bu_realloc((char *)dest->buffer,
					   sizeof(long *)*(dest->blen += src->blen + 8),
					   "bu_ptbl.buffer[] (cat_uniq)");
    }
    for (BU_PTBL_FOR(p, (long **), src)) {
	bu_ptbl_ins_unique(dest, *p);
    }
}


void
bu_ptbl_free(struct bu_ptbl *b)
{
    BU_CK_PTBL(b);

    if (b->buffer) {
	bu_free((genptr_t)b->buffer, "bu_ptbl.buffer[]");
    }
    memset((char *)b, 0, sizeof(struct bu_ptbl));	/* sanity */

    if (UNLIKELY(bu_debug & BU_DEBUG_PTBL))
	bu_log("bu_ptbl_free(%p)\n", (void *)b);
}


void
bu_pr_ptbl(const char *title, const struct bu_ptbl *tbl, int verbose)
{
    register long **lp;

    BU_CK_PTBL(tbl);

    bu_log("%s: bu_ptbl array with %d entries\n",
	   title, tbl->end);

    if (!verbose)
	return;

    /* Go in ascending order */
    for (lp = (long **)BU_PTBL_BASEADDR(tbl);
	 lp <= (long **)BU_PTBL_LASTADDR(tbl); lp++
	) {
	if (*lp == 0) {
	    bu_log("  %p NULL entry\n", (void *)*lp);
	    continue;
	}
	bu_log("  %p %s\n", (void *)*lp, bu_identify_magic(**lp));
    }
}


void
bu_ptbl_trunc(struct bu_ptbl *tbl, int end)
{
    BU_CK_PTBL(tbl);

    if (tbl->end <= end)
	return;

    tbl->end = end;
    return;
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
