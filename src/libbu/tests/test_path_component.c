/*           T E S T _ P A T H _ C O M P O N E N T . C
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

#include "bu.h"


/* If expected_str is NULL, check that return code is zero */
static void
pc_compare(const char *input, const char *expected_str, bu_path_component_t type)
{
    struct bu_vls component = BU_VLS_INIT_ZERO;
    int found = bu_path_component(&component, input, type);

    if (!expected_str && found) {
	bu_log("no result expected, but result found: %s\n", bu_vls_addr(&component));
	bu_vls_free(&component);
	bu_exit(EXIT_FAILURE, "pc_compare: unexpected result");
    }
    if (expected_str && !found) {
	bu_log("%24s -> %24s (should be: %s) [FAIL]\n", input, bu_vls_addr(&component), expected_str);
	bu_vls_free(&component);
	bu_exit(EXIT_FAILURE, "pc_compare: FAIL1");
	return;
    }
    if (!expected_str && !found) {
	bu_log("%24s -> %24s [PASSED]\n", input, bu_vls_addr(&component));
	bu_vls_free(&component);
	return;
    }

    if (BU_STR_EQUAL(expected_str, bu_vls_addr(&component))) {
	printf("%24s -> %24s [PASSED]\n", input, bu_vls_addr(&component));
	bu_vls_free(&component);
	return;
    } else {
	bu_log("%24s -> %24s (should be: %s) [FAIL]\n", input, bu_vls_addr(&component), expected_str);
	bu_vls_free(&component);
	bu_exit(EXIT_FAILURE, "pc_compare: FAIL2");
    }
}


int
main(int argc, char *argv[])
{
    const char *control = NULL;
    int intarg = 0;

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    /* If we don't have any args at all, test NULL */
    if (argc == 1) {
	pc_compare(NULL, NULL, BU_PATH_DIRNAME);
	return 0;
    }

    if (argc == 2) {
	printf("Testing empty path handling\n");
	pc_compare("", NULL, BU_PATH_DIRNAME);
	return 0;
    }

    /* If we have an input and a number, do the empty test */
    if (argc == 3) {
	intarg = atoi(argv[2]);
    }

    /* If we have a standard to test against, use it */
    if (argc == 4) {
	intarg = atoi(argv[3]);
	control = argv[2];
    }

    printf("Testing path \"%s\", component %d\n", argv[1], intarg);
    switch (intarg) {
	case 0:
	    pc_compare(argv[1], control, BU_PATH_DIRNAME);
	    break;
	case 1:
	    pc_compare(argv[1], control, BU_PATH_EXTLESS);
	    break;
	case 2:
	    pc_compare(argv[1], control, BU_PATH_BASENAME);
	    break;
	case 3:
	    pc_compare(argv[1], control, BU_PATH_BASENAME_EXTLESS);
	    break;
	case 4:
	    pc_compare(argv[1], control, BU_PATH_EXT);
	    break;
	default:
	    bu_exit(1, "ERROR: unknown component\n");
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
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
