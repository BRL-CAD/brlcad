/*
 *			G _ A R B N . C
 *  
 *  Function -
 *  	Intersect a ray with an Arbitrary Regular Polyhedron with
 *	an arbitrary number of faces.
 *  
 *  Author -
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
static char RCSarbn[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nmg.h"
#include "db.h"
#include "./debug.h"


#define DIST_TOL	(1.0e-8)
#define DIST_TOL_SQ	(1.0e-10)

struct arbn_internal  {
	long	magic;
	int	neqn;
	plane_t	*eqn;
};
#define RT_ARBN_INTERNAL_MAGIC	0x18236461
#define RT_ARBN_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ARBN_INTERNAL_MAGIC,"arbn_internal")

RT_EXTERN(void rt_arbn_print, (struct soltab *stp) );
RT_EXTERN(void rt_arbn_ifree, (struct rt_db_internal *ip) );

/*
 *  			R T _ A R B N _ P R E P
 *
 *  Returns -
 *	 0	OK
 *	!0	failure
 */
#if NEW_IF
int
rt_arbn_prep( stp, ep, rtip )
struct soltab		*stp;
struct rt_external	*ep;
struct rt_i		*rtip;
{
#else
int
rt_arbn_prep( stp, rec, rtip )
struct soltab	*stp;
union record	*rec;
struct rt_i	*rtip;
{
	struct rt_external	ext, *ep;
#endif
	struct arbn_internal	*aip;
	struct rt_db_internal	intern, *ip;
	vect_t		work;
	fastf_t		f;
	register int	i;
	int		j;
	int		k;
	int		*used = (int *)0;	/* plane eqn use count */

#if NEW_IF
	/* All set */
#else
	ep = &ext;
	RT_INIT_EXTERNAL(ep);
	ep->ext_buf = (genptr_t)rec;
	ep->ext_nbytes = stp->st_dp->d_len*sizeof(union record);
	ip = &intern;
	if( rt_arbn_import( ip, ep, stp->st_pathmat ) < 0 )  {
		rt_log("arbn(%s): db import error\n", stp->st_name );
		return(-1);		/* BAD */
	}
#endif
	RT_CK_DB_INTERNAL( ip );
	aip = (struct arbn_internal *)intern.idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	stp->st_specific = (genptr_t)aip;
	used = (int *)rt_malloc(aip->neqn*sizeof(int), "arbn used[]");

	/*
	 *  ARBN must be convex.  Test for concavity.
	 *  Byproduct is an enumeration of all the verticies,
	 *  which are used to make the bounding RPP.
	 */

	/* Zero face use counts */
	for( i=0; i<aip->neqn; i++ )  {
		used[i] = 0;
	}
	for( i=0; i<aip->neqn-2; i++ )  {
		for( j=i+1; j<aip->neqn-1; j++ )  {
			double	dot;

			/* If normals are parallel, no intersection */
			dot = VDOT( aip->eqn[i], aip->eqn[j] );
			if( !NEAR_ZERO( dot, 0.999999 ) )  continue;

			/* Have an edge line, isect with higher numbered planes */
			for( k=j+1; k<aip->neqn; k++ )  {
				register int	m;
				point_t		pt;

				if( rt_mkpoint_3planes( pt, aip->eqn[i], aip->eqn[j], aip->eqn[k] ) < 0 )  continue;

				/* See if point is outside arb */
				for( m=0; m<aip->neqn; m++ )  {
					if( i==m || j==m || k==m )  continue;
					if( VDOT(pt, aip->eqn[m])-aip->eqn[m][3] > DIST_TOL )
						goto next_k;
				}
				VMINMAX( stp->st_min, stp->st_max, pt );

				/* Increment "face used" counts */
				used[i]++;
				used[j]++;
				used[k]++;
next_k:				;
			}
		}
	}

	/* If any planes were not used, then arbn is not convex */
	for( i=0; i<aip->neqn; i++ )  {
		if( used[i] != 0 )  continue;	/* face was used */
		rt_log("arbn(%s) face %d unused, solid is not convex\n",
			stp->st_name, i);
		rt_free( (char *)used, "arbn used[]");
		rt_arbn_ifree( &intern );
		return(-1);		/* BAD */
	}

	rt_free( (char *)used, "arbn used[]");

	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSUB2SCALE( work, stp->st_max, stp->st_min, 0.5 );

	f = work[X];
	if( work[Y] > f )  f = work[Y];
	if( work[Z] > f )  f = work[Z];
	stp->st_aradius = f;
	stp->st_bradius = MAGNITUDE(work);
	return(0);			/* OK */
}

/*
 *  			R T _ A R B N _ P R I N T
 */
void
rt_arbn_print( stp )
register struct soltab *stp;
{
}

/*
 *			R T _ A R B N _ S H O T
 *
 *  Intersect a ray with an ARBN.
 *  Find the largest "in" distance and the smallest "out" distance.
 *  Cyrus & Beck algorithm for convex polyhedra.
 *
 *  Returns -
 *	0	MISS
 *	>0	HIT
 */
int
rt_arbn_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct arbn_internal	*aip =
		(struct arbn_internal *)stp->st_specific;
	register int	i;
	LOCAL int	iplane, oplane;
	LOCAL fastf_t	in, out;	/* ray in/out distances */

	in = -INFINITY;
	out = INFINITY;
	iplane = oplane = -1;

	for( i = aip->neqn-1; i >= 0; i-- )  {
		FAST fastf_t	slant_factor;	/* Direction dot Normal */
		FAST fastf_t	norm_dist;
		FAST fastf_t	s;

		norm_dist = VDOT( aip->eqn[i], rp->r_pt ) - aip->eqn[i][3];
		if( (slant_factor = -VDOT( aip->eqn[i], rp->r_dir )) < -1.0e-10 )  {
			/* exit point, when dir.N < 0.  out = min(out,s) */
			if( out > (s = norm_dist/slant_factor) )  {
				out = s;
				oplane = i;
			}
		} else if ( slant_factor > 1.0e-10 )  {
			/* entry point, when dir.N > 0.  in = max(in,s) */
			if( in < (s = norm_dist/slant_factor) )  {
				in = s;
				iplane = i;
			}
		}  else  {
			/* ray is parallel to plane when dir.N == 0.
			 * If it is outside the solid, stop now
			 * Allow very small amount of slop, to catch
			 * rays that lie very nearly in the plane of a face.
			 */
			if( norm_dist > SQRT_SMALL_FASTF )
				return( 0 );	/* MISS */
		}
		if( in > out )
			return( 0 );	/* MISS */
	}

	/* Validate */
	if( iplane == -1 || oplane == -1 )  {
		rt_log("rt_arbn_shoot(%s): 1 hit => MISS\n",
			stp->st_name);
		return( 0 );	/* MISS */
	}
	if( in >= out || out >= INFINITY )
		return( 0 );	/* MISS */

	{
		register struct seg *segp;

		RT_GET_SEG( segp, ap->a_resource );
		segp->seg_stp = stp;
		segp->seg_in.hit_dist = in;
		segp->seg_in.hit_surfno = iplane;

		segp->seg_out.hit_dist = out;
		segp->seg_out.hit_surfno = oplane;
		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	}
	return(2);			/* HIT */
}

/*
 *			R T _ A R B N _ V S H O T
 */
void
rt_arbn_vshot( stp, rp, segp, n, resp)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		 	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
{
	rt_vstub( stp, rp, segp, n, resp );
}

/*
 *  			R T _ A R B N _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_arbn_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct arbn_internal *aip =
		(struct arbn_internal *)stp->st_specific;
	int	h;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	h = hitp->hit_surfno;
	if( h < 0 || h > aip->neqn )  {
		rt_log("rt_arbn_norm(%s): hit_surfno=%d?\n", h );
		VSETALL( hitp->hit_normal, 0 );
		return;
	}
	VMOVE( hitp->hit_normal, aip->eqn[h] );
}

/*
 *			R T _ A R B N _ C U R V E
 *
 *  Return the "curvature" of the ARB face.
 *  Pick a principle direction orthogonal to normal, and 
 *  indicate no curvature.
 */
void
rt_arbn_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{

	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/*
 *  			R T _ A R B N _ U V
 *  
 *  For a hit on a face of an ARB, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the arb_U direction defined by B-A,
 *  v extends along the arb_V direction defined by Nx(B-A).
 */
void
rt_arbn_uv( ap, stp, hitp, uvp )
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	uvp->uv_u = uvp->uv_v = 0;
	uvp->uv_du = uvp->uv_dv = 0;
}

/*
 *			R T _ A R B N _ F R E E
 */
void
rt_arbn_free( stp )
register struct soltab *stp;
{
	register struct arbn_internal *aip =
		(struct arbn_internal *)stp->st_specific;

	rt_free( (char *)aip->eqn, "arbn_internal eqn[]");
	rt_free( (char *)aip, "arbn_internal" );
}

/*
 *  			R T _ A R B N _ P L O T
 *
 *  Brute force through all possible plane intersections.
 *  Generate all edge lines, then intersect the line with all
 *  the other faces to find the vertices on that line.
 *  If the geometry is correct, there will be no more than two.
 *  While not the fastest strategy, this will produce an accurate
 *  plot without requiring extra bookkeeping.
 *  Note that the vectors will be drawn in no special order.
 */
#if NEW_IF
int
rt_arbn_plot( vhead, mat, ip, abs_tol, rel_tol, norm_tol )
struct vlhead	*vhead;
mat_t		mat;
struct rt_db_internal *ip;
double		abs_tol;
double		rel_tol;
double		norm_tol;
{
#else
int
rt_arbn_plot( rp, mat, vhead, dp )
union record		*rp;
mat_t			mat;
struct vlhead		*vhead;
struct directory	*dp;
{
	struct rt_external	ext, *ep;
	struct rt_db_internal	intern, *ip;
#endif
	register struct arbn_internal	*aip;
	register int	i;
	register int	j;
	register int	k;

#if NEW_IF
	/* All set */
#else
	ep = &ext;
	RT_INIT_EXTERNAL(ep);
	ep->ext_buf = (genptr_t)rp;
	ep->ext_nbytes = dp->d_len*sizeof(union record);
	i = rt_arbn_import( &intern, ep, mat );
	if( i < 0 )  {
		rt_log("rt_arbn_plot(): db import failure\n");
		return(-1);		/* BAD */
	}
	ip = &intern;
#endif
	RT_CK_DB_INTERNAL(ip);
	aip = (struct arbn_internal *)ip->idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	for( i=0; i<aip->neqn-1; i++ )  {
		for( j=i+1; j<aip->neqn; j++ )  {
			double	dot;
			int	point_count;	/* # points on this line */
			point_t	a,b;		/* start and end points */
			vect_t	dist;

			/* If normals are parallel, no intersection */
			dot = VDOT( aip->eqn[i], aip->eqn[j] );
			if( !NEAR_ZERO( dot, 0.999999 ) )  continue;

			/* Have an edge line, isect with all other planes */
			point_count = 0;
			for( k=0; k<aip->neqn; k++ )  {
				register int	m;
				point_t		pt;

				if( k==i || k==j )  continue;
				if( rt_mkpoint_3planes( pt, aip->eqn[i], aip->eqn[j], aip->eqn[k] ) < 0 )  continue;

				/* See if point is outside arb */
				for( m=0; m<aip->neqn; m++ )  {
					if( i==m || j==m || k==m )  continue;
					if( VDOT(pt, aip->eqn[m])-aip->eqn[m][3] > DIST_TOL )
						goto next_k;
				}

				if( point_count <= 0 )  {
					ADD_VL( vhead, pt, 0 );
					VMOVE( a, pt );
				} else if( point_count == 1 )  {
					VSUB2( dist, pt, a );
					if( MAGSQ(dist) < DIST_TOL_SQ )  continue;
					ADD_VL( vhead, pt, 1 );
					VMOVE( b, pt );
				} else {
					VSUB2( dist, pt, a );
					if( MAGSQ(dist) < DIST_TOL_SQ )  continue;
					VSUB2( dist, pt, b );
					if( MAGSQ(dist) < DIST_TOL_SQ )  continue;
					rt_log("rt_arbn_plot(%s): error, point_count=%d (>2) on edge %d/%d, non-convex\n",
						dp->d_namep, point_count+1,
						i, j );
					VPRINT(" a", a);
					VPRINT(" b", b);
					VPRINT("pt", pt);
					ADD_VL( vhead, pt, 1 );	/* draw it */
				}
				point_count++;
next_k:				;
			}
			/* Point counts of 1 are (generally) not harmful,
			 * occuring on pyramid peaks and the like.
			 */
		}
	}
	return(0);
}

/*
 *			R T _ A R B N _ C L A S S
 */
int
rt_arbn_class()
{
	return(0);
}

/*
 *			R T _ A R B N _ T E S S
 *
 *  "Tessellate" an ARB into an NMG data structure.
 *  Purely a mechanical transformation of one faceted object
 *  into another.
 */
int
rt_arbn_tess( s, rp, mat, dp, abs_tol, rel_tol, norm_tol )
struct shell		*s;
register union record	*rp;
register mat_t		mat;
struct directory	*dp;
double			abs_tol;
double			rel_tol;
double			norm_tol;
{
	return(-1);
}

/*
 *			R T _ A R B N _ I M P O R T
 *
 *  Convert from "network" doubles to machine specific.
 *  Transform
 */
int
rt_arbn_import( ip, ep, mat )
struct rt_db_internal	*ip;
struct rt_external	*ep;
register mat_t		mat;
{
	union record		*rp;
	struct arbn_internal	*aip;
	register int	i;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != DBID_ARBN )  {
		rt_log("rt_arbn_import: defective record, id=x%x\n", rp->u_id );
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_ARBN;
	ip->idb_ptr = rt_malloc( sizeof(struct arbn_internal), "arbn_internal");
	aip = (struct arbn_internal *)ip->idb_ptr;
	aip->magic = RT_ARBN_INTERNAL_MAGIC;
	aip->neqn = rp->n.n_neqn;
	if( aip->neqn <= 0 )  return(-1);
	aip->eqn = (plane_t *)rt_malloc( aip->neqn*sizeof(plane_t), "arbn plane eqn[]");

	ntohd( (char *)aip->eqn, (char *)(&rp[1]), aip->neqn*4 );

	/* Transform by the matrix */
#	include "noalias.h"
	for( i=0; i < aip->neqn; i++ )  {
		point_t	orig_pt;
		point_t	pt;
		vect_t	norm;

		/* Pick a point on the original halfspace */
		VSCALE( orig_pt, aip->eqn[i], aip->eqn[i][3] );

		/* Transform the point, and the normal */
		MAT4X3VEC( norm, mat, aip->eqn[i] );
		MAT4X3PNT( pt, mat, orig_pt );

		/* Measure new distance from origin to new point */
		VMOVE( aip->eqn[i], norm );
		aip->eqn[i][3] = VDOT( pt, norm );
	}

	return(0);
}

/*
 *			R T _ A R B N _ E X P O R T
 */
int
rt_arbn_export( ep, ip, local2mm )
struct rt_external	*ep;
struct rt_db_internal	*ip;
double			local2mm;
{
	struct arbn_internal	*aip;
	union record		*rec;
	int			ngrans;
	double			*sbuf;		/* scalling buffer */
	register double		*sp;
	register int		i;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_ARBN )  return(-1);
	aip = (struct arbn_internal *)ip->idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	if( aip->neqn <= 0 )  return(-1);

	/*
	 * The network format for a double is 8 bytes and there are 4
	 * doubles per plane equation.
	 */
	ngrans = (aip->neqn * 8 * 4 + sizeof(union record)-1 ) /
		sizeof(union record);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = (ngrans + 1) * sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "arbn external");
	rec = (union record *)ep->ext_buf;

	rec[0].n.n_id = DBID_ARBN;
	rec[0].n.n_neqn = aip->neqn;
	rec[0].n.n_grans = ngrans;

	/* Take the data from the caller, and scale it, into sbuf */
	sp = sbuf = (double *)rt_malloc(
		aip->neqn * sizeof(double) * 4, "arbn temp");
	for( i=0; i<aip->neqn; i++ )  {
		/* Normal is unscaled, should have unit length; d is scaled */
		*sp++ = aip->eqn[i][X];
		*sp++ = aip->eqn[i][Y];
		*sp++ = aip->eqn[i][Z];
		*sp++ = aip->eqn[i][3] * local2mm;
	}

	htond( (char *)&rec[1], (char *)sbuf, aip->neqn * 4 );

	rt_free( (char *)sbuf, "arbn temp" );
	return(0);			/* OK */
}


/*
 *			R T _ A R B N _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_arbn_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct arbn_internal	*aip =
		(struct arbn_internal *)ip->idb_ptr;
	char	buf[256];
	int	i;

	RT_ARBN_CK_MAGIC(aip);
	sprintf(buf, "arbn bounded by %d planes\n", aip->neqn);
	rt_vls_strcat( str, buf );

	if( !verbose )  return(0);

	for( i=0; i < aip->neqn; i++ )  {
		sprintf(buf, "\t%d: (%g, %g, %g) %g\n",
			i,
			aip->eqn[i][X],		/* should have unit length */
			aip->eqn[i][Y],
			aip->eqn[i][Z],
			aip->eqn[i][3] * mm2local );
		rt_vls_strcat( str, buf );
	}
	return(0);
}


/*
 *			R T _ A R B N _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_arbn_ifree( ip )
struct rt_db_internal	*ip;
{
	struct arbn_internal	*aip;

	RT_CK_DB_INTERNAL(ip);
	aip = (struct arbn_internal *)ip->idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	rt_free( (char *)aip->eqn, "arbn_internal eqn[]");
	rt_free( (char *)aip, "arbn_internal" );

	ip->idb_ptr = (genptr_t)0;	/* sanity */
}
