/*                 T E S T _ B A S E N A M E . C
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_LIBGEN_H /* for basename */
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

#ifdef HAVE_BASENAME
    if (input)
	bu_strlcpy(buf_input, input, strlen(input)+1);

    /* build UNIX 'basename' command */
    if (!input)
	ans = basename(NULL);
    else
	ans = basename(buf_input);

    if (!input)
	res = bu_basename(NULL);
    else
	res = bu_basename(buf_input);

    if (BU_STR_EQUAL(res, ans))
	printf("%24s -> %24s [PASSED]\n", input, res);
    else
	bu_exit(EXIT_FAILURE, "%24s -> %24s (should be: %s) [FAIL]\n", input, res, ans);

    bu_free(res, NULL);
#else
    printf("BASENAME not available on this platform\n");
#endif

    /* FIXME: this does not functionally halt */
}


int
main(int argc, char *argv[])
{
    /* If we don't have any args at all, test NULL */
    if (argc == 1) {
	automatic_test(NULL);
    }

    /* If we have something, print it and test it */
    if (argc > 1) {
       printf("Testing string \"%s\"\n", argv[1]);
       automatic_test(argv[1]);
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
