/*                 T E S T _ V L S _ I N C R . C
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
    int ret = 1;
    int i = 0;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    long inc_count = 0;
    char *endptr;
    const char *rs = NULL;
    const char *rs_complex = "([-_:]*[0-9]+[-_:]*)[^0-9]*$";
    const char *formatting = NULL;

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    /* Sanity check */
    if (argc < 6)
	bu_exit(1, "Usage: %s {name} {num} {formatting} {incr_count} {expected}\n", argv[0]);

    if (BU_STR_EQUAL(argv[2], "1")) {
	rs = rs_complex;
    }

    if (!rs && !BU_STR_EQUAL(argv[2], "0") && !BU_STR_EQUAL(argv[2], "NULL")) {
	rs = argv[2];
    }

    if (!BU_STR_EQUAL(argv[3], "NULL")) {
	formatting = argv[3];
    }

    errno = 0;
    inc_count = strtol(argv[4], &endptr, 10);
    if (errno == ERANGE || inc_count <= 0) {
	bu_exit(1, "invalid increment count: %s\n", argv[4]);
    }

    bu_vls_sprintf(&name, "%s", argv[1]);
    while (i < inc_count) {
	(void)bu_vls_incr(&name, rs, formatting, NULL, NULL);
	i++;
    }

    if (BU_STR_EQUAL(bu_vls_addr(&name), argv[5])) ret = 0;

    bu_log("output: %s\n", bu_vls_addr(&name));

    bu_vls_free(&name);

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
