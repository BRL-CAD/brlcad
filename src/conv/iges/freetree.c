/*                      F R E E T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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

void
Freetree(struct node *root)
{
    struct node *ptr;

    ptr = root;
    while (1) {
	while (ptr != NULL) {
	    Push((union tree *)ptr);
	    ptr = ptr->left;
	}
	ptr = (struct node *)Pop();
	bu_free((char *)ptr, "Freetree: ptr");

	if (ptr->parent == NULL)
	    return;

	if (ptr != ptr->parent->right)
	    ptr = ptr->parent->right;
	else
	    ptr = NULL;

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
