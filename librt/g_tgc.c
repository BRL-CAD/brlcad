/*
 *			G _ T G C . C
 *
 * Purpose -
 *	Intersect a ray with a Truncated General Cone.
 *
 * Method -
 *	TGC:  solve quartic equation of cone and line
 *
 * Authors -
 *	Edwin O. Davisson	(Analysis)
 *	Jeff Hanes		(Programming)
 *	Gary Moss		(Improvement)
 *	Mike Muuss		(Optimization)
 *	Peter F. Stiller	(Curvature)
 *	Phillip Dykstra		(Curvature)
 *	Bill Homer		(Vectorization)
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCStgc[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"
#include "./complex.h"
#include "./polyno.h"

RT_EXTERN(int rt_rec_prep, (struct soltab *stp, struct rt_db_internal *ip,
	struct rt_i *rtip));

static void	rt_tgc_rotate(), rt_tgc_shear();
static void	rt_tgc_scale();
void rt_pt_sort();

struct  tgc_specific {
	vect_t	tgc_V;		/*  Vector to center of base of TGC	*/
	fastf_t	tgc_sH;		/*  magnitude of sheared H vector	*/
	fastf_t	tgc_A;		/*  magnitude of A vector		*/
	fastf_t	tgc_B;		/*  magnitude of B vector		*/
	fastf_t	tgc_C;		/*  magnitude of C vector		*/
	fastf_t	tgc_D;		/*  magnitude of D vector		*/
	fastf_t	tgc_CdAm1;	/*  (C/A - 1)				*/
	fastf_t tgc_DdBm1;	/*  (D/B - 1)				*/
	fastf_t	tgc_AAdCC;	/*  (|A|**2)/(|C|**2)			*/
	fastf_t	tgc_BBdDD;	/*  (|B|**2)/(|D|**2)			*/
	vect_t	tgc_N;		/*  normal at 'top' of cone		*/
	mat_t	tgc_ScShR;	/*  Scale( Shear( Rot( vect )))		*/
	mat_t	tgc_invRtShSc;	/*  invRot( trnShear( Scale( vect )))	*/
	char	tgc_AD_CB;	/*  boolean:  A*D == C*B  */
};

#define VLARGE		1000000.0
#define	ALPHA(x,y,c,d)	( (x)*(x)*(c) + (y)*(y)*(d) )

/*
 *			R T _ T G C _ P R E P
 *
 *  Given the parameters (in vector form) of a truncated general cone,
 *  compute the constant terms and a transformation matrix needed for
 *  solving the intersection of a ray with the cone.
 *
 *  Also compute the return transformation for normals in the transformed
 *  space to the original space.  This NOT the inverse of the transformation
 *  matrix (if you really want to know why, talk to Ed Davisson).
 */
int
rt_tgc_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_tgc_internal	*tip;
	register struct tgc_specific *tgc;
	register fastf_t	f;
	LOCAL fastf_t	prod_ab, prod_cd;
	LOCAL fastf_t	magsq_a, magsq_b, magsq_c, magsq_d;
	LOCAL fastf_t	mag_h, mag_a, mag_b, mag_c, mag_d;
	LOCAL mat_t	Rot, Shr, Scl;
	LOCAL mat_t	iRot, tShr, iShr, iScl;
	LOCAL mat_t	tmp;
	LOCAL vect_t	nH;
	LOCAL vect_t	work;

	tip = (struct rt_tgc_internal *)ip->idb_ptr;
	RT_TGC_CK_MAGIC(tip);

	/*
	 *  For a fast way out, hand this solid off to the REC routine.
	 *  If it takes it, then there is nothing to do, otherwise
	 *  the solid is a TGC.
	 */
	if( rt_rec_prep( stp, ip, rtip ) == 0 )
		return(0);		/* OK */

	/* Validate that |H| > 0, compute |A| |B| |C| |D|		*/
	mag_h = sqrt( MAGSQ( tip->h ) );
	mag_a = sqrt( magsq_a = MAGSQ( tip->a ) );
	mag_b = sqrt( magsq_b = MAGSQ( tip->b ) );
	mag_c = sqrt( magsq_c = MAGSQ( tip->c ) );
	mag_d = sqrt( magsq_d = MAGSQ( tip->d ) );
	prod_ab = mag_a * mag_b;
	prod_cd = mag_c * mag_d;

	if( NEAR_ZERO( mag_h, RT_LEN_TOL ) ) {
		rt_log("tgc(%s):  zero length H vector\n", stp->st_name );
		return(1);		/* BAD */
	}

	/* Validate that figure is not two-dimensional			*/
	if( NEAR_ZERO( mag_a, RT_LEN_TOL ) &&
	     NEAR_ZERO( mag_c, RT_LEN_TOL ) ) {
		rt_log("tgc(%s):  vectors A, C zero length\n", stp->st_name );
		return (1);
	}
	if( NEAR_ZERO( mag_b, RT_LEN_TOL ) &&
	    NEAR_ZERO( mag_d, RT_LEN_TOL ) ) {
		rt_log("tgc(%s):  vectors B, D zero length\n", stp->st_name );
		return (1);
	}

	/* Validate that both ends are not degenerate */
	if( prod_ab <= SMALL )  {
		/* AB end is degenerate */
		if( prod_cd <= SMALL )  {
			rt_log("tgc(%s):  Both ends degenerate\n", stp->st_name);
			return(1);		/* BAD */
		}
		/* Exchange ends, so that in solids with one degenerate end,
		 * the CD end always is always the degenerate one
		 */
		VADD2( tip->v, tip->v, tip->h );
		VREVERSE( tip->h, tip->h );
#define VEXCHANGE( a, b, tmp )	{ VMOVE(tmp,a); VMOVE(a,b); VMOVE(b,tmp); }
		VEXCHANGE( tip->a, tip->c, work );
		VEXCHANGE( tip->b, tip->d, work );
		rt_log("NOTE: tgc(%s): degenerate end exchanged\n", stp->st_name);
	}

	/* Ascertain whether H lies in A-B plane 			*/
	VCROSS( work, tip->a, tip->b );
	f = VDOT( tip->h, work ) / ( prod_ab*mag_h );
	if ( NEAR_ZERO(f, RT_DOT_TOL) ) {
		rt_log("tgc(%s):  H lies in A-B plane\n",stp->st_name);
		return(1);		/* BAD */
	}

	if( prod_ab > SMALL )  {
		/* Validate that A.B == 0 */
		f = VDOT( tip->a, tip->b ) / prod_ab;
		if( ! NEAR_ZERO(f, RT_DOT_TOL) ) {
			rt_log("tgc(%s):  A not perpendicular to B, f=%g\n",
				stp->st_name, f);
			rt_log("tgc: dot=%g / a*b=%g\n",
				VDOT( tip->a, tip->b ),  prod_ab );
			return(1);		/* BAD */
		}
	}
	if( prod_cd > SMALL )  {
		/* Validate that C.D == 0 */
		f = VDOT( tip->c, tip->d ) / prod_cd;
		if( ! NEAR_ZERO(f, RT_DOT_TOL) ) {
			rt_log("tgc(%s):  C not perpendicular to D, f=%g\n",
				stp->st_name, f);
			rt_log("tgc: dot=%g / c*d=%g\n",
				VDOT( tip->c, tip->d ), prod_cd );
			return(1);		/* BAD */
		}
	}

	if( mag_a * mag_c > SMALL )  {
		/* Validate that  A || C */
		f = 1.0 - VDOT( tip->a, tip->c ) / (mag_a * mag_c);
		if( ! NEAR_ZERO(f, RT_DOT_TOL) ) {
			rt_log("tgc(%s):  A not parallel to C, f=%g\n",
				stp->st_name, f);
			return(1);		/* BAD */
		}
	}

	if( mag_b * mag_d > SMALL )  {
		/* Validate that  B || D, for parallel planes	*/
		f = 1.0 - VDOT( tip->b, tip->d ) / (mag_b * mag_d);
		if( ! NEAR_ZERO(f, RT_DOT_TOL) ) {
			rt_log("tgc(%s):  B not parallel to D, f=%g\n",
				stp->st_name, f);
			return(1);		/* BAD */
		}
	}

	/* solid is OK, compute constant terms, etc. */
	GETSTRUCT( tgc, tgc_specific );
	stp->st_specific = (genptr_t)tgc;

	VMOVE( tgc->tgc_V, tip->v );
	tgc->tgc_A = mag_a;
	tgc->tgc_B = mag_b;
	tgc->tgc_C = mag_c;
	tgc->tgc_D = mag_d;

	/* Part of computing ALPHA() */
	if( NEAR_ZERO(magsq_c, SMALL) )
		tgc->tgc_AAdCC = VLARGE;
	else
		tgc->tgc_AAdCC = magsq_a / magsq_c;
	if( NEAR_ZERO(magsq_d, SMALL) )
		tgc->tgc_BBdDD = VLARGE;
	else
		tgc->tgc_BBdDD = magsq_b / magsq_d;

	/*  If the eccentricities of the two ellipses are the same,
	 *  then the cone equation reduces to a much simpler quadratic
	 *  form.  Otherwise it is a (gah!) quartic equation.
	 */
	f = rt_reldiff( (tgc->tgc_A*tgc->tgc_D), (tgc->tgc_C*tgc->tgc_B) );
	tgc->tgc_AD_CB = (f < 0.0001);		/* A*D == C*B */
	rt_tgc_rotate( tip->a, tip->b, tip->h, Rot, iRot, tgc );
	MAT4X3VEC( nH, Rot, tip->h );
	tgc->tgc_sH = nH[Z];

	tgc->tgc_CdAm1 = tgc->tgc_C/tgc->tgc_A - 1.0;
	tgc->tgc_DdBm1 = tgc->tgc_D/tgc->tgc_B - 1.0;
	if( NEAR_ZERO( tgc->tgc_CdAm1, SMALL ) )
		tgc->tgc_CdAm1 = 0.0;
	if( NEAR_ZERO( tgc->tgc_DdBm1, SMALL ) )
		tgc->tgc_DdBm1 = 0.0;

	/*
	 *	Added iShr parameter to tgc_shear().
	 *	Changed inverse transformation of normal vectors of std.
	 *		solid intersection to include shear inverse
	 *		(tgc_invRtShSc).
	 *	Fold in scaling transformation into the transformation to std.
	 *		space from target space (tgc_ScShR).
	 */
	rt_tgc_shear( nH, Z, Shr, tShr, iShr );
	rt_tgc_scale( tgc->tgc_A, tgc->tgc_B, tgc->tgc_sH, Scl, iScl );

	mat_mul( tmp, Shr, Rot );
	mat_mul( tgc->tgc_ScShR, Scl, tmp );

	mat_mul( tmp, tShr, Scl );
	mat_mul( tgc->tgc_invRtShSc, iRot, tmp );

	/* Compute bounding sphere and RPP */
	{
		LOCAL fastf_t dx, dy, dz;	/* For bounding sphere */
		LOCAL vect_t temp;

		/* There are 8 corners to the bounding RPP */
		/* This may not be minimal, but does fully contain the TGC */
		VADD2( temp, tgc->tgc_V, tip->a );
		VADD2( work, temp, tip->b );
#define TGC_MM(v)	VMINMAX( stp->st_min, stp->st_max, v );
		TGC_MM( work );	/* V + A + B */
		VSUB2( work, temp, tip->b );
		TGC_MM( work );	/* V + A - B */

		VSUB2( temp, tgc->tgc_V, tip->a );
		VADD2( work, temp, tip->b );
		TGC_MM( work );	/* V - A + B */
		VSUB2( work, temp, tip->b );
		TGC_MM( work );	/* V - A - B */

		VADD3( temp, tgc->tgc_V, tip->h, tip->c );
		VADD2( work, temp, tip->d );
		TGC_MM( work );	/* V + H + C + D */
		VSUB2( work, temp, tip->d );
		TGC_MM( work );	/* V + H + C - D */

		VADD2( temp, tgc->tgc_V, tip->h );
		VSUB2( temp, temp, tip->c );
		VADD2( work, temp, tip->d );
		TGC_MM( work );	/* V + H - C + D */
		VSUB2( work, temp, tip->d );
		TGC_MM( work );	/* V + H - C - D */

		VSET( stp->st_center,
			(stp->st_max[X] + stp->st_min[X])/2,
			(stp->st_max[Y] + stp->st_min[Y])/2,
			(stp->st_max[Z] + stp->st_min[Z])/2 );

		dx = (stp->st_max[X] - stp->st_min[X])/2;
		f = dx;
		dy = (stp->st_max[Y] - stp->st_min[Y])/2;
		if( dy > f )  f = dy;
		dz = (stp->st_max[Z] - stp->st_min[Z])/2;
		if( dz > f )  f = dz;
		stp->st_aradius = f;
		stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);
	}
	return (0);
}


/*
 *			R T _ T G C _ R O T A T E
 *
 *  To rotate vectors  A  and  B  ( where  A  is perpendicular to  B )
 *  to the X and Y axes respectively, create a rotation matrix
 *
 *	    | A' |
 *	R = | B' |
 *	    | C' |
 *
 *  where  A',  B'  and  C'  are vectors such that
 *
 *	A' = A/|A|	B' = B/|B|	C' = C/|C|
 *
 *  where    C = H - ( H.A' )A' - ( H.B' )B'
 *
 *  The last operation ( Gram Schmidt method ) finds the component
 *  of the vector  H  perpendicular  A  and to  B.  This is, therefore
 *  the normal for the planar sections of the truncated cone.
 */
static void
rt_tgc_rotate( A, B, Hv, Rot, Inv, tgc )
vect_t		A, B, Hv;
mat_t		Rot, Inv;
struct tgc_specific	*tgc;
{
	LOCAL vect_t	uA, uB, uC;	/*  unit vectors		*/
	LOCAL fastf_t	mag_ha,		/*  magnitude of H in the	*/
			mag_hb;		/*    A and B directions	*/

	/* copy A and B, then 'unitize' the results			*/
	VMOVE( uA, A );
	VUNITIZE( uA );
	VMOVE( uB, B );
	VUNITIZE( uB );

	/*  Find component of H in the A direction			*/
	mag_ha = VDOT( Hv, uA );
	/*  Find component of H in the B direction			*/
	mag_hb = VDOT( Hv, uB );

	/*  Subtract the A and B components of H to find the component
	 *  perpendicular to both, then 'unitize' the result.
	 */
	VJOIN2( uC, Hv, -mag_ha, uA, -mag_hb, uB );
	VUNITIZE( uC );
	VMOVE( tgc->tgc_N, uC );

	mat_idn( Rot );
	mat_idn( Inv );

	Rot[0] = Inv[0] = uA[X];
	Rot[1] = Inv[4] = uA[Y];
	Rot[2] = Inv[8] = uA[Z];

	Rot[4] = Inv[1] = uB[X];
	Rot[5] = Inv[5] = uB[Y];
	Rot[6] = Inv[9] = uB[Z];

	Rot[8]  = Inv[2]  = uC[X];
	Rot[9]  = Inv[6]  = uC[Y];
	Rot[10] = Inv[10] = uC[Z];
}

/*
 *			R T _ T G C _ S H E A R
 *
 *  To shear the H vector to the Z axis, every point must be shifted
 *  in the X direction by  -(Hx/Hz)*z , and in the Y direction by
 *  -(Hy/Hz)*z .  This operation makes the equation for the standard
 *  cone much easier to work with.
 *
 *  NOTE:  This computes the TRANSPOSE of the shear matrix rather than
 *  the inverse.
 *
 * Begin changes GSM, EOD -- Added INVERSE (Inv) calculation.
 */
static void
rt_tgc_shear( vect, axis, Shr, Trn, Inv )
vect_t	vect;
int	axis;
mat_t	Shr, Trn, Inv;
{
	mat_idn( Shr );
	mat_idn( Trn );
	mat_idn( Inv );

	if ( axis == X ){
		Inv[4] = -(Shr[4] = Trn[1] = -vect[Y]/vect[X]);
		Inv[8] = -(Shr[8] = Trn[2] = -vect[Z]/vect[X]);
	} else if ( axis == Y ){
		Inv[1] = -(Shr[1] = Trn[4] = -vect[X]/vect[Y]);
		Inv[9] = -(Shr[9] = Trn[6] = -vect[Z]/vect[Y]);
	} else if ( axis == Z ){
		Inv[2] = -(Shr[2] = Trn[8] = -vect[X]/vect[Z]);
		Inv[6] = -(Shr[6] = Trn[9] = -vect[Y]/vect[Z]);
	}
}

/*
 *			R T _ T G C _ S C A L E
 */
static void
rt_tgc_scale( a, b, h, Scl, Inv )
fastf_t	a, b, h;
mat_t	Scl, Inv;
{
	mat_idn( Scl );
	mat_idn( Inv );
	Scl[0]  /= a;
	Scl[5]  /= b;
	Scl[10] /= h;
	Inv[0]  = a;
	Inv[5]  = b;
	Inv[10] = h;
	return;
}

/*
 *  			R T _ T G C _ P R I N T
 */
void
rt_tgc_print( stp )
register CONST struct soltab	*stp;
{
	register CONST struct tgc_specific	*tgc =
		(struct tgc_specific *)stp->st_specific;

	VPRINT( "V", tgc->tgc_V );
	rt_log( "mag sheared H = %f\n", tgc->tgc_sH );
	rt_log( "mag A = %f\n", tgc->tgc_A );
	rt_log( "mag B = %f\n", tgc->tgc_B );
	rt_log( "mag C = %f\n", tgc->tgc_C );
	rt_log( "mag D = %f\n", tgc->tgc_D );
	VPRINT( "Top normal", tgc->tgc_N );

	mat_print( "Sc o Sh o R", tgc->tgc_ScShR );
	mat_print( "invR o trnSh o Sc", tgc->tgc_invRtShSc );

	if( tgc->tgc_AD_CB )  {
		rt_log( "A*D == C*B.  Equal eccentricities gives quadratic equation.\n");
	} else {
		rt_log( "A*D != C*B.  Quartic equation.\n");
	}
	rt_log( "(C/A - 1) = %f\n", tgc->tgc_CdAm1 );
	rt_log( "(D/B - 1) = %f\n", tgc->tgc_DdBm1 );
	rt_log( "(|A|**2)/(|C|**2) = %f\n", tgc->tgc_AAdCC );
	rt_log( "(|B|**2)/(|D|**2) = %f\n", tgc->tgc_BBdDD );
}

/* hit_surfno is set to one of these */
#define	TGC_NORM_BODY	(1)		/* compute normal */
#define	TGC_NORM_TOP	(2)		/* copy tgc_N */
#define	TGC_NORM_BOT	(3)		/* copy reverse tgc_N */

/*
 *			R T _ T G C _ S H O T
 *
 *  Intersect a ray with a truncated general cone, where all constant
 *  terms have been computed by rt_tgc_prep().
 *
 *  NOTE:  All lines in this function are represented parametrically
 *  by a point,  P( Px, Py, Pz ) and a unit direction vector,
 *  D = iDx + jDy + kDz.  Any point on a line can be expressed
 *  by one variable 't', where
 *
 *        X = Dx*t + Px,
 *        Y = Dy*t + Py,
 *        Z = Dz*t + Pz.
 *
 *  First, convert the line to the coordinate system of a "stan-
 *  dard" cone.  This is a cone whose base lies in the X-Y plane,
 *  and whose H (now H') vector is lined up with the Z axis.  
 *
 *  Then find the equation of that line and the standard cone
 *  as an equation in 't'.  Solve the equation using a general
 *  polynomial root finder.  Use those values of 't' to compute
 *  the points of intersection in the original coordinate system.
 */
int
rt_tgc_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct tgc_specific	*tgc =
		(struct tgc_specific *)stp->st_specific;
	register struct seg	*segp;
	LOCAL vect_t		pprime;
	LOCAL vect_t		dprime;
	LOCAL vect_t		work;
	LOCAL fastf_t		k[4], pt[2];
	LOCAL fastf_t		t, b, zval, dir;
	LOCAL fastf_t		t_scale;
	LOCAL fastf_t		alf1, alf2;
	LOCAL int		npts;
	LOCAL int		intersect;
	LOCAL vect_t		cor_pprime;	/* corrected P prime */
	LOCAL fastf_t		cor_proj = 0;	/* corrected projected dist */
	LOCAL int		i;
	LOCAL poly		C;	/*  final equation	*/
	LOCAL poly		Xsqr, Ysqr;
	LOCAL poly		R, Rsqr;

	/* find rotated point and direction */
	MAT4X3VEC( dprime, tgc->tgc_ScShR, rp->r_dir );

	/*
	 *  A vector of unit length in model space (r_dir) changes length in
	 *  the special unit-tgc space.  This scale factor will restore
	 *  proper length after hit points are found.
	 */
	t_scale = 1/MAGNITUDE( dprime );
	VSCALE( dprime, dprime, t_scale );	/* VUNITIZE( dprime ); */

	if( NEAR_ZERO( dprime[Z], RT_PCOEF_TOL ) )  {
		dprime[Z] = 0.0;	/* prevent rootfinder heartburn */
	}

	VSUB2( work, rp->r_pt, tgc->tgc_V );
	MAT4X3VEC( pprime, tgc->tgc_ScShR, work );

	/* Translating ray origin along direction of ray to closest
	 * pt. to origin of solids coordinate system, new ray origin
	 * is 'cor_pprime'.
	 */
	cor_proj = -VDOT( pprime, dprime );
	VJOIN1( cor_pprime, pprime, cor_proj, dprime );

	/*
	 * The TGC is defined in "unit" space, so the parametric distance
	 * from one side of the TGC to the other is on the order of 2.
	 * Therefore, any vector/point coordinates that are very small
	 * here may be considered to be zero,
	 * since double precision only has 18 digits of significance.
	 * If these tiny values were left in, then as they get
	 * squared (below) they will cause difficulties.
	 */
	for( i=0; i<3; i++ )  {
		/* Direction cosines */
		if( NEAR_ZERO( dprime[i], 1e-10 ) )  dprime[i] = 0;
		/* Position in -1..+1 coordinates */
		if( NEAR_ZERO( cor_pprime[i], 1e-20 ) )  cor_pprime[i] = 0;
	}

	/*
	 *  Given a line and the parameters for a standard cone, finds
	 *  the roots of the equation for that cone and line.
	 *  Returns the number of real roots found.
	 * 
	 *  Given a line and the cone parameters, finds the equation
	 *  of the cone in terms of the variable 't'.
	 *
	 *  The equation for the cone is:
	 *
	 *      X**2 * Q**2  +  Y**2 * R**2  -  R**2 * Q**2 = 0
	 *
	 *  where	R = a + ((c - a)/|H'|)*Z 
	 *		Q = b + ((d - b)/|H'|)*Z
	 *
	 *  First, find X, Y, and Z in terms of 't' for this line, then
	 *  substitute them into the equation above.
	 *
	 *  Express each variable (X, Y, and Z) as a linear equation
	 *  in 'k', eg, (dprime[X] * k) + cor_pprime[X], and
	 *  substitute into the cone equation.
	 */
	Xsqr.dgr = 2;
	Xsqr.cf[0] = dprime[X] * dprime[X];
	Xsqr.cf[1] = 2.0 * dprime[X] * cor_pprime[X];
	Xsqr.cf[2] = cor_pprime[X] * cor_pprime[X];

	Ysqr.dgr = 2;
	Ysqr.cf[0] = dprime[Y] * dprime[Y];
	Ysqr.cf[1] = 2.0 * dprime[Y] * cor_pprime[Y];
	Ysqr.cf[2] = cor_pprime[Y] * cor_pprime[Y];

	R.dgr = 1;
	R.cf[0] = dprime[Z] * tgc->tgc_CdAm1;
	/* A vector is unitized (tgc->tgc_A == 1.0) */
	R.cf[1] = (cor_pprime[Z] * tgc->tgc_CdAm1) + 1.0;

	/* (void) polyMul( &R, &R, &Rsqr ); */
	Rsqr.dgr = 2;
	Rsqr.cf[0] = R.cf[0] * R.cf[0];
	Rsqr.cf[1] = R.cf[0] * R.cf[1] * 2;
	Rsqr.cf[2] = R.cf[1] * R.cf[1];

	/*
	 *  If the eccentricities of the two ellipses are the same,
	 *  then the cone equation reduces to a much simpler quadratic
	 *  form.  Otherwise it is a (gah!) quartic equation.
	 */
	if ( tgc->tgc_AD_CB ){
		FAST fastf_t roots;

		/*
		 *  (void) polyAdd( &Xsqr, &Ysqr, &sum );
		 *  (void) polySub( &sum, &Rsqr, &C );
		 */
		C.dgr = 2;
		C.cf[0] = Xsqr.cf[0] + Ysqr.cf[0] - Rsqr.cf[0];
		C.cf[1] = Xsqr.cf[1] + Ysqr.cf[1] - Rsqr.cf[1];
		C.cf[2] = Xsqr.cf[2] + Ysqr.cf[2] - Rsqr.cf[2];

		/* Find the real roots the easy way.  C.dgr==2 */
		if( (roots = C.cf[1]*C.cf[1] - 4 * C.cf[0] * C.cf[2]) < 0 ) {
			npts = 0;	/* no real roots */
		} else {
			register fastf_t	f;
			roots = sqrt(roots);
			k[0] = (roots - C.cf[1]) * (f = 0.5 / C.cf[0]);
			k[1] = (roots + C.cf[1]) * -f;
			npts = 2;
		}
	} else {
		LOCAL poly	Q, Qsqr;
		LOCAL complex	val[MAXP];	/* roots of final equation */
		register int	l;
		register int nroots;

		Q.dgr = 1;
		Q.cf[0] = dprime[Z] * tgc->tgc_DdBm1;
		/* B vector is unitized (tgc->tgc_B == 1.0) */
		Q.cf[1] = (cor_pprime[Z] * tgc->tgc_DdBm1) + 1.0;

		/* (void) polyMul( &Q, &Q, &Qsqr ); */
		Qsqr.dgr = 2;
		Qsqr.cf[0] = Q.cf[0] * Q.cf[0];
		Qsqr.cf[1] = Q.cf[0] * Q.cf[1] * 2;
		Qsqr.cf[2] = Q.cf[1] * Q.cf[1];

		/*
		 * (void) polyMul( &Qsqr, &Xsqr, &T1 );
		 * (void) polyMul( &Rsqr, &Ysqr, &T2 );
		 * (void) polyMul( &Rsqr, &Qsqr, &T3 );
		 * (void) polyAdd( &T1, &T2, &sum );
		 * (void) polySub( &sum, &T3, &C );
		 */
		C.dgr = 4;
		C.cf[0] = Qsqr.cf[0] * Xsqr.cf[0] +
			  Rsqr.cf[0] * Ysqr.cf[0] -
			  (Rsqr.cf[0] * Qsqr.cf[0]);
		C.cf[1] = Qsqr.cf[0] * Xsqr.cf[1] + Qsqr.cf[1] * Xsqr.cf[0] +
			  Rsqr.cf[0] * Ysqr.cf[1] + Rsqr.cf[1] * Ysqr.cf[0] -
			  (Rsqr.cf[0] * Qsqr.cf[1] + Rsqr.cf[1] * Qsqr.cf[0]);
		C.cf[2] = Qsqr.cf[0] * Xsqr.cf[2] + Qsqr.cf[1] * Xsqr.cf[1] +
				 Qsqr.cf[2] * Xsqr.cf[0] +
			  Rsqr.cf[0] * Ysqr.cf[2] + Rsqr.cf[1] * Ysqr.cf[1] +
				 Rsqr.cf[2] * Ysqr.cf[0] -
			  (Rsqr.cf[0] * Qsqr.cf[2] + Rsqr.cf[1] * Qsqr.cf[1] +
				 Rsqr.cf[2] * Qsqr.cf[0]);
		C.cf[3] = Qsqr.cf[1] * Xsqr.cf[2] + Qsqr.cf[2] * Xsqr.cf[1] +
			  Rsqr.cf[1] * Ysqr.cf[2] + Rsqr.cf[2] * Ysqr.cf[1] -
			  (Rsqr.cf[1] * Qsqr.cf[2] + Rsqr.cf[2] * Qsqr.cf[1]);
		C.cf[4] = Qsqr.cf[2] * Xsqr.cf[2] +
			  Rsqr.cf[2] * Ysqr.cf[2] -
			  (Rsqr.cf[2] * Qsqr.cf[2]);

		/*  The equation is 4th order, so we expect 0 to 4 roots */
		nroots = polyRoots( &C , val );

		/*  Only real roots indicate an intersection in real space.
		 *
		 *  Look at each root returned; if the imaginary part is zero
		 *  or sufficiently close, then use the real part as one value
		 *  of 't' for the intersections
		 */
		for ( l=0, npts=0; l < nroots; l++ ){
			if ( NEAR_ZERO( val[l].im, 1e-10 ) )
				k[npts++] = val[l].re;
		}
		/* Here, 'npts' is number of points being returned */
		if ( npts != 0 && npts != 2 && npts != 4 ){
			rt_log("tgc:  reduced %d to %d roots\n",nroots,npts);
			rt_pr_roots( stp->st_name, val, nroots );
		}
	}

	/*
	 * Reverse above translation by adding distance to all 'k' values.
	 */
	for( i = 0; i < npts; ++i )  {
		k[i] += cor_proj;
	}

	if ( npts != 0 && npts != 2 && npts != 4 ){
		rt_log("tgc(%s):  %d intersects != {0,2,4}\n",
			stp->st_name, npts );
		return(0);			/* No hit */
	}

	/* Sort Most distant to least distant: rt_pt_sort( k, npts ) */
	{
		register fastf_t	u;
		register short		lim, n;

		for( lim = npts-1; lim > 0; lim-- )  {
			for( n = 0; n < lim; n++ )  {
				if( (u=k[n]) < k[n+1] )  {
					/* bubble larger towards [0] */
					k[n] = k[n+1];
					k[n+1] = u;
				}
			}
		}
	}
	/* Now, k[0] > k[npts-1] */

	/* General Cone may have 4 intersections, but	*
	 * Truncated Cone may only have 2.		*/

#define OUT		0
#define	IN		1

	/*		Truncation Procedure
	 *
	 *  Determine whether any of the intersections found are
	 *  between the planes truncating the cone.
	 */
	intersect = 0;
	for ( i=0; i < npts; i++ ){
		zval = k[i]*dprime[Z] + pprime[Z];
		/* Height vector is unitized (tgc->tgc_sH == 1.0) */
		if ( zval < 1.0 && zval > 0.0 ){
			if ( ++intersect == 2 )  {
				pt[IN] = k[i];
			}  else  {
				pt[OUT] = k[i];
			}
		}
	}
	if ( intersect == 2 ){
		/*  If two between-plane intersections exist, they are
		 *  the hit points for the ray.
		 */
		RT_GET_SEG( segp, ap->a_resource );
		segp->seg_stp = stp;

		segp->seg_in.hit_dist = pt[IN] * t_scale;
		segp->seg_in.hit_surfno = TGC_NORM_BODY;	/* compute N */
		VJOIN1( segp->seg_in.hit_vpriv, pprime, pt[IN], dprime );

		segp->seg_out.hit_dist = pt[OUT] * t_scale;
		segp->seg_out.hit_surfno = TGC_NORM_BODY;	/* compute N */
		VJOIN1( segp->seg_out.hit_vpriv, pprime, pt[OUT], dprime );

		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
		return(2);
	}
	if ( intersect == 1 )  {
		int	nflag;
		/*
		 *  If only one between-plane intersection exists (pt[OUT]),
		 *  then the other intersection must be on
		 *  one of the planar surfaces (pt[IN]).
		 *
		 *  Find which surface it lies on by calculating the 
		 *  X and Y values of the line as it intersects each
		 *  plane (in the standard coordinate system), and test
		 *  whether this lies within the governing ellipse.
		 */
		if( dprime[Z] == 0.0 )  {
			rt_log("tgc: dprime[Z] = 0!\n" );
			return(0);
		}
		b = ( -pprime[Z] )/dprime[Z];
		/*  Height vector is unitized (tgc->tgc_sH == 1.0) */
		t = ( 1.0 - pprime[Z] )/dprime[Z];

		VJOIN1( work, pprime, b, dprime );
		/* A and B vectors are unitized (tgc->tgc_A == _B == 1.0) */
		/* alf1 = ALPHA(work[X], work[Y], 1.0, 1.0 ) */
		alf1 = work[X]*work[X] + work[Y]*work[Y];

		VJOIN1( work, pprime, t, dprime );
		/* Must scale C and D vectors */
		alf2 = ALPHA(work[X], work[Y], tgc->tgc_AAdCC,tgc->tgc_BBdDD);

		if ( alf1 <= 1.0 ){
			pt[IN] = b;
			nflag = TGC_NORM_BOT; /* copy reverse normal */
		} else if ( alf2 <= 1.0 ){
			pt[IN] = t;
			nflag = TGC_NORM_TOP;	/* copy normal */
		} else {
			/* intersection apparently invalid  */
			rt_log("tgc(%s):  only 1 intersect\n", stp->st_name);
			return(0);
		}

		RT_GET_SEG( segp, ap->a_resource );
		segp->seg_stp = stp;
		/* pt[OUT] on skin, pt[IN] on end */
		if ( pt[OUT] >= pt[IN] )  {
			segp->seg_in.hit_dist = pt[IN] * t_scale;
			segp->seg_in.hit_surfno = nflag;

			segp->seg_out.hit_dist = pt[OUT] * t_scale;
			segp->seg_out.hit_surfno = TGC_NORM_BODY;	/* compute N */
			/* transform-space vector needed for normal */
			VJOIN1( segp->seg_out.hit_vpriv, pprime, pt[OUT], dprime );
		} else {
			segp->seg_in.hit_dist = pt[OUT] * t_scale;
			/* transform-space vector needed for normal */
			segp->seg_in.hit_surfno = TGC_NORM_BODY;	/* compute N */
			VJOIN1( segp->seg_in.hit_vpriv, pprime, pt[OUT], dprime );

			segp->seg_out.hit_dist = pt[IN] * t_scale;
			segp->seg_out.hit_surfno = nflag;
		}
		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
		return(2);
	}

	/*  If all conic interections lie outside the plane,
	 *  then check to see whether there are two planar
	 *  intersections inside the governing ellipses.
	 *
	 *  But first, if the direction is parallel (or nearly
	 *  so) to the planes, it (obviously) won't intersect
	 *  either of them.
	 */
	if( dprime[Z] == 0.0 )
		return(0);

	dir = VDOT( tgc->tgc_N, rp->r_dir );	/* direc */
	if ( NEAR_ZERO( dir, RT_DOT_TOL ) )
		return(0);

	b = ( -pprime[Z] )/dprime[Z];
	/* Height vector is unitized (tgc->tgc_sH == 1.0) */
	t = ( 1.0 - pprime[Z] )/dprime[Z];

	VJOIN1( work, pprime, b, dprime );
	/* A and B vectors are unitized (tgc->tgc_A == _B == 1.0) */
	/* alpf = ALPHA(work[0], work[1], 1.0, 1.0 ) */
	alf1 = work[X]*work[X] + work[Y]*work[Y];

	VJOIN1( work, pprime, t, dprime );
	/* Must scale C and D vectors. */
	alf2 = ALPHA(work[X], work[Y], tgc->tgc_AAdCC,tgc->tgc_BBdDD);

	/*  It should not be possible for one planar intersection
	 *  to be outside its ellipse while the other is inside ...
	 *  but I wouldn't take any chances.
	 */
	if ( alf1 > 1.0 || alf2 > 1.0 )
		return(0);

	RT_GET_SEG( segp, ap->a_resource );
	segp->seg_stp = stp;

	/*  Use the dot product (found earlier) of the plane
	 *  normal with the direction vector to determine the
	 *  orientation of the intersections.
	 */
	if ( dir > 0.0 ){
		segp->seg_in.hit_dist = b * t_scale;
		segp->seg_in.hit_surfno = TGC_NORM_BOT;	/* reverse normal */

		segp->seg_out.hit_dist = t * t_scale;
		segp->seg_out.hit_surfno = TGC_NORM_TOP;	/* normal */
	} else {
		segp->seg_in.hit_dist = t * t_scale;
		segp->seg_in.hit_surfno = TGC_NORM_TOP;	/* normal */

		segp->seg_out.hit_dist = b * t_scale;
		segp->seg_out.hit_surfno = TGC_NORM_BOT;	/* reverse normal */
	}
	RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	return(2);
}

#define RT_TGC_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ T G C _ V S H O T
 *
 *  The Homer vectorized version.
 */
void
rt_tgc_vshot( stp, rp, segp, n, ap )
struct soltab		*stp[];
register struct xray	*rp[];
struct  seg            segp[]; /* array of segs (results returned) */
int                         n; /* Number of ray/object pairs */
struct application	*ap;
{
	register struct tgc_specific	*tgc;
	register int		ix;
	LOCAL vect_t		pprime;
	LOCAL vect_t		dprime;
	LOCAL vect_t		work;
	LOCAL fastf_t		k[4], pt[2];
	LOCAL fastf_t		t, b, zval, dir;
	LOCAL fastf_t		t_scale = 0;
	LOCAL fastf_t		alf1, alf2;
	LOCAL int		npts;
	LOCAL int		intersect;
	LOCAL vect_t		cor_pprime;	/* corrected P prime */
	LOCAL fastf_t		cor_proj = 0;	/* corrected projected dist */
	LOCAL int		i;
	LOCAL poly		*C;	/*  final equation	*/
	LOCAL poly		Xsqr, Ysqr;
	LOCAL poly		R, Rsqr;

        /* Allocate space for polys and roots */
        C = (poly *)rt_malloc(n * sizeof(poly), "tor poly");
 
        /* Initialize seg_stp to assume hit (zero will then flag miss) */
#       include "noalias.h"
        for(ix = 0; ix < n; ix++) segp[ix].seg_stp = stp[ix];

    /* for each ray/cone pair */
#   include "noalias.h"
    for(ix = 0; ix < n; ix++) {

#if !CRAY       /* XXX currently prevents vectorization on cray */
	if (segp[ix].seg_stp == 0) continue; /* == 0 signals skip ray */
#endif

	tgc = (struct tgc_specific *)stp[ix]->st_specific;

	/* find rotated point and direction */
	MAT4X3VEC( dprime, tgc->tgc_ScShR, rp[ix]->r_dir );

	/*
	 *  A vector of unit length in model space (r_dir) changes length in
	 *  the special unit-tgc space.  This scale factor will restore
	 *  proper length after hit points are found.
	 */
	t_scale = 1/MAGNITUDE( dprime );
	VSCALE( dprime, dprime, t_scale );	/* VUNITIZE( dprime ); */

	if( NEAR_ZERO( dprime[Z], RT_PCOEF_TOL ) )
		dprime[Z] = 0.0;	/* prevent rootfinder heartburn */

	/* Use segp[ix].seg_in.hit_normal as tmp to hold dprime */
	VMOVE( segp[ix].seg_in.hit_normal, dprime );
 
	VSUB2( work, rp[ix]->r_pt, tgc->tgc_V );
	MAT4X3VEC( pprime, tgc->tgc_ScShR, work );

	/* Use segp[ix].seg_out.hit_normal as tmp to hold pprime */
	VMOVE( segp[ix].seg_out.hit_normal, pprime );

	/* Translating ray origin along direction of ray to closest
	 * pt. to origin of solids coordinate system, new ray origin
	 * is 'cor_pprime'.
	 */
	cor_proj = VDOT( pprime, dprime );
	VSCALE( cor_pprime, dprime, cor_proj );
	VSUB2( cor_pprime, pprime, cor_pprime );

	/*
	 *  Given a line and the parameters for a standard cone, finds
	 *  the roots of the equation for that cone and line.
	 *  Returns the number of real roots found.
	 * 
	 *  Given a line and the cone parameters, finds the equation
	 *  of the cone in terms of the variable 't'.
	 *
	 *  The equation for the cone is:
	 *
	 *      X**2 * Q**2  +  Y**2 * R**2  -  R**2 * Q**2 = 0
	 *
	 *  where	R = a + ((c - a)/|H'|)*Z 
	 *		Q = b + ((d - b)/|H'|)*Z
	 *
	 *  First, find X, Y, and Z in terms of 't' for this line, then
	 *  substitute them into the equation above.
	 *
	 *  Express each variable (X, Y, and Z) as a linear equation
	 *  in 'k', eg, (dprime[X] * k) + cor_pprime[X], and
	 *  substitute into the cone equation.
	 */
	Xsqr.dgr = 2;
	Xsqr.cf[0] = dprime[X] * dprime[X];
	Xsqr.cf[1] = 2.0 * dprime[X] * cor_pprime[X];
	Xsqr.cf[2] = cor_pprime[X] * cor_pprime[X];

	Ysqr.dgr = 2;
	Ysqr.cf[0] = dprime[Y] * dprime[Y];
	Ysqr.cf[1] = 2.0 * dprime[Y] * cor_pprime[Y];
	Ysqr.cf[2] = cor_pprime[Y] * cor_pprime[Y];

	R.dgr = 1;
	R.cf[0] = dprime[Z] * tgc->tgc_CdAm1;
	/* A vector is unitized (tgc->tgc_A == 1.0) */
	R.cf[1] = (cor_pprime[Z] * tgc->tgc_CdAm1) + 1.0;

	/* (void) polyMul( &R, &R, &Rsqr ); inline expands to: */
                Rsqr.dgr = 2;
                Rsqr.cf[0] = R.cf[0] * R.cf[0];
                Rsqr.cf[1] = R.cf[0] * R.cf[1] * 2;
                Rsqr.cf[2] = R.cf[1] * R.cf[1];

	/*
	 *  If the eccentricities of the two ellipses are the same,
	 *  then the cone equation reduces to a much simpler quadratic
	 *  form.  Otherwise it is a (gah!) quartic equation.
	 */
	if ( tgc->tgc_AD_CB ){
		/* (void) polyAdd( &Xsqr, &Ysqr, &sum ); and */
		/* (void) polySub( &sum, &Rsqr, &C ); inline expand to */
		C[ix].dgr = 2;
		C[ix].cf[0] = Xsqr.cf[0] + Ysqr.cf[0] - Rsqr.cf[0];
		C[ix].cf[1] = Xsqr.cf[1] + Ysqr.cf[1] - Rsqr.cf[1];
		C[ix].cf[2] = Xsqr.cf[2] + Ysqr.cf[2] - Rsqr.cf[2];
	} else {
		LOCAL poly	Q, Qsqr;

		Q.dgr = 1;
		Q.cf[0] = dprime[Z] * tgc->tgc_DdBm1;
		/* B vector is unitized (tgc->tgc_B == 1.0) */
		Q.cf[1] = (cor_pprime[Z] * tgc->tgc_DdBm1) + 1.0;

		/* (void) polyMul( &Q, &Q, &Qsqr ); inline expands to */
			Qsqr.dgr = 2;
			Qsqr.cf[0] = Q.cf[0] * Q.cf[0];
			Qsqr.cf[1] = Q.cf[0] * Q.cf[1] * 2;
			Qsqr.cf[2] = Q.cf[1] * Q.cf[1];

		/* (void) polyMul( &Qsqr, &Xsqr, &T1 ); inline expands to */
			C[ix].dgr = 4;
			C[ix].cf[0] = Qsqr.cf[0] * Xsqr.cf[0];
			C[ix].cf[1] = Qsqr.cf[0] * Xsqr.cf[1] +
					 Qsqr.cf[1] * Xsqr.cf[0];
			C[ix].cf[2] = Qsqr.cf[0] * Xsqr.cf[2] +
					 Qsqr.cf[1] * Xsqr.cf[1] +
					 Qsqr.cf[2] * Xsqr.cf[0];
			C[ix].cf[3] = Qsqr.cf[1] * Xsqr.cf[2] +
					 Qsqr.cf[2] * Xsqr.cf[1];
			C[ix].cf[4] = Qsqr.cf[2] * Xsqr.cf[2];

		/* (void) polyMul( &Rsqr, &Ysqr, &T2 ); and */
		/* (void) polyAdd( &T1, &T2, &sum ); inline expand to */
			C[ix].cf[0] += Rsqr.cf[0] * Ysqr.cf[0];
			C[ix].cf[1] += Rsqr.cf[0] * Ysqr.cf[1] +
					 Rsqr.cf[1] * Ysqr.cf[0];
			C[ix].cf[2] += Rsqr.cf[0] * Ysqr.cf[2] +
					 Rsqr.cf[1] * Ysqr.cf[1] +
					 Rsqr.cf[2] * Ysqr.cf[0];
			C[ix].cf[3] += Rsqr.cf[1] * Ysqr.cf[2] +
					 Rsqr.cf[2] * Ysqr.cf[1];
			C[ix].cf[4] += Rsqr.cf[2] * Ysqr.cf[2];

		/* (void) polyMul( &Rsqr, &Qsqr, &T3 ); and */
		/* (void) polySub( &sum, &T3, &C ); inline expand to */
			C[ix].cf[0] -= Rsqr.cf[0] * Qsqr.cf[0];
			C[ix].cf[1] -= Rsqr.cf[0] * Qsqr.cf[1] +
					 Rsqr.cf[1] * Qsqr.cf[0];
			C[ix].cf[2] -= Rsqr.cf[0] * Qsqr.cf[2] +
					 Rsqr.cf[1] * Qsqr.cf[1] +
					 Rsqr.cf[2] * Qsqr.cf[0];
			C[ix].cf[3] -= Rsqr.cf[1] * Qsqr.cf[2] +
					 Rsqr.cf[2] * Qsqr.cf[1];
			C[ix].cf[4] -= Rsqr.cf[2] * Qsqr.cf[2];
		}

	}

    /* It seems impractical to try to vectorize finding and sorting roots. */
    for(ix = 0; ix < n; ix++){
	if (segp[ix].seg_stp == 0) continue; /* == 0 signals skip ray */

	/* Again, check for the equal eccentricities case. */
	if ( C[ix].dgr == 2 ){
		FAST fastf_t roots;

		/* Find the real roots the easy way. */
		if( (roots = C[ix].cf[1]*C[ix].cf[1]-4*C[ix].cf[0]*C[ix].cf[2]
		    ) < 0 ) {
			npts = 0;	/* no real roots */
		} else {
			roots = sqrt(roots);
			k[0] = (roots - C[ix].cf[1]) * 0.5 / C[ix].cf[0];
			k[1] = (roots + C[ix].cf[1]) * (-0.5) / C[ix].cf[0];
			npts = 2;
		}
	} else {
		LOCAL complex	val[MAXP];	/* roots of final equation */
		register int	l;
		register int nroots;

		/*  The equation is 4th order, so we expect 0 to 4 roots */
		nroots = polyRoots( &C[ix] , val );

		/*  Only real roots indicate an intersection in real space.
		 *
		 *  Look at each root returned; if the imaginary part is zero
		 *  or sufficiently close, then use the real part as one value
		 *  of 't' for the intersections
		 */
		for ( l=0, npts=0; l < nroots; l++ ){
			if ( NEAR_ZERO( val[l].im, 0.0001 ) )
				k[npts++] = val[l].re;
		}
		/* Here, 'npts' is number of points being returned */
		if ( npts != 0 && npts != 2 && npts != 4 ){
			rt_log("tgc:  reduced %d to %d roots\n",nroots,npts);
			rt_pr_roots( "tgc", val, nroots );
		}
	}

	/*
	 * Reverse above translation by adding distance to all 'k' values.
	 */
	for( i = 0; i < npts; ++i )
		k[i] -= cor_proj;

	if ( npts != 0 && npts != 2 && npts != 4 ){
		rt_log("tgc(%s):  %d intersects != {0,2,4}\n",
			stp[ix]->st_name, npts );
		RT_TGC_SEG_MISS(segp[ix]);		/* No hit	*/
		continue;
	}

	/* Most distant to least distant	*/
	rt_pt_sort( k, npts );

	/* Now, k[0] > k[npts-1] */

	/* General Cone may have 4 intersections, but	*
	 * Truncated Cone may only have 2.		*/

#define OUT		0
#define	IN		1

	/*		Truncation Procedure
	 *
	 *  Determine whether any of the intersections found are
	 *  between the planes truncating the cone.
	 */
	intersect = 0;
	tgc = (struct tgc_specific *)stp[ix]->st_specific;
	for ( i=0; i < npts; i++ ){
		/* segp[ix].seg_in.hit_normal holds dprime */
		/* segp[ix].seg_out.hit_normal holds pprime */
		zval = k[i]*segp[ix].seg_in.hit_normal[Z] +
			segp[ix].seg_out.hit_normal[Z];
		/* Height vector is unitized (tgc->tgc_sH == 1.0) */
		if ( zval < 1.0 && zval > 0.0 ){
			if ( ++intersect == 2 )  {
				pt[IN] = k[i];
			}  else
				pt[OUT] = k[i];
		}
	}
	/* Reuse C to hold values of intersect and k. */
	C[ix].dgr = intersect;
	C[ix].cf[OUT] = pt[OUT];
	C[ix].cf[IN]  = pt[IN];
    }

    /* for each ray/cone pair */
#   include "noalias.h"
    for(ix = 0; ix < n; ix++) {
	if (segp[ix].seg_stp == 0) continue; /* Skip */

	tgc = (struct tgc_specific *)stp[ix]->st_specific;
	intersect = C[ix].dgr;
	pt[OUT] = C[ix].cf[OUT];
	pt[IN]  = C[ix].cf[IN];
	/* segp[ix].seg_out.hit_normal holds pprime */
	VMOVE( pprime, segp[ix].seg_out.hit_normal );
	/* segp[ix].seg_in.hit_normal holds dprime */
	VMOVE( dprime, segp[ix].seg_in.hit_normal );

	if ( intersect == 2 ){
		/*  If two between-plane intersections exist, they are
		 *  the hit points for the ray.
		 */
		segp[ix].seg_in.hit_dist = pt[IN] * t_scale;
		segp[ix].seg_in.hit_surfno = TGC_NORM_BODY;	/* compute N */
		VJOIN1( segp[ix].seg_in.hit_vpriv, pprime, pt[IN], dprime );

		segp[ix].seg_out.hit_dist = pt[OUT] * t_scale;
		segp[ix].seg_out.hit_surfno = TGC_NORM_BODY;	/* compute N */
		VJOIN1( segp[ix].seg_out.hit_vpriv, pprime, pt[OUT], dprime );
	} else if ( intersect == 1 ) {
		int	nflag;
		/*
		 *  If only one between-plane intersection exists (pt[OUT]),
		 *  then the other intersection must be on
		 *  one of the planar surfaces (pt[IN]).
		 *
		 *  Find which surface it lies on by calculating the 
		 *  X and Y values of the line as it intersects each
		 *  plane (in the standard coordinate system), and test
		 *  whether this lies within the governing ellipse.
		 */
		if( dprime[Z] == 0.0 )  {
#if 0
			rt_log("tgc: dprime[Z] = 0!\n" );
#endif
			RT_TGC_SEG_MISS(segp[ix]);
			continue;
		}
		b = ( -pprime[Z] )/dprime[Z];
		/*  Height vector is unitized (tgc->tgc_sH == 1.0) */
		t = ( 1.0 - pprime[Z] )/dprime[Z];

		VJOIN1( work, pprime, b, dprime );
		/* A and B vectors are unitized (tgc->tgc_A == _B == 1.0) */
		/* alf1 = ALPHA(work[X], work[Y], 1.0, 1.0 ) */
		alf1 = work[X]*work[X] + work[Y]*work[Y];

		VJOIN1( work, pprime, t, dprime );
		/* Must scale C and D vectors */
		alf2 = ALPHA(work[X], work[Y], tgc->tgc_AAdCC,tgc->tgc_BBdDD);

		if ( alf1 <= 1.0 ){
			pt[IN] = b;
			nflag = TGC_NORM_BOT; /* copy reverse normal */
		} else if ( alf2 <= 1.0 ){
			pt[IN] = t;
			nflag = TGC_NORM_TOP;	/* copy normal */
		} else {
			/* intersection apparently invalid  */
#if 0
			rt_log("tgc(%s):  only 1 intersect\n", stp[ix]->st_name);
#endif
			RT_TGC_SEG_MISS(segp[ix]);
			continue;
		}

		/* pt[OUT] on skin, pt[IN] on end */
		if ( pt[OUT] >= pt[IN] )  {
			segp[ix].seg_in.hit_dist = pt[IN] * t_scale;
			segp[ix].seg_in.hit_surfno = nflag;

			segp[ix].seg_out.hit_dist = pt[OUT] * t_scale;
			segp[ix].seg_out.hit_surfno = TGC_NORM_BODY;	/* compute N */
			/* transform-space vector needed for normal */
			VJOIN1( segp[ix].seg_out.hit_vpriv, pprime, pt[OUT], dprime );
		} else {
			segp[ix].seg_in.hit_dist = pt[OUT] * t_scale;
			/* transform-space vector needed for normal */
			segp[ix].seg_in.hit_surfno = TGC_NORM_BODY;	/* compute N */
			VJOIN1( segp[ix].seg_in.hit_vpriv, pprime, pt[OUT], dprime );

			segp[ix].seg_out.hit_dist = pt[IN] * t_scale;
			segp[ix].seg_out.hit_surfno = nflag;
		}
	} else {

	/*  If all conic interections lie outside the plane,
	 *  then check to see whether there are two planar
	 *  intersections inside the governing ellipses.
	 *
	 *  But first, if the direction is parallel (or nearly
	 *  so) to the planes, it (obviously) won't intersect
	 *  either of them.
	 */
	if( dprime[Z] == 0.0 ) {
		RT_TGC_SEG_MISS(segp[ix]);
		continue;
	}

	dir = VDOT( tgc->tgc_N, rp[ix]->r_dir );	/* direc */
	if ( NEAR_ZERO( dir, RT_DOT_TOL ) ) {
		RT_TGC_SEG_MISS(segp[ix]);
		continue;
	}

	b = ( -pprime[Z] )/dprime[Z];
	/* Height vector is unitized (tgc->tgc_sH == 1.0) */
	t = ( 1.0 - pprime[Z] )/dprime[Z];

	VJOIN1( work, pprime, b, dprime );
	/* A and B vectors are unitized (tgc->tgc_A == _B == 1.0) */
	/* alpf = ALPHA(work[0], work[1], 1.0, 1.0 ) */
	alf1 = work[X]*work[X] + work[Y]*work[Y];

	VJOIN1( work, pprime, t, dprime );
	/* Must scale C and D vectors. */
	alf2 = ALPHA(work[X], work[Y], tgc->tgc_AAdCC,tgc->tgc_BBdDD);

	/*  It should not be possible for one planar intersection
	 *  to be outside its ellipse while the other is inside ...
	 *  but I wouldn't take any chances.
	 */
	if ( alf1 > 1.0 || alf2 > 1.0 ) {
		RT_TGC_SEG_MISS(segp[ix]);
		continue;
	}

	/*  Use the dot product (found earlier) of the plane
	 *  normal with the direction vector to determine the
	 *  orientation of the intersections.
	 */
	if ( dir > 0.0 ){
		segp[ix].seg_in.hit_dist = b * t_scale;
		segp[ix].seg_in.hit_surfno = TGC_NORM_BOT;	/* reverse normal */

		segp[ix].seg_out.hit_dist = t * t_scale;
		segp[ix].seg_out.hit_surfno = TGC_NORM_TOP;	/* normal */
	} else {
		segp[ix].seg_in.hit_dist = t * t_scale;
		segp[ix].seg_in.hit_surfno = TGC_NORM_TOP;	/* normal */

		segp[ix].seg_out.hit_dist = b * t_scale;
		segp[ix].seg_out.hit_surfno = TGC_NORM_BOT;	/* reverse normal */
	}
	}
    } /* end for each ray/cone pair */
    rt_free( (char *)C, "tor poly" );
}

/*
 *			R T _ P T _ S O R T
 *
 *  Sorts the values in t[] in descending order.
 */
void
rt_pt_sort( t, npts )
register fastf_t t[];
int npts;
{
	FAST fastf_t	u;
	register short	lim, n;

	for( lim = npts-1; lim > 0; lim-- )  {
		for( n = 0; n < lim; n++ )  {
			if( (u=t[n]) < t[n+1] )  {
				/* bubble larger towards [0] */
				t[n] = t[n+1];
				t[n+1] = u;
			}
		}
	}
}


/*
 *			R T _ T G C _ N O R M
 *
 *  Compute the normal to the cone, given a point on the STANDARD
 *  CONE centered at the origin of the X-Y plane.
 *
 *  The gradient of the cone at that point is the normal vector in
 *  the standard space.  This vector will need to be transformed
 *  back to the coordinate system of the original cone in order
 *  to be useful.  Then the transformed vector must be 'unitized.'
 *
 *  NOTE:  The transformation required is NOT the inverse of the of
 *	   the rotation to the standard cone, due to the shear involved
 *	   in the mapping.  The inverse maps points back to points,
 *	   but it is the transpose which maps normals back to normals.
 *	   If you really want to know why, talk to Ed Davisson or
 *	   Peter Stiller.
 *
 *  The equation for the standard cone *without* scaling is:
 *  (rotated the sheared)
 *
 *	f(X,Y,Z) =  X**2 * Q**2  +  Y**2 * R**2  -  R**2 * Q**2 = 0
 *
 *  where,
 *		R = a + ((c - a)/|H'|)*Z 
 *		Q = b + ((d - b)/|H'|)*Z
 *
 *  When the equation is scaled so the A, B, and the sheared H are
 *  unit length, as is done here, the equation can be coerced back
 *  into this same form with R and Q now being:
 *
 *		R = 1 + (c/a - 1)*Z
 *		Q = 1 + (d/b - 1)*Z
 *
 *  The gradient of f(x,y,z) = 0 is:
 *
 *	df/dx = 2 * x * Q**2
 *	df/dy = 2 * y * R**2
 *	df/dz = x**2 * 2 * Q * dQ/dz + y**2 * 2 * R * dR/dz
 *	      - R**2 * 2 * Q * dQ/dz - Q**2 * 2 * R * dR/dz
 *	      = 2 [(x**2 - R**2) * Q * dQ/dz + (y**2 - Q**2) * R * dR/dz]
 *
 *  where,
 *		dR/dz = (c/a - 1)
 *		dQ/dz = (d/b - 1)
 *
 *  [in the *unscaled* case these would be (c - a)/|H'| and (d - b)/|H'|]
 *  Since the gradient (normal) needs to be rescaled to unit length
 *  after mapping back to absolute coordinates, we divide the 2 out of
 *  the above expressions.
 */
void
rt_tgc_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct tgc_specific	*tgc =
		(struct tgc_specific *)stp->st_specific;
	FAST fastf_t	Q;
	FAST fastf_t	R;
	LOCAL vect_t	stdnorm;

	/* Hit point */
	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );

	/* Hits on the end plates are easy */
	switch( hitp->hit_surfno )  {
	case TGC_NORM_TOP:
		VMOVE( hitp->hit_normal, tgc->tgc_N );
		break;
	case TGC_NORM_BOT:
		VREVERSE( hitp->hit_normal, tgc->tgc_N );
		break;
	case TGC_NORM_BODY:
		/* Compute normal, given hit point on standard (unit) cone */
		R = 1 + tgc->tgc_CdAm1 * hitp->hit_vpriv[Z];
		Q = 1 + tgc->tgc_DdBm1 * hitp->hit_vpriv[Z];
		stdnorm[X] = hitp->hit_vpriv[X] * Q * Q;
		stdnorm[Y] = hitp->hit_vpriv[Y] * R * R;
		stdnorm[Z] = (hitp->hit_vpriv[X]*hitp->hit_vpriv[X] - R*R)
			     * Q * tgc->tgc_DdBm1
			   + (hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] - Q*Q)
			     * R * tgc->tgc_CdAm1;
		MAT4X3VEC( hitp->hit_normal, tgc->tgc_invRtShSc, stdnorm );
/*XXX - save scale */
		VUNITIZE( hitp->hit_normal );
		break;
	default:
		rt_log("rt_tgc_norm: bad surfno=%d\n", hitp->hit_surfno);
		break;
	}
}

/*
 *			R T _ T G C _ U V
 */
void
rt_tgc_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct tgc_specific	*tgc =
		(struct tgc_specific *)stp->st_specific;
	LOCAL vect_t work;
	LOCAL vect_t pprime;
	FAST fastf_t len;

	/* hit_point is on surface;  project back to unit cylinder,
	 * creating a vector from vertex to hit point.
	 */
	VSUB2( work, hitp->hit_point, tgc->tgc_V );
	MAT4X3VEC( pprime, tgc->tgc_ScShR, work );

	switch( hitp->hit_surfno )  {
	case TGC_NORM_BODY:
		/* Skin.  x,y coordinates define rotation.  radius = 1 */
		uvp->uv_u = acos(pprime[Y]) * rt_inv2pi;
		uvp->uv_v = pprime[Z];		/* height */
		break;
	case TGC_NORM_TOP:
		/* top plate */
		len = sqrt(pprime[X]*pprime[X]+pprime[Y]*pprime[Y]);
		uvp->uv_u = acos(pprime[Y]/len) * rt_inv2pi;
		uvp->uv_v = len;		/* rim v = 1 */
		break;
	case TGC_NORM_BOT:
		/* bottom plate */
		len = sqrt(pprime[X]*pprime[X]+pprime[Y]*pprime[Y]);
		uvp->uv_u = acos(pprime[Y]/len) * rt_inv2pi;
		uvp->uv_v = 1 - len;	/* rim v = 0 */
		break;
	}
	/* Handle other half of acos() domain */
	if( pprime[X] < 0 )
		uvp->uv_u = 1.0 - uvp->uv_u;

	/* uv_du should be relative to rotation, uv_dv relative to height */
	uvp->uv_du = uvp->uv_dv = 0;
}


/*
 *			R T _ T G C _ F R E E
 */
void
rt_tgc_free( stp )
struct soltab *stp;
{
	register struct tgc_specific	*tgc =
		(struct tgc_specific *)stp->st_specific;

	rt_free( (char *)tgc, "tgc_specific");
}

int
rt_tgc_class()
{
	return(0);
}


/*
 *			R T _ T G C _ I M P O R T
 *
 *  Import a TGC from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_tgc_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	struct rt_tgc_internal	*tip;
	union record		*rp;
	LOCAL fastf_t	vec[3*6];

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		rt_log("rt_tgc_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_TGC;
	ip->idb_ptr = rt_malloc( sizeof(struct rt_tgc_internal), "rt_tgc_internal");
	tip = (struct rt_tgc_internal *)ip->idb_ptr;
	tip->magic = RT_TGC_INTERNAL_MAGIC;

	/* Convert from database to internal format */
	rt_fastf_float( vec, rp->s.s_values, 6 );

	/* Apply modeling transformations */
	MAT4X3PNT( tip->v, mat, &vec[0*3] );
	MAT4X3VEC( tip->h, mat, &vec[1*3] );
	MAT4X3VEC( tip->a, mat, &vec[2*3] );
	MAT4X3VEC( tip->b, mat, &vec[3*3] );
	MAT4X3VEC( tip->c, mat, &vec[4*3] );
	MAT4X3VEC( tip->d, mat, &vec[5*3] );

	return(0);		/* OK */
}

/*
 *			R T _ T G C _ E X P O R T
 */
int
rt_tgc_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_tgc_internal	*tip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_TGC )  return(-1);
	tip = (struct rt_tgc_internal *)ip->idb_ptr;
	RT_TGC_CK_MAGIC(tip);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "tgc external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->s.s_type = GENTGC;

	/* NOTE: This also converts to dbfloat_t */
	VSCALE( &rec->s.s_values[0], tip->v, local2mm );
	VSCALE( &rec->s.s_values[3], tip->h, local2mm );
	VSCALE( &rec->s.s_values[6], tip->a, local2mm );
	VSCALE( &rec->s.s_values[9], tip->b, local2mm );
	VSCALE( &rec->s.s_values[12], tip->c, local2mm );
	VSCALE( &rec->s.s_values[15], tip->d, local2mm );

	return(0);
}

/*
 *			R T _ T G C _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_tgc_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_tgc_internal	*tip =
		(struct rt_tgc_internal *)ip->idb_ptr;
	char	buf[256];
	double	angles[5];
	vect_t	unitv;
	fastf_t	Hmag;

	RT_TGC_CK_MAGIC(tip);
	rt_vls_strcat( str, "truncated general cone (TGC)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
		tip->v[X] * mm2local,
		tip->v[Y] * mm2local,
		tip->v[Z] * mm2local );
	rt_vls_strcat( str, buf );

	Hmag = MAGNITUDE(tip->h);
	sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
		tip->h[X] * mm2local,
		tip->h[Y] * mm2local,
		tip->h[Z] * mm2local,
		Hmag * mm2local);
	rt_vls_strcat( str, buf );
	if( Hmag < VDIVIDE_TOL )  {
		rt_vls_strcat( str, "H vector is zero!\n");
	} else {
		register double	f = 1/Hmag;
		VSCALE( unitv, tip->h, f );
		rt_find_fallback_angle( angles, unitv );
		rt_pr_fallback_angle( str, "\tH", angles );
	}

	sprintf(buf, "\tA (%g, %g, %g) mag=%g\n",
		tip->a[X] * mm2local,
		tip->a[Y] * mm2local,
		tip->a[Z] * mm2local,
		MAGNITUDE(tip->a) * mm2local);
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tB (%g, %g, %g) mag=%g\n",
		tip->b[X] * mm2local,
		tip->b[Y] * mm2local,
		tip->b[Z] * mm2local,
		MAGNITUDE(tip->b) * mm2local);
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tC (%g, %g, %g) mag=%g\n",
		tip->c[X] * mm2local,
		tip->c[Y] * mm2local,
		tip->c[Z] * mm2local,
		MAGNITUDE(tip->c) * mm2local);
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tD (%g, %g, %g) mag=%g\n",
		tip->d[X] * mm2local,
		tip->d[Y] * mm2local,
		tip->d[Z] * mm2local,
		MAGNITUDE(tip->d) * mm2local);
	rt_vls_strcat( str, buf );

	VCROSS( unitv, tip->c, tip->d );
	VUNITIZE( unitv );
	rt_find_fallback_angle( angles, unitv );
	rt_pr_fallback_angle( str, "\tAxB", angles );

	return(0);
}

/*
 *			R T _ T G C _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_tgc_ifree( ip )
struct rt_db_internal	*ip;
{
	RT_CK_DB_INTERNAL(ip);
	rt_free( ip->idb_ptr, "tgc ifree" );
}

/*
 *			R T _ T G C _ P L O T
 */
int
rt_tgc_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct rt_tgc_internal	*tip;
	register int		i;
	LOCAL fastf_t		top[16*3];
	LOCAL fastf_t		bottom[16*3];
	LOCAL vect_t		work;		/* Vec addition work area */

	RT_CK_DB_INTERNAL(ip);
	tip = (struct rt_tgc_internal *)ip->idb_ptr;
	RT_TGC_CK_MAGIC(tip);

	rt_ell_16pts( bottom, tip->v, tip->a, tip->b );
	VADD2( work, tip->v, tip->h );
	rt_ell_16pts( top, work, tip->c, tip->d );

	/* Draw the top */
	RT_ADD_VLIST( vhead, &top[15*ELEMENTS_PER_VECT], RT_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ )  {
		RT_ADD_VLIST( vhead, &top[i*ELEMENTS_PER_VECT], RT_VLIST_LINE_DRAW );
	}

	/* Draw the bottom */
	RT_ADD_VLIST( vhead, &bottom[15*ELEMENTS_PER_VECT], RT_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ )  {
		RT_ADD_VLIST( vhead, &bottom[i*ELEMENTS_PER_VECT], RT_VLIST_LINE_DRAW );
	}

	/* Draw connections */
	for( i=0; i<16; i += 4 )  {
		RT_ADD_VLIST( vhead, &top[i*ELEMENTS_PER_VECT], RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, &bottom[i*ELEMENTS_PER_VECT], RT_VLIST_LINE_DRAW );
	}
	return(0);
}

/*
 *			R T _ T G C _ C U R V E
 *
 *  Return the curvature of the TGC.
 */
void
rt_tgc_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct tgc_specific *tgc =
		(struct tgc_specific *)stp->st_specific;
	fastf_t	R, Q, R2, Q2;
	mat_t	M, dN, mtmp;
	vect_t	gradf, tmp, u, v;
	fastf_t	a, b, c, scale;
	vect_t	vec1, vec2;

	if( hitp->hit_surfno != TGC_NORM_BODY ) {
		/* We hit an end plate.  Choose any tangent vector. */
		vec_ortho( cvp->crv_pdir, hitp->hit_normal );
		cvp->crv_c1 = cvp->crv_c2 = 0;
		return;
	}

	R = 1 + tgc->tgc_CdAm1 * hitp->hit_vpriv[Z];
	Q = 1 + tgc->tgc_DdBm1 * hitp->hit_vpriv[Z];
	R2 = R*R;
	Q2 = Q*Q;

	/*
	 * Compute derivatives of the gradient (normal) field
	 * in ideal coords.  This is a symmetric matrix with
	 * the columns (dNx, dNy, dNz).
	 */
	mat_idn( dN );
	dN[0] = Q2;
	dN[2] = dN[8] = 2.0*Q*tgc->tgc_DdBm1 * hitp->hit_vpriv[X];
	dN[5] = R2;
	dN[6] = dN[9] = 2.0*R*tgc->tgc_CdAm1 * hitp->hit_vpriv[Y];
	dN[10] = tgc->tgc_DdBm1*tgc->tgc_DdBm1 * hitp->hit_vpriv[X]*hitp->hit_vpriv[X]
	       + tgc->tgc_CdAm1*tgc->tgc_CdAm1 * hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y]
	       - tgc->tgc_DdBm1*tgc->tgc_DdBm1 * R2
	       - tgc->tgc_CdAm1*tgc->tgc_CdAm1 * Q2
	       - 4.0*tgc->tgc_CdAm1*tgc->tgc_DdBm1 * R*Q;

	/* M = At * dN * A */
	mat_mul( mtmp, dN, tgc->tgc_ScShR );
	mat_mul( M, tgc->tgc_invRtShSc, mtmp );

	/* XXX - determine the scaling */
	gradf[X] = Q2 * hitp->hit_vpriv[X];
	gradf[Y] = R2 * hitp->hit_vpriv[Y];
	gradf[Z] = (hitp->hit_vpriv[X]*hitp->hit_vpriv[X] - R2) * Q * tgc->tgc_DdBm1 +
		   (hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] - Q2) * R * tgc->tgc_CdAm1;
	MAT4X3VEC( tmp, tgc->tgc_invRtShSc, gradf );
	scale = -1.0 / MAGNITUDE(tmp);
	/* XXX */

	/*
	 * choose a tangent plane coordinate system
	 *  (u, v, normal) form a right-handed triple
	 */
	vec_ortho( u, hitp->hit_normal );
	VCROSS( v, hitp->hit_normal, u );

	/* find the second fundamental form */
	MAT4X3VEC( tmp, M, u );
	a = VDOT(u, tmp) * scale;
	b = VDOT(v, tmp) * scale;
	MAT4X3VEC( tmp, M, v );
	c = VDOT(v, tmp) * scale;

	eigen2x2( &cvp->crv_c1, &cvp->crv_c2, vec1, vec2, a, b, c );
	VCOMB2( cvp->crv_pdir, vec1[X], u, vec1[Y], v );
	VUNITIZE( cvp->crv_pdir );
}

/*
 *			R T _ T G C _ T E S S
 *
 *  Preliminary tesselation of the TGC, same algorithm as vector list.
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_tgc_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	struct shell		*s;
	register int		i;
	LOCAL fastf_t		top[16*3];
	struct vertex		*vtop[16+1];
	LOCAL fastf_t		bottom[16*3];
	struct vertex		*vbottom[16+1];
	struct vertex		*vtemp[16+1];
	LOCAL vect_t		work;		/* Vec addition work area */
	LOCAL struct rt_tgc_internal	*tip;
	struct faceuse		*outfaceuses[2*16+2];
	struct vertex		*vertlist[4];

	RT_CK_DB_INTERNAL(ip);
	tip = (struct rt_tgc_internal *)ip->idb_ptr;
	RT_TGC_CK_MAGIC(tip);

	/* Create two 16 point ellipses
	 *  Note that in both cases the points go counterclockwise (CCW).
	 */
	rt_ell_16pts( bottom, tip->v, tip->a, tip->b );
	VADD2( work, tip->v, tip->h );
	rt_ell_16pts( top, work, tip->c, tip->d );

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = RT_LIST_FIRST(shell, &(*r)->s_hd);

	for( i=0; i<16; i++ )  {
		vtop[i] = vtemp[i] = (struct vertex *)0;
	}

	/* Top face topology.  Verts are considered to go CCW */
	outfaceuses[0] = nmg_cface(s, vtop, 16);

	/* Bottom face topology.  Verts must go in opposite dir (CW) */
	outfaceuses[1] = nmg_cface(s, vtemp, 16);
	for( i=0; i<16; i++ )  vbottom[i] = vtemp[16-1-i];

	/* Duplicate [0] as [16] to handle loop end condition, below */
	vtop[16] = vtop[0];
	vbottom[16] = vbottom[0];

	/* Build topology for all the triangular side faces (2*16 of them)
	 * hanging down from the top face to the bottom face.
	 * increasing indices go towards counter-clockwise (CCW).
	 */
	for( i=0; i<16; i++ )  {
		vertlist[0] = vtop[i];		/* from top, */
		vertlist[1] = vbottom[i];	/* straight down, */
		vertlist[2] = vbottom[i+1];	/* to left & back to top */
		outfaceuses[2+2*i] = nmg_cface(s, vertlist, 3);

		vertlist[0] = vtop[i];		/* from top, */
		vertlist[1] = vbottom[i+1];	/* down to left, */
		vertlist[2] = vtop[i+1];	/* straight up & to right */
		outfaceuses[2+2*i+1] = nmg_cface(s, vertlist, 3);
	}

	for( i=0; i<16; i++ )  {
		NMG_CK_VERTEX(vtop[i]);
		NMG_CK_VERTEX(vbottom[i]);
	}

	/* Associate the vertex geometry, CCW */
	for( i=0; i<16; i++ )  {
		nmg_vertex_gv( vtop[i], &top[3*(i)] );
	}
	for( i=0; i<16; i++ )  {
		nmg_vertex_gv( vbottom[i], &bottom[3*(i)] );
	}

	/* Associate the face geometry */
	for (i=0 ; i < 2*16+2 ; ++i) {
		if( nmg_fu_planeeqn( outfaceuses[i], tol ) < 0 )
			return -1;		/* FAIL */
	}

	/* Glue the edges of different outward pointing face uses together */
	nmg_gluefaces( outfaceuses, 2*16+2 );

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r );

	/* XXX just for testing, to make up for loads of triangles ... */
	nmg_shell_coplanar_face_merge( s, tol, 1 );

	return(0);
}
