/*                        T E S T _ F U N C S . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file test_fucs.c
 *
 * Miscellaneous functions for libbu tests.
 *
 */

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "bu.h"
#include "./test_internals.h"


void
dump_bitv(const struct bu_bitv *b)
{
    size_t len = b->nbits;
    size_t i, j, k;

    /* print out one set of data per eight bytes (32 bits) */
    i = j = len - 1;

    printf("\n====================================================");
    printf("\nbitv dump:");
    printf("\n nbits = %zu", len);

  NEXT:

    k = i - 7;
    printf("\n----------------------------------------------------");
    printf("\n bit ");
    for (; i > k; --i) {
        printf(" %4zu", i);
    }
    printf(" %4zu", i ? i-- : 0);

    printf("\n val ");
    for (; j > k; --j) {
        printf(" 0x%02x", b->bits[j]);
    }
    printf(" 0x%02x", j ? b->bits[j--] : b->bits[0]);

    if (i >= 7) {
        goto NEXT;
    }

    printf("\n----------------------------------------------------");
    printf("\n====================================================");
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

