/*                     B A S E N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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

/* These functions wrap the system and bu implementations to
 * standardize the memory behavior. The input string is unmodified,
 * the output string is dynamically allocated and must be freed by the
 * caller.
 */

#ifdef HAVE_BASENAME
char *
get_system_output(const char *input)
{
    char *in = input ? bu_strdup(input) : NULL;
    char *out = bu_strdup(basename(in));

    bu_free(in, "input copy");

    return out;
}
#endif

#if !defined(HAVE_BASENAME) && defined(HAVE__SPLITPATH)
char *
get_system_output(const char *input)
{
    char fname[_MAX_FNAME];
    char dir[_MAX_DIR];
    char *base = NULL;
    if (input && !strlen(input) == 0) {
	char *in = bu_strdup(input);
	if (!strchr(in, '/') && !strchr(in, '\\')) return in;
	if (BU_STR_EQUAL(in, "/")) {
	    base = bu_strdup("/");
	    bu_free(in, "input copy");
	    return base;
	}
	_splitpath(in, NULL, dir, fname, NULL);
	if (strlen(dir) != strlen(in) && strlen(fname) == 0 && in[strlen(in) - 1] == '.') {
	    base = bu_strdup(&(in[strlen(in) - 1]));
	    bu_free(in, "input copy");
	    return base;
	}
	while (strlen(dir) > 1 && strlen(fname) == 0) {
	    in[strlen(in) - 1] = '\0';
	    _splitpath(in, NULL, dir, fname, NULL);
	}
	if (strlen(fname) > 0) {
	    base = bu_strdup(fname);
	} else {
	    if (in[strlen(in) - 1] == '/') {
		base = bu_strdup("/");
	    }
	}
	bu_free(in, "input copy");
    } else {
	base = bu_strdup(".");
    }
    return base;
}
#endif

char *
get_bu_output(const char *input)
{
    /* basename should return "." when given a NULL string */
    size_t null_result_chars = sizeof(".") / sizeof(char);
    size_t max_result_chars = input ? strlen(input) + 1 : null_result_chars;

    char *output = (char *)bu_calloc(max_result_chars, sizeof(char), "bu output");

    bu_path_basename(input, output);

    return output;
}

#if defined(HAVE_BASENAME) || defined(HAVE__SPLITPATH)
void
compare_bu_to_system_basename(const char *input)
{
   char *sys_out = get_system_output(input);
    char *bu_out = get_bu_output(input);

    if (BU_STR_EQUAL(sys_out, bu_out)) {
	bu_log("%24s -> %24s [PASSED]\n", input, bu_out);
	bu_free(bu_out, "bu output");
	bu_free(sys_out, "system output");
    } else {
	bu_log("%24s -> %24s (should be: %s) [FAIL]\n", input, bu_out, sys_out);
	bu_free(bu_out, "bu output");
	bu_free(sys_out, "system output");
	bu_exit(EXIT_FAILURE, "compare_bu_to_system_basename failed");
    }
#else
void
compare_bu_to_system_basename(const char *UNUSED(input))
{
    bu_exit(EXIT_FAILURE, "BASENAME not available on this platform\n");
#endif
}


int
main(int argc, char *argv[])
{
    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    /* If we don't have any args at all, test NULL */
    if (argc == 1) {
	compare_bu_to_system_basename(NULL);
    }

    /* If we have something, print it and test it */
    if (argc > 1) {
	bu_log("Testing string \"%s\"\n", argv[1]);
	compare_bu_to_system_basename(argv[1]);
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
