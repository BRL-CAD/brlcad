/*                    B U _ B 6 4 . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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
#include <string.h>

#include "bu.h"


/* Test for bu_b64_encode/bu_b64_decode for reversibility:
 *
 *   1. encode the input string
 *   2. decode the encoded string
 *   3. decoded string should be the same as input
 *
 */
int
test_encode(const char *str)
{
    int status = 0;
    int decoded_size = 0;
    char *encoded = NULL;
    char *decoded = NULL;

    encoded = bu_b64_encode(str);
    decoded_size = bu_b64_decode(&decoded, encoded);

    if (BU_STR_EQUAL(str, decoded) && decoded_size == (int)strlen(str)) {
	printf("%s -> %s -> %s [PASS]\n", str, encoded, decoded);
    } else {
	printf("%s -> %s -> %s [FAIL]\n", str, encoded, decoded);
	status = 1;
    }

    bu_free(encoded, "free encoded");
    bu_free(decoded, "free decoded");

    return status;
}


int
main(int ac, char *av[])
{
    if (ac != 1)
	fprintf(stderr,"Usage: %s \n", av[0]);

    test_encode("hello world!");
    test_encode("!@#&#$%@&#$^@(*&^%(#$@&^#*$nasty_string!<>?");

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
