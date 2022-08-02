/*                       L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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

#include "bu.h"

struct test_list {
    struct bu_list l;
    long i;
};

static
int build_list_append(struct test_list *list, long lcnt)
{
    if (!list || lcnt < 0)
	return -1;

    for (long i = 0; i < lcnt; i++) {
	struct test_list *le;
	BU_ALLOC(le, struct test_list);
	le->i = i;
	BU_LIST_APPEND(&list->l, &le->l);
    }
    return lcnt;
}

int
main(int argc, char *argv[])
{
    long lcnt;
    struct test_list tlist;

    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    argc--;argv++;

    if (argc != 1)
	bu_exit(1, "Usage: bu_test list list_element_cnt");

    if (bu_opt_long(NULL, 1, (const char **) argv, (void *) &lcnt) < 0 || lcnt < 0)
	bu_exit(1, "Error reading list element count\n");

    BU_LIST_INIT(&(tlist.l));

    if (!BU_LIST_IS_EMPTY(&tlist.l))
	bu_exit(1, "Error - list reports non-empty after initialization\n");

    if (build_list_append(&tlist, lcnt) < 0)
	bu_exit(1, "Error initializing test list with %ld elements\n", lcnt);

    bu_list_free(&tlist.l);

    if (!BU_LIST_IS_EMPTY(&tlist.l))
	bu_exit(1, "Error - list reports non-empty after clean-up\n");


    return bu_list_len(&tlist.l);
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
