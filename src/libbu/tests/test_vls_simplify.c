/*             T E S T _ V L S _ S I M P L I F Y . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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
#include <limits.h>
#include <stdlib.h> /* for strtol */
#include <ctype.h>
#include <errno.h> /* for errno */
#include "bu.h"
#include "bn.h"
#include "string.h"


int
main(int argc, char *argv[])
{
    int ret = 0;
    struct bu_vls vstr = BU_VLS_INIT_ZERO;
    const char *expected = NULL;
    const char *keep_chars = NULL;
    const char *dedup_chars = NULL;
    const char *trim_chars = NULL;

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    /* Sanity check */
    if (argc < 3)
	bu_exit(1, "Usage: %s {start_str} {expected_str} {keep_chars} {dedup_chars} {trim_chars}\n", argv[0]);

    bu_vls_sprintf(&vstr, "%s", argv[1]);
    expected = argv[2];

    if (argc > 3 && strlen(argv[3]) > 0)
	keep_chars = argv[3];
    if (argc > 4 && strlen(argv[4]) > 0)
	dedup_chars = argv[4];
    if (argc > 5 && strlen(argv[5]) > 0)
	trim_chars = argv[5];

    (void)bu_vls_simplify(&vstr, keep_chars, dedup_chars, trim_chars);

    if (!BU_STR_EQUAL(bu_vls_addr(&vstr), expected)) {
	bu_log("got: %s, expected: %s\n", bu_vls_addr(&vstr), expected);
	ret = 1;
    }

    bu_vls_free(&vstr);

    return ret;
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
