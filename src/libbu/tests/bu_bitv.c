/*                       B U _ B I T V . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
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
#include "../bu_internals.h"
#include "./test_internals.h"


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
    int test_results = CTEST_FAIL;
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
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }
    if (bv_rbs == NULL) {
	bu_log("\nERROR: NULL from bu_binary_to_bitv.");
	test_results = CTEST_FAIL;
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
	test_results = CTEST_PASS;
    else
	test_results = CTEST_FAIL;

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
    int test_results = CTEST_FAIL;
    const char *input, *expected;

    if (argc < 4)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    input    = argv[2];
    expected = argv[3];

    res_bitv = bu_hex_to_bitv(input);
    if (res_bitv == NULL) {
	printf("\nbu_hex_to_bitv FAILED Input: '%s' Output: 'NULL' Expected: '%s'",
	       input, expected);
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }

    bu_bitv_vls(&v, res_bitv);
    test_results = bu_strcmp(bu_vls_cstr(&v), expected);

    if (!test_results)
	printf("\nbu_hex_to_bitv PASSED");
    else
	printf("\nbu_hex_to_bitv FAILED");

    printf("\n  Input:    '%s'", input);
    printf("\n  Output:   '%s'", bu_vls_cstr(&v));
    printf("\n  Expected: '%s'", expected);

ERROR_RETURN:

    if (res_bitv != NULL)
	bu_bitv_free(res_bitv);
    bu_vls_free(&v);

    /* a false return above is a CTEST_PASS, so the value is the same
     * as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}


static int
test_bu_bitv_vls(int argc, char **argv)
{
    /*         argv[1]    argv[2]             argv[3]
     * inputs: <func num> <input hex string> <expected bitv dump>
     */
    int test_results = CTEST_FAIL;
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
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }

    bu_bitv_vls(&a, res_bitv);
    test_results = bu_strcmp(bu_vls_cstr(&a), expected);

    if (!test_results)
	printf("\nbu_bitv_vls test PASSED");
    else
	printf("\nbu_bitv_vls test FAILED");

    printf("\n  Input: '%s'", input);
    printf("\n  Output: '%s'", bu_vls_cstr(&a));
    printf("\n  Expected: '%s'", expected);

ERROR_RETURN:

    bu_vls_free(&a);
    bu_bitv_free(res_bitv);

    /* a false return above is a CTEST_PASS, so the value is the same
     * as ctest's CTEST_PASS/CTEST_FAIL */
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
    int test_results = CTEST_FAIL;

    if (argc < 5)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    length = atoi(argv[4]);
    if (length < (int)BU_BITS_PER_BYTE || length % (int)BU_BITS_PER_BYTE) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    input    = argv[2];
    expected = argv[3];

    result_bitv = bu_bitv_new(length);

    if (result_bitv == NULL) {
	bu_log("\nERROR: NULL from bu_bitv_new.");
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }

    /* accessing the bits array directly as a char* is not safe since
     * there's no bounds checking and assumes implementation is
     * contiguous memory.
     */
    bu_strlcpy((char*)result_bitv->bits, input, length/BU_BITS_PER_BYTE);

    bu_bitv_to_hex(&result, result_bitv);

    test_results = bu_strcmp(bu_vls_cstr(&result), expected);

    if (!test_results)
        printf("\nbu_bitv_to_hex PASSED");
    else
        printf("\nbu_bitv_to_hex FAILED");

    printf("\n  Input: '%s'", input);
    printf("\n  Output: '%s'", bu_vls_cstr(&result));
    printf("\n  Expected: '%s'", expected);

ERROR_RETURN:

    if (result_bitv)
	bu_bitv_free(result_bitv);
    bu_vls_free(&result);

    /* a false return above is a CTEST_PASS, so the value is the same
     * as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}


static unsigned int
power(const unsigned int base, const int exponent)
{
    int i ;
    unsigned int product = 1;

    for (i = 0; i < exponent; i++)
	product *= base;

    return product;
}


static int
test_bu_bitv_shift()
{
    int res;
    int test_results = CTEST_FAIL;

    printf("\nTesting bu_bitv_shift...");

    /*test bu_bitv_shift*/
    res = bu_bitv_shift();

    if (power(2, res) <= (sizeof(bitv_t) * BU_BITS_PER_BYTE)
	&& power(2, res + 1) > (sizeof(bitv_t) * BU_BITS_PER_BYTE)) {
	test_results = CTEST_PASS;
	printf("\nPASSED: bu_bitv_shift working");
    } else {
	printf("\nFAILED: bu_bitv_shift incorrect");
	test_results = CTEST_FAIL;
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
    int test_results = CTEST_FAIL;
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
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }
    res_bitv  = bu_hex_to_bitv(input2);
    if (res_bitv == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }
    result    = bu_hex_to_bitv(expected);
    if (result == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }

    bu_bitv_or(res_bitv1, res_bitv);
    bu_bitv_vls(&a, res_bitv1);
    bu_bitv_vls(&b, result);

    test_results = bu_strcmp(bu_vls_cstr(&a), bu_vls_cstr(&b));

    if (!test_results)
	printf("\nbu_bitv_or test PASSED");
    else
	printf("\nbu_bitv_or test FAILED");

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

    /* a false return above is a CTEST_PASS, so the value is the same
     * as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
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
    int test_results = CTEST_FAIL;
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
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }
    res_bitv  = bu_hex_to_bitv(input2);
    if (res_bitv == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }
    result    = bu_hex_to_bitv(expected);
    if (result == NULL) {
	bu_log("\nERROR: NULL from bu_hex_to_bitv.");
	test_results = CTEST_FAIL;
	goto ERROR_RETURN;
    }

    bu_bitv_and(res_bitv1, res_bitv);
    bu_bitv_vls(&a, res_bitv1);
    bu_bitv_vls(&b, result);

    test_results = bu_strcmp(bu_vls_cstr(&a), bu_vls_cstr(&b));

    if (!test_results)
	printf("\nbu_bitv_and test PASSED");
    else
	printf("\nbu_bitv_and test FAILED");

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

    /* a false return above is a CTEST_PASS, so the value is the same
     * as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}


static int
test_bu_bitv_compare_equal(int argc, char **argv)
{
    /*          argv[1]    argv[2]         argv[3]  argv[4]         argv[5]
     *  inputs: <func num> <binary string> <nbytes> <binary string> <nbytes>
     */
    int test_results = CTEST_FAIL;
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
     * CTEST_PASS/CTEST_FAIL */
    return !test_results;
}


static int
test_bu_bitv_compare_equal2(int argc, char **argv)
{
    /*          argv[1]    argv[2]         argv[3]  argv[4]         argv[5]
     *  inputs: <func num> <binary string> <nbytes> <binary string> <nbytes>
     */
    int test_results = CTEST_FAIL;
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
     * CTEST_PASS/CTEST_FAIL */
    return !test_results;
}



static int
test_bu_bitv_to_binary(int argc, char **argv)
{
    /*         argv[1]    argv[2]         argv[3]
     * inputs: <func num> <input hex num> <expected binary string>
     */
    int test_results = CTEST_FAIL;
    unsigned long int input_num = strtoul(argv[2], (char **)NULL, 16);
    struct bu_vls v = BU_VLS_INIT_ZERO;
    struct bu_bitv *b1, *b2;
    unsigned len;
    int i;

    if (argc < 4)
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);

    bu_vls_strcpy(&v, argv[3]);
    b1 = bu_bitv_new(sizeof(input_num) * BU_BITS_PER_BYTE);
    b2 = bu_binary_to_bitv(bu_vls_cstr(&v));
    printf("\nExpected bitv: '%s'", bu_vls_cstr(&v));
    printf("\nExpected bitv dump:");
    dump_bitv(b2);

    BU_CK_VLS(&v);
    BU_CK_BITV(b1);
    BU_CK_BITV(b2);

    len = sizeof(input_num) * BU_BITS_PER_BYTE;

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

    /* a true return above is a pass, invert the value to CTEST_PASS/CTEST_FAIL */
    return !test_results;
}


static int
test_bu_binary_to_bitv(int argc, char **argv)
{
    /*         argv[1]    argv[2]         argv[3]
     * inputs: <func num> <binary string> <expected hex num>
     */
    int test_results = CTEST_FAIL;
    unsigned long int expected_num = strtol(argv[3], (char **)NULL, 16);
    struct bu_vls v = BU_VLS_INIT_ZERO;
    struct bu_bitv *b;
    unsigned i, len, err = 0;
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

    test_results = err ? CTEST_FAIL : CTEST_PASS;

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
    int test_results = CTEST_FAIL;
    unsigned nbytes = (unsigned)strtol(argv[3], (char **)NULL, 10);
    unsigned long long int expected_num = strtol(argv[4], (char **)NULL, 16);
    unsigned ull_size = sizeof(unsigned long long);
    struct bu_vls v = BU_VLS_INIT_ZERO;
    struct bu_bitv *b;
    unsigned i, len, err = 0;

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

    test_results = err ? CTEST_FAIL : CTEST_PASS;

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
    int test_results = CTEST_FAIL;
    const char *input, *expected;
    struct bu_vls v = BU_VLS_INIT_ZERO;

    if (argc < 4)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    input    = argv[2];
    expected = argv[3];

    bu_binstr_to_hexstr(input, &v);

    test_results = bu_strcmp(expected, bu_vls_cstr(&v));

    if (!test_results)
	printf("\nbu_binstr_to_hexstr PASSED");
    else
	printf("\nbu_binstr_to_hexstr FAILED");

    printf("\n  Input:    '%s'", input);
    printf("\n  Output:   '%s'", bu_vls_cstr(&v));
    printf("\n  Expected: '%s'", expected);

    bu_vls_free(&v);

    /* a false return above is a CTEST_PASS, so the value is the same
     * as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}

static int
test_bu_hexstr_to_binstr(int argc, char **argv)
{
    /*         argv[1]    argv[2]            argv[3]
     * inputs: <func num> <input hex string> <expected binary string>
     */
    int test_results = CTEST_FAIL;
    const char *input, *expected;
    struct bu_vls v = BU_VLS_INIT_ZERO;

    if (argc < 4)
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);

    input    = argv[2];
    expected = argv[3];

    bu_hexstr_to_binstr(input, &v);

    test_results = bu_strcmp(expected, bu_vls_cstr(&v));

    if (!test_results)
	printf("\nbu_hexstr_to_binstr PASSED");
    else
	printf("\nbu_hexstr_to_binstr FAILED");

    printf("\n  Input:    '%s'", input);
    printf("\n  Output:   '%s'", bu_vls_cstr(&v));
    printf("\n  Expected: '%s'", expected);

    bu_vls_free(&v);

    /* a false return above is a CTEST_PASS, so the value is the same
     * as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}


int
main(int argc, char **argv)
{
    int function_num = 0;

    if (argc < 2) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
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
