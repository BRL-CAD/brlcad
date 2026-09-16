/*                  T E S T _ D I R N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
	res = bu_path_dirname(NULL);
    else
	res = bu_path_dirname(buf_input);

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
    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc > 2)
	fprintf(stderr, "Usage: %s {test_string}\n", argv[0]);

    if (argc == 1)
	return !automatic_test(NULL);

    return !automatic_test(argv[1]);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
