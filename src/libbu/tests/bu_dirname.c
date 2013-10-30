/*                  T E S T _ D I R N A M E . C
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_LIBGEN_H /* for dirname */
#  include <libgen.h>
#endif

#include "bu.h"


/* Test against basename UNIX tool */
int
automatic_test(const char *input)
{
#ifdef HAVE_DIRNAME
    char *ans = NULL;
    char buf_input[1000];
    char dirname_buf_input[1000];
    char *res = NULL;
    int pass = 0;

    if (input) {
	bu_strlcpy(buf_input, input, strlen(input)+1);
	bu_strlcpy(dirname_buf_input, input, strlen(input)+1);
    }

    /* build UNIX 'dirname' command */
    if (!input)
	ans = dirname(NULL);
    else
	ans = dirname(dirname_buf_input);

    if (!input)
	res = bu_dirname(NULL);
    else
	res = bu_dirname(buf_input);

    if (BU_STR_EQUAL(res, ans)) {
	printf("%24s -> %24s [PASSED]\n", input, res);
	pass = 1;
    } else {
	printf("%24s -> %24s (should be: %s) [FAIL]\n", input, res, ans);
    }

    bu_free(res, NULL);
    return pass;
#else
    printf("%s untested - dirname not implemented on this platform\n", input);
    return 1;
#endif
}


int
main(int argc, char *argv[])
{
    if (argc == 1)
       return !automatic_test(NULL);

    if (argc > 2)
       fprintf(stderr,"Usage: %s test_string\n", argv[0]);

    return !automatic_test(argv[1]);
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
