/*                 B U _ B I T V _ V L S . C
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
main(int argc , char *argv[])
{
    int ret;

    if(argc < 3)
      return -1;

    ret = test_bu_bitv_vls(argv[1], argv[2]);
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
