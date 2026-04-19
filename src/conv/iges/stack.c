/*                         S T A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "./iges_struct.h"

#define STKBLK 100	/* Allocation block size */

static union tree **tree_stk;
static int tree_jtop, tree_stklen;

void
Initstack(void)
{

    tree_jtop = (-1);
    tree_stklen = STKBLK;
    tree_stk = (union tree **)bu_malloc(tree_stklen*sizeof(union tree *), "Initstack: tree_stk");
    if (tree_stk == NULL) {
	bu_log("Cannot allocate stack space\n");
	perror("Initstack");
	bu_exit(1, NULL);
    }
}


/* This function pushes a pointer onto the stack. */

void
Push(union tree *ptr)
{

    tree_jtop++;
    if (tree_jtop == tree_stklen) {
	tree_stklen += STKBLK;
	tree_stk = (union tree **)bu_realloc((char *)tree_stk, tree_stklen*sizeof(union tree *),
					"Push: tree_stk");
	if (tree_stk == NULL) {
	    bu_log("Cannot reallocate stack space\n");
	    perror("Push");
	    bu_exit(1, NULL);
	}
    }
    tree_stk[tree_jtop] = ptr;
}


/* This function pops the top of the stack. */


union tree *
Pop(void)
{
    union tree *ptr;

    if (tree_jtop == (-1))
	ptr = NULL;
    else {
	ptr = tree_stk[tree_jtop];
	tree_jtop--;
    }

    return ptr;
}


void
Freestack(void)
{
    tree_jtop = (-1);
    return;
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
