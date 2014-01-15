/*                 B U _ H E X _ T O  _ B I T V . C
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
test_bu_hex_to_bitv(char *inp, char *res , int errno)
{
    struct bu_bitv *res_bitv ;
    int pass;

    res_bitv = bu_hex_to_bitv(inp);

    if (errno == 1 && res_bitv == NULL) {
	printf("\nbu_hex_to_bitv PASSED Input:%s Output:%s", inp, res);
	return 1;
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
main(int argc , char *argv[])
{
    int error_code = 0;
    int ret;

    if(argc < 4)
      return -1;

    sscanf(argv[3], "%d", &error_code);

    ret = test_bu_hex_to_bitv(argv[1], argv[2], error_code);
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
