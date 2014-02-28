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
/** @file test_funcs.c
 *
 * Miscellaneous functions for libbu tests.
 *
 */

#include "common.h"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_STDINT_H
#   include <stdint.h>
#endif

#include "bio.h"

#include "bu.h"

#include "../bu_internals.h"
#include "./test_internals.h"


long int
bu_get_urandom_number()
{
    int rdata = open("/dev/urandom", O_RDONLY);
    long int rnum;

    ssize_t result = read(rdata, (char*)&rnum, sizeof(rnum));
    if (result < 0) {
	bu_log("ERROR:  Unable to read '/dev/urandom'.\n");
	bu_exit(1, NULL);
    }
    close(rdata);

    return rnum;
}


void
dump_bitv(const struct bu_bitv *b)
{
    const size_t len = b->nbits;
    size_t x;
    int i, j, k;
    int bit, ipad = 0, jpad = 0;
    size_t word_count;
    size_t chunksize = 0;
    volatile size_t BVS = sizeof(bitv_t); /* should be 1 byte as defined in bu.h */
    unsigned bytes;
    struct bu_vls *v = bu_vls_vlsinit();

    bytes = len / BITS_PER_BYTE; /* eight digits per byte */
    word_count = bytes / BVS;
    chunksize = bytes % BVS;

    if (chunksize == 0) {
	chunksize = BVS;
    } else {
	/* handle partial chunk before using chunksize == BVS */
	word_count++;
    }

    while (word_count--) {
	while (chunksize--) {
	    /* get the appropriate bits in the bit vector */
	    unsigned long lval = (unsigned long)((b->bits[word_count] & ((bitv_t)(0xff)<<(chunksize * BITS_PER_BYTE))) >> (chunksize * BITS_PER_BYTE)) & (bitv_t)0xff;

	    /* get next eight binary digits from bitv */
	    int ii;
	    for (ii = 7; ii >= 0; --ii) {
		unsigned long uval = lval >> ii & 0x1;
		bu_vls_printf(v, "%1lx", uval);
	    }
	}
	chunksize = BVS;
    }


    /* print out one set of data per X bits */
    x = 16; /* X number of bits per line */
    x = x > len ? len : x;
    /* we want even output lines (lengths in multiples of BITS_PER_BYTE) */
    if (len % x) {
	ipad = jpad = x - (len % x);
    };

    if (ipad > 0)
	i = j = -ipad;
    else
	i = j = 0;

    bit = len - 1;

    bu_log("\n=====================================================================");
    bu_log("\nbitv dump:");
    bu_log("\n nbits = %zu", len);

  NEXT:

    k = i + x;
    bu_log("\n---------------------------------------------------------------------");
    bu_log("\n bit ");
    for (; i < k; ++i, --ipad) {
	if (ipad > 0)
	    bu_log(" %3s", " ");
	else
	    bu_log(" %3d", bit--);
    }

    bu_log("\n val ");
    /* the actual values are a little tricky due to the actual structure and number of words */
    for (; j < k; ++j, --jpad) {
	if (jpad > 0)
	    bu_log(" %3s", " ");
	else
	    bu_log("   %c", bu_vls_cstr(v)[j]);
    }

    if ((size_t)i < len - 1) {
        goto NEXT;
    }

    bu_log("\n---------------------------------------------------------------------");
    bu_log("\n=====================================================================");

    bu_vls_free(v);
}


void
random_hex_or_binary_string(struct bu_vls *v, const hex_bin_enum_t typ, const int nbytes)
{
    const char hex_chars[] = "0123456789abcdef";
    const char bin_chars[] = "01";
    const int nstrchars = (typ & HEX) ? nbytes * HEXCHARS_PER_BYTE : nbytes * BITS_PER_BYTE;
    const char *chars = (typ & HEX) ? hex_chars : bin_chars;
    const int nchars = (typ & HEX) ? sizeof(hex_chars)/sizeof(char) : sizeof(bin_chars)/sizeof(char);
    int i;
    long int seed;

    /* get a random seed from system entropy to seed "random()" */
    seed = bu_get_urandom_number();
    srand(seed);

    bu_vls_trunc(v, 0);
    bu_vls_extend(v, nchars);
    for (i = 0; i < nstrchars; ++i) {
	long int r = rand();
	int n = r ? (int)(r % (nchars - 1)) : 0;
	char c = chars[n];
	bu_vls_putc(v, c);
    }
    bu_vls_strcat(v, '\0');

    if (typ == HEX) {
	bu_vls_prepend(v, "0x");
    } else if (typ == BINARY) {
	bu_vls_prepend(v, "0b");
    }

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

