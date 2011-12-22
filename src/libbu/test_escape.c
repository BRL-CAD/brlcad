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
    char *bufp;
    char buffer[32];

    if (ac > 1)
	printf("Usage: %s\n", av[0]);

    printf("Testing unescape:\n");

#define CMP_UNESC(in, out) compare((in), bu_str_unescape((in), buffer, 32), (out))
    CMP_UNESC(NULL, "");
    CMP_UNESC("", "");
    CMP_UNESC(" ", " ");
    CMP_UNESC("hello", "hello");
    CMP_UNESC("\"", "\"");
    CMP_UNESC("\'", "\'");
    CMP_UNESC("\\", "");
    CMP_UNESC("\\\"", "\"");
    CMP_UNESC("\\\\", "\\");
    CMP_UNESC("\"hello\"", "\"hello\"");
    CMP_UNESC("\'hello\'", "'hello'");
    CMP_UNESC("\\hello", "hello");
    CMP_UNESC("\\hello\"", "hello\"");
    CMP_UNESC("hello\\\\", "hello\\");
    CMP_UNESC("\"hello\'\"", "\"hello'\"");
    CMP_UNESC("\"hello\'", "\"hello'");
    CMP_UNESC("\'hello\'", "'hello'");
    CMP_UNESC("\'hello\"", "'hello\"");
    CMP_UNESC("\"\"hello\"", "\"\"hello\"");
    CMP_UNESC("\'\'hello\'\'", "''hello''");
    CMP_UNESC("\'\"hello\"\'", "'\"hello\"'");
    CMP_UNESC("\"\"hello\"\"", "\"\"hello\"\"");
    CMP_UNESC("\\\"\\\"\\\"hello\\", "\"\"\"hello");

    printf("Testing escape:\n");

#define CMP_ESC(in, c, out) compare((in), bu_str_escape((in), (c), buffer, 32), (out))
    CMP_ESC(NULL, NULL, "");
    CMP_ESC(NULL, "", "");
    CMP_ESC("", NULL, "");
    CMP_ESC("", "", "");
    CMP_ESC(" ", "", " ");
    CMP_ESC("[ ]", " ", "[\\ ]");
    CMP_ESC("[  ]", " ", "[\\ \\ ]");
    CMP_ESC("h e l l o", " ", "h\\ e\\ l\\ l\\ o");
    CMP_ESC("h\\ ello", " ", "h\\\\ ello");
    CMP_ESC("[]", "\\", "[]");
    CMP_ESC("\\", "\\", "\\\\");
    CMP_ESC("\\\\", "\\", "\\\\\\\\");
    CMP_ESC("\\a\\b", "\\", "\\\\a\\\\b");
    CMP_ESC("abc", "a", "\\abc");
    CMP_ESC("abc", "b", "a\\bc");
    CMP_ESC("abc", "c", "ab\\c");
    CMP_ESC("abc", "ab", "\\a\\bc");
    CMP_ESC("abc", "bc", "a\\b\\c");
    CMP_ESC("abc", "abc", "\\a\\b\\c");
    CMP_ESC("aaa", "bc", "aaa");
    CMP_ESC("aaa", "a", "\\a\\a\\a");
    CMP_ESC("aaa", "aaa", "\\a\\a\\a");

    printf("Testing escape+unescape:\n");

    bufp = bu_str_unescape(bu_str_escape("abc", "b", buffer, 32), NULL, 0);
    compare("abc", bufp, "abc");
    bu_free(bufp, NULL);

    bufp = bu_str_unescape(bu_str_escape("abc\\cba", "b", buffer, 32), NULL, 0);
    compare("abc\\cba", bufp, "abccba");
    bu_free(bufp, NULL);

    bufp = bu_str_unescape(bu_str_escape("abc\\\\cba", "b", buffer, 32), NULL, 0);
    compare("abc\\\\cba", bufp, "abc\\cba");
    bu_free(bufp, NULL);

    bufp = bu_str_unescape(bu_str_escape("abc\\\\\\c\\ba\\", "b", buffer, 32), NULL, 0);
    compare("abc\\\\\\c\\ba\\", bufp, "abc\\c\\ba");
    bu_free(bufp, NULL);

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
