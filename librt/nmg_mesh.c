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

/* XXX move to raytrace.h */
RT_EXTERN(double nmg_measure_2fu_angle, (CONST struct edgeuse *eu1, CONST struct edgeuse *eu2));
RT_EXTERN(double nmg_measure_fu_angle, (CONST struct edgeuse *eu, CONST vect_t xvec, CONST vect_t yvec, CONST vect_t zvec));


/*
 *			N M G _ M E A S U R E _ 2 F U _ A N G L E
 *
 *  Return the angle (in radians) from the interior portion of the
 *  faceuse associated with eu1, to the interior portion of the faceuse
 *  associated with eu2, around the common (geometric) edge
 *  shared by eu1 and eu2.
 *
 *  This is done by finding a "left"ward (into-the-face-ward) pointing
 *  vector for each faceuse, and measuring the angle between them.
 *
 *  Calling this routine with two common edgeuses of one face
 *  (different faceuses) should always return PI (180 degrees).
 *
 *  The return will be the same, regardless of whether eu2 or eu2->eumate_p
 *  is passed, because the interior of that face(use) is the same.
 */
double nmg_measure_2fu_angle(eu1, eu2)
CONST struct edgeuse *eu1, *eu2;
{
 	vect_t		Norm1, Norm2;
 	CONST fastf_t	*pt, *ptmate;
 	point_t		test_pt;
 	vect_t		edgevect1;
 	vect_t		edgevect2;
 	vect_t		left1;
	vect_t		left2;
	vect_t		right1;
 	vect_t		right2;
 	CONST struct loopuse	*lu1, *lu2;
 	CONST struct faceuse	*fu1, *fu2;
 	register CONST struct edgeuse *eutmp;
 	CONST struct vertex	*v1a, *v1b;
	CONST struct vertex	*v2a, *v2b;
 	double		ret;

	NMG_CK_EDGEUSE(eu1);
	lu1 = eu1->up.lu_p;
	NMG_CK_LOOPUSE(lu1);
	fu1 = lu1->up.fu_p;
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACE(fu1->f_p);
	NMG_CK_FACE_G(fu1->f_p->fg_p);

	NMG_CK_EDGEUSE(eu2);
	lu2 = eu2->up.lu_p;
	NMG_CK_LOOPUSE(lu2);
	fu2 = lu2->up.fu_p;
	NMG_CK_FACEUSE(fu2);
	NMG_CK_FACE(fu2->f_p);
	NMG_CK_FACE_G(fu2->f_p->fg_p);

	NMG_CK_VERTEXUSE(eu1->vu_p);
	v1a = eu1->vu_p->v_p;
	NMG_CK_VERTEX(v1a);

	NMG_CK_VERTEXUSE(eu1->eumate_p->vu_p);
	v1b = eu1->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v1b);

	NMG_CK_VERTEXUSE(eu2->vu_p);
	v2a = eu2->vu_p->v_p;
	NMG_CK_VERTEX(v2a);

	NMG_CK_VERTEXUSE(eu2->eumate_p->vu_p);
	v2b = eu2->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v2b);

#if 0
	if (rt_g.NMG_debug & DEBUG_MESH_EU) {
		NMG_CK_VERTEX_G(v1a->vg_p);
		pt = v1a->vg_p->coord;

		NMG_CK_VERTEX_G(v1b->vg_p);
		ptmate = v1b->vg_p->coord;

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

		NMG_CK_VERTEX_G(v2a->vg_p);
		pt = v2a->vg_p->coord;
		NMG_CK_VERTEX_G(v2b->vg_p);
		ptmate = v2b->vg_p->coord;

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
#endif

	/* get Normal vectors for edgeuse faceUSEs */
	if (fu1->orientation == OT_SAME) {
		VMOVE(Norm1, fu1->f_p->fg_p->N);
	} else if (fu1->orientation == OT_OPPOSITE){
		VREVERSE(Norm1, fu1->f_p->fg_p->N);
	} else rt_bomb("bad fu1 orientation\n");

	if (fu2->orientation == OT_SAME) {
		VMOVE(Norm2, fu2->f_p->fg_p->N);
	} else if (fu2->orientation == OT_OPPOSITE){
		VREVERSE(Norm2, fu2->f_p->fg_p->N);
	} else rt_bomb("bad fu2 orientation\n");

	/*
	 *  Get direction vector for each edgeuse.
	 *  These may happen to be pointed in same or opposite direction.
	 *  Regardless, their only purpose is to get the 'left' vectors.
	 */
	pt = v2a->vg_p->coord;
	ptmate = v2b->vg_p->coord;
	VSUB2(edgevect2, ptmate, pt);
	VUNITIZE(edgevect2);

	pt = v1a->vg_p->coord;
	ptmate = v1b->vg_p->coord;
	VSUB2(edgevect1, ptmate, pt);
	VUNITIZE(edgevect1);

	/* XXX It would be a good double-check here to make sure that
	 * XXX edgevect1 and edgevect2 dot to either 1 or -1.  What dot tol to use?
	 */
	ret = fabs(VDOT(edgevect1, edgevect2)) - 1;
	if( !NEAR_ZERO(ret, 1.0e-6) )
		rt_log("nmg_measure_2fu_angle() WARNING: bad edge dot=%g\n", ret);

	/*
	 * Because edgeuses are oriented, and run CCW for an exterior
	 * face loop, crossing the face normal with the edge vector will
	 * give a vector which lies in the plane of the face and
	 * points "left", towards the interior of the faceloop.
	 */
	VCROSS(left1, Norm1, edgevect1);
	VCROSS(left2, Norm2, edgevect2);

	VREVERSE( right1, left1 );
	VREVERSE( right2, left2 );

	/*
	 *  Define the plane of rotation about the edge.
	 *    +X = right1
	 *    +Y = face1 normal
	 *   (+Z = edge1 vector)
	 *
	 *  The angle to the other face is the angle from right1 to right2,
	 *  lieing in the plane defined by right1 and the normal.
	 */
	ret = rt_angle_measure( right2, right1, fu1->f_p->fg_p->N );

#if 0
/* Put this one back in */
	if (rt_g.NMG_debug & DEBUG_MESH_EU)  {
		rt_log("\tnmg_measure_2fu_angle(x%x, x%x) = %g radians (%g deg)\n",
			eu1, eu2, ret, ret * rt_radtodeg);
	}
#endif
	return ret;
}

/*
 *  Given an edgeuse, return two arbitrary unit-length vectors which
 *  are perpendicular to each other and to the edgeuse, such that
 *  they can be considered the +X and +Y axis, and the edgeuse is +Z.
 *  That is, X cross Y = Z.
 *
 *  Useful for erecting a coordinate system around an edge suitable
 *  for measuring the angles of other edges and faces with.
 */
void
nmg_eu_2vecs_perp( xvec, yvec, zvec, eu )
vect_t		xvec;
vect_t		yvec;
vect_t		zvec;
CONST struct edgeuse	*eu;
{
	CONST struct vertex	*v1, *v2;

	NMG_CK_EDGEUSE(eu);
	v1 = eu->vu_p->v_p;
	NMG_CK_VERTEX(v1);
	v2 = eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v2);
	if( v1 == v2 )  rt_bomb("nmg_eu_2vecs_perp() start&end vertex of edge are the same!\n");

	/* XXX Could really use 'tol' arg here, to see if v1~=v2, within tol */

	NMG_CK_VERTEX_G(v1->vg_p);
	NMG_CK_VERTEX_G(v2->vg_p);
	VSUB2( zvec, v2->vg_p->coord, v1->vg_p->coord );
	VUNITIZE( zvec );

	mat_vec_perp( xvec, zvec );
	VCROSS( yvec, zvec, xvec );
VPRINT("xvec", xvec);
VPRINT("yvec", yvec);
VPRINT("zvec", zvec);
}

/*
 *			N M G _ M E A S U R E _ F U _ A N G L E
 *
 *  Return the angle in radians from the interior portion of the faceuse
 *  associated with edgeuse 'eu', measured in the coordinate system
 *  defined by xvec and yvec, which are known to be perpendicular to
 *  each other, and to the edge vector.
 *
 *  This is done by finding the "left-ward" vector for the edge in the
 *  face, which points into the interior of the face, and measuring
 *  the angle it forms relative to xvec and yvec.
 */
double
nmg_measure_fu_angle( eu, xvec, yvec, zvec )
CONST struct edgeuse	*eu;
CONST vect_t		xvec;
CONST vect_t		yvec;
CONST vect_t		zvec;
{
	CONST struct loopuse	*lu;
	CONST struct faceuse	*fu;
	vect_t			Norm;
	vect_t			edgevect;
	vect_t			left;
	double			ret;

	NMG_CK_EDGEUSE(eu);
	lu = eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);
	fu = lu->up.fu_p;
	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_FACE_G(fu->f_p->fg_p);

	/* get Normal vectors for edgeuse faceUSEs */
	if (fu->orientation == OT_SAME) {
		VMOVE(Norm, fu->f_p->fg_p->N);
	} else if (fu->orientation == OT_OPPOSITE){
		VREVERSE(Norm, fu->f_p->fg_p->N);
	} else rt_bomb("bad fu orientation\n");

	VSUB2( edgevect, eu->eumate_p->vu_p->v_p->vg_p->coord, eu->vu_p->v_p->vg_p->coord );
	VUNITIZE( edgevect );

	/* Make sure the edge is aligned with +Z or -Z */
	ret = fabs(VDOT(edgevect, zvec)) - 1;
	if( !NEAR_ZERO(ret, 1.0e-6) )
		rt_log("nmg_measure_fu_angle() WARNING: bad edge/zvec dot=%g\n", ret);

	VCROSS( left, Norm, edgevect );

	ret = rt_angle_measure( left, xvec, yvec );

#if 0
	if (rt_g.NMG_debug & DEBUG_MESH_EU)  {
		rt_log("\tnmg_measure_fu_angle(x%x) = %g radians (%g deg)\n",
			eu, ret, ret * rt_radtodeg);
	}
#endif
	return ret;
}

/*
 *			N M G _ I S _ A N G L E _ I N _ W E D G E
 *
 *  Determine if T lies within angle AB, such that A < T < B.
 *  The angle B is expected to be "more ccw" than A.
 *  Because of the wrap from 2pi to 0, B may have a smaller numeric value.
 *
 *  Returns -
 *	-2	t is equal to a
 *	-1	t is equal to b
 *	 0	t is outside angle ab
 *	 1	t is inside angle ab
 */
int
nmg_is_angle_in_wedge( a, b, t )
double	a;
double	b;
double	t;
{
	/* XXX What tolerance to use here (in radians)? */
	if( NEAR_ZERO( a-t, 1.0e-8 ) )  return -2;
	if( NEAR_ZERO( b-t, 1.0e-8 ) )  return -1;

	/* If A==B, if T is not also equal, it's outside the wedge */
	if( NEAR_ZERO( a-b, 1.0e-8 ) )  return 0;

	if( b < a )  {
		/* B angle has wrapped past zero, add on 2pi */
		if( t <= b )  {
			/* Range is A..0, 0..B, and 0<t<B; so T is in wedge */
			return 1;
		}
		b += rt_twopi;
	}
	if( NEAR_ZERO( b-t, 1.0e-8 ) )  return -1;

	if( t < a )  return 0;
	if( t > b )  return 0;
	return 1;
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
 *
 *  The two edgeuses being joined might well be from separate shells,
 *  so the issue of preserving faceuse orientation parity
 *  (SAME, OPPOSITE, OPPOSITE, SAME, ...)
 *  can't be used here -- that only applies to faceuses from the same shell.
 */
void
nmg_radial_join_eu(eu1, eu2)
struct edgeuse *eu1, *eu2;
{
	struct edgeuse	*original_eu1 = eu1;
	struct edgeuse	*nexteu;
	fastf_t		angle12;	/* Angle from eu1 to eu2 */
	fastf_t		angle1r;	/* Angle from eu1 to eu1's radial */
	fastf_t		angler2;	/* Angle from eu1's radial to eu2 */
	int	iteration1, iteration2;
	vect_t		xvec, yvec, zvec;
	fastf_t		abs1;
	fastf_t		abs2;
	fastf_t		absr;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu1->radial_p);
	NMG_CK_EDGEUSE(eu1->eumate_p);
	NMG_CK_EDGEUSE(eu2);
	NMG_CK_EDGEUSE(eu2->radial_p);
	NMG_CK_EDGEUSE(eu2->eumate_p);

	if( eu1->e_p == eu2->e_p )  return;

	if( eu1->vu_p->v_p == eu2->vu_p->v_p && 
	    eu1->eumate_p->vu_p->v_p == eu2->eumate_p->vu_p->v_p )  {
	    	/* Both edgeuses are oriented in the same direction */
	} else if( eu1->vu_p->v_p == eu2->eumate_p->vu_p->v_p &&
	    eu1->eumate_p->vu_p->v_p == eu2->vu_p->v_p )  {
	    	/* Edgeuses are oriented in opposite directions */
		rt_log("nmg_radial_join_eu() FLIP eu2\n");
	    	eu2 = eu2->eumate_p;
	} else {
		rt_bomb("nmg_radial_join_eu(): edgeuses don't share both vertices\n");
	}

	if( eu1->vu_p->v_p == eu1->eumate_p->vu_p->v_p )  rt_bomb("nmg_radial_join_eu(): 0 length edge\n");

	nmg_eu_2vecs_perp( xvec, yvec, zvec, original_eu1 );

	if (rt_g.NMG_debug & (DEBUG_MESH_EU|DEBUG_MESH) ) {
		rt_log("nmg_radial_join_eu(eu1=x%x, eu2=x%x) e1=x%x, e2=x%x\n",
			eu1, eu2,
			eu1->e_p, eu2->e_p);
		EUPRINT("\tJoining", eu1);
		EUPRINT("\t     to", eu2);
		rt_log( "Faces around eu1:\n" );
		nmg_pr_fu_around_eu_vecs( eu1, xvec, yvec, zvec );
		rt_log( "Faces around eu2:\n" );
		nmg_pr_fu_around_eu_vecs( eu2, xvec, yvec, zvec );
	}

	for ( iteration1=0; eu2 && iteration1 < 10000; iteration1++ ) {
		int	code = 0;
		struct edgeuse	*first_eu1 = eu1;
		/* Resume where we left off from last eu2 insertion */

		/* because faces are always created with counter-clockwise
		 * exterior loops and clockwise interior loops, radial
		 * edgeuses will never share the same vertex.  We thus make
		 * sure that eu2 is an edgeuse which might be radial to eu1
		 */
		if (eu2->vu_p->v_p == eu1->vu_p->v_p)
			eu2 = eu2->eumate_p;

		/* find a place to insert eu2 on eu1's edge */
		for ( iteration2=0; iteration2 < 10000; iteration2++ ) {
			angle12 = nmg_measure_2fu_angle(eu1, eu2);
			angle1r = nmg_measure_2fu_angle(eu1, eu1->radial_p);
			angler2 = nmg_measure_2fu_angle(eu1->radial_p->eumate_p, eu2);

			abs1 = nmg_measure_fu_angle( eu1, xvec, yvec, zvec );
			abs2 = nmg_measure_fu_angle( eu2, xvec, yvec, zvec );
			absr = nmg_measure_fu_angle( eu1->radial_p, xvec, yvec, zvec );

			if (rt_g.NMG_debug & (DEBUG_MESH_EU|DEBUG_MESH) )  {
				rt_log("  angle12=%g, angle1r=%g, angler2=%g\n",
					angle12*rt_radtodeg, angle1r*rt_radtodeg, angler2*rt_radtodeg );
				rt_log("  abs1=%g, abs2=%g, absr=%g\n",
					abs1*rt_radtodeg,
					abs2*rt_radtodeg,
					absr*rt_radtodeg );
			}

			/*
			 *  If abs1 < abs2 < absr
			 *  (taking into account 360 wrap),
			 *  then insert face here.
			 *  Special handling if abs1==abs2 or abs2==absr.
			 */
			code = nmg_is_angle_in_wedge( abs1, absr, abs2 );
			if (rt_g.NMG_debug & (DEBUG_MESH_EU|DEBUG_MESH) )
				rt_log("    code=%d %s\n", code, (code!=0)?"INSERT_HERE":"skip");
			if( code != 0 )  break;

			if( iteration2 > 9997 )  rt_g.NMG_debug |= DEBUG_MESH_EU;
			/* If eu1 is only one pair of edgeuses, done */
			if( eu1 == eu1->radial_p->eumate_p )  break;
			eu1 = eu1->radial_p->eumate_p;
			if( eu1 == first_eu1 )  {
				rt_bomb("nmg_radial_join_eu():  went full circle, no face insertion point.\n");
				break;
			}
		}
		if(iteration2 >= 10000)  {
			rt_log("angle12=%e, angle1r=%e\n", angle12, angle1r);
			rt_bomb("nmg_radial_join_eu: infinite loop (2)\n");
		}
		/*
		 *  Here, angle12 > angle1r.
		 *  XXX If angle12 == angle1r within a very tight tolerance,
		 *  then it's necessary to be sure that the face gets
		 *  interted on the proper side of eu1.
		 *  Really, these two faces should be fused together here
		 *  (or somewhere).
		 *  XXX What tolerance to use here?  (radians)
		 */
		if( code < 0 ) {
			rt_log("nmg_radial_join_eu: WARNING 2 faces should have been fused.\n");
		}

		/* find the next use of the edge eu2 is on.  If eu2 and it's
		 * mate are the last uses of the edge, there will be no next
		 * edgeuse to move. (Loop termination condition).
		 */
		nexteu = eu2->radial_p;
		if (nexteu == eu2->eumate_p)
			nexteu = (struct edgeuse *)NULL;

		if (rt_g.NMG_debug & DEBUG_MESH_EU)
			rt_log("joining eu1=x%x eu2=x%x with angle12=%g, angle1r=%g\n",
				eu1, eu2,
				angle12*rt_radtodeg, angle1r*rt_radtodeg);

		/*
		 *  Make eu2 radial to eu1.
		 *  This should insert eu2 between eu1 and eu1->radial_p.
		 */
		nmg_moveeu(eu1, eu2);

		if (rt_g.NMG_debug & DEBUG_MESH_EU)  {
			rt_log("Now, faces around original_eu1:\n");
			nmg_pr_fu_around_eu_vecs( original_eu1, xvec, yvec, zvec );
		}
		/* This will catch the errors, anyway */
		if( nmg_check_radial(eu1) )
			rt_bomb("nmg_radial_join_eu(): radial orientation ERROR 1\n");
		if( nmg_check_radial(eu2) )
			rt_bomb("nmg_radial_join_eu(): radial orientation ERROR 2\n");

		/* Proceed to the next source edgeuse */
		eu2 = nexteu;
	}
	if( iteration1 >= 10000 )  rt_bomb("nmg_radial_join_eu:  infinite loop (1)\n");
	if (rt_g.NMG_debug & DEBUG_MESH_EU)  rt_log("nmg_radial_join_eu: END\n");
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
			if (rt_g.NMG_debug & DEBUG_MESH)  {
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
					if (rt_g.NMG_debug & DEBUG_MESH) {
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
