/*              R E A D _ V E R T E X _ L I S T . C
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
 * Read an IGES 502 Vertex List entity from the file into a newly
 * allocated iges_vertex_list.
 *
 * The vert_de is converted to a dir[] index, the entity is verified to
 * be type 502, and each vertex coordinate triple is read (and unit
 * converted); the NMG vertex pointer for each is initialized to NULL.
 * Returns NULL on any error.
 */
struct iges_vertex_list *
Read_vertex_list(int vert_de)
{
    struct iges_vertex_list *vertex_list;
    size_t entityno;
    int sol_num = 0;
    int i;

    entityno = IGES_DE2INDEX(vert_de);

    if (entityno >= dirarraylen) {
	bu_log("Read_vertex_list: DE=%d is too large, dirarraylen = %zu\n", vert_de, dirarraylen);
	return (struct iges_vertex_list *)NULL;
    }

    /* Acquiring Data */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n",
	       dir[entityno]->direct, dir[entityno]->name);
	return (struct iges_vertex_list *)NULL;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    if (sol_num != 502) {
	/* this is not an vertex list entity */
	bu_log("Read_vertex_list: entity at DE %d is not an vertex list entity\n", vert_de);
	return (struct iges_vertex_list *)NULL;
    }

    BU_ALLOC(vertex_list, struct iges_vertex_list);

    vertex_list->vert_de = vert_de;
    vertex_list->next = NULL;
    Readint(&vertex_list->no_of_verts, "");
    if (vertex_list->no_of_verts <= 0 || vertex_list->no_of_verts > 10000000) {
	bu_log("Read_vertex_list: illegal number of vertices (%d) for entity D%07d\n",
	       vertex_list->no_of_verts, dir[entityno]->direct);
	bu_free(vertex_list, "Read_vertex_list: iges_vertex_list");
	return (struct iges_vertex_list *)NULL;
    }
    vertex_list->i_verts = (struct iges_vertex *)bu_calloc(vertex_list->no_of_verts, sizeof(*vertex_list->i_verts),
							   "Read_vertex_list: iges_vertex");

    for (i = 0; i < vertex_list->no_of_verts; i++) {
	Readcnv(&vertex_list->i_verts[i].pt[X], "");
	Readcnv(&vertex_list->i_verts[i].pt[Y], "");
	Readcnv(&vertex_list->i_verts[i].pt[Z], "");
	vertex_list->i_verts[i].v = (struct vertex *)NULL;
    }

    return vertex_list;
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
