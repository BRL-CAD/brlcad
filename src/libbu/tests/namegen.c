/*                       N A M E G E N . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
#include <limits.h>
#include <ctype.h>
#include "bu.h"
#include "bn.h"
#include "string.h"


int
main(int argc, const char **argv)
{
    int i = 0;
    int ret = -1;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    const char *i1 = "0:0:0:0:-";
    const char *regex_str = "([-_:]*[0-9]+[-_:]*)[^0-9]*$";
    const char *regex_str_2 = "([0-9]+)[^0-9]*$";

    /* Sanity check */
    if (argc < 2) bu_exit(1, "ERROR: wrong number of parameters");

    if (strlen(argv[1]) <= 0) {
	bu_exit(1, "invalid string: %s\n", argv[1]);
    }

    bu_vls_sprintf(&name, "%s", argv[1]);
    ret = bu_namegen(&name, regex_str, i1);
    bu_log("output: %s\n", bu_vls_addr(&name));

    bu_vls_sprintf(&name, "%s", argv[1]);
    while (i < 100) {
	ret = bu_namegen(&name, regex_str_2, NULL);
	bu_log("output: %s\n", bu_vls_addr(&name));
	i++;
    }

/*
    switch (argc) {
	case 2:
	    bu_log("output: %s\n", bu_vls_addr(&out));
	    break;
	default:
	    break;
    }
*/

    return ret;
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
