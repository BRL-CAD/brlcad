/*
 *			N M G _ M E S H . C
 *
 *	Meshing routines for n-Manifold Geometry
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "nmg.h"
#include "raytrace.h"

/*			G E T _ A N G L E
 *
 *	find the angle between the planes of two edgeuses.
 *	The angle is expressed as a number between 0 and 4 (Butlerians ;-).
 *
 *			       1.0
 *				|
 *			2.0 ----+--- 0.0 or 4.0
 *				|
 *			       3.0
 */
static fastf_t get_angle(eu1, eu2)
struct edgeuse *eu1, *eu2;
{
	double cosangle, dist;
	plane_t Norm1, Norm2;
	pointp_t pt, ptmate;
	point_t test_pt;
	vect_t veu1, veu2, plvec1, plvec2;
	register struct edgeuse *eutmp;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_LOOPUSE(eu1->up.lu_p);
	NMG_CK_FACEUSE(eu1->up.lu_p->up.fu_p);
	NMG_CK_EDGEUSE(eu2);
	NMG_CK_LOOPUSE(eu2->up.lu_p);
	NMG_CK_FACEUSE(eu2->up.lu_p->up.fu_p);


	if (rt_g.NMG_debug & DEBUG_MESH) {
		NMG_CK_VERTEXUSE(eu1->vu_p);
		NMG_CK_VERTEX(eu1->vu_p->v_p);
		NMG_CK_VERTEX_G(eu1->vu_p->v_p->vg_p);
		pt = eu1->vu_p->v_p->vg_p->coord;

		NMG_CK_VERTEXUSE(eu1->eumate_p->vu_p);
		NMG_CK_VERTEX(eu1->eumate_p->vu_p->v_p);
		NMG_CK_VERTEX_G(eu1->eumate_p->vu_p->v_p->vg_p);
		ptmate = eu1->eumate_p->vu_p->v_p->vg_p->coord;

		eutmp = RT_LIST_PLAST_CIRC(edgeuse, eu1);
		NMG_CK_EDGEUSE(eutmp);
		NMG_CK_VERTEXUSE(eutmp->vu_p);
		NMG_CK_VERTEX(eutmp->vu_p->v_p);
		NMG_CK_VERTEX_G(eutmp->vu_p->v_p->vg_p);

		VMOVE(test_pt, eutmp->vu_p->v_p->vg_p->coord);

		rt_log("Angle\t(%g, %g, %g ->) %g, %g, %g -> %g, %g, %g\n",
			test_pt[0], test_pt[1], test_pt[2],
			pt[0], pt[1], pt[2],
			ptmate[0], ptmate[1], ptmate[2]);

		pt = eu2->vu_p->v_p->vg_p->coord;

		NMG_CK_EDGEUSE(eu2);
		NMG_CK_VERTEXUSE(eu2->vu_p);
		NMG_CK_VERTEX(eu2->vu_p->v_p);
		NMG_CK_VERTEX_G(eu2->vu_p->v_p->vg_p);
		ptmate = eu2->eumate_p->vu_p->v_p->vg_p->coord;

		eutmp = RT_LIST_PLAST_CIRC(edgeuse, eu2);
		NMG_CK_EDGEUSE(eutmp);
		NMG_CK_VERTEXUSE(eutmp->vu_p);
		NMG_CK_VERTEX(eutmp->vu_p->v_p);
		NMG_CK_VERTEX_G(eutmp->vu_p->v_p->vg_p);

		VMOVE(test_pt, eutmp->vu_p->v_p->vg_p->coord);

		rt_log("\t(%g, %g, %g ->) %g, %g, %g -> %g, %g, %g\n",
			test_pt[0], test_pt[1], test_pt[2],
			pt[0], pt[1], pt[2],
			ptmate[0], ptmate[1], ptmate[2]);

		nmg_pr_orient(eu1->up.lu_p->up.fu_p->orientation, "\teu1");
		nmg_pr_orient(eu2->up.lu_p->up.fu_p->orientation, "\teu2");
	}


	/* get Normal vectors for edgeuse faces */
	if (eu1->up.lu_p->up.fu_p->orientation == OT_SAME) {
		VMOVEN(Norm1, eu1->up.lu_p->up.fu_p->f_p->fg_p->N, 4);
	} else if (eu1->up.lu_p->up.fu_p->orientation == OT_OPPOSITE){
		VREVERSE(Norm1, eu1->up.lu_p->up.fu_p->f_p->fg_p->N);
		Norm1[3] = -eu1->up.lu_p->up.fu_p->f_p->fg_p->N[3];
	} else rt_bomb("bad fu1 orientation\n");

	if (eu2->up.lu_p->up.fu_p->orientation == OT_SAME) {
		VMOVEN(Norm2, eu2->up.lu_p->up.fu_p->f_p->fg_p->N, 4);
	} else if (eu2->up.lu_p->up.fu_p->orientation == OT_OPPOSITE){
		VREVERSE(Norm2, eu2->up.lu_p->up.fu_p->f_p->fg_p->N);
		Norm2[3] = -eu2->up.lu_p->up.fu_p->f_p->fg_p->N[3];
	} else rt_bomb("bad fu2 orientation\n");

	/* get vectors for edgeuses (edgeuse mates should point
	 * in opposite directions)
	 */
	pt = eu2->vu_p->v_p->vg_p->coord;
	ptmate = eu2->eumate_p->vu_p->v_p->vg_p->coord;
	VSUB2(veu2, ptmate, pt); VUNITIZE(veu2);

	pt = eu1->vu_p->v_p->vg_p->coord;
	ptmate = eu1->eumate_p->vu_p->v_p->vg_p->coord;
	VSUB2(veu1, ptmate, pt); VUNITIZE(veu1);

	/* Get vectors which lie in the plane of each face,
	 * and point left, towards the interior of the CCW loop.
	 */
	VCROSS(plvec1, Norm1, veu1);
	VCROSS(plvec2, Norm2, veu2);

	VUNITIZE(plvec1);
	VUNITIZE(plvec2);

	cosangle = VDOT(plvec1, plvec2);

	VADD2(test_pt, pt, plvec2);
	dist = DIST_PT_PLANE(test_pt, Norm1);

	if (rt_g.NMG_debug & DEBUG_MESH) {
		VPRINT("\tplvec1", plvec1);
		VPRINT("\tplvec2", plvec2);
		rt_log("\tCosangle:%g", cosangle);
	}

	if (dist > 0.0) {
		/* the point is in the direction of the normal vector
		 * from the face of eu1.  Hence the angle between the faces
		 * is between 0 and 180 degrees
		 */
		cosangle = 1.0 - cosangle;
	} else if (dist < 0.0) {
		/* the point is in the opposite direction from the normal
		 * vector of the face of eu1.  Hence the angle between the
		 * faces is between 180 and 360 degrees.
		 */
		cosangle = 3.0 + cosangle;
	} else if (cosangle < 0.0) {
		/* the angle is 180 degrees, or 2 butlerians */
		cosangle = 2.0;
	} else {
		/* the angle is 0 degrees, or 0 butlerians */
		cosangle = 0.0;
	}



	if (rt_g.NMG_debug & DEBUG_MESH)
	rt_log("\tdist:%g\n\tAngle between faces is %g butlerians (0<=X<=4)\n", 
		dist, cosangle);

	return(cosangle);
}

/*			J O I N _ E U
 *
 *	Join edgeuses to same edge
 */
static void join_eu(eu1, eu2)
struct edgeuse *eu1, *eu2;
{
	struct edgeuse *nexteu;
	fastf_t angle1, angle2;
	int	iteration1, iteration2;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu1->radial_p);
	NMG_CK_EDGEUSE(eu1->eumate_p);
	NMG_CK_EDGEUSE(eu2);
	NMG_CK_EDGEUSE(eu2->radial_p);
	NMG_CK_EDGEUSE(eu2->eumate_p);

	if (rt_g.NMG_debug & DEBUG_MESH_EU) {
		EUPRINT("\tJoining", eu1);
		EUPRINT("\t     to", eu2);
	}
	for ( iteration1=0; eu2 && iteration1 < 10000; iteration1++ ) {

		/* because faces are always created with counter-clockwise
		 * exterior loops and clockwise interior loops, radial
		 * edgeuses will never share the same vertex.  We thus make
		 * sure that eu2 is an edgeuse which might be radial to eu1
		 */
		if (eu2->vu_p->v_p == eu1->vu_p->v_p)
			eu2 = eu2->eumate_p;

		/* find a place to insert eu2 on eu1's edge */

		angle1 = get_angle(eu1, eu2);
		angle2 = get_angle(eu1, eu1->radial_p);

		for ( iteration2=0; (angle1 > angle2) && iteration2 < 10000; iteration2++ ) {
			eu1 = eu1->radial_p->eumate_p;
			angle1 = get_angle(eu1, eu2);
			angle2 = get_angle(eu1, eu1->radial_p);
		}
		if(iteration2 >= 10000) rt_bomb("join_eu: infinite loop (2)\n");

		/* find the next use of the edge eu2 is on.  If eu2 and it's
		 * mate are the last uses of the edge, there will be no next
		 * edgeuse to move.
		 */
		nexteu = eu2->radial_p;
		if (nexteu == eu2->eumate_p)
			nexteu = (struct edgeuse *)NULL;


		if (rt_g.NMG_debug & DEBUG_MESH)
			rt_log("joining edgeuses with angle1(to eu2):%g angle2(to eu1->radial):%g\n",
				angle1, angle2);

		/* make eu2 radial to eu1 */
		nmg_moveeu(eu1, eu2);

		/* get ready to move the next edgeuse */
		eu2 = nexteu;
	}
	if( iteration1 >= 10000 )  rt_bomb("join_eu:  infinite loop (1)\n");
}

/*
 *			C M P _ M E S H _ E U
 *
 *	compare the edgeuses in the edgeuse list "eu2" with the edgeuse "eu1"
 *	Any edgeuses which share the same vertices as "eu1" should also be
 *	made to share the same edge.
 */
static cmp_mesh_eu(eu1, hd)
struct edgeuse *eu1;
struct rt_list *hd;
{
	struct edgeuse *eu;

	pointp_t pt1, pt2;
	
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_LIST(hd);

	pt1 = eu1->vu_p->v_p->vg_p->coord;
	pt2 = eu1->eumate_p->vu_p->v_p->vg_p->coord;
	if (rt_g.NMG_debug & DEBUG_MESH_EU)
		rt_log("meshing against %g, %g, %g -> %g, %g, %g (edge %8x)\n",
		pt1[X], pt1[Y], pt1[Z], pt2[X], pt2[Y], pt2[Z], eu1->e_p);

	for (RT_LIST_FOR(eu, edgeuse, hd)) {
		NMG_CK_EDGEUSE(eu);
		if (rt_g.NMG_debug & DEBUG_MESH_EU) {
			pt1 = eu->vu_p->v_p->vg_p->coord;
			pt2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("\tcomparing %g, %g, %g -> %g, %g, %g (edge %8x)\n",
			    pt1[X], pt1[Y], pt1[Z], pt2[X], pt2[Y], pt2[Z], eu->e_p);
		}

		/* if vertices are the same but edges aren't shared, make
		 * them shared
		 */
		if (eu->e_p != eu1->e_p &&
		    (eu->vu_p->v_p == eu1->vu_p->v_p &&
		    eu->eumate_p->vu_p->v_p == eu1->eumate_p->vu_p->v_p ||
		    eu->eumate_p->vu_p->v_p == eu1->vu_p->v_p &&
		    eu->vu_p->v_p == eu1->eumate_p->vu_p->v_p))
			join_eu(eu1, eu);
	}

}

/*
 *			N M G _ M E S H _ F A C E S
 *
 *	Make sure that all shareable edges of fu1/fu2 are indeed shared
 */
void
nmg_mesh_faces(fu1, fu2)
struct faceuse *fu1, *fu2;
{
	struct loopuse *lu1, *lu2;
	struct edgeuse *eu;

	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

    	if (rt_g.NMG_debug & DEBUG_MESH && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fnum=1;
    	    	nmg_pl_2fu( "Before_mesh%d.pl", fnum++, fu1, fu2, 1 );
    	}

	/* Make sure all edges within fu1 that can be shared
	 * with other edges in fu1 are in fact shared.
	 */

	if (rt_g.NMG_debug & DEBUG_MESH)
		rt_log("meshing self (fu %8x)\n", fu1);

	for (RT_LIST_FOR(lu1, loopuse, &fu1->lu_hd)) {

		NMG_CK_LOOPUSE(lu1);
		if (RT_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_EDGEUSE_MAGIC){
			for(RT_LIST_FOR(eu, edgeuse, &lu1->down_hd)) {

				NMG_CK_EDGEUSE(eu);
				for (lu2 = RT_LIST_PNEXT(loopuse, lu1);
				    RT_LIST_NOT_HEAD(lu2, &fu1->lu_hd) ;
				    lu2 = RT_LIST_PNEXT(loopuse, lu2) ) {

				    	if (RT_LIST_FIRST_MAGIC(&lu2->down_hd)
				    	    == NMG_EDGEUSE_MAGIC)
				    	        cmp_mesh_eu(eu, &lu2->down_hd);
				}
			}
		}
	}

	if (rt_g.NMG_debug & DEBUG_MESH)
		rt_log("meshing to other (fu1:%8x fu2:%8x)\n", fu1, fu2);


	/* now make sure that all edges of fu2 that could be shared with
	 * an edge of fu1 are indeed shared
	 */
	for (RT_LIST_FOR(lu1, loopuse, &fu1->lu_hd)) {

		NMG_CK_LOOPUSE(lu1);
		if (RT_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_EDGEUSE_MAGIC){
			for(RT_LIST_FOR(eu, edgeuse, &lu1->down_hd)) {

				NMG_CK_EDGEUSE(eu);
				for (RT_LIST_FOR(lu2, loopuse, &fu2->lu_hd)) {

				    	if(RT_LIST_FIRST_MAGIC(&lu2->down_hd)
				    	    == NMG_EDGEUSE_MAGIC)
						cmp_mesh_eu(eu, &lu2->down_hd);
				}
			}
		}
	}

    	if (rt_g.NMG_debug & DEBUG_MESH && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fno=1;
    	    	nmg_pl_2fu( "After_mesh%d.pl", fno++, fu1, fu2, 1 );
    	}
}
