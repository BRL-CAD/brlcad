/*
 *			N M G _ M E S H . C
 *
 *	Meshing routines for n-Manifold Geometry
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
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
	struct faceuse	*fu1, *fu2;
	register struct edgeuse *eutmp;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_LOOPUSE(eu1->up.lu_p);
	fu1 = eu1->up.lu_p->up.fu_p;
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACE(fu1->f_p);
	NMG_CK_FACE_G(fu1->f_p->fg_p);

	NMG_CK_EDGEUSE(eu2);
	NMG_CK_LOOPUSE(eu2->up.lu_p);
	fu2 = eu2->up.lu_p->up.fu_p;
	NMG_CK_FACEUSE(fu2);
	NMG_CK_FACE(fu2->f_p);
	NMG_CK_FACE_G(fu2->f_p->fg_p);

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

		nmg_pr_orient(fu1->orientation, "\teu1");
		nmg_pr_orient(fu2->orientation, "\teu2");
	}

	/* get Normal vectors for edgeuse faces */
	if (fu1->orientation == OT_SAME) {
		VMOVEN(Norm1, fu1->f_p->fg_p->N, 4);
	} else if (fu1->orientation == OT_OPPOSITE){
		VREVERSE(Norm1, fu1->f_p->fg_p->N);
		Norm1[3] = -fu1->f_p->fg_p->N[3];
	} else rt_bomb("bad fu1 orientation\n");

	if (fu2->orientation == OT_SAME) {
		VMOVEN(Norm2, fu2->f_p->fg_p->N, 4);
	} else if (fu2->orientation == OT_OPPOSITE){
		VREVERSE(Norm2, fu2->f_p->fg_p->N);
		Norm2[3] = -fu2->f_p->fg_p->N[3];
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

/*
 *			N M G _ R A D I A L _ J O I N _ E U
 *
 *	Make all the edgeuses around eu2's edge to refer to eu1's edge,
 *	taking care to organize them into the proper angular orientation,
 *	so that the attached faces are correctly arranged radially
 *	around the edge.
 *
 *	This depends on both edges being part of face loops,
 *	with vertex and face geometry already associated.
 */
void
nmg_radial_join_eu(eu1, eu2)
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

	if( eu1->e_p == eu2->e_p )  return;
	if( (eu1->vu_p->v_p != eu2->vu_p->v_p && eu1->vu_p->v_p != eu2->eumate_p->vu_p->v_p) ||
	    (eu1->eumate_p->vu_p->v_p != eu2->vu_p->v_p && eu1->eumate_p->vu_p->v_p != eu2->eumate_p->vu_p->v_p) )
		rt_bomb("nmg_radial_join_eu(): edgeuses don't share both vertices\n");

	if (rt_g.NMG_debug & (DEBUG_MESH_EU|DEBUG_MESH) ) {
		rt_log("nmg_radial_join_eu(eu1=x%x, eu2=x%x) e1=x%x, e2=x%x\n",
			eu1, eu2,
			eu1->e_p, eu2->e_p);
		EUPRINT("\tJoining", eu1);
		EUPRINT("\t     to", eu2);
	}
	for ( iteration1=0; eu2 && iteration1 < 10000; iteration1++ ) {
		struct edgeuse	*first_eu1 = eu1;

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
			if( iteration2 > 9997 )  rt_g.NMG_debug |= DEBUG_MESH;
			/* If eu1 is only one pair of edgeuses, done */
			if( eu1 == eu1->radial_p->eumate_p )  break;
			eu1 = eu1->radial_p->eumate_p;
			if( eu1 == first_eu1 )  {
				rt_bomb("nmg_radial_join_eu():  went full circle, no face insertion point.\n");
				break;
			}
			angle1 = get_angle(eu1, eu2);
			angle2 = get_angle(eu1, eu1->radial_p);
		}
		if(iteration2 >= 10000)  {
			rt_log("angle1=%e, angle2=%e\n", angle1, angle2);
			rt_bomb("nmg_radial_join_eu: infinite loop (2)\n");
		}

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
	if( iteration1 >= 10000 )  rt_bomb("nmg_radial_join_eu:  infinite loop (1)\n");
}

/*
 *			N M G _ M E S H _ T W O _ F A C E S
 *
 *  Actuall do the work of meshing two faces.
 *  The two fu arguments may be the same, which causes the face to be
 *  meshed against itself.
 *
 *  The return is the number of edges meshed.
 */
static int
nmg_mesh_two_faces(fu1, fu2)
register struct faceuse *fu1, *fu2;
{
	struct loopuse	*lu1;
	struct loopuse	*lu2;
	struct edgeuse	*eu1;
	struct edgeuse	*eu2;
	struct vertex	*v1a, *v1b;
	struct edge	*e1;
	pointp_t	pt1, pt2;
	int		count = 0;

	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	/* Visit all the loopuses in faceuse 1 */
	for (RT_LIST_FOR(lu1, loopuse, &fu1->lu_hd)) {
		NMG_CK_LOOPUSE(lu1);
		/* Ignore self-loops */
		if (RT_LIST_FIRST_MAGIC(&lu1->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;

		/* Visit all the edgeuses in loopuse1 */
		for(RT_LIST_FOR(eu1, edgeuse, &lu1->down_hd)) {
			NMG_CK_EDGEUSE(eu1);

			v1a = eu1->vu_p->v_p;
			v1b = eu1->eumate_p->vu_p->v_p;
			NMG_CK_VERTEX(v1a);
			NMG_CK_VERTEX(v1b);
			e1 = eu1->e_p;
			NMG_CK_EDGE(e1);
			if (rt_g.NMG_debug & DEBUG_MESH_EU)  {
				pt1 = v1a->vg_p->coord;
				pt2 = v1b->vg_p->coord;
				rt_log("ref_e=%8x v:%8x--%8x (%g, %g, %g)->(%g, %g, %g)\n",
					e1, v1a, v1b,
					V3ARGS(pt1), V3ARGS(pt2) );
			}

			/* Visit all the loopuses in faceuse2 */
			for (RT_LIST_FOR(lu2, loopuse, &fu2->lu_hd)) {
				/* Ignore self-loops */
				if(RT_LIST_FIRST_MAGIC(&lu2->down_hd) != NMG_EDGEUSE_MAGIC)
					continue;
				/* Visit all the edgeuses in loopuse2 */
				for( RT_LIST_FOR(eu2, edgeuse, &lu2->down_hd) )  {
					NMG_CK_EDGEUSE(eu2);
					if (rt_g.NMG_debug & DEBUG_MESH_EU) {
						pt1 = eu2->vu_p->v_p->vg_p->coord;
						pt2 = eu2->eumate_p->vu_p->v_p->vg_p->coord;
						rt_log("\te:%8x v:%8x--%8x (%g, %g, %g)->(%g, %g, %g)\n",
							eu2->e_p,
							eu2->vu_p->v_p,
							eu2->eumate_p->vu_p->v_p,
							V3ARGS(pt1), V3ARGS(pt2) );
					}

					/* See if already shared */
					if( eu2->e_p == e1 ) continue;
					if( (eu2->vu_p->v_p == v1a &&
					     eu2->eumate_p->vu_p->v_p == v1b) ||
					    (eu2->eumate_p->vu_p->v_p == v1a &&
					     eu2->vu_p->v_p == v1b) )  {
						nmg_radial_join_eu(eu1, eu2);
					     	count++;
					 }
				}
			}
		}
	}
	return count;
}

/*
 *			N M G _ M E S H _ F A C E S
 *
 *  Scan through all the edges of fu1 and fu2, ensuring that all
 *  edges involving the same vertex pair are indeed shared.
 *  This means worrying about merging ("meshing") all the faces in the
 *  proper radial orientation around the edge.
 *  XXX probably should return(count);
 */
void
nmg_mesh_faces(fu1, fu2)
struct faceuse *fu1, *fu2;
{
	int	count = 0;

	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

    	if (rt_g.NMG_debug & DEBUG_MESH && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fnum=1;
    	    	nmg_pl_2fu( "Before_mesh%d.pl", fnum++, fu1, fu2, 1 );
    	}

	if (rt_g.NMG_debug & DEBUG_MESH)
		rt_log("meshing self (fu1 %8x)\n", fu1);
	count += nmg_mesh_two_faces( fu1, fu1 );

	if (rt_g.NMG_debug & DEBUG_MESH)
		rt_log("meshing self (fu2 %8x)\n", fu2);
	count += nmg_mesh_two_faces( fu2, fu2 );

	if (rt_g.NMG_debug & DEBUG_MESH)
		rt_log("meshing to other (fu1:%8x fu2:%8x)\n", fu1, fu2);
	count += nmg_mesh_two_faces( fu1, fu2 );

    	if (rt_g.NMG_debug & DEBUG_MESH && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fno=1;
    	    	nmg_pl_2fu( "After_mesh%d.pl", fno++, fu1, fu2, 1 );
    	}
}

/*
 *			N M G _ M E S H _ F A C E _ S H E L L
 *
 *  The return is the number of edges meshed.
 */
int
nmg_mesh_face_shell( fu1, s )
struct faceuse	*fu1;
struct shell	*s;
{
	register struct faceuse	*fu2;
	int		count = 0;

	NMG_CK_FACEUSE(fu1);
	NMG_CK_SHELL(s);

	count += nmg_mesh_two_faces( fu1, fu1 );
	for( RT_LIST_FOR( fu2, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu2);
		count += nmg_mesh_two_faces( fu2, fu2 );
		count += nmg_mesh_two_faces( fu1, fu2 );
	}
	/* XXX What about wire edges in the shell? */
	return count;
}

/*
 *			N M G _ M E S H _ S H E L L _ S H E L L
 *
 *  Mesh every edge in shell 1 with every edge in shell 2.
 *  The return is the number of edges meshed.
 *
 *  Does not use nmg_mesh_face_shell() to keep face/self meshing
 *  to the absolute minimum necessary.
 */
int
nmg_mesh_shell_shell( s1, s2 )
struct shell	*s1;
struct shell	*s2;
{
	struct faceuse	*fu1;
	struct faceuse	*fu2;
	int		count = 0;

	NMG_CK_SHELL(s1);
	NMG_CK_SHELL(s2);

	/* First, mesh all faces of shell 2 with themselves */
	for( RT_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )  {
		NMG_CK_FACEUSE(fu2);
		count += nmg_mesh_two_faces( fu2, fu2 );
	}

	/* Visit every face in shell 1 */
	for( RT_LIST_FOR( fu1, faceuse, &s1->fu_hd ) )  {
		NMG_CK_FACEUSE(fu1);

		/* First, mesh each face in shell 1 with itself */
		count += nmg_mesh_two_faces( fu1, fu1 );

		/* Visit every face in shell 2 */
		for( RT_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )  {
			NMG_CK_FACEUSE(fu2);
			count += nmg_mesh_two_faces( fu1, fu2 );
		}
	}
	/* XXX What about wire edges in the shell? */
	return count;
}
