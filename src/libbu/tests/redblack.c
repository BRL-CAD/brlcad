/*                    R E D B L A C K . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2019 United States Government as represented by
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
/** @file bu_redblack.c
 * Test unit for Red - Black Tree API
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bu.h"

static int
compareFunc(const void* a, const void* b)
{
    return *(int *) a - *(int *) b;
}


/**
 * Function to be applied to every node of the
 * red-black tree.
 */
static void
displayNode(void* data, int dep)
{
    printf("Depth = %d Value = %s, ", dep, (char*)data);
}


int
main(int UNUSED(ac), char *UNUSED(av[]))
{
    struct bu_rb_tree *testTree;
    void *searchedValue;
    const char *sources[] = {"h", "e", "a", "l", "l", "o"};
    int i = 0;
    int passed = 0;
    bu_rb_cmp_t farray[1];

    farray[0] = &compareFunc;

    testTree = bu_rb_create("TestingTree", 1, farray);
    for (i = 0; i < 6; i++)
	bu_rb_insert(testTree, (void *)sources[i]);

    printf("SEARCH TEST: \n\tSEARCHING AN EXISTING VALUE:\n");
    searchedValue = bu_rb_search(testTree, 0, (void *)"h");

    if (searchedValue == NULL) {
	printf("\t\t\t[FAILED]\n\t\t\tShould be h \n");
    } else {
	printf("\t\t\t[PASSED]\n");
	passed++;
    }

    printf("\tSEARCHING A NONEXISTENT VALUE:\n");
    searchedValue = bu_rb_search(testTree, 0, (void *)"not");

    if (searchedValue == 0) {
	printf("\t\t\t[PASSED]\n");
	passed++;
    } else {
	printf("\t\t\t[FAILED]\n\t\t\tShould be NULL\n");
    }

    printf("DELETE TEST: \n\tDELETING AN EXISTENT VALUE:\n");
    searchedValue = bu_rb_search(testTree, 0, (void *)"a");
    bu_rb_delete(testTree, 0);

    printf("\tSEARCHING THE SAME VALUE AFTER DELETION \n");
    searchedValue = bu_rb_search(testTree, 0, (void *)"a");

    if (searchedValue == 0) {
	printf("\t\t\t[PASSED]\n");
	passed++;
    } else {
	printf("\t\t\t[FAILED]\n\t\t\tShould be NULL\n");
    }

    /* user tests */
    printf("RED-BLACK TREE WALKING TESTS :\n");

    printf("\nPREORDER:\n");
    bu_rb_walk(testTree, 0, BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(displayNode), 0);
    bu_rb_diagnose_tree(testTree, 0, BU_RB_WALK_PREORDER);
    searchedValue = bu_rb_search(testTree, 0, (void *)"h");

    printf("\nPREORDER AFTER SEARCH:\n");
    bu_rb_walk(testTree, 0, BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(displayNode), 0);
    bu_rb_diagnose_tree(testTree, 0, BU_RB_WALK_PREORDER);

    printf("\nINORDER:\n");
    bu_rb_walk(testTree, 0, BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(displayNode), 1);
    bu_rb_diagnose_tree(testTree, 0, BU_RB_WALK_INORDER);

    printf("\nPOSTORDER\n");
    bu_rb_walk(testTree, 0, BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(displayNode), 2);
    bu_rb_diagnose_tree(testTree, 0, BU_RB_WALK_POSTORDER);

    return passed != 3;
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
