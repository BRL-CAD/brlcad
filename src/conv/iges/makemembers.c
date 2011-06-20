/*                   M A K E M E M B E R S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @file iges/makemembers.c
 *
 * This routine traverses a boolean tree that has been converted to a
 * BRL-CAD acceptable format, and creates the member records for the
 * region.  The tree is traversed in LNR order.
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Makemembers(struct node *root, struct wmember *head)
{
    struct node *ptr;
    struct wmember *wmem;
    fastf_t *flt;
    int op=Union, entno, i;

    Freestack();
    ptr = root;
    while (1) {
	while (ptr != NULL) {
	    Push((union tree *)ptr);
	    ptr = ptr->left;
	}
	ptr = (struct node *)Pop();

	if (ptr == NULL)
	    return;

	if (ptr->op < 0) {
	    /* this is an operand */
	    entno = (-(1+ptr->op)/2); /* entity number */

	    /* make the member record */
	    wmem = mk_addmember(dir[entno]->name, &(head->l), NULL, operator[op]);
	    flt = (fastf_t *)dir[entno]->rot;
	    for (i=0; i<16; i++) {
		wmem->wm_mat[i] = (*flt);
		flt++;
	    }

	    /* increment the reference count */
	    dir[entno]->referenced++;
	} else	/* this is an operator, save it*/
	    op = ptr->op;

	ptr = ptr->right;

    }
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
