/*                     T R E E C H E C K . C
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

#include "./iges_struct.h"

#define	Union		1
#define	Intersect	2
#define	Subtract	3

int
Treecheck(struct node *root)
{
    struct node *ptr, *Pop();

    ptr = root;
    while (1) {
	while (ptr != NULL) {
	    Push(ptr);
	    ptr = ptr->left;
	}
	ptr = Pop();

	if (ptr->op == Intersect || ptr->op == Subtract)
	    if (ptr->right->op == Union)	/* Not a legal BRL-CAD tree */
		return 0;
	if (ptr->parent == NULL)
	    return 1;

	if (ptr != ptr->parent->right)
	    ptr = ptr->parent->right;
	else
	    ptr = NULL;

    }
    return 0;
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
