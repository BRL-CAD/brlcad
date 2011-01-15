/*                 G E T _ E D G E _ L I S T . C
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

struct iges_edge_list *
Get_edge_list(struct iges_edge_use *edge)
{
    struct iges_edge_list *e_list;

    if (edge_root == NULL) {
	edge_root = Read_edge_list(edge);
	if (edge_root != NULL) {
	    edge_root->next = NULL;
	    return edge_root;
	} else
	    return (struct iges_edge_list *)NULL;
    } else {
	e_list = edge_root;
	while (e_list->next != NULL && e_list->edge_de != edge->edge_de)
	    e_list = e_list->next;
    }

    if (e_list->edge_de == edge->edge_de)
	return e_list;

    e_list->next = Read_edge_list(edge);
    if (e_list->next == NULL)
	return (struct iges_edge_list *)NULL;
    else
	return e_list->next;

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
