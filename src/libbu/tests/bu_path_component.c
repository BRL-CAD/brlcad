/*           T E S T _ B U _ P A T H _ C O M P O N E N T . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2016 United States Government as represented by
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"

/* If expected_str is NULL, check that return code is zero */
void
compare(const char *input, const char *expected_str, bu_path_component_t type)
{
    struct bu_vls component = BU_VLS_INIT_ZERO;
    int found = bu_path_component(&component, input, type);

    if (!expected_str && found) {
	bu_log("no result expected, but result found: %s\n", bu_vls_addr(&component));
	bu_vls_free(&component);
	bu_exit(EXIT_FAILURE, "");
    }
    if (expected_str && !found) {
	bu_log("%24s -> %24s (should be: %s) [FAIL]\n", input, bu_vls_addr(&component), expected_str);
	bu_vls_free(&component);
	bu_exit(EXIT_FAILURE, "");
	return;
    }
    if (!expected_str && !found) {
	bu_log("%24s -> %24s [PASSED]\n", input, bu_vls_addr(&component));
	bu_vls_free(&component);
	return;
    }

    if (BU_STR_EQUAL(expected_str, bu_vls_addr(&component))) {
	printf("%24s -> %24s [PASSED]\n", input, bu_vls_addr(&component));
	bu_vls_free(&component);
	return;
    } else {
	bu_log("%24s -> %24s (should be: %s) [FAIL]\n", input, bu_vls_addr(&component), expected_str);
	bu_vls_free(&component);
	bu_exit(EXIT_FAILURE, "");
    }
}


int
main(int argc, char *argv[])
{
    const char *control = NULL;
    int intarg = 0;

    /* If we don't have any args at all, test NULL */
    if (argc == 1) {
	compare(NULL, NULL, BU_PATH_DIRNAME);
	return 0;
    }

    if (argc == 2) {
	printf("Testing empty path handling\n");
	compare("", NULL, BU_PATH_DIRNAME);
	return 0;
    }

    /* If we have an input and a number, do the empty test */
    if (argc == 3) {
	intarg = atoi(argv[2]);
    }

    /* If we have a standard to test against, use it */
    if (argc == 4) {
	intarg = atoi(argv[3]);
	control = argv[2];
    }

    printf("Testing path \"%s\", component %d\n", argv[1], intarg);
    switch (intarg) {
	case 0:
	    compare(argv[1], control, BU_PATH_DIRNAME);
	    break;
	case 1:
	    compare(argv[1], control, BU_PATH_EXTLESS);
	    break;
	case 2:
	    compare(argv[1], control, BU_PATH_BASENAME);
	    break;
	case 3:
	    compare(argv[1], control, BU_PATH_BASENAME_EXTLESS);
	    break;
	case 4:
	    compare(argv[1], control, BU_PATH_EXT);
	    break;
	default:
	    bu_log("Error - unknown component\n");
	    break;
    }

    return 0;
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
