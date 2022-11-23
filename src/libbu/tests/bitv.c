/*                       B I T V . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2022 United States Government as represented by
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

#include "bu.h"
#include "../bitv.h"


typedef enum {
    HEX         = 0x0001,
    HEX_RAW     = 0x0011,
    BINARY      = 0x0100,
    BINARY_RAW  = 0x1100
} hex_bin_enum_t;


static void
random_hex_or_binary_string(struct bu_vls *v, const hex_bin_enum_t typ, const int nbytes)
{
    const char hex_chars[] = "0123456789abcdef";
    const char bin_chars[] = "01";
    const int nstrchars = (typ & HEX) ? nbytes * HEXCHARS_PER_BYTE : nbytes * BITS_PER_BYTE;
    const char *chars = (typ & HEX) ? hex_chars : bin_chars;
    const int nchars = (typ & HEX) ? sizeof(hex_chars)/sizeof(char) : sizeof(bin_chars)/sizeof(char);
    int i;

    srand((unsigned)time(NULL));

    bu_vls_trunc(v, 0);
    bu_vls_extend(v, nchars);
    for (i = 0; i < nstrchars; ++i) {
	long int r = rand();
	int n = r ? (int)(r % (nchars - 1)) : 0;
	char c = chars[n];
	bu_vls_putc(v, c);
    }

    if (typ == HEX) {
	bu_vls_prepend(v, "0x");
    } else if (typ == BINARY) {
	bu_vls_prepend(v, "0b");
    }

}


static int
test_bu_bitv_master()
{
    /*
     * This tests a round trip capability to
     *
     * 1. a. Get a random hex nine-byte string and convert it to a bitv.
     *    b. Convert the bitv to a hex string.
     *    c. Compare the two hex strings successfully.
     * 2. a. Get a random binary nine-byte string and convert it to a bitv.
     *    b. Convert the bitv to a binary string.
     *    c. Compare the two binary strings successfully.
     */
    struct bu_vls rand_hex_str = BU_VLS_INIT_ZERO;
    struct bu_vls rand_bin_str = BU_VLS_INIT_ZERO;
    struct bu_vls bv2hex_str   = BU_VLS_INIT_ZERO;
    struct bu_vls bv2bin_str   = BU_VLS_INIT_ZERO;
    struct bu_bitv *bv_rhs;
    struct bu_bitv *bv_rbs;
    int test_results = BRLCAD_ERROR;
    int thex, tbin;

    /* get the random strings */
    random_hex_or_binary_string(&rand_hex_str, HEX_RAW, 9);
    random_hex_or_binary_string(&rand_bin_str, BINARY, 9);

    /* convert to bitvs */
    bv_rhs = bu_hex_to_bitv(bu_vls_cstr(&rand_hex_str));
    bv_rbs = bu_binary_to_bitv(bu_vls_cstr(&rand_bin_str));

    /* check for failures */
    if (bv_rhs == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }
    if (bv_rbs == NULL) {
	bu_log("\nERROR: NULL from bu_binary_to_bitv.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }


    /* bitvs back to strings */
    bu_bitv_to_hex(&bv2hex_str, bv_rhs);
    bu_bitv_to_binary(&bv2bin_str, bv_rbs);

    /* finally, the comparisons (not case sensitive) */
    thex = BU_STR_EQUIV(bu_vls_cstr(&rand_hex_str), bu_vls_cstr(&bv2hex_str));
    tbin = BU_STR_EQUIV(bu_vls_cstr(&rand_bin_str), bu_vls_cstr(&bv2bin_str));

    if (thex)
	printf("\nbu_hex_bitv_master PASSED");
    else
	printf("\nbu_hex_bitv_master FAILED");

    printf("\n  Input:  '%s'", bu_vls_cstr(&rand_hex_str));
    printf("\n  Output: '%s'", bu_vls_cstr(&bv2hex_str));

    if (tbin)
	printf("\nbu_bin_bitv_master PASSED");
    else
	printf("\nbu_bin_bitv_master FAILED");

    printf("\n  Input:  '%s'", bu_vls_cstr(&rand_bin_str));
    printf("\n  Output: '%s'", bu_vls_cstr(&bv2bin_str));

    /* both tests must pass for success */
    if (thex && tbin)
	test_results = BRLCAD_OK;
    else
	test_results = BRLCAD_ERROR;

ERROR_RETURN:

    if (bv_rhs)
	bu_bitv_free(bv_rhs);
    if (bv_rbs)
	bu_bitv_free(bv_rbs);
    bu_vls_free(&rand_hex_str);
    bu_vls_free(&rand_bin_str);
    bu_vls_free(&bv2hex_str);
    bu_vls_free(&bv2bin_str);

    return test_results;
}


static int
test_bu_hex_to_bitv(int argc, char **argv)
{
    /*         argv[1]    argv[2]            argv[3]
     * inputs: <func num> <input hex string> <expected bitv dump>
     */
    struct bu_bitv *res_bitv ;
    struct bu_vls v = BU_VLS_INIT_ZERO;
    int test_results = BRLCAD_ERROR;
    const char *input, *expected;

    if (argc < 4)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    input    = argv[2];
    expected = argv[3];

    res_bitv = bu_hex_to_bitv(input);
    if (res_bitv == NULL) {
	printf("\nbu_hex_to_bitv FAILED Input: '%s' Output: 'NULL' Expected: '%s'",
	       input, expected);
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }

    bu_bitv_vls(&v, res_bitv);

    if (BU_STR_EQUAL(bu_vls_cstr(&v), expected)) {
	test_results = BRLCAD_OK;
	printf("\nbu_hex_to_bitv PASSED");
    } else {
	test_results = BRLCAD_ERROR;
	printf("\nbu_hex_to_bitv FAILED");
    }

    printf("\n  Input:    '%s'", input);
    printf("\n  Output:   '%s'", bu_vls_cstr(&v));
    printf("\n  Expected: '%s'", expected);

ERROR_RETURN:

    if (res_bitv != NULL)
	bu_bitv_free(res_bitv);
    bu_vls_free(&v);

    return test_results;
}


static int
test_bu_bitv_vls(int argc, char **argv)
{
    /*         argv[1]    argv[2]             argv[3]
     * inputs: <func num> <input hex string> <expected bitv dump>
     */
    int test_results = BRLCAD_ERROR;
    struct bu_vls a = BU_VLS_INIT_ZERO;
    struct bu_bitv *res_bitv;
    const char *input, *expected;

    if (argc < 4)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    input    = argv[2];
    expected = argv[3];

    res_bitv = bu_hex_to_bitv(input);
    if (res_bitv == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }

    bu_bitv_vls(&a, res_bitv);

    if (BU_STR_EQUAL(bu_vls_cstr(&a), expected)) {
	test_results = BRLCAD_OK;
	printf("\nbu_bitv_vls test PASSED");
    } else {
	test_results = BRLCAD_ERROR;
	printf("\nbu_bitv_vls test FAILED");
    }

    printf("\n  Input: '%s'", input);
    printf("\n  Output: '%s'", bu_vls_cstr(&a));
    printf("\n  Expected: '%s'", expected);

ERROR_RETURN:

    bu_vls_free(&a);
    bu_bitv_free(res_bitv);

    return test_results;
}


static int
test_bu_bitv_to_hex(int argc, char **argv)
{
    /*         argv[1]    argv[2]             argv[3]               argv[4]
     * inputs: <func num> <input char string> <expected hex string> <expected bitv length>
     */
    struct bu_vls result = BU_VLS_INIT_ZERO;
    struct bu_bitv *result_bitv;
    int length;
    const char *input, *expected;
    int test_results = BRLCAD_ERROR;

    if (argc < 5)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    length = atoi(argv[4]);
    if (length < (int)BITS_PER_BYTE || length % BITS_PER_BYTE) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    input    = argv[2];
    expected = argv[3];

    result_bitv = bu_bitv_new(length);

    if (result_bitv == NULL) {
	bu_log("\nERROR: NULL from bu_bitv_new.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }

    /* accessing the bits array directly as a char* is not safe since
     * there's no bounds checking and assumes implementation is
     * contiguous memory.
     */
    bu_strlcpy((char*)result_bitv->bits, input, length/BITS_PER_BYTE);

    bu_bitv_to_hex(&result, result_bitv);

    if (BU_STR_EQUAL(bu_vls_cstr(&result), expected)) {
	test_results = BRLCAD_OK;
	printf("\nbu_bitv_to_hex PASSED");
    } else {
	test_results = BRLCAD_ERROR;
	printf("\nbu_bitv_to_hex FAILED");
    }

    printf("\n  Input: '%s'", input);
    printf("\n  Output: '%s'", bu_vls_cstr(&result));
    printf("\n  Expected: '%s'", expected);

ERROR_RETURN:

    if (result_bitv)
	bu_bitv_free(result_bitv);
    bu_vls_free(&result);

    return test_results;
}


static unsigned int
power(const unsigned int base, const size_t exponent)
{
    size_t i ;
    unsigned int product = 1;

    for (i = 0; i < exponent; i++)
	product *= base;

    return product;
}


static int
test_bu_bitv_shift()
{
    size_t res;
    int test_results = BRLCAD_ERROR;

    printf("\nTesting bu_bitv_shift...");

    /*test bu_bitv_shift*/
    res = bu_bitv_shift();

    if (power(2, res) <= (sizeof(bitv_t) * BITS_PER_BYTE)
	&& power(2, res + 1) > (sizeof(bitv_t) * BITS_PER_BYTE)) {
	test_results = BRLCAD_OK;
	printf("\nPASSED: bu_bitv_shift working");
    } else {
	printf("\nFAILED: bu_bitv_shift incorrect");
	test_results = BRLCAD_ERROR;
    }

    return test_results;
}


static int
test_bu_bitv_or(int argc , char **argv)
{
    /*         argv[1]    argv[2]              argv[3]              argv[4]
     * inputs: <func num> <input hex string 1> <input hex string 2> <expected hex result>
     */
    struct bu_bitv *res_bitv , *res_bitv1 , *result;
    struct bu_vls a = BU_VLS_INIT_ZERO;
    struct bu_vls b = BU_VLS_INIT_ZERO;
    int test_results = BRLCAD_ERROR;
    const char *input1, *input2, *expected;

    if (argc < 5)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    input1   = argv[2];
    input2   = argv[3];
    expected = argv[4];

    res_bitv1 = res_bitv = result = NULL;
    res_bitv1 = bu_hex_to_bitv(input1);
    if (res_bitv1 == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }
    res_bitv  = bu_hex_to_bitv(input2);
    if (res_bitv == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }
    result    = bu_hex_to_bitv(expected);
    if (result == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }

    bu_bitv_or(res_bitv1, res_bitv);
    bu_bitv_vls(&a, res_bitv1);
    bu_bitv_vls(&b, result);

    if (BU_STR_EQUAL(bu_vls_cstr(&a), bu_vls_cstr(&b))) {
	test_results = BRLCAD_OK;
	printf("\nbu_bitv_or test PASSED");
    } else {
	test_results = BRLCAD_ERROR;
	printf("\nbu_bitv_or test FAILED");
    }

    printf("\n  Input1:   '%s'", input1);
    printf("\n  Input2:   '%s'", input2);
    printf("\n  Expected: '%s'", expected);

ERROR_RETURN:

    bu_vls_free(&a);
    bu_vls_free(&b);
    if (res_bitv)
	bu_bitv_free(res_bitv);
    if (res_bitv1)
	bu_bitv_free(res_bitv1);
    if (result)
	bu_bitv_free(result);

    return test_results;
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
    }

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


static int
test_bu_bitv_and(int argc, char **argv)
{
    /*         argv[1]    argv[2]              argv[3]              argv[4]
     * inputs: <func_num> <input hex string 1> <input hex string 2> <expected hex result>
     */
    struct bu_bitv *res_bitv , *res_bitv1 , *result;
    struct bu_vls a = BU_VLS_INIT_ZERO;
    struct bu_vls b = BU_VLS_INIT_ZERO;
    int test_results = BRLCAD_ERROR;
    const char *input1, *input2, *expected;

    if (argc < 5) {
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);
    }

    input1   = argv[2];
    input2   = argv[3];
    expected = argv[4];

    res_bitv1 = res_bitv = result = NULL;
    res_bitv1 = bu_hex_to_bitv(input1);
    if (res_bitv1 == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }
    res_bitv  = bu_hex_to_bitv(input2);
    if (res_bitv == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }
    result    = bu_hex_to_bitv(expected);
    if (result == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = BRLCAD_ERROR;
	goto ERROR_RETURN;
    }

    bu_bitv_and(res_bitv1, res_bitv);
    bu_bitv_vls(&a, res_bitv1);
    bu_bitv_vls(&b, result);

    if (BU_STR_EQUAL(bu_vls_cstr(&a), bu_vls_cstr(&b))) {
	test_results = BRLCAD_OK;
	printf("\nbu_bitv_and test PASSED");
    } else {
	test_results = BRLCAD_ERROR;
	printf("\nbu_bitv_and test FAILED");
    }

    printf("   Input1:   '%s'", input1);
    printf("   Input2:   '%s'", input2);
    printf("   Output:   '%s'", bu_vls_cstr(&b));
    printf("   Expected: '%s'", expected);

ERROR_RETURN:

    bu_vls_free(&a);
    bu_vls_free(&b);
    if (res_bitv)
	bu_bitv_free(res_bitv);
    if (res_bitv1)
	bu_bitv_free(res_bitv1);
    if (result)
	bu_bitv_free(result);

    return test_results;
}


static int
test_bu_bitv_compare_equal(int argc, char **argv)
{
    /*          argv[1]    argv[2]         argv[3]  argv[4]         argv[5]
     *  inputs: <func num> <binary string> <nbytes> <binary string> <nbytes>
     */
    int test_results = BRLCAD_ERROR;
    struct bu_vls v1 = BU_VLS_INIT_ZERO;
    struct bu_vls v2 = BU_VLS_INIT_ZERO;
    struct bu_bitv *b1, *b2;
    int lenbytes1 = atoi(argv[3]);
    int lenbytes2 = atoi(argv[5]);

    if (argc < 6)
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);

    bu_vls_strcpy(&v1, argv[2]);
    bu_vls_strcpy(&v2, argv[4]);

    b1 = bu_binary_to_bitv2(bu_vls_cstr(&v1), lenbytes1);
    b2 = bu_binary_to_bitv2(bu_vls_cstr(&v2), lenbytes2);

    BU_CK_VLS(&v1);
    BU_CK_VLS(&v2);
    BU_CK_BITV(b1);
    BU_CK_BITV(b2);

    printf("\nInput bitv 1: '%s'", bu_vls_cstr(&v1));
    printf("\nExpected bitv dump:");
    dump_bitv(b1);

    printf("\nInput bitv 2: '%s'", bu_vls_cstr(&v2));
    printf("\nExpected bitv dump:");
    dump_bitv(b2);

    /* see what another function does */
    bu_vls_trunc(&v1, 0);
    bu_vls_trunc(&v2, 0);
    bu_bitv_vls(&v1, b1);
    bu_bitv_vls(&v2, b2);
    printf("\nInput bitv 1 from vls: '%s'", bu_vls_cstr(&v1));
    printf("\nInput bitv 2 from vls: '%s'", bu_vls_cstr(&v2));

    test_results = bu_bitv_compare_equal(b1, b2);

    bu_vls_free(&v1);
    bu_vls_free(&v2);
    bu_bitv_free(b1);
    bu_bitv_free(b2);

    /* a true return above is a pass, invert the value to
     * BRLCAD_OK/BRLCAD_ERROR */
    return !test_results;
}


static int
test_bu_bitv_compare_equal2(int argc, char **argv)
{
    /*          argv[1]    argv[2]         argv[3]  argv[4]         argv[5]
     *  inputs: <func num> <binary string> <nbytes> <binary string> <nbytes>
     */
    int test_results = BRLCAD_ERROR;
    struct bu_vls v1 = BU_VLS_INIT_ZERO;
    struct bu_vls v2 = BU_VLS_INIT_ZERO;
    struct bu_bitv *b1, *b2;
    int lenbytes1 = atoi(argv[3]);
    int lenbytes2 = atoi(argv[5]);

    if (argc < 6)
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);

    bu_vls_strcpy(&v1, argv[2]);
    bu_vls_strcpy(&v2, argv[4]);

    b1 = bu_binary_to_bitv2(bu_vls_cstr(&v1), lenbytes1);
    b2 = bu_binary_to_bitv2(bu_vls_cstr(&v2), lenbytes2);

    BU_CK_VLS(&v1);
    BU_CK_VLS(&v2);
    BU_CK_BITV(b1);
    BU_CK_BITV(b2);

    printf("\nInput bitv 1: '%s'", bu_vls_cstr(&v1));
    printf("\nExpected bitv dump:");
    dump_bitv(b1);

    printf("\nInput bitv 2: '%s'", bu_vls_cstr(&v2));
    printf("\nExpected bitv dump:");
    dump_bitv(b2);

    /* see what another function does */
    bu_vls_trunc(&v1, 0);
    bu_vls_trunc(&v2, 0);
    bu_bitv_vls(&v1, b1);
    bu_bitv_vls(&v2, b2);
    printf("\nInput bitv 1 from vls: '%s'", bu_vls_cstr(&v1));
    printf("\nInput bitv 2 from vls: '%s'", bu_vls_cstr(&v2));

    test_results = bu_bitv_compare_equal2(b1, b2);

    bu_vls_free(&v1);
    bu_vls_free(&v2);
    bu_bitv_free(b1);
    bu_bitv_free(b2);

    /* a true return above is a pass, invert the value to
     * BRLCAD_OK/BRLCAD_ERROR */
    return !test_results;
}


static int
test_bu_bitv_to_binary(int argc, char **argv)
{
    /*         argv[1]    argv[2]         argv[3]
     * inputs: <func num> <input hex num> <expected binary string>
     */
    int test_results = BRLCAD_ERROR;
    unsigned long int input_num = strtoul(argv[2], (char **)NULL, 16);
    struct bu_vls v = BU_VLS_INIT_ZERO;
    struct bu_bitv *b1, *b2;
    unsigned len;
    int i;

    if (argc < 4)
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);

    bu_vls_strcpy(&v, argv[3]);
    b1 = bu_bitv_new(sizeof(input_num) * BITS_PER_BYTE);
    b2 = bu_binary_to_bitv(bu_vls_cstr(&v));
    printf("\nExpected bitv: '%s'", bu_vls_cstr(&v));
    printf("\nExpected bitv dump:");
    dump_bitv(b2);

    BU_CK_VLS(&v);
    BU_CK_BITV(b1);
    BU_CK_BITV(b2);

    len = sizeof(input_num) * BITS_PER_BYTE;

    /* set the bits of the new bitv */
    for (i = 0; i < (int)len; ++i) {
	/* get the expected bit */
	unsigned long i1 = (input_num >> i) & 0x1;
	if (i1)
	    BU_BITSET(b1, i);
    }

    printf("\nInput hex:              '0x%lx'", (unsigned long)input_num);
    printf("\nExpected binary string: '%s'", argv[3]);

    printf("\nOutput bitv: '%s'", argv[3]);
    printf("\nOutput bitv dump:");
    dump_bitv(b1);

    /* compare the two bitvs (without regard to length) */
    test_results = bu_bitv_compare_equal2(b1, b2);

    bu_vls_free(&v);
    bu_bitv_free(b1);
    bu_bitv_free(b2);

    /* a true return above is a pass, invert the value to BRLCAD_OK/BRLCAD_ERROR */
    return !test_results;
}


static int
test_bu_binary_to_bitv(int argc, char **argv)
{
    /*         argv[1]    argv[2]         argv[3]
     * inputs: <func num> <binary string> <expected hex num>
     */
    int test_results = BRLCAD_ERROR;
    unsigned long int expected_num = strtol(argv[3], (char **)NULL, 16);
    struct bu_vls v = BU_VLS_INIT_ZERO;
    struct bu_bitv *b;
    size_t i, len, err = 0;
    unsigned ull_size = sizeof(unsigned long long);

    if (argc < 4)
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);

    bu_vls_strcpy(&v, argv[2]);
    b = bu_binary_to_bitv(bu_vls_cstr(&v));
    printf("\nNew bitv: '%s'", bu_vls_cstr(&v));
    printf("\nNew bitv dump:");
    dump_bitv(b);

    BU_CK_VLS(&v);
    BU_CK_BITV(b);

    len = b->nbits;

    /* now compare the bitv with the expected int */
    len = len > ull_size ? ull_size : len;
    for (i = 0; i < len; ++i) {
	/* get the expected bit */
	unsigned long i1 = (expected_num >> i) & 0x01;
	unsigned long i2 = BU_BITTEST(b, i) ? 1 : 0;
	if (i1 != i2) {
	    ++err;
	    break;
	}
    }

    printf("\n  Input binary string: '%s'", argv[2]);
    printf("\n  Expected hex:        '0x%x'", (unsigned)expected_num);

    bu_vls_trunc(&v, 0);
    bu_bitv_vls(&v, b);
    printf("\nInput bitv: '%s'", bu_vls_cstr(&v));
    printf("\nInput bitv dump:");
    dump_bitv(b);

    test_results = err ? BRLCAD_ERROR : BRLCAD_OK;

    bu_vls_free(&v);
    bu_bitv_free(b);

    return test_results;
}


static int
test_bu_binary_to_bitv2(int argc, char **argv)
{
    /*         argv[1]    argv[2]         argv[3]      argv[4]
     * inputs: <func num> <binary string> <min nbytes> <expected hex num>
     */
    int test_results = BRLCAD_ERROR;
    unsigned nbytes = (unsigned)strtol(argv[3], (char **)NULL, 10);
    unsigned long long int expected_num = strtol(argv[4], (char **)NULL, 16);
    unsigned ull_size = sizeof(unsigned long long);
    struct bu_vls v = BU_VLS_INIT_ZERO;
    struct bu_bitv *b;
    size_t i, len, err = 0;

    if (argc < 5)
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);

    bu_vls_strcpy(&v, argv[2]);
    b = bu_binary_to_bitv2(bu_vls_cstr(&v), nbytes);
    printf("\nNew bitv: '%s'", bu_vls_cstr(&v));
    printf("\nNew bitv dump:");
    dump_bitv(b);

    BU_CK_VLS(&v);
    BU_CK_BITV(b);

    len = b->nbits;

    /* now compare the bitv with the expected int */
    len = len > ull_size ? ull_size : len;
    for (i = 0; i < len; ++i) {
	/* get the expected bit */
	unsigned long long i1 = (expected_num >> i) & 0x01;
	unsigned long long i2 = BU_BITTEST(b, i) ? 1 : 0;
	if (i1 != i2) {
	    ++err;
	    break;
	}
    }

    printf("\n  Input binary string: '%s'", argv[4]);
    printf("\n  Expected hex:        '0x%x'", (unsigned)expected_num);

    bu_vls_trunc(&v, 0);
    bu_bitv_vls(&v, b);
    printf("\nInput bitv: '%s'", bu_vls_cstr(&v));
    printf("\nInput bitv dump:");
    dump_bitv(b);

    test_results = err ? BRLCAD_ERROR : BRLCAD_OK;

    bu_vls_free(&v);
    bu_bitv_free(b);

    return test_results;
}


static int
test_bu_binstr_to_hexstr(int argc, char **argv)
{
    /*         argv[1]    argv[2]               argv[3]
     * inputs: <func num> <input binary string> <expected hex string>
     */
    int test_results = BRLCAD_ERROR;
    const char *input, *expected;
    struct bu_vls v = BU_VLS_INIT_ZERO;

    if (argc < 4)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    input    = argv[2];
    expected = argv[3];

    bu_binstr_to_hexstr(input, &v);

    if (BU_STR_EQUAL(expected, bu_vls_cstr(&v))) {
	test_results = BRLCAD_OK;
	printf("\nbu_binstr_to_hexstr PASSED");
    } else {
	test_results = BRLCAD_ERROR;
	printf("\nbu_binstr_to_hexstr FAILED");
    }

    printf("\n  Input:    '%s'", input);
    printf("\n  Output:   '%s'", bu_vls_cstr(&v));
    printf("\n  Expected: '%s'", expected);

    bu_vls_free(&v);

    return test_results;
}


static int
test_bu_binstr_to_hexstr_empty_input(int argc, char **argv)
{
    /*         argv[1]    ""                    argv[2]
     * inputs: <func num> <input binary string> <expected hex string>
     */
    int test_results = BRLCAD_ERROR;
    const char *input = "";
    const char *expected;
    struct bu_vls v = BU_VLS_INIT_ZERO;

    if (argc < 3)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    expected = argv[2];

    bu_binstr_to_hexstr(input, &v);

    if (BU_STR_EQUAL(expected, bu_vls_cstr(&v))) {
	test_results = BRLCAD_OK;
	printf("\nbu_binstr_to_hexstr PASSED");
    } else {
	test_results = BRLCAD_ERROR;
	printf("\nbu_binstr_to_hexstr FAILED");
    }

    printf("\n  Input:    '%s'", input);
    printf("\n  Output:   '%s'", bu_vls_cstr(&v));
    printf("\n  Expected: '%s'", expected);

    bu_vls_free(&v);

    return test_results;
}


static int
test_bu_hexstr_to_binstr(int argc, char **argv)
{
    /*         argv[1]    argv[2]            argv[3]
     * inputs: <func num> <input hex string> <expected binary string>
     */
    int test_results = BRLCAD_ERROR;
    const char *input, *expected;
    struct bu_vls v = BU_VLS_INIT_ZERO;

    if (argc < 4)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    input    = argv[2];
    expected = argv[3];

    bu_hexstr_to_binstr(input, &v);

    if (BU_STR_EQUAL(expected, bu_vls_cstr(&v))) {
	test_results = BRLCAD_OK;
	printf("\nbu_hexstr_to_binstr PASSED");
    } else {
	test_results = BRLCAD_ERROR;
	printf("\nbu_hexstr_to_binstr FAILED");
    }

    printf("\n  Input:    '%s'", input);
    printf("\n  Output:   '%s'", bu_vls_cstr(&v));
    printf("\n  Expected: '%s'", expected);

    bu_vls_free(&v);

    return test_results;
}


int
main(int argc, char *argv[])
{
    int function_num = 0;

    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_log("Usage: %s {function_num} {function_test_arg0} {...}", argv[0]);
	bu_exit(1, "ERROR: missing function number\n");
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 0:
	    return test_bu_bitv_master();
	    break;
	case 1:
	    return test_bu_binary_to_bitv(argc, argv);
	    break;
	case 2:
	    return test_bu_bitv_to_binary(argc, argv);
	    break;
	case 3:
	    return test_bu_bitv_compare_equal(argc, argv);
	    break;
	case 4:
	    return test_bu_bitv_compare_equal2(argc, argv);
	    break;
	case 5:
	    return test_bu_binary_to_bitv2(argc, argv);
	    break;
	case 6:
	    return test_bu_bitv_and(argc, argv);
	    break;
	case 7:
	    return test_bu_bitv_or(argc, argv);
	    break;
	case 8:
	    return test_bu_bitv_shift();
	    break;
	case 9:
	    return test_bu_bitv_vls(argc, argv);
	    break;
	case 10:
	    return test_bu_bitv_to_hex(argc, argv);
	    break;
	case 11:
	    return test_bu_hex_to_bitv(argc, argv);
	    break;
	case 12:
	    return test_bu_hexstr_to_binstr(argc, argv);
	    break;
	case 13:
	    return test_bu_binstr_to_hexstr(argc, argv);
	case 14:
	    return test_bu_binstr_to_hexstr_empty_input(argc, argv);
	    break;

    }

    bu_log("ERROR: function_num %d is not valid [%s]\n", function_num, argv[0]);
    return 1;
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
