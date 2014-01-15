/*                 B U _ B I T V _ T O _ H E X . C
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
main(int argc , char *argv[])
{
    int length = 0;
    int ret;

    if(argc < 4)
      return -1;

    sscanf(argv[3], "%d", &length);

    ret = test_bu_bitv_to_hex(argv[1], argv[2], length);
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
