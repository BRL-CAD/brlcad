/*                     T E S T _ C T Y P E . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2014 United States Government as represented by
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

#include <string.h>
#include <ctype.h>

#include "bu.h"


int
test_str_isprint(const char *inp , int in_exp)
{
    int res;
    res = bu_str_isprint(inp);
    if (res == in_exp) {

	if (res)
	    printf("Testing with string : %10s is printable->PASSED!\n", inp);
	else
	    printf("Given string not printable->PASSED!\n");

	return 1;
    }
    printf("Failed\n");
    return 0;
}


int
main(int argc, char **argv)
{
    int test_num = 0;

    if (argc < 2) {
       fprintf(stderr,"Usage: %s test_num\n", argv[0]);
       return 1;
    }

    sscanf(argv[1], "%d", &test_num);

    switch (test_num) {
	case 1:
	    return !test_str_isprint("abc", 1);
	case 2:
	    return !test_str_isprint("abc123\n", 0);	/* \n is end of line -not printable */
	case 3:
	    return !test_str_isprint("abc123\\n1!", 1);
	case 4:
	    return !test_str_isprint("123\txyz", 0);	/* \t is horizontal tab - not printable */
	case 5:
	    return !test_str_isprint("#$ ab12", 1);
	case 6:
	    return !test_str_isprint("#$%\n 748", 0);	/* \n is end of line -not printable */
	case 7:
	    return !test_str_isprint("#$^\ry", 0);	/* \r is carriage return - not printable */
    }

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
