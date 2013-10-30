/*                 T E S T _ B O O L E A N I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2013 United States Government as represented by
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
#include "bu.h"

#include <string.h>

int
automatic_test(const char *input)
{

    char buf_input[1000], buf_aux[1000];
    int buf_len, check, res, ans;

    if (input) {
	bu_strlcpy(buf_input, input, strlen(input)+1);

	buf_len = strlen(buf_input);

	/* Remove ending white space  */
	while ((buf_len > 0) && (buf_input[buf_len-1] == ' ')) {
	    buf_input[buf_len-1] = '\0';
	    buf_len = strlen(buf_input);
	}

	/* Remove leading white space  */
	while ((buf_len > 0) && (buf_input[0] == ' ')) {
	    bu_strlcpy(buf_aux, buf_input + 1, buf_len);
	    bu_strlcpy(buf_input, buf_aux, buf_len);
	    buf_len = strlen(buf_input);
	}
	res = bu_str_true(buf_input);

	/* empty/'n'/'N' as first character for buf_input string */
	if ((buf_len == 0) || (buf_input[0] == 'n') || (buf_input[0] == 'N') ||
	    (bu_strcmp(buf_input, "(null)") == 0)) {
	    ans = 0;
	    check = (res == ans);
	} else {
	    /* true value comes from here on */
	    /* 'y'/'Y' as first character/"1" or variants of 1 for buf_input string  */
	    if ((buf_input[0] == 'y') || (buf_input[0] == 'Y') || (atol(buf_input) == 1)) {
		ans = 1;
		check = (res == ans);
	    } else {
		/* "0" or variants of 0 */
		if ((buf_input[0] == '0') && (atol(buf_input) == 0)) {
		    ans = 0;
		    check = (res == ans);
		} else {
		    ans = (int)buf_input[0];
		    check = (res == ans);
		}
	    }
	}
    }

    if (!input) {
	ans = 0;
	res = bu_str_true(NULL);
	check = (res == ans);
    }

    if (check) {
	printf("%24s -> %d [PASSED]\n", buf_input, res);
    } else {
	printf("%24s -> %d (should be: %d) [FAIL]\n", buf_input, res, ans);
    }

    return check;
}


int main(int argc, char *argv[])
{
    int pass = 0;

    if (argc > 2)
       fprintf(stderr,"Usage: %s test_string\n", argv[0]);

    if (argc == 1) {
       pass = automatic_test(NULL);
       return !pass;
    }

    if (argc > 1)
       pass = automatic_test(argv[1]);

    return !pass;
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
