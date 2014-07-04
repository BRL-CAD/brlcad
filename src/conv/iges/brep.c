/*                          B R E P . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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

int
brep(int entityno)
{

    int sol_num;		/* IGES solid type number */
    int shell_de;		/* Directory sequence number for a shell */
    int orient;			/* Orientation of shell */
    int *void_shell_de;		/* Directory sequence number for an void shell */
    int *void_orient;		/* Orientation of void shell */
    int num_of_voids;		/* Number of inner void shells */
    struct shell *s;		/* Outer shell */
    struct iges_vertex_list *v_list;
    struct iges_vertex_list *v_list_tmp;
    struct iges_edge_list *e_list;
    struct iges_edge_list *e_list_tmp;
    int i;
    int mk_nmg_executed_flag = 0;	/* Boolean indicating if mk_nmg was executed */

    /* Acquiring Data */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return 0;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    Readint(&shell_de, "");
    Readint(&orient, "");
    Readint(&num_of_voids, "");

    if (num_of_voids) {
	void_shell_de = (int *)bu_calloc(num_of_voids, sizeof(int), "BREP: void shell DE's");
	void_orient = (int *)bu_calloc(num_of_voids, sizeof(int), "BREP: void shell orients");

	for (i = 0; i < num_of_voids; i++) {
	    Readint(&void_shell_de[i], "");
	    Readint(&void_orient[i], "");
	}
    } else {
	void_shell_de = NULL;
	void_orient = NULL;
    }

    /* start building */
    s = nmg_ms();

    /* orient shells */
    nmg_fix_normals(s, &tol);
    nmg_fix_normals(s, &tol);
    nmg_invert_shell(s);

    if (do_bots) {
	/* write out BOT */
	if (mk_bot_from_nmg(fdout, dir[entityno]->name, s))
	    goto err;
    } else {
	/* Compute "geometry" for region and shell */
	nmg_shell_a(s, &tol);

	/* Write NMG solid */
	mk_nmg_executed_flag = 1;
	if (mk_nmg(fdout, dir[entityno]->name, s))
	    goto err;
    }

    v_list = vertex_root;
    while (v_list != NULL) {
	v_list_tmp = v_list->next;
	bu_free((char *)v_list->i_verts, "brep: iges_vertex");
	bu_free((char *)v_list, "brep: vertex list");
	v_list = v_list_tmp;
    }
    vertex_root = NULL;

    e_list = edge_root;
    while (e_list != NULL) {
	e_list_tmp = e_list->next;
	bu_free((char *)e_list->i_edge, "brep:iges_edge");
	bu_free((char *)e_list, "brep: edge list");
	e_list = e_list_tmp;
    }
    edge_root = NULL;

    /* perform nmg kill model if make nmg was not executed since
     * make nmg will have already performed an nmg kill model
     */
    if (!mk_nmg_executed_flag) {
	nmg_ks(s);
    }

    return 1;

    err :
	if (num_of_voids) {
	    bu_free((char *)void_shell_de, "BREP: void shell DE's");
	    bu_free((char *)void_orient, "BREP: void shell orients");
	}

    /* perform nmg kill model if make nmg was not executed since
     * make nmg will have already performed an nmg kill model
     */
    if (!mk_nmg_executed_flag) {
	nmg_ks(s);
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
