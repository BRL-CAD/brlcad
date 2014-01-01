/*                   T E S T _ E S C A P E . C
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

#include <stdio.h>

#include "bu.h"

static int
compare(const char *input, const char *output, const char *correct)
{
    if (BU_STR_EQUAL(output, correct)) {
	printf("%24s -> %28s [PASS]\n", input, output);
	return 1;
    } else {
	printf("%24s -> %28s [FAIL]  (should be '%s')\n", input, output, correct);
	return 0;
    }
}


int
main(int ac, char *av[])
{
    int function_num = 0;
    int test_num = 0;
    char *bufp;
    char buffer[32];

    if (ac < 3)
	fprintf(stderr,"Usage: %s function_to_test test_number\n", av[0]);

    sscanf(av[1], "%d", &function_num);
    sscanf(av[2], "%d", &test_num);

    if (function_num == 1) {
#define CMP_UNESC(in, out) compare((in), bu_str_unescape((in), buffer, 32), (out))
	switch (test_num) {
	    case 1:
		return !CMP_UNESC(NULL, "");
	    case 2:
		return !CMP_UNESC("", "");
	    case 3:
		return !CMP_UNESC(" ", " ");
	    case 4:
		return !CMP_UNESC("hello", "hello");
	    case 5:
		return !CMP_UNESC("\"", "\"");
	    case 6:
		return !CMP_UNESC("\'", "\'");
	    case 7:
		return !CMP_UNESC("\\", "");
	    case 8:
		return !CMP_UNESC("\\\"", "\"");
	    case 9:
		return !CMP_UNESC("\\\\", "\\");
	    case 10:
		return !CMP_UNESC("\"hello\"", "\"hello\"");
	    case 11:
		return !CMP_UNESC("\'hello\'", "'hello'");
	    case 12:
		return !CMP_UNESC("\\hello", "hello");
	    case 13:
		return !CMP_UNESC("\\hello\"", "hello\"");
	    case 14:
		return !CMP_UNESC("hello\\\\", "hello\\");
	    case 15:
		return !CMP_UNESC("\"hello\'\"", "\"hello'\"");
	    case 16:
		return !CMP_UNESC("\"hello\'", "\"hello'");
	    case 17:
		return !CMP_UNESC("\'hello\'", "'hello'");
	    case 18:
		return !CMP_UNESC("\'hello\"", "'hello\"");
	    case 19:
		return !CMP_UNESC("\"\"hello\"", "\"\"hello\"");
	    case 20:
		return !CMP_UNESC("\'\'hello\'\'", "''hello''");
	    case 21:
		return !CMP_UNESC("\'\"hello\"\'", "'\"hello\"'");
	    case 22:
		return !CMP_UNESC("\"\"hello\"\"", "\"\"hello\"\"");
	    case 23:
		return !CMP_UNESC("\\\"\\\"\\\"hello\\", "\"\"\"hello");
	}
	return 1;
    }

    if (function_num == 2) {
#define CMP_ESC(in, c, out) compare((in), bu_str_escape((in), (c), buffer, 32), (out))
	switch (test_num) {
	    case 1:
		return !CMP_ESC(NULL, NULL, "");
	    case 2:
		return !CMP_ESC(NULL, "", "");
	    case 3:
		return !CMP_ESC("", NULL, "");
	    case 4:
		return !CMP_ESC("", "", "");
	    case 5:
		return !CMP_ESC(" ", "", " ");
	    case 6:
		return !CMP_ESC("[ ]", " ", "[\\ ]");
	    case 7:
		return !CMP_ESC("[  ]", " ", "[\\ \\ ]");
	    case 8:
		return !CMP_ESC("h e l l o", " ", "h\\ e\\ l\\ l\\ o");
	    case 9:
		return !CMP_ESC("h\\ ello", " ", "h\\\\ ello");
	    case 10:
		return !CMP_ESC("[]", "\\", "[]");
	    case 11:
		return !CMP_ESC("\\", "\\", "\\\\");
	    case 12:
		return !CMP_ESC("\\\\", "\\", "\\\\\\\\");
	    case 13:
		return !CMP_ESC("\\a\\b", "\\", "\\\\a\\\\b");
	    case 14:
		return !CMP_ESC("abc", "a", "\\abc");
	    case 15:
		return !CMP_ESC("abc", "b", "a\\bc");
	    case 16:
		return !CMP_ESC("abc", "c", "ab\\c");
	    case 17:
		return !CMP_ESC("abc", "ab", "\\a\\bc");
	    case 18:
		return !CMP_ESC("abc", "bc", "a\\b\\c");
	    case 19:
		return !CMP_ESC("abc", "abc", "\\a\\b\\c");
	    case 20:
		return !CMP_ESC("aaa", "bc", "aaa");
	    case 21:
		return !CMP_ESC("aaa", "a", "\\a\\a\\a");
	    case 22:
		return !CMP_ESC("aaa", "aaa", "\\a\\a\\a");
	    case 23:
		return !CMP_ESC("abc", "^a", "a\\b\\c");
	    case 24:
		return !CMP_ESC("abc", "^b", "\\ab\\c");
	    case 25:
		return !CMP_ESC("abc", "^c", "\\a\\bc");
	    case 26:
		return !CMP_ESC("abc", "^ab", "ab\\c");
	    case 27:
		return !CMP_ESC("abc", "^bc", "\\abc");
	    case 28:
		return !CMP_ESC("abc", "^abc", "abc");
	    case 29:
		return !CMP_ESC("aaa", "^bc", "\\a\\a\\a");
	    case 30:
		return !CMP_ESC("aaa", "^a", "aaa");
	    case 31:
		return !CMP_ESC("aaa", "^aaa", "aaa");
	}
	return 1;
    }

    if (function_num == 3) {
	int pass = 0;
	switch (test_num) {
	    case 1:
		bufp = bu_str_unescape(bu_str_escape("abc", "b", buffer, 32), NULL, 0);
		pass = compare("abc", bufp, "abc");
		bu_free(bufp, NULL);

	    case 2:
		bufp = bu_str_unescape(bu_str_escape("abc\\cba", "b", buffer, 32), NULL, 0);
		pass = compare("abc\\cba", bufp, "abccba");
		bu_free(bufp, NULL);

	    case 3:
		bufp = bu_str_unescape(bu_str_escape("abc\\\\cba", "b", buffer, 32), NULL, 0);
		pass = compare("abc\\\\cba", bufp, "abc\\cba");
		bu_free(bufp, NULL);

	    case 4:
		bufp = bu_str_unescape(bu_str_escape("abc\\\\\\c\\ba\\", "b", buffer, 32), NULL, 0);
		pass = compare("abc\\\\\\c\\ba\\", bufp, "abc\\c\\ba");
		bu_free(bufp, NULL);
	}
	return !pass;
    }
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
