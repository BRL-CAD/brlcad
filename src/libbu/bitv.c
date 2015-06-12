/*                          B I T V . C
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

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include "bu/bitv.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "./bu_internals.h"

/**
 * Private 32-bit recursive reduction using "SIMD Within A Register"
 * (SWAR) to count the number of one bits in a given integer. The
 * first step is mapping 2-bit values into sum of 2 1-bit values in
 * sneaky way. This technique was taken from the University of
 * Kentucky's Aggregate Magic Algorithms collection.
 *
 * LLVM 3.2 complains about a static inline function here, so use a macro instead
 */
#define COUNT_ONES32(x) { \
    x -= ((x >> 1) & 0x55555555); \
    x  = (((x >> 2) & 0x33333333) + (x & 0x33333333)); \
    x  = (((x >> 4) + x) & 0x0f0f0f0f); \
    x += (x >> 8); \
    x += (x >> 16); \
    x &= 0x0000003f; \
}


/**
 * Private 32-bit recursive reduction using "SIMD Within A Register"
 * (SWAR) to compute a base-2 integer logarithm for a given integer.
 * This technique was taken from the University of Kentucky's
 * Aggregate Magic Algorithms collection.
 *
 * LLVM 3.2 complains about a static inline function here, so use a macro instead
 */
#define FLOOR_ILOG2(x) { \
    x |= (x >> 1); \
    x |= (x >> 2); \
    x |= (x >> 4); \
    x |= (x >> 8); \
    x |= (x >> 16); \
    x >>= 1; \
    COUNT_ONES32(x) \
}


/**
 * Wrap the above private routines for computing the bitv shift size.
 * Users should not call this directly, instead calling the
 * BU_BITV_SHIFT macro instead.
 */
inline size_t
bu_bitv_shift(void)
{
    size_t x = sizeof(bitv_t) * BITS_PER_BYTE;

    FLOOR_ILOG2(x);

    return x;
}


struct bu_bitv *
bu_bitv_new(size_t nbits)
{
    struct bu_bitv *bv;
    size_t bv_bytes;
    size_t total_bytes;

    bv_bytes = BU_BITS2BYTES(nbits);
    total_bytes = sizeof(struct bu_bitv) - 2*sizeof(bitv_t) + bv_bytes;

    /* allocate bigger than struct, bits array extends past the end */
    bv = (struct bu_bitv *)bu_malloc(total_bytes, "struct bu_bitv");

    /* manually initialize */
    BU_LIST_INIT_MAGIC(&(bv->l), BU_BITV_MAGIC);
    bv->nbits = bv_bytes * BITS_PER_BYTE;
    BU_BITV_ZEROALL(bv);

    return bv;
}


void
bu_bitv_free(struct bu_bitv *bv)
{
    BU_CK_BITV(bv);

    bv->l.forw = bv->l.back = BU_LIST_NULL;	/* sanity */
    bu_free((char *)bv, "struct bu_bitv");
}


void
bu_bitv_clear(struct bu_bitv *bv)
{
    BU_CK_BITV(bv);

    BU_BITV_ZEROALL(bv);
}


void
bu_bitv_or(struct bu_bitv *ov, const struct bu_bitv *iv)
{
    register bitv_t *out;
    register const bitv_t *in;
    register size_t words;

    if (UNLIKELY(ov->nbits != iv->nbits))
	bu_bomb("bu_bitv_or: length mis-match");

    out = ov->bits;
    in = iv->bits;
    words = BU_BITS2WORDS(iv->nbits);
#ifdef VECTORIZE
    for (--words; words >= 0; words--)
	out[words] |= in[words];
#else
    while (words-- > 0)
	*out++ |= *in++;
#endif
}


void
bu_bitv_and(struct bu_bitv *ov, const struct bu_bitv *iv)
{
    register bitv_t *out;
    register const bitv_t *in;
    register size_t words;

    if (UNLIKELY(ov->nbits != iv->nbits))
	bu_bomb("bu_bitv_and: length mis-match");

    out = ov->bits;
    in = iv->bits;
    words = BU_BITS2WORDS(iv->nbits);
#ifdef VECTORIZE
    for (--words; words >= 0; words--)
	out[words] &= in[words];
#else
    while (words-- > 0)
	*out++ &= *in++;
#endif
}


void
bu_bitv_vls(struct bu_vls *v, register const struct bu_bitv *bv)
{
    int seen = 0;
    register size_t i;
    size_t len;

    BU_CK_VLS(v);
    BU_CK_BITV(bv);

    len = bv->nbits;

    bu_vls_strcat(v, "(");

    /* Visit all the bits in ascending order */
    for (i = 0; i < len; i++) {
	if (BU_BITTEST(bv, i) == 0)
	    continue;
	if (seen)
	    bu_vls_strcat(v, ", ");
	bu_vls_printf(v, "%lu", i);
	seen = 1;
    }
    bu_vls_strcat(v, ")");
}


void
bu_pr_bitv(const char *str, register const struct bu_bitv *bv)
{
    struct bu_vls v = BU_VLS_INIT_ZERO;

    BU_CK_BITV(bv);

    if (str) {
	bu_vls_strcat(&v, str);
	bu_vls_strcat(&v, ": ");
    }
    bu_bitv_vls(&v, bv);
    bu_log("%s", bu_vls_addr(&v));
    bu_vls_free(&v);
}


void
bu_bitv_to_hex(struct bu_vls *v, const struct bu_bitv *bv)
{
    size_t word_count = 0;
    size_t chunksize  = 0;
    /* necessarily volatile to keep the compiler from complaining
     * about unreachable code during optimization.
     */
    volatile size_t BVS = sizeof(bitv_t);

    BU_CK_VLS(v);
    BU_CK_BITV(bv);

    word_count = bv->nbits / BITS_PER_BYTE / BVS;
    bu_vls_extend(v, word_count * BVS * 2 + 1);

    while (word_count--) {
	chunksize = BVS;
	while (chunksize--) {
	    unsigned long val = (unsigned long)((bv->bits[word_count] & ((bitv_t)(0xff)<<(chunksize * BITS_PER_BYTE))) >> (chunksize * BITS_PER_BYTE)) & (bitv_t)0xff;
	    bu_vls_printf(v, "%02lx", val);
	}
    }
}


struct bu_bitv *
bu_hex_to_bitv(const char *str)
{
    char abyte[3];
    const char *str_start;
    unsigned int len = 0;
    int bytes;
    struct bu_bitv *bv;
    size_t word_count;
    size_t chunksize = 0;
    volatile size_t BVS = sizeof(bitv_t);

    abyte[2] = '\0';

    /* skip over any initial white space */
    while (isspace((int)(*str)))
	str++;

    str_start = str;
    /* count hex digits */
    while (isxdigit((int)(*str)))
	str++;

    len = str - str_start;

    if (len < 2 || len % 2) {
	/* Must be two digits per byte */
	bu_log("ERROR: bitv length is not a multiple of two.\nIllegal hex bitv: [%s]\n", str_start);
	return (struct bu_bitv *)NULL;
    }

    bytes = len / 2; /* two hex digits per byte */
    word_count = bytes / BVS;
    chunksize = bytes % BVS;
    if (chunksize == 0) {
	chunksize = BVS;
    } else {
	/* handle partial chunk before using chunksize == BVS */
	word_count++;
    }

    bv = bu_bitv_new(len * 4); /* 4 bits per hex digit */

    str = str_start;
    while (word_count--) {
	while (chunksize--) {
	    unsigned long ulval;
	    /* prepare for error checking the next conversion */
	    char *endptr;
	    errno = 0;

	    /* get next two hex digits from string */
	    abyte[0] = *str++;
	    abyte[1] = *str++;

	    /* convert into an unsigned long */
	    ulval = strtoul(abyte, &endptr, 16);

	    /* check for various errors (from 'man strol') */
	    if ((errno == ERANGE && (ulval == ULONG_MAX))
		|| (errno != 0 && ulval == 0)) {
		bu_log("ERROR: strtoul: %s\n", strerror(errno));
		bu_bitv_free(bv);
		bv = (struct bu_bitv *)NULL;
		goto ERROR_RETURN;
	    }
	    if (endptr == abyte) {
		bu_log("ERROR: no digits were found in '%s'\n", abyte);
		bu_bitv_free(bv);
		bv = (struct bu_bitv *)NULL;
		goto ERROR_RETURN;
	    }
	    /* parsing was successful */

	    /* set the appropriate bits in the bit vector */
	    bv->bits[word_count] |= (bitv_t)ulval<<(chunksize * BITS_PER_BYTE);
	}
	chunksize = BVS;
    }

ERROR_RETURN:

    return bv;
}


struct bu_bitv *
bu_bitv_dup(register const struct bu_bitv *bv)
{
    struct bu_bitv *bv2;

    if (!bv)
	return bu_bitv_new(0);

    bv2 = bu_bitv_new(bv->nbits);
    bu_bitv_or(bv2, bv);

    return bv2;
}


void
bu_bitv_to_binary(struct bu_vls *v, const struct bu_bitv *bv)
{
    int i;
    size_t len;

    BU_CK_VLS(v);
    BU_CK_BITV(bv);

    len = bv->nbits;

    /* clear incoming vls, then extend to ensure enough room */
    bu_vls_trunc(v, 0);
    bu_vls_extend(v, len);

    /* we use the GCC binary format: '0bnnnnn..." */
    bu_vls_strcat(v, "0b");

    /* Visit all the bits from left (len - 1) to right (0) */
    for (i = (int)len - 1; i >= 0; --i) {
	if (!BU_BITTEST(bv, i))
	    bu_vls_strcat(v, "0");
	else
	    bu_vls_strcat(v, "1");
    }
}


struct bu_bitv *
bu_binary_to_bitv(const char *str)
{
    /* The incoming binary string is expected to be in the GCC format:
     * "0b<1 or more binary digits>'" (and it may have leading and
     * trailing spaces).  The input bit length will be increased to a
     * multiple of eight if necessary when converting to a bitv_t.
     *
     *   Example 1: "  0b00110100 " ('b' may be 'B' if desired)
     *
     * Note that only the zeroes and ones are actually tested and any
     * other character will ignored.  So one can ease producing the
     * binary input string by using spaces or other characters to
     * group the bits as shown in Examples 2, 3, and 4:
     *
     *   Example 2: "  0b 0011 0100 "
     *
     *   Example 3: "  0b_0011_0100 "
     *
     *   Example 4: "  0b|0011|0100 "
     *
     * Note also that an empty input ('0b') results in a valid but
     * zero-value bitv.
     *
     */
    return bu_binary_to_bitv2(str, 0);
}


struct bu_bitv *
bu_binary_to_bitv2(const char *str, const int nbytes)
{
    /* The incoming binary string is expected to be in the GCC format:
     * "0b<1 or more binary digits>'" (and it may have leading and
     * trailing spaces).  The input bit length will be increased to a
     * multiple of eight if necessary when converting to a bitv_t.
     * The "nbytes" argument may be zero if the user has no minimum
     * length preference.
     *
     *   Example 1: "  0b00110100 " ('b' may be 'B' if desired)
     *
     * Note that only the zeroes and ones are actually tested and any
     * other character will ignored.  So one can ease producing the
     * binary input string by using spaces or other characters
     * to group the bits as shown in Examples 2, 3, and 4:
     *
     *   Example 2: "  0b 0011 0100 "
     *
     *   Example 3: "  0b_0011_0100 "
     *
     *   Example 4: "  0b|0011|0100 "
     *
     * Note also that an empty input ('0b') results in a valid but
     * zero-value bitv.
     *
     */
    struct bu_vls v  = BU_VLS_INIT_ZERO;
    struct bu_vls v2 = BU_VLS_INIT_ZERO;
    unsigned nbits = nbytes > 0 ? nbytes * BITS_PER_BYTE : 0;
    size_t i, j, vlen, new_vlen, len = 0;
    int err = 0;
    struct bu_bitv *bv;
    char abyte[9];
    size_t word_count;
    size_t chunksize = 0;
    volatile size_t BVS = sizeof(bitv_t); /* should be 1 byte as defined in bu/bitv.h */
    size_t bytes;

    /* copy the input string and remove leading and trailing white space */
    bu_vls_strcpy(&v, str);
    bu_vls_trimspace(&v);

    /* check first two chars (and remove them) */
    if (*bu_vls_cstr(&v) != '0')
	++err;
    bu_vls_nibble(&v, 1);

    if (*bu_vls_cstr(&v) != 'b' && *bu_vls_cstr(&v) != 'B')
	++err;
    bu_vls_nibble(&v, 1);

    /* close up any embedded non-zero or non-one spaces */
    bu_vls_extend(&v2, nbits ? 2 * nbits : 2 * bu_vls_strlen(&v));
    bu_vls_trunc(&v2, 0);
    for (i = 0; i < bu_vls_strlen(&v); ++i) {
	const char c = bu_vls_cstr(&v)[i];
	if (c != '0' && c != '1')
	    continue;
	bu_vls_printf(&v2, "%c", c);
    }

    if (err)
	bu_exit(1, "Incorrect binary format '%s' (should be '0b<binary digits>').\n", str);

    /* zero-length input is okay, but we always need to add leading
     * zeroes to get a bit-length which is a multiple of eight
     */
    vlen = bu_vls_strlen(&v2);
    new_vlen = nbits > vlen ? nbits : vlen;
    if (new_vlen < BITS_PER_BYTE) {
	new_vlen = BITS_PER_BYTE;
    }
    else if (new_vlen % BITS_PER_BYTE) {
	new_vlen = (new_vlen / BITS_PER_BYTE) * BITS_PER_BYTE + BITS_PER_BYTE;
    }

    if (new_vlen > vlen) {
	size_t needed_zeroes = new_vlen - vlen;
	for (i = 0; i < needed_zeroes; ++i) {
	    bu_vls_prepend(&v2, "0");
	}
	vlen = new_vlen;
    }

    len = vlen;
    bv = bu_bitv_new(len); /* note it is initialized to all zeroes */
    BU_CK_BITV(bv);

    /* note the final length of the bitv may be greater due to word sizes, etc. */

    abyte[8] = '\0';
    bytes = len / BITS_PER_BYTE; /* eight digits per byte */
    word_count = bytes / BVS;
    chunksize = bytes % BVS;

    if (chunksize == 0) {
	chunksize = BVS;
    } else {
	/* handle partial chunk before using chunksize == BVS */
	word_count++;
    }

    j = 0;
    while (word_count--) {
	while (chunksize--) {
	    unsigned long ulval;
	    /* prepare for error checking the next conversion */
	    char *endptr;
	    errno = 0;

	    /* get next eight binary digits from string */
	    for (i = 0; i < BITS_PER_BYTE; ++i) {
		abyte[i] = bu_vls_cstr(&v2)[j++];
	    }

	    /* convert into an unsigned long */
	    ulval = strtoul(abyte, (char **)&endptr, 2);

	    /* check for various errors (from 'man strol') */
	    if ((errno == ERANGE && (ulval == ULONG_MAX))
		|| (errno != 0 && ulval == 0)) {
		bu_log("ERROR: strtoul: %s\n", strerror(errno));
		bu_bitv_free(bv);
		bv = (struct bu_bitv *)NULL;
		goto ERROR_RETURN;
	    }
	    if (endptr == abyte) {
		bu_log("ERROR: no digits were found in '%s'\n", abyte);
		bu_bitv_free(bv);
		bv = (struct bu_bitv *)NULL;
		goto ERROR_RETURN;
	    }
	    /* parsing was successful */

	    /* set the appropriate bits in the bit vector */
	    bv->bits[word_count] |= (bitv_t)ulval  << (chunksize * BITS_PER_BYTE);
	}
	chunksize = BVS;
    }

ERROR_RETURN:

    bu_vls_free(&v);
    bu_vls_free(&v2);

    return bv;
}


int
bu_bitv_compare_equal(const struct bu_bitv *bv1, const struct bu_bitv *bv2)
{
    /* Returns 1 (true) iff lengths are the same and each bit is
     * identical, 0 (false) otherwise.
     */
    int i;

    BU_CK_BITV(bv1);
    BU_CK_BITV(bv2);

    if (bv1->nbits != bv2->nbits)
	return 0;

    for (i = 0; i < (int)bv1->nbits; ++i) {
	int i1 = BU_BITTEST(bv1, i) ? 1 : 0;
	int i2 = BU_BITTEST(bv2, i) ? 1 : 0;
	if (i1 != i2)
	    return 0;
    }

    return 1;
}


int
bu_bitv_compare_equal2(const struct bu_bitv *bv1, const struct bu_bitv *bv2)
{
    /* Returns 1 (true) iff each non-zero bit is identical, 0 (false) otherwise. */
    BU_CK_BITV(bv1);
    BU_CK_BITV(bv2);

    if (bv1->nbits > bv2->nbits) {
	int i, j;
	int maxlen = (int)bv1->nbits;
	int minlen = (int)bv2->nbits;
	for (i = 0; i < minlen; ++i) {
	    int i1 = BU_BITTEST(bv1, i) ? 1 : 0;
	    int i2 = BU_BITTEST(bv2, i) ? 1 : 0;
	    if (i1 != i2)
		return 0;
	}
	/* remainder of bv1 bits must be zero */
	for (j = i; j < maxlen; ++j) {
	    int i1 = BU_BITTEST(bv1, j) ? 1 : 0;
	    if (i1)
		return 0;
	}
    } else if (bv2->nbits > bv1->nbits) {
	int i, j;
	int maxlen = (int)bv2->nbits;
	int minlen = (int)bv1->nbits;
	for (i = 0; i < minlen; ++i) {
	    int i1 = BU_BITTEST(bv1, i) ? 1 : 0;
	    int i2 = BU_BITTEST(bv2, i) ? 1 : 0;
	    if (i1 != i2)
		return 0;
	}
	/* remainder of bv2 bits must be zero */
	for (j = i; j < maxlen; ++j) {
	    int i1 = BU_BITTEST(bv2, j) ? 1 : 0;
	    if (i1)
		return 0;
	}
    } else {
	/* equal nbits */
	int i;
	int len = (int)bv1->nbits;
	for (i = 0; i < len; ++i) {
	    int i1 = BU_BITTEST(bv1, i) ? 1 : 0;
	    int i2 = BU_BITTEST(bv2, i) ? 1 : 0;
	    if (i1 != i2)
		return 0;
	}
    }

    return 1;
}


int
bu_binstr_to_hexstr(const char *bstr, struct bu_vls *h)
{
    struct bu_vls b   = BU_VLS_INIT_ZERO;
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    size_t len;
    size_t i;
    char abyte[BITS_PER_BYTE + 1];
    int have_prefix = 0;
    int results = BRLCAD_OK;

    if (!bstr) {
	bu_log("ERROR:  NULL 'bstr'\n");
	results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }
    if (!h) {
	bu_log("ERROR:  NULL 'h'\n");
	results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }

    bu_vls_strcpy(&b, bstr);
    bu_vls_trimspace(&b);
    len = bu_vls_strlen(&b);

    /* check for '0b' or '0B' */
    if (bu_vls_cstr(&b)[0] == '0' && len >= 2) {
	const char c1 = bu_vls_cstr(&b)[1];
	if (c1 == 'b' || c1 == 'B') {
	    have_prefix = 1;
	    bu_vls_nibble(&b, 2);
	    len = bu_vls_strlen(&b);
	}
    }

    /* eliminate non-binary characters used for user-spacing */
    bu_vls_vlscatzap(&tmp, &b);
    for (i = 0; i < len; ++i) {
	const char c = bu_vls_cstr(&tmp)[i];
	if (c == '0' || c == '1')
	    bu_vls_putc(&b, c);
    }
    len = bu_vls_strlen(&b);

    /* check valid length, pad with leading zeroes if necessary to get
     * an integral number of bytes */
    if (len < BITS_PER_BYTE || len % BITS_PER_BYTE) {
	size_t new_len, leading_zeroes;
	bu_log("WARNING:  Incoming string length (%zu) not an integral number of bytes,\n", len);
	bu_log("          padding with leading zeroes to ensure such.\n");
	if (len < BITS_PER_BYTE)
	    new_len = BITS_PER_BYTE;
	else
	    new_len = (len/BITS_PER_BYTE) * BITS_PER_BYTE + BITS_PER_BYTE;
	leading_zeroes = new_len - len;
	for (i = 0; i < leading_zeroes; ++i)
	    bu_vls_prepend(&b, "0");
	len = bu_vls_strlen(&b);
    }

    abyte[BITS_PER_BYTE] = '\0';

    bu_vls_trunc(h, 0);
    bu_vls_extend(h, len);
    for (i = 0; i < len; /* i is incremented inside first loop below */) {
	int j, k = BITS_PER_HEXCHAR;
	unsigned long ulval;
	/* prepare for error checking the next conversion */
	char *endptr;
	errno = 0;

	/* get next eight binary digits (one byte) from string; note i
	 * is incremented here */
	for (j = 0; j < BITS_PER_BYTE; ++j) {
	    abyte[j] = bu_vls_cstr(&b)[i++];
	}

	/* convert into an unsigned long */
	ulval = strtoul(abyte, (char **)&endptr, BINARY_BASE);
#if 0
	bu_log("DEBUG: abyte: '%s'; ulval: 0x%08x\n", abyte, ulval);
#endif

	/* check for various errors (from 'man strol') */
	if ((errno == ERANGE && (ulval == ULONG_MAX))
	    || (errno != 0 && ulval == 0)) {
	    bu_log("ERROR: strtoul: %s\n", strerror(errno));
	    results = BRLCAD_ERROR;
	    goto ERROR_RETURN;
	}
	if (endptr == abyte) {
	    bu_log("ERROR: no digits were found in '%s'\n", abyte);
	    results = BRLCAD_ERROR;
	    goto ERROR_RETURN;
	}
	/* parsing was successful */

	for (j = 0; j < HEXCHARS_PER_BYTE; ++j, k -= BITS_PER_HEXCHAR)
	    bu_vls_printf(h, "%1lx", (ulval >> k) & 0xf);
    }
    bu_vls_putc(h, '\0');

    if (have_prefix)
	bu_vls_prepend(h, "0x");

ERROR_RETURN:

    bu_vls_free(&b);
    bu_vls_free(&tmp);

    return results;
}


int
bu_hexstr_to_binstr(const char *hstr, struct bu_vls *b)
{
    struct bu_vls h   = BU_VLS_INIT_ZERO;
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    size_t len;
    size_t i;
    char abyte[HEXCHARS_PER_BYTE + 1];
    int have_prefix = 0;
    int results = BRLCAD_OK;

    if (!hstr) {
	bu_log("ERROR:  NULL 'hstr'\n");
	results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }
    if (!b) {
	bu_log("ERROR:  NULL 'b'\n");
	results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }

    bu_vls_strcpy(&h, hstr);
    bu_vls_trimspace(&h);
    len = bu_vls_strlen(&h);

    /* check for '0x' or '0X' */
    if (bu_vls_cstr(&h)[0] == '0' && len >= 2) {
	const char c1 = bu_vls_cstr(&h)[1];
	if (c1 == 'x' || c1 == 'X') {
	    have_prefix = 1;
	    bu_vls_nibble(&h, 2);
	    len = bu_vls_strlen(&h);
	}
    }

    /* eliminate non-hex characters used for user-spacing */
    bu_vls_vlscatzap(&tmp, &h);
    for (i = 0; i < len; ++i) {
	const unsigned char c = bu_vls_cstr(&tmp)[i];
	if (isxdigit(c))
	    bu_vls_putc(&h, c);
    }
    len = bu_vls_strlen(&h);

    /* check valid length, pad with leading zeroes if necessary to get
     * an integral number of bytes */
    if (len < HEXCHARS_PER_BYTE || len % HEXCHARS_PER_BYTE) {
	size_t new_len, leading_zeroes;
	bu_log("WARNING:  Incoming string length (%zu) not an integral number of bytes,\n", len);
	bu_log("          padding with leading zeroes to ensure such.\n");
	if (len < HEXCHARS_PER_BYTE)
	    new_len = HEXCHARS_PER_BYTE;
	else
	    new_len = (len/HEXCHARS_PER_BYTE) * HEXCHARS_PER_BYTE + HEXCHARS_PER_BYTE;
	leading_zeroes = new_len - len;
	for (i = 0; i < leading_zeroes; ++i)
	    bu_vls_prepend(&h, "0");
	len = bu_vls_strlen(&h);
    }

    abyte[HEXCHARS_PER_BYTE] = '\0';

    bu_vls_trunc(b, 0);
    bu_vls_extend(b, len * BITS_PER_HEXCHAR + (have_prefix ? 2 : 0));
    for (i = 0; i < len; /* i is incremented inside first loop below */) {
	size_t j, k = BITS_PER_BYTE;
	unsigned long ulval;
	/* prepare for error checking the next conversion */
	char *endptr;
	errno = 0;

	/* get next two hex digits (one byte) from string; note i is
	 * incremented here */
	for (j = 0; j < HEXCHARS_PER_BYTE; ++j) {
	    abyte[j] = bu_vls_cstr(&h)[i++];
	}

	/* convert into an unsigned long */
	ulval = strtoul(abyte, (char **)&endptr, HEX_BASE);
#if 0
	bu_log("DEBUG: abyte: '%s'; ulval: 0x%08x\n", abyte, ulval);
#endif

	/* check for various errors (from 'man strol') */
	if ((errno == ERANGE && (ulval == ULONG_MAX))
	    || (errno != 0 && ulval == 0)) {
	    bu_log("ERROR: strtoul: %s\n", strerror(errno));
	    results = BRLCAD_ERROR;
	    goto ERROR_RETURN;
	}
	if (endptr == abyte) {
	    bu_log("ERROR: no digits were found in '%s'\n", abyte);
	    results = BRLCAD_ERROR;
	    goto ERROR_RETURN;
	}
	/* parsing was successful */

	for (j = 0; j < BITS_PER_BYTE; ++j)
	    bu_vls_printf(b, "%1lx", (ulval >> --k) & 0x00000001);
    }
    bu_vls_putc(b, '\0');

    if (have_prefix)
	bu_vls_prepend(b, "0b");

ERROR_RETURN:

    bu_vls_free(&h);
    bu_vls_free(&tmp);

    return results;

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
