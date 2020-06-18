/*                      A D D _ F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2020 United States Government as represented by
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

struct faceuse *
Add_face_to_shell(struct shell *s, int entityno, int face_orient)
{

    int sol_num = 0;		/* IGES solid type number */
    int surf_de = 0;		/* Directory sequence number for underlying surface */
    int no_of_loops = 0;		/* Number of loops in face */
    int outer_loop_flag = 0;	/* Indicates if first loop is an the outer loop */
    int *loop_de;		/* Directory sequence numbers for loops */
    int loop;
    int planar = 0;
    struct faceuse *fu;			/* NMG face use */

    /* Acquiring Data */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return (struct faceuse *)NULL;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    Readint(&surf_de, "");
    Readint(&no_of_loops, "");
    Readint(&outer_loop_flag, "");
    loop_de = (int *)bu_calloc(no_of_loops, sizeof(int), "Get_outer_face loop DE's");
    for (loop = 0; loop < no_of_loops; loop++)
	Readint(&loop_de[loop], "");

    /* Check that this is a planar surface */
    if (dir[(surf_de-1)/2]->type == 190) /* plane entity */
	planar = 1;

    if (planar) {
	fu = Make_planar_face(s, (loop_de[0]-1)/2, face_orient);
	if (!fu)
	    goto err;
	for (loop = 1; loop < no_of_loops; loop++) {
	    if (!Add_loop_to_face(s, fu, ((loop_de[loop]-1)/2), face_orient))
		goto err;
	}
    } else if (dir[(surf_de-1)/2]->type == 128) {
	struct face *f;

	fu = Make_nurb_face(s, (surf_de-1)/2);
	NMG_CK_FACEUSE(fu);
	if (!face_orient) {
	    f = fu->f_p;
	    NMG_CK_FACE(f);
	    f->flip = 1;
	}

	NMG_CK_FACE_G_SNURB(fu->f_p->g.snurb_p);

	for (loop = 0; loop < no_of_loops; loop++) {
	    if (!Add_nurb_loop_to_face(s, fu, ((loop_de[loop]-1)/2)))
		goto err;
	}
	NMG_CK_FACE_G_SNURB(fu->f_p->g.snurb_p);
    } else {
	fu = (struct faceuse *)NULL;
	bu_log("Add_face_to_shell: face at DE%d is neither planar nor NURB, ignoring\n", surf_de);
    }

    err :
	bu_free((char *)loop_de, "Add_face_to_shell: loop DE's");
    return fu;
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
