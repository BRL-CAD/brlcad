/*                 T E S T _ P R O G N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2012 United States Government as represented by
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


int
main(int ac, char *av[])
{
    int pass = 0;
    const char *label;
    const char *ans;
    const char *res;

    if (ac > 1) {
	printf("Usage: %s\n", av[0]);
	return 1;
    }

    /* pre-define tests */
    printf("Performing pre-defined tests:\n");

    /* CASE 0: getting unset name */
    label = "CASE 0";
    res = bu_getprogname();
    ans = "(BRL-CAD)";

    if (BU_STR_EQUAL(res, ans ? ans : "") || BU_STR_EQUAL(res, bu_basename(av[0]))) {
	printf("%s: %24s -> %24s [PASSED]\n", label, "unset", res);
    } else {
	printf("%24s -> %24s (should be: %s) [FAIL]\n", label, res, ans);
	pass++;
    }

    /* CASE 1: try again unset */
    label = "CASE 1";
    res = bu_getprogname();
    ans = "(BRL-CAD)";

    if (BU_STR_EQUAL(res, ans ? ans : "") || BU_STR_EQUAL(res, bu_basename(av[0]))) {
	printf("%s: %24s -> %24s [PASSED]\n", label, "unset#2", res);
    } else {
	printf("%24s -> %24s (should be: %s) [FAIL]\n", label, res, ans);
	pass++;
    }

    /* CASE 2: set NULL, then get */
    label = "CASE 2";
    bu_setprogname(NULL);
    res = bu_getprogname();
    ans = "(BRL-CAD)";

    if (BU_STR_EQUAL(res, ans ? ans : "") || BU_STR_EQUAL(res, bu_basename(av[0]))) {
	printf("%s: %24s -> %24s [PASSED]\n", label, "NULL", res);
    } else {
	printf("%24s -> %24s (should be: %s) [FAIL]\n", label, res, ans);
	pass++;
    }

    /* CASE 3: set, then get */
    label = "CASE 3";
    bu_setprogname(av[0]);
    res = bu_getprogname();
    ans = av[0];

    if (BU_STR_EQUAL(res, ans ? ans : "") || BU_STR_EQUAL(res, bu_basename(av[0]))) {
	printf("%s: %24s -> %24s [PASSED]\n", label, "av[0]", res);
    } else {
	printf("%24s -> %24s (should be: %s) [FAIL]\n", label, res, ans);
	pass++;
    }

    /* CASE 4: set full, then get */
    label = "CASE 4";
    bu_setprogname(bu_argv0_full_path());
    res = bu_getprogname();
    ans = bu_argv0_full_path();

    if (BU_STR_EQUAL(res, ans ? ans : "") || BU_STR_EQUAL(res, bu_basename(ans))) {
	printf("%s: %24s -> %24s [PASSED]\n", label, ans, res);
    } else {
	printf("%24s -> %24s (should be: %s) [FAIL]\n", label, res, ans);
	pass++;
    }

/* These two tests only make sense if bu_setprogname is supposed to override
 * a program name from argv0 - until that is decided (and if so decided, the
 * behavior of bu_setprogname and bu_getprogname needs to be made to match that)
 * turn off these two tests.*/
#if 0
    /* CASE 5: set 2x, then get */
    label = "CASE 5";
    bu_setprogname(av[0]);
    bu_setprogname("monkey_see_monkey_do");
    res = bu_getprogname();
    ans = "monkey_see_monkey_do";

    if (BU_STR_EQUAL(res, ans ? ans : "")) {
	printf("%s: %24s -> %24s [PASSED]\n", label, ans, res);
    } else {
	printf("%24s -> %24s (should be: %s) [FAIL]\n", label, res, ans);
	pass++;
    }

    /* CASE 6: set 2x full path, then get */
    label = "CASE 6";
    bu_setprogname(bu_argv0_full_path());
    bu_setprogname("/monkey/see/monkey/do");
    res = bu_getprogname();
    ans = "do";

    if (BU_STR_EQUAL(res, ans ? ans : "")) {
	printf("%s: %24s -> %24s [PASSED]\n", label, "/monkey/see/monkey/do", res);
    } else {
	printf("%24s -> %24s (should be: %s) [FAIL]\n", label, res, ans);
	pass++;
    }
#endif
    return pass;
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
