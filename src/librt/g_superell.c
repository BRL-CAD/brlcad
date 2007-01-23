/*                    G _ S U P E R E L L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup g_  */
/** @{ */
/** @file g_superell.c
 *
 * Intersect a ray with a Superquadratic Ellipsoid.
 *
 *      NOTICE: this primitive is incomplete and should beconsidered
 *              experimental.  this primitive will exhibit several
 *              instabilities in the existing root solver method.
 *
 *
 *
 *  Authors -
 *      Christopher Sean Morrison (Programming)
 *      Edwin O. Davisson (Mathematics)
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/** @} */

#ifndef lint
static const char RCSsuperell[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "./debug.h"

const struct bu_structparse rt_superell_parse[] = {
    { "%f", 3, "V", bu_offsetof(struct rt_superell_internal, v[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "A", bu_offsetof(struct rt_superell_internal, a[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "B", bu_offsetof(struct rt_superell_internal, b[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "C", bu_offsetof(struct rt_superell_internal, c[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "n", bu_offsetof(struct rt_superell_internal, n), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "e", bu_offsetof(struct rt_superell_internal, e), BU_STRUCTPARSE_FUNC_NULL },
    { {'\0','\0','\0','\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL }
 };


/*
 *  Algorithm:
 *
 *  Given V, A, B, and C, there is a set of points on this superellipsoid
 *
 *  { (x,y,z) | (x,y,z) is on superellipsoid defined by V, A, B, C }
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
 *  To find the intersection of a line with the superellipsoid, consider
 *  the parametric line L:
 *
 *  	L : { P(n) | P + t(n) . D }
 *
 *  Call W the actual point of intersection between L and the superellipsoid.
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
 *  NORMALS.  Given the point W on the superellipsoid, what is the vector
 *  normal to the tangent plane at that point?
 *
 *  Map W onto the unit sphere, ie:  W' = S( R( W - V ) ).
 *
 *  Plane on unit sphere at W' has a normal vector of the same value(!).
 *  N' = W'
 *
 *  The plane transforms back to the tangent plane at W, and this
 *  new plane (on the superellipsoid) has a normal vector of N, viz:
 *
 *  N = inverse[ transpose( inverse[ S o R ] ) ] ( N' )
 *
 *  because if H is perpendicular to plane Q, and matrix M maps from
 *  Q to Q', then inverse[ transpose(M) ] (H) is perpendicular to Q'.
 *  Here, H and Q are in "prime space" with the unit sphere.
 *  [Somehow, the notation here is backwards].
 *  So, the mapping matrix M = inverse( S o R ), because
 *  S o R maps from normal space to the unit sphere.
 *
 *  N = inverse[ transpose( inverse[ S o R ] ) ] ( N' )
 *    = inverse[ transpose(invR o invS) ] ( N' )
 *    = inverse[ transpose(invS) o transpose(invR) ] ( N' )
 *    = inverse[ inverse(S) o R ] ( N' )
 *    = invR o S ( N' )
 *
 *    = invR o S ( W' )
 *    = invR( S( S( R( W - V ) ) ) )
 *
 *  because inverse(R) = transpose(R), so R = transpose( invR ),
 *  and S = transpose( S ).
 *
 *  Note that the normal vector N produced above will not have unit length.
 */

struct superell_specific {
  vect_t superell_V; /* Vector to center of superellipsoid */
  vect_t superell_Au; /* unit-length A vector */
  vect_t superell_Bu;
  vect_t superell_Cu;
  double superell_n; /* north-south curvature power */
  double superell_e; /* east-west curvature power */
  double superell_invmsAu; /* 1.0 / |Au|^2 */
  double superell_invmsBu; /* 1.0 / |Bu|^2 */
  double superell_invmsCu; /* 1.0 / |Cu|^2 */
  vect_t superell_invsq;
  mat_t	superell_SoR; /* matrix for local cordinate system, Scale(Rotate(V))*/
  mat_t superell_invRSSR; /* invR(Scale(Scale(Rot(V)))) */
  mat_t superell_invR; /* transposed rotation matrix */
};
#define SUPERELL_NULL	((struct superell_specific *)0)

/**
 *  			R T _ S U P E R E L L _ P R E P
 *
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid superellipsoid, and if so, precompute various
 *  terms of the formula.
 *
 *  Returns -
 *  	0	SUPERELL is OK
 *  	!0	Error in description
 *
 *  Implicit return -
 *  	A struct superell_specific is created, and it's address is stored in
 *  	stp->st_specific for use by rt_superell_shot().
 */
int
rt_superell_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{

  register struct superell_specific *superell;
  struct rt_superell_internal	*eip;
  LOCAL fastf_t	magsq_a, magsq_b, magsq_c;
  LOCAL mat_t	R, TEMP;
  LOCAL vect_t	Au, Bu, Cu;	/* A,B,C with unit length */
  LOCAL vect_t	w1, w2, P;	/* used for bounding RPP */
  LOCAL fastf_t	f;

  eip = (struct rt_superell_internal *)ip->idb_ptr;
  RT_SUPERELL_CK_MAGIC(eip);

  /* Validate that |A| > 0, |B| > 0, |C| > 0 */
  magsq_a = MAGSQ( eip->a );
  magsq_b = MAGSQ( eip->b );
  magsq_c = MAGSQ( eip->c );

  if( magsq_a < rtip->rti_tol.dist || magsq_b < rtip->rti_tol.dist || magsq_c < rtip->rti_tol.dist ) {
    bu_log("superell(%s):  near-zero length A(%g), B(%g), or C(%g) vector\n",
	   stp->st_name, magsq_a, magsq_b, magsq_c );
    return(1);		/* BAD */
  }
  if (eip->n < rtip->rti_tol.dist || eip->e < rtip->rti_tol.dist) {
    bu_log("superell(%s):  near-zero length <n,e> curvature (%g, %g) causes problems\n",
	   stp->st_name, eip->n, eip->e);
    /* BAD */
  }
  if (eip->n > 10000.0 || eip->e > 10000.0) {
    bu_log("superell(%s):  very large <n,e> curvature (%g, %g) causes problems\n",
	   stp->st_name, eip->n, eip->e);
    /* BAD */
  }

  /* Create unit length versions of A,B,C */
  f = 1.0/sqrt(magsq_a);
  VSCALE( Au, eip->a, f );
  f = 1.0/sqrt(magsq_b);
  VSCALE( Bu, eip->b, f );
  f = 1.0/sqrt(magsq_c);
  VSCALE( Cu, eip->c, f );

  /* Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only) */
  f = VDOT( Au, Bu );
  if( ! NEAR_ZERO(f, rtip->rti_tol.dist) )  {
    bu_log("superell(%s):  A not perpendicular to B, f=%f\n",stp->st_name, f);
    return(1);		/* BAD */
  }
  f = VDOT( Bu, Cu );
  if( ! NEAR_ZERO(f, rtip->rti_tol.dist) )  {
    bu_log("superell(%s):  B not perpendicular to C, f=%f\n",stp->st_name, f);
    return(1);		/* BAD */
  }
  f = VDOT( Au, Cu );
  if( ! NEAR_ZERO(f, rtip->rti_tol.dist) )  {
    bu_log("superell(%s):  A not perpendicular to C, f=%f\n",stp->st_name, f);
    return(1);		/* BAD */
  }

  /* Solid is OK, compute constant terms now */

  BU_GETSTRUCT( superell, superell_specific );
  stp->st_specific = (genptr_t)superell;

  superell->superell_n = eip->n;
  superell->superell_e = eip->e;

  VMOVE( superell->superell_V, eip->v );

  VSET( superell->superell_invsq, 1.0/magsq_a, 1.0/magsq_b, 1.0/magsq_c );
  VMOVE( superell->superell_Au, Au );
  VMOVE( superell->superell_Bu, Bu );
  VMOVE( superell->superell_Cu, Cu );

  /* compute the inverse magnitude square for equations during shot */
  superell->superell_invmsAu = 1.0 / magsq_a;
  superell->superell_invmsBu = 1.0 / magsq_b;
  superell->superell_invmsCu = 1.0 / magsq_c;

  /* compute the rotation matrix */
  MAT_IDN(R);
  VMOVE( &R[0], Au );
  VMOVE( &R[4], Bu );
  VMOVE( &R[8], Cu );
  bn_mat_trn( superell->superell_invR, R );

  /* computer invRSSR */
  MAT_IDN(superell->superell_invRSSR);
  MAT_IDN(TEMP);
  TEMP[0] = superell->superell_invsq[0];
  TEMP[5] = superell->superell_invsq[1];
  TEMP[10] = superell->superell_invsq[2];
  bn_mat_mul(TEMP, TEMP, R);
  bn_mat_mul(superell->superell_invRSSR, superell->superell_invR, TEMP);

  /* compute Scale(Rotate(vect)) */
  MAT_IDN(superell->superell_SoR);
  VSCALE( &superell->superell_SoR[0], eip->a, superell->superell_invsq[0]);
  VSCALE( &superell->superell_SoR[4], eip->b, superell->superell_invsq[1]);
  VSCALE( &superell->superell_SoR[8], eip->c, superell->superell_invsq[2]);

  /* Compute bounding sphere */
  VMOVE( stp->st_center, eip->v );
  f = magsq_a;
  if( magsq_b > f )
    f = magsq_b;
  if( magsq_c > f )
    f = magsq_c;
  stp->st_aradius = stp->st_bradius = sqrt(f);

  /* Compute bounding RPP */
  VSET( w1, magsq_a, magsq_b, magsq_c );

  /* X */
  VSET( P, 1.0, 0, 0 );		/* bounding plane normal */
  MAT3X3VEC( w2, R, P );		/* map plane to local coord syst */
  VELMUL( w2, w2, w2 );		/* square each term */
  f = VDOT( w1, w2 );
  f = sqrt(f);
  stp->st_min[X] = superell->superell_V[X] - f;	/* V.P +/- f */
  stp->st_max[X] = superell->superell_V[X] + f;

  /* Y */
  VSET( P, 0, 1.0, 0 );		/* bounding plane normal */
  MAT3X3VEC( w2, R, P );		/* map plane to local coord syst */
  VELMUL( w2, w2, w2 );		/* square each term */
  f = VDOT( w1, w2 );
  f = sqrt(f);
  stp->st_min[Y] = superell->superell_V[Y] - f;	/* V.P +/- f */
  stp->st_max[Y] = superell->superell_V[Y] + f;

  /* Z */
  VSET( P, 0, 0, 1.0 );		/* bounding plane normal */
  MAT3X3VEC( w2, R, P );		/* map plane to local coord syst */
  VELMUL( w2, w2, w2 );		/* square each term */
  f = VDOT( w1, w2 );
  f = sqrt(f);
  stp->st_min[Z] = superell->superell_V[Z] - f;	/* V.P +/- f */
  stp->st_max[Z] = superell->superell_V[Z] + f;

  return(0);			/* OK */
}

/**
 *			R T _ S U P E R E L L _ P R I N T
 */
void
rt_superell_print(register const struct soltab *stp)
{
	register struct superell_specific *superell =
		(struct superell_specific *)stp->st_specific;

	VPRINT("V", superell->superell_V);
}

/* Equation of a superellipsoid:
 *
 * f(x) = [ (x^(2/e2) + y^(2/e2))^(e2/e1) + z^(2/e1) ]^(e1/2) - 1
 */

/*
 *  			R T _ S U P E R E L L _ S H O T
 *
 *  Intersect a ray with an superellipsoid, where all constant terms have
 *  been precomputed by rt_superell_prep().  If an intersection occurs,
 *  a struct seg will be acquired and filled in.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_superell_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
  static int counter=10;

  register struct superell_specific *superell = (struct superell_specific *)stp->st_specific;
  LOCAL bn_poly_t equation; /* equation of superell to be solved */
  LOCAL vect_t translated;  /* translated shot vector */
  LOCAL vect_t newShotPoint; /* P' */
  LOCAL vect_t newShotDir; /* D' */
  LOCAL vect_t normalizedShotPoint; /* P' with normalized dist from superell */
  LOCAL bn_complex_t complexRoot[4]; /* roots returned from poly solver */
  LOCAL double realRoot[4];  /* real ray distance values */
  register int i,j;
  register struct seg *segp;

  /* translate ray point */
  /*  VSUB2(translated, rp->r_pt, superell->superell_V); */
  (translated)[X] = (rp->r_pt)[X] - (superell->superell_V)[X];
  (translated)[Y] = (rp->r_pt)[Y] - (superell->superell_V)[Y];
  (translated)[Z] = (rp->r_pt)[Z] - (superell->superell_V)[Z];

  /* scale and rotate point to get P' */

  /*  MAT4X3VEC(newShotPoint, superell->superell_SoR, translated); */
  newShotPoint[X] = (superell->superell_SoR[0]*translated[X] + superell->superell_SoR[1]*translated[Y] + superell->superell_SoR[ 2]*translated[Z]) * 1.0/(superell->superell_SoR[15]);
  newShotPoint[Y] = (superell->superell_SoR[4]*translated[X] + superell->superell_SoR[5]*translated[Y] + superell->superell_SoR[ 6]*translated[Z]) * 1.0/(superell->superell_SoR[15]);
  newShotPoint[Z] = (superell->superell_SoR[8]*translated[X] + superell->superell_SoR[9]*translated[Y] + superell->superell_SoR[10]*translated[Z]) * 1.0/(superell->superell_SoR[15]);

  /* translate ray direction vector */
  MAT4X3VEC(newShotDir, superell->superell_SoR, rp->r_dir);
  VUNITIZE(newShotDir);

  /* normalize distance from the superell.  substitues a corrected ray
   * point, which contains a translation along the ray direction to the
   * closest approach to vertex of the superell.  Translating the ray
   * along the direction of the ray to the closest point near the
   * primitives center vertex.  New ray origin is hence, normalized.
   */
  VSCALE( normalizedShotPoint, newShotDir,
	  VDOT( newShotPoint, newShotDir ));
  VSUB2( normalizedShotPoint, newShotPoint, normalizedShotPoint );

  /* Now generate the polynomial equation for passing to the root finder */

  equation.dgr = 2;

  /* (x^2 / A) + (y^2 / B) + (z^2 / C) - 1 */
  equation.cf[0] = newShotPoint[X] * newShotPoint[X] * superell->superell_invmsAu + newShotPoint[Y] * newShotPoint[Y] * superell->superell_invmsBu + newShotPoint[Z] * newShotPoint[Z] * superell->superell_invmsCu - 1;
  /* (2xX / A) + (2yY / B) + (2zZ / C) */
  equation.cf[1] = 2 * newShotDir[X] * newShotPoint[X] * superell->superell_invmsAu + 2 * newShotDir[Y] * newShotPoint[Y] * superell->superell_invmsBu + 2 * newShotDir[Z] * newShotPoint[Z] * superell->superell_invmsCu;
  /* (X^2 / A) + (Y^2 / B) + (Z^2 / C) */
  equation.cf[2] = newShotDir[X] * newShotDir[X] * superell->superell_invmsAu  + newShotDir[Y] * newShotDir[Y] * superell->superell_invmsBu + newShotDir[Z] * newShotDir[Z] * superell->superell_invmsCu;

  if ( (i = rt_poly_roots( &equation, complexRoot, stp->st_dp->d_namep)) != 2 ) {
      if (i > 0) {
	  bu_log("superell: rt_poly_roots() 2 != %d\n", i);
	  bn_pr_roots(stp->st_name, complexRoot, i);
      } else if (i < 0) {
	  static int reported=0;
	  bu_log("The root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
	  if (!reported) {
	      VPRINT("while shooting from:\t", rp->r_pt);
	      VPRINT("while shooting at:\t", rp->r_dir);
	      bu_log("Additional superellipsoid convergence failure details will be suppressed.\n");
	      reported=1;
	  }
      }
      return (0); /* MISS */
  }

  /* XXX BEGIN CUT */
  /*  Only real roots indicate an intersection in real space.
   *
   *  Look at each root returned; if the imaginary part is zero
   *  or sufficiently close, then use the real part as one value
   *  of 't' for the intersections
   */
  for ( j=0, i=0; j < 2; j++ ){
    if( NEAR_ZERO( complexRoot[j].im, 0.001 ) )
      realRoot[i++] = complexRoot[j].re;
  }

  /* reverse above translation by adding distance to all 'k' values. */
  /*  for( j = 0; j < i; ++j )
      realRoot[j] -= VDOT(newShotPoint, newShotDir);
  */

  /* Here, 'i' is number of points found */
  switch( i )  {
  case 0:
    return(0);		/* No hit */

  default:
    bu_log("rt_superell_shot: reduced 4 to %d roots\n",i);
    bn_pr_roots( stp->st_name, complexRoot, 4 );
    return(0);		/* No hit */

  case 2:
    {
      /* Sort most distant to least distant. */
      FAST fastf_t	u;
      if( (u=realRoot[0]) < realRoot[1] )  {
	/* bubble larger towards [0] */
	realRoot[0] = realRoot[1];
	realRoot[1] = u;
      }
    }
    break;
  case 4:
    {
      register short	n;
      register short	lim;

      /*  Inline rt_pt_sort().  Sorts realRoot[] into descending order. */
      for( lim = i-1; lim > 0; lim-- )  {
	for( n = 0; n < lim; n++ )  {
	  FAST fastf_t	u;
	  if( (u=realRoot[n]) < realRoot[n+1] )  {
	    /* bubble larger towards [0] */
	    realRoot[n] = realRoot[n+1];
	    realRoot[n+1] = u;
	  }
	}
      }
    }
    break;
  }

  if (counter > 0) {
    bu_log("realroot: in %d  out %d\n", realRoot[1], realRoot[0]);
    counter--;
  }


  /* Now, t[0] > t[npts-1] */
  /* realRoot[1] is entry point, and realRoot[0] is farthest exit point */
  RT_GET_SEG(segp, ap->a_resource);
  segp->seg_stp = stp;
  segp->seg_in.hit_dist = realRoot[1];
  segp->seg_out.hit_dist = realRoot[0];
  /*  segp->seg_in.hit_surfno = segp->seg_out.hit_surfno = 0; */
  /* Set aside vector for rt_superell_norm() later */
  /*  VJOIN1( segp->seg_in.hit_vpriv, newShotPoint, realRoot[1], newShotDir ); */
  /*  VJOIN1( segp->seg_out.hit_vpriv, newShotPoint, realRoot[0], newShotDir ); */
  BU_LIST_INSERT( &(seghead->l), &(segp->l) );

  if( i == 2 ) {
    return(2);			/* HIT */
  }

  /* 4 points */
  /* realRoot[3] is entry point, and realRoot[2] is exit point */
  RT_GET_SEG(segp, ap->a_resource);
  segp->seg_stp = stp;
  segp->seg_in.hit_dist = realRoot[3]*superell->superell_e;
  segp->seg_out.hit_dist = realRoot[2]*superell->superell_e;
  segp->seg_in.hit_surfno = segp->seg_out.hit_surfno = 1;
  VJOIN1( segp->seg_in.hit_vpriv, newShotPoint, realRoot[3], newShotDir );
  VJOIN1( segp->seg_out.hit_vpriv, newShotPoint, realRoot[2], newShotDir );
  BU_LIST_INSERT( &(seghead->l), &(segp->l) );
  return(4);			/* HIT */
  /* XXX END CUT */

  /* Is there any possibility of hitting another segment?  Only when there
   * is a concave curvature (<n,e> > <2.0, 2.0>).
   */
  if ( (superell->superell_n > 2.0) || (superell->superell_e > 2.0) ) {

  }

  return 1;
}


/**
 *			R T _ S U P E R E L L _ V S H O T
 *
 *  This is the Becker vector version.
 */
void
rt_superell_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		  	       /* Number of ray/object pairs */

{
  return;
}


/**
 *  			R T _ S U P E R E L L _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_superell_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
  register struct superell_specific *superell =
    (struct superell_specific *)stp->st_specific;

  LOCAL vect_t xlated;
  LOCAL fastf_t scale;

  VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
  VSUB2( xlated, hitp->hit_point, superell->superell_V );
  MAT4X3VEC( hitp->hit_normal, superell->superell_invRSSR, xlated );
  scale = 1.0 / MAGNITUDE( hitp->hit_normal );
  VSCALE( hitp->hit_normal, hitp->hit_normal, scale );

  return;
}


/**
 *			R T _ S U P E R E L L _ C U R V E
 *
 *  Return the curvature of the superellipsoid.
 */
void
rt_superell_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
  bu_log("called rt_superell_curve!\n");
  return;
}


/**
 *  			R T _ S U P E R E L L _ U V
 *
 *  For a hit on the surface of an SUPERELL, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_superell_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
  bu_log("called rt_superell_uv!\n");
  return;
}

/**
 *			R T _ S U P E R E L L _ F R E E
 */
void
rt_superell_free(register struct soltab *stp)
{
	register struct superell_specific *superell =
		(struct superell_specific *)stp->st_specific;

	bu_free( (char *)superell, "superell_specific" );
}


int
rt_superell_class(void)
{
	return(0);
}


/**
 *			R T _ S U P E R E L L _ 1 6 P T S
 *
 * Also used by the TGC code
 */
#define SUPERELLOUT(n)	ov+(n-1)*3
void
rt_superell_16pts(register fastf_t *ov,
	     register fastf_t *V,
	     fastf_t *A,
	     fastf_t *B)
{
	static fastf_t c, d, e, f,g,h;

	e = h = .92388;			/* cos(22.5) */
	c = d = .707107;		/* cos(45) */
	g = f = .382683;		/* cos(67.5) */

	/*
	 * For angle theta, compute surface point as
	 *
	 *	V  +  cos(theta) * A  + sin(theta) * B
	 *
	 * note that sin(theta) is cos(90-theta).
	 */

	VADD2( SUPERELLOUT(1), V, A );
	VJOIN2( SUPERELLOUT(2), V, e, A, f, B );
	VJOIN2( SUPERELLOUT(3), V, c, A, d, B );
	VJOIN2( SUPERELLOUT(4), V, g, A, h, B );
	VADD2( SUPERELLOUT(5), V, B );
	VJOIN2( SUPERELLOUT(6), V, -g, A, h, B );
	VJOIN2( SUPERELLOUT(7), V, -c, A, d, B );
	VJOIN2( SUPERELLOUT(8), V, -e, A, f, B );
	VSUB2( SUPERELLOUT(9), V, A );
	VJOIN2( SUPERELLOUT(10), V, -e, A, -f, B );
	VJOIN2( SUPERELLOUT(11), V, -c, A, -d, B );
	VJOIN2( SUPERELLOUT(12), V, -g, A, -h, B );
	VSUB2( SUPERELLOUT(13), V, B );
	VJOIN2( SUPERELLOUT(14), V, g, A, -h, B );
	VJOIN2( SUPERELLOUT(15), V, c, A, -d, B );
	VJOIN2( SUPERELLOUT(16), V, e, A, -f, B );
}

/**
 *			R T _ S U P E R E L L _ P L O T
 */
int
rt_superell_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	register int		i;
	struct rt_superell_internal	*eip;
	fastf_t top[16*3];
	fastf_t middle[16*3];
	fastf_t bottom[16*3];

	RT_CK_DB_INTERNAL(ip);
	eip = (struct rt_superell_internal *)ip->idb_ptr;
	RT_SUPERELL_CK_MAGIC(eip);

	rt_superell_16pts( top, eip->v, eip->a, eip->b );
	rt_superell_16pts( bottom, eip->v, eip->b, eip->c );
	rt_superell_16pts( middle, eip->v, eip->a, eip->c );

	RT_ADD_VLIST( vhead, &top[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ )  {
		RT_ADD_VLIST( vhead, &top[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}

	RT_ADD_VLIST( vhead, &bottom[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ )  {
		RT_ADD_VLIST( vhead, &bottom[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}

	RT_ADD_VLIST( vhead, &middle[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ )  {
		RT_ADD_VLIST( vhead, &middle[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}
	return(0);
}

#if 0
static point_t	octa_verts[6] = {
	{ 1, 0, 0 },	/* XPLUS */
	{-1, 0, 0 },	/* XMINUS */
	{ 0, 1, 0 },	/* YPLUS */
	{ 0,-1, 0 },	/* YMINUS */
	{ 0, 0, 1 },	/* ZPLUS */
	{ 0, 0,-1 }	/* ZMINUS */
};

#define XPLUS 0
#define XMIN  1
#define YPLUS 2
#define YMIN  3
#define ZPLUS 4
#define ZMIN  5
#endif

/* Vertices of a unit octahedron */
/* These need to be organized properly to give reasonable normals */
/* static struct usvert {
 * 	int	a;
 * 	int	b;
 * 	int	c;
 * } octahedron[8] = {
 *     { XPLUS, ZPLUS, YPLUS },
 *     { YPLUS, ZPLUS, XMIN  },
 *     { XMIN , ZPLUS, YMIN  },
 *     { YMIN , ZPLUS, XPLUS },
 *     { XPLUS, YPLUS, ZMIN  },
 *     { YPLUS, XMIN , ZMIN  },
 *     { XMIN , YMIN , ZMIN  },
 *     { YMIN , XPLUS, ZMIN  }
 * };
 */
struct superell_state {
	struct shell	*s;
	struct rt_superell_internal	*eip;
	mat_t		invRinvS;
	mat_t		invRoS;
	fastf_t		theta_tol;
};

struct superell_vert_strip {
	int		nverts_per_strip;
	int		nverts;
	struct vertex	**vp;
	vect_t		*norms;
	int		nfaces;
	struct faceuse	**fu;
};

/**
 *			R T _ S U P E R E L L _ T E S S
 *
 *  Tesssuperellate an superellipsoid.
 *
 *  The strategy is based upon the approach of Jon Leech 3/24/89,
 *  from program "sphere", which generates a polygon mesh
 *  approximating a sphere by
 *  recursive subdivision. First approximation is an octahedron;
 *  each level of refinement increases the number of polygons by
 *  a factor of 4.
 *  Level 3 (128 polygons) is a good tradeoff if gouraud
 *  shading is used to render the database.
 *
 *  At the start, points ABC lie on surface of the unit sphere.
 *  Pick DEF as the midpoints of the three edges of ABC.
 *  Normalize the new points to lie on surface of the unit sphere.
 *
 *	  1
 *	  B
 *	 /\
 *    3 /  \ 4
 *    D/____\E
 *    /\    /\
 *   /	\  /  \
 *  /____\/____\
 * A      F     C
 * 0      5     2
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tesssuperellation.
 */
int
rt_superell_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
  bu_log("rt_superell_tess called!\n");
  return -1;
}

/**
 *			R T _ S U P E R E L L _ I M P O R T
 *
 *  Import an superellipsoid/sphere from the database format to
 *  the internal structure.
 *  Apply modeling transformations as wsuperell.
 */
int
rt_superell_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_superell_internal	*eip;
	union record		*rp;
	LOCAL fastf_t	vec[3*4 + 2];

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		bu_log("rt_superell_import: defective record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_SUPERELL;
	ip->idb_meth = &rt_functab[ID_SUPERELL];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_superell_internal), "rt_superell_internal");
	eip = (struct rt_superell_internal *)ip->idb_ptr;
	eip->magic = RT_SUPERELL_INTERNAL_MAGIC;

	/* Convert from database to internal format */
	rt_fastf_float( vec, rp->s.s_values, 4 );

	/* Apply modeling transformations */
	MAT4X3PNT( eip->v, mat, &vec[0*3] );
	MAT4X3VEC( eip->a, mat, &vec[1*3] );
	MAT4X3VEC( eip->b, mat, &vec[2*3] );
	MAT4X3VEC( eip->c, mat, &vec[3*3] );
	eip->n = rp->s.s_values[12];
	eip->e = rp->s.s_values[13];

	return(0);		/* OK */
}

/**
 *			R T _ S U P E R E L L _ E X P O R T
 */
int
rt_superell_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_superell_internal	*tip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_SUPERELL )  return(-1);
	tip = (struct rt_superell_internal *)ip->idb_ptr;
	RT_SUPERELL_CK_MAGIC(tip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "superell external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->s.s_type = SUPERELL;

	/* NOTE: This also converts to dbfloat_t */
	VSCALE( &rec->s.s_values[0], tip->v, local2mm );
	VSCALE( &rec->s.s_values[3], tip->a, local2mm );
	VSCALE( &rec->s.s_values[6], tip->b, local2mm );
	VSCALE( &rec->s.s_values[9], tip->c, local2mm );

	printf("SUPERELL: %g %g\n", tip->n, tip->e);

	rec->s.s_values[12] = tip->n;
	rec->s.s_values[13] = tip->e;

	return(0);
}

/**
 *			R T _ S U P E R E L L _ I M P O R T 5
 *
 *  Import an superellipsoid/sphere from the database format to
 *  the internal structure.
 *  Apply modeling transformations as wsuperell.
 */
int
rt_superell_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_superell_internal	*eip;
	fastf_t			vec[ELEMENTS_PER_VECT*4 + 2];

	RT_CK_DB_INTERNAL( ip );
	BU_CK_EXTERNAL( ep );

	BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * (ELEMENTS_PER_VECT*4 + 2));

	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_SUPERELL;
	ip->idb_meth = &rt_functab[ID_SUPERELL];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_superell_internal), "rt_superell_internal");

	eip = (struct rt_superell_internal *)ip->idb_ptr;
	eip->magic = RT_SUPERELL_INTERNAL_MAGIC;

	/* Convert from database (network) to internal (host) format */
	ntohd( (unsigned char *)vec, ep->ext_buf, ELEMENTS_PER_VECT*4 + 2);

	/* Apply modeling transformations */
	MAT4X3PNT( eip->v, mat, &vec[0*ELEMENTS_PER_VECT] );
	MAT4X3VEC( eip->a, mat, &vec[1*ELEMENTS_PER_VECT] );
	MAT4X3VEC( eip->b, mat, &vec[2*ELEMENTS_PER_VECT] );
	MAT4X3VEC( eip->c, mat, &vec[3*ELEMENTS_PER_VECT] );
	eip->n = vec[4*ELEMENTS_PER_VECT];
	eip->e = vec[4*ELEMENTS_PER_VECT + 1];

	return 0;		/* OK */
}

/**
 *			R T _ S U P E R E L L _ E X P O R T 5
 *
 *  The external format is:
 *	V point
 *	A vector
 *	B vector
 *	C vector
 */
int
rt_superell_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_superell_internal	*eip;
	fastf_t			vec[ELEMENTS_PER_VECT*4 + 2];

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_SUPERELL )  return(-1);
	eip = (struct rt_superell_internal *)ip->idb_ptr;
	RT_SUPERELL_CK_MAGIC(eip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * (ELEMENTS_PER_VECT*4 + 2);
	ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "superell external");

	/* scale 'em into local buffer */
	VSCALE( &vec[0*ELEMENTS_PER_VECT], eip->v, local2mm );
	VSCALE( &vec[1*ELEMENTS_PER_VECT], eip->a, local2mm );
	VSCALE( &vec[2*ELEMENTS_PER_VECT], eip->b, local2mm );
	VSCALE( &vec[3*ELEMENTS_PER_VECT], eip->c, local2mm );

	vec[4*ELEMENTS_PER_VECT] = eip->n;
	vec[4*ELEMENTS_PER_VECT + 1] = eip->e;

	/* Convert from internal (host) to database (network) format */
	htond( ep->ext_buf, (unsigned char *)vec, ELEMENTS_PER_VECT*4 + 2 );

	return 0;
}

/**
 *			R T _ S U P E R E L L _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_superell_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_superell_internal	*tip =
		(struct rt_superell_internal *)ip->idb_ptr;
	fastf_t	mag_a, mag_b, mag_c;
	char	buf[256];
	double	angles[5];
	vect_t	unitv;

	RT_SUPERELL_CK_MAGIC(tip);
	bu_vls_strcat( str, "superellipsoid (SUPERELL)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
		INTCLAMP(tip->v[X] * mm2local),
		INTCLAMP(tip->v[Y] * mm2local),
		INTCLAMP(tip->v[Z] * mm2local) );
	bu_vls_strcat( str, buf );

	mag_a = MAGNITUDE(tip->a);
	mag_b = MAGNITUDE(tip->b);
	mag_c = MAGNITUDE(tip->c);

	sprintf(buf, "\tA (%g, %g, %g) mag=%g\n",
		INTCLAMP(tip->a[X] * mm2local),
		INTCLAMP(tip->a[Y] * mm2local),
		INTCLAMP(tip->a[Z] * mm2local),
		INTCLAMP(mag_a * mm2local) );
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tB (%g, %g, %g) mag=%g\n",
		INTCLAMP(tip->b[X] * mm2local),
		INTCLAMP(tip->b[Y] * mm2local),
		INTCLAMP(tip->b[Z] * mm2local),
		INTCLAMP(mag_b * mm2local) );
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tC (%g, %g, %g) mag=%g\n",
		INTCLAMP(tip->c[X] * mm2local),
		INTCLAMP(tip->c[Y] * mm2local),
		INTCLAMP(tip->c[Z] * mm2local),
		INTCLAMP(mag_c * mm2local) );
	bu_vls_strcat( str, buf );

	sprintf(buf, "\t<n,e> (%g, %g)\n", INTCLAMP(tip->n), INTCLAMP(tip->e));
	bu_vls_strcat(str, buf);

	if( !verbose )  return(0);

	VSCALE( unitv, tip->a, 1/mag_a );
	rt_find_fallback_angle( angles, unitv );
	rt_pr_fallback_angle( str, "\tA", angles );

	VSCALE( unitv, tip->b, 1/mag_b );
	rt_find_fallback_angle( angles, unitv );
	rt_pr_fallback_angle( str, "\tB", angles );

	VSCALE( unitv, tip->c, 1/mag_c );
	rt_find_fallback_angle( angles, unitv );
	rt_pr_fallback_angle( str, "\tC", angles );

	return(0);
}

/**
 *			R T _ S U P E R E L L _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_superell_ifree(struct rt_db_internal *ip)
{
	RT_CK_DB_INTERNAL(ip);
	bu_free( ip->idb_ptr, "superell ifree" );
	ip->idb_ptr = GENPTR_NULL;
}

/*  The U parameter runs south to north.
 *  In order to orient loop CCW, need to start with 0,1-->0,0 transition
 *  at the south pole.
 */
static const fastf_t rt_superell_uvw[5*ELEMENTS_PER_VECT] = {
	0, 1, 0,
	0, 0, 0,
	1, 0, 0,
	1, 1, 0,
	0, 1, 0
};

/**
 *			R T _ S U P E R E L L _ T N U R B
 */
int
rt_superell_tnurb(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bn_tol *tol)
{
  bu_log("rt_superell_tnurb called!\n");
	return 0;
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
