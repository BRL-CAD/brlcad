/*
 *			G _ P I P E . C
 *
 *  Purpose -
 *	Intersect a ray with a pipe solid
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

RT_EXTERN( void rt_pipe_ifree, (struct rt_db_internal *ip) );

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
#if NEW_IF
int
rt_pipe_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
#else
int
rt_pipe_prep( stp, rec, rtip )
struct soltab		*stp;
union record		*rec;
struct rt_i		*rtip;
{
	struct rt_external	ext, *ep;
	struct rt_db_internal	intern, *ip;
#endif
	register struct pipe_specific *pipe;
	struct pipe_internal	*pip;
	int			i;

#if NEW_IF
	/* All set */
#else
	ep = &ext;
	RT_INIT_EXTERNAL(ep);
	ep->ext_buf = (genptr_t)rec;
	ep->ext_nbytes = stp->st_dp->d_len*sizeof(union record);
	ip = &intern;
	i = rt_pipe_import( &intern, ep, stp->st_pathmat );
	if( i < 0 )  {
		rt_log("rt_pipe_setup(%s): db import failure\n", stp->st_name);
		return(-1);		/* BAD */
	}
#endif
	RT_CK_DB_INTERNAL( ip );
	pip = (struct pipe_internal *)ip->idb_ptr;

	rt_pipe_ifree( ip );
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
#if NEW_IF
int
rt_pipe_plot( vhead, mat, ip, abs_tol, rel_tol, norm_tol )
struct vlhead	*vhead;
mat_t		mat;
struct rt_db_internal *ip;
double		abs_tol;
double		rel_tol;
double		norm_tol;
{
#else
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
	struct rt_external	ext, *ep;
	struct rt_db_internal	intern, *ip;
#endif
	register struct wdb_pipeseg	*psp;
	register struct wdb_pipeseg	*np;
	struct pipe_internal	*pip;
	vect_t		head, tail;
	point_t		pt;
	int		i;

#if NEW_IF
	/* All set */
#else
	ep = &ext;
	RT_INIT_EXTERNAL(ep);
	ep->ext_buf = (genptr_t)rp;
	ep->ext_nbytes = dp->d_len*sizeof(union record);
	i = rt_pipe_import( &intern, ep, mat );
	if( i < 0 )  {
		rt_log("rt_pipe_plot(): db import failure\n");
		return(-1);		/* BAD */
	}
	ip = &intern;
#endif
	RT_CK_DB_INTERNAL(ip);
	pip = (struct pipe_internal *)ip->idb_ptr;

	np = RT_LIST_FIRST(wdb_pipeseg, &pip->pipe_segs.l);
	ADD_VL( vhead, np->ps_start, 0 );
	for( RT_LIST( psp, wdb_pipeseg, &pip->pipe_segs.l ) )  {
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

	rt_pipe_ifree( ip );
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
rt_pipe_import( ip, ep, mat )
struct rt_db_internal	*ip;
struct rt_external	*ep;
register mat_t		mat;
{
	register struct exported_pipeseg *exp;
	register struct wdb_pipeseg	*psp;
	struct wdb_pipeseg		tmp;
	struct pipe_internal		*pipe;
	union record			*rp;
	int				count;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_PIPE )  {
		rt_log("rt_pipe_import: defective record\n");
		return(-1);
	}

	/* Count number of segments */
	count = 0;
	for( exp = &rp->pw.pw_data[0]; ; exp++ )  {
		count++;
		switch( (int)(exp->eps_type[0]) )  {
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
	if( count <= 1 )
		return(-3);		/* Not enough for 1 pipe! */

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_PIPE;
	ip->idb_ptr = rt_malloc( sizeof(struct pipe_internal), "pipe_internal");
	pipe = (struct pipe_internal *)ip->idb_ptr;
	pipe->pipe_count = count;

	/*
	 *  Walk the array of segments in reverse order,
	 *  allocating a linked list of segments in internal format,
	 *  using exactly the same structures as libwdb.
	 */
	RT_LIST_INIT( &pipe->pipe_segs.l );
	for( exp = &rp->pw.pw_data[pipe->pipe_count-1]; exp >= &rp->pw.pw_data[0]; exp-- )  {
		tmp.ps_type = (int)exp->eps_type[0];
		ntohd( tmp.ps_start, exp->eps_start, 3 );
		ntohd( &tmp.ps_id, exp->eps_id, 1 );
		ntohd( &tmp.ps_od, exp->eps_od, 1 );

		/* Apply modeling transformations */
		GETSTRUCT( psp, wdb_pipeseg );
		psp->ps_type = tmp.ps_type;
		MAT4X3PNT( psp->ps_start, mat, tmp.ps_start );
		if( psp->ps_type == WDB_PIPESEG_TYPE_BEND )  {
			ntohd( tmp.ps_bendcenter, exp->eps_bendcenter, 3 );
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

/*
 *			R T _ P I P E _ E X P O R T
 */
int
rt_pipe_export( ep, ip )
struct rt_external	*ep;
struct rt_db_internal	*ip;
{
	return(-1);
}

/*
 *			R T _ P I P E _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_pipe_describe( str, ip, verbose )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
{
	register struct pipe_internal	*pip =
		(struct pipe_internal *)ip->idb_ptr;
	register struct wdb_pipeseg	*psp;
	char	buf[256];
	int	segno = 0;

	RT_CK_DB_INTERNAL(ip);

	sprintf(buf, "pipe with %d segments\n", pip->pipe_count );
	rt_vls_strcat( str, buf );

	for( RT_LIST_FOR( psp, wdb_pipeseg, &pip->pipe_segs.l ) )  {
		/* XXX check magic number here */
		sprintf(buf, "\t%d ", segno++ );
		rt_vls_strcat( str, buf );
		switch( psp->ps_type )  {
		case WDB_PIPESEG_TYPE_END:
			rt_vls_strcat( str, "END" );
			break;
		case WDB_PIPESEG_TYPE_LINEAR:
			rt_vls_strcat( str, "LINEAR" );
			break;
		case WDB_PIPESEG_TYPE_BEND:
			rt_vls_strcat( str, "BEND" );
			break;
		default:
			return(-1);
		}
		sprintf(buf, "  od=%g", psp->ps_od );
		rt_vls_strcat( str, buf );
		if( psp->ps_id > 0 )  {
			sprintf(buf, ", id  = %g", psp->ps_id );
			rt_vls_strcat( str, buf );
		}
		rt_vls_strcat( str, "\n" );

		sprintf(buf, "\t  start=(%g, %g, %g)\n", V3ARGS(psp->ps_start) );
		rt_vls_strcat( str, buf );

		if( psp->ps_type == WDB_PIPESEG_TYPE_BEND )  {
			sprintf(buf, "\t  bendcenter=(%g, %g, %g)\n",
				V3ARGS(psp->ps_bendcenter) );
			rt_vls_strcat( str, buf );
		}
	}
	return(0);
}

/*
 *  Free the storage associated with the rt_db_internal version of this solid.
 *  XXX The suffix of this name is temporary.
 */
void
rt_pipe_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct pipe_internal	*pipe =
		(struct pipe_internal*)ip->idb_ptr;
	register struct wdb_pipeseg	*psp;

	RT_CK_DB_INTERNAL(ip);

	while( RT_LIST_WHILE( psp, wdb_pipeseg, &pipe->pipe_segs.l ) )  {
		RT_LIST_DEQUEUE( &(psp->l) );
		rt_free( (char *)psp, "wdb_pipeseg" );
	}
	rt_free( ip->idb_ptr, "pipe ifree" );
}
