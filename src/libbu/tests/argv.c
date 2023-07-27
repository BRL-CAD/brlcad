/*                       A R G V . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2023 United States Government as represented by
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

#include "vmath.h"

size_t
argv_test(const char *s, size_t expected)
{
    char *ts = bu_strdup(s);
    char **av = (char **)bu_calloc(strlen(ts)+1, sizeof(char *), "av");
    size_t ac = bu_argv_from_string(av, strlen(ts), ts);
    if (ac != expected)
	bu_exit(EXIT_FAILURE, "Test failed: %s\n", s);
    bu_log("Test results (%zd elements): %s\n", ac,  s);
    for (size_t i = 0; i < ac; i++) {
	bu_log("%zd\t: %s\n", i, av[i]);
    }
    bu_free(av, "array");
    bu_free(ts, "string copy");
    return ac;
}

size_t
argv_test_limited(const char *s, size_t expected, int lmax)
{
    char *ts = bu_strdup(s);
    char **av = (char **)bu_calloc(strlen(ts)+1, sizeof(char *), "av");
    size_t ac = bu_argv_from_string(av, lmax, ts);
    if (ac != expected)
	bu_exit(EXIT_FAILURE, "Test failed: %s\n", s);
    bu_log("Test results (%zd elements): %s\n", ac,  s);
    for (size_t i = 0; i < ac; i++) {
	bu_log("%zd\t: %s\n", i, av[i]);
    }
    bu_free(av, "array");
    bu_free(ts, "string copy");
    return ac;
}

int
main(int UNUSED(argc), char *argv[])
{
    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    argv_test("a0", 1);
    argv_test("a0 a1", 2);
    argv_test("a0 a1 a2", 3);
    argv_test("a0 \"a 1\"", 2);
    argv_test("\"a 0\" a1", 2);
    argv_test("\"a 0\" \"a 1\"", 2);
    argv_test("a0 \"a 1\" a2", 3);
    argv_test("a0 a\\ 1", 2);
    argv_test("a\\ 0 a1", 2);
    argv_test("a\\ 0 a\\ 1", 2);
    argv_test("a\\ 0 \"a 1\"", 2);
    argv_test("\"a 0\" a\\ 1", 2);
    argv_test("\\\"a 0\\\" a\\ 1", 3);
    argv_test("a\\ 0 \\\"a 1\\\"", 3);
    argv_test("a\\\\ 0 a\\ 1", 3);
    argv_test("a\\ 0 a\\\\ 1", 3);
    argv_test("\"a\\ 0\" a\\ 1", 2);
    argv_test("\"a\\\\ 0\" a\\ 1", 2);


    argv_test_limited("a0 a1 a2", 0, 0);
    argv_test_limited("a0 a1 a2", 1, 1);
    argv_test_limited("a0 a1 a2", 2, 2);
    argv_test_limited("a0 a1 a2", 3, 3);
    argv_test_limited("a0 a1 a2", 3, 4);

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
