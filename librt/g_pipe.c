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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "rtlist.h"
#include "raytrace.h"
#include "nmg.h"
#include "wdb.h"
#include "rtgeom.h"
#include "./debug.h"

struct pipe_specific {
	struct rt_list l;
	int	pipe_type;
	vect_t	pipe_V;
	vect_t	pipe_H;
	vect_t	pipe_bend_center;
	fastf_t	pipe_len;
	fastf_t pipe_ribase, pipe_ritop;
	fastf_t pipe_ribase_sq, pipe_ritop_sq;
	fastf_t pipe_ridiff_sq, pipe_ridiff;
	fastf_t pipe_rodiff_sq, pipe_rodiff;
	fastf_t pipe_robase, pipe_rotop;
	fastf_t pipe_robase_sq, pipe_rotop_sq;
	mat_t	pipe_SoR;	/* Scale and rotate */
	mat_t	pipe_invRoS;	/* inverse rotation and scale */
};

#define PIPE_MM(_v)       VMINMAX( stp->st_min, stp->st_max, _v );

#define	ARC_SEGS	16	/* number of segments used to plot a circle */

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
int
rt_pipe_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	register struct pipe_specific *head;
	struct rt_pipe_internal	*pip;
	struct wdb_pipeseg *pipe_seg;
	fastf_t dx,dy,dz,f;

	RT_CK_DB_INTERNAL( ip );
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	GETSTRUCT( head, pipe_specific );
	stp->st_specific = (genptr_t)head;
	RT_LIST_INIT( &head->l );

	/* Compute bounding sphere and RPP */
	for( RT_LIST_FOR( pipe_seg, wdb_pipeseg, &(pip->pipe_segs_head) ) )
	{
		register struct pipe_specific *pipe;
		register struct wdb_pipeseg *next_seg;
		LOCAL mat_t	R;
		LOCAL mat_t	Rinv;
		LOCAL mat_t	S;
		LOCAL point_t work;
		LOCAL point_t top;
		LOCAL vect_t seg_ht;
		LOCAL vect_t v1,v2;
		LOCAL fastf_t seg_length;

		if( pipe_seg->ps_type == WDB_PIPESEG_TYPE_END )
			break;

		GETSTRUCT( pipe, pipe_specific );
		RT_LIST_INSERT( &head->l, &pipe->l );

		next_seg = RT_LIST_PNEXT( wdb_pipeseg, pipe_seg );

		VMOVE( pipe->pipe_V, pipe_seg->ps_start );

		VSUB2( seg_ht, next_seg->ps_start, pipe_seg->ps_start );
		VMOVE( pipe->pipe_bend_center, pipe_seg->ps_bendcenter );
		pipe->pipe_ribase = pipe_seg->ps_id/2.0;
		pipe->pipe_ribase_sq = pipe->pipe_ribase * pipe->pipe_ribase;
		pipe->pipe_ritop = next_seg->ps_id/2.0;
		pipe->pipe_ritop_sq = pipe->pipe_ritop * pipe->pipe_ritop;
		pipe->pipe_robase = pipe_seg->ps_od/2.0;
		pipe->pipe_robase_sq = pipe->pipe_robase * pipe->pipe_robase;
		pipe->pipe_rotop = next_seg->ps_od/2.0;
		pipe->pipe_rotop_sq = pipe->pipe_rotop * pipe->pipe_rotop;
		pipe->pipe_ridiff = pipe->pipe_ritop - pipe->pipe_ribase;
		pipe->pipe_ridiff_sq = pipe->pipe_ridiff * pipe->pipe_ridiff;
		pipe->pipe_rodiff = pipe->pipe_rotop - pipe->pipe_robase;
		pipe->pipe_rodiff_sq = pipe->pipe_rodiff * pipe->pipe_rodiff;
		pipe->pipe_type = pipe_seg->ps_type;

		seg_length = MAGNITUDE( seg_ht );
		pipe->pipe_len = seg_length;
		VSCALE( seg_ht, seg_ht, 1.0/seg_length );
		VMOVE( pipe->pipe_H, seg_ht );
		mat_vec_ortho( v1, seg_ht );
		VCROSS( v2, seg_ht, v1 );

		/* build R matrix */
		mat_idn( R );
		VMOVE( &R[0], v1 );
		VMOVE( &R[4], v2 );
		VMOVE( &R[8], seg_ht );

		/* Rinv is transpose */
		mat_trn( Rinv, R );

		/* Build Scale matrix */
		mat_idn( S );
		S[10] = 1.0/seg_length;

		/* Compute SoR and invRoS */
		mat_mul( pipe->pipe_SoR, S, R );
		mat_mul( pipe->pipe_invRoS, Rinv, S );



		VJOIN2( work, pipe_seg->ps_start, pipe_seg->ps_od, v1, pipe_seg->ps_od, v2 );
		PIPE_MM( work )
		VJOIN2( work, pipe_seg->ps_start, -pipe_seg->ps_od, v1, pipe_seg->ps_od, v2 );
		PIPE_MM( work )
		VJOIN2( work, pipe_seg->ps_start, pipe_seg->ps_od, v1, -pipe_seg->ps_od, v2 );
		PIPE_MM( work )
		VJOIN2( work, pipe_seg->ps_start, -pipe_seg->ps_od, v1, -pipe_seg->ps_od, v2 );
		PIPE_MM( work )

		if( next_seg->ps_type != WDB_PIPESEG_TYPE_END )
			continue;
		VJOIN1( top, pipe_seg->ps_start, seg_length, seg_ht );
		VJOIN2( work, top, pipe_seg->ps_od, v1, pipe_seg->ps_od, v2 );
		PIPE_MM( work )
		VJOIN2( work, top, -pipe_seg->ps_od, v1, pipe_seg->ps_od, v2 );
		PIPE_MM( work )
		VJOIN2( work, top, pipe_seg->ps_od, v1, -pipe_seg->ps_od, v2 );
		PIPE_MM( work )
		VJOIN2( work, top, -pipe_seg->ps_od, v1, -pipe_seg->ps_od, v2 );
		PIPE_MM( work )

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

	return( 0 );
}

/*
 *			R T _ P I P E _ P R I N T
 */
void
rt_pipe_print( stp )
register CONST struct soltab *stp;
{
/*	register struct pipe_specific *pipe =
		(struct pipe_specific *)stp->st_specific; */
}

#define	PIPE_NORM_OUTER_BODY	1
#define	PIPE_NORM_INNER_BODY	2
#define	PIPE_NORM_TOP		3
#define	PIPE_NORM_BASE		4

void
linear_pipe_shot( stp, rp, ap, seghead, pipe, hits, hit_count, seg_no )
struct soltab           *stp;
register struct xray    *rp;
struct application      *ap;
struct seg              *seghead;
struct pipe_specific	*pipe;
struct hit		*hits;
int			*hit_count;
int			seg_no;
{
	LOCAL point_t	work_pt;
	LOCAL point_t	ray_start;
	LOCAL vect_t	ray_dir;
	LOCAL point_t	hit_pt;
	LOCAL double	t_tmp;
	LOCAL double	a,b,c;
	LOCAL double	descrim;
	LOCAL double	radius_sq;

	if( pipe->pipe_type != WDB_PIPESEG_TYPE_LINEAR )
	{
		rt_log( "linear_pipe_shot called for pipe type %d\n" , pipe->pipe_type );
		rt_bomb( "linear_pipe_shot\n" );
	}

	/* transform ray start point */
	VSUB2( work_pt, rp->r_pt, pipe->pipe_V );
	MAT4X3VEC( ray_start, pipe->pipe_SoR, work_pt );

	/* rotate ray direction */
	MAT4X3VEC( ray_dir, pipe->pipe_SoR, rp->r_dir );

	/* Intersect with outer sides */
	a = ray_dir[X]*ray_dir[X]
		+ ray_dir[Y]*ray_dir[Y]
		- ray_dir[Z]*ray_dir[Z]*pipe->pipe_rodiff_sq;
	b = 2.0*(ray_start[X]*ray_dir[X]
		+ ray_start[Y]*ray_dir[Y]
		- ray_start[Z]*ray_dir[Z]*pipe->pipe_rodiff_sq
		- ray_dir[Z]*pipe->pipe_robase*pipe->pipe_rodiff);
	c = ray_start[X]*ray_start[X]
		+ ray_start[Y]*ray_start[Y]
		- pipe->pipe_robase*pipe->pipe_robase
		- ray_start[Z]*ray_start[Z]*pipe->pipe_rodiff_sq
		- 2.0*ray_start[Z]*pipe->pipe_robase*pipe->pipe_rodiff;

	descrim = b*b - 4.0*a*c;

	if( descrim > 0.0 )
	{
		LOCAL fastf_t	sqrt_descrim;
		LOCAL point_t	hit_pt;

		sqrt_descrim = sqrt( descrim );

		t_tmp = (-b - sqrt_descrim)/(2.0*a);
		VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
		if( hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0 )
		{
			hits[*hit_count].hit_dist = t_tmp;
			hits[*hit_count].hit_surfno = seg_no*10 + PIPE_NORM_OUTER_BODY;
			VMOVE( hits[*hit_count].hit_vpriv, hit_pt );
			hits[*hit_count].hit_vpriv[Z] = (-pipe->pipe_robase - hit_pt[Z] * pipe->pipe_rodiff) *
					pipe->pipe_rodiff;
			(*hit_count)++;
		}

		t_tmp = (-b + sqrt_descrim)/(2.0*a);
		VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
		if( hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0 )
		{
			hits[*hit_count].hit_dist = t_tmp;
			hits[*hit_count].hit_surfno = seg_no*10 + PIPE_NORM_OUTER_BODY;
			VMOVE( hits[*hit_count].hit_vpriv, hit_pt );
			hits[*hit_count].hit_vpriv[Z] = (-pipe->pipe_robase - hit_pt[Z] * pipe->pipe_rodiff) *
					pipe->pipe_rodiff;
			(*hit_count)++;
		}
	}

	if( pipe->pipe_ribase > 0.0 || pipe->pipe_ritop > 0.0 )
	{
		/* Intersect with inner sides */

		a = ray_dir[X]*ray_dir[X]
			+ ray_dir[Y]*ray_dir[Y]
			- ray_dir[Z]*ray_dir[Z]*pipe->pipe_ridiff_sq;
		b = 2.0*(ray_start[X]*ray_dir[X]
			+ ray_start[Y]*ray_dir[Y]
			- ray_start[Z]*ray_dir[Z]*pipe->pipe_ridiff_sq
			- ray_dir[Z]*pipe->pipe_ribase*pipe->pipe_ridiff);
		c = ray_start[X]*ray_start[X]
			+ ray_start[Y]*ray_start[Y]
			- pipe->pipe_ribase*pipe->pipe_ribase
			- ray_start[Z]*ray_start[Z]*pipe->pipe_ridiff_sq
			- 2.0*ray_start[Z]*pipe->pipe_ribase*pipe->pipe_ridiff;

		descrim = b*b - 4.0*a*c;

		if( descrim > 0.0 )
		{
			LOCAL fastf_t	sqrt_descrim;
			LOCAL point_t	hit_pt;

			sqrt_descrim = sqrt( descrim );

			t_tmp = (-b - sqrt_descrim)/(2.0*a);
			VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
			if( hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0 )
			{
				hits[*hit_count].hit_dist = t_tmp;
				hits[*hit_count].hit_surfno = seg_no*10 + PIPE_NORM_INNER_BODY;
				VMOVE( hits[*hit_count].hit_vpriv, hit_pt );
				hits[*hit_count].hit_vpriv[Z] = (-pipe->pipe_ribase - hit_pt[Z] * pipe->pipe_ridiff) *
					pipe->pipe_ridiff;
				(*hit_count)++;
			}

			t_tmp = (-b + sqrt_descrim)/(2.0*a);
			VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
			if( hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0 )
			{
				hits[*hit_count].hit_dist = t_tmp;
				hits[*hit_count].hit_surfno = seg_no*10 + PIPE_NORM_INNER_BODY;
				VMOVE( hits[*hit_count].hit_vpriv, hit_pt );
				hits[*hit_count].hit_vpriv[Z] = (-pipe->pipe_ribase - hit_pt[Z] * pipe->pipe_ridiff) *
					pipe->pipe_ridiff;
				(*hit_count)++;
			}
		}
	}

	if( NEAR_ZERO( ray_dir[Z], SMALL ) )
		goto out;

	/* Intersect with base plane */
	t_tmp = (-ray_start[Z]/ray_dir[Z]);
	VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
	radius_sq = hit_pt[X]*hit_pt[X] + hit_pt[Y]*hit_pt[Y];
	VJOIN1( hit_pt, rp->r_pt, t_tmp, rp->r_dir );
	if( radius_sq <= pipe->pipe_robase_sq && radius_sq >= pipe->pipe_ribase_sq )
	{
		hits[*hit_count].hit_dist = t_tmp;
		hits[*hit_count].hit_surfno = seg_no*10 + PIPE_NORM_BASE;
		(*hit_count)++;
	}

	/* Intersect with top plane */
	t_tmp = (1.0 - ray_start[Z])/ray_dir[Z];
	VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
	radius_sq = hit_pt[X]*hit_pt[X] + hit_pt[Y]*hit_pt[Y];
	VJOIN1( hit_pt, rp->r_pt, t_tmp, rp->r_dir );
	if( radius_sq <= pipe->pipe_rotop_sq && radius_sq >= pipe->pipe_ritop_sq )
	{
		hits[*hit_count].hit_dist = t_tmp;
		hits[*hit_count].hit_surfno = seg_no*10 + PIPE_NORM_TOP;
		(*hit_count)++;
	}

out:
	if( (*hit_count) == 1 )
		(*hit_count) = 0;
}

HIDDEN void
rt_pipe_hitsort( h, nh )
register struct hit h[];
int *nh;
{
	register int i, j;
	LOCAL struct hit temp;

	for( i=0; i < (*nh)-1; i++ )  {
		for( j=i+1; j < (*nh); j++ )  {
			if( h[i].hit_dist <= h[j].hit_dist )
				continue;
			temp = h[j];		/* struct copy */
			h[j] = h[i];		/* struct copy */
			h[i] = temp;		/* struct copy */
		}
	}

	/* delete duplicate hits */
	for( i=0; i < (*nh)-1; i++ )
	{
		if( NEAR_ZERO( h[i].hit_dist - h[i+1].hit_dist , 0.0001 ) )
		{
			for( j=i ; j<(*nh)-1 ; j++ )
				h[j] = h[j+1];
			(*nh)--;
		}
	}

	if( *nh == 1 )
		(*nh) = 0;
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
	register struct pipe_specific *head =
		(struct pipe_specific *)stp->st_specific;
	register struct pipe_specific *pipe;
	register struct seg *segp;
	LOCAL int total_hits=0;
	LOCAL int seg_no=0;
	LOCAL int i;

	for( RT_LIST_FOR( pipe, pipe_specific, &head->l ) )
	{
		LOCAL struct hit hits[8];
		LOCAL int hit_count=0;

		seg_no++;

		switch( pipe->pipe_type )
		{
			case WDB_PIPESEG_TYPE_LINEAR:
				linear_pipe_shot( stp, rp, ap, seghead, pipe, hits, &hit_count, seg_no );
				total_hits += hit_count;
				break;
			case WDB_PIPESEG_TYPE_BEND:
				break;
			default:
				rt_log( "rt_pipe_shot: Bad pipe type (%d)\n", pipe->pipe_type );
				rt_bomb( "rt_pipe_shot\n" );
				break;
		}

		if( !hit_count )
			continue;

		rt_pipe_hitsort( hits, &hit_count );

		/* Build segments for this pipe segment */
		if( hit_count%2 )
		{
			rt_log( "rt_pipe_shot: bad number of hits (%d)\n" , hit_count );
			for( i=0 ; i<hit_count ; i++ )
			{
				point_t hit_pt;

				rt_log( "#%d, dist = %g, surfno=%d\n" , i, hits[i].hit_dist, hits[i].hit_surfno );
				VJOIN1( hit_pt, rp->r_pt, hits[i].hit_dist,  rp->r_dir );
				rt_log( "\t( %g %g %g )\n" , V3ARGS( hit_pt ) );
			}
			rt_bomb( "rt_pipe_shot\n" );
		}

		for( i=0 ; i<hit_count ; i += 2 )
		{

			RT_GET_SEG(segp, ap->a_resource);
			segp->seg_stp = stp;

			segp->seg_in = hits[i];
			segp->seg_out = hits[i+1];

			RT_LIST_INSERT( &(seghead->l), &(segp->l) );
		}
	}

	if( total_hits )
		return( 1 );		/* HIT */
	else
		return(0);		/* MISS */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/*
 *			R T_ P I P E _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_pipe_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
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
	LOCAL int	segno;
	LOCAL int	i;

	segno = hitp->hit_surfno/10;

	for( i=0 ; i<segno ; i++ )
		pipe = RT_LIST_PNEXT( pipe_specific, &pipe->l );

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	switch( hitp->hit_surfno%10 )
	{
		case PIPE_NORM_TOP:
			VMOVE( hitp->hit_normal, pipe->pipe_H );
			break;
		case PIPE_NORM_BASE:
			VREVERSE( hitp->hit_normal, pipe->pipe_H );
			break;
		case PIPE_NORM_OUTER_BODY:
			MAT4X3VEC( hitp->hit_normal, pipe->pipe_invRoS, hitp->hit_vpriv );
			VUNITIZE( hitp->hit_normal );
			break;
		case PIPE_NORM_INNER_BODY:
			MAT4X3VEC( hitp->hit_normal, pipe->pipe_invRoS, hitp->hit_vpriv );
			VUNITIZE( hitp->hit_normal );
			VREVERSE( hitp->hit_normal, hitp->hit_normal );
			break;
		default:
			rt_log( "rt_pipe_norm: Unrecognized surfno (%d)\n", hitp->hit_surfno );
			break;
	}
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
/*	register struct pipe_specific *pipe =
		(struct pipe_specific *)stp->st_specific; */

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
/*	register struct pipe_specific *pipe =
		(struct pipe_specific *)stp->st_specific; */
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

	/* free linked list */
	while( RT_LIST_NON_EMPTY( &pipe->l ) )
	{
		register struct pipe_specific *pipe_ptr;

		pipe_ptr = RT_LIST_FIRST( pipe_specific, &pipe->l );
		rt_free( (char *)pipe_ptr, "pipe_specific" );
	}

	/* free list head */
	rt_free( (char *)pipe, "pipe_specific head" );
}

/*
 *			R T _ P I P E _ C L A S S
 */
int
rt_pipe_class()
{
	return(0);
}
void
draw_pipe_circle( vhead, r, center, v1, v2, segments )
struct rt_list		*vhead;
fastf_t			r;
point_t			center;
vect_t			v1,v2;
int			segments;
{
	fastf_t		x;
	fastf_t		y;
	fastf_t		xnew;
	fastf_t		ynew;
	fastf_t		delta_ang;
	fastf_t		cos_delta;
	fastf_t		sin_delta;
	point_t		pt;
	int		i;

	if( r <= 0.0 )
		return;

	delta_ang = 2.0*M_PI/segments;
	cos_delta = cos( delta_ang );
	sin_delta = sin( delta_ang );

	x = r;
	y = 0.0;
	VJOIN1( pt, center, x, v1 );
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_MOVE );
	for( i=0 ; i<ARC_SEGS ; i++ )
	{
		xnew = x*cos_delta - y*sin_delta;
		ynew = x*sin_delta + y*cos_delta;
		VJOIN2( pt, center, xnew, v1, ynew, v2 );
		RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );
		x = xnew;
		y = ynew;
	}
	x = r;
	y = 0.0;
	VJOIN1( pt, center, x, v1 );
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );
}

static void
draw_pipe_surface( vhead, r1, r2, base_vertex, height, v1, v2, seg_count )
struct rt_list		*vhead;
fastf_t			r1;
fastf_t			r2;
point_t			base_vertex;
vect_t			height;
vect_t			v1,v2;
int			seg_count;
{
	point_t		pt;

	if( r1 > 0.0 )
		draw_pipe_circle( vhead, r1, base_vertex, v1, v2, seg_count );

	if( r1 > 0.0 )
	{
		VJOIN1( pt, base_vertex, r1, v2 );
	}
	else
	{
		VMOVE( pt, base_vertex );
	}
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_MOVE );
	VADD2( pt, base_vertex, height );
	if( r2 > 0.0 )
	{
		VJOIN1( pt, pt, r2, v2 );
	}
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );

	if( r1 > 0.0 )
	{
		VJOIN1( pt, base_vertex, r1, v1 );
	}
	else
	{
		VMOVE( pt, base_vertex );
	}
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_MOVE );
	VADD2( pt, base_vertex, height );
	if( r2 > 0.0 )
	{
		VJOIN1( pt, pt, r2, v1 );
	}
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );

	if( r1 > 0.0 )
	{
		VJOIN1( pt, base_vertex, (-r1), v2 );
	}
	else
	{
		VMOVE( pt, base_vertex );
	}
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_MOVE );
	VADD2( pt, base_vertex, height );
	if( r2 > 0.0 )
	{
		VJOIN1( pt, pt, (-r2), v2 );
	}
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );

	if( r1 > 0.0 )
	{
		VJOIN1( pt, base_vertex, (-r1), v1 );
	}
	else
	{
		VMOVE( pt, base_vertex );
	}
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_MOVE );
	VADD2( pt, base_vertex, height );
	if( r2 > 0.0 )
	{
		VJOIN1( pt, pt, (-r2), v1 );
	}
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );
}

static void
draw_pipe_arc( vhead, r_start, r_end, center, v1, v2, start, end, seg_count )
struct rt_list		*vhead;
fastf_t			r_start,r_end;
point_t			center;
vect_t			v1,v2;
point_t			start,end;
int			seg_count;
{
	fastf_t		arc_angle;
	fastf_t		delta_ang;
	fastf_t		cos_del, sin_del;
	fastf_t		x, y, xnew, ynew;
	vect_t		to_end;
	point_t		pt;
	int		i;

	VSUB2( to_end, end, center );
	arc_angle = atan2( VDOT( to_end, v2 ), VDOT( to_end, v1 ) );
	delta_ang = arc_angle/seg_count;
	cos_del = cos( delta_ang );
	sin_del = sin( delta_ang );

	x = 1.0;
	y = 0.0;
	RT_ADD_VLIST( vhead, start, RT_VLIST_LINE_MOVE );
	for( i=0 ; i<seg_count ; i++ )
	{
		vect_t radial_dir;
		fastf_t local_radius;

		xnew = x*cos_del - y*sin_del;
		ynew = x*sin_del + y*cos_del;
		VBLEND2( radial_dir, xnew, v1, ynew, v2 );
		local_radius = r_start + (fastf_t)i/(fastf_t)(seg_count-1) * (r_end - r_start);
		VJOIN1( pt, center, local_radius, radial_dir );
		RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );
		x = xnew;
		y = ynew;
	}
}

/*
 *			R T _ P I P E _ P L O T
 */
int
rt_pipe_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	register struct wdb_pipeseg		*psp;
	register struct wdb_pipeseg		*np;
	LOCAL struct rt_pipe_internal		*pip;
	LOCAL vect_t				height;
	LOCAL vect_t				pipe_dir;
	LOCAL vect_t				v1,v2,v3;
	LOCAL vect_t				tmp_vec;
	LOCAL point_t				tmp_center;
	LOCAL fastf_t				delta_ang;
	LOCAL fastf_t				cos_delta, sin_delta;
	LOCAL int				prev_type;

	RT_CK_DB_INTERNAL(ip);
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	delta_ang = 2.0*M_PI/ARC_SEGS;
	cos_delta = cos( delta_ang );
	sin_delta = sin( delta_ang );

	prev_type = 0;

	for( RT_LIST_FOR( psp, wdb_pipeseg, &pip->pipe_segs_head ) ) 
	{
		LOCAL fastf_t radius1,radius2;
		LOCAL fastf_t inv_radius1,inv_radius2;
		LOCAL vect_t end_dir;
		LOCAL point_t start,end;

		np = RT_LIST_PNEXT( wdb_pipeseg, &psp->l );
		switch( psp->ps_type )
		{
			case WDB_PIPESEG_TYPE_LINEAR:
				VSUB2( height, np->ps_start , psp->ps_start );
				VMOVE( pipe_dir, height );
				VUNITIZE( pipe_dir );
				mat_vec_ortho( v1, pipe_dir );
				VCROSS( v2, pipe_dir, v1 );

				/* draw outer surface */
				draw_pipe_surface( vhead, psp->ps_od/2.0, np->ps_od/2.0,
					psp->ps_start, height, v1, v2, ARC_SEGS );

				/* draw inner surface */
				if( psp->ps_id <= 0.0 && np->ps_id <= 0.0 )
					break;
				draw_pipe_surface( vhead, psp->ps_id/2.0, np->ps_id/2.0,
					psp->ps_start, height, v1, v2, ARC_SEGS );
				prev_type = WDB_PIPESEG_TYPE_LINEAR;
				break;
			case WDB_PIPESEG_TYPE_BEND:
				if( psp->ps_od > 0.0 )
					draw_pipe_circle( vhead, psp->ps_od/2.0, psp->ps_start,
							v1, v2, ARC_SEGS );
				if( psp->ps_id > 0.0 )
					draw_pipe_circle( vhead, psp->ps_id/2.0, psp->ps_start,
							v1, v2, ARC_SEGS );

				/* draw bend arcs in plane of bend */
				VSUB2( v1, psp->ps_start, psp->ps_bendcenter );
				radius1 = MAGNITUDE( v1 );
				inv_radius1 = 1.0/radius1;
				VSCALE( v1, v1, inv_radius1 );
				VCROSS( v2, pipe_dir, v1 );
				VSUB2( end_dir, np->ps_start, psp->ps_bendcenter );
				radius2 = MAGNITUDE( end_dir );
				inv_radius2 = 1.0/radius2;
				VSCALE( end_dir, end_dir, inv_radius2 );
				VJOIN1( start, psp->ps_start, psp->ps_od/2.0, v1 );
				VJOIN1( end, np->ps_start, np->ps_od/2.0, end_dir );
				draw_pipe_arc( vhead, radius1+psp->ps_od/2.0, radius2+np->ps_od/2.0,
					psp->ps_bendcenter, v1, pipe_dir, start, end, ARC_SEGS );
				VJOIN1( start, psp->ps_start, -psp->ps_od/2.0, v1 );
				VJOIN1( end, np->ps_start, -np->ps_od/2.0, end_dir );
				draw_pipe_arc( vhead, radius1-psp->ps_od/2.0, radius2-np->ps_od/2.0,
					psp->ps_bendcenter, v1, pipe_dir, start, end, ARC_SEGS );
				if( psp->ps_id > 0.0 || np->ps_id > 0.0 )
				{
					VJOIN1( start, psp->ps_start, psp->ps_id/2.0, v1 );
					VJOIN1( end, np->ps_start, np->ps_id/2.0, end_dir );
					draw_pipe_arc( vhead, radius1+psp->ps_id/2.0, radius2+np->ps_id/2.0,
						psp->ps_bendcenter, v1, pipe_dir, start, end, ARC_SEGS );
					VJOIN1( start, psp->ps_start, -psp->ps_id/2.0, v1 );
					VJOIN1( end, np->ps_start, -np->ps_id/2.0, end_dir );
					draw_pipe_arc( vhead, radius1-psp->ps_id/2.0, radius2-np->ps_id/2.0,
						psp->ps_bendcenter, v1, pipe_dir, start, end, ARC_SEGS );
				}

				/* draw bend arcs not in plane of bend */
				VJOIN1( tmp_center, psp->ps_bendcenter, psp->ps_od/2.0, v2 );
				VJOIN1( start, psp->ps_start, psp->ps_od/2.0, v2 );
				VJOIN1( end, np->ps_start, np->ps_od/2.0, v2 );
				VSUB2( v1, start, tmp_center );
				radius1 = MAGNITUDE( v1 );
				inv_radius1 = 1.0/radius1;
				VSCALE( v1, v1, inv_radius1 );
				VSUB2( v3, end, tmp_center );
				radius2 = MAGNITUDE( v3 );
				VCROSS( tmp_vec, v3, v1 );
				VCROSS( v3, v1, tmp_vec );
				VUNITIZE( v3 );
				draw_pipe_arc( vhead, radius1, radius2,
					tmp_center, v1, v3, start, end, ARC_SEGS );

				VJOIN1( tmp_center, psp->ps_bendcenter, -psp->ps_od/2.0, v2 );
				VJOIN1( start, psp->ps_start, -psp->ps_od/2.0, v2 );
				VJOIN1( end, np->ps_start, -np->ps_od/2.0, v2 );
				VSUB2( v1, start, tmp_center );
				radius1 = MAGNITUDE( v1 );
				inv_radius1 = 1.0/radius1;
				VSCALE( v1, v1, inv_radius1 );
				VSUB2( v3, end, tmp_center );
				radius2 = MAGNITUDE( v3 );
				VCROSS( tmp_vec, v3, v1 );
				VCROSS( v3, v1, tmp_vec );
				VUNITIZE( v3 );
				draw_pipe_arc( vhead, radius1, radius2,
					tmp_center, v1, v3, start, end, ARC_SEGS );

				if( psp->ps_id > 0.0 || np->ps_id > 0.0 )
				{
					VJOIN1( tmp_center, psp->ps_bendcenter, psp->ps_id/2.0, v2 );
					VJOIN1( start, psp->ps_start, psp->ps_id/2.0, v2 );
					VJOIN1( end, np->ps_start, np->ps_id/2.0, v2 );
					VSUB2( v1, start, tmp_center );
					radius1 = MAGNITUDE( v1 );
					inv_radius1 = 1.0/radius1;
					VSCALE( v1, v1, inv_radius1 );
					VSUB2( v3, end, tmp_center );
					radius2 = MAGNITUDE( v3 );
					VCROSS( tmp_vec, v3, v1 );
					VCROSS( v3, v1, tmp_vec );
					VUNITIZE( v3 );
					draw_pipe_arc( vhead, radius1, radius2,
						tmp_center, v1, v3, start, end, ARC_SEGS );

					VJOIN1( tmp_center, psp->ps_bendcenter, -psp->ps_id/2.0, v2 );
					VJOIN1( start, psp->ps_start, -psp->ps_id/2.0, v2 );
					VJOIN1( end, np->ps_start, -np->ps_id/2.0, v2 );
					VSUB2( v1, start, tmp_center );
					radius1 = MAGNITUDE( v1 );
					inv_radius1 = 1.0/radius1;
					VSCALE( v1, v1, inv_radius1 );
					VSUB2( v3, end, tmp_center );
					radius2 = MAGNITUDE( v3 );
					VCROSS( tmp_vec, v3, v1 );
					VCROSS( v3, v1, tmp_vec );
					VUNITIZE( v3 );
					draw_pipe_arc( vhead, radius1, radius2,
						tmp_center, v1, v3, start, end, ARC_SEGS );
				}

				/* prepare for drawing END circles */
				VSUB2( v1, np->ps_start, psp->ps_bendcenter );
				VUNITIZE( v1 );
				prev_type = WDB_PIPESEG_TYPE_BEND;
				break;
			case WDB_PIPESEG_TYPE_END:
				if( psp->ps_od > 0.0 )
					draw_pipe_circle( vhead, psp->ps_od/2.0,
						psp->ps_start, v1, v2, ARC_SEGS );
				if( psp->ps_id > 0.0 )
					draw_pipe_circle( vhead, psp->ps_id/2.0,
						psp->ps_start, v1, v2, ARC_SEGS );
				break;
			default:
				rt_log("rt_pipe_plot: unknown ps_type=%d\n", psp->ps_type);
				return(-1);
		}
	}
	return(0);
}

/*
 *			R T _ P I P E _ T E S S
 */
int
rt_pipe_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	return(-1);
}

/*
 *			R T _ P I P E _ I M P O R T
 */
int
rt_pipe_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	register struct exported_pipeseg *exp;
	register struct wdb_pipeseg	*psp;
	struct wdb_pipeseg		tmp;
	struct rt_pipe_internal		*pipe;
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
	ip->idb_ptr = rt_malloc( sizeof(struct rt_pipe_internal), "rt_pipe_internal");
	pipe = (struct rt_pipe_internal *)ip->idb_ptr;
	pipe->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
	pipe->pipe_count = count;

	/*
	 *  Walk the array of segments in reverse order,
	 *  allocating a linked list of segments in internal format,
	 *  using exactly the same structures as libwdb.
	 */
	RT_LIST_INIT( &pipe->pipe_segs_head );
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
		RT_LIST_APPEND( &pipe->pipe_segs_head, &psp->l );
	}

	return(0);			/* OK */
}

/*
 *			R T _ P I P E _ E X P O R T
 */
int
rt_pipe_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_pipe_internal	*pip;
	struct rt_list		*headp;
	register struct exported_pipeseg *eps;
	register struct wdb_pipeseg	*psp;
	struct wdb_pipeseg		tmp;
	int		count;
	int		ngran;
	int		nbytes;
	union record	*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_PIPE )  return(-1);
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	headp = &pip->pipe_segs_head;

	/* Count number of segments, verify that last seg is an END seg */
	count = 0;
	for( RT_LIST_FOR( psp, wdb_pipeseg, headp ) )  {
		count++;
		switch( psp->ps_type )  {
		case WDB_PIPESEG_TYPE_END:
			if( RT_LIST_NEXT_NOT_HEAD( psp, headp ) )
				return(-1);	/* Inconsistency in list */
			break;
		case WDB_PIPESEG_TYPE_LINEAR:
		case WDB_PIPESEG_TYPE_BEND:
			if( RT_LIST_NEXT_IS_HEAD( psp, headp ) )
				return(-2);	/* List ends w/o TYPE_END */
			break;
		default:
			return(-3);		/* unknown segment type */
		}
	}
	if( count <= 1 )
		return(-4);			/* Not enough for 1 pipe! */

	/* Determine how many whole granules will be required */
	nbytes = sizeof(struct pipe_wire_rec) +
		(count-1) * sizeof(struct exported_pipeseg);
	ngran = (nbytes + sizeof(union record) - 1) / sizeof(union record);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = ngran * sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "pipe external");
	rec = (union record *)ep->ext_buf;

	rec->pw.pw_id = DBID_PIPE;
	(void)rt_plong( rec->pw.pw_count, ngran-1 );	/* # EXTRA grans */

	/* Convert the pipe segments to external form */
	eps = &rec->pw.pw_data[0];
	for( RT_LIST_FOR( psp, wdb_pipeseg, headp ), eps++ )  {
		/* Avoid need for htonl() here */
		eps->eps_type[0] = (char)psp->ps_type;
		/* Convert from user units to mm */
		VSCALE( tmp.ps_start, psp->ps_start, local2mm );
		VSCALE( tmp.ps_bendcenter, psp->ps_bendcenter, local2mm );
		tmp.ps_id = psp->ps_id * local2mm;
		tmp.ps_od = psp->ps_od * local2mm;
		htond( eps->eps_start, tmp.ps_start, 3 );
		htond( eps->eps_bendcenter, tmp.ps_bendcenter, 3 );
		htond( eps->eps_id, &tmp.ps_id, 1 );
		htond( eps->eps_od, &tmp.ps_od, 1 );
	}

	return(0);
}

/*
 *			R T _ P I P E _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_pipe_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_pipe_internal	*pip;
	register struct wdb_pipeseg	*psp;
	char	buf[256];
	int	segno = 0;

	RT_CK_DB_INTERNAL(ip);
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	sprintf(buf, "pipe with %d segments\n", pip->pipe_count );
	rt_vls_strcat( str, buf );

	if( !verbose )  return(0);

	for( RT_LIST_FOR( psp, wdb_pipeseg, &pip->pipe_segs_head ) )  {
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
		sprintf(buf, "  od=%g", psp->ps_od * mm2local );
		rt_vls_strcat( str, buf );
		if( psp->ps_id > 0 )  {
			sprintf(buf, ", id  = %g", psp->ps_id * mm2local );
			rt_vls_strcat( str, buf );
		}
		rt_vls_strcat( str, "\n" );

		sprintf(buf, "\t  start=(%g, %g, %g)\n",
			psp->ps_start[X] * mm2local,
			psp->ps_start[Y] * mm2local,
			psp->ps_start[Z] * mm2local );
		rt_vls_strcat( str, buf );

		if( psp->ps_type == WDB_PIPESEG_TYPE_BEND )  {
			sprintf(buf, "\t  bendcenter=(%g, %g, %g)\n",
				psp->ps_bendcenter[X] * mm2local,
				psp->ps_bendcenter[Y] * mm2local,
				psp->ps_bendcenter[Z] * mm2local );
			rt_vls_strcat( str, buf );
		}
	}
	return(0);
}

/*
 *			R T _ P I P E _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_pipe_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_pipe_internal	*pipe;
	register struct wdb_pipeseg	*psp;

	RT_CK_DB_INTERNAL(ip);
	pipe = (struct rt_pipe_internal*)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pipe);

	while( RT_LIST_WHILE( psp, wdb_pipeseg, &pipe->pipe_segs_head ) )  {
		RT_LIST_DEQUEUE( &(psp->l) );
		rt_free( (char *)psp, "wdb_pipeseg" );
	}
	rt_free( ip->idb_ptr, "pipe ifree" );
	ip->idb_ptr = GENPTR_NULL;
}
