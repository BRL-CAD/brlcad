/*                 B U _ B I T V _ S H I F T . C
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
main(int UNUSED(argc), char **UNUSED(argv))
{
    int res = 1;
    int pass = 1;

    printf("\nTesting bu_bitv_shift...");

    /*test bu_bitv_shift*/
    res = bu_bitv_shift();

    if (power(2, res) <= (sizeof(bitv_t) * 8) && power(2, res + 1) > (sizeof(bitv_t) * 8)) {
	printf("\nPASSED: bu_bitv_shift working");
    } else {
	printf("\nFAILED: bu_bitv_shift incorrect");
	pass = 0;
    }

    return !pass;
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
