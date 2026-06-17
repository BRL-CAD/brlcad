/*                    T E S T _ A R B 8 . C
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
/** @file libbsg/tests/test_arb8.c
 *
 * Phase 4 test: bsg_vlist_arb8 emits exactly 18 vlist commands.
 *
 * The expected draw strategy is:
 *   bottom loop  MOVE(0) DRAW(1) DRAW(2) DRAW(3) DRAW(0)   — 5 cmds
 *   top loop     MOVE(4) DRAW(5) DRAW(6) DRAW(7) DRAW(4)   — 5 cmds
 *   verticals    MOVE(0)/DRAW(4)  MOVE(1)/DRAW(5)
 *                MOVE(2)/DRAW(6)  MOVE(3)/DRAW(7)           — 8 cmds
 *   Total: 18 commands, each unique edge drawn exactly once.
 *
 * Usage: test_bsg_arb8
 *   Returns 0 on success, 1 on failure.
 */

#include "common.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/list.h"
#include "bsg/vlist.h"

int
main(int UNUSED(argc), char *argv[])
{
    bu_setprogname(argv[0]);
    /* Unit cube corners */
    point_t pts[8] = {
	{0.0, 0.0, 0.0}, /* 0 */
	{1.0, 0.0, 0.0}, /* 1 */
	{1.0, 1.0, 0.0}, /* 2 */
	{0.0, 1.0, 0.0}, /* 3 */
	{0.0, 0.0, 1.0}, /* 4 */
	{1.0, 0.0, 1.0}, /* 5 */
	{1.0, 1.0, 1.0}, /* 6 */
	{0.0, 1.0, 1.0}  /* 7 */
    };

    struct bu_list head;
    struct bu_list vlfree;
    BU_LIST_INIT(&head);
    BU_LIST_INIT(&vlfree);

    bsg_vlist_arb8(&head, &vlfree, pts);

    size_t cnt = bsg_vlist_cmd_cnt((bsg_vlist *)&head);
    if (cnt != 18) {
	bu_log("FAIL: bsg_vlist_arb8 produced %zu commands, expected 18\n", cnt);
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
