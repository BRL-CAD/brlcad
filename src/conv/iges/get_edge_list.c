/*                 G E T _ E D G E _ L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2026 United States Government as represented by
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

/*
 * Return the IGES 504 edge list referenced by an IGES edge use.
 *
 * Edge lists are cached in the global edge_root singly-linked list, each
 * node keyed by its edge_de.  If a matching node is already present it is
 * returned; otherwise the list is read from the file with
 * Read_edge_list() and appended to the cache.
 */
struct iges_edge_list *
Get_edge_list(struct iges_edge_use *edge)
{
    struct iges_edge_list *e_list;

    /* empty cache: read the first list and make it the cache root */
    if (edge_root == NULL) {
	edge_root = Read_edge_list(edge);
	if (edge_root != NULL) {
	    edge_root->next = NULL;
	    return edge_root;
	} else
	    return (struct iges_edge_list *)NULL;
    } else {
	/* walk the cache looking for a node keyed by this edge_de */
	e_list = edge_root;
	while (e_list->next != NULL && e_list->edge_de != edge->edge_de)
	    e_list = e_list->next;
    }

    /* cache hit */
    if (e_list->edge_de == edge->edge_de)
	return e_list;

    /* cache miss: read the list and append it to the end of the cache */
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
