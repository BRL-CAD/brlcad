/*                R E A D _ E D G E _ L I S T . C
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
Read_edge_list(struct iges_edge_use *edge)
{
    struct iges_edge_list *edge_list;
    int entityno;
    int sol_num;
    int i;

    entityno = (edge->edge_de - 1)/2;

    /* Acquiring Data */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return (struct iges_edge_list *)NULL;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    if (sol_num != 504) {
	/* this is not an edge list entity */
	bu_log("Read_edge_list: entity at DE %d is not an edge list entity\n", edge->edge_de);
	return (struct iges_edge_list *)NULL;
    }

    edge_list = (struct iges_edge_list *)bu_malloc(sizeof(struct iges_edge_list)  ,
						   "Read_edge_list: iges_edge_list");

    edge_list->edge_de = edge->edge_de;
    edge_list->next = NULL;
    Readint(&edge_list->no_of_edges, "");
    edge_list->i_edge = (struct iges_edge *)bu_calloc(edge_list->no_of_edges, sizeof(struct iges_edge) ,
						      "Read_edge_list: iges_edge");

    for (i=0; i<edge_list->no_of_edges; i++) {
	Readint(&edge_list->i_edge[i].curve_de, "");
	Readint(&edge_list->i_edge[i].start_vert_de, "");
	Readint(&edge_list->i_edge[i].start_vert_index, "");
	Readint(&edge_list->i_edge[i].end_vert_de, "");
	Readint(&edge_list->i_edge[i].end_vert_index, "");
    }

    return edge_list;
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
