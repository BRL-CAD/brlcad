/*                 B U _ B I T V _ A N D . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
main(int argc , char *argv[])
{
    int ret;

    if(argc < 4)
      return -1;

    ret = test_bu_bitv_and(argv[1], argv[2], argv[3]);
    return !ret;
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
