/*                      A D D _ F A C E . C
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
#include "./iges_surf.h"

struct faceuse *
Add_face_to_shell(struct shell *s, size_t entityno, int face_orient, struct bu_list *vlfree)
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
    if (dir[IGES_DE2INDEX(surf_de)]->type == 190) /* plane entity */
	planar = 1;

    if (planar) {
	fu = Make_planar_face(s, IGES_DE2INDEX(loop_de[0]), face_orient, vlfree);
	if (!fu)
	    goto err;
	for (loop = 1; loop < no_of_loops; loop++) {
	    if (!Add_loop_to_face(s, fu, (IGES_DE2INDEX(loop_de[loop])), face_orient, vlfree))
		goto err;
	}
    } else if (dir[IGES_DE2INDEX(surf_de)]->type == 128) {
	struct face *f;

	fu = Make_nurb_face(s, IGES_DE2INDEX(surf_de));
	NMG_CK_FACEUSE(fu);
	if (!face_orient) {
	    f = fu->f_p;
	    NMG_CK_FACE(f);
	    f->flip = 1;
	}

	NMG_CK_FACE_G_SNURB(fu->f_p->g.snurb_p);

	for (loop = 0; loop < no_of_loops; loop++) {
	    if (!Add_nurb_loop_to_face(s, fu, (IGES_DE2INDEX(loop_de[loop]))))
		goto err;
	}
	NMG_CK_FACE_G_SNURB(fu->f_p->g.snurb_p);
    } else if (dir[IGES_DE2INDEX(surf_de)]->type == 114 ||
	       dir[IGES_DE2INDEX(surf_de)]->type == 118 ||
	       dir[IGES_DE2INDEX(surf_de)]->type == 120 ||
	       dir[IGES_DE2INDEX(surf_de)]->type == 122 ||
	       dir[IGES_DE2INDEX(surf_de)]->type == 140) {
	/* Analytic / spline surface types converted to an NMG rational
	 * B-spline surface (face_g_snurb) via OpenNURBS.  Build the face
	 * exactly as Make_nurb_face() does for a 128, then attach the
	 * loops as in the 128 branch. */
	struct face *f;
	struct face_g_snurb *srf;
	struct model *m;
	struct vertex *verts[1];
	struct loopuse *lu;

	m = nmg_find_model(&s->l.magic);

	srf = Get_iges_nurb_surf(IGES_DE2INDEX(surf_de), m);
	if (!srf) {
	    fu = (struct faceuse *)NULL;
	    bu_log("Add_face_to_shell: could not convert surface (type %s) at DE%d, ignoring face\n",
		   iges_type(dir[IGES_DE2INDEX(surf_de)]->type), surf_de);
	    goto err;
	}

	verts[0] = (struct vertex *)NULL;
	fu = nmg_cface(s, verts, 1);
	Assign_surface_to_fu(fu, srf);

	/* remove the throwaway loop created by nmg_cface */
	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	(void)nmg_klu(lu);

	NMG_CK_FACEUSE(fu);
	if (!face_orient) {
	    f = fu->f_p;
	    NMG_CK_FACE(f);
	    f->flip = 1;
	}

	NMG_CK_FACE_G_SNURB(fu->f_p->g.snurb_p);

	for (loop = 0; loop < no_of_loops; loop++) {
	    if (!Add_nurb_loop_to_face(s, fu, (IGES_DE2INDEX(loop_de[loop]))))
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
