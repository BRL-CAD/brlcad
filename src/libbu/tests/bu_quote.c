/*                    T E S T _ Q U O T E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2013 United States Government as represented by
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


/* Test for bu_vls_encode/bu_vls_decode for reversibility:
 *
 *   1. encode the input string
 *   2. decode the encoded string
 *   3. decoded string should be the same as input
 *
 */
int
test_quote(const char *str)
{
    int status = 0;
    int len_s = str ? strlen(str) : 0;
    int len_d = 0; /* for length of decoded */
    int f_wid = 28; /* desired total field width */
    struct bu_vls encoded = BU_VLS_INIT_ZERO;
    struct bu_vls decoded = BU_VLS_INIT_ZERO;

    bu_vls_encode(&encoded, str);
    bu_vls_decode(&decoded, bu_vls_addr(&encoded)); /* should be same as input string */

    len_d = bu_vls_strlen(&decoded);
    if (f_wid < len_s)
	f_wid = len_s + 1;
    if (f_wid < len_d)
	f_wid = len_d + 1;

    if (BU_STR_EQUAL(str, bu_vls_addr(&decoded))
	/* && !BU_STR_EQUAL(str, bu_vls_addr(&encoded)) */
	) {
	/* a hack for str showing '(null)' in printf if zero length */
	if (len_s == 0)
	    len_s = 6;
	printf("{%*s}%*s -> {%*s}%*s [PASS]\n",
	       len_s, str, f_wid - len_s, " ",
	       len_d, bu_vls_addr(&decoded), f_wid - len_d, " "
	       );
    } else {
	/* a hack for str showing '(null)' in printf if zero length */
	if (len_s == 0)
	    len_s = 6;
	printf("{%*s}%*s -> {%*s}%*s [FAIL]  (should be: {%s})\n",
	       len_s, str, f_wid - len_s, " ",
	       len_d, bu_vls_addr(&decoded), f_wid - len_d, " ",
	       str
	       );
	status = 1;
    }

    bu_vls_free(&encoded);
    bu_vls_free(&decoded);

    return status;
}


int
main(int ac, char *av[])
{
    int test_num = 0;
    if (ac != 2)
	fprintf(stderr,"Usage: %s test_number\n", av[0]);

    sscanf(av[1], "%d", &test_num);

    switch (test_num) {
	case 1:
	    return test_quote(NULL);
	case 2:
	    return test_quote("");
	case 3:
	    return test_quote(" ");
	case 4:
	    return test_quote("hello");
	case 5:
	    return test_quote("\"");
	case 6:
	    return test_quote("\'");
	case 7:
	    return test_quote("\\");
	case 8:
	    return test_quote("\\\"");
	case 9:
	    return test_quote("\\\\");
	case 10:
	    return test_quote("\"hello\"");
	case 11:
	    return test_quote("\'hello\'");
	case 12:
	    return test_quote("\\hello");
	case 13:
	    return test_quote("\\hello\"");
	case 14:
	    return test_quote("hello\\\\");
	case 15:
	    return test_quote("\"hello\'\"");
	case 16:
	    return test_quote("\"hello\'");
	case 17:
	    return test_quote("\'hello\'");
	case 18:
	    return test_quote("\'hello\"");
	case 19:
	    return test_quote("\"\"hello\"");
	case 20:
	    return test_quote("\'\'hello\'\'");
	case 21:
	    return test_quote("\'\"hello\"\'");
	case 22:
	    return test_quote("\"\"hello\"\"");
	case 23:
	    return test_quote("\"\"\"hello\"\"\"");
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
