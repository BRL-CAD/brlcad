/*
 *			G _ E T O . C
 *
 * Purpose -
 *	Intersect a ray with an Elliptical Torus
 *
 * Authors -
 *	Michael Markowski	(Programming)
 *	ERIM GIFT code		(ETO Eqn)
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1992 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSeto[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

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

/*
 * The ETO has the following input fields:
 *	V	V from origin to center.
 *	N	Normal to plane of eto.
 *	C	Semi-major axis of elliptical cross section.
 *      r	Radius of revolution.
 *	rd	Semi-minor axis of elliptical cross section.
 *
 */

/*
 *  Algorithm:
 *  
 *  Given V, N, C, r, and rd, there is a set of points on this eto
 *  
 *  { (x,y,z) | (x,y,z) is on eto defined by V, N, C, r, rd }
 *  
 *  Through a series of  Transformations, this set will be
 *  transformed into a set of points on an eto centered at the origin
 *  which lies on the X-Y plane (ie, N is on the Z axis).
 *  
 *  { (x',y',z') | (x',y',z') is an eto at origin }
 *  
 *  The transformation from X to X' is accomplished by:
 *  
 *  X' = R( X - V )
 *  
 *  where R(X) =  ( B/(|B|) )
 *  		 (  A/(|A|)  ) . X
 *  		  ( N/(|N|) )
 *
 *  To find the intersection of a line with the eto, consider
 *  the parametric line L:
 *  
 *  	L : { P(n) | P + t(n) . D }
 *  
 *  Call W the actual point of intersection between L and the eto.
 *  Let W' be the point of intersection between L' and the unit eto.
 *  
 *  	L' : { P'(n) | P' + t(n) . D' }
 *  
 *  W = invR( W' ) + V
 *  
 *  Where W' = k D' + P'.
 *  
 *
 *  Given a line and a ratio, alpha, finds the equation of the
 *  unit eto in terms of the variable 't'.
 *
 *  Given that the equation for the eto is:
 *
 *            _______                           _______
 *           / 2    2              2           / 2    2               2
 *  [Eu(+- \/ x  + y   - R) + Ev z]  + [Fu(+-\/ x  + y   - R) + Fv z ]
 * --------------------------------    -------------------------------  = 1
 *               2                                      2
 *             Rc                                     Rd
 *
 *  First, find X, Y, and Z in terms of 't' for this line, then
 *  substitute them into the equation above.
 *
 *  	Wx = Dx*t + Px
 *
 *  	Wx**2 = Dx**2 * t**2  +  2 * Dx * Px  +  Px**2
 *
 *  The real roots of the equation in 't' are the intersect points
 *  along the parameteric line.
 *  
 *  NORMALS.  Given the point W on the eto, what is the vector
 *  normal to the tangent plane at that point?
 *  
 *  Map W onto the eto, ie:  W' = R( W - V ).
 *  In this case, we find W' by solving the parameteric line given k.
 *  
 *  The gradient of the eto at W' is in fact the
 *  normal vector.
 *
 *  For f(x,y,z) = 0, the gradient of f() is ( df/dx, df/dy, df/dz ).
 *
 *  Note that the normal vector (gradient) produced above will not have
 *  unit length.  Also, to make this useful for the original eto, it will
 *  have to be rotated back to the orientation of the original eto.
 */

struct eto_specific {
	vect_t	eto_V;		/* Vector to center of eto */
	vect_t	eto_N;		/* unit normal to plane of eto */
	vect_t	eto_C;		/* semi-major axis of ellipse */
	fastf_t	eto_r;		/* radius of revolution */
	fastf_t	eto_rc;		/* semi-major axis of ellipse */
	fastf_t	eto_rd;		/* semi-minor axis of ellipse */
	mat_t	eto_R;		/* Rot(vect) */
	mat_t	eto_invR;	/* invRot(vect') */
	fastf_t	eu, ev, fu, fv;
};

struct pt_node {
	point_t		p;	/* a point */
	struct pt_node	*next;	/* ptr to next pt */
};

/*
 *  			R T _ E T O _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid eto, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	ETO is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct eto_specific is created, and it's address is stored in
 *  	stp->st_specific for use by rt_eto_shot().
 */
int
rt_eto_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	register struct eto_specific *eto;
	LOCAL vect_t	P, w1;	/* for RPP calculation */
	LOCAL vect_t	Au, Bu, Cu, Nu;
	FAST fastf_t	ch, cv, dh, f, phi;
	struct rt_eto_internal	*tip;

	tip = (struct rt_eto_internal *)ip->idb_ptr;
	RT_ETO_CK_MAGIC(tip);

	/* Solid is OK, compute constant terms now */
	GETSTRUCT( eto, eto_specific );
	stp->st_specific = (genptr_t)eto;

	eto->eto_r = tip->eto_r;
	eto->eto_rd = tip->eto_rd;
	eto->eto_rc = MAGNITUDE( tip->eto_C );
	if ( NEAR_ZERO(eto->eto_r, 0.0001) || NEAR_ZERO(eto->eto_rd, 0.0001)
		|| NEAR_ZERO(eto->eto_rc, 0.0001)) {
		rt_log("eto(%s): r, rd, or rc zero length\n", stp->st_name);
		return(1);
	}

	VMOVE( eto->eto_V, tip->eto_V );
	VMOVE( eto->eto_N, tip->eto_N );
	VMOVE( eto->eto_C, tip->eto_C );
	VMOVE( Nu, tip->eto_N );
	VUNITIZE( Nu );		/* z axis of coord sys */
	vec_ortho( Bu, Nu );	/* x axis */
	VUNITIZE( Bu );
	VCROSS( Au, Nu, Bu );	/* y axis */
	VMOVE( Cu, tip->eto_C );
	VUNITIZE( Cu );

	/* get horizontal and vertical components of C and Rd */
	cv = VDOT( eto->eto_C, Nu );
	ch = sqrt( VDOT( eto->eto_C, eto->eto_C ) - cv * cv );
	/* angle between C and Nu */
	phi = acos( cv / eto->eto_rc );
	dh = eto->eto_rd * cos(phi);
	/* make sure ellipse doesn't overlap itself when revolved */
	if (ch > eto->eto_r || dh > eto->eto_r) {
		rt_log("eto(%s): revolved ellipse overlaps itself\n",
			stp->st_name);
		return(1);
	}
	
	eto->ev = fabs( VDOT( Cu, Nu ) );	/* vertical component of Cu */
	eto->eu = sqrt( 1.0 - eto->ev * eto->ev );	/* horiz component */
	eto->fu = -eto->ev;
	eto->fv =  eto->eu;

	/* Compute R and invR matrices */
	mat_idn( eto->eto_R );
	VMOVE( &eto->eto_R[0], Bu );
	VMOVE( &eto->eto_R[4], Au );
	VMOVE( &eto->eto_R[8], Nu );
	mat_inv( eto->eto_invR, eto->eto_R );

	stp->st_aradius = stp->st_bradius = eto->eto_r + eto->eto_rc;

	/*
	 *  Compute the bounding RPP planes for a circular eto.
	 *
	 *  Given a circular eto with vertex V, vector N, and
	 *  radii r and |C|.  A bounding plane with direction
	 *  vector P will touch the surface of the eto at the
	 *  points:  V +/- [|C| + r * |N x P|] P
	 */

	/* X */
	VSET( P, 1.0, 0, 0 );		/* bounding plane normal */
	VCROSS( w1, Nu, P );	/* for sin(angle N P) */
	f = eto->eto_rc + eto->eto_r * MAGNITUDE(w1);
	VSCALE( w1, P, f );
	f = fabs( w1[X] );
	stp->st_min[X] = eto->eto_V[X] - f;
	stp->st_max[X] = eto->eto_V[X] + f;

	/* Y */
	VSET( P, 0, 1.0, 0 );		/* bounding plane normal */
	VCROSS( w1, Nu, P );	/* for sin(angle N P) */
	f = eto->eto_rc + eto->eto_r * MAGNITUDE(w1);
	VSCALE( w1, P, f );
	f = fabs( w1[Y] );
	stp->st_min[Y] = eto->eto_V[Y] - f;
	stp->st_max[Y] = eto->eto_V[Y] + f;

	/* Z */
	VSET( P, 0, 0, 1.0 );		/* bounding plane normal */
	VCROSS( w1, Nu, P );	/* for sin(angle N P) */
	f = eto->eto_rc + eto->eto_r * MAGNITUDE(w1);
	VSCALE( w1, P, f );
	f = fabs( w1[Z] );
	stp->st_min[Z] = eto->eto_V[Z] - f;
	stp->st_max[Z] = eto->eto_V[Z] + f;

	return(0);			/* OK */
}

/*
 *			R T _ E T O _ P R I N T
 */
void
rt_eto_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct eto_specific *eto =
		(struct eto_specific *)stp->st_specific;

	VPRINT("V", eto->eto_V);
	VPRINT("N", eto->eto_N);
	VPRINT("C", eto->eto_C);
	rt_log("r = %f\n", eto->eto_r);
	rt_log("rc = %f\n", eto->eto_rc);
	rt_log("rd = %f\n", eto->eto_rd);
	mat_print("R", eto->eto_R );
	mat_print("invR", eto->eto_invR );
	rt_log( "rpp: (%g, %g, %g) to (%g, %g, %g)\n",
		stp->st_min[X], stp->st_min[Y], stp->st_min[Z], 
		stp->st_max[X], stp->st_max[Y], stp->st_max[Z]);
}

/*
 *  			R T _ E T O _ S H O T
 *  
 *  Intersect a ray with an eto, where all constant terms have
 *  been precomputed by rt_eto_prep().  If an intersection occurs,
 *  one or two struct seg(s) will be acquired and filled in.
 *
 *  NOTE:  All lines in this function are represented parametrically
 *  by a point,  P( x0, y0, z0 ) and a direction normal,
 *  D = ax + by + cz.  Any point on a line can be expressed
 *  by one variable 't', where
 *
 *	X = a*t + x0,	eg,  X = Dx*t + Px
 *	Y = b*t + y0,
 *	Z = c*t + z0.
 *
 *  First, convert the line to the coordinate system of a "stan-
 *  dard" eto.  This is a eto which lies in the X-Y plane
 *  and circles the origin.  The semimajor axis is C.
 *
 *  Then find the equation of that line and the standard eto,
 *  which turns out to be a quartic equation in 't'.  Solve the
 *  equation using a general polynomial root finder.  Use those
 *  values of 't' to compute the points of intersection in the
 *  original coordinate system.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_eto_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct eto_specific *eto =
		(struct eto_specific *)stp->st_specific;
	register struct seg *segp;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL vect_t	work;		/* temporary vector */
	LOCAL poly	C;		/* The final equation */
	LOCAL complex	val[4];	/* The complex roots */
	LOCAL double	k[4];		/* The real roots */
	register int	i;
	LOCAL int	j;
	LOCAL vect_t	cor_pprime;	/* new ray origin */
	LOCAL fastf_t	cor_proj;
	LOCAL fastf_t	A1,A2,A3,A4,A5,A6,A7,A8,B1,B2,B3,C1,C2,C3,D1,term;

	/* Convert vector into the space of the unit eto */
	MAT4X3VEC( dprime, eto->eto_R, rp->r_dir );
	VUNITIZE( dprime );

	VSUB2( work, rp->r_pt, eto->eto_V );
	MAT4X3VEC( pprime, eto->eto_R, work );

	/* normalize distance from eto.  substitute
	 * corrected pprime which contains a translation along ray
	 * direction to closest approach to vertex of eto.
	 * Translating ray origin along direction of ray to closest pt. to
	 * origin of solid's coordinate system, new ray origin is
	 * 'cor_pprime'.
	 */
	cor_proj = VDOT( pprime, dprime );
	VSCALE( cor_pprime, dprime, cor_proj );
	VSUB2( cor_pprime, pprime, cor_pprime );

	/*
	 *  NOTE: The following code is based on code in
	 *        eto.f by ERIM for GIFT.
	 *
	 *  Given a line, finds the equation of the
	 *  eto in terms of the variable 't'.
	 *
	 *  The equation for the eto is:
	 *
            _______                           ________
           / 2    2              2           / 2    2               2
  [Eu(+- \/ x  + y   - R) + Ev z]  + [Fu(+-\/ x  + y   - R) + Fv z ]
 --------------------------------    -------------------------------  = 1
               2                                      2
             Rc                                     Rd
	 *
	 *                  ^   ^
	 *       where Ev = C . N
	 *
	 *                  ^   ^
	 *             Eu = C . A
	 *
	 *                  ^   ^
	 *             Fv = C . A
	 *
	 *                  ^   ^
	 *             Fu =-C . N.
	 *
	 *  First, find X, Y, and Z in terms of 't' for this line, then
	 *  substitute them into the equation above.
	 *
	 *  	Wx = Dx*t + Px, etc.
	 *
	 *  Regrouping coefficients and constants, the equation can then
	 *  be rewritten as:
	 *
	 *  [A1*sqrt(C1 + C2*t + C3*t^2) + A2 + A3*t]^2  +
	 *  [B1*sqrt(C1 + C2*t + C3*t^2) + B2 + B3*t]^2  - D1 = 0
	 *
	 *  where, (variables defined in code below)
	 */
	A1 = eto->eto_rd * eto->eu;
	B1 = eto->eto_rc * eto->fu;
	C1 = cor_pprime[X] * cor_pprime[X] + cor_pprime[Y] * cor_pprime[Y];
	C2 = 2 * (dprime[X] * cor_pprime[X] + dprime[Y] * cor_pprime[Y]);
	C3 = dprime[X] * dprime[X] + dprime[Y] * dprime[Y];
	A2 = -eto->eto_rd * eto->eto_r * eto->eu + eto->eto_rd * eto->ev * cor_pprime[Z];
   	B2 = -eto->eto_rc * eto->eto_r * eto->fu + eto->eto_rc * eto->fv * cor_pprime[Z];
	A3 = eto->eto_rd * eto->ev * dprime[Z];
	B3 = eto->eto_rc * eto->fv * dprime[Z];
	D1 = eto->eto_rc * eto->eto_rc * eto->eto_rd * eto->eto_rd;

	/*
	 *  Squaring the two terms above and again regrouping coefficients
	 *  the equation now becomes:
	 *
	 *  A6*t^2 + A5*t + A4 = -(A8*t + A7)*sqrt(C1 + C2*t + C3*t^2)
	 *
	 *  where, (variables defined in code)
	 */
	A4 = A1*A1*C1 + B1*B1*C1 + A2*A2 + B2*B2 - D1;
	A5 = A1*A1*C2 + B1*B1*C2 + 2*A2*A3 + 2*B2*B3;
	A6 = A1*A1*C3 + B1*B1*C3 + A3*A3 + B3*B3;
	A7 = 2*(A1*A2 + B1*B2);
	A8 = 2*(A1*A3 + B1*B3);
	term = A6*A6 - A8*A8*C3;

	/*
	 *  Squaring both sides and subtracting RHS from LHS yields:
	 */
	C.dgr=4;
	C.cf[4] = (A4*A4 - A7*A7*C1);			/* t^0 */
	C.cf[3] = (2*A4*A5 - A7*A7*C2 - 2*A7*A8*C1);	/* t^1 */
	C.cf[2] = (2*A4*A6 + A5*A5 - A7*A7*C3 - 2*A7*A8*C2 - A8*A8*C1);	/* t^2 */
	C.cf[1] = (2*A5*A6 - 2*A7*A8*C3 - A8*A8*C2);	/* t^3 */
	C.cf[0] = term;					/* t^4 */
	/* NOTE: End of ERIM based code */

	/*  It is known that the equation is 4th order.  Therefore,
	 *  if the root finder returns other than 4 roots, error.
	 */
	if ( (i = rt_poly_roots( &C, val )) != 4 ){
		if( i != 0 )  {
			rt_log("eto:  rt_poly_roots() 4!=%d\n", i);
			rt_pr_roots( stp->st_name, val, i );
		}
		return(0);		/* MISS */
	}

	/*  Only real roots indicate an intersection in real space.
	 *
	 *  Look at each root returned; if the imaginary part is zero
	 *  or sufficiently close, then use the real part as one value
	 *  of 't' for the intersections
	 */
	for ( j=0, i=0; j < 4; j++ ){
		if( NEAR_ZERO( val[j].im, 0.0001 ) )
			k[i++] = val[j].re;
	}

	/* reverse above translation by adding distance to all 'k' values. */
	for( j = 0; j < i; ++j )
		k[j] -= cor_proj;

	/* Here, 'i' is number of points found */
	switch( i )  {
	case 0:
		return(0);		/* No hit */

	default:
		rt_log("rt_eto_shot: reduced 4 to %d roots\n",i);
		rt_pr_roots( stp->st_name, val, 4 );
		return(0);		/* No hit */

	case 2:
		{
			/* Sort most distant to least distant. */
			FAST fastf_t	u;
			if( (u=k[0]) < k[1] )  {
				/* bubble larger towards [0] */
				k[0] = k[1];
				k[1] = u;
			}
		}
		break;
	case 4:
		{
			register short	n;
			register short	lim;

			/*  Inline rt_pt_sort().  Sorts k[] into descending order. */
			for( lim = i-1; lim > 0; lim-- )  {
				for( n = 0; n < lim; n++ )  {
					FAST fastf_t	u;
					if( (u=k[n]) < k[n+1] )  {
						/* bubble larger towards [0] */
						k[n] = k[n+1];
						k[n+1] = u;
					}
				}
			}
		}
		break;
	}

	/* Now, t[0] > t[npts-1] */
	/* k[1] is entry point, and k[0] is farthest exit point */
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in.hit_dist = k[1];
	segp->seg_out.hit_dist = k[0];
	/* Set aside vector for rt_eto_norm() later */
	VJOIN1( segp->seg_in.hit_vpriv, pprime, k[1], dprime );
	VJOIN1( segp->seg_out.hit_vpriv, pprime, k[0], dprime );
	RT_LIST_INSERT( &(seghead->l), &(segp->l) );

	if( i == 2 )
		return(2);			/* HIT */
				
	/* 4 points */
	/* k[3] is entry point, and k[2] is exit point */
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in.hit_dist = k[3];
	segp->seg_out.hit_dist = k[2];
	VJOIN1( segp->seg_in.hit_vpriv, pprime, k[3], dprime );
	VJOIN1( segp->seg_out.hit_vpriv, pprime, k[2], dprime );
	RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	return(4);			/* HIT */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	
/*
 *			R T _ E T O _ V S H O T
 *
 *  This is the Becker vector version
 */
void
rt_eto_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
}

/*
 *			R T _ E T O _ N O R M
 *
 *  Compute the normal to the eto,
 *  given a point on the eto centered at the origin on the X-Y plane.
 *  The gradient of the eto at that point is in fact the
 *  normal vector, which will have to be given unit length.
 *  To make this useful for the original eto, it will have
 *  to be rotated back to the orientation of the original eto.
 *  The equation for the eto is:
 *
 *            _______                           ________
 *           / 2    2              2           / 2    2               2
 *  [Eu(+- \/ x  + y   - R) + Ev z]  + [Fu(+-\/ x  + y   - R) + Fv z ]
 * --------------------------------    -------------------------------  = 1
 *               2                                      2
 *             Rc                                     Rd
 *
 *  The normal is the gradient of f(x,y,z) = 0 or
 *
 *	(df/dx, df/dy, df/dz)
 */
void
rt_eto_norm( hitp, stp, rp)
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct eto_specific *eto =
		(struct eto_specific *)stp->st_specific;
	FAST fastf_t sqrt_x2y2, efact, ffact, xcomp, ycomp, zcomp;
	LOCAL vect_t normp;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );

	sqrt_x2y2 = sqrt( hitp->hit_vpriv[X] * hitp->hit_vpriv[X]
			+ hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y] );

	efact = 2 * eto->eto_rd * eto->eto_rd * (eto->eu *
		(sqrt_x2y2 - eto->eto_r) + eto->ev * hitp->hit_vpriv[Z]);

	ffact = 2 * eto->eto_rc * eto->eto_rc * (eto->fu *
		(sqrt_x2y2 - eto->eto_r) + eto->fv * hitp->hit_vpriv[Z]);
	
	xcomp = (efact * eto->eu + ffact * eto->fu) / sqrt_x2y2;
	
	ycomp = hitp->hit_vpriv[Y] * xcomp;
	xcomp = hitp->hit_vpriv[X] * xcomp;
	zcomp = efact * eto->ev + ffact * eto->fv;
	
	VSET( normp, xcomp, ycomp, zcomp );
	VUNITIZE( normp );
	MAT3X3VEC( hitp->hit_normal, eto->eto_invR, normp );
}

/*
 *			R T _ E T O _ C U R V E
 *
 *  Return the curvature of the eto.
 */
void
rt_eto_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	fastf_t	a, b, ch, cv, dh, dv, k_circ, k_ell, phi, rad, xp,
		yp1, yp2, work;
	register struct eto_specific *eto =
		(struct eto_specific *)stp->st_specific;
	vect_t	Cp, Dp, Hit_Ell, Nu, Radius, Ru;

	a = eto->eto_rc;
	b = eto->eto_rd;
	VMOVE( Nu, eto->eto_N );
	VUNITIZE( Nu );

	/* take elliptical slice of eto at hit point */
	VSET( Ru, hitp->hit_vpriv[X], hitp->hit_vpriv[Y], 0. );
	VUNITIZE( Ru );
	VSCALE( Radius, Ru, eto->eto_r );

	/* get horizontal and vertical components of C and Rd */
	cv = VDOT( eto->eto_C, Nu );
	ch = sqrt( VDOT( eto->eto_C, eto->eto_C ) - cv * cv );
	/* angle between C and Nu */
	phi = acos( cv / MAGNITUDE(eto->eto_C) );
	dv = eto->eto_rd * sin(phi);
	dh = -eto->eto_rd * cos(phi);

	/* build coord system for ellipse: x,y directions are Dp,Cp */
	VCOMB2( Cp, ch, Ru, cv, Nu );
	VCOMB2( Dp, dh, Ru, dv, Nu );
	VUNITIZE( Cp );
	VUNITIZE( Dp );

	/* put hit point in cross sectional coordinates */
	VSUB2( Hit_Ell, hitp->hit_vpriv, Radius );
	xp = VDOT( Hit_Ell, Dp );
	/* yp = VDOT( Hit_Ell, Cp ); */

	/* calculate curvature along ellipse */
	/* k = y'' / (1 + y'^2) ^ 3/2 */
	rad = 1. / sqrt(1. - xp*xp/(a*a));
	yp1 = -b/(a*a)*xp*rad;
	yp2 = -b/(a*a)*rad*(xp*xp*rad*rad + 1.);
	work = 1 + yp1*yp1;
	k_ell = yp2 / (work*sqrt(work));
	
	/* calculate curvature along radial circle */
	k_circ = -1. / MAGNITUDE(Radius);

	if (fabs(k_ell) < fabs(k_circ)) {
		/* use 1st deriv for principle dir of curvature */
		VCOMB2( cvp->crv_pdir, xp, Dp, yp1, Cp );
		cvp->crv_c1 = k_ell;
		cvp->crv_c2 = k_circ;
	} else {
		VCROSS( cvp->crv_pdir, hitp->hit_normal, Radius );
		cvp->crv_c1 = k_circ;
		cvp->crv_c2 = k_ell;
	}
	VUNITIZE( cvp->crv_pdir );
}

/*
 *			R T _ E T O _ U V
 */
void
rt_eto_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	fastf_t	horz, theta_u, theta_v, vert;
	vect_t	Hit_Ell, Nu, Radius, Ru;

	register struct eto_specific *eto =
		(struct eto_specific *)stp->st_specific;

	/* take elliptical slice of eto at hit point */
	VSET( Ru, hitp->hit_vpriv[X], hitp->hit_vpriv[Y], 0. );
	VUNITIZE( Ru );
	VSCALE( Radius, Ru, eto->eto_r );
	/* put cross sectional ellipse at origin */
	VSUB2( Hit_Ell, hitp->hit_vpriv, Radius );

	/* find angle between Ru and Hit_Ell
	   (better for circle than ellipse...) */
	VMOVE( Nu, eto->eto_N );
	VUNITIZE( Nu );
	vert = VDOT( Hit_Ell, Nu );
	horz = VDOT( Hit_Ell, Ru );
	theta_u = atan2(vert, -horz);	/* tuck seam on eto inner diameter */

	/* find angle between hitp and x axis */
	theta_v = atan2(hitp->hit_vpriv[Y], hitp->hit_vpriv[X]);

	/* normalize to [0, 2pi] */
	if (theta_u < 0.)
		theta_u += rt_twopi;
	if (theta_v < 0.)
		theta_v += rt_twopi;

	/* normalize to [0, 1] */
	uvp->uv_u = theta_u/rt_twopi;
	uvp->uv_v = theta_v/rt_twopi;
	uvp->uv_du = uvp->uv_dv = 0;
}

/*
 *			R T _ E T O _ F R E E
 */
void
rt_eto_free( stp )
struct soltab *stp;
{
	register struct eto_specific *eto =
		(struct eto_specific *)stp->st_specific;

	rt_free( (char *)eto, "eto_specific");
}

int
rt_eto_class()
{
	return(0);
}

/*
 *			R T _ E T O _ P L O T
 *
 * The ETO has the following input fields:
 *	eto_V	V from origin to center
 *	eto_r	Radius scalar
 *	eto_N	Normal to plane of eto
 *	eto_C	Semimajor axis (vector) of eto cross section
 *	eto_rd	Semiminor axis length (scalar) of eto cross section
 *
 */
int
rt_eto_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	fastf_t		a, b;	/* axis lengths of ellipse */
	fastf_t		ang, ch, cv, dh, dv, ntol, dtol, phi, theta;
	fastf_t		*eto_ells;
	int		i, j, npts, nells;
	point_t		*ell;	/* array of ellipse points */
	point_t		Ell_V;	/* vertex of an ellipse */
	point_t		*rt_mk_ell();
	struct rt_eto_internal	*tip;
	vect_t		Au, Bu, Nu, Cp, Dp, Xu;

	RT_CK_DB_INTERNAL(ip);
	tip = (struct rt_eto_internal *)ip->idb_ptr;
	RT_ETO_CK_MAGIC(tip);

	a = MAGNITUDE( tip->eto_C );
	b = tip->eto_rd;

	if ( NEAR_ZERO(tip->eto_r, 0.0001) || NEAR_ZERO(b, 0.0001)
		|| NEAR_ZERO(a, 0.0001)) {
		rt_log("eto_plot: r, rd, or rc zero length\n");
		return(1);
	}

	/* Establish tolerances */
	if( ttol->rel <= 0.0 || ttol->rel >= 1.0 )  {
		dtol = 0.0;		/* none */
	} else {
		/*
		 * Convert relative to absolute by scaling smallest of
		 * radius and semi-minor axis
		 */
		if (tip->eto_r < b)
			dtol = ttol->rel * 2 * tip->eto_r;
		else
			dtol = ttol->rel * 2 * b;
	}
	if( ttol->abs <= 0.0 )  {
		if( dtol <= 0.0 )  {
			/* No tolerance given, use a default */
			if (tip->eto_r < b)
				dtol = 2 * 0.10 * tip->eto_r;	/* 10% */
			else
				dtol = 2 * 0.10 * b;	/* 10% */
		} else {
			/* Use absolute-ized relative tolerance */
		}
	} else {
		/* Absolute tolerance was given, pick smaller */
		if( ttol->rel <= 0.0 || dtol > ttol->abs )
			dtol = ttol->abs;
	}
	/* To ensure normal tolerance, remain below this angle */
	if( ttol->norm > 0.0 )
		ntol = ttol->norm;
	else
		/* tolerate everything */
		ntol = rt_pi;

	/* (x, y) coords for an ellipse */
	ell = rt_mk_ell( &npts, a, b, dtol, ntol );
	/* generate coordinate axes */
	VMOVE( Nu, tip->eto_N );
	VUNITIZE( Nu );			/* z axis of coord sys */
	vec_ortho( Bu, Nu );		/* x axis */
	VUNITIZE( Bu );
	VCROSS( Au, Nu, Bu );		/* y axis */

	/* number of segments required in eto circles */
	nells = rt_num_circular_segments( dtol, tip->eto_r );
	theta = rt_twopi / nells;	/* put ellipse every theta rads */
	/* get horizontal and vertical components of C and Rd */
	cv = VDOT( tip->eto_C, Nu );
	ch = sqrt( VDOT( tip->eto_C, tip->eto_C ) - cv * cv );
	/* angle between C and Nu */
	phi = acos( cv / MAGNITUDE(tip->eto_C) );
	dv = tip->eto_rd * sin(phi);
	dh = -tip->eto_rd * cos(phi);

	/* make sure ellipse doesn't overlap itself when revolved */
	if (ch > tip->eto_r || dh > tip->eto_r) {
		rt_log("eto_plot: revolved ellipse overlaps itself\n");
		return(1);
	}
	
	/* get memory for nells ellipses */
	eto_ells = (fastf_t *)rt_malloc(nells * npts * sizeof(point_t), "ells[]");

	/* place each ellipse properly to make eto */
	for (i = 0, ang = 0.; i < nells; i++, ang += theta) {
		/* direction of current ellipse */
		VCOMB2( Xu, cos(ang), Bu, sin(ang), Au );
		VUNITIZE( Xu );
		/* vertex of ellipse */
		VJOIN1( Ell_V, tip->eto_V, tip->eto_r, Xu );
		/* coord system for ellipse: x,y directions are Dp,Cp */
		VCOMB2( Cp, ch, Xu, cv, Nu );
		VCOMB2( Dp, dh, Xu, dv, Nu );
		VUNITIZE( Cp );
		VUNITIZE( Dp );

/* convert 2D address to index into 1D array */
#define PT(www,lll)	((((www)%nells)*npts)+((lll)%npts))
#define PTA(ww,ll)	(&eto_ells[PT(ww,ll)*3])
#define NMA(ww,ll)	(norms[PT(ww,ll)])

		/* make ellipse */
		for (j = 0; j < npts; j++) {
			VJOIN2( PTA(i,j),
				Ell_V, ell[j][X], Dp, ell[j][Y], Cp );
		}
	}
	
	/* draw ellipses */
	for (i = 0; i < nells; i++) {
		RT_ADD_VLIST( vhead, PTA(i,npts-1), RT_VLIST_LINE_MOVE );
		for( j = 0; j < npts; j++ )
			RT_ADD_VLIST( vhead, PTA(i,j), RT_VLIST_LINE_DRAW );
	}
	
	/* draw connecting circles */
	for (i = 0; i < npts; i++) {
		RT_ADD_VLIST( vhead, PTA(nells-1,i), RT_VLIST_LINE_MOVE );
		for( j = 0; j < nells; j++ )
			RT_ADD_VLIST( vhead, PTA(j,i), RT_VLIST_LINE_DRAW );
	}

	rt_free( (char *)eto_ells, "ells[]" );
	return(0);
}

/*
 *	R T _ E L L 4
 *
 *	Approximate one fourth (1st quadrant) of an ellipse with line
 *	segments.  The initial single segment is broken at the point
 *	farthest from the ellipse if that point is not aleady within the
 *	distance and normal error tolerances.  The two resulting segments are
 *	passed recursively to this routine until each segment is within
 *	tolerance.
 */
int
rt_ell4( pts, a, b, dtol, ntol )
fastf_t	a, b, dtol, ntol;
struct pt_node	*pts;
{
	fastf_t	dist, intr, m, theta0, theta1;
	int	n;
	point_t	mpt, p0, p1;
	vect_t	norm_line, norm_ell;
	struct pt_node *new, *rt_ptalloc();

	/* endpoints of segment approximating ellipse */
	VMOVE( p0, pts->p );
	VMOVE( p1, pts->next->p );
	/* slope and intercept of segment */
	m = ( p1[X] - p0[X] ) / ( p1[Y] - p0[Y] );
	intr = p0[X] - m * p0[Y];
	/* point on ellipse with max dist between ellipse and line */
	mpt[Y] = a / sqrt( b*b / (m*m*a*a) + 1 );
	mpt[X] = b * sqrt( 1 - mpt[Y] * mpt[Y] / (a*a) );
	mpt[Z] = 0;
	/* max distance between that point and line */
	dist = fabs( m * mpt[Y] - mpt[X] + intr ) / sqrt( m * m + 1 );
	/* angles between normal of line and of ellipse at line endpoints */
	VSET( norm_line, m, -1., 0.);
	VSET( norm_ell, b * b * p0[Y], a * a * p0[X], 0. );
	VUNITIZE( norm_line );
	VUNITIZE( norm_ell );
	theta0 = fabs( acos( VDOT( norm_line, norm_ell )));
	VSET( norm_ell, b * b * p1[Y], a * a * p1[X], 0. );
	VUNITIZE( norm_ell );
	theta1 = fabs( acos( VDOT( norm_line, norm_ell )));
	/* split segment at widest point if not within error tolerances */
	if ( dist > dtol || theta0 > ntol || theta1 > ntol ) {
		/* split segment */
		new = rt_ptalloc();
		VMOVE( new->p, mpt );
		new->next = pts->next;
		pts->next = new;
		/* keep track of number of pts added */
		n = 1;
		/* recurse on first new segment */
		n += rt_ell4( pts, a, b, dtol, ntol );
		/* recurse on second new segment */
		n += rt_ell4( new, a, b, dtol, ntol );
	} else
		n  = 0;
	return( n );
}

/*
 *	R T _ M K _ E L L
 *
 *	Return pointer an array of points approximating an ellipse
 *	with semi-major and semi-minor axes a and b.  The line
 *	segments fall within the normal and distance tolerances
 *	of ntol and dtol.
 */
point_t *
rt_mk_ell( n, a, b, dtol, ntol )
int	*n;
fastf_t	a, b, dtol, ntol;
{
	int		i;
	point_t		*ell;
	struct pt_node	*ell_quad, *oldpos, *pos, *rt_ptalloc();

	ell_quad = rt_ptalloc();
	VSET( ell_quad->p, b, 0., 0. );
	ell_quad->next = rt_ptalloc();
	VSET( ell_quad->next->p, 0., a, 0. );
	ell_quad->next->next = NULL;

	*n = rt_ell4( ell_quad, a, b, dtol, ntol );
	ell = (point_t *)rt_malloc(4*(*n+1)*sizeof(point_t), "rt_mk_ell pts");

	/* put 1st quad of ellipse into an array */
	pos = ell_quad;
	for (i = 0; i < *n+2; i++) {
		VMOVE( ell[i], pos->p );
		oldpos = pos;
		pos = pos->next;
		rt_free( (char *)oldpos, "pt_node" );
	}
	/* mirror 1st quad to make 2nd */
	for (i = (*n+1)+1; i < 2*(*n+1); i++) {
		VMOVE( ell[i], ell[(*n*2+2)-i] );
		ell[i][X] = -ell[i][X];
	}
	/* mirror 2nd quad to make 3rd */
	for (i = 2*(*n+1); i < 3*(*n+1); i++) {
		VMOVE( ell[i], ell[i-(*n*2+2)] );
		ell[i][X] = -ell[i][X];
		ell[i][Y] = -ell[i][Y];
	}
	/* mirror 3rd quad to make 4th */
	for (i = 3*(*n+1); i < 4*(*n+1); i++) {
		VMOVE( ell[i], ell[i-(*n*2+2)] );
		ell[i][X] = -ell[i][X];
		ell[i][Y] = -ell[i][Y];
	}
	*n = 4*(*n + 1);
	return(ell);
}

/*
 *			R T _ E T O _ T E S S
 */
int
rt_eto_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	fastf_t		a, b;	/* axis lengths of ellipse */
	fastf_t		ang, ch, cv, dh, dv, ntol, dtol, phi, theta;
	fastf_t		*eto_ells;
	int		i, j, nfaces, npts, nells;
	point_t		*ell;	/* array of ellipse points */
	point_t		Ell_V;	/* vertex of an ellipse */
	point_t		*rt_mk_ell();
	struct rt_eto_internal	*tip;
	struct shell	*s;
	struct vertex	**verts;
	struct faceuse	**faces;
	struct vertex	**vertp[4];
	vect_t		Au, Bu, Nu, Cp, Dp, Xu;
	vect_t		*norms;	/* normal vectors for each vertex */
	int		fail=0;

	RT_CK_DB_INTERNAL(ip);
	tip = (struct rt_eto_internal *)ip->idb_ptr;
	RT_ETO_CK_MAGIC(tip);

	a = MAGNITUDE( tip->eto_C );
	b = tip->eto_rd;

	if ( NEAR_ZERO(tip->eto_r, 0.0001) || NEAR_ZERO(b, 0.0001)
		|| NEAR_ZERO(a, 0.0001)) {
		rt_log("eto_tess: r, rd, or rc zero length\n");
		fail = (-2);
		goto failure;
	}

	/* Establish tolerances */
	if( ttol->rel <= 0.0 || ttol->rel >= 1.0 )  {
		dtol = 0.0;		/* none */
	} else {
		/*
		 * Convert relative to absolute by scaling smallest of
		 * radius and semi-minor axis
		 */
		if (tip->eto_r < b)
			dtol = ttol->rel * 2 * tip->eto_r;
		else
			dtol = ttol->rel * 2 * b;
	}
	if( ttol->abs <= 0.0 )  {
		if( dtol <= 0.0 )  {
			/* No tolerance given, use a default */
			if (tip->eto_r < b)
				dtol = 2 * 0.10 * tip->eto_r;	/* 10% */
			else
				dtol = 2 * 0.10 * b;	/* 10% */
		} else {
			/* Use absolute-ized relative tolerance */
		}
	} else {
		/* Absolute tolerance was given, pick smaller */
		if( ttol->rel <= 0.0 || dtol > ttol->abs )
			dtol = ttol->abs;
	}
	/* To ensure normal tolerance, remain below this angle */
	if( ttol->norm > 0.0 )
		ntol = ttol->norm;
	else
		/* tolerate everything */
		ntol = rt_pi;

	/* (x, y) coords for an ellipse */
	ell = rt_mk_ell( &npts, a, b, dtol, ntol );
	/* generate coordinate axes */
	VMOVE( Nu, tip->eto_N );
	VUNITIZE( Nu );			/* z axis of coord sys */
	vec_ortho( Bu, Nu );		/* x axis */
	VUNITIZE( Bu );
	VCROSS( Au, Nu, Bu );		/* y axis */

	/* number of segments required in eto circles */
	nells = rt_num_circular_segments( dtol, tip->eto_r );
	theta = rt_twopi / nells;	/* put ellipse every theta rads */
	/* get horizontal and vertical components of C and Rd */
	cv = VDOT( tip->eto_C, Nu );
	ch = sqrt( VDOT( tip->eto_C, tip->eto_C ) - cv * cv );
	/* angle between C and Nu */
	phi = acos( cv / MAGNITUDE(tip->eto_C) );
	dv = tip->eto_rd * sin(phi);
	dh = -tip->eto_rd * cos(phi);

	/* make sure ellipse doesn't overlap itself when revolved */
	if (ch > tip->eto_r || dh > tip->eto_r) {
		rt_log("eto_tess: revolved ellipse overlaps itself\n");
		fail = (-3);
		goto failure;
	}
	
	/* get memory for nells ellipses */
	eto_ells = (fastf_t *)rt_malloc(nells * npts * sizeof(point_t), "ells[]");
	norms = (vect_t *)rt_calloc( nells*npts , sizeof( vect_t ) , "rt_eto_tess: norms" );

	/* place each ellipse properly to make eto */
	for (i = 0, ang = 0.; i < nells; i++, ang += theta) {
		/* direction of current ellipse */
		VCOMB2( Xu, cos(ang), Bu, sin(ang), Au );
		VUNITIZE( Xu );
		/* vertex of ellipse */
		VJOIN1( Ell_V, tip->eto_V, tip->eto_r, Xu );
		/* coord system for ellipse: x,y directions are Dp,Cp */
		VCOMB2( Cp, ch, Xu, cv, Nu );
		VCOMB2( Dp, dh, Xu, dv, Nu );
		VUNITIZE( Cp );
		VUNITIZE( Dp );
		/* make ellipse */
		for (j = 0; j < npts; j++) {
			VJOIN2( PTA(i,j),
				Ell_V, ell[j][X], Dp, ell[j][Y], Cp );
			VBLEND2( NMA(i,j),
				a*a*ell[j][X], Dp , b*b*ell[j][Y], Cp );
			VUNITIZE( NMA(i,j) );
		}
	}

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = RT_LIST_FIRST(shell, &(*r)->s_hd);

	verts = (struct vertex **)rt_calloc( npts*nells, sizeof(struct vertex *),
		"rt_eto_tess *verts[]" );
	faces = (struct faceuse **)rt_calloc( npts*nells, sizeof(struct faceuse *),
		"rt_eto_tess *faces[]" );

	/* Build the topology of the eto */
	nfaces = 0;
	for( i = 0; i < nells; i++ )  {
		for( j = 0; j < npts; j++ )  {
			vertp[0] = &verts[ PT(i+0,j+0) ];
			vertp[1] = &verts[ PT(i+0,j+1) ];
			vertp[2] = &verts[ PT(i+1,j+1) ];
			vertp[3] = &verts[ PT(i+1,j+0) ];
			if( (faces[nfaces++] = nmg_cmface( s, vertp, 4 )) == (struct faceuse *)0 )  {
				rt_log("rt_eto_tess() nmg_cmface failed, i=%d/%d, j=%d/%d\n",
					i, nells, j, npts );
				nfaces--;
			}
		}
	}

	/* Associate vertex geometry */
	for( i = 0; i < nells; i++ )  {
		for( j = 0; j < npts; j++ )  {
			nmg_vertex_gv( verts[PT(i,j)], PTA(i,j) );
		}
	}

	/* Associate face geometry */
	for( i=0; i < nfaces; i++ )  {
		if( nmg_fu_planeeqn( faces[i], tol ) < 0 )
		{
			fail = (-1);
			goto failure;
		}
	}

	/* associate vertexuse normals */
	for( i=0 ; i<nells ; i++ )
	{
		for( j=0 ; j<npts ; j++ )
		{
			struct vertexuse *vu;
			vect_t rev_norm;

			VREVERSE( rev_norm , NMA(i,j) );

			NMG_CK_VERTEX( verts[PT(i,j)] );

			for( RT_LIST_FOR( vu , vertexuse , &verts[PT(i,j)]->vu_hd ) )
			{
				struct faceuse *fu;

				NMG_CK_VERTEXUSE( vu );

				fu = nmg_find_fu_of_vu( vu );
				NMG_CK_FACEUSE( fu );

				if( fu->orientation == OT_SAME )
					nmg_vertexuse_nv( vu , NMA(i,j) );
				else if( fu->orientation == OT_OPPOSITE )
					nmg_vertexuse_nv( vu , rev_norm );
			}
		}
	}

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r, tol );

failure:	
	rt_free( (char *)ell, "rt_mk_ell pts" );
	rt_free( (char *)eto_ells, "ells[]" );
	rt_free( (char *)verts, "rt_eto_tess *verts[]" );
	rt_free( (char *)faces, "rt_eto_tess *faces[]" );
	rt_free( (char *)norms, "rt_eto_tess: norms[]" );

	return( fail );
}

/*
 *			R T _ E T O _ I M P O R T
 *
 *  Import a eto from the database format to the internal format.
 *  Apply modeling transformations at the same time.
 */
int
rt_eto_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	struct rt_eto_internal	*tip;
	union record		*rp;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		rt_log("rt_eto_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_ETO;
	ip->idb_ptr = rt_malloc(sizeof(struct rt_eto_internal), "rt_eto_internal");
	tip = (struct rt_eto_internal *)ip->idb_ptr;
	tip->eto_magic = RT_ETO_INTERNAL_MAGIC;

	/* Apply modeling transformations */
	MAT4X3PNT( tip->eto_V, mat, &rp->s.s_values[0*3] );
	MAT4X3VEC( tip->eto_N, mat, &rp->s.s_values[1*3] );
	MAT4X3VEC( tip->eto_C, mat, &rp->s.s_values[2*3] );
	tip->eto_r  = rp->s.s_values[3*3] / mat[15];
	tip->eto_rd = rp->s.s_values[3*3+1] / mat[15];

	if( tip->eto_r < SMALL || tip->eto_rd < SMALL )  {
		rt_log("rt_eto_import:  zero length R or Rd vector\n");
		return(-1);
	}

	return(0);		/* OK */
}

/*
 *			R T _ E T O _ E X P O R T
 *
 *  The name will be added by the caller.
 */
int
rt_eto_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_eto_internal	*tip;
	union record		*eto;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_ETO )  return(-1);
	tip = (struct rt_eto_internal *)ip->idb_ptr;
	RT_ETO_CK_MAGIC(tip);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "eto external");
	eto = (union record *)ep->ext_buf;

	eto->s.s_id = ID_SOLID;
	eto->s.s_type = ETO;

	if (MAGNITUDE(tip->eto_C) < RT_LEN_TOL
		|| MAGNITUDE(tip->eto_N) < RT_LEN_TOL
		|| tip->eto_r < RT_LEN_TOL
		|| tip->eto_rd < RT_LEN_TOL) {
		rt_log("rt_eto_export: not all dimensions positive!\n");
		return(-1);
	}
	
	if (tip->eto_rd > MAGNITUDE(tip->eto_C) ) {
		rt_log("rt_eto_export: semi-minor axis cannot be longer than semi-major axis!\n");
		return(-1);
	}

	/* Warning:  type conversion */
	VSCALE( &eto->s.s_values[0*3], tip->eto_V, local2mm );
	VSCALE( &eto->s.s_values[1*3], tip->eto_N, local2mm );
	VSCALE( &eto->s.s_values[2*3], tip->eto_C, local2mm );
	eto->s.s_values[3*3] = tip->eto_r * local2mm;
	eto->s.s_values[3*3+1] = tip->eto_rd * local2mm;

	return(0);
}

/*
 *			R T _ E T O _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_eto_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_eto_internal	*tip =
		(struct rt_eto_internal *)ip->idb_ptr;
	char				buf[256];

	RT_ETO_CK_MAGIC(tip);
	rt_vls_strcat( str, "Elliptical Torus (ETO)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
		tip->eto_V[X] * mm2local,
		tip->eto_V[Y] * mm2local,
		tip->eto_V[Z] * mm2local);
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tN=(%g, %g, %g)\n",
		tip->eto_N[X] * mm2local,
		tip->eto_N[Y] * mm2local,
		tip->eto_N[Z] * mm2local );
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tC=(%g, %g, %g) mag=%g\n",
		tip->eto_C[X] * mm2local,
		tip->eto_C[Y] * mm2local,
		tip->eto_C[Z] * mm2local,
		MAGNITUDE(tip->eto_C) * mm2local);
	rt_vls_strcat( str, buf );
	
	sprintf(buf, "\tr=%g\n", tip->eto_r * mm2local);
	rt_vls_strcat( str, buf );
	
	sprintf(buf, "\td=%g\n", tip->eto_rd * mm2local);
	rt_vls_strcat( str, buf );

	return(0);
}

/*
 *			R T _ E T O _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_eto_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_eto_internal	*tip;

	RT_CK_DB_INTERNAL(ip);
	tip = (struct rt_eto_internal *)ip->idb_ptr;
	RT_ETO_CK_MAGIC(tip);

	rt_free( (char *)tip, "eto ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
