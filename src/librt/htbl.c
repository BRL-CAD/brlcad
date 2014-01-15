/*                          H T B L . C
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
/** @addtogroup librt */
/** @{ */
/** @file librt/htbl.c
 *
 * Support for variable length arrays of "struct hit".  Patterned
 * after the libbu/ptbl.c idea.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"


void
rt_htbl_init(struct rt_htbl *b, size_t len, const char *str)

/* initial len. */

{
    if (bu_debug & BU_DEBUG_PTBL)
	bu_log("rt_htbl_init(%p, len=%zu, %s)\n", (void *)b, len, str);
    BU_LIST_INIT(&b->l);
    b->l.magic = RT_HTBL_MAGIC;
    if (len == 0) len = 64;
    b->blen = len;
    b->hits = (struct hit *)bu_calloc(b->blen, sizeof(struct hit), str);
    b->end = 0;
}


void
rt_htbl_reset(struct rt_htbl *b)
{
    RT_CK_HTBL(b);
    b->end = 0;
    if (bu_debug & BU_DEBUG_PTBL)
	bu_log("rt_htbl_reset(%p)\n", (void *)b);
}


void
rt_htbl_free(struct rt_htbl *b)
{
    RT_CK_HTBL(b);

    bu_free((genptr_t)b->hits, "rt_htbl.hits[]");
    memset((char *)b, 0, sizeof(struct rt_htbl));	/* sanity */

    if (bu_debug & BU_DEBUG_PTBL)
	bu_log("rt_htbl_free(%p)\n", (void *)b);
}


struct hit *
rt_htbl_get(struct rt_htbl *b)
{
    RT_CK_HTBL(b);

    if (b->end >= b->blen) {
	/* Increase size of array */
	b->hits = (struct hit *)bu_realloc((char *)b->hits,
					   sizeof(struct hit) * (b->blen *= 4),
					   "rt_htbl.hits[]");
    }

    /* There is sufficient room */
    return &b->hits[b->end++];
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
