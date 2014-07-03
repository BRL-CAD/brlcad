/*               G E T _ O U T E R _ S H E L L . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
#include "brep.h"

bool
Get_outer_brep(ON_Brep* brep, int entityno, int shell_orient)
{
    int sol_num;		/* IGES solid type number */
    int no_of_faces;		/* Number of faces in shell */
    int face_count = 0;		/* Number of faces actually made */
    int *face_de;		/* Directory sequence numbers for faces */
    int *face_orient;		/* Orientation of faces */
    int face;

    /* Acquiring Data */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct , dir[entityno]->name);
	return 0;
    }
    Readrec(dir[entityno]->param);
    Readint(&sol_num , "");
    Readint(&no_of_faces , "");

    face_de = (int *)bu_calloc(no_of_faces , sizeof(int) , "Get_outer_shell face DE's");
    face_orient = (int *)bu_calloc(no_of_faces , sizeof(int) , "Get_outer_shell orients");

    for (face = 0; face < no_of_faces; face++) {
	Readint(&face_de[face] , "");
	Readint(&face_orient[face] , "");
    }

    for (face = 0; face < no_of_faces; face++) {
	Add_face_to_brep(brep, (face_de[face]-1)/2, face_orient[face]);
    }

    // ??? do I need to glue these faces together?

    bu_free((char *)face_de , "Get_outer_shell: face DE's");
    bu_free((char *)face_orient , "Get_outer_shell: face orients");
    return s;
}


struct shell *
Get_outer_shell(r , entityno , shell_orient)
     struct nmgregion *r;
     int entityno;
     int shell_orient;
{

    int sol_num;		/* IGES solid type number */
    int no_of_faces;		/* Number of faces in shell */
    int face_count = 0;		/* Number of faces actually made */
    int *face_de;		/* Directory sequence numbers for faces */
    int *face_orient;		/* Orientation of faces */
    int face;
    struct shell *s;			/* NMG shell */
    struct faceuse **fu;			/* list of faceuses */

    /* Acquiring Data */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct , dir[entityno]->name);
	return 0;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num , "");
    Readint(&no_of_faces , "");

    face_de = (int *)bu_calloc(no_of_faces , sizeof(int) , "Get_outer_shell face DE's");
    face_orient = (int *)bu_calloc(no_of_faces , sizeof(int) , "Get_outer_shell orients");
    fu = (struct faceuse **)bu_calloc(no_of_faces , sizeof(struct faceuse *) , "Get_outer_shell faceuses ");

    for (face = 0; face < no_of_faces; face++) {
	Readint(&face_de[face] , "");
	Readint(&face_orient[face] , "");
    }

    s = nmg_msv(r);

    for (face = 0; face < no_of_faces; face++) {
	fu[face_count] = Add_face_to_shell(s , (face_de[face]-1)/2 , face_orient[face]);
	if (fu[face_count] != (struct faceuse *)NULL)
	    face_count++;
    }

    nmg_gluefaces(fu , face_count, &tol);

    bu_free((char *)fu , "Get_outer_shell: faceuse list");
    bu_free((char *)face_de , "Get_outer_shell: face DE's");
    bu_free((char *)face_orient , "Get_outer_shell: face orients");
    return s;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
