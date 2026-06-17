/*                   T E S T _ V L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libbsg/tests/test_vlist.c
 *
 * Phase 4 test: bsg_vlist_cmd_cnt returns correct counts.
 *
 * Usage: test_bsg_vlist <expected_count>
 *   If expected_count < 0, tests the NULL-pointer return value (0).
 *   Otherwise builds a vlist of that many commands and asserts
 *   bsg_vlist_cmd_cnt matches.
 */

#include "common.h"

#include <stdlib.h>
#include "vmath.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/list.h"
#include "bsg/vlist.h"

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc < 2)
	bu_exit(1, "Usage: %s <expected_count>\n", argv[0]);

    int expected = atoi(argv[1]);

    if (expected < 0) {
	/* NULL input must return 0 */
	size_t cnt = bsg_vlist_cmd_cnt(NULL);
	if (cnt != 0) {
	    bu_log("FAIL: bsg_vlist_cmd_cnt(NULL) returned %zu, expected 0\n", cnt);
	    return 1;
	}
	return 0;
    }

    point_t ptzero = VINIT_ZERO;
    struct bu_list head;
    struct bu_list vlfree;
    BU_LIST_INIT(&head);
    BU_LIST_INIT(&vlfree);

    for (int i = 0; i < expected; i++)
	BSG_ADD_VLIST(&vlfree, &head, ptzero, BSG_VLIST_LINE_DRAW);

    size_t cnt = bsg_vlist_cmd_cnt((bsg_vlist *)&head);
    if (cnt != (size_t)expected) {
	bu_log("FAIL: bsg_vlist_cmd_cnt returned %zu, expected %d\n", cnt, expected);
	return 1;
    }
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
