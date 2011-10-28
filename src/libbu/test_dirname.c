/*                  T E S T _ D I R N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
#ifdef HAVE_LIBGEN_H /* for dirname */
#  include <libgen.h>
#endif

#include "bu.h"


/* Test against basename UNIX tool */
void
automatic_test(const char *input)
{

    char *ans = NULL;
    char buf_input[1000];
    char *res = NULL;

    if (input)
	bu_strlcpy(buf_input, input, strlen(input)+1);

#ifdef HAVE_DIRNAME
    /* build UNIX 'dirname' command */
    if (!input)
	ans = dirname(NULL);
    else
	ans = dirname(buf_input);
#endif

    if (!input)
	res = bu_dirname(NULL);
    else
	res = bu_dirname(buf_input);

    if (BU_STR_EQUAL(res, ans))
	printf("%24s -> %24s [PASSED]\n", input, res);
    else
	printf("%24s -> %24s (should be: %s) [FAIL]\n", input, res, ans);

    bu_free(res, NULL);
}


int
main(int ac, char *av[])
{
    char input[1000] = {0};

    /* pre-define tests */
    printf("Performing pre-defined tests:\n");
    automatic_test("/usr/dir/file");
    automatic_test("/usr/dir/");
    automatic_test("/usr\\/dir");
    automatic_test("/usr/.");
    automatic_test("/usr/");
    automatic_test("/usr");
    automatic_test("usr");
    automatic_test("/usr/some long/file");
    automatic_test("/usr/some file");
    automatic_test("C:/usr/some\\ drivepath");
    automatic_test("/a test file");
    automatic_test("another file");
    automatic_test("C:\\Temp");
    automatic_test("C:/Temp");
    automatic_test("/");
    automatic_test("/////");
    automatic_test(".");
    automatic_test("..");
    automatic_test("...");
    automatic_test("   ");
    automatic_test("");
    automatic_test(NULL);

    /* user tests */
    if (ac > 1) {
	printf("Enter a string:\n");
	bu_fgets(input, 1000, stdin);
	if (strlen(input) > 0)
	    input[strlen(input)-1] = '\0';
	automatic_test(input);
    }

    printf("%s: testing complete\n", av[0]);
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
