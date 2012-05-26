/*                 T E S T _ B I T V . C
 * BRL-CAD
 *
 * Copyright (c) 2012 United States Government as represented by
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

#include "bu.h"


unsigned int
power(unsigned int base , int exponent)
{
    int i ;
    unsigned int product = 1;

    for (i = 0; i < exponent; i++) {
	product = product*base;
    }

    return product;
}


int
test_bu_hex_to_bitv(char *inp, char *res , int errno)
{
    struct bu_bitv *res_bitv ;
    int pass;

    res_bitv = bu_hex_to_bitv(inp);

    if (errno == 1 && res_bitv == NULL) {
	printf("\nbu_hex_to_bitv PASSED Input:%s Output:%s", inp, res);
	return 0;
    }

    if (!bu_strcmp((char*)res_bitv->bits, res)) {
	printf("\nbu_hex_to_bitv PASSED Input:%s Output:%s", inp, (char*)res_bitv->bits);
	pass = 1 ;
    } else {
	printf("\nbu_hex_to_bitv FAILED for Input:%s Expected:%s", inp, res);
	pass = 0;
    }

    bu_bitv_free(res_bitv);

    return pass;
}


int
test_bu_bitv_to_hex(char *inp , char *res , int length)
{
    struct bu_vls *a;
    struct bu_bitv *res_bitv;
    int pass;

    a = bu_vls_vlsinit();
    res_bitv = bu_bitv_new(length);

    /* accessing the bits array directly as a char* is not safe since
     * there's no bounds checking and assumes implementation is
     * contiguous memory.
     */
    bu_strlcpy((char*)res_bitv->bits, inp, length/8);

    bu_bitv_to_hex(a, res_bitv);

    if (!bu_strcmp(a->vls_str, res)) {
	printf("\nbu_bitv_to_hex PASSED Input:%5s Output:%9s", inp, res);
	pass = 1 ;
    } else {
	printf("\nbu_bitv_to_hex FAILED for Input:%s Expected:%s", inp, res);
	pass = 0;
    }

    bu_bitv_free(res_bitv);
    bu_vls_free(a);

    return pass;
}


int
test_bu_bitv_vls(char *inp , char *exp)
{
    struct bu_vls *a;
    struct bu_bitv *res_bitv;
    int pass;

    a = bu_vls_vlsinit();
    res_bitv = bu_hex_to_bitv(inp);
    bu_bitv_vls(a, res_bitv);

    if (!bu_strcmp(a->vls_str, exp)) {
	printf("\nbu_bitv_vls test PASSED Input:%s Output:%s", inp, (char *)a->vls_str);
	pass = 1;
    } else {
	printf("\nbu_bitv_vls FAILED for Input:%s Expected:%s", inp, exp);
	pass = 0;
    }

    bu_vls_free(a);
    bu_bitv_free(res_bitv);

    return pass;
}


int
test_bu_bitv_or(char *inp1 , char *inp2 , char *exp)
{
    struct bu_bitv *res_bitv , *res_bitv1 , *result;
    struct bu_vls *a , *b;
    int pass;

    a = bu_vls_vlsinit();
    b = bu_vls_vlsinit();

    res_bitv1 = bu_hex_to_bitv(inp1);
    res_bitv  = bu_hex_to_bitv(inp2);
    result    = bu_hex_to_bitv(exp);

    bu_bitv_or(res_bitv1, res_bitv);
    bu_bitv_vls(a, res_bitv1);
    bu_bitv_vls(b, result);

    if (!bu_strcmp(a->vls_str, b->vls_str)) {
	printf("\nbu_bitv_or test PASSED Input1:%s Input2:%s Output:%s", inp1, inp2, exp);
	pass = 1;
    } else {
	printf("\nbu_bitv_or test FAILED Input1:%s Input2:%s Expected:%s", inp1, inp2, exp);
	pass = 0;
    }

    bu_bitv_free(res_bitv);
    bu_bitv_free(res_bitv1);
    bu_bitv_free(result);

    return pass;
}


int
test_bu_bitv_and(char *inp1 , char *inp2 , char *exp)
{
    struct bu_bitv *res_bitv , *res_bitv1 , *result;
    struct bu_vls *a , *b;
    int pass;

    a = bu_vls_vlsinit();
    b = bu_vls_vlsinit();

    res_bitv1 = bu_hex_to_bitv(inp1);
    res_bitv  = bu_hex_to_bitv(inp2);
    result    = bu_hex_to_bitv(exp);

    bu_bitv_and(res_bitv1,res_bitv);
    bu_bitv_vls(a,res_bitv1);
    bu_bitv_vls(b,result);

    if (!bu_strcmp(a->vls_str , b->vls_str)) {
	printf("\nbu_bitv_and test PASSED Input1:%s Input2:%s Output:%s", inp1, inp2, exp);
	pass = 1;
    } else {
	printf("\nbu_bitv_and test FAILED Input1:%s Input2:%s Expected:%s", inp1, inp2, exp);
	pass = 0;
    }

    bu_bitv_free(res_bitv);
    bu_bitv_free(res_bitv1);
    bu_bitv_free(result);

    return pass;
}


int
main(int ac , char **av)
{
    int res , pass = 1;

    /* unused variables generate warnings, and sometimes warnings are treated as errors*/
    if (ac) {};
    if (av) {};

    /*test bu_bitv_shift*/
    res = bu_bitv_shift();

    printf("\nTesting bu_bitv_shift...");
    if (power(2, res) <= (sizeof(bitv_t) * 8) && power(2, res + 1) > (sizeof(bitv_t) * 8)) {
	printf("\nPASSED: bu_bitv_shift working");
    } else {
	printf("\nFAILED: bu_bitv_shift incorrect");
	pass = 0;
    }

    printf("\n\n");

    /*testing bu_hex_to_bitv*/
    printf("Testing bu_hex_to_bitv...");
    test_bu_hex_to_bitv("33323130", "0123", 0);
    test_bu_hex_to_bitv("30", "0", 0);
    printf("\n(Intentionally invoked an error here; a message about it is normal!)");
    test_bu_hex_to_bitv("303", "", 1);
    printf("\n\n");

    /*testing bu_bitv_to_hex*/
    printf("Testing bu_bitv_to_hex...");
    pass *= test_bu_bitv_to_hex("0123", "33323130", 32);
    pass *= test_bu_bitv_to_hex("12", "3231", 16);
    printf("\n\n");

    /* testing bu_bitv_vls */
    printf("Testing bu_bitv_vls...");
    pass *= test_bu_bitv_vls("00000000", "() ");
    pass *= test_bu_bitv_vls("f0f0f0f0", "(4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31) ");
    printf("\n\n");

    /*test bu_bitv_or*/
    printf("Testing bu_bitv_or...");
    pass *= test_bu_bitv_or("ffffffff", "00000000", "ffffffff");
    pass *= test_bu_bitv_or("ab00", "1200", "bb00");
    printf("\n\n");

    /*test bu_bitv_and*/
    printf("Testing bu_bitv_and...");
    pass *= test_bu_bitv_and("ffffffff", "00000000", "00000000");
    pass *= test_bu_bitv_and("ab00", "1200", "0200");
    printf("\n\n");

    return (1  - pass);
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
