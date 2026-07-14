/*                R E A D _ E D G E _ L I S T . C
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
 * Read an IGES 504 Edge List entity from the file into a newly allocated
 * iges_edge_list.
 *
 * The edge use's edge_de is converted to a dir[] index, the entity is
 * verified to be type 504, and each edge's curve DE plus start/end
 * vertex DE and index are read in.  Returns NULL on any error.
 */
struct iges_edge_list *
Read_edge_list(struct iges_edge_use *edge)
{
    struct iges_edge_list *edge_list;
    size_t entityno;
    int sol_num = 0;
    int i;

    entityno = (edge->edge_de - 1)/2;

    if (entityno >= dirarraylen) {
	bu_log("Read_edge_list: DE=%d is too large, dirarraylen = %zu\n", edge->edge_de, dirarraylen);
	return (struct iges_edge_list *)NULL;
    }

    /* Acquiring Data */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n",
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

    BU_ALLOC(edge_list, struct iges_edge_list);

    edge_list->edge_de = edge->edge_de;
    edge_list->next = NULL;
    Readint(&edge_list->no_of_edges, "");
    if (edge_list->no_of_edges <= 0 || edge_list->no_of_edges > 10000000) {
	bu_log("Read_edge_list: illegal number of edges (%d) for entity D%07d\n",
	       edge_list->no_of_edges, dir[entityno]->direct);
	bu_free(edge_list, "Read_edge_list: iges_edge_list");
	return (struct iges_edge_list *)NULL;
    }
    edge_list->i_edge = (struct iges_edge *)bu_calloc(edge_list->no_of_edges, sizeof(*edge_list->i_edge),
						      "Read_edge_list: iges_edge");

    for (i = 0; i < edge_list->no_of_edges; i++) {
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
