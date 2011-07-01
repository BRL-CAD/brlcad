/*                      C O P Y T R E E . C
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
/** @file iges/copytree.c
 *
 * This routine copies a tree rooted at "root" by recursion the
 * "parent" field of the root of the new tree is filed by the "parent"
 * argument.
 *
 */

#include "./iges_struct.h"

struct node *Copytree(struct node *root, struct node *parent)
{

    struct node *ptr;

    if (root == NULL)
	return (struct node *)NULL;


    ptr = (struct node *)bu_malloc(sizeof(struct node), "Copytree: ptr");

    *ptr = (*root);
    ptr->parent = parent;

    if (root->left != NULL)
	ptr->left = Copytree(root->left, ptr);

    if (root->right != NULL)
	ptr->right = Copytree(root->right, ptr);

    return ptr;
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
