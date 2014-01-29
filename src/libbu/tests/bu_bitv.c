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
test_bu_bitv_compare_equal(int argc, char **argv)
{
    /*          argv[1]    argv[2]         argv[3]  argv[4]         argv[5]
     *  inputs: <func num> <binary string> <nbytes> <binary string> <nbytes>
     */
    int test_results = FAIL;
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

    b1 = bu_binary_to_bitv(bu_vls_cstr(v1), lenbytes1);
    b2 = bu_binary_to_bitv(bu_vls_cstr(v2), lenbytes2);

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

    /* a true return above is a pass, invert the value to PASS/FAIL */
    return !test_results;
}


static int
test_bu_bitv_compare_equal2(int argc, char **argv)
{
    /*          argv[1]    argv[2]         argv[3]  argv[4]         argv[5]
     *  inputs: <func num> <binary string> <nbytes> <binary string> <nbytes>
     */
    int test_results = FAIL;
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

    b1 = bu_binary_to_bitv(bu_vls_cstr(v1), lenbytes1);
    b2 = bu_binary_to_bitv(bu_vls_cstr(v2), lenbytes2);

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

    /* a true return above is a pass, invert the value to PASS/FAIL */
    return !test_results;
}



static int
test_bu_bitv_to_binary(int argc, char **argv)
{
    /*         argv[1]    argv[2]         argv[3]
     * inputs: <func num> <input hex num> <expected binary string>
     */
    int test_results = FAIL;
    unsigned long int input_num = strtoul(argv[2], (char **)NULL, 16);
    struct bu_vls *v = bu_vls_vlsinit();
    struct bu_bitv *b1, *b2;
    unsigned len;
    int i;

    if (argc < 2) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    bu_vls_strcpy(v, argv[3]);
    b1 = bu_bitv_new(sizeof(input_num) * 8);
    b2 = bu_binary_to_bitv(bu_vls_cstr(v), 0);
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

    /* a true return above is a pass, invert the value to PASS/FAIL */
    return !test_results;
}


static int
test_bu_binary_to_bitv(int argc, char **argv)
{
    /*         argv[1]    argv[2]         argv[3]
     * inputs: <func num> <binary string> <expected hex num>
     */
    int test_results = FAIL;
    unsigned long int expected_num = strtol(argv[3], (char **)NULL, 16);
    struct bu_vls *v = bu_vls_vlsinit();
    struct bu_bitv *b;
    unsigned i, len, err = 0;

    if (argc < 2) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    bu_vls_strcpy(v, argv[2]);
    b = bu_binary_to_bitv(bu_vls_cstr(v), 0);
    printf("\nNew bitv: '%s'", bu_vls_cstr(v));
    printf("\nNew bitv dump:");
    dump_bitv(b);

    BU_CK_VLS(v);
    BU_CK_BITV(b);

    len = b->nbits;

    /* now compare the bitv with the expected int */
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

    test_results = err ? FAIL : PASS;

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
