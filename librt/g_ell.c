/*
 *			E L L G . C
 *
 * Function -
 *	Intersect a ray with a Generalized Ellipsoid
 *
 * Authors -
 *	Edwin O. Davisson	(Analysis)
 *	Michael John Muuss	(Programming)
 *
 * U. S. Army Ballistic Research Laboratory
 * March 27, 1984.
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "ray.h"
#include "db.h"

/*
 *  Algorithm;
 *  
 *  Given V, A, B, and C, there is a set of points on this ellipsoid
 *  
 *  { (x,y,z) | (x,y,z) is on ellipsoid defined by V, A, B, C }
 *  
 *  Through a series of Affine Transformations, this set will be
 *  transformed into a set of points on a unit sphere at the origin
 *  
 *  { (x',y',z') | (x',y',z') is on Sphere at origin }
 *  
 *  The transformation from X to X' is accomplished by:
 *  
 *  X' = S(R( X - V ))
 *  
 *  where R(X) =  ( A/(|A|) )
 *  		 (  B/(|B|)  ) . X
 *  		  ( C/(|C|) )
 *  
 *  and S(X) =	 (  1/|A|   0     0   )
 *  		(    0    1/|B|   0    ) . X
 *  		 (   0      0   1/|C| )
 *  
 *  To find the intersection of a line with the ellipsoid, consider
 *  the parametric line L:
 *  
 *  	L : { P(n) | P + t(n) . D }
 *  
 *  Call W the actual point of intersection between L and the ellipsoid.
 *  Let W' be the point of intersection between L' and the unit sphere.
 *  
 *  	L' : { P'(n) | P' + t(n) . D' }
 *  
 *  W = invR( invS( W' ) ) + V
 *  
 *  Where W' = k D' + P'.
 *  
 *  Let dp = D' dot P'
 *  Let dd = D' dot D'
 *  Let pp = P' dot P'
 *  
 *  and k = [ -dp +/- sqrt( dp*dp - dd * (pp - 1) ) ] / dd
 *  which is constant.
 *  
 *  Now, D' = S( R( D ) )
 *  and  P' = S( R( P - V ) )
 *  
 *  Substituting,
 *  
 *  W = V + invR( invS[ k *( S( R( D ) ) ) + S( R( P - V ) ) ] )
 *    = V + invR( ( k * R( D ) ) + R( P - V ) )
 *    = V + k * D + P - V
 *    = k * D + P
 *  
 *  Note that ``k'' is constant, and is the same in the formulations
 *  for both W and W'.
 *  
 *  NORMALS.  Given the point W on the ellipsoid, what is the vector
 *  normal to the tangent plane at that point?
 *  
 *  Map W onto the unit sphere, ie:  W' = S( R( W - V ) ).
 *  
 *  Plane on unit sphere at W' has a normal vector of the same value(!).
 *  
 *  The plane transforms back to the tangent plane at W, and this
 *  new plane (on the ellipsoid) has a normal vector of N, viz:
 *  
 *  N = inverse[ transpose(invR o invS) ] ( W' )
 *    = inverse[ transpose(invS) o transpose(invR) ] ( W' )
 *    = inverse[ inverse(S) o R ] ( W' )
 *    = invR o S ( W' )
 *    = invR( S( S( R( W - V ) ) ) )
 */

struct ell_specific {
	vect_t	ell_V;		/* Vector to center of ellipsoid */
	vect_t	ell_A;		/* Three axis vectors */
	vect_t	ell_B;
	vect_t	ell_C;
	mat_t	ell_SoR;	/* Scale(Rot(vect)) */
	mat_t	ell_SoS;	/* Scale(Scale(vect)) */
	vect_t	ell_invsq;	/* [ 1/(|A|**2), 1/(|B|**2), 1/(|C|**2) ] */
};

ellg_prep( sp, stp, mat )
struct solidrec *sp;
struct soltab *stp;
matp_t mat;
{
	register struct ell_specific *ell;
	double magsq_a, magsq_b, magsq_c;
	float f;

	/* Should do some validation here */
	/* A.B == 0, B.C == 0, A.C == 0 */
	/* |A| > 0, |B| > 0, |C| > 0 */

	GETSTRUCT( ell, ell_specific );
	stp->st_specific = (int *)ell;

	VMOVE( ell->ell_V, &sp->s_values[0] );
	VMOVE( ell->ell_A, &sp->s_values[3] );
	VMOVE( ell->ell_B, &sp->s_values[6] );
	VMOVE( ell->ell_C, &sp->s_values[9] );

	magsq_a = MAGSQ( ell->ell_A );
	magsq_b = MAGSQ( ell->ell_B );
	magsq_c = MAGSQ( ell->ell_C );

	VSET( ell->ell_invsq, 1.0/magsq_a, 1.0/magsq_b, 1.0/magsq_c );

	mat_zero( ell->ell_SoR );
	mat_zero( ell->ell_SoS );

	/* Uses 3x3 of the 4x4 matrix */
	ell->ell_SoS[ 0] = ell->ell_invsq[0];
	ell->ell_SoS[ 5] = ell->ell_invsq[1];
	ell->ell_SoS[10] = ell->ell_invsq[2];

	VSCALE( &ell->ell_SoR[0], ell->ell_A, ell->ell_invsq[0] );
	VSCALE( &ell->ell_SoR[4], ell->ell_B, ell->ell_invsq[1] );
	VSCALE( &ell->ell_SoR[8], ell->ell_C, ell->ell_invsq[2] );

	/* Compute bounding sphere */
	VMOVE( stp->st_center, ell->ell_V );
	f = magsq_a;
	if( magsq_b > f )
		f = magsq_b;
	if( magsq_c > f )
		f = magsq_c;
	stp->st_radsq = f;

	return(0);			/* OK */
}

ellg_print( stp )
register struct soltab *stp;
{
	register struct ell_specific *ell =
		(struct ell_specific *)stp->st_specific;

	VPRINT("V", ell->ell_V);
	VPRINT("A", ell->ell_A);
	VPRINT("B", ell->ell_B);
	VPRINT("C", ell->ell_C);
	VPRINT("1/magitude**2", ell->ell_invsq );
	mat_print("S o R", ell->ell_SoR );
	mat_print("S o S", ell->ell_SoS );
}

extern struct seg *HeadSeg;	/* Pointer to segment list */

ellg_shot( stp, rp )
struct soltab *stp;
register struct ray *rp;
{
	register struct ell_specific *ell =
		(struct ell_specific *)stp->st_specific;
	register struct seg *segp;
	static vect_t	dprime;		/* D' */
	static vect_t	pprime;		/* P' */
	static float	dp, dd, pp;	/* D' dot P', D' dot D', P' dot P' */
	static float	root;		/* root of radical */
	static float	k1, k2;		/* distance constants of solution */
	static vect_t	xlated;		/* translated vector */

	/* out, Mat, vect */
	MAT3XVEC( dprime, ell->ell_SoR, rp->r_dir );
	VSUB2( xlated, rp->r_pt, ell->ell_V );
	MAT3XVEC( pprime, ell->ell_SoR, xlated );

	dp = VDOT( dprime, pprime );
	dd = VDOT( dprime, dprime );
	pp = VDOT( pprime, pprime );

	root = dp*dp - dd * (pp-1);
	if( root < 0 )
		return(1);		/* No hit */
	root = sqrt(root);

	k1 = (-dp + root) / dd;
	k2 = (-dp - root) / dd;

	if( k1 > k2 )  {
		register float f;	/*  XXX  */
		f = k1;
		k1 = k2;
		k2 = f;
	}
	/*
	 * Now, k1 is entry point, and k2 is exit point
	 */
	GETSTRUCT(segp, seg);
	segp->seg_stp = stp;

	/* ASSERT that MAGNITUDE(rp->r_dir) == 1 */
	segp->seg_in.hit_dist = k1;
	segp->seg_out.hit_dist = k2;

	/*
	 *  For each point, if intersect comes "after" start of ray,
	 *  compute exact intersection point, and surface normal.
	 */
	segp->seg_flag = 0;
	if( k1 >= 0 )  {
		register float f;		/* XXX */
		segp->seg_flag |= SEG_IN;

		/* Intersection point */
		VCOMPOSE1( segp->seg_in.hit_point, rp->r_pt, k1, rp->r_dir );

		/* Normal at that point, pointing out */
		VSUB2( xlated, segp->seg_in.hit_point, ell->ell_V );
		VELMUL( segp->seg_in.hit_normal, xlated, ell->ell_invsq );
		f = 1.0 / MAGNITUDE(segp->seg_in.hit_normal );
		VSCALE( segp->seg_in.hit_normal, segp->seg_in.hit_normal, f);
	}
	if( k2 >= 0 )  {
		register float f;		/* XXX */
		segp->seg_flag |= SEG_OUT;

		VCOMPOSE1( segp->seg_out.hit_point, rp->r_pt, k2, rp->r_dir );

		VSUB2( xlated, segp->seg_out.hit_point, ell->ell_V );
		VELMUL( segp->seg_out.hit_normal, xlated, ell->ell_invsq );
		f = 1.0 / MAGNITUDE( segp->seg_out.hit_normal );
		VSCALE(segp->seg_out.hit_normal, segp->seg_out.hit_normal, f);
	}
	segp->seg_next = HeadSeg;
	HeadSeg = segp;
	return(0);			/* HIT */
}
