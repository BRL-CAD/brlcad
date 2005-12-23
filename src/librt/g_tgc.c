/*                         G _ T G C . C
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup cc */

/*@{*/
/** @file g_tgc.c
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
 *	Paul Stay		(Convert to tnurbs)
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/*@}*/

#ifndef lint
static const char RCStgc[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"
#include "nurb.h"

BU_EXTERN(int rt_rec_prep, (struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip));

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


static void rt_tgc_rotate(fastf_t *A, fastf_t *B, fastf_t *Hv, fastf_t *Rot, fastf_t *Inv, struct tgc_specific *tgc);
static void rt_tgc_shear(const fastf_t *vect, int axis, fastf_t *Shr, fastf_t *Trn, fastf_t *Inv);
static void rt_tgc_scale(fastf_t a, fastf_t b, fastf_t h, fastf_t *Scl, fastf_t *Inv);
static void nmg_tgc_disk(struct faceuse *fu, fastf_t *rmat, fastf_t height, int flip);
static void nmg_tgc_nurb_cyl(struct faceuse *fu, fastf_t *top_mat, fastf_t *bot_mat);
void rt_pt_sort(register fastf_t *t, int npts);

#define VLARGE		1000000.0
#define	ALPHA(x,y,c,d)	( (x)*(x)*(c) + (y)*(y)*(d) )

const struct bu_structparse rt_tgc_parse[] = {
    { "%f", 3, "V", offsetof(struct rt_tgc_internal, v[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "H", offsetof(struct rt_tgc_internal, h[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "A", offsetof(struct rt_tgc_internal, a[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "B", offsetof(struct rt_tgc_internal, b[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "C", offsetof(struct rt_tgc_internal, c[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "D", offsetof(struct rt_tgc_internal, d[X]), BU_STRUCTPARSE_FUNC_NULL },
    { {'\0','\0','\0','\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL }
};


/**
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
rt_tgc_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
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
		bu_log("tgc(%s):  zero length H vector\n", stp->st_name );
		return(1);		/* BAD */
	}

	/* Validate that figure is not two-dimensional			*/
	if( NEAR_ZERO( mag_a, RT_LEN_TOL ) &&
	    NEAR_ZERO( mag_c, RT_LEN_TOL ) ) {
		bu_log("tgc(%s):  vectors A, C zero length\n", stp->st_name );
		return (1);
	}
	if( NEAR_ZERO( mag_b, RT_LEN_TOL ) &&
	    NEAR_ZERO( mag_d, RT_LEN_TOL ) ) {
		bu_log("tgc(%s):  vectors B, D zero length\n", stp->st_name );
		return (1);
	}

	/* Validate that both ends are not degenerate */
	if( prod_ab <= SMALL )  {
		/* AB end is degenerate */
		if( prod_cd <= SMALL )  {
			bu_log("tgc(%s):  Both ends degenerate\n", stp->st_name);
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
		bu_log("NOTE: tgc(%s): degenerate end exchanged\n", stp->st_name);
	}

	/* Ascertain whether H lies in A-B plane 			*/
	VCROSS( work, tip->a, tip->b );
	f = VDOT( tip->h, work ) / ( prod_ab*mag_h );
	if ( NEAR_ZERO(f, RT_DOT_TOL) ) {
		bu_log("tgc(%s):  H lies in A-B plane\n",stp->st_name);
		return(1);		/* BAD */
	}

	if( prod_ab > SMALL )  {
		/* Validate that A.B == 0 */
		f = VDOT( tip->a, tip->b ) / prod_ab;
		if( ! NEAR_ZERO(f, RT_DOT_TOL) ) {
			bu_log("tgc(%s):  A not perpendicular to B, f=%g\n",
			    stp->st_name, f);
			bu_log("tgc: dot=%g / a*b=%g\n",
			    VDOT( tip->a, tip->b ),  prod_ab );
			return(1);		/* BAD */
		}
	}
	if( prod_cd > SMALL )  {
		/* Validate that C.D == 0 */
		f = VDOT( tip->c, tip->d ) / prod_cd;
		if( ! NEAR_ZERO(f, RT_DOT_TOL) ) {
			bu_log("tgc(%s):  C not perpendicular to D, f=%g\n",
			    stp->st_name, f);
			bu_log("tgc: dot=%g / c*d=%g\n",
			    VDOT( tip->c, tip->d ), prod_cd );
			return(1);		/* BAD */
		}
	}

	if( mag_a * mag_c > SMALL )  {
		/* Validate that  A || C */
		f = 1.0 - VDOT( tip->a, tip->c ) / (mag_a * mag_c);
		if( ! NEAR_ZERO(f, RT_DOT_TOL) ) {
			bu_log("tgc(%s):  A not parallel to C, f=%g\n",
			    stp->st_name, f);
			return(1);		/* BAD */
		}
	}

	if( mag_b * mag_d > SMALL )  {
		/* Validate that  B || D, for parallel planes	*/
		f = 1.0 - VDOT( tip->b, tip->d ) / (mag_b * mag_d);
		if( ! NEAR_ZERO(f, RT_DOT_TOL) ) {
			bu_log("tgc(%s):  B not parallel to D, f=%g\n",
			    stp->st_name, f);
			return(1);		/* BAD */
		}
	}

	/* solid is OK, compute constant terms, etc. */
	BU_GETSTRUCT( tgc, tgc_specific );
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

	bn_mat_mul( tmp, Shr, Rot );
	bn_mat_mul( tgc->tgc_ScShR, Scl, tmp );

	bn_mat_mul( tmp, tShr, Scl );
	bn_mat_mul( tgc->tgc_invRtShSc, iRot, tmp );

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


/**
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
rt_tgc_rotate(fastf_t *A, fastf_t *B, fastf_t *Hv, fastf_t *Rot, fastf_t *Inv, struct tgc_specific *tgc)
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

	MAT_IDN( Rot );
	MAT_IDN( Inv );

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

/**
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
rt_tgc_shear(const fastf_t *vect, int axis, fastf_t *Shr, fastf_t *Trn, fastf_t *Inv)
{
	MAT_IDN( Shr );
	MAT_IDN( Trn );
	MAT_IDN( Inv );

	if( NEAR_ZERO( vect[axis], SMALL_FASTF ) )
		rt_bomb("rt_tgc_shear() divide by zero\n");

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

/**
 *			R T _ T G C _ S C A L E
 */
static void
rt_tgc_scale(fastf_t a, fastf_t b, fastf_t h, fastf_t *Scl, fastf_t *Inv)
{
	MAT_IDN( Scl );
	MAT_IDN( Inv );
	Scl[0]  /= a;
	Scl[5]  /= b;
	Scl[10] /= h;
	Inv[0]  = a;
	Inv[5]  = b;
	Inv[10] = h;
	return;
}

/**
 *  			R T _ T G C _ P R I N T
 */
void
rt_tgc_print(register const struct soltab *stp)
{
	register const struct tgc_specific	*tgc =
	(struct tgc_specific *)stp->st_specific;

	VPRINT( "V", tgc->tgc_V );
	bu_log( "mag sheared H = %f\n", tgc->tgc_sH );
	bu_log( "mag A = %f\n", tgc->tgc_A );
	bu_log( "mag B = %f\n", tgc->tgc_B );
	bu_log( "mag C = %f\n", tgc->tgc_C );
	bu_log( "mag D = %f\n", tgc->tgc_D );
	VPRINT( "Top normal", tgc->tgc_N );

	bn_mat_print( "Sc o Sh o R", tgc->tgc_ScShR );
	bn_mat_print( "invR o trnSh o Sc", tgc->tgc_invRtShSc );

	if( tgc->tgc_AD_CB )  {
		bu_log( "A*D == C*B.  Equal eccentricities gives quadratic equation.\n");
	} else {
		bu_log( "A*D != C*B.  Quartic equation.\n");
	}
	bu_log( "(C/A - 1) = %f\n", tgc->tgc_CdAm1 );
	bu_log( "(D/B - 1) = %f\n", tgc->tgc_DdBm1 );
	bu_log( "(|A|**2)/(|C|**2) = %f\n", tgc->tgc_AAdCC );
	bu_log( "(|B|**2)/(|D|**2) = %f\n", tgc->tgc_BBdDD );
}

/* hit_surfno is set to one of these */
#define	TGC_NORM_BODY	(1)		/* compute normal */
#define	TGC_NORM_TOP	(2)		/* copy tgc_N */
#define	TGC_NORM_BOT	(3)		/* copy reverse tgc_N */

/**
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
rt_tgc_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register const struct tgc_specific	*tgc =
	(struct tgc_specific *)stp->st_specific;
	register struct seg	*segp;
	LOCAL vect_t		pprime;
	LOCAL vect_t		dprime;
	LOCAL vect_t		work;
	LOCAL fastf_t		k[6];
	LOCAL int		hit_type[6];
	LOCAL fastf_t		t, b, zval, dir;
	LOCAL fastf_t		t_scale;
	LOCAL fastf_t		alf1, alf2;
	LOCAL int		npts;
	LOCAL int		intersect;
	LOCAL vect_t		cor_pprime;	/* corrected P prime */
	LOCAL fastf_t		cor_proj = 0;	/* corrected projected dist */
	LOCAL int		i;
	LOCAL bn_poly_t		C;	/*  final equation	*/
	LOCAL bn_poly_t		Xsqr, Ysqr;
	LOCAL bn_poly_t		R, Rsqr;

	/* find rotated point and direction */
	MAT4X3VEC( dprime, tgc->tgc_ScShR, rp->r_dir );

	/*
	 *  A vector of unit length in model space (r_dir) changes length in
	 *  the special unit-tgc space.  This scale factor will restore
	 *  proper length after hit points are found.
	 */
	t_scale = MAGNITUDE(dprime);
	if( NEAR_ZERO( t_scale, SMALL_FASTF ) )  {
		bu_log("tgc(%s) dprime=(%g,%g,%g), t_scale=%e, miss.\n",
		    V3ARGS(dprime), t_scale);
		return 0;
	}
	t_scale = 1/t_scale;
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

	/* (void) rt_poly_mul( &R, &R, &Rsqr ); */
	Rsqr.dgr = 2;
	Rsqr.cf[0] = R.cf[0] * R.cf[0];
	Rsqr.cf[1] = R.cf[0] * R.cf[1] * 2;
	Rsqr.cf[2] = R.cf[1] * R.cf[1];

	/*
	 *  If the eccentricities of the two ellipses are the same,
	 *  then the cone equation reduces to a much simpler quadratic
	 *  form.  Otherwise it is a (gah!) quartic equation.
	 *
	 *  this can only be done when C.cf[0] is not too small!!!! (JRA)
	 */
	C.cf[0] = Xsqr.cf[0] + Ysqr.cf[0] - Rsqr.cf[0];
	if( tgc->tgc_AD_CB && !NEAR_ZERO( C.cf[0], 1.0e-10 )  ) {
		FAST fastf_t roots;

		/*
		 *  (void) rt_poly_add( &Xsqr, &Ysqr, &sum );
		 *  (void) rt_poly_sub( &sum, &Rsqr, &C );
		 */
		C.dgr = 2;
		C.cf[1] = Xsqr.cf[1] + Ysqr.cf[1] - Rsqr.cf[1];
		C.cf[2] = Xsqr.cf[2] + Ysqr.cf[2] - Rsqr.cf[2];

		/* Find the real roots the easy way.  C.dgr==2 */
		if( (roots = C.cf[1]*C.cf[1] - 4 * C.cf[0] * C.cf[2]) < 0 ) {
			npts = 0;	/* no real roots */
		} else {
			register fastf_t	f;
			roots = sqrt(roots);
			k[0] = (roots - C.cf[1]) * (f = 0.5 / C.cf[0]);
			hit_type[0] = TGC_NORM_BODY;
			k[1] = (roots + C.cf[1]) * -f;
			hit_type[1] = TGC_NORM_BODY;
			npts = 2;
		}
	} else {
		LOCAL bn_poly_t	Q, Qsqr;
		LOCAL bn_complex_t	val[4];	/* roots of final equation */
		register int	l;
		register int nroots;

		Q.dgr = 1;
		Q.cf[0] = dprime[Z] * tgc->tgc_DdBm1;
		/* B vector is unitized (tgc->tgc_B == 1.0) */
		Q.cf[1] = (cor_pprime[Z] * tgc->tgc_DdBm1) + 1.0;

		/* (void) rt_poly_mul( &Q, &Q, &Qsqr ); */
		Qsqr.dgr = 2;
		Qsqr.cf[0] = Q.cf[0] * Q.cf[0];
		Qsqr.cf[1] = Q.cf[0] * Q.cf[1] * 2;
		Qsqr.cf[2] = Q.cf[1] * Q.cf[1];

		/*
		 * (void) rt_poly_mul( &Qsqr, &Xsqr, &T1 );
		 * (void) rt_poly_mul( &Rsqr, &Ysqr, &T2 );
		 * (void) rt_poly_mul( &Rsqr, &Qsqr, &T3 );
		 * (void) rt_poly_add( &T1, &T2, &sum );
		 * (void) rt_poly_sub( &sum, &T3, &C );
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
		nroots = rt_poly_roots( &C , val, stp->st_dp->d_namep );

		/*  Only real roots indicate an intersection in real space.
		 *
		 *  Look at each root returned; if the imaginary part is zero
		 *  or sufficiently close, then use the real part as one value
		 *  of 't' for the intersections
		 */
		for ( l=0, npts=0; l < nroots; l++ ){
			if ( NEAR_ZERO( val[l].im, 1e-10 ) ) {
				hit_type[npts] = TGC_NORM_BODY;
				k[npts++] = val[l].re;
			}
		}
		/* Here, 'npts' is number of points being returned */
		if ( npts != 0 && npts != 2 && npts != 4 && npts > 0 ){
			bu_log("tgc:  reduced %d to %d roots\n",nroots,npts);
			bn_pr_roots( stp->st_name, val, nroots );
		} else if (nroots < 0) {
		    static int reported=0;
		    bu_log("The root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
		    if (!reported) {
			VPRINT("while shooting from:\t", rp->r_pt);
			VPRINT("while shooting at:\t", rp->r_dir);
			bu_log("Additional truncated general cone convergence failure details will be suppressed.\n");
			reported=1;
		    }
		}
	}

	/*
	 * Reverse above translation by adding distance to all 'k' values.
	 */
	for( i = 0; i < npts; ++i )  {
		k[i] += cor_proj;
	}

	/*
	 * Eliminate hits beyond the end planes
	 */
	i = 0;
	while( i < npts ) {
		zval = k[i]*dprime[Z] + pprime[Z];
		/* Height vector is unitized (tgc->tgc_sH == 1.0) */
		if ( zval >= 1.0 || zval <= 0.0 ){
			int j;
			/* drop this hit */
			npts--;
			for( j=i ; j<npts ; j++ ) {
				hit_type[j] = hit_type[j+1];
				k[j] = k[j+1];
			}
		} else {
			i++;
		}
	}

	/*
	 * Consider intersections with the end ellipses
	 */
	dir = VDOT( tgc->tgc_N, rp->r_dir );
	if( !NEAR_ZERO( dprime[Z], SMALL_FASTF ) && !NEAR_ZERO( dir, RT_DOT_TOL ) )  {
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
			hit_type[npts] = TGC_NORM_BOT;
			k[npts++] = b;
		}
		if ( alf2 <= 1.0 ){
			hit_type[npts] = TGC_NORM_TOP;
			k[npts++] = t;
		}
	}


	/* Sort Most distant to least distant: rt_pt_sort( k, npts ) */
	{
		register fastf_t	u;
		register short		lim, n;
		register int		type;

		for( lim = npts-1; lim > 0; lim-- )  {
			for( n = 0; n < lim; n++ )  {
				if( (u=k[n]) < k[n+1] )  {
					/* bubble larger towards [0] */
					type = hit_type[n];
					hit_type[n] = hit_type[n+1];
					hit_type[n+1] = type;
					k[n] = k[n+1];
					k[n+1] = u;
				}
			}
		}
	}
	/* Now, k[0] > k[npts-1] */

	if( npts%2 ) {
		/* odd number of hits!!!
		 * perhaps we got two hits on an edge
		 * check for duplicate hit distances
		 */

		for( i=npts-1 ; i>0 ; i-- ) {
			fastf_t diff;

			diff = k[i-1] - k[i];	/* non-negative due to sorting */
			if( diff < ap->a_rt_i->rti_tol.dist ) {
				/* remove this duplicate hit */
				int j;

				npts--;
				for( j=i ; j<npts ; j++ ) {
					hit_type[j] = hit_type[j+1];
					k[j] = k[j+1];
				}

				/* now have even number of hits */
				break;
			}
		}
	}

	if ( npts != 0 && npts != 2 && npts != 4 ){
		bu_log("tgc(%s):  %d intersects != {0,2,4}\n",
		    stp->st_name, npts );
		bu_log( "\tray: pt = (%g %g %g), dir = (%g %g %g)\n",
			V3ARGS( ap->a_ray.r_pt ),
			V3ARGS( ap->a_ray.r_dir ) );
		for( i=0 ; i<npts ; i++ ) {
			bu_log( "\t%g", k[i]*t_scale );
		}
		bu_log( "\n" );
		return(0);			/* No hit */
	}

	intersect = 0;
	for( i=npts-1 ; i>0 ; i -= 2 ) {
		RT_GET_SEG( segp, ap->a_resource );
		segp->seg_stp = stp;

		segp->seg_in.hit_dist = k[i] * t_scale;
		segp->seg_in.hit_surfno = hit_type[i];
		if( segp->seg_in.hit_surfno == TGC_NORM_BODY ) {
			VJOIN1( segp->seg_in.hit_vpriv, pprime, k[i], dprime );
		} else {
			if( dir > 0.0 ) {
				segp->seg_in.hit_surfno = TGC_NORM_BOT;
			} else {
				segp->seg_in.hit_surfno = TGC_NORM_TOP;
			}
		}

		segp->seg_out.hit_dist = k[i-1] * t_scale;
		segp->seg_out.hit_surfno = hit_type[i-1];
		if( segp->seg_out.hit_surfno == TGC_NORM_BODY ) {
			VJOIN1( segp->seg_out.hit_vpriv, pprime, k[i-1], dprime );
		} else {
			if( dir > 0.0 ) {
				segp->seg_out.hit_surfno = TGC_NORM_TOP;
			} else {
				segp->seg_out.hit_surfno = TGC_NORM_BOT;
			}
		}
		intersect++;
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	}

	return( intersect );
}

#define RT_TGC_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/**
 *			R T _ T G C _ V S H O T
 *
 *  The Homer vectorized version.
 */
void
rt_tgc_vshot(struct soltab **stp, register struct xray **rp, struct seg *segp, int n, struct application *ap)


                               /* array of segs (results returned) */
                               /* Number of ray/object pairs */

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
	LOCAL bn_poly_t		*C;	/*  final equation	*/
	LOCAL bn_poly_t		Xsqr, Ysqr;
	LOCAL bn_poly_t		R, Rsqr;

	/* Allocate space for polys and roots */
	C = (bn_poly_t *)bu_malloc(n * sizeof(bn_poly_t), "tor bn_poly_t");

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

		/* (void) rt_poly_mul( &R, &R, &Rsqr ); inline expands to: */
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
			/* (void) rt_poly_add( &Xsqr, &Ysqr, &sum ); and */
			/* (void) rt_poly_sub( &sum, &Rsqr, &C ); inline expand to */
			C[ix].dgr = 2;
			C[ix].cf[0] = Xsqr.cf[0] + Ysqr.cf[0] - Rsqr.cf[0];
			C[ix].cf[1] = Xsqr.cf[1] + Ysqr.cf[1] - Rsqr.cf[1];
			C[ix].cf[2] = Xsqr.cf[2] + Ysqr.cf[2] - Rsqr.cf[2];
		} else {
			LOCAL bn_poly_t	Q, Qsqr;

			Q.dgr = 1;
			Q.cf[0] = dprime[Z] * tgc->tgc_DdBm1;
			/* B vector is unitized (tgc->tgc_B == 1.0) */
			Q.cf[1] = (cor_pprime[Z] * tgc->tgc_DdBm1) + 1.0;

			/* (void) rt_poly_mul( &Q, &Q, &Qsqr ); inline expands to */
			Qsqr.dgr = 2;
			Qsqr.cf[0] = Q.cf[0] * Q.cf[0];
			Qsqr.cf[1] = Q.cf[0] * Q.cf[1] * 2;
			Qsqr.cf[2] = Q.cf[1] * Q.cf[1];

			/* (void) rt_poly_mul( &Qsqr, &Xsqr, &T1 ); inline expands to */
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

			/* (void) rt_poly_mul( &Rsqr, &Ysqr, &T2 ); and */
			/* (void) rt_poly_add( &T1, &T2, &sum ); inline expand to */
			C[ix].cf[0] += Rsqr.cf[0] * Ysqr.cf[0];
			C[ix].cf[1] += Rsqr.cf[0] * Ysqr.cf[1] +
			    Rsqr.cf[1] * Ysqr.cf[0];
			C[ix].cf[2] += Rsqr.cf[0] * Ysqr.cf[2] +
			    Rsqr.cf[1] * Ysqr.cf[1] +
			    Rsqr.cf[2] * Ysqr.cf[0];
			C[ix].cf[3] += Rsqr.cf[1] * Ysqr.cf[2] +
			    Rsqr.cf[2] * Ysqr.cf[1];
			C[ix].cf[4] += Rsqr.cf[2] * Ysqr.cf[2];

			/* (void) rt_poly_mul( &Rsqr, &Qsqr, &T3 ); and */
			/* (void) rt_poly_sub( &sum, &T3, &C ); inline expand to */
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
			LOCAL bn_complex_t	val[4];	/* roots of final equation */
			register int	l;
			register int nroots;

			/*  The equation is 4th order, so we expect 0 to 4 roots */
			nroots = rt_poly_roots( &C[ix] , val, (*stp)->st_dp->d_namep );

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
			if ( npts != 0 && npts != 2 && npts != 4 && npts > 0 ){
				bu_log("tgc:  reduced %d to %d roots\n",nroots,npts);
				bn_pr_roots( "tgc", val, nroots );
			} else if (nroots < 0) {
			    static int reported=0;
			    bu_log("The root solver failed to converge on a solution for %s\n", stp[ix]->st_dp->d_namep);
			    if (!reported) {
				VPRINT("while shooting from:\t", rp[ix]->r_pt);
				VPRINT("while shooting at:\t", rp[ix]->r_dir);
				bu_log("Additional truncated general cone convergence failure details will be suppressed.\n");
				reported=1;
			    }
			}
		}

		/*
	 * Reverse above translation by adding distance to all 'k' values.
	 */
		for( i = 0; i < npts; ++i )
			k[i] -= cor_proj;

		if ( npts != 0 && npts != 2 && npts != 4 ){
			bu_log("tgc(%s):  %d intersects != {0,2,4}\n",
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
				bu_log("tgc: dprime[Z] = 0!\n" );
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
				bu_log("tgc(%s):  only 1 intersect\n", stp[ix]->st_name);
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
	bu_free( (char *)C, "tor bn_poly_t" );
}

/**
 *			R T _ P T _ S O R T
 *
 *  Sorts the values in t[] in descending order.
 */
void
rt_pt_sort(register fastf_t t[], int npts)
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


/**
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
rt_tgc_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
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
		bu_log("rt_tgc_norm: bad surfno=%d\n", hitp->hit_surfno);
		break;
	}
}

/**
 *			R T _ T G C _ U V
 */
void
rt_tgc_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
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
		/* scale coords to unit circle (they are already scaled by bottom plate radii) */
		pprime[X] *= tgc->tgc_A / (tgc->tgc_A*( 1.0 - pprime[Z]) + tgc->tgc_C*pprime[Z]);
		pprime[Y] *= tgc->tgc_B / (tgc->tgc_B*( 1.0 - pprime[Z]) + tgc->tgc_D*pprime[Z]);
		uvp->uv_u = atan2( pprime[Y], pprime[X] ) / bn_twopi + 0.5;
		uvp->uv_v = pprime[Z];		/* height */
		break;
	case TGC_NORM_TOP:
		/* top plate */
		/* scale coords to unit circle (they are already scaled by bottom plate radii) */
		pprime[X] *= tgc->tgc_A / tgc->tgc_C;
		pprime[Y] *= tgc->tgc_B / tgc->tgc_D;
		uvp->uv_u = atan2( pprime[Y], pprime[X] ) / bn_twopi + 0.5;
		len = sqrt(pprime[X]*pprime[X]+pprime[Y]*pprime[Y]);
		uvp->uv_v = len;		/* rim v = 1 */
		break;
	case TGC_NORM_BOT:
		/* bottom plate */
		len = sqrt(pprime[X]*pprime[X]+pprime[Y]*pprime[Y]);
		uvp->uv_u = atan2( pprime[Y], pprime[X] ) / bn_twopi + 0.5;
		uvp->uv_v = 1 - len;	/* rim v = 0 */
		break;
	}

	if( uvp->uv_u < 0.0 )
		uvp->uv_u = 0.0;
	else if( uvp->uv_u > 1.0 )
		uvp->uv_u = 1.0;
	if( uvp->uv_v < 0.0 )
		uvp->uv_v = 0.0;
	else if( uvp->uv_v > 1.0 )
		uvp->uv_v = 1.0;

	/* uv_du should be relative to rotation, uv_dv relative to height */
	uvp->uv_du = uvp->uv_dv = 0;
}


/**
 *			R T _ T G C _ F R E E
 */
void
rt_tgc_free(struct soltab *stp)
{
	register struct tgc_specific	*tgc =
	(struct tgc_specific *)stp->st_specific;

	bu_free( (char *)tgc, "tgc_specific");
}

int
rt_tgc_class(void)
{
	return(0);
}


/**
 *			R T _ T G C _ I M P O R T
 *
 *  Import a TGC from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_tgc_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_tgc_internal	*tip;
	union record		*rp;
	LOCAL fastf_t	vec[3*6];

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		bu_log("rt_tgc_import: defective record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_TGC;
	ip->idb_meth = &rt_functab[ID_TGC];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_tgc_internal), "rt_tgc_internal");
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

/**
 *			R T _ T G C _ E X P O R T
 */
int
rt_tgc_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_tgc_internal	*tip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_TGC && ip->idb_type != ID_REC )  return(-1);
	tip = (struct rt_tgc_internal *)ip->idb_ptr;
	RT_TGC_CK_MAGIC(tip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "tgc external");
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

/**
 *			R T _ T G C _ I M P O R T 5
 *
 *  Import a TGC from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_tgc_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_tgc_internal	*tip;
	fastf_t			vec[3*6];

	BU_CK_EXTERNAL( ep );

	BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3*6 );

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_TGC;
	ip->idb_meth = &rt_functab[ID_TGC];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_tgc_internal), "rt_tgc_internal");

	tip = (struct rt_tgc_internal *)ip->idb_ptr;
	tip->magic = RT_TGC_INTERNAL_MAGIC;

	/* Convert from database (network) to internal (host) format */
	ntohd( (unsigned char *)vec, ep->ext_buf, 3*6 );

	/* Apply modeling transformations */
	MAT4X3PNT( tip->v, mat, &vec[0*3] );
	MAT4X3VEC( tip->h, mat, &vec[1*3] );
	MAT4X3VEC( tip->a, mat, &vec[2*3] );
	MAT4X3VEC( tip->b, mat, &vec[3*3] );
	MAT4X3VEC( tip->c, mat, &vec[4*3] );
	MAT4X3VEC( tip->d, mat, &vec[5*3] );

	return(0);		/* OK */
}

/**
 *			R T _ T G C _ E X P O R T 5
 */
int
rt_tgc_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_tgc_internal	*tip;
	fastf_t			vec[3*6];

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_TGC && ip->idb_type != ID_REC )  return(-1);
	tip = (struct rt_tgc_internal *)ip->idb_ptr;
	RT_TGC_CK_MAGIC(tip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 3*6;
	ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "tgc external");

	/* scale 'em into local buffer */
	VSCALE( &vec[0*3], tip->v, local2mm );
	VSCALE( &vec[1*3], tip->h, local2mm );
	VSCALE( &vec[2*3], tip->a, local2mm );
	VSCALE( &vec[3*3], tip->b, local2mm );
	VSCALE( &vec[4*3], tip->c, local2mm );
	VSCALE( &vec[5*3], tip->d, local2mm );

	/* Convert from internal (host) to database (network) format */
	htond( ep->ext_buf, (unsigned char *)vec, 3*6 );

	return(0);
}

/**
 *			R T _ T G C _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_tgc_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_tgc_internal	*tip =
	(struct rt_tgc_internal *)ip->idb_ptr;
	char	buf[256];
	double	angles[5];
	vect_t	unitv;
	fastf_t	Hmag;

	RT_TGC_CK_MAGIC(tip);
	bu_vls_strcat( str, "truncated general cone (TGC)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(tip->v[X] * mm2local),
	    INTCLAMP(tip->v[Y] * mm2local),
	    INTCLAMP(tip->v[Z] * mm2local) );
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tTop (%g, %g, %g)\n",
	    INTCLAMP((tip->v[X] + tip->h[X]) * mm2local),
	    INTCLAMP((tip->v[Y] + tip->h[Y]) * mm2local),
	    INTCLAMP((tip->v[Z] + tip->h[Z]) * mm2local) );
	bu_vls_strcat( str, buf );

	Hmag = MAGNITUDE(tip->h);
	sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->h[X] * mm2local),
	    INTCLAMP(tip->h[Y] * mm2local),
	    INTCLAMP(tip->h[Z] * mm2local),
	    INTCLAMP(Hmag * mm2local) );
	bu_vls_strcat( str, buf );
	if( Hmag < VDIVIDE_TOL )  {
		bu_vls_strcat( str, "H vector is zero!\n");
	} else {
		register double	f = 1/Hmag;
		VSCALE( unitv, tip->h, f );
		rt_find_fallback_angle( angles, unitv );
		rt_pr_fallback_angle( str, "\tH", angles );
	}

	sprintf(buf, "\tA (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->a[X] * mm2local),
	    INTCLAMP(tip->a[Y] * mm2local),
	    INTCLAMP(tip->a[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(tip->a) * mm2local) );
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tB (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->b[X] * mm2local),
	    INTCLAMP(tip->b[Y] * mm2local),
	    INTCLAMP(tip->b[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(tip->b) * mm2local) );
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tC (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->c[X] * mm2local),
	    INTCLAMP(tip->c[Y] * mm2local),
	    INTCLAMP(tip->c[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(tip->c) * mm2local) );
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tD (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->d[X] * mm2local),
	    INTCLAMP(tip->d[Y] * mm2local),
	    INTCLAMP(tip->d[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(tip->d) * mm2local) );
	bu_vls_strcat( str, buf );

	VCROSS( unitv, tip->c, tip->d );
	VUNITIZE( unitv );
	rt_find_fallback_angle( angles, unitv );
	rt_pr_fallback_angle( str, "\tAxB", angles );

	return(0);
}

/**
 *			R T _ T G C _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_tgc_ifree(struct rt_db_internal *ip)
{
	RT_CK_DB_INTERNAL(ip);
	bu_free( ip->idb_ptr, "tgc ifree" );
	ip->idb_ptr = GENPTR_NULL;
}

/**
 *			R T _ T G C _ P L O T
 */
int
rt_tgc_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
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
	RT_ADD_VLIST( vhead, &top[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ )  {
		RT_ADD_VLIST( vhead, &top[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}

	/* Draw the bottom */
	RT_ADD_VLIST( vhead, &bottom[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ )  {
		RT_ADD_VLIST( vhead, &bottom[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}

	/* Draw connections */
	for( i=0; i<16; i += 4 )  {
		RT_ADD_VLIST( vhead, &top[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, &bottom[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}
	return(0);
}

/**
 *			R T _ T G C _ C U R V E
 *
 *  Return the curvature of the TGC.
 */
void
rt_tgc_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
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
		bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
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
	MAT_IDN( dN );
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
	bn_mat_mul( mtmp, dN, tgc->tgc_ScShR );
	bn_mat_mul( M, tgc->tgc_invRtShSc, mtmp );

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
	bn_vec_ortho( u, hitp->hit_normal );
	VCROSS( v, hitp->hit_normal, u );

	/* find the second fundamental form */
	MAT4X3VEC( tmp, M, u );
	a = VDOT(u, tmp) * scale;
	b = VDOT(v, tmp) * scale;
	MAT4X3VEC( tmp, M, v );
	c = VDOT(v, tmp) * scale;

	bn_eigen2x2( &cvp->crv_c1, &cvp->crv_c2, vec1, vec2, a, b, c );
	VCOMB2( cvp->crv_pdir, vec1[X], u, vec1[Y], v );
	VUNITIZE( cvp->crv_pdir );
}

/**
 *			R T _ T G C _ T E S S
 *
 *  Tesselation of the TGC.
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */

struct tgc_pts
{
	point_t		pt;
	vect_t		tan_axb;
	struct vertex	*v;
	char		dont_use;
};

#define	MAX_RATIO	10.0	/* maximum allowed height-to-width ration for triangles */

/* version using tolerances */
int
rt_tgc_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	struct shell		*s;		/* shell to hold facetted TGC */
	struct faceuse		*fu,*fu_top,*fu_base;
	struct rt_tgc_internal	*tip;
	fastf_t			radius;		/* bounding sphere radius */
	fastf_t			max_radius,min_radius; /* max/min of a,b,c,d */
	fastf_t			h,a,b,c,d;	/* lengths of TGC vectors */
	fastf_t			inv_length;	/* 1.0/length of a vector */
	vect_t			unit_a,unit_b,unit_c,unit_d; /* units vectors in a,b,c,d directions */
	fastf_t			rel,abs,norm;	/* interpreted tolerances */
	fastf_t			alpha_tol;	/* final tolerance for ellipse parameter */
	fastf_t			abs_tol;	/* handle invalid ttol->abs */
	int			nells;		/* total number of ellipses */
	int			nsegs;		/* number of vertices/ellipse */
	vect_t			*A;		/* array of A vectors for ellipses */
	vect_t			*B;		/* array of B vectors for ellipses */
	fastf_t			*factors;	/* array of ellipse locations along height vector */
	vect_t			vtmp;
	vect_t			normal;		/* normal vector */
	vect_t			rev_norm;	/* reverse normal */
	struct tgc_pts		**pts;		/* array of points (pts[ellipse#][seg#]) */
	struct bu_ptbl		verts;		/* table of vertices used for top and bottom faces */
	struct bu_ptbl		faces;		/* table of faceuses for nmg_gluefaces */
	struct vertex		**v[3];		/* array for making triangular faces */

	int			i;

	RT_CK_DB_INTERNAL(ip);
	tip = (struct rt_tgc_internal *)ip->idb_ptr;
	RT_TGC_CK_MAGIC(tip);

	if( ttol->abs > 0.0 && ttol->abs < tol->dist )
	{
	    bu_log( "WARNING: tesselation tolerance is %fmm while calculational tolerance is %fmm\n",
		    ttol->abs , tol->dist );
	    bu_log( "Cannot tesselate a TGC to finer tolerance than the calculational tolerance\n" );
	    abs_tol = tol->dist;
	} else {
	    abs_tol = ttol->abs;
	}

	h = MAGNITUDE( tip->h );
	a = MAGNITUDE( tip->a );
	if( 2.0*a <= tol->dist )
		a = 0.0;
	b = MAGNITUDE( tip->b );
	if( 2.0*b <= tol->dist )
		b = 0.0;
	c = MAGNITUDE( tip->c );
	if( 2.0*c <= tol->dist )
		c = 0.0;
	d = MAGNITUDE( tip->d );
	if( 2.0*d <= tol->dist )
		d = 0.0;

	if( a == 0.0 && b == 0.0 && (c == 0.0 || d == 0.0) )
	{
		bu_log( "Illegal TGC a, b, and c or d less than tolerance\n" );
		return( -1);
	}
	else if( c == 0.0 && d == 0.0 && (a == 0.0 || b == 0.0 ) )
	{
		bu_log( "Illegal TGC c, d, and a or b less than tolerance\n" );
		return( -1 );
	}

	if( a > 0.0 )
	{
		inv_length = 1.0/a;
		VSCALE( unit_a, tip->a, inv_length );
	}
	if( b > 0.0 )
	{
		inv_length = 1.0/b;
		VSCALE( unit_b, tip->b, inv_length );
	}
	if( c > 0.0 )
	{
		inv_length = 1.0/c;
		VSCALE( unit_c, tip->c, inv_length );
	}
	if( d > 0.0 )
	{
		inv_length = 1.0/d;
		VSCALE( unit_d, tip->d, inv_length );
	}

	/* get bounding sphere radius for relative tolerance */
	radius = h/2.0;
	max_radius = 0.0;
	if( a > max_radius )
		max_radius = a;
	if( b > max_radius )
		max_radius = b;
	if( c > max_radius )
		max_radius = c;
	if( d > max_radius )
		max_radius = d;

	if( max_radius > radius )
		radius = max_radius;

	min_radius = MAX_FASTF;
	if( a < min_radius && a > 0.0 )
		min_radius = a;
	if( b < min_radius && b > 0.0 )
		min_radius = b;
	if( c < min_radius && c > 0.0 )
		min_radius = c;
	if( d < min_radius && d > 0.0 )
		min_radius = d;

	if( abs_tol <= 0.0 && ttol->rel <= 0.0 && ttol->norm <= 0.0 )
	{
		/* no tolerances specified, use 10% relative tolerance */
		if( (radius * 0.2) < max_radius )
			alpha_tol = 2.0 * acos( 1.0 - 2.0 * radius * 0.1 / max_radius );
		else
			alpha_tol = bn_halfpi;
	}
	else
	{
		if( abs_tol > 0.0 )
			abs = 2.0 * acos( 1.0 - abs_tol/max_radius );
		else
			abs = bn_halfpi;

		if( ttol->rel > 0.0 )
		{
			if( ttol->rel * 2.0 * radius < max_radius )
				rel = 2.0 * acos( 1.0 - ttol->rel * 2.0 * radius/max_radius );
			else
				rel = bn_halfpi;
		}
		else
			rel = bn_halfpi;

		if( ttol->norm > 0.0 )
		{
			fastf_t norm_top,norm_bot;

			if( a<b )
				norm_bot = 2.0 * atan( tan( ttol->norm ) * (a/b) );
			else
				norm_bot = 2.0 * atan( tan( ttol->norm ) * (b/a) );

			if( c<d )
				norm_top = 2.0 * atan( tan( ttol->norm ) * (c/d) );
			else
				norm_top = 2.0 * atan( tan( ttol->norm ) * (d/c) );

			if( norm_bot < norm_top )
				norm = norm_bot;
			else
				norm = norm_top;
		}
		else
			norm = bn_halfpi;

		if( abs < rel )
			alpha_tol = abs;
		else
			alpha_tol = rel;
		if( norm < alpha_tol )
			alpha_tol = norm;
	}

	/* get number of segments per quadrant */
	nsegs = (int)(bn_halfpi / alpha_tol + 0.9999);
	if( nsegs < 2 )
		nsegs = 2;

	/* and for complete ellipse */
	nsegs *= 4;

	/* get nunber and placement of intermediate ellipses */
	{
		fastf_t ratios[4],max_ratio;
		fastf_t new_ratio = 0;
		int which_ratio;
		fastf_t len_ha,len_hb;
		vect_t ha,hb;
		fastf_t ang;
		fastf_t sin_ang,cos_ang,cos_m_1_sq, sin_sq;
		fastf_t len_A, len_B, len_C, len_D;
		int bot_ell=0, top_ell=1;
		int reversed=0;

		nells = 2;

		max_ratio = MAX_RATIO + 1.0;

		factors = (fastf_t *)bu_malloc( nells*sizeof( fastf_t ), "factors" );
		A = (vect_t *)bu_malloc( nells*sizeof( vect_t ), "A vectors" );
		B = (vect_t *)bu_malloc( nells*sizeof( vect_t ), "B vectors" );

		factors[bot_ell] = 0.0;
		factors[top_ell] = 1.0;
		VMOVE( A[bot_ell], tip->a );
		VMOVE( A[top_ell], tip->c );
		VMOVE( B[bot_ell], tip->b );
		VMOVE( B[top_ell], tip->d );

		/* make sure that AxB points in the general direction of H */
		VCROSS( vtmp , A[0] , B[0] );
		if( VDOT( vtmp , tip->h ) < 0.0 )
		{
			VMOVE( A[bot_ell], tip->b );
			VMOVE( A[top_ell], tip->d );
			VMOVE( B[bot_ell], tip->a );
			VMOVE( B[top_ell], tip->c );
			reversed = 1;
		}
		ang = 2.0*bn_pi/((double)nsegs);
		sin_ang = sin( ang );
		cos_ang = cos( ang );
		cos_m_1_sq = (cos_ang - 1.0)*(cos_ang - 1.0);
		sin_sq = sin_ang*sin_ang;

		VJOIN2( ha, tip->h, 1.0, tip->c, -1.0, tip->a )
		VJOIN2( hb, tip->h, 1.0, tip->d, -1.0, tip->b )
		len_ha = MAGNITUDE( ha );
		len_hb = MAGNITUDE( hb );

		while( max_ratio > MAX_RATIO )
		{
			fastf_t tri_width;

			len_A = MAGNITUDE( A[bot_ell] );
			if( 2.0*len_A <= tol->dist )
				len_A = 0.0;
			len_B = MAGNITUDE( B[bot_ell] );
			if( 2.0*len_B <= tol->dist )
				len_B = 0.0;
			len_C = MAGNITUDE( A[top_ell] );
			if( 2.0*len_C <= tol->dist )
				len_C = 0.0;
			len_D = MAGNITUDE( B[top_ell] );
			if( 2.0*len_D <= tol->dist )
				len_D = 0.0;

			if( (len_B > 0.0 && len_D > 0.0) ||
				(len_B > 0.0 && (len_D == 0.0 && len_C == 0.0 )) )
			{
				tri_width = sqrt( cos_m_1_sq*len_A*len_A + sin_sq*len_B*len_B );
				ratios[0] = (factors[top_ell] - factors[bot_ell])*len_ha
						/tri_width;
			}
			else
				ratios[0] = 0.0;

			if( (len_A > 0.0 && len_C > 0.0) ||
				( len_A > 0.0 && (len_C == 0.0 && len_D == 0.0)) )
			{
				tri_width = sqrt( sin_sq*len_A*len_A + cos_m_1_sq*len_B*len_B );
				ratios[1] = (factors[top_ell] - factors[bot_ell])*len_hb
						/tri_width;
			}
			else
				ratios[1] = 0.0;

			if( (len_D > 0.0 && len_B > 0.0) ||
				(len_D > 0.0 && (len_A == 0.0 && len_B == 0.0)) )
			{
				tri_width = sqrt( cos_m_1_sq*len_C*len_C + sin_sq*len_D*len_D );
				ratios[2] = (factors[top_ell] - factors[bot_ell])*len_ha
						/tri_width;
			}
			else
				ratios[2] = 0.0;

			if( (len_C > 0.0 && len_A > 0.0) ||
				(len_C > 0.0 && (len_A == 0.0 && len_B == 0.0)) )
			{
				tri_width = sqrt( sin_sq*len_C*len_C + cos_m_1_sq*len_D*len_D );
				ratios[3] = (factors[top_ell] - factors[bot_ell])*len_hb
						/tri_width;
			}
			else
				ratios[3] = 0.0;

			which_ratio = -1;
			max_ratio = 0.0;

			for( i=0 ; i<4 ; i++ )
			{
				if( ratios[i] > max_ratio )
				{
					max_ratio = ratios[i];
					which_ratio = i;
				}
			}

			if( len_A == 0.0 && len_B == 0.0 && len_C == 0.0 && len_D == 0.0 )
			{
				if( top_ell == nells - 1 )
				{
					VMOVE( A[top_ell-1], A[top_ell] )
					VMOVE( B[top_ell-1], A[top_ell] )
					factors[top_ell-1] = factors[top_ell];
				}
				else if( bot_ell == 0 )
				{
					for( i=0 ; i<nells-1 ; i++ )
					{
						VMOVE( A[i], A[i+1] )
						VMOVE( B[i], B[i+1] )
						factors[i] = factors[i+1];
					}
				}

				nells -= 1;
				break;
			}

			if( max_ratio <= MAX_RATIO )
				break;

			if( which_ratio == 0 || which_ratio == 1 )
			{
				new_ratio = MAX_RATIO/max_ratio;
				if( bot_ell == 0 && new_ratio > 0.5 )
					new_ratio = 0.5;
			}
			else if( which_ratio == 2 || which_ratio == 3 )
			{
				new_ratio = 1.0 - MAX_RATIO/max_ratio;
				if( top_ell == nells - 1 && new_ratio < 0.5 )
					new_ratio = 0.5;
			}
			else	/* no MAX??? */
			{
				bu_log( "rt_tgc_tess: Should never get here!!\n" );
				rt_bomb( "rt_tgc_tess: Should never get here!!\n" );
			}

			nells++;
			factors = (fastf_t *)bu_realloc( factors, nells*sizeof( fastf_t ), "factors" );
			A = (vect_t *)bu_realloc( A, nells*sizeof( vect_t ), "A vectors" );
			B = (vect_t *)bu_realloc( B, nells*sizeof( vect_t ), "B vectors" );

			for( i=nells-1 ; i>top_ell ; i-- )
			{
				factors[i] = factors[i-1];
				VMOVE( A[i], A[i-1] )
				VMOVE( B[i], B[i-1] )
			}

			factors[top_ell] = factors[bot_ell] +
				new_ratio*(factors[top_ell+1] - factors[bot_ell]);

			if( reversed )
			{
				VBLEND2( A[top_ell], (1.0-factors[top_ell]), tip->b, factors[top_ell], tip->d )
				VBLEND2( B[top_ell], (1.0-factors[top_ell]), tip->a, factors[top_ell], tip->c )
			}
			else
			{
				VBLEND2( A[top_ell], (1.0-factors[top_ell]), tip->a, factors[top_ell], tip->c )
				VBLEND2( B[top_ell], (1.0-factors[top_ell]), tip->b, factors[top_ell], tip->d )
			}

			if( which_ratio == 0 || which_ratio == 1 )
			{
				top_ell++;
				bot_ell++;
			}

		}

	}

	/* get memory for points */
	pts = (struct tgc_pts **)bu_calloc( nells , sizeof( struct tgc_pts *) , "rt_tgc_tess: pts" );
	for( i=0 ; i<nells ; i++ )
		pts[i] = (struct tgc_pts *)bu_calloc( nsegs , sizeof( struct tgc_pts ) , "rt_tgc_tess: pts" );

	/* calculate geometry for points */
	for( i=0 ; i<nells ; i++ )
	{
		fastf_t h_factor;
		int j;

		h_factor = factors[i];
		for( j=0 ; j<nsegs ; j++ )
		{
			double alpha;
			double sin_alpha,cos_alpha;

			alpha = bn_twopi * (double)(2*j+1)/(double)(2*nsegs);
			sin_alpha = sin( alpha );
			cos_alpha = cos( alpha );

			/* vertex geometry */
			if( i == 0 && a == 0.0 && b == 0.0 )
				VMOVE( pts[i][j].pt , tip->v )
			else if( i == nells-1 && c == 0.0 && d == 0.0 )
				VADD2( pts[i][j].pt, tip->v, tip->h )
			else
				VJOIN3( pts[i][j].pt , tip->v , h_factor , tip->h , cos_alpha , A[i] , sin_alpha , B[i] )

			/* Storing the tangent here while sines and cosines are available */
			if( i == 0 && a == 0.0 && b == 0.0 )
				VCOMB2( pts[0][j].tan_axb, -sin_alpha, unit_c, cos_alpha, unit_d )
			else if( i == nells-1 && c == 0.0 && d == 0.0 )
				VCOMB2( pts[i][j].tan_axb, -sin_alpha, unit_a, cos_alpha, unit_b )
			else
				VCOMB2( pts[i][j].tan_axb , -sin_alpha , A[i] , cos_alpha , B[i] )
		}
	}

	/* make sure no edges will be created with length < tol->dist */
	for( i=0 ; i<nells ; i++ )
	{
		int j;
		point_t curr_pt;
		vect_t edge_vect;

		if( i == 0 && (a == 0.0 || b == 0.0) )
			continue;
		else if( i == nells-1 && (c == 0.0 || d == 0.0) )
			continue;

		VMOVE( curr_pt, pts[i][0].pt )
		for( j=1 ; j<nsegs ; j++ )
		{
			fastf_t edge_len_sq;

			VSUB2( edge_vect, curr_pt, pts[i][j].pt )
			edge_len_sq = MAGSQ( edge_vect );
			if(edge_len_sq > tol->dist_sq )
				VMOVE( curr_pt, pts[i][j].pt )
			else
			{
				/* don't use this point, it will create a too short edge */
				pts[i][j].dont_use = 'n';
			}
		}
	}

	/* make region, shell, vertex */
	*r = nmg_mrsv( m );
	s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	bu_ptbl_init( &verts , 64, " &verts ");
	bu_ptbl_init( &faces , 64, " &faces ");
	/* Make bottom face */
	if( a > 0.0 && b > 0.0 )
	{
		for( i=nsegs-1 ; i>=0 ; i-- ) /* reverse order to get outward normal */
		{
			if( !pts[0][i].dont_use )
				bu_ptbl_ins( &verts , (long *)&pts[0][i].v );
		}

		if( BU_PTBL_END( &verts ) > 2 )
		{
			fu_base = nmg_cmface( s , (struct vertex ***)BU_PTBL_BASEADDR( &verts ), BU_PTBL_END( &verts ) );
			bu_ptbl_ins( &faces , (long *)fu_base );
		}
		else
			fu_base = (struct faceuse *)NULL;
	}
	else
		fu_base = (struct faceuse *)NULL;


	/* Make top face */
	if( c > 0.0 && d > 0.0 )
	{
		bu_ptbl_reset( &verts );
		for( i=0 ; i<nsegs ; i++ )
		{
			if( !pts[nells-1][i].dont_use )
				bu_ptbl_ins( &verts , (long *)&pts[nells-1][i].v );
		}

		if( BU_PTBL_END( &verts ) > 2 )
		{
			fu_top = nmg_cmface( s , (struct vertex ***)BU_PTBL_BASEADDR( &verts ), BU_PTBL_END( &verts ) );
			bu_ptbl_ins( &faces , (long *)fu_top );
		}
		else
			fu_top = (struct faceuse *)NULL;
	}
	else
		fu_top = (struct faceuse *)NULL;

	/* Free table of vertices */
	bu_ptbl_free( &verts );

	/* Make triangular faces */
	for( i=0 ; i<nells-1 ; i++ )
	{
		int j;
		struct vertex **curr_top;
		struct vertex **curr_bot;

		curr_bot = &pts[i][0].v;
		curr_top = &pts[i+1][0].v;
		for( j=0 ; j<nsegs ; j++ )
		{
			int k;

			k = j+1;
			if( k == nsegs )
				k = 0;
			if( i != 0 || a > 0.0 || b > 0.0 )
			{
				if( !pts[i][k].dont_use )
				{
					v[0] = curr_bot;
					v[1] = &pts[i][k].v;
					if( i+1 == nells-1 && c == 0.0 && d == 0.0 )
						v[2] = &pts[i+1][0].v;
					else
						v[2] = curr_top;
					fu = nmg_cmface( s , v , 3 );
					bu_ptbl_ins( &faces , (long *)fu );
					curr_bot = &pts[i][k].v;
				}
			}

			if( i != nells-2 || c > 0.0 || d > 0.0 )
			{
				if( !pts[i+1][k].dont_use )
				{
					v[0] = &pts[i+1][k].v;
					v[1] = curr_top;
					if( i == 0 && a == 0.0 && b == 0.0 )
						v[2] = &pts[i][0].v;
					else
						v[2] = curr_bot;
					fu = nmg_cmface( s , v , 3 );
					bu_ptbl_ins( &faces , (long *)fu );
					curr_top = &pts[i+1][k].v;
				}
			}
		}
	}

	/* Assign geometry */
	for( i=0 ; i<nells ; i++ )
	{
		int j;

		for( j=0 ; j<nsegs ; j++ )
		{
			point_t pt_geom;
			double alpha;
			double sin_alpha,cos_alpha;

			alpha = bn_twopi * (double)(2*j+1)/(double)(2*nsegs);
			sin_alpha = sin( alpha );
			cos_alpha = cos( alpha );

			/* vertex geometry */
			if( i == 0 && a == 0.0 && b == 0.0 )
			{
				if( j == 0 )
					nmg_vertex_gv( pts[0][0].v, tip->v );
			}
			else if( i == nells-1 && c == 0.0 && d == 0.0 )
			{
				if( j == 0 )
				{
					VADD2( pt_geom, tip->v, tip->h );
					nmg_vertex_gv( pts[i][0].v , pt_geom );
				}
			}
			else if( pts[i][j].v )
				nmg_vertex_gv( pts[i][j].v , pts[i][j].pt );

			/* Storing the tangent here while sines and cosines are available */
			if( i == 0 && a == 0.0 && b == 0.0 )
				VCOMB2( pts[0][j].tan_axb, -sin_alpha, unit_c, cos_alpha, unit_d )
			else if( i == nells-1 && c == 0.0 && d == 0.0 )
				VCOMB2( pts[i][j].tan_axb, -sin_alpha, unit_a, cos_alpha, unit_b )
			else
				VCOMB2( pts[i][j].tan_axb , -sin_alpha , A[i] , cos_alpha , B[i] )
		}
	}

	/* Associate face plane equations */
	for( i=0 ; i<BU_PTBL_END( &faces ) ; i++ )
	{
		fu = (struct faceuse *)BU_PTBL_GET( &faces , i );
		NMG_CK_FACEUSE( fu );

		if( nmg_calc_face_g( fu ) )
		{
			bu_log( "rt_tess_tgc: failed to calculate plane equation\n" );
			nmg_pr_fu_briefly( fu, "" );
			return( -1 );
		}
	}


	/* Calculate vertexuse normals */
	for( i=0 ; i<nells ; i++ )
	{
		int j,k;

		k = i + 1;
		if( k == nells )
			k = i - 1;

		for( j=0 ; j<nsegs ; j++ )
		{
			vect_t tan_h;		/* vector tangent from one ellipse to next */
			struct vertexuse *vu;

			/* normal at vertex */
			if( i == nells - 1 )
			{
				if( c == 0.0 && d == 0.0 )
					VSUB2( tan_h , pts[i][0].pt , pts[k][j].pt )
				else if( k == 0 && c == 0.0 && d == 0.0 )
					VSUB2( tan_h , pts[i][j].pt , pts[k][0].pt )
				else
					VSUB2( tan_h , pts[i][j].pt , pts[k][j].pt )
			}
			else if( i == 0 )
			{
				if( a == 0.0 && b == 0.0 )
					VSUB2( tan_h , pts[k][j].pt , pts[i][0].pt )
				else if( k == nells-1 && c == 0.0 && d == 0.0 )
					VSUB2( tan_h , pts[k][0].pt , pts[i][j].pt )
				else
					VSUB2( tan_h , pts[k][j].pt , pts[i][j].pt )
			}
			else if( k == 0 && a == 0.0 && b == 0.0 )
				VSUB2( tan_h , pts[k][0].pt , pts[i][j].pt )
			else if( k == nells-1 && c == 0.0 && d == 0.0 )
				VSUB2( tan_h , pts[k][0].pt , pts[i][j].pt )
			else
				VSUB2( tan_h , pts[k][j].pt , pts[i][j].pt )

			VCROSS( normal , pts[i][j].tan_axb , tan_h );
			VUNITIZE( normal );
			VREVERSE( rev_norm , normal );

			if( !(i == 0 && a == 0.0 && b == 0.0) &&
			    !(i == nells-1 && c == 0.0 && d == 0.0 ) &&
			      pts[i][j].v )
			{
				for( BU_LIST_FOR( vu , vertexuse , &pts[i][j].v->vu_hd ) )
				{
					NMG_CK_VERTEXUSE( vu );

					fu = nmg_find_fu_of_vu( vu );
					NMG_CK_FACEUSE( fu );

					/* don't need vertexuse normals for faces that are really flat */
					if( fu == fu_base || fu->fumate_p == fu_base ||
					    fu == fu_top  || fu->fumate_p == fu_top )
						continue;

					if( fu->orientation == OT_SAME )
						nmg_vertexuse_nv( vu , normal );
					else if( fu->orientation == OT_OPPOSITE )
						nmg_vertexuse_nv( vu , rev_norm );
				}
			}
		}
	}

	/* Finished with storage, so free it */
	bu_free( (char *)factors, "rt_tgc_tess: factors" );
	bu_free( (char *)A , "rt_tgc_tess: A" );
	bu_free( (char *)B , "rt_tgc_tess: B" );
	for( i=0 ; i<nells ; i++ )
		bu_free( (char *)pts[i] , "rt_tgc_tess: pts[i]" );
	bu_free( (char *)pts , "rt_tgc_tess: pts" );

	/* mark real edges for top and bottom faces */
	for( i=0 ; i<2 ; i++ )
	{
		struct loopuse *lu;

		if( i == 0 )
			fu = fu_base;
		else
			fu = fu_top;

		if( fu == (struct faceuse *)NULL )
			continue;

		NMG_CK_FACEUSE( fu );

		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				continue;

			for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct edge *e;

				NMG_CK_EDGEUSE( eu );
				e = eu->e_p;
				NMG_CK_EDGE( e );
				e->is_real = 1;
			}
		}
	}

	nmg_region_a( *r , tol );

	/* glue faces together */
	nmg_gluefaces( (struct faceuse **)BU_PTBL_BASEADDR( &faces) , BU_PTBL_END( &faces ), tol );
	bu_ptbl_free( &faces );

	return( 0 );
}

/**	R T _ T G C _ T N U R B
 *
 *  "Tessellate an TGC into a trimmed-NURB-NMG data structure.
 *  Computing NRUB surfaces and trimming curves to interpolate
 *  the parameters of the TGC
 *
 *  The process is to create the nmg  topology of the TGC fill it
 *  in with a unit cylinder geometry (i.e. unitcircle at the top (0,0,1)
 *  unit cylinder of radius 1, and unitcirlce at the bottom), and then
 *  scale it with a perspective matrix derived from the parameters of the
 *  tgc. The result is three trimmed nub surfaces which interpolate the
 *  parameters of  the original TGC.
 *
 *  Returns -
 *	-1 	failure
 *	0	OK. *r points to nmgregion that holds this tesselation
 */

int
rt_tgc_tnurb(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bn_tol *tol)
{
	LOCAL struct rt_tgc_internal	*tip;

	struct shell			*s;
	struct vertex			*verts[2];
	struct vertex			**vertp[4];
	struct faceuse			* top_fu;
	struct faceuse			* cyl_fu;
	struct faceuse			* bot_fu;
	vect_t				uvw;
	struct loopuse			*lu;
	struct edgeuse			*eu;
	struct edgeuse			*top_eu;
	struct edgeuse			*bot_eu;

	mat_t 				mat;
	mat_t 				imat, omat, top_mat, bot_mat;
	vect_t 				anorm;
	vect_t 				bnorm;
	vect_t 				cnorm;


	/* Get the internal representation of the tgc */

	RT_CK_DB_INTERNAL(ip);
	tip = (struct rt_tgc_internal *) ip->idb_ptr;
	RT_TGC_CK_MAGIC(tip);

	/* Create the NMG Topology */

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = BU_LIST_FIRST( shell, &(*r)->s_hd);


	/* Create transformation matrix  for the top cap surface*/

	MAT_IDN( omat );
	MAT_IDN( mat);

	omat[0] = MAGNITUDE(tip->c);
	omat[5] = MAGNITUDE(tip->d);
	omat[3] = tip->v[0] + tip->h[0];
	omat[7] = tip->v[1] + tip->h[1];
	omat[11] = tip->v[2] + tip->h[2];

	bn_mat_mul(imat, mat, omat);

        VMOVE(anorm, tip->c);
        VMOVE(bnorm, tip->d);
        VCROSS(cnorm, tip->c, tip->d);
        VUNITIZE(anorm);
        VUNITIZE(bnorm);
        VUNITIZE(cnorm);

        MAT_IDN( omat );

        VMOVE( &omat[0], anorm);
        VMOVE( &omat[4], bnorm);
        VMOVE( &omat[8], cnorm);


	bn_mat_mul(top_mat, omat, imat);

	/* Create topology for top cap surface */

	verts[0] = verts[1] = NULL;
	vertp[0] = &verts[0];
	top_fu = nmg_cmface(s, vertp, 1);

	lu = BU_LIST_FIRST( loopuse, &top_fu->lu_hd);
	NMG_CK_LOOPUSE(lu);
	eu= BU_LIST_FIRST( edgeuse, &lu->down_hd);
	NMG_CK_EDGEUSE(eu);
	top_eu = eu;

	VSET( uvw, 0,0,0);
	nmg_vertexuse_a_cnurb( eu->vu_p, uvw);
	VSET( uvw, 1, 0, 0);
	nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, uvw );
	eu = BU_LIST_NEXT( edgeuse, &eu->l);

	/* Top cap surface */
	nmg_tgc_disk( top_fu, top_mat, 0.0, 0 );

	/* Create transformation matrix  for the bottom cap surface*/

	MAT_IDN( omat );
	MAT_IDN( mat);

	omat[0] = MAGNITUDE(tip->a);
	omat[5] = MAGNITUDE(tip->b);
        omat[3] = tip->v[0];
        omat[7] = tip->v[1];
        omat[11] = tip->v[2];

        bn_mat_mul(imat, mat, omat);

        VMOVE(anorm, tip->a);
        VMOVE(bnorm, tip->b);
        VCROSS(cnorm, tip->a, tip->b);
        VUNITIZE(anorm);
        VUNITIZE(bnorm);
        VUNITIZE(cnorm);

        MAT_IDN( omat );

        VMOVE( &omat[0], anorm);
        VMOVE( &omat[4], bnorm);
        VMOVE( &omat[8], cnorm);

        bn_mat_mul(bot_mat, omat, imat);

	/* Create topology for bottom cap surface */

	vertp[0] = &verts[1];
	bot_fu = nmg_cmface(s, vertp, 1);

	lu = BU_LIST_FIRST( loopuse, &bot_fu->lu_hd);
	NMG_CK_LOOPUSE(lu);
	eu= BU_LIST_FIRST( edgeuse, &lu->down_hd);
	NMG_CK_EDGEUSE(eu);
	bot_eu = eu;

	VSET( uvw, 0,0,0);
	nmg_vertexuse_a_cnurb( eu->vu_p, uvw);
	VSET( uvw, 1, 0, 0);
	nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, uvw );


	nmg_tgc_disk( bot_fu, bot_mat, 0.0, 1 );

	/* Create topology for cylinder surface  */

	vertp[0] = &verts[0];
	vertp[1] = &verts[0];
	vertp[2] = &verts[1];
	vertp[3] = &verts[1];
	cyl_fu = nmg_cmface(s, vertp, 4);

	nmg_tgc_nurb_cyl( cyl_fu, top_mat, bot_mat);

	/* fuse top cylinder edge to matching edge on body of cylinder */
	lu = BU_LIST_FIRST( loopuse, &cyl_fu->lu_hd);

	eu= BU_LIST_FIRST( edgeuse, &lu->down_hd);

	nmg_je( top_eu, eu );

	eu= BU_LIST_LAST( edgeuse, &lu->down_hd);
	eu= BU_LIST_LAST( edgeuse, &eu->l);

	nmg_je( bot_eu, eu );
	nmg_region_a( *r,tol);

	return( 0 );
}


#define RAT  .707107

fastf_t nmg_tgc_unitcircle[36] = {
	1.0, 0.0, 0.0, 1.0,
	RAT, -RAT, 0.0, RAT,
	0.0, -1.0, 0.0, 1.0,
	-RAT, -RAT, 0.0, RAT,
	-1.0, 0.0, 0.0, 1.0,
	-RAT, RAT, 0.0, RAT,
	0.0, 1.0, 0.0, 1.0,
	RAT, RAT, 0.0, RAT,
	1.0, 0.0, 0.0, 1.0
};

fastf_t nmg_uv_unitcircle[27] = {
	1.0,   .5,  1.0,
	RAT,  RAT,  RAT,
	.5,   1.0,  1.0,
	0.0,  RAT,  RAT,
	0.0,   .5,  1.0,
	0.0,  0.0,  RAT,
	.5,   0.0,  1.0,
	RAT,  0.0,  RAT,
	1.0,   .5,  1.0
};

static void
nmg_tgc_disk(struct faceuse *fu, fastf_t *rmat, fastf_t height, int flip)
{
	struct face_g_snurb 	* fg;
	struct loopuse		* lu;
	struct edgeuse		* eu;
	struct edge_g_cnurb	* eg;
	fastf_t	*mptr;
	int i;
	vect_t	vect;
	point_t	point;

	nmg_face_g_snurb( fu,
		2, 2,  			/* u,v order */
		4, 4,			/* number of knots */
		NULL, NULL, 		/* initial knot vectors */
		2, 2, 			/* n_rows, n_cols */
		RT_NURB_MAKE_PT_TYPE(3, 2, 0),
		NULL );			/* Initial mesh */

	fg = fu->f_p->g.snurb_p;


	fg->u.knots[0] = 0.0;
	fg->u.knots[1] = 0.0;
	fg->u.knots[2] = 1.0;
	fg->u.knots[3] = 1.0;

	fg->v.knots[0] = 0.0;
	fg->v.knots[1] = 0.0;
	fg->v.knots[2] = 1.0;
	fg->v.knots[3] = 1.0;

	if(flip)
	{
		fg->ctl_points[0] = 1.;
		fg->ctl_points[1] = -1.;
		fg->ctl_points[2] = height;

		fg->ctl_points[3] = -1;
		fg->ctl_points[4] = -1.;
		fg->ctl_points[5] = height;

		fg->ctl_points[6] = 1.;
		fg->ctl_points[7] = 1.;
		fg->ctl_points[8] = height;

		fg->ctl_points[9] = -1.;
		fg->ctl_points[10] = 1.;
		fg->ctl_points[11] = height;
	} else
	{

		fg->ctl_points[0] = -1.;
		fg->ctl_points[1] = -1.;
		fg->ctl_points[2] = height;

		fg->ctl_points[3] = 1;
		fg->ctl_points[4] = -1.;
		fg->ctl_points[5] = height;

		fg->ctl_points[6] = -1.;
		fg->ctl_points[7] = 1.;
		fg->ctl_points[8] = height;

		fg->ctl_points[9] = 1.;
		fg->ctl_points[10] = 1.;
		fg->ctl_points[11] = height;
	}

	/* multiple the matrix to get orientation and scaling that we want */
	mptr = fg->ctl_points;

	i = fg->s_size[0] * fg->s_size[1];

        for( ; i> 0; i--)
        {
		MAT4X3PNT(vect,rmat,mptr);
                mptr[0] = vect[0];
                mptr[1] = vect[1];
                mptr[2] = vect[2];
                mptr += 3;
        }

	lu = BU_LIST_FIRST( loopuse, &fu->lu_hd);
	NMG_CK_LOOPUSE(lu);
	eu= BU_LIST_FIRST( edgeuse, &lu->down_hd);
	NMG_CK_EDGEUSE(eu);


	if(!flip)
	{
		rt_nurb_s_eval( fu->f_p->g.snurb_p,
			nmg_uv_unitcircle[0], nmg_uv_unitcircle[1], point );
		nmg_vertex_gv( eu->vu_p->v_p, point );
	} else
	{
		rt_nurb_s_eval( fu->f_p->g.snurb_p,
			nmg_uv_unitcircle[12], nmg_uv_unitcircle[13], point );
		nmg_vertex_gv( eu->vu_p->v_p, point );
	}

	nmg_edge_g_cnurb(eu, 3, 12, NULL, 9, RT_NURB_MAKE_PT_TYPE(3,3,1),
		NULL);

	eg = eu->g.cnurb_p;
	eg->order = 3;

	eg->k.knots[0] = 0.0;
	eg->k.knots[1] = 0.0;
	eg->k.knots[2] = 0.0;
	eg->k.knots[3] = .25;
	eg->k.knots[4] = .25;
	eg->k.knots[5] = .5;
	eg->k.knots[6] = .5;
	eg->k.knots[7] = .75;
	eg->k.knots[8] = .75;
	eg->k.knots[9] = 1.0;
	eg->k.knots[10] = 1.0;
	eg->k.knots[11] = 1.0;

	if( !flip )
	{
		for( i = 0; i < 27; i++)
			eg->ctl_points[i] = nmg_uv_unitcircle[i];
	}
	else
	{

		VSET(&eg->ctl_points[0], 0.0, .5, 1.0);
		VSET(&eg->ctl_points[3], 0.0, 0.0, RAT);
		VSET(&eg->ctl_points[6], 0.5, 0.0, 1.0);
		VSET(&eg->ctl_points[9], RAT, 0.0, RAT);
		VSET(&eg->ctl_points[12], 1.0, .5, 1.0);
		VSET(&eg->ctl_points[15], RAT,RAT, RAT);
		VSET(&eg->ctl_points[18], .5, 1.0, 1.0);
		VSET(&eg->ctl_points[21], 0.0, RAT, RAT);
		VSET(&eg->ctl_points[24], 0.0, .5, 1.0);
	}
}

/* Create a cylinder with a top surface and a bottom surfce
 * defined by the ellipsods at the top and bottom of the
 * cylinder, the top_mat, and bot_mat are applied to a unit circle
 * for the top row of the surface and the bot row of the surface
 * respectively.
 */
static void
nmg_tgc_nurb_cyl(struct faceuse *fu, fastf_t *top_mat, fastf_t *bot_mat)
{
	struct face_g_snurb 	* fg;
	struct loopuse		* lu;
	struct edgeuse		* eu;
	fastf_t		* mptr;
	int		i;
	hvect_t		vect;
	point_t		uvw;
	point_t		point;
	hvect_t		hvect;

	nmg_face_g_snurb( fu,
		3, 2,
		12, 4,
		NULL, NULL,
		2, 9,
		RT_NURB_MAKE_PT_TYPE(4,3,1),
		NULL );

	fg = fu->f_p->g.snurb_p;

	fg->v.knots[0] = 0.0;
	fg->v.knots[1] = 0.0;
	fg->v.knots[2] = 1.0;
	fg->v.knots[3] = 1.0;

	fg->u.knots[0] = 0.0;
	fg->u.knots[1] = 0.0;
	fg->u.knots[2] = 0.0;
	fg->u.knots[3] = .25;
	fg->u.knots[4] = .25;
	fg->u.knots[5] = .5;
	fg->u.knots[6] = .5;
	fg->u.knots[7] = .75;
	fg->u.knots[8] = .75;
	fg->u.knots[9] = 1.0;
	fg->u.knots[10] = 1.0;
	fg->u.knots[11] = 1.0;

	mptr = fg->ctl_points;

	for(i = 0; i < 9; i++)
	{
		MAT4X4PNT(vect, top_mat, &nmg_tgc_unitcircle[i*4]);
		mptr[0] = vect[0];
		mptr[1] = vect[1];
		mptr[2] = vect[2];
		mptr[3] = vect[3];
		mptr += 4;
	}

	for(i = 0; i < 9; i++)
	{
		MAT4X4PNT(vect, bot_mat, &nmg_tgc_unitcircle[i*4]);
		mptr[0] = vect[0];
		mptr[1] = vect[1];
		mptr[2] = vect[2];
		mptr[3] = vect[3];
		mptr += 4;
	}

	/* Assign edgeuse parameters & vertex geometry */

	lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
	NMG_CK_LOOPUSE(lu);
	eu = BU_LIST_FIRST( edgeuse, &lu->down_hd);
	NMG_CK_EDGEUSE(eu);

	/* March around the fu's loop assigning uv parameter values */

	rt_nurb_s_eval( fg, 0.0, 0.0, hvect);
	HDIVIDE( point, hvect );
	nmg_vertex_gv( eu->vu_p->v_p, point );	/* 0,0 vertex */

	VSET( uvw, 0, 0, 0);
	nmg_vertexuse_a_cnurb( eu->vu_p, uvw );
	VSET( uvw, 1, 0, 0);
	nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, uvw );
	eu = BU_LIST_NEXT( edgeuse, &eu->l);

	VSET( uvw, 1, 0, 0);
	nmg_vertexuse_a_cnurb( eu->vu_p, uvw );
	VSET( uvw, 1, 1, 0);
	nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, uvw );
	eu = BU_LIST_NEXT( edgeuse, &eu->l);

	VSET( uvw, 1, 1, 0);
	nmg_vertexuse_a_cnurb( eu->vu_p, uvw );
	VSET( uvw, 0, 1, 0);
	nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, uvw );

	rt_nurb_s_eval( fg, 1., 1., hvect);
	HDIVIDE( point, hvect);
	nmg_vertex_gv( eu->vu_p->v_p, point );		/* 4,1 vertex */

	eu = BU_LIST_NEXT( edgeuse, &eu->l);

	VSET( uvw, 0, 1, 0);
	nmg_vertexuse_a_cnurb( eu->vu_p, uvw );
	VSET( uvw, 0, 0, 0);
	nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, uvw );
	eu = BU_LIST_NEXT( edgeuse, &eu->l);

	/* Create the edge loop geometry */

	lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
	NMG_CK_LOOPUSE(lu);
	eu = BU_LIST_FIRST( edgeuse, &lu->down_hd);
	NMG_CK_EDGEUSE(eu);

	for( i = 0; i < 4; i++)
	{
		nmg_edge_g_cnurb_plinear(eu);
		eu = BU_LIST_NEXT(edgeuse, &eu->l);
	}
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
