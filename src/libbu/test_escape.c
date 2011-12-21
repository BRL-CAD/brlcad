/*                   T E S T _ E S C A P E . C
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

#include <stdio.h>

#include "bu.h"


static void
compare(const char *input, const char *output, const char *correct)
{
    if (BU_STR_EQUAL(output, correct)) {
	printf("%24s -> %28s [PASS]\n", input, output);
    } else {
	printf("%24s -> %28s [FAIL]  (should be '%s')\n", input, output, correct);
    }
}


int
main(int ac, char *av[])
{
    char buffer[32];

    printf("Testing unescape\n");

    if (ac > 1)
	printf("Usage: %s\n", av[0]);

#define CMPU(in, out) compare((in), bu_str_unescape((in), buffer, 32), (out))
    CMPU(NULL, "");
    CMPU("", "");
    CMPU(" ", " ");
    CMPU("hello", "hello");
    CMPU("\"", "\"");
    CMPU("\'", "\'");
    CMPU("\\", "");
    CMPU("\\\"", "\"");
    CMPU("\\\\", "\\");
    CMPU("\"hello\"", "\"hello\"");
    CMPU("\'hello\'", "'hello'");
    CMPU("\\hello", "hello");
    CMPU("\\hello\"", "hello\"");
    CMPU("hello\\\\", "hello\\");
    CMPU("\"hello\'\"", "\"hello'\"");
    CMPU("\"hello\'", "\"hello'");
    CMPU("\'hello\'", "'hello'");
    CMPU("\'hello\"", "'hello\"");
    CMPU("\"\"hello\"", "\"\"hello\"");
    CMPU("\'\'hello\'\'", "''hello''");
    CMPU("\'\"hello\"\'", "'\"hello\"'");
    CMPU("\"\"hello\"\"", "\"\"hello\"\"");
    CMPU("\\\"\\\"\\\"hello\\", "\"\"\"hello");

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
