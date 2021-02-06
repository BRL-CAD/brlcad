/*                     V L S _ I N C R . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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
    int ret = 1;
    int i = 0;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    long inc_count = 0;
    char *endptr;
    const char *rs = NULL;
    const char *rs_complex = "([-_:]*[0-9]+[-_:]*)[^0-9]*$";
    const char *formatting = NULL;

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
 * ex: shiftwidth=4 tabstop=8
 */
