/*
 *			G _ H A L F . C
 *  
 *  Function -
 *  	Intersect a ray with a Halfspace
 *
 *
 *  A HALFSPACE is defined by an outward pointing normal vector,
 *  and the distance from the origin to the plane, which is defined
 *  by N and d.
 *
 *  With outward pointing normal vectors,
 *  the ray enters the half-space defined by a plane when D dot N < 0,
 *  is parallel to the plane when D dot N = 0, and exits otherwise.
 *
 *  The inside of the halfspace bounded by the plane
 *  consists of all points P such that
 *	VDOT(P,N) - N[3] <= 0
 *
 *  See the remarks in h/vmath.h for more details.
 *  
 *  Authors -
 *	Michael John Muuss
 *	Dave Becker		(Vectorization)
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
static char RCShalf[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "raytrace.h"
#include "nmg.h"
#include "db.h"
#include "rtgeom.h"
#include "./debug.h"

struct half_specific  {
	plane_t	half_eqn;		/* Plane equation, outward normal */
	vect_t	half_Xbase;		/* "X" basis direction */
	vect_t	half_Ybase;		/* "Y" basis direction */
};
#define HALF_NULL	((struct half_specific *)0)

struct bu_structparse rt_half_parse[] = {
    { "%f", 3, "N", offsetof(struct rt_half_internal, eqn[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "d", offsetof(struct rt_half_internal, eqn[3]), BU_STRUCTPARSE_FUNC_NULL },
    {0} };
/*
 *  			R T _ H L F _ P R E P
 */
int
rt_hlf_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_half_internal	*hip;
	register struct half_specific *halfp;

	hip = (struct rt_half_internal *)ip->idb_ptr;
	RT_HALF_CK_MAGIC(hip);

	/*
	 * Process a HALFSPACE, which is represented as a 
	 * normal vector, and a distance.
	 */
	GETSTRUCT( halfp, half_specific );
	stp->st_specific = (genptr_t)halfp;

	VMOVE( halfp->half_eqn, hip->eqn );
	halfp->half_eqn[3] = hip->eqn[3];

	/* Select one point on the halfspace as the "center" */
	VSCALE( stp->st_center, halfp->half_eqn, halfp->half_eqn[3] );

	/* X and Y basis for uv map */
	vec_perp( halfp->half_Xbase, stp->st_center );
	VCROSS( halfp->half_Ybase, halfp->half_Xbase, halfp->half_eqn );
	VUNITIZE( halfp->half_Xbase );
	VUNITIZE( halfp->half_Ybase );

	/* No bounding sphere or bounding RPP is possible */
	VSETALL( stp->st_min, -INFINITY);
	VSETALL( stp->st_max,  INFINITY);

	stp->st_aradius = INFINITY;
	stp->st_bradius = INFINITY;
	return(0);		/* OK */
}

/*
 *  			R T _ H L F _ P R I N T
 */
void
rt_hlf_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct half_specific *halfp =
		(struct half_specific *)stp->st_specific;

	if( halfp == HALF_NULL )  {
		rt_log("half(%s):  no data?\n", stp->st_name);
		return;
	}
	VPRINT( "Normal", halfp->half_eqn );
	rt_log( "d = %f\n", halfp->half_eqn[3] );
	VPRINT( "Xbase", halfp->half_Xbase );
	VPRINT( "Ybase", halfp->half_Ybase );
}

/*
 *			R T _ H L F _ S H O T
 *  
 * Function -
 *	Shoot a ray at a HALFSPACE
 *
 * Algorithm -
 * 	The intersection distance is computed.
 *  
 * Returns -
 *	0	MISS
 *	>0	HIT
 */
int
rt_hlf_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct half_specific *halfp =
		(struct half_specific *)stp->st_specific;
	LOCAL fastf_t	in, out;	/* ray in/out distances */

	in = -INFINITY;
	out = INFINITY;

	{
		FAST fastf_t	slant_factor;	/* Direction dot Normal */
		FAST fastf_t	norm_dist;

		norm_dist = VDOT( halfp->half_eqn, rp->r_pt ) - halfp->half_eqn[3];
		if( (slant_factor = -VDOT( halfp->half_eqn, rp->r_dir )) < -1.0e-10 )  {
			/* exit point, when dir.N < 0.  out = min(out,s) */
			out = norm_dist/slant_factor;
			if( !NEAR_ZERO(out, INFINITY) )
				return(0);	/* MISS */
		} else if ( slant_factor > 1.0e-10 )  {
			/* entry point, when dir.N > 0.  in = max(in,s) */
			in = norm_dist/slant_factor;
			if( !NEAR_ZERO(in, INFINITY) )
				return(0);	/* MISS */
		}  else  {
			/* ray is parallel to plane when dir.N == 0.
			 * If it is outside the solid, stop now */
			if( norm_dist > 0.0 )
				return(0);	/* MISS */
		}
	}
	if( rt_g.debug & DEBUG_ARB8 )
		rt_log("half: in=%f, out=%f\n", in, out);

	{
		register struct seg *segp;

		RT_GET_SEG( segp, ap->a_resource );
		segp->seg_stp = stp;
		segp->seg_in.hit_dist = in;
		segp->seg_out.hit_dist = out;
		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	}
	return(2);			/* HIT */
}

#define RT_HALF_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ H L F _ V S H O T
 *
 *  This is the Becker vector version
 */
void
rt_hlf_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	register int    i;
	register struct half_specific *halfp;
	LOCAL fastf_t	in, out;	/* ray in/out distances */
	FAST fastf_t	slant_factor;	/* Direction dot Normal */
	FAST fastf_t	norm_dist;

	/* for each ray/halfspace pair */
#	include "noalias.h"
	for(i = 0; i < n; i++){
		if (stp[i] == 0) continue; /* indicates "skip this pair" */

		halfp = (struct half_specific *)stp[i]->st_specific;

		in = -INFINITY;
		out = INFINITY;

		norm_dist = VDOT(halfp->half_eqn, rp[i]->r_pt) - halfp->half_eqn[3];

		if((slant_factor = -VDOT(halfp->half_eqn, rp[i]->r_dir)) <
								-1.0e-10) {
			/* exit point, when dir.N < 0.  out = min(out,s) */
			out = norm_dist/slant_factor;
		} else if ( slant_factor > 1.0e-10 )  {
			/* entry point, when dir.N > 0.  in = max(in,s) */
			in = norm_dist/slant_factor;
		}  else  {
			/* ray is parallel to plane when dir.N == 0.
			 * If it is outside the solid, stop now */
			if( norm_dist > 0.0 ) {
				RT_HALF_SEG_MISS(segp[i]);	/* No hit */
			        continue;
			}
		}

		/* HIT */
		segp[i].seg_stp = stp[i];
		segp[i].seg_in.hit_dist = in;
		segp[i].seg_out.hit_dist = out;
	}
}

/*
 *  			R T _ H L F _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 *  The normal is already filled in.
 */
void
rt_hlf_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct half_specific *halfp =
		(struct half_specific *)stp->st_specific;
	FAST fastf_t f;

	/*
	 * At most one normal is really defined, but whichever one
	 * it is, it has value half_eqn.
	 */
	VMOVE( hitp->hit_normal, halfp->half_eqn );

	/* We are expected to compute hit_point here.  May be infinite. */
	f = hitp->hit_dist;
	if( f <= -INFINITY )  {
		rt_log("rt_hlf_norm:  hit_dist = -INFINITY, unable to compute pt.\n");
		VSETALL( hitp->hit_point, -INFINITY );
	} else if( f >= INFINITY )  {
		rt_log("rt_hlf_norm:  hit_dist = +INFINITY, unable to compute pt.\n");
		VSETALL( hitp->hit_point, INFINITY );
	} else {
		VJOIN1( hitp->hit_point, rp->r_pt, f, rp->r_dir );
	}
}

/*
 *			R T _ H L F _ C U R V E
 *
 *  Return the "curvature" of the halfspace.
 *  Pick a principle direction orthogonal to normal, and 
 *  indicate no curvature.
 */
void
rt_hlf_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct half_specific *halfp =
		(struct half_specific *)stp->st_specific;

	vec_ortho( cvp->crv_pdir, halfp->half_eqn );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/*
 *  			R T _ H L F _ U V
 *  
 *  For a hit on a face of an HALF, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the Xbase direction
 *  v extends along the "Ybase" direction
 *  Note that a "toroidal" map is established, varying each from
 *  0 up to 1 and then back down to 0 again.
 */
void
rt_hlf_uv( ap, stp, hitp, uvp )
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	register struct half_specific *halfp =
		(struct half_specific *)stp->st_specific;
	LOCAL vect_t P_A;
	FAST fastf_t f;
	auto double ival;

	f = hitp->hit_dist;
	if( f <= -INFINITY || f >= INFINITY )  {
		rt_log("rt_hlf_uv:  infinite dist\n");
		rt_pr_hit( "rt_hlf_uv", hitp );
		uvp->uv_u = uvp->uv_v = 0;
		uvp->uv_du = uvp->uv_dv = 0;
		return;
	}
	VSUB2( P_A, hitp->hit_point, stp->st_center );

	f = VDOT( P_A, halfp->half_Xbase )/10000;
	if( f <= -INFINITY || f >= INFINITY )  {
		rt_log("rt_hlf_uv:  bad X vdot\n");
		VPRINT("Xbase", halfp->half_Xbase);
		rt_pr_hit( "rt_hlf_uv", hitp );
		VPRINT("st_center", stp->st_center );
		f = 0;
	}
	if( f < 0 )  f = -f;
	f = modf( f, &ival );
	if( f < 0.5 )
		uvp->uv_u = 2 * f;		/* 0..1 */
	else
		uvp->uv_u = 2 * (1 - f);	/* 1..0 */

	f = VDOT( P_A, halfp->half_Ybase )/10000;
	if( f <= -INFINITY || f >= INFINITY )  {
		rt_log("rt_hlf_uv:  bad Y vdot\n");
		VPRINT("Xbase", halfp->half_Ybase);
		rt_pr_hit( "rt_hlf_uv", hitp );
		VPRINT("st_center", stp->st_center );
		f = 0;
	}
	if( f < 0 )  f = -f;
	f = modf( f, &ival );
	if( f < 0.5 )
		uvp->uv_v = 2 * f;		/* 0..1 */
	else
		uvp->uv_v = 2 * (1 - f);	/* 1..0 */

	if( uvp->uv_u < 0 || uvp->uv_v < 0 )  {
		if( rt_g.debug )
			rt_log("half_uv: bad uv=%f,%f\n", uvp->uv_u, uvp->uv_v);
		/* Fix it up */
		if( uvp->uv_u < 0 )  uvp->uv_u = (-uvp->uv_u);
		if( uvp->uv_v < 0 )  uvp->uv_v = (-uvp->uv_v);
	}
	
	uvp->uv_du = uvp->uv_dv =
		(ap->a_rbeam + ap->a_diverge * hitp->hit_dist) / (10000/2);
	if( uvp->uv_du < 0 || uvp->uv_dv < 0 )  {
		rt_pr_hit( "rt_hlf_uv", hitp );
		uvp->uv_du = uvp->uv_dv = 0;
	}
}

/*
 *			R T _ H L F _ F R E E
 */
void
rt_hlf_free( stp )
struct soltab *stp;
{
	register struct half_specific *halfp =
		(struct half_specific *)stp->st_specific;

	rt_free( (char *)halfp, "half_specific");
}

int
rt_hlf_class( stp, min, max, tol )
register CONST struct soltab	*stp;
CONST vect_t			 min, max;
CONST struct bn_tol		*tol;
{
	register CONST struct half_specific *halfp =
		(struct half_specific *)stp->st_specific;
	int class;
	fastf_t d;

	if( halfp == HALF_NULL ) {
		rt_log( "half(%s):  no data?\n", stp->st_name );
		return 0;
	}

#define CHECK_PT( x, y, z ) \
	d = (x)*halfp->half_eqn[0] + (y)*halfp->half_eqn[1] + (z)*halfp->half_eqn[2] - halfp->half_eqn[3]; \
	if( d < -tol->dist ) { \
		if( class == RT_CLASSIFY_OUTSIDE ) \
			return RT_CLASSIFY_OVERLAPPING; \
		else class = RT_CLASSIFY_INSIDE; \
	} else if( d > tol->dist ) { \
		if( class == RT_CLASSIFY_INSIDE ) \
			return RT_CLASSIFY_OVERLAPPING; \
		else class = RT_CLASSIFY_OUTSIDE; \
	} else return RT_CLASSIFY_OVERLAPPING

	class = 0;
	CHECK_PT( min[X], min[Y], min[Z] );
	CHECK_PT( min[X], min[Y], max[Z] );
	CHECK_PT( min[X], max[Y], min[Z] );
	CHECK_PT( min[X], max[Y], max[Z] );
	CHECK_PT( max[X], min[Y], min[Z] );
	CHECK_PT( max[X], min[Y], max[Z] );
	CHECK_PT( max[X], max[Y], min[Z] );
	CHECK_PT( max[X], max[Y], max[Z] );

	if( class == 0 )
		bu_log( "rt_hlf_class: error in implementation\
min = (%g, %g, %g), max = (%g, %g, %g), half_eqn = (%d, %d, %d, %d)\n",
			V3ARGS(min), V3ARGS(max), V3ARGS(halfp->half_eqn),
			halfp->half_eqn[3]);
	
	return class;
}

/*
 *			R T _ H L F _ P L O T
 *
 *  The representation of a halfspace is an OUTWARD pointing
 *  normal vector, and the distance of the plane from the origin.
 *
 *  Drawing a halfspace is difficult when using a finite display.
 *  Drawing the boundary plane is hard enough.
 *  We just make a cross in the plane, with the outward normal
 *  drawn shorter.
 */
int
rt_hlf_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal 	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	struct rt_half_internal	*hip;
	vect_t cent;		/* some point on the plane */
	vect_t xbase, ybase;	/* perpendiculars to normal */
	vect_t x1, x2;
	vect_t y1, y2;
	vect_t tip;

	RT_CK_DB_INTERNAL(ip);
	hip = (struct rt_half_internal *)ip->idb_ptr;
	RT_HALF_CK_MAGIC(hip);

	/* Invent a "center" point on the plane -- point closets to origin */
	VSCALE( cent, hip->eqn, hip->eqn[3] );

	/* The use of "x" and "y" here is not related to the axis */
	vec_perp( xbase, cent );
	VCROSS( ybase, xbase, hip->eqn );

	/* Arrange for the cross to be 2 meters across */
	VUNITIZE( xbase );
	VUNITIZE( ybase);
	VSCALE( xbase, xbase, 1000 );
	VSCALE( ybase, ybase, 1000 );

	VADD2( x1, cent, xbase );
	VSUB2( x2, cent, xbase );
	VADD2( y1, cent, ybase );
	VSUB2( y2, cent, ybase );

	RT_ADD_VLIST( vhead, x1, RT_VLIST_LINE_MOVE );	/* the cross */
	RT_ADD_VLIST( vhead, x2, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, y1, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, y2, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, x2, RT_VLIST_LINE_DRAW );	/* the box */
	RT_ADD_VLIST( vhead, y1, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, x1, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, y2, RT_VLIST_LINE_DRAW );

	VSCALE( tip, hip->eqn, 500 );
	VADD2( tip, cent, tip );
	RT_ADD_VLIST( vhead, cent, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, tip, RT_VLIST_LINE_DRAW );
	return(0);
}
/*
 *			H A L F _ X F O R M
 *
 *  Returns -
 *	-1	failure
 *	 0	success
 */
int
rt_hlf_xform(op, mat, ip, free)
struct rt_db_internal	*op;
CONST mat_t		mat;
struct rt_db_internal *ip;
CONST int	free;
{
	struct rt_half_internal *hip, *hop;
	point_t			orig_pt, pt;
	register double		f,t;

	RT_CK_DB_INTERNAL( ip );
	hip = (struct rt_half_internal *)ip->idb_ptr;
	RT_HALF_CK_MAGIC(hip);
	RT_CK_DB_INTERNAL( op );
	hop = (struct rt_half_internal *)op->idb_ptr;
	RT_HALF_CK_MAGIC(hop);

	/* Pick a point on the original halfspace */
	VSCALE( orig_pt, hip->eqn, hip->eqn[1*ELEMENTS_PER_VECT] );

	/* Transform the picked point and the normal */
	MAT4X3VEC( hop->eqn, mat, hip->eqn);
	MAT4X3PNT( pt, mat, orig_pt);

	/*
	 * We are done with the input solid so free it if required.
	 */
	if (free && ip != op) {
		rt_functab[ip->idb_type].ft_ifree( ip );
		ip->idb_ptr = (genptr_t) 0;
	}
	/*
	 * The transformed normal is all that is required.
	 * The new distance is found from the transforemd point on the plane.
	 */
	hop->eqn[3] = VDOT( pt, hop->eqn );

	/* Now some safety.  Verify that the normal has unit length */
	f = MAGNITUDE( hop->eqn);
	if ( f < SMALL ) {
		rt_log("rt_half_xform: bad normal, len = %g\n", f);
		return(-1);
	}
	t = f - 1.0;
	if ( !NEAR_ZERO( t, 0.001 ) ) {
		/* Restore normal to unit length */
		f = 1/f;
		VSCALE( hop->eqn, hop->eqn, f );
		hip->eqn[3] *= f;
	}
	return 0;
}
/*
 *			H A L F _ I M P O R T
 *
 *  Returns -
 *	-1	failure
 *	 0	success
 */
int
rt_hlf_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
CONST mat_t			mat;
{
	struct rt_half_internal	*hip;
	union record	*rp;
	point_t		orig_pt;
	point_t		pt;
	fastf_t		orig_eqn[3*2];
	register double	f,t;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != ID_SOLID )  {
		rt_log("rt_hlf_import: defective record, id=x%x\n", rp->u_id);
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_HALF;
	ip->idb_ptr = rt_malloc( sizeof(struct rt_half_internal), "rt_half_internal");
	hip = (struct rt_half_internal *)ip->idb_ptr;
	hip->magic = RT_HALF_INTERNAL_MAGIC;

	rt_fastf_float( orig_eqn, rp->s.s_values, 2 );	/* 2 floats too many */

	/* Pick a point on the original halfspace */
	VSCALE( orig_pt, orig_eqn, orig_eqn[1*ELEMENTS_PER_VECT] );

	/* Transform the point, and the normal */
	MAT4X3VEC( hip->eqn, mat, orig_eqn );
	MAT4X3PNT( pt, mat, orig_pt );

	/*
	 *  The transformed normal is all that is required.
	 *  The new distance is found from the transformed point on the plane.
	 */
	hip->eqn[3] = VDOT( pt, hip->eqn );

	/* Verify that normal has unit length */
	f = MAGNITUDE( hip->eqn );
	if( f < SMALL )  {
		rt_log("rt_hlf_import:  bad normal, len=%g\n", f );
		return(-1);		/* BAD */
	}
	t = f - 1.0;
	if( !NEAR_ZERO( t, 0.001 ) )  {
		/* Restore normal to unit length */
		f = 1/f;
		VSCALE( hip->eqn, hip->eqn, f );
		hip->eqn[3] *= f;
	}
	return(0);			/* OK */
}

/*
 *			R T _ H L F _ E X P O R T
 */
int
rt_hlf_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_half_internal	*hip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_HALF )  return(-1);
	hip = (struct rt_half_internal *)ip->idb_ptr;
	RT_HALF_CK_MAGIC(hip);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "half external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->s.s_type = HALFSPACE;
	VMOVE( rec->s.s_values, hip->eqn );
	rec->s.s_values[3] = hip->eqn[3] * local2mm;

	return(0);
}

/*
 *			R T _ H L F _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_hlf_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_half_internal	*hip =
		(struct rt_half_internal *)ip->idb_ptr;
	char	buf[256];

	RT_HALF_CK_MAGIC(hip);
	rt_vls_strcat( str, "halfspace\n");

	sprintf(buf, "\tN (%g, %g, %g) d=%g\n",
		V3ARGS(hip->eqn),		/* should have unit length */
		hip->eqn[3] * mm2local );
	rt_vls_strcat( str, buf );

	return(0);
}

/*
 *			R T _ H L F _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_hlf_ifree( ip )
struct rt_db_internal	*ip;
{
	RT_CK_DB_INTERNAL(ip);
	rt_free( ip->idb_ptr, "hlf ifree" );
	ip->idb_ptr = GENPTR_NULL;
}

/*
 *			R T _ H L F _ T E S S
 */
int
rt_hlf_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	struct rt_half_internal	*vip;
#if 0
	register int	i;
	struct shell	*s;
	struct vertex	**verts;	/* dynamic array of pointers */
	struct vertex	***vertp;	/* dynam array of ptrs to pointers */
	struct faceuse	*fu;
#endif

	RT_CK_DB_INTERNAL(ip);
	vip = (struct rt_half_internal *)ip->idb_ptr;
	RT_HALF_CK_MAGIC(vip);

	/* XXX tess routine needed */
	return(-1);
}
