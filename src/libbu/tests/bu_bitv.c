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
#include "./test_internals.h"


static int
test_bu_hex_to_bitv(int argc, char **argv)
{
    /*         argv[1]    argv[2]            argv[3]
     * inputs: <func num> <input hex string> <expected char string>
     */
    struct bu_bitv *res_bitv ;
    int test_results = CTEST_FAIL;
    const char *input, *expected;

    if (argc < 4) {
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);
    }

    input    = argv[2];
    expected = argv[3];

    res_bitv = bu_hex_to_bitv(input);

    if (res_bitv == NULL) {
	printf("\nbu_hex_to_bitv FAILED Input: '%s' Output: 'NULL' Expected: '%s'",
	       input, expected);
	test_results = CTEST_FAIL;
    } else {
	test_results = bu_strcmp((char*)res_bitv->bits, expected);

	if (!test_results) {
	    printf("\nbu_hex_to_bitv PASSED Input: '%s' Output: '%s' Expected: '%s'",
		   input, (char *)res_bitv->bits, expected);
	} else {
	    printf("\nbu_hex_to_bitv FAILED Input: '%s' Output: '%s' Expected: '%s'",
		   input, (char *)res_bitv->bits, expected);
	}
    }

    if (res_bitv != NULL)
	bu_bitv_free(res_bitv);

    /* a false return above is a CTEST_PASS, so the value is the same as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}


static int
test_bu_bitv_vls(int argc, char **argv)
{
    /*         argv[1]    argv[2]             argv[3]
     * inputs: <func num> <input char string> <expected hex string>
     */
    int test_results = CTEST_FAIL;
    struct bu_vls *a;
    struct bu_bitv *res_bitv;
    const char *input, *expected;

    if (argc < 4) {
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);
    }

    input    = argv[2];
    expected = argv[3];

    a = bu_vls_vlsinit();
    res_bitv = bu_hex_to_bitv(input);
    bu_bitv_vls(a, res_bitv);

    test_results = bu_strcmp(bu_vls_cstr(a), expected);

    if (!test_results) {
	printf("\nbu_bitv_vls test PASSED Input: '%s' Output: '%s' Expected: '%s'",
	       input, bu_vls_cstr(a), expected);
    } else {
	printf("\nbu_bitv_vls FAILED for Input: '%s' Expected: '%s' Expected: '%s'",
	       input, bu_vls_cstr(a), expected);
    }

    bu_vls_free(a);
    bu_bitv_free(res_bitv);

    /* a false return above is a CTEST_PASS, so the value is the same as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}


static int
test_bu_bitv_to_hex(int argc, char **argv)
{
    /*         argv[1]    argv[2]             argv[3]               argv[4]
     * inputs: <func num> <input char string> <expected hex string> <expected bitv length>
     */
    struct bu_vls  *result;
    struct bu_bitv *result_bitv;
    int length;
    const char *input, *expected;
    int test_results = CTEST_FAIL;

    if (argc < 5) {
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);
    }

    length = atoi(argv[4]);
    if (length < (int)BITS_PER_BYTE || length % (int)BITS_PER_BYTE) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    input    = argv[2];
    expected = argv[3];

    result = bu_vls_vlsinit();
    result_bitv = bu_bitv_new(length);

    /* accessing the bits array directly as a char* is not safe since
     * there's no bounds checking and assumes implementation is
     * contiguous memory.
     */
    bu_strlcpy((char*)result_bitv->bits, input, length/BITS_PER_BYTE);

    bu_bitv_to_hex(result, result_bitv);

    test_results = bu_strcmp(bu_vls_cstr(result), expected);

    if (!test_results) {
        printf("bu_bitv_to_hex PASSED Input: '%s' Output: '%s' Expected: '%s'\n",
               input, bu_vls_cstr(result), expected);
    } else {
        printf("bu_bitv_to_hex FAILED for Input: '%s' Output: '%s' Expected: '%s'\n",
               input, bu_vls_cstr(result), expected);
    }

    bu_bitv_free(result_bitv);
    bu_vls_free(result);

    /* a false return above is a CTEST_PASS, so the value is the same as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}


static unsigned int
power(const unsigned int base, const int exponent)
{
    int i ;
    unsigned int product = 1;

    for (i = 0; i < exponent; i++) {
	product *= base;
    }

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

    if (power(2, res) <= (sizeof(bitv_t) * BITS_PER_BYTE)
	&& power(2, res + 1) > (sizeof(bitv_t) * BITS_PER_BYTE)) {
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
    struct bu_vls *a , *b;
    int test_results = CTEST_FAIL;
    const char *input1, *input2, *expected;

    if (argc < 5) {
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);
    }

    input1   = argv[2];
    input2   = argv[3];
    expected = argv[4];

    a = bu_vls_vlsinit();
    b = bu_vls_vlsinit();

    res_bitv1 = bu_hex_to_bitv(input1);
    res_bitv  = bu_hex_to_bitv(input2);
    result    = bu_hex_to_bitv(expected);

    bu_bitv_or(res_bitv1, res_bitv);
    bu_bitv_vls(a, res_bitv1);
    bu_bitv_vls(b, result);

    test_results = bu_strcmp(bu_vls_cstr(a), bu_vls_cstr(b));

    if (!test_results) {
	printf("\nbu_bitv_or test PASSED Input1: '%s' Input2: '%s' Output: '%s'",
	       input1, input2, expected);
    } else {
	printf("\nbu_bitv_or test FAILED Input1: '%s' Input2: '%s' Expected: '%s'",
	       input1, input2, expected);
    }

    bu_bitv_free(res_bitv);
    bu_bitv_free(res_bitv1);
    bu_bitv_free(result);

    /* a false return above is a CTEST_PASS, so the value is the same as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}


static int
test_bu_bitv_and(int argc, char **argv)
{
    /*         argv[1]    argv[2]              argv[3]              argv[4]
     * inputs: <func_num> <input hex string 1> <input hex string 2> <expected hex result>
     */
    struct bu_bitv *res_bitv , *res_bitv1 , *result;
    struct bu_vls *a , *b;
    int test_results = CTEST_FAIL;
    const char *input1, *input2, *expected;

    if (argc < 5) {
	bu_exit(1, "ERROR: input format: function_num function_test_args [%s]\n", argv[0]);
    }

    input1   = argv[2];
    input2   = argv[3];
    expected = argv[4];

    a = bu_vls_vlsinit();
    b = bu_vls_vlsinit();

    res_bitv1 = bu_hex_to_bitv(input1);
    res_bitv  = bu_hex_to_bitv(input2);
    result    = bu_hex_to_bitv(expected);

    bu_bitv_and(res_bitv1,res_bitv);
    bu_bitv_vls(a,res_bitv1);
    bu_bitv_vls(b,result);

    test_results = bu_strcmp(bu_vls_cstr(a), bu_vls_cstr(b));

    if (!test_results) {
	printf("\nbu_bitv_and test PASSED Input1: '%s' Input2: '%s' Output: '%s'",
	       input1, input2, expected);
    } else {
	printf("\nbu_bitv_and test FAILED Input1: '%s' Input2: '%s' Expected: '%s'",
	       input1, input2, expected);
    }

    bu_bitv_free(res_bitv);
    bu_bitv_free(res_bitv1);
    bu_bitv_free(result);

    /* a false return above is a CTEST_PASS, so the value is the same as ctest's CTEST_PASS/CTEST_FAIL */
    return test_results;
}


static int
test_bu_bitv_compare_equal(int argc, char **argv)
{
    /*          argv[1]    argv[2]         argv[3]  argv[4]         argv[5]
     *  inputs: <func num> <binary string> <nbytes> <binary string> <nbytes>
     */
    int test_results = CTEST_FAIL;
    struct bu_vls *v1 = bu_vls_vlsinit();
    struct bu_vls *v2 = bu_vls_vlsinit();
    struct bu_bitv *b1, *b2;
    int lenbytes1 = atoi(argv[3]);
    int lenbytes2 = atoi(argv[5]);

    if (argc < 6) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    bu_vls_strcpy(v1, argv[2]);
    bu_vls_strcpy(v2, argv[4]);

    b1 = bu_binary_to_bitv2(bu_vls_cstr(v1), lenbytes1);
    b2 = bu_binary_to_bitv2(bu_vls_cstr(v2), lenbytes2);

    BU_CK_VLS(v1);
    BU_CK_VLS(v2);
    BU_CK_BITV(b1);
    BU_CK_BITV(b2);

    printf("\nInput bitv 1: '%s'", bu_vls_cstr(v1));
    printf("\nExpected bitv dump:");
    dump_bitv(b1);

    printf("\nInput bitv 2: '%s'", bu_vls_cstr(v2));
    printf("\nExpected bitv dump:");
    dump_bitv(b2);

    /* see what another function does */
    bu_vls_trunc(v1, 0);
    bu_vls_trunc(v2, 0);
    bu_bitv_vls(v1, b1);
    bu_bitv_vls(v2, b2);
    printf("\nInput bitv 1 from vls: '%s'", bu_vls_cstr(v1));
    printf("\nInput bitv 2 from vls: '%s'", bu_vls_cstr(v2));

    test_results = bu_bitv_compare_equal(b1, b2);

    bu_vls_free(v1);
    bu_vls_free(v2);
    bu_bitv_free(b1);
    bu_bitv_free(b2);

    /* a true return above is a pass, invert the value to CTEST_PASS/CTEST_FAIL */
    return !test_results;
}


static int
test_bu_bitv_compare_equal2(int argc, char **argv)
{
    /*          argv[1]    argv[2]         argv[3]  argv[4]         argv[5]
     *  inputs: <func num> <binary string> <nbytes> <binary string> <nbytes>
     */
    int test_results = CTEST_FAIL;
    struct bu_vls *v1 = bu_vls_vlsinit();
    struct bu_vls *v2 = bu_vls_vlsinit();
    struct bu_bitv *b1, *b2;
    int lenbytes1 = atoi(argv[3]);
    int lenbytes2 = atoi(argv[5]);

    if (argc < 6) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    bu_vls_strcpy(v1, argv[2]);
    bu_vls_strcpy(v2, argv[4]);

    b1 = bu_binary_to_bitv2(bu_vls_cstr(v1), lenbytes1);
    b2 = bu_binary_to_bitv2(bu_vls_cstr(v2), lenbytes2);

    BU_CK_VLS(v1);
    BU_CK_VLS(v2);
    BU_CK_BITV(b1);
    BU_CK_BITV(b2);

    printf("\nInput bitv 1: '%s'", bu_vls_cstr(v1));
    printf("\nExpected bitv dump:");
    dump_bitv(b1);

    printf("\nInput bitv 2: '%s'", bu_vls_cstr(v2));
    printf("\nExpected bitv dump:");
    dump_bitv(b2);

    /* see what another function does */
    bu_vls_trunc(v1, 0);
    bu_vls_trunc(v2, 0);
    bu_bitv_vls(v1, b1);
    bu_bitv_vls(v2, b2);
    printf("\nInput bitv 1 from vls: '%s'", bu_vls_cstr(v1));
    printf("\nInput bitv 2 from vls: '%s'", bu_vls_cstr(v2));

    test_results = bu_bitv_compare_equal2(b1, b2);

    bu_vls_free(v1);
    bu_vls_free(v2);
    bu_bitv_free(b1);
    bu_bitv_free(b2);

    /* a true return above is a pass, invert the value to CTEST_PASS/CTEST_FAIL */
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
    struct bu_vls *v = bu_vls_vlsinit();
    struct bu_bitv *b1, *b2;
    unsigned len;
    int i;

    if (argc < 4) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    bu_vls_strcpy(v, argv[3]);
    b1 = bu_bitv_new(sizeof(input_num) * 8);
    b2 = bu_binary_to_bitv(bu_vls_cstr(v));
    printf("\nExpected bitv: '%s'", bu_vls_cstr(v));
    printf("\nExpected bitv dump:");
    dump_bitv(b2);

    BU_CK_VLS(v);
    BU_CK_BITV(b1);
    BU_CK_BITV(b2);

    len = sizeof(input_num) * 8;

    /* set the bits of the new bitv */
    for (i = 0; i < (int)len; ++i) {
	/* get the expected bit */
	unsigned long i1 = (input_num >> i) & 0x1;
	if (i1)
	    BU_BITSET(b1, i);
    }

    printf("\nInput hex:  '0x%lx'  Expected binary string: '%s'", (unsigned long)input_num, argv[3]);
    printf("\nOutput bitv: '%s'", argv[3]);
    printf("\nOutput bitv dump:");
    dump_bitv(b1);

    /* compare the two bitvs (without regard to length) */
    test_results = bu_bitv_compare_equal2(b1, b2);

    bu_vls_free(v);
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
    struct bu_vls *v = bu_vls_vlsinit();
    struct bu_bitv *b;
    unsigned i, len, err = 0;
    unsigned ull_size = sizeof(unsigned long long);

    if (argc < 4) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    bu_vls_strcpy(v, argv[2]);
    b = bu_binary_to_bitv(bu_vls_cstr(v));
    printf("\nNew bitv: '%s'", bu_vls_cstr(v));
    printf("\nNew bitv dump:");
    dump_bitv(b);

    BU_CK_VLS(v);
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

    printf("\nInput binary string: '%s' Expected hex: '0x%x'", argv[2], (unsigned)expected_num);
    bu_vls_trunc(v, 0);
    bu_bitv_vls(v, b);
    printf("\nInput bitv: '%s'", bu_vls_cstr(v));
    printf("\nInput bitv dump:");
    dump_bitv(b);

    test_results = err ? CTEST_FAIL : CTEST_PASS;

    bu_bitv_free(b);
    bu_vls_free(v);

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
    struct bu_vls *v = bu_vls_vlsinit();
    struct bu_bitv *b;
    unsigned i, len, err = 0;

    if (argc < 5) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    bu_vls_strcpy(v, argv[2]);
    b = bu_binary_to_bitv2(bu_vls_cstr(v), nbytes);
    printf("\nNew bitv: '%s'", bu_vls_cstr(v));
    printf("\nNew bitv dump:");
    dump_bitv(b);

    BU_CK_VLS(v);
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

    printf("\nInput binary string: '%s' Expected hex: '0x%x'", argv[4], (unsigned)expected_num);
    bu_vls_trunc(v, 0);
    bu_bitv_vls(v, b);
    printf("\nInput bitv: '%s'", bu_vls_cstr(v));
    printf("\nInput bitv dump:");
    dump_bitv(b);

    test_results = err ? CTEST_FAIL : CTEST_PASS;

    bu_bitv_free(b);
    bu_vls_free(v);

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
