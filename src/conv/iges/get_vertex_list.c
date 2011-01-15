/*               G E T _ V E R T E X _ L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
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

struct iges_vertex_list *
Get_vertex_list(int vert_de)
{
    struct iges_vertex_list *v_list;

    if (vertex_root == NULL) {
	vertex_root = Read_vertex_list(vert_de);
	if (vertex_root != NULL) {
	    vertex_root->next = NULL;
	    return vertex_root;
	} else
	    return (struct iges_vertex_list *)NULL;
    } else {
	v_list = vertex_root;
	while (v_list->next != NULL && v_list->vert_de != vert_de)
	    v_list = v_list->next;
    }

    if (v_list->vert_de == vert_de)
	return v_list;

    v_list->next = Read_vertex_list(vert_de);
    if (v_list->next == NULL)
	return (struct iges_vertex_list *)NULL;
    else
	return v_list->next;

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
