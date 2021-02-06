/*                              V L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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

 /* the program will test if the function bn_vlist_cmd_cnt() in vlist.c works fine
  * the function bn_vlist_cmd_cnt() calculates the number of commands in a vlist so
  * we will try all possible cases like empty list and list with diffrent lengthes and
  * list which needs more than one chunk of memory so here we will send the lengthe of the
  * the list and construct it inside bn_vlist.c and compare
  * the results with expected result from bn_vlist_cmd_cnt(), the <args> format is as follows : expected_result
  */

#include "common.h"

#include <string.h>
#include "bu.h"
#include "bn.h"

int
vlist_main(int argc, char* argv[])
{
    point_t ptzero = VINIT_ZERO;
    struct bu_list head;
    struct bu_list vlfree;
    size_t cmd_cnt_length = 0;
    int expected_length = 0;

    sscanf(argv[1], "%d", &expected_length);

    if (argc < 2) {
	bu_exit(1, "ERROR: input format is test_args [%s]\n", argv[0]);
    }

    BU_LIST_INIT(&head);
    BU_LIST_INIT(&vlfree);
    if (expected_length < 0) {
	return (int)bn_vlist_cmd_cnt(NULL);
    }

    for (int i = 0; i < expected_length; i++) {
	BN_ADD_VLIST(&vlfree, &head, ptzero, BN_VLIST_LINE_DRAW);
    }

    cmd_cnt_length = bn_vlist_cmd_cnt((struct bn_vlist*) & head);

    return !((size_t)expected_length == cmd_cnt_length);
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
