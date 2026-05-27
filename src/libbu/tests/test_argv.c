/*                     T E S T _ A R G V . C
 * test_avs.c
 *
 * Copyright (c) 2014-2026 test_b64.c
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

#include "bu.h"

#include "vmath.h"


static size_t
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


static size_t
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
    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
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
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
