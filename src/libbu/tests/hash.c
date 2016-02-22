/*                       H A S H . C
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
#include <string.h>
#include "bu.h"

#ifdef TEST_TCL_HASH
#include <tcl.h>
#endif

int
main(int argc, const char **argv)
{
    int ret = 0;
    long test_num;
    char *endptr = NULL;

    /* Sanity checks */
    if (argc < 2) {
	bu_exit(1, "ERROR: wrong number of parameters - need test num");
    }
    test_num = strtol(argv[1], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid test number: %s\n", argv[1]);
    }

    switch (test_num) {
	case 0:
	    /*ret = hash_noop_test();*/
	case 1:
	    /*ret = ;*/
	    break;
	case 2:
	    /*ret = ;*/
	    break;
    }

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
