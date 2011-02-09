/*                     E V A L X F O R M . C
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
#include "./iges_extern.h"


/* This routine evaluates the transformation matrix list for each
 * transformation matrix entity.
 */

struct list /* simple linked list for remembering a matrix sequence */
{
    int index; /* index to "dir" array for a xform matrix */
    struct list *prev; /* link to previous xform matrix */
};


void
Evalxform()
{

    int i, j, xform;
    struct list *ptr, *ptr1, *ptr_root;
    mat_t rot;


    for (i=0; i<totentities; i++) {
	/* loop through all entities */
	/* skip non-transformation entities */
	if (dir[i]->type != 124 && dir[i]->type != 700)
	    continue;

	if (dir[i]->trans >= 0 && !dir[i]->referenced) {
	    /* Make a linked list of the xform matrices
	       in reverse order */
	    ptr = NULL;
	    ptr1 = NULL;
	    ptr_root = NULL;
	    xform = i;
	    while (xform >= 0) {
		if (ptr == NULL)
		    ptr = (struct list *)bu_malloc(sizeof(struct list),
						   "Evalxform: ptr");
		else {
		    ptr1 = ptr;
		    ptr = (struct list *)bu_malloc(sizeof(struct list),
						   "Evalxform: ptr");
		}
		ptr->prev = ptr1;
		ptr->index = xform;
		xform = dir[xform]->trans;
	    }

	    for (j=0; j<16; j++)
		rot[j] = (*identity)[j];

	    ptr_root = ptr;
	    while (ptr != NULL) {
		if (!dir[ptr->index]->referenced) {
		    Matmult(rot, *dir[ptr->index]->rot, rot);
		    for (j=0; j<16; j++)
			(*dir[ptr->index]->rot)[j] = rot[j];
		    dir[ptr->index]->referenced++;
		} else {
		    for (j=0; j<16; j++)
			rot[j] = (*dir[ptr->index]->rot)[j];
		}
		ptr = ptr->prev;
	    }

	    /* Free some memory */
	    ptr = ptr_root;
	    while (ptr) {
		ptr1 = ptr;
		ptr = ptr->prev;
		bu_free((char *)ptr1, "Evalxform: ptr1");
	    }
	}
    }

    /* set matrices for all other entities */
    for (i=0; i<totentities; i++) {
	/* skip xform entities */
	if (dir[i]->type == 124 || dir[i]->type == 700)
	    continue;

	if (dir[i]->trans >= 0)
	    dir[i]->rot = dir[dir[i]->trans]->rot;
	else
	    dir[i]->rot = identity;
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
