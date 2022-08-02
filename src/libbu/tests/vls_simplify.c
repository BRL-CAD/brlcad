/*                   V L S _ S I M P L I F Y . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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

    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
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
 * ex: shiftwidth=4 tabstop=8
 */
