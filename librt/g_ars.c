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
#include "./plane.h"

#define TRI_NULL	((struct tri_specific *)0)

/* Describe algorithm here */

HIDDEN fastf_t	*rt_ars_rd_curve();

HIDDEN void	rt_ars_hitsort();
extern int	rt_ars_face();

/*
 *			R T _ A R S _ I M P O R T
 *
 *  Read all the curves in as a two dimensional array.
 *  The caller is responsible for freeing the dynamic memory.
 *
 *  Note that in each curve array, the first point is replicated
 *  as the last point, to make processing the data easier.
 */
int
rt_ars_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
CONST mat_t			mat;
{
	struct rt_ars_internal *ari;
	union record	*rp;
	register int	i, j;
	LOCAL vect_t	base_vect;
	int		currec;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != ID_ARS_A )  {
		rt_log("rt_ars_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_ARS;
	ip->idb_ptr = rt_malloc(sizeof(struct rt_ars_internal), "rt_ars_internal");
	ari = (struct rt_ars_internal *)ip->idb_ptr;
	ari->magic = RT_ARS_INTERNAL_MAGIC;
	ari->ncurves = rp[0].a.a_m;
	ari->pts_per_curve = rp[0].a.a_n;

	/*
	 * Read all the curves into internal form.
	 */
	ari->curves = (fastf_t **)rt_malloc(
		(ari->ncurves+1) * sizeof(fastf_t **), "ars curve ptrs" );
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
 *			R T _ A R S _ E X P O R T
 *
 *  The name will be added by the caller.
 *  Generally, only libwdb will set conv2mm != 1.0
 */
int
rt_ars_export( ep, ip, local2mm )
struct rt_external	*ep;
struct rt_db_internal	*ip;
double			local2mm;
{
	struct rt_ars_internal	*arip;
	union record		*rec;
	point_t		base_pt;
	int		per_curve_grans;
	int		cur;		/* current curve number */
	int		gno;		/* current granule number */

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_ARS )  return(-1);
	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	per_curve_grans = (arip->pts_per_curve+7)/8;

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = (1 + per_curve_grans * arip->ncurves) *
		sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "ars external");
	rec = (union record *)ep->ext_buf;

	rec[0].a.a_id = ID_ARS_A;
	rec[0].a.a_type = ARS;			/* obsolete? */
	rec[0].a.a_m = arip->ncurves;
	rec[0].a.a_n = arip->pts_per_curve;
	rec[0].a.a_curlen = per_curve_grans;
	rec[0].a.a_totlen = per_curve_grans * arip->ncurves;

	VMOVE( base_pt, &arip->curves[0][0] );
	gno = 1;
	for( cur=0; cur<arip->ncurves; cur++ )  {
		register fastf_t	*fp;
		int			npts;
		int			left;

		fp = arip->curves[cur];
		left = arip->pts_per_curve;
		for( npts=0; npts < arip->pts_per_curve; npts+=8, left -= 8 )  {
			register int	el;
			register int	lim;
			register struct ars_ext	*bp = &rec[gno].b;

			bp->b_id = ID_ARS_B;
			bp->b_type = ARSCONT;	/* obsolete? */
			bp->b_n = cur+1;		/* obsolete? */
			bp->b_ngranule = (npts/8)+1; /* obsolete? */

			lim = (left > 8 ) ? 8 : left;
			for( el=0; el < lim; el++ )  {
				vect_t	diff;
				if( cur==0 && npts==0 && el==0 )
					VSCALE( diff , fp , local2mm )
				else
					VSUB2SCALE( diff, fp, base_pt, local2mm )
				/* NOTE: also type converts to dbfloat_t */
				VMOVE( &(bp->b_values[el*3]), diff );
				fp += ELEMENTS_PER_VECT;
			}
			gno++;
		}
	}
	return(0);
}

/*
 *			R T _ A R S _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_ars_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register int			j;
	register struct rt_ars_internal	*arip =
		(struct rt_ars_internal *)ip->idb_ptr;
	char				buf[256];
	int				i;

	RT_ARS_CK_MAGIC(arip);
	rt_vls_strcat( str, "arbitrary rectangular solid (ARS)\n");

	sprintf(buf, "\t%d curves, %d points per curve\n",
		arip->ncurves, arip->pts_per_curve );
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tV (%g, %g, %g)\n",
		arip->curves[0][X] * mm2local,
		arip->curves[0][Y] * mm2local,
		arip->curves[0][Z] * mm2local );
	rt_vls_strcat( str, buf );

	if( !verbose )  return(0);

	/* Print out all the points */
	for( i=0; i < arip->ncurves; i++ )  {
		register fastf_t *v = arip->curves[i];

		sprintf( buf, "\tCurve %d:\n", i );
		rt_vls_strcat( str, buf );
		for( j=0; j < arip->pts_per_curve; j++ )  {
			sprintf(buf, "\t\t(%g, %g, %g)\n",
				v[X] * mm2local,
				v[Y] * mm2local,
				v[Z] * mm2local );
			rt_vls_strcat( str, buf );
			v += ELEMENTS_PER_VECT;
		}
	}

	return(0);
}

/*
 *			R T _ A R S _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_ars_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_ars_internal	*arip;
	register int			i;

	RT_CK_DB_INTERNAL(ip);
	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	/*
	 *  Free storage for faces
	 */
	for( i = 0; i < arip->ncurves; i++ )  {
		rt_free( (char *)arip->curves[i], "ars curve" );
	}
	rt_free( (char *)arip->curves, "ars curve ptrs" );
	arip->magic = 0;		/* sanity */
	arip->ncurves = 0;
	rt_free( (char *)arip, "ars ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
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
rt_ars_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	LOCAL fastf_t	dx, dy, dz;	/* For finding the bounding spheres */
	register int	i, j;
	LOCAL fastf_t	f;
	struct rt_ars_internal	*arip;

	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	/*
	 * Compute bounding sphere.
	 * Find min and max of the point co-ordinates.
	 */
	VSETALL( stp->st_max, -INFINITY );
	VSETALL( stp->st_min,  INFINITY );

	for( i = 0; i < arip->ncurves; i++ )  {
		register fastf_t *v;

		v = arip->curves[i];
		for( j = 0; j < arip->pts_per_curve; j++ )  {
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
	for( i = 0; i < arip->ncurves-1; i++ )  {
		register fastf_t *v1, *v2;

		v1 = arip->curves[i];
		v2 = arip->curves[i+1];
		for( j = 0; j < arip->pts_per_curve;
		    j++, v1 += ELEMENTS_PER_VECT, v2 += ELEMENTS_PER_VECT )  {
		    	/* carefully make faces, w/inward pointing normals */
			rt_ars_face( stp,
				&v1[0],				/* [0][0] */
				&v2[ELEMENTS_PER_VECT],		/* [1][1] */
				&v1[ELEMENTS_PER_VECT] );	/* [0][1] */
			rt_ars_face( stp,
				&v2[0],				/* [1][0] */
				&v2[ELEMENTS_PER_VECT],		/* [1][1] */
				&v1[0] );			/* [0][0] */
		}
	}

	rt_ars_ifree( ip );

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
register CONST struct soltab *stp;
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
 *  	!0	HIT
 */
int
rt_ars_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct tri_specific *trip =
		(struct tri_specific *)stp->st_specific;
#define RT_ARS_MAXHITS 128		/* # surfaces hit, must be even */
	LOCAL struct hit hits[RT_ARS_MAXHITS];
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
		if( ++nhits >= RT_ARS_MAXHITS )  {
			rt_log("ars(%s): too many hits\n", stp->st_name);
			break;
		}
		hp++;
	}
	if( nhits == 0 )
		return(0);		/* MISS */

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
		return(0);		/* MISS */
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

#ifndef CONSERVATIVE
			/* if this segment is small enough, just swap the in/out hits */
			if( (hits[i-1].hit_dist - hits[i-2].hit_dist) < 200.0*RT_LEN_TOL )
			{
				struct hit temp;
				fastf_t temp_dist;

				temp_dist = hits[i-1].hit_dist;
				hits[i-1].hit_dist = hits[i-2].hit_dist;
				hits[i-2].hit_dist = temp_dist;

				temp = hits[i-1];	/* struct copy */
				hits[i-1] = hits[i-2];	/* struct copy */
				hits[i-2] = temp;	/* struct copy */
				continue;
			}
#endif
		   	rt_log("ars(%s): in/out error\n", stp->st_name );
			for( j=nhits-1; j >= 0; j-- )  {
		   		rt_log("%d %s dist=%g dn=%g\n",
					j,
					((hits[j].hit_vpriv[X] > 0) ?
		   				" In" : "Out" ),
			   		hits[j].hit_dist,
					hits[j].hit_vpriv[X] );
				if( j>0 )
					rt_log( "\tseg length = %g\n", hits[j].hit_dist - hits[j-1].hit_dist );
		   	}
#ifdef CONSERVATIVE
		   	return(0);
#else
			/* For now, just chatter, and return *something* */
			break;
#endif
		}

		for( i=nhits; i > 0; i -= 2 )  {
			RT_GET_SEG(segp, ap->a_resource);
			segp->seg_stp = stp;
			segp->seg_in = hits[i-2];	/* struct copy */
			segp->seg_out = hits[i-1];	/* struct copy */
			RT_LIST_INSERT( &(seghead->l), &(segp->l) );
		}
	}
	return(nhits);			/* HIT */
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
/*	register struct tri_specific *trip =
 *		(struct tri_specific *)hitp->hit_private;
 */
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
rt_ars_plot( vhead, ip, ttol, tol )
struct rt_list	*vhead;
struct rt_db_internal *ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	register int	i;
	register int	j;
	struct rt_ars_internal	*arip;

	RT_CK_DB_INTERNAL(ip);
	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	/*
	 *  Draw the "waterlines", by tracing each curve.
	 *  n+1th point is first point replicated by code above.
	 */
	for( i = 0; i < arip->ncurves; i++ )  {
		register fastf_t *v1;

		v1 = arip->curves[i];
		RT_ADD_VLIST( vhead, v1, RT_VLIST_LINE_MOVE );
		v1 += ELEMENTS_PER_VECT;
		for( j = 1; j <= arip->pts_per_curve; j++, v1 += ELEMENTS_PER_VECT )
			RT_ADD_VLIST( vhead, v1, RT_VLIST_LINE_DRAW );
	}

	/*
	 *  Connect the Ith points on each curve, to make a mesh.
	 */
	for( i = 0; i < arip->pts_per_curve; i++ )  {
		RT_ADD_VLIST( vhead, &arip->curves[0][i*ELEMENTS_PER_VECT], RT_VLIST_LINE_MOVE );
		for( j = 1; j < arip->ncurves; j++ )
			RT_ADD_VLIST( vhead, &arip->curves[j][i*ELEMENTS_PER_VECT], RT_VLIST_LINE_DRAW );
	}

	return(0);
}

#define IJ(ii,jj)	(((i+(ii))*(arip->pts_per_curve+1))+(j+(jj)))
#define ARS_PT(ii,jj)	(&arip->curves[i+(ii)][(j+(jj))*ELEMENTS_PER_VECT])
#define FIND_IJ(a,b)	\
	if( !(verts[IJ(a,b)]) )  { \
		verts[IJ(a,b)] = \
		nmg_find_pt_in_shell( s, ARS_PT(a,b), tol ); \
	}
#define ASSOC_GEOM(corn, a,b)	\
	if( !((*corners[corn])->vg_p) )  { \
		nmg_vertex_gv( *(corners[corn]), ARS_PT(a,b) ); \
	}
/*
 *			R T _ A R S _ T E S S
 */
int
rt_ars_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	register int	i;
	register int	j;
	register int	k;
	struct rt_ars_internal	*arip;
	struct shell	*s;
	struct vertex	**verts;
	struct faceuse	*fu;
	struct nmg_ptbl	kill_fus;
	struct nmg_ptbl tbl;
	int		bad_ars=0;

	RT_CK_DB_INTERNAL(ip);
	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	/* Check for a legal ARS */
	for( i = 0; i < arip->ncurves-1; i++ )
	{
		for( j = 2; j < arip->pts_per_curve; j++ )
		{
			fastf_t dist;
			vect_t pca;
			int code;

			if( VAPPROXEQUAL( ARS_PT(0,-2), ARS_PT(0,-1), tol->dist ) )
				continue;

			code = rt_dist_pt3_lseg3( &dist, pca, ARS_PT(0,-2), ARS_PT(0,-1), ARS_PT(0,0), tol );

			if( code < 2 )
			{
				rt_log( "ARS curve backtracks on itself!!!\n" );
				rt_log( "\tCurve #%d, points #%d through %d are:\n", i, j-2, j );
				rt_log( "\t\t%d (%f %f %f)\n", j-2, V3ARGS( ARS_PT(0,-2) ) );
				rt_log( "\t\t%d (%f %f %f)\n", j-1, V3ARGS( ARS_PT(0,-1) ) );
				rt_log( "\t\t%d (%f %f %f)\n", j, V3ARGS( ARS_PT(0,0) ) );
				bad_ars = 1;
				j++;
			}
		}
	}

	if( bad_ars )
	{
		rt_log( "TESSELATION FAILURE: This ARS solid has not been tesselated.\n\tAny result you may obtain is incorrect.\n" );
		return( -1 );
	}

	nmg_tbl( &kill_fus, TBL_INIT, (long *)NULL );

	/* Build the topology of the ARS.  Start by allocating storage */

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = RT_LIST_FIRST(shell, &(*r)->s_hd);

	verts = (struct vertex **)rt_calloc( arip->ncurves * (arip->pts_per_curve+1),
		sizeof(struct vertex *),
		"rt_tor_tess *verts[]" );

	/*
	 *  Draw the "waterlines", by tracing each curve.
	 *  n+1th point is first point replicated by import code.
	 */
	k = arip->pts_per_curve-2;	/* next to last point on curve */
	for( i = 0; i < arip->ncurves-1; i++ )  {
		int double_ended;

		if( k != 1 && VAPPROXEQUAL( &arip->curves[i][1*ELEMENTS_PER_VECT], &arip->curves[i][k*ELEMENTS_PER_VECT], tol->dist ) )
			double_ended = 1;
		else
			double_ended = 0;

		for( j = 0; j < arip->pts_per_curve; j++ )  {
			struct vertex **corners[3];


			if( double_ended &&
			     i != 0 &&
			     ( j == 0 || j == k || j == arip->pts_per_curve-1 ) )
					continue;

			/*
			 *  First triangular face
			 */
			if( rt_3pts_distinct( ARS_PT(0,0), ARS_PT(1,1),
				ARS_PT(0,1), tol )
			   && !rt_3pts_collinear( ARS_PT(0,0), ARS_PT(1,1),
				ARS_PT(0,1), tol )
			)  {
				/* Locate these points, if previously mentioned */
				FIND_IJ(0, 0);
				FIND_IJ(1, 1);
				FIND_IJ(0, 1);

				/* Construct first face topology, CCW order */
				corners[0] = &verts[IJ(0,0)];
				corners[1] = &verts[IJ(0,1)];
				corners[2] = &verts[IJ(1,1)];

				if( (fu = nmg_cmface( s, corners, 3 )) == (struct faceuse *)0 )  {
					rt_log("rt_ars_tess() nmg_cmface failed, skipping face a[%d][%d]\n",
						i,j);
				}

				/* Associate vertex geometry, if new */
				ASSOC_GEOM( 0, 0, 0 );
				ASSOC_GEOM( 1, 0, 1 );
				ASSOC_GEOM( 2, 1, 1 );
				if( nmg_calc_face_g( fu ) )
				{
					rt_log( "Degenerate face created, will kill it later\n" );
					nmg_tbl( &kill_fus, TBL_INS, (long *)fu );
				}
			}

			/*
			 *  Second triangular face
			 */
			if( rt_3pts_distinct( ARS_PT(1,0), ARS_PT(1,1),
				ARS_PT(0,0), tol )
			   && !rt_3pts_collinear( ARS_PT(1,0), ARS_PT(1,1),
				ARS_PT(0,0), tol )
			)  {
				/* Locate these points, if previously mentioned */
				FIND_IJ(1, 0);
				FIND_IJ(1, 1);
				FIND_IJ(0, 0);

				/* Construct second face topology, CCW */
				corners[0] = &verts[IJ(1,0)];
				corners[1] = &verts[IJ(0,0)];
				corners[2] = &verts[IJ(1,1)];

				if( (fu = nmg_cmface( s, corners, 3 )) == (struct faceuse *)0 )  {
					rt_log("rt_ars_tess() nmg_cmface failed, skipping face b[%d][%d]\n",
						i,j);
				}

				/* Associate vertex geometry, if new */
				ASSOC_GEOM( 0, 1, 0 );
				ASSOC_GEOM( 1, 0, 0 );
				ASSOC_GEOM( 2, 1, 1 );
				if( nmg_calc_face_g( fu ) )
				{
					rt_log( "Degenerate face created, will kill it later\n" );
					nmg_tbl( &kill_fus, TBL_INS, (long *)fu );
				}
			}
		}
	}

	rt_free( (char *)verts, "rt_ars_tess *verts[]" );

	/* kill any degenerate faces that may have been created */
	for( i=0 ; i<NMG_TBL_END( &kill_fus ) ; i++ )
	{
		fu = (struct faceuse *)NMG_TBL_GET( &kill_fus, i );
		NMG_CK_FACEUSE( fu );
		(void)nmg_kfu( fu );
	}

	/* ARS solids are often built with incorrect face normals.
	 * Don't depend on them to be correct.
	 */
	nmg_fix_normals( s , tol );

	/* set edge's is_real flag */
	nmg_mark_edges_real( &s->l );

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r, tol );

	return(0);
}
