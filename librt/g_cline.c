/*
 *			G _ C L I N E . C
 *
 *  Purpose -
 *	Intersect a ray with a FSTGEN4 CLINE element
 *
 *  Authors -
 *  	John Anderson
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 2000 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSxxx[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "tcl.h"
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"

/* ray tracing form of solid, including precomputed terms */
struct cline_specific {
	point_t V;
	vect_t height;
	fastf_t radius;
	fastf_t thickness;
	vect_t h;	/* unitized height */
};

#define	RT_CLINE_O(m)	offsetof( struct rt_cline_internal, m )

CONST struct bu_structparse rt_cline_parse[] = {
	{ "%f", 3, "V", RT_CLINE_O( v ),  BU_STRUCTPARSE_FUNC_NULL },
	{ "%f", 3, "H", RT_CLINE_O( h ),  BU_STRUCTPARSE_FUNC_NULL },
	{ "%f", 1, "r", RT_CLINE_O( radius ), BU_STRUCTPARSE_FUNC_NULL },
	{ "%f", 1, "t", RT_CLINE_O( thickness ), BU_STRUCTPARSE_FUNC_NULL },
	{0} };

/*
 *  			R T _ C L I N E _ P R E P
 *  
 *  Given a pointer to a GED database record,
 *  determine if this is a valid cline solid, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	cline is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct cline_specific is created, and it's address is stored in
 *  	stp->st_specific for use by rt_cline_shot().
 */
int
rt_cline_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_cline_internal		*cline_ip;
	register struct cline_specific		*cline;
	vect_t					work;
	vect_t					rad;
	point_t					top;
	fastf_t					tmp;
	fastf_t					max_tr=50.0;

	RT_CK_DB_INTERNAL(ip);
	cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
	RT_CLINE_CK_MAGIC(cline_ip);

	BU_GETSTRUCT( cline, cline_specific );
	cline->thickness = cline_ip->thickness;
	cline->radius = cline_ip->radius;
	VMOVE( cline->V, cline_ip->v );
	VMOVE( cline->height, cline_ip->h );
	VMOVE( cline->h, cline_ip->h );
	VUNITIZE( cline->h );
	stp->st_specific = (genptr_t)cline;

	/* XXXX max_tr (maximum threat radius) is a temporary kludge */
	tmp = MAGNITUDE( cline_ip->h ) * 0.5;
	stp->st_aradius = sqrt( tmp*tmp + cline_ip->radius*cline_ip->radius );
	stp->st_bradius = stp->st_aradius + 50.0;
	VSETALL( stp->st_min, MAX_FASTF );
	VREVERSE( stp->st_max, stp->st_min );

	VSETALL( rad, cline_ip->radius + max_tr );
	VADD2( work, cline_ip->v, rad );
	VMINMAX( stp->st_min,stp->st_max, work );
	VSUB2( work, cline_ip->v, rad );
	VMINMAX( stp->st_min,stp->st_max, work );
	VADD2( top, cline_ip->v, cline_ip->h );
	VADD2( work, top, rad );
	VMINMAX( stp->st_min,stp->st_max, work );
	VSUB2( work, top, rad );
	VMINMAX( stp->st_min,stp->st_max, work );

	return( 0 );
}

/*
 *			R T _ C L I N E _ P R I N T
 */
void
rt_cline_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct cline_specific *cline =
		(struct cline_specific *)stp->st_specific;

	VPRINT( "V", cline->V );
	VPRINT( "Height", cline->height );
	VPRINT( "Unit Height", cline->h );
	bu_log( "Radius: %g\n", cline->radius );
	if( cline->thickness > 0.0 )
		bu_log( "Plate Mode Thickness: %g\n", cline->thickness );
	else
		bu_log( "Volume mode\n" );
}

/*
 *  			R T _ C L I N E _ S H O T
 *  
 *  Intersect a ray with a cline mode solid.
 *  If an intersection occurs, at least one struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_cline_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct cline_specific *cline =
		(struct cline_specific *)stp->st_specific;
	struct seg		ref_seghead;
	register struct seg	*segp;
	fastf_t reff;
	fastf_t dist[3];
	fastf_t cosa, sina;
	fastf_t half_los;
	point_t pt1, pt2;
	vect_t diff;
	fastf_t tmp;
	fastf_t distmin, distmax;

	BU_LIST_INIT( &ref_seghead.l );

	/* This is a CLINE FASTGEN element */
	reff = cline->radius + ap->a_rbeam;

	cosa = VDOT( rp->r_dir, cline->h );

	if( bn_distsq_line3_line3( dist, cline->V, cline->height, rp->r_pt, rp->r_dir, pt1, pt2 ) )
	{
		/* ray is parallel to CLINE */

		if( cline->thickness > 0.0 )
			return( 0 );	/* No end-on hits for plate mode cline */

		if( dist[2] > reff*reff )
			return( 0 );	/* missed */

		VJOIN2( diff, cline->V, 1.0, cline->height, -1.0, rp->r_pt );
		dist[0] = VDOT( diff, rp->r_dir );
		if( dist[1] < dist[0] )
		{
			dist[2] = dist[0];
			dist[0] = dist[1];
			dist[1] = dist[2];
		}

		/* vloume mode */

		RT_GET_SEG( segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in.hit_dist = dist[0];
		segp->seg_in.hit_surfno = 1;
		if( cosa > 0.0 )
			VREVERSE( segp->seg_in.hit_normal, cline->h )
		else
			VMOVE( segp->seg_in.hit_normal, cline->h );

		segp->seg_out.hit_dist = dist[1];
		segp->seg_out.hit_surfno = -1;
		if( cosa < 0.0 )
			VREVERSE( segp->seg_out.hit_normal, cline->h )
		else
			VMOVE( segp->seg_out.hit_normal, cline->h );
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );
		return( 1 );
	}

	if( dist[2] > reff*reff )
		return( 0 );	/* missed */

	if( dist[0] <= 0.0 || dist[0] >= 1.0 )
		return( 0 );	/* missed */

	sina = sqrt( 1.0 - cosa*cosa);
	tmp = sqrt( dist[2] ) - ap->a_rbeam;
	if( dist[2] > ap->a_rbeam * ap->a_rbeam )
		half_los = sqrt( cline->radius*cline->radius - tmp*tmp) / sina;
	else
		half_los = cline->radius / sina;

	VSUB2( diff, cline->V, rp->r_pt );
	distmin = VDOT( rp->r_dir, diff );
	VADD2( diff, cline->V, cline->height );
	VSUB2( diff, diff, rp->r_pt );
	distmax = VDOT( rp->r_dir, diff );

	if( distmin > distmax )
	{
		tmp = distmin;
		distmin = distmax;
		distmax = tmp;
	}

	distmin -= cline->radius;
	distmax += cline->radius;

	if( cline->thickness <= 0.0 )
	{
		/* volume mode */

		RT_GET_SEG( segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in.hit_surfno = 2;
		segp->seg_in.hit_dist = dist[1] - half_los;
		if( segp->seg_in.hit_dist < distmin )
			segp->seg_in.hit_dist = distmin;
		VMOVE( segp->seg_in.hit_vpriv, cline->h );

		segp->seg_out.hit_surfno = -2;
		segp->seg_out.hit_dist = dist[1] + half_los;
		if( segp->seg_out.hit_dist > distmax )
			segp->seg_out.hit_dist = distmax;
		VMOVE( segp->seg_out.hit_vpriv, cline->h );
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );

		return( 1 );
	}
	else
	{
		/* plate mode */

		RT_GET_SEG( segp, ap->a_resource);
                        segp->seg_stp = stp;
                        segp->seg_in.hit_surfno = 2;
		segp->seg_in.hit_dist = dist[1] - half_los;
		if( segp->seg_in.hit_dist < distmin )
			segp->seg_in.hit_dist = distmin;
		VMOVE( segp->seg_in.hit_vpriv, cline->h );

		segp->seg_out.hit_surfno = -2;
		segp->seg_out.hit_dist = segp->seg_in.hit_dist + cline->thickness;
		VMOVE( segp->seg_out.hit_vpriv, cline->h );
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );

		RT_GET_SEG( segp, ap->a_resource);
                        segp->seg_stp = stp;
                        segp->seg_in.hit_surfno = 2;
		segp->seg_in.hit_dist = dist[1] + half_los;
		if( segp->seg_in.hit_dist > distmax )
			segp->seg_in.hit_dist = distmax;
		segp->seg_in.hit_dist -=  cline->thickness;
		VMOVE( segp->seg_in.hit_vpriv, cline->h );

		segp->seg_out.hit_surfno = -2;
		segp->seg_out.hit_dist = segp->seg_in.hit_dist + cline->thickness;
		VMOVE( segp->seg_out.hit_vpriv, cline->h );
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );

		return( 2 );
	}
}

#define RT_CLINE_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ C L I N E _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_cline_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ C L I N E _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_cline_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	vect_t tmp;
	fastf_t dot;

	if( hitp->hit_surfno == 1 || hitp->hit_surfno == -1 )
		return;

	/* only need to do some calculations for surfno 2 or -2 */

	/* this is wrong, but agrees with FASTGEN */
	VCROSS( tmp, rp->r_dir, hitp->hit_vpriv );
	VCROSS( hitp->hit_normal, tmp, hitp->hit_vpriv );
	VUNITIZE( hitp->hit_normal );
	dot = VDOT( hitp->hit_normal, rp->r_dir );
	if( dot < 0.0 && hitp->hit_surfno < 0 )
		VREVERSE( hitp->hit_normal, hitp->hit_normal )
	else if( dot >  0.0 && hitp->hit_surfno > 0 )
		VREVERSE( hitp->hit_normal, hitp->hit_normal )
}

/*
 *			R T _ C L I N E _ C U R V E
 *
 *  Return the curvature of the cline.
 */
void
rt_cline_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{

	/* for now, don't do curvature */
 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ C L I N E_ U V
 *  
 *  For a hit on the surface of an cline, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 */
void
rt_cline_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	uvp->uv_u = 0.0;
	uvp->uv_v = 0.0;
	uvp->uv_du = 0.0;
	uvp->uv_dv = 0.0;
}

/*
 *		R T _ C L I N E _ F R E E
 */
void
rt_cline_free( stp )
register struct soltab *stp;
{
	register struct cline_specific *cline =
		(struct cline_specific *)stp->st_specific;

	bu_free( (char *)cline, "cline_specific" );
}

/*
 *			R T _ C L I N E _ C L A S S
 */
int
rt_cline_class( stp, min, max, tol )
CONST struct soltab    *stp;
CONST vect_t		min, max;
CONST struct bn_tol    *tol;
{

	return( 0 );
}

/*
 *			R T _ C L I N E _ P L O T
 */
int
rt_cline_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_cline_internal	*cline_ip;
        LOCAL fastf_t           top[16*3];
        LOCAL fastf_t           bottom[16*3];
	point_t top_pt;
	vect_t unit_a, unit_b;
	vect_t a, b;
	fastf_t inner_radius;
	int i;

	RT_CK_DB_INTERNAL(ip);
	cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
	RT_CLINE_CK_MAGIC(cline_ip);

	VADD2( top_pt, cline_ip->v, cline_ip->h );
	bn_vec_ortho( unit_a, cline_ip->h );
	VCROSS( unit_b, unit_a, cline_ip->h );
	VUNITIZE( unit_b );
	VSCALE( a, unit_a, cline_ip->radius );
	VSCALE( b, unit_b, cline_ip->radius );

	rt_ell_16pts( bottom, cline_ip->v, a, b );
	rt_ell_16pts( top, top_pt, a, b );

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

	if( cline_ip->thickness > 0.0 && cline_ip->thickness < cline_ip->radius )
	{
		/* draw inner cylinder */

		inner_radius = cline_ip->radius - cline_ip->thickness;

		VSCALE( a, unit_a, inner_radius );
		VSCALE( b, unit_b, inner_radius );

		rt_ell_16pts( bottom, cline_ip->v, a, b );
		rt_ell_16pts( top, top_pt, a, b );

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

	}

        return(0);
}

/*
 *			R T _ C L I N E _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_cline_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_cline_internal	*cline_ip;

	RT_CK_DB_INTERNAL(ip);
	cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
	RT_CLINE_CK_MAGIC(cline_ip);

	return(-1);
}

/*
 *			R T _ C L I N E _ I M P O R T
 *
 *  Import an cline from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_cline_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	LOCAL struct rt_cline_internal	*cline_ip;
	union record			*rp;
	point_t 			work;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */

	if( rp->u_id != DBID_CLINE )  {
		bu_log("rt_cline_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_CLINE;
	ip->idb_meth = &rt_functab[ID_CLINE];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_cline_internal), "rt_cline_internal");
	cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
	cline_ip->magic = RT_CLINE_INTERNAL_MAGIC;
	ntohd( (unsigned char *)(&cline_ip->thickness), rp->cli.cli_thick, 1 );
	cline_ip->thickness /= mat[15];
	ntohd( (unsigned char *)(&cline_ip->radius), rp->cli.cli_radius, 1 );
	cline_ip->radius /= mat[15];
	ntohd( (unsigned char *)(&work), rp->cli.cli_V, 3 );
	MAT4X3PNT( cline_ip->v, mat, work );
	ntohd( (unsigned char *)(&work), rp->cli.cli_h, 3 );
	MAT4X3VEC( cline_ip->h, mat, work );

	return(0);			/* OK */
}

/*
 *			R T _ C L I N E _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_cline_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_cline_internal	*cline_ip;
	union record			*rec;
	fastf_t				tmp;
	point_t				work;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_CLINE )  return(-1);
	cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
	RT_CLINE_CK_MAGIC(cline_ip);

	BU_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "cline external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->cli.cli_id = DBID_CLINE;	/* GED primitive type from db.h */

	tmp = cline_ip->thickness * local2mm;
	htond( rec->cli.cli_thick, (unsigned char *)(&tmp), 1 );
	tmp = cline_ip->radius * local2mm;
	htond( rec->cli.cli_radius, (unsigned char *)(&tmp), 1 );
	VSCALE( work, cline_ip->v, local2mm );
	htond( rec->cli.cli_V, (unsigned char *)work, 3 );
	VSCALE( work, cline_ip->h, local2mm );
	htond( rec->cli.cli_h, (unsigned char *)work, 3 );

	return(0);
}

/*
 *			R T _ C L I N E _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_cline_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
CONST struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_cline_internal	*cline_ip =
		(struct rt_cline_internal *)ip->idb_ptr;
	char	buf[256];
	point_t local_v;
	vect_t local_h;

	RT_CLINE_CK_MAGIC(cline_ip);
	bu_vls_strcat( str, "cline solid (CLINE)\n");

	VSCALE( local_v, cline_ip->v, mm2local );
	VSCALE( local_h, cline_ip->h, mm2local );

	if( cline_ip->thickness > 0.0 )
	{
		sprintf( buf, "\tV (%g %g %g)\n\tH (%g %g %g)\n\tradius %g\n\tplate mode thickness %g",
				V3ARGS( local_v ), V3ARGS( local_h ), cline_ip->radius*mm2local, cline_ip->thickness*mm2local );
	}
	else
	{
		sprintf( buf, "\tV (%g %g %g)\n\tH (%g %g %g)\n\tradius %g\n\tVolume mode\n",
				V3ARGS( local_v ), V3ARGS( local_h ), cline_ip->radius*mm2local );
	}
	bu_vls_strcat( str, buf );

	return(0);
}

/*
 *			R T _ C L I N E _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_cline_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_cline_internal	*cline_ip;

	RT_CK_DB_INTERNAL(ip);
	cline_ip = (struct rt_cline_internal *)ip->idb_ptr;
	RT_CLINE_CK_MAGIC(cline_ip);
	cline_ip->magic = 0;			/* sanity */

	bu_free( (char *)cline_ip, "cline ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

int
rt_cline_tnurb( r, m, ip, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct bn_tol		*tol;
{
	return( 1 );
}

int
rt_cline_tclget( interp, intern, attr )
Tcl_Interp                      *interp;
CONST struct rt_db_internal     *intern;
CONST char                      *attr;
{
	register struct rt_cline_internal *cli = 
		(struct rt_cline_internal *)intern->idb_ptr;
	Tcl_DString     ds;
	struct bu_vls   vls;
	int ret=TCL_OK;

	RT_CLINE_CK_MAGIC( cli );

	Tcl_DStringInit( &ds );
	bu_vls_init( &vls );

	if( attr == (char *)NULL )
	{
		bu_vls_strcpy( &vls, "cline" );
		bu_vls_printf( &vls, " V {%.25g %.25g %.25g}", V3ARGS( cli->v ) );
		bu_vls_printf( &vls, " H {%.25g %.25g %.25g}", V3ARGS( cli->h ) );
		bu_vls_printf( &vls, " R %.25g T %.25g", cli->radius, cli->thickness );
	}
	else if( *attr == 'V')
		bu_vls_printf( &vls, "%.25g %.25g %.25g", V3ARGS( cli->v ) );
	else if( *attr == 'H' )
		bu_vls_printf( &vls, "%.25g %.25g %.25g", V3ARGS( cli->h ) );
	else if( *attr == 'R' )
		bu_vls_printf( &vls, "%.25g", cli->radius );
	else if( *attr == 'T' )
		bu_vls_printf( &vls, "%.25g", cli->thickness );
	else
	{
		bu_vls_strcat( &vls, "ERROR: unrecognized attribute, must be V, H, R, or T!!!" );
		ret = TCL_ERROR;
	}

        Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
        Tcl_DStringResult( interp, &ds );
        Tcl_DStringFree( &ds );
        bu_vls_free( &vls );
        return( ret );
}

int
rt_cline_tcladjust( interp, intern, argc, argv )
Tcl_Interp              *interp;
struct rt_db_internal   *intern;
int                     argc;
char                    **argv;
{
	struct rt_cline_internal *cli =
		(struct rt_cline_internal *)intern->idb_ptr;
	int ret;
	fastf_t *new;

	RT_CK_DB_INTERNAL( intern );
	RT_CLINE_CK_MAGIC( cli );

	while( argc >= 2 )
	{
		int array_len=3;

		if( *argv[0] == 'V' )
		{
			new = cli->v;
			if( (ret=tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) ) )
				return( ret );
		}
		else if( *argv[0] == 'H' )
		{
			new = cli->h;
			if( (ret=tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) ) )
				return( ret );
		}
		else if( *argv[0] == 'R' )
			cli->radius = atof( argv[1] );
		else if( *argv[0] == 'T' )
			cli->thickness = atof( argv[1] );

		argc -= 2;
		argv += 2;
	}

	return( TCL_OK );
}
