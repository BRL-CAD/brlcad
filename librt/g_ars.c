/*
 *			G _ A R S . C
 *
 *  Function -
 *	Intersect a ray with an ARS (Arbitrary faceted solid)
 *  
 *  Author -
 *	Michael John Muuss
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
static char RCSars[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"
#include "./plane.h"

#define TRI_NULL	((struct tri_specific *)0)

/* The internal (in memory) form of an ARS */
struct ars_internal {
	int ncurves;
	int pts_per_curve;
	fastf_t ** curves;
};


/* Describe algorithm here */

HIDDEN fastf_t	*rt_ars_rd_curve();

HIDDEN void	rt_ars_hitsort();
extern int	rt_ars_face();

/*
 *			R T _ A R S _ I M P O R T
 *
 *  Read all the curves in as a two dimensional array.
 *  The caller is responsible for freeing the dynamic memory.
 */
int
rt_ars_import( ari, rp, mat )
struct ars_internal * ari;
union record	*rp;
matp_t		mat;
{
	register int	i, j;
	LOCAL vect_t	base_vect;
	int		currec;

	ari->ncurves = rp[0].a.a_m;
	ari->pts_per_curve = rp[0].a.a_n;

	/*
	 * Read all the curves into memory, and store their pointers
	 */
	i = (ari->ncurves+1) * sizeof(fastf_t **);
	ari->curves = (fastf_t **)rt_malloc( i, "ars curve ptrs" );
	currec = 1;
	for( i=0; i < ari->ncurves; i++ )  {
		ari->curves[i] = 
			rt_ars_rd_curve( &rp[currec], ari->pts_per_curve );
		currec += (ari->pts_per_curve+7)/8;
	}

	/*
	 * Convert from vector to point notation IN PLACE
	 * by rotating vectors and adding base vector.
	 * Observe special treatment for base vector.
	 */
	for( i = 0; i < ari->ncurves; i++ )  {
		register fastf_t *v;

		v = ari->curves[i];
		for( j = 0; j < ari->pts_per_curve; j++ )  {
			LOCAL vect_t	homog;

			if( i==0 && j == 0 )  {
				/* base vector */
				VMOVE( homog, v );
				MAT4X3PNT( base_vect, mat, homog );
				VMOVE( v, base_vect );
			}  else  {
				MAT4X3VEC( homog, mat, v );
				VADD2( v, base_vect, homog );
			}
			v += ELEMENTS_PER_VECT;
		}
		VMOVE( v, ari->curves[i] );		/* replicate first point */
	}
	return( 0 );
}

/*
 *			R T _ A R S _ P R E P
 *  
 *  This routine is used to prepare a list of planar faces for
 *  being shot at by the ars routines.
 *
 * Process an ARS, which is represented as a vector
 * from the origin to the first point, and many vectors
 * from the first point to the remaining points.
 *  
 *  This routine is unusual in that it has to read additional
 *  database records to obtain all the necessary information.
 */
int
rt_ars_prep( stp, rp, rtip )
struct soltab	*stp;
union record	*rp;
struct rt_i	*rtip;
{
	LOCAL fastf_t	dx, dy, dz;	/* For finding the bounding spheres */
	register int	i, j;
	register fastf_t **curves;	/* array of curve base addresses */
	LOCAL fastf_t	f;
	LOCAL struct ars_internal ari;

	i = rt_ars_import( &ari, rp, stp->st_pathmat );

	if ( i < 0) {
		rt_log("rt_ars_prep(%s): db import failure\n", stp->st_name);
		return(-1);
	}

	/*
	 * Compute bounding sphere.
	 * Find min and max of the point co-ordinates.
	 */
	VSETALL( stp->st_max, -INFINITY );
	VSETALL( stp->st_min,  INFINITY );

	for( i = 0; i < ari.ncurves; i++ )  {
		register fastf_t *v;

		v = ari.curves[i];
		for( j = 0; j < ari.pts_per_curve; j++ )  {
			VMINMAX( stp->st_min, stp->st_max, v );
			v += ELEMENTS_PER_VECT;
		}
	}
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
	stp->st_specific = (genptr_t) 0;

	/*
	 *  Compute planar faces
	 *  Will examine curves[i][pts_per_curve], provided by rt_ars_rd_curve.
	 */
	for( i = 0; i < ari.ncurves-1; i++ )  {
		register fastf_t *v1, *v2;

		v1 = ari.curves[i];
		v2 = ari.curves[i+1];
		for( j = 0; j < ari.pts_per_curve;
		    j++, v1 += ELEMENTS_PER_VECT, v2 += ELEMENTS_PER_VECT )  {
		    	/* carefully make faces, w/inward pointing normals */
			rt_ars_face( stp,
				&v1[0],
				&v2[ELEMENTS_PER_VECT],
				&v1[ELEMENTS_PER_VECT] );
			rt_ars_face( stp,
				&v2[0],
				&v2[ELEMENTS_PER_VECT],
				&v1[0] );
		}
	}

	/*
	 *  Free storage for faces
	 */
	for( i = 0; i < ari.ncurves; i++ )  {
		rt_free( (char *)ari.curves[i], "ars curve" );
	}
	rt_free( (char *)ari.curves, "ars curve ptrs" );

	return(0);		/* OK */
}

/*
 *			R T _ A R S _ R D _ C U R V E
 *
 *  rt_ars_rd_curve() reads a set of ARS B records and returns a pointer
 *  to a malloc()'ed memory area of fastf_t's to hold the curve.
 */
fastf_t *
rt_ars_rd_curve(rp, npts)
union record	*rp;
int		npts;
{
	LOCAL int lim;
	LOCAL fastf_t *base;
	register fastf_t *fp;		/* pointer to temp vector */
	register int i;
	LOCAL union record *rr;
	int	rec;

	/* Leave room for first point to be repeated */
	base = fp = (fastf_t *)rt_malloc(
	    (npts+1) * sizeof(fastf_t) * ELEMENTS_PER_VECT,
	    "ars curve" );

	rec = 0;
	for( ; npts > 0; npts -= 8 )  {
		rr = &rp[rec++];
		if( rr->b.b_id != ID_ARS_B )  {
			rt_log("rt_ars_rd_curve():  non-ARS_B record!\n");
			break;
		}
		lim = (npts>8) ? 8 : npts;
		for( i=0; i<lim; i++ )  {
			/* cvt from dbfloat_t */
			VMOVE( fp, (&(rr->b.b_values[i*3])) );
			fp += ELEMENTS_PER_VECT;
		}
	}
	return( base );
}

/*
 *			R T _ A R S _ F A C E
 *
 *  This function is called with pointers to 3 points,
 *  and is used to prepare ARS faces.
 *  ap, bp, cp point to vect_t points.
 *
 * Return -
 *	0	if the 3 points didn't form a plane (eg, colinear, etc).
 *	#pts	(3) if a valid plane resulted.
 */
int
rt_ars_face( stp, ap, bp, cp )
struct soltab *stp;
pointp_t ap, bp, cp;
{
	register struct tri_specific *trip;
	vect_t work;
	LOCAL fastf_t m1, m2, m3, m4;

	GETSTRUCT( trip, tri_specific );
	VMOVE( trip->tri_A, ap );
	VSUB2( trip->tri_BA, bp, ap );
	VSUB2( trip->tri_CA, cp, ap );
	VCROSS( trip->tri_wn, trip->tri_BA, trip->tri_CA );

	/* Check to see if this plane is a line or pnt */
	m1 = MAGNITUDE( trip->tri_BA );
	m2 = MAGNITUDE( trip->tri_CA );
	VSUB2( work, bp, cp );
	m3 = MAGNITUDE( work );
	m4 = MAGNITUDE( trip->tri_wn );
	if( NEAR_ZERO(m1,0.0001) || NEAR_ZERO(m2,0.0001) ||
	    NEAR_ZERO(m3,0.0001) || NEAR_ZERO(m4,0.0001) )  {
		rt_free( (char *)trip, "tri_specific struct");
		if( rt_g.debug & DEBUG_ARB8 )
			rt_log("ars(%s): degenerate facet\n", stp->st_name);
		return(0);			/* BAD */
	}		

	/*
	 *  wn is an inward pointing normal, of non-unit length.
	 *  tri_N is a unit length outward pointing normal.
	 */
	VREVERSE( trip->tri_N, trip->tri_wn );
	VUNITIZE( trip->tri_N );

	/* Add this face onto the linked list for this solid */
	trip->tri_forw = (struct tri_specific *)stp->st_specific;
	stp->st_specific = (genptr_t)trip;
	return(3);				/* OK */
}

/*
 *  			R T _ A R S _ P R I N T
 */
void
rt_ars_print( stp )
register struct soltab *stp;
{
	register struct tri_specific *trip =
		(struct tri_specific *)stp->st_specific;

	if( trip == TRI_NULL )  {
		rt_log("ars(%s):  no faces\n", stp->st_name);
		return;
	}
	do {
		VPRINT( "A", trip->tri_A );
		VPRINT( "B-A", trip->tri_BA );
		VPRINT( "C-A", trip->tri_CA );
		VPRINT( "BA x CA", trip->tri_wn );
		VPRINT( "Normal", trip->tri_N );
		rt_log("\n");
	} while( trip = trip->tri_forw );
}

/*
 *			R T _ A R S _ S H O T
 *  
 * Function -
 *	Shoot a ray at an ARS.
 *  
 * Returns -
 *	0	MISS
 *  	segp	HIT
 */
struct seg *
rt_ars_shot( stp, rp, ap )
struct soltab *stp;
register struct xray *rp;
struct application	*ap;
{
	register struct tri_specific *trip =
		(struct tri_specific *)stp->st_specific;
#define MAXHITS 12		/* # surfaces hit, must be even */
	LOCAL struct hit hits[MAXHITS];
	register struct hit *hp;
	LOCAL int	nhits;

	nhits = 0;
	hp = &hits[0];

	/* consider each face */
	for( ; trip; trip = trip->tri_forw )  {
		FAST fastf_t	dn;		/* Direction dot Normal */
		LOCAL fastf_t	abs_dn;
		FAST fastf_t	k;
		LOCAL fastf_t	alpha, beta;
		LOCAL fastf_t	ds;
		LOCAL vect_t	wxb;		/* vertex - ray_start */
		LOCAL vect_t	xp;		/* wxb cross ray_dir */

		/*
		 *  Ray Direction dot N.  (wn is inward pointing normal)
		 */
		dn = VDOT( trip->tri_wn, rp->r_dir );
		if( rt_g.debug & DEBUG_ARB8 )
			rt_log("N.Dir=%g ", dn );

		/*
		 *  If ray lies directly along the face, drop this face.
		 */
		abs_dn = dn >= 0.0 ? dn : (-dn);
		if( abs_dn < 1.0e-10 )
			continue;
		VSUB2( wxb, trip->tri_A, rp->r_pt );
		VCROSS( xp, wxb, rp->r_dir );

		/* Check for exceeding along the one side */
		alpha = VDOT( trip->tri_CA, xp );
		if( dn < 0.0 )  alpha = -alpha;
		if( alpha < 0.0 || alpha > abs_dn )
			continue;

		/* Check for exceeding along the other side */
		beta = VDOT( trip->tri_BA, xp );
		if( dn > 0.0 )  beta = -beta;
		if( beta < 0.0 || beta > abs_dn )
			continue;
		if( alpha+beta > abs_dn )
			continue;
		ds = VDOT( wxb, trip->tri_wn );
		k = ds / dn;		/* shot distance */

		/* For hits other than the first one, might check
		 *  to see it this is approx. equal to previous one */

		/*  If dn < 0, we should be entering the solid.
		 *  However, we just assume in/out sorting later will work.
		 */
		hp->hit_dist = k;
		hp->hit_private = (char *)trip;
		hp->hit_vpriv[X] = dn;

		if(rt_g.debug&DEBUG_ARB8) rt_log("ars: dist k=%g, ds=%g, dn=%g\n", k, ds, dn );

		/* Bug fix: next line was "nhits++".  This caused ars_hitsort
			to exceed bounds of "hits" array by one member and
			clobber some stack variables i.e. "stp" -- GSM */
		if( ++nhits >= MAXHITS )  {
			rt_log("ars(%s): too many hits\n", stp->st_name);
			break;
		}
		hp++;
	}
	if( nhits == 0 )
		return(SEG_NULL);		/* MISS */

	/* Sort hits, Near to Far */
	rt_ars_hitsort( hits, nhits );

	if( nhits&1 )  {
		register int i;
		/*
		 * If this condition exists, it is almost certainly due to
		 * the dn==0 check above.  Just log error.
		 */
		rt_log("ERROR: ars(%s): %d hits odd, skipping solid\n",
			stp->st_name, nhits);
		for(i=0; i < nhits; i++ )
			rt_log("k=%g dn=%g\n",
				hits[i].hit_dist, hp->hit_vpriv[X]);
		return(SEG_NULL);		/* MISS */
	}

	/* nhits is even, build segments */
	{
		register struct seg *segp;
		register int	i,j;

		/* Check in/out properties */
		for( i=nhits; i > 0; i -= 2 )  {
			if( hits[i-2].hit_vpriv[X] >= 0 )
				continue;		/* seg_in */
			if( hits[i-1].hit_vpriv[X] <= 0 )
				continue;		/* seg_out */
		   	rt_log("ars(%s): in/out error\n", stp->st_name );
			for( j=nhits-1; j >= 0; j-- )  {
		   		rt_log("%d %s dist=%g dn=%g\n",
					j,
					((hits[j].hit_vpriv[X] > 0) ?
		   				" In" : "Out" ),
			   		hits[j].hit_dist,
					hits[j].hit_vpriv[X] );
		   	}
#ifdef CONSERVATIVE
		   	return(SEG_NULL);
#else
			/* For now, just chatter, and return *something* */
			break;
#endif
		}

		segp = SEG_NULL;
		for( i=nhits; i > 0; i -= 2 )  {
			register struct seg *newseg;

			GET_SEG(newseg, ap->a_resource);
			newseg->seg_next = segp;
			segp = newseg;
			segp->seg_stp = stp;
			segp->seg_in = hits[i-2];	/* struct copy */
			segp->seg_out = hits[i-1];	/* struct copy */
		}
		return(segp);			/* HIT */
	}
	/* NOTREACHED */
}

/*
 *			R T _ A R S _ H I T S O R T
 */
HIDDEN void
rt_ars_hitsort( h, nh )
register struct hit h[];
register int nh;
{
	register int i, j;
	LOCAL struct hit temp;

	for( i=0; i < nh-1; i++ )  {
		for( j=i+1; j < nh; j++ )  {
			if( h[i].hit_dist <= h[j].hit_dist )
				continue;
			temp = h[j];		/* struct copy */
			h[j] = h[i];		/* struct copy */
			h[i] = temp;		/* struct copy */
		}
	}
}

/*
 *  			R T _ A R S _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_ars_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct tri_specific *trip =
		(struct tri_specific *)hitp->hit_private;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	VMOVE( hitp->hit_normal, trip->tri_N );
}

/*
 *			R T _ A R S _ C U R V E
 *
 *  Return the "curvature" of the ARB face.
 *  Pick a principle direction orthogonal to normal, and 
 *  indicate no curvature.
 */
void
rt_ars_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct tri_specific *trip =
		(struct tri_specific *)hitp->hit_private;

	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/*
 *  			R T _ A R S _ U V
 *  
 *  For a hit on a face of an ARB, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the Xbasis direction defined by B-A,
 *  v extends along the "Ybasis" direction defined by (B-A)xN.
 */
void
rt_ars_uv( ap, stp, hitp, uvp )
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	register struct tri_specific *trip =
		(struct tri_specific *)hitp->hit_private;
	LOCAL vect_t P_A;
	LOCAL fastf_t r;
	LOCAL fastf_t xxlen, yylen;

	xxlen = MAGNITUDE(trip->tri_BA);
	yylen = MAGNITUDE(trip->tri_CA);

	VSUB2( P_A, hitp->hit_point, trip->tri_A );
	/* Flipping v is an artifact of how the faces are built */
	uvp->uv_u = VDOT( P_A, trip->tri_BA ) * xxlen;
	uvp->uv_v = 1.0 - ( VDOT( P_A, trip->tri_CA ) * yylen );
	if( uvp->uv_u < 0 || uvp->uv_v < 0 )  {
		if( rt_g.debug )
			rt_log("rt_ars_uv: bad uv=%g,%g\n", uvp->uv_u, uvp->uv_v);
		/* Fix it up */
		if( uvp->uv_u < 0 )  uvp->uv_u = (-uvp->uv_u);
		if( uvp->uv_v < 0 )  uvp->uv_v = (-uvp->uv_v);
	}
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
	uvp->uv_du = r * xxlen;
	uvp->uv_dv = r * yylen;
}

/*
 *			R T _ A R S _ F R E E
 */
void
rt_ars_free( stp )
register struct soltab *stp;
{
	register struct tri_specific *trip =
		(struct tri_specific *)stp->st_specific;

	while( trip != TRI_NULL )  {
		register struct tri_specific *nexttri = trip->tri_forw;

		rt_free( (char *)trip, "ars tri_specific");
		trip = nexttri;
	}
}

int
rt_ars_class()
{
	return(0);
}

/*
 *			R T _ A R S _ P L O T
 */
int
rt_ars_plot( rp, mat, vhead, dp )
union record	*rp;
mat_t		mat;
struct vlhead	*vhead;
struct directory *dp;
{
	register int	i;
	register int	j;
	struct ars_internal ari;

	i = rt_ars_import(&ari, rp, mat );

	if ( i < 0) {
		rt_log("rt_ars_plot: db import failure\n");
		return(-1);
	}

	/*
	 *  Draw the "waterlines", by tracing each curve.
	 *  n+1th point is first point replicated by code above.
	 */
	for( i = 0; i < ari.ncurves; i++ )  {
		register fastf_t *v1;

		v1 = ari.curves[i];
		ADD_VL( vhead, v1, 0 );
		v1 += ELEMENTS_PER_VECT;
		for( j = 1; j <= ari.pts_per_curve; j++, v1 += ELEMENTS_PER_VECT )
			ADD_VL( vhead, v1, 1 );
	}

	/*
	 *  Connect the Ith points on each curve, to make a mesh.
	 */
	for( i = 0; i < ari.pts_per_curve; i++ )  {
		ADD_VL( vhead, &ari.curves[0][i*ELEMENTS_PER_VECT], 0 );
		for( j = 1; j < ari.ncurves; j++ )
			ADD_VL( vhead, &ari.curves[j][i*ELEMENTS_PER_VECT], 1 );
	}

	/*
	 *  Free storage for faces
	 */
	for( i = 0; i < ari.ncurves; i++ )  {
		rt_free( (char *)ari.curves[i], "ars curve" );
	}
	rt_free( (char *)ari.curves, "ars curves[]" );
	return(0);
}

int
rt_ars_tess()
{
	return(-1);
}
