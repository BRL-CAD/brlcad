/*
 *			G _ P I P E . C
 *
 *  Purpose -
 *	Intersect a ray with a 
 *
 *  Authors -
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSpipe[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "rtlist.h"
#include "raytrace.h"
#include "wdb.h"
#include "./debug.h"

struct pipe_internal {
	int		pipe_count;
	struct wdb_pipeseg pipe_segs;
};

struct pipe_specific {
	vect_t	pipe_V;
};

/*
 *  			R T _ P I P E _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid pipe solid, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	pipe solid is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct pipe_specific is created, and it's address is stored in
 *  	stp->st_specific for use by pipe_shot().
 */
int
rt_pipe_prep( stp, rec, rtip )
struct soltab		*stp;
union record		*rec;
struct rt_i		*rtip;
{
	register struct pipe_specific *pipe;
	struct pipe_internal	pi;
	int			i;

	if( rec == (union record *)0 )  {
		rec = db_getmrec( rtip->rti_dbip, stp->st_dp );
		i = rt_pipe_import( &pi, rec, stp->st_pathmat );
		rt_free( (char *)rec, "pipe record" );
	} else {
		i = rt_pipe_import( &pi, rec, stp->st_pathmat );
	}
	if( i < 0 )  {
		rt_log("rt_pipe_setup(%s): db import failure\n", stp->st_name);
		return(-1);		/* BAD */
	}

	return(-1);	/* unfinished */
}

/*
 *			R T _ P I P E _ P R I N T
 */
void
rt_pipe_print( stp )
register struct soltab *stp;
{
	register struct pipe_specific *pipe =
		(struct pipe_specific *)stp->st_specific;
}

/*
 *  			R T _ P I P E _ S H O T
 *  
 *  Intersect a ray with a pipe.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_pipe_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct pipe_specific *pipe =
		(struct pipe_specific *)stp->st_specific;
	register struct seg *segp;

	return(2);			/* HIT */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/*
 *			S P H _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_pipe_vshot( stp, rp, segp, n, resp)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
{
	rt_vstub( stp, rp, segp, n, resp );
}

/*
 *  			R T _ P I P E _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_pipe_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct pipe_specific *pipe =
		(struct pipe_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/*
 *			R T _ P I P E _ C U R V E
 *
 *  Return the curvature of the pipe.
 */
void
rt_pipe_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct pipe_specific *pipe =
		(struct pipe_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ P I P E _ U V
 *  
 *  For a hit on the surface of an pipe, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_pipe_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct pipe_specific *pipe =
		(struct pipe_specific *)stp->st_specific;
}

/*
 *		R T _ P I P E _ F R E E
 */
void
rt_pipe_free( stp )
register struct soltab *stp;
{
	register struct pipe_specific *pipe =
		(struct pipe_specific *)stp->st_specific;

	rt_free( (char *)pipe, "pipe_specific" );
}

/*
 *			R T _ P I P E _ C L A S S
 */
int
rt_pipe_class()
{
	return(0);
}

/*
 *			R T _ P I P E _ P L O T
 */
int
rt_pipe_plot( rp, mat, vhead, dp, abs_tol, rel_tol, norm_tol )
union record	*rp;
mat_t		mat;
struct vlhead	*vhead;
struct directory *dp;
double		abs_tol;
double		rel_tol;
double		norm_tol;
{
	register struct wdb_pipeseg	*psp;
	register struct wdb_pipeseg	*np;
	struct pipe_internal	pi;
	vect_t		head, tail;
	point_t		pt;
	int		i;

	if( rt_pipe_import( &pi, rp, mat ) < 0 )  {
		rt_log("rt_pipe_plot(%s): db import failure\n", dp->d_namep);
		return(-1);
	}

	np = RT_LIST_FIRST(wdb_pipeseg, &pi.pipe_segs.l);
	ADD_VL( vhead, np->ps_start, 0 );
	for( RT_LIST( psp, wdb_pipeseg, &pi.pipe_segs.l ) )  {
		switch( psp->ps_type )  {
		case WDB_PIPESEG_TYPE_END:
			/* Previous segment aleady connected to end plate */
			break;
		case WDB_PIPESEG_TYPE_LINEAR:
			np = RT_LIST_PNEXT(wdb_pipeseg, &psp->l);
			ADD_VL( vhead, np->ps_start, 1 );
			break;
		case WDB_PIPESEG_TYPE_BEND:
			VSUB2( head, psp->ps_start, psp->ps_bendcenter );
			np = RT_LIST_PNEXT(wdb_pipeseg, &psp->l);
			VSUB2( tail, np->ps_start, psp->ps_bendcenter );
			for( i=0; i <= 4; i++ )  {
				double	ang;
				double	cos_ang, sin_ang;
				ang = rt_halfpi * i / 4.0;
				cos_ang = cos(ang);
				sin_ang = sin(ang);
				VJOIN2( pt, psp->ps_bendcenter,
					cos_ang, head,
					sin_ang, tail );
				ADD_VL( vhead, pt, 1 );
			}
			break;
		default:
			return(-1);
		}
	}

	/* XXX Need to release storage here! */
	return(0);
}

/*
 *			R T _ P I P E _ T E S S
 */
int
rt_pipe_tess( r, m, rp, mat, dp, abs_tol, rel_tol, norm_tol )
struct nmgregion	**r;
struct model		*m;
union record		*rp;
mat_t			mat;
struct directory	*dp;
double			abs_tol;
double			rel_tol;
double			norm_tol;
{
	return(-1);
}

/*
 *			R T _ P I P E _ I M P O R T
 */
int
rt_pipe_import( pipe, rp, mat )
struct pipe_internal	*pipe;
union record		*rp;
register mat_t		mat;
{
	register struct exported_pipeseg *ep;
	register struct wdb_pipeseg	*psp;
	struct wdb_pipeseg		tmp;

	/* Check record type */
	if( rp->u_id != DBID_PIPE )  {
		rt_log("rt_pipe_import: defective record\n");
		return(-1);
	}

	/* Count number of segments */
	pipe->pipe_count = 0;
	for( ep = &rp->pw.pw_data[0]; ; ep++ )  {
		pipe->pipe_count++;
		switch( (int)(ep->eps_type[0]) )  {
		case WDB_PIPESEG_TYPE_END:
			goto done;
		case WDB_PIPESEG_TYPE_LINEAR:
		case WDB_PIPESEG_TYPE_BEND:
			break;
		default:
			return(-2);	/* unknown segment type */
		}
	}
done:	;
	if( pipe->pipe_count <= 1 )
		return(-3);		/* Not enough for 1 pipe! */

	/*
	 *  Walk the array of segments in reverse order,
	 *  allocating a linked list of segments in internal format,
	 *  using exactly the same structures as libwdb.
	 */
	RT_LIST_INIT( &pipe->pipe_segs.l );
	for( ep = &rp->pw.pw_data[pipe->pipe_count-1]; ep >= &rp->pw.pw_data[0]; ep-- )  {
		tmp.ps_type = (int)ep->eps_type[0];
		ntohd( tmp.ps_start, ep->eps_start, 3 );
		ntohd( &tmp.ps_id, ep->eps_id, 1 );
		ntohd( &tmp.ps_od, ep->eps_od, 1 );

		/* Apply modeling transformations */
		GETSTRUCT( psp, wdb_pipeseg );
		psp->ps_type = tmp.ps_type;
		MAT4X3PNT( psp->ps_start, mat, tmp.ps_start );
		if( psp->ps_type == WDB_PIPESEG_TYPE_BEND )  {
			ntohd( tmp.ps_bendcenter, ep->eps_bendcenter, 3 );
			MAT4X3PNT( psp->ps_bendcenter, mat, tmp.ps_bendcenter );
		} else {
			VSETALL( psp->ps_bendcenter, 0 );
		}
		psp->ps_id = tmp.ps_id / mat[15];
		psp->ps_od = tmp.ps_od / mat[15];
		RT_LIST_APPEND( &pipe->pipe_segs.l, &psp->l );
	}

	return(0);			/* OK */
}
