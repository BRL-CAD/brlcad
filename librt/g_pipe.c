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
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"
#include "./debug.h"
#include "./complex.h"
#include "./polyno.h"

union pipe_specific {
	struct pipe_id
	{
		struct rt_list l;
		int	pipe_type;
	} id;
	struct lin_pipe
	{
		struct rt_list l;
		int	pipe_type;
		vect_t	pipe_V;			/* start point for pipe section */
		vect_t	pipe_H;			/* unit vector in direction of pipe section */
		fastf_t pipe_ribase, pipe_ritop;	/* base and top inner radii */
		fastf_t pipe_ribase_sq, pipe_ritop_sq;	/* inner radii squared */
		fastf_t pipe_ridiff_sq, pipe_ridiff;	/* difference between top and base inner radii */
		fastf_t pipe_rodiff_sq, pipe_rodiff;	/* difference between top and base outer radii */
		fastf_t pipe_robase, pipe_rotop;	/* base and top outer radii */
		fastf_t pipe_robase_sq, pipe_rotop_sq;	/* outer radii squared */
		fastf_t	pipe_len;			/* length of pipe segment */
		mat_t	pipe_SoR;	/* Scale and rotate */
		mat_t	pipe_invRoS;	/* inverse rotation and scale */
	} lin;
	struct bend_pipe
	{
		struct rt_list l;
		int	pipe_type;
		fastf_t	bend_radius;		/* distance from bend_v to center of pipe */
		fastf_t	bend_or;		/* outer radius */
		fastf_t	bend_ir;		/* inner radius */
		mat_t	bend_invR;		/* inverse rotation matrix */
		mat_t	bend_SoR;		/* Scale and rotate */
		point_t	bend_V;			/* Center of bend */
		point_t	bend_start;		/* Start of bend */
		point_t	bend_end;		/* End of bend */
		fastf_t	bend_alpha_i;		/* ratio of inner radius to bend radius */
		fastf_t	bend_alpha_o;		/* ratio of outer radius to bend radius */
		fastf_t	bend_angle;		/* Angle that bend goes through */
		vect_t	bend_ra;		/* unit vector in plane of bend (points toward start from bend_V) */
		vect_t	bend_rb;		/* unit vector in plane of bend (normal to bend_ra) */
		vect_t	bend_N;			/* unit vector normal to plane of bend */
		fastf_t	bend_R_SQ;		/* bounding sphere radius squared */
	} bend;
};

struct hit_list
{
	struct rt_list	l;
	struct hit	*hitp;
};

#define PIPE_MM(_v)       VMINMAX( stp->st_min, stp->st_max, _v );

#define	ARC_SEGS	16	/* number of segments used to plot a circle */

#define	PIPE_LINEAR_OUTER_BODY	1
#define	PIPE_LINEAR_INNER_BODY	2
#define	PIPE_LINEAR_TOP		3
#define	PIPE_LINEAR_BASE	4
#define PIPE_BEND_OUTER_BODY	5
#define PIPE_BEND_INNER_BODY	6
#define	PIPE_BEND_BASE		7
#define PIPE_BEND_TOP		8

RT_EXTERN( void rt_pipe_ifree, (struct rt_db_internal *ip) );

int
rt_bend_pipe_prep( stp, pipe_seg, head )
struct soltab		*stp;
struct wdb_pipeseg	*pipe_seg;
union pipe_specific	*head;
{
	register union pipe_specific *pipe;
	register struct wdb_pipeseg *next_seg;
	LOCAL vect_t	to_start,to_end;
	LOCAL mat_t	R;
	LOCAL point_t	work;
	LOCAL vect_t	tmp_vec;
	LOCAL fastf_t	f;
	LOCAL point_t	tmp_pt_min;
	LOCAL point_t	tmp_pt_max;

	if( pipe_seg->ps_type != WDB_PIPESEG_TYPE_BEND )
	{
		rt_log( "rt_bend_pipe_prep called for non-bend segment (type=%d)\n", pipe_seg->ps_type );
		return( 1 );
	}

	pipe = (union pipe_specific *)rt_malloc( sizeof( union pipe_specific ), "rt_bend_pipe_prep:pipe" )	 ;
	RT_LIST_INSERT( &head->id.l, &pipe->id.l );

	next_seg = RT_LIST_PNEXT( wdb_pipeseg, pipe_seg );

	pipe->id.pipe_type = WDB_PIPESEG_TYPE_BEND;
	pipe->bend.bend_or = pipe_seg->ps_od * 0.5;
	pipe->bend.bend_ir = pipe_seg->ps_id * 0.5;

	VMOVE( pipe->bend.bend_start, pipe_seg->ps_start );
	VMOVE( pipe->bend.bend_end, next_seg->ps_start );
	VMOVE( pipe->bend.bend_V, pipe_seg->ps_bendcenter );
	VSUB2( to_start, pipe_seg->ps_start, pipe_seg->ps_bendcenter );
	pipe->bend.bend_radius = MAGNITUDE( to_start );
	VSUB2( to_end, next_seg->ps_start, pipe_seg->ps_bendcenter );
	VSCALE( pipe->bend.bend_ra, to_start, 1.0/pipe->bend.bend_radius );
	VCROSS( pipe->bend.bend_N, to_start, to_end );
	VUNITIZE( pipe->bend.bend_N );
	VCROSS( pipe->bend.bend_rb, pipe->bend.bend_N, pipe->bend.bend_ra );

	pipe->bend.bend_angle = atan2( VDOT( to_end, pipe->bend.bend_rb ), VDOT( to_end, pipe->bend.bend_ra ) );

	/* angle goes from 0.0 at start to some angle less than PI */
	if( pipe->bend.bend_angle < 0.0 )
		pipe->bend.bend_angle += 2.0 * M_PI;

	if( pipe->bend.bend_angle >= M_PI )
	{
		rt_log( "Error: rt_pipe_prep: Bend section bends through more than 180 degrees\n" );
		return( 1 );
	}

	pipe->bend.bend_alpha_i = pipe->bend.bend_ir/pipe->bend.bend_radius;
	pipe->bend.bend_alpha_o = pipe->bend.bend_or/pipe->bend.bend_radius;

	pipe->bend.bend_R_SQ = (pipe->bend.bend_radius + pipe->bend.bend_or) *
				(pipe->bend.bend_radius + pipe->bend.bend_or);

	mat_idn( R );
	VMOVE( &R[0], pipe->bend.bend_ra );
	VMOVE( &R[4], pipe->bend.bend_rb );
	VMOVE( &R[8], pipe->bend.bend_N );
	mat_inv( pipe->bend.bend_invR, R );
	mat_copy( pipe->bend.bend_SoR, R );
	pipe->bend.bend_SoR[15] *= pipe->bend.bend_radius;

	/* bounding box for entire torus */
	/* X */
	VSET( tmp_vec, 1.0, 0.0, 0.0 );
	VCROSS( work, pipe->bend.bend_N, tmp_vec );
	f = pipe->bend.bend_or + pipe->bend.bend_radius * MAGNITUDE(work);
	tmp_pt_min[X] = pipe->bend.bend_V[X] - f;
	tmp_pt_max[X] = pipe->bend.bend_V[X] + f;

	/* Y */
	VSET( tmp_vec, 0.0, 1.0, 0.0 );
	VCROSS( work, pipe->bend.bend_N, tmp_vec );
	f = pipe->bend.bend_or + pipe->bend.bend_radius * MAGNITUDE(work);
	tmp_pt_min[Y] = pipe->bend.bend_V[Y] - f;
	tmp_pt_max[Y] = pipe->bend.bend_V[Y] + f;

	/* Z */
	VSET( tmp_vec, 0.0, 0.0, 1.0 );
	VCROSS( work, pipe->bend.bend_N, tmp_vec );
	f = pipe->bend.bend_or + pipe->bend.bend_radius * MAGNITUDE(work);
	tmp_pt_min[Z] = pipe->bend.bend_V[Z] - f;
	tmp_pt_max[Z] = pipe->bend.bend_V[Z] + f;

	PIPE_MM( tmp_pt_min );
	PIPE_MM( tmp_pt_max );

	return( 0 );

}

void
rt_linear_pipe_prep( stp, pipe_seg, head )
struct soltab		*stp;
struct wdb_pipeseg	*pipe_seg;
union pipe_specific	*head;
{
	fastf_t dx,dy,dz,f;
	register union pipe_specific *pipe;
	register struct wdb_pipeseg *next_seg;
	LOCAL mat_t	R;
	LOCAL mat_t	Rinv;
	LOCAL mat_t	S;
	LOCAL point_t work;
	LOCAL point_t top;
	LOCAL vect_t seg_ht;
	LOCAL vect_t v1,v2;

	pipe = (union pipe_specific *)rt_malloc( sizeof( union pipe_specific ), "rt_bend_pipe_prep:pipe" );
	RT_LIST_INSERT( &head->id.l, &pipe->id.l );

	next_seg = RT_LIST_PNEXT( wdb_pipeseg, pipe_seg );

	VMOVE( pipe->lin.pipe_V, pipe_seg->ps_start );

	VSUB2( seg_ht, next_seg->ps_start, pipe_seg->ps_start );
	pipe->lin.pipe_ribase = pipe_seg->ps_id/2.0;
	pipe->lin.pipe_ribase_sq = pipe->lin.pipe_ribase * pipe->lin.pipe_ribase;
	pipe->lin.pipe_ritop = next_seg->ps_id/2.0;
	pipe->lin.pipe_ritop_sq = pipe->lin.pipe_ritop * pipe->lin.pipe_ritop;
	pipe->lin.pipe_robase = pipe_seg->ps_od/2.0;
	pipe->lin.pipe_robase_sq = pipe->lin.pipe_robase * pipe->lin.pipe_robase;
	pipe->lin.pipe_rotop = next_seg->ps_od/2.0;
	pipe->lin.pipe_rotop_sq = pipe->lin.pipe_rotop * pipe->lin.pipe_rotop;
	pipe->lin.pipe_ridiff = pipe->lin.pipe_ritop - pipe->lin.pipe_ribase;
	pipe->lin.pipe_ridiff_sq = pipe->lin.pipe_ridiff * pipe->lin.pipe_ridiff;
	pipe->lin.pipe_rodiff = pipe->lin.pipe_rotop - pipe->lin.pipe_robase;
	pipe->lin.pipe_rodiff_sq = pipe->lin.pipe_rodiff * pipe->lin.pipe_rodiff;
	pipe->lin.pipe_type = pipe_seg->ps_type;

	pipe->lin.pipe_len = MAGNITUDE( seg_ht );
	VSCALE( seg_ht, seg_ht, 1.0/pipe->lin.pipe_len );
	VMOVE( pipe->lin.pipe_H, seg_ht );
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
	S[10] = 1.0/pipe->lin.pipe_len;

	/* Compute SoR and invRoS */
	mat_mul( pipe->lin.pipe_SoR, S, R );
	mat_mul( pipe->lin.pipe_invRoS, Rinv, S );



	VJOIN2( work, pipe_seg->ps_start, pipe_seg->ps_od, v1, pipe_seg->ps_od, v2 );
	PIPE_MM( work )
	VJOIN2( work, pipe_seg->ps_start, -pipe_seg->ps_od, v1, pipe_seg->ps_od, v2 );
	PIPE_MM( work )
	VJOIN2( work, pipe_seg->ps_start, pipe_seg->ps_od, v1, -pipe_seg->ps_od, v2 );
	PIPE_MM( work )
	VJOIN2( work, pipe_seg->ps_start, -pipe_seg->ps_od, v1, -pipe_seg->ps_od, v2 );
	PIPE_MM( work )

	if( next_seg->ps_type == WDB_PIPESEG_TYPE_END )
	{
		VJOIN1( top, pipe_seg->ps_start, pipe->lin.pipe_len, seg_ht );
		VJOIN2( work, top, pipe_seg->ps_od, v1, pipe_seg->ps_od, v2 );
		PIPE_MM( work )
		VJOIN2( work, top, -pipe_seg->ps_od, v1, pipe_seg->ps_od, v2 );
		PIPE_MM( work )
		VJOIN2( work, top, pipe_seg->ps_od, v1, -pipe_seg->ps_od, v2 );
		PIPE_MM( work )
		VJOIN2( work, top, -pipe_seg->ps_od, v1, -pipe_seg->ps_od, v2 );
		PIPE_MM( work )
	}
}

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
 *  	A union pipe_specific is created, and it's address is stored in
 *  	stp->st_specific for use by pipe_shot().
 */
int
rt_pipe_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	register union pipe_specific *head;
	struct rt_pipe_internal	*pip;
	struct wdb_pipeseg *pipe_seg;
	LOCAL fastf_t dx;
	LOCAL fastf_t dy;
	LOCAL fastf_t dz;
	LOCAL fastf_t f;

	RT_CK_DB_INTERNAL( ip );
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	head = (union pipe_specific *)rt_malloc( sizeof( union pipe_specific ), "rt_pipe_prep:head" );
	stp->st_specific = (genptr_t)head;
	RT_LIST_INIT( &head->id.l );

	/* Compute bounding sphere and RPP */
	for( RT_LIST_FOR( pipe_seg, wdb_pipeseg, &(pip->pipe_segs_head) ) )
	{
		switch( pipe_seg->ps_type )
		{
			case WDB_PIPESEG_TYPE_END:
				break;
			case WDB_PIPESEG_TYPE_LINEAR:
				rt_linear_pipe_prep( stp, pipe_seg, head );
				break;
			case WDB_PIPESEG_TYPE_BEND:
				rt_bend_pipe_prep( stp, pipe_seg, head );
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

	return( 0 );
}

/*
 *			R T _ P I P E _ P R I N T
 */
void
rt_pipe_print( stp )
register CONST struct soltab *stp;
{
/*	register union pipe_specific *pipe =
		(union pipe_specific *)stp->st_specific; */
}

void
rt_pipeseg_print( pipe, mm2local )
struct wdb_pipeseg *pipe;
double mm2local;
{
	point_t p1,p2;
	fastf_t radius;
	struct wdb_pipeseg *next;

	next = RT_LIST_NEXT( wdb_pipeseg, &pipe->l );
	switch( pipe->ps_type )
	{
		case WDB_PIPESEG_TYPE_LINEAR:
			rt_log( "Linear Pipe Segment:\n" );
			VSCALE( p1, pipe->ps_start, mm2local );
			VSCALE( p2, next->ps_start, mm2local );
			rt_log( "\tfrom (%g %g %g) to (%g %g %g)\n" ,
				V3ARGS( p1 ), V3ARGS( p2 ) );
			if( pipe->ps_id > 0.0 )
				rt_log( "\tat start: od=%g, id=%g\n",
					pipe->ps_od*mm2local,
					pipe->ps_id*mm2local );
			else
				rt_log( "\tat start: od=%g\n",
					pipe->ps_od*mm2local );
			if( next->ps_id > 0.0 )
				rt_log( "\tat end: od=%g, id=%g\n",
					next->ps_od*mm2local,
					next->ps_id*mm2local );
			else
				rt_log( "\tat end: od=%g\n",
					next->ps_od*mm2local );
			break;
		case WDB_PIPESEG_TYPE_BEND:
			rt_log( "Bend Pipe Segment:\n" );
			VSCALE( p1, pipe->ps_start, mm2local );
			VSCALE( p2, next->ps_start, mm2local );
			rt_log( "\tfrom (%g %g %g) to (%g %g %g)\n" ,
				V3ARGS( p1 ), V3ARGS( p2 ) );
			VSUB2( p2, pipe->ps_start, pipe->ps_bendcenter );
			radius = MAGNITUDE( p2 );
			VSCALE( p1, pipe->ps_bendcenter, mm2local );
			rt_log( "\tBend center at (%g %g %g), bend radius = %g\n",
				V3ARGS( p1 ), radius*mm2local );
			if( pipe->ps_id > 0.0 )
				rt_log( "\tod=%g, id=%g\n",
					pipe->ps_od*mm2local,
					pipe->ps_id*mm2local );
			else
				rt_log( "\tod=%g\n", pipe->ps_od*mm2local );
			break;
		case WDB_PIPESEG_TYPE_END:
			rt_log( "End Pipe Segment\n" );
			break;
	}

}

void
vls_pipeseg( vp, seg_no, ip, mm2local )
struct rt_vls *vp;
int seg_no;
CONST struct rt_db_internal *ip;
double mm2local;
{
	point_t p1,p2;
	fastf_t radius;
	struct rt_pipe_internal *pint;
	struct wdb_pipeseg *pipe;
	struct wdb_pipeseg *next;
	int seg_count=0;
	char buf[256];

	pint = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC( pint );

	pipe = RT_LIST_FIRST( wdb_pipeseg, &pint->pipe_segs_head );
	while( ++seg_count != seg_no && RT_LIST_NOT_HEAD( &pipe->l, &pint->pipe_segs_head ) )
		pipe = RT_LIST_NEXT( wdb_pipeseg, &pipe->l );

	next = RT_LIST_NEXT( wdb_pipeseg, &pipe->l );
	switch( pipe->ps_type )
	{
		case WDB_PIPESEG_TYPE_LINEAR:
			sprintf( buf, "Linear Pipe Segment:\n" );
			rt_vls_strcat( vp, buf );
			VSCALE( p1, pipe->ps_start, mm2local );
			VSCALE( p2, next->ps_start, mm2local );
			sprintf( buf, "\tfrom (%g %g %g) to (%g %g %g)\n" ,
				V3ARGS( p1 ), V3ARGS( p2 ) );
			rt_vls_strcat( vp, buf );
			if( pipe->ps_id > 0.0 )
				sprintf( buf, "\tat start: od=%g, id=%g\n",
					pipe->ps_od*mm2local,
					pipe->ps_id*mm2local );
			else
				sprintf( buf, "\tat start: od=%g\n",
					pipe->ps_od*mm2local );
			rt_vls_strcat( vp, buf );
			if( next->ps_id > 0.0 )
				sprintf( buf, "\tat end: od=%g, id=%g\n",
					next->ps_od*mm2local,
					next->ps_id*mm2local );
			else
				sprintf( buf, "\tat end: od=%g\n",
					next->ps_od*mm2local );
			rt_vls_strcat( vp, buf );
			break;
		case WDB_PIPESEG_TYPE_BEND:
			sprintf( buf, "Bend Pipe Segment:\n" );
			rt_vls_strcat( vp, buf );
			VSCALE( p1, pipe->ps_start, mm2local );
			VSCALE( p2, next->ps_start, mm2local );
			sprintf( buf, "\tfrom (%g %g %g) to (%g %g %g)\n" ,
				V3ARGS( p1 ), V3ARGS( p2 ) );
			rt_vls_strcat( vp, buf );
			VSUB2( p2, pipe->ps_start, pipe->ps_bendcenter );
			radius = MAGNITUDE( p2 );
			VSCALE( p1, pipe->ps_bendcenter, mm2local );
			sprintf( buf, "\tBend center at (%g %g %g), bend radius = %g\n",
				V3ARGS( p1 ), radius*mm2local );
			rt_vls_strcat( vp, buf );
			if( pipe->ps_id > 0.0 )
				sprintf( buf, "\tod=%g, id=%g\n",
					pipe->ps_od*mm2local,
					pipe->ps_id*mm2local );
			else
				sprintf( buf, "\tod=%g\n", pipe->ps_od*mm2local );
			rt_vls_strcat( vp, buf );
			break;
		case WDB_PIPESEG_TYPE_END:
			sprintf( buf, "End Pipe Segment\n" );
			rt_vls_strcat( vp, buf );
			break;
	}

}

void
bend_pipe_shot( stp, rp, ap, seghead, pipe, hit_headp, hit_count, seg_no )
struct soltab           *stp;
register struct xray    *rp;
struct application      *ap;
struct seg              *seghead;
union pipe_specific	*pipe;
struct hit_list		*hit_headp;
int			*hit_count;
int			seg_no;
{
	register struct seg *segp;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL vect_t	work;		/* temporary vector */
	LOCAL poly	C;		/* The final equation */
	LOCAL complex	val[MAXP];	/* The complex roots */
	LOCAL double	k[4];		/* The real roots */
	register int	i;
	LOCAL int	j;
	LOCAL int	root_count=0;
	LOCAL poly	A, Asqr;
	LOCAL poly	X2_Y2;		/* X**2 + Y**2 */
	LOCAL vect_t	cor_pprime;	/* new ray origin */
	LOCAL fastf_t	cor_proj;
	LOCAL fastf_t	dist_to_pca;
	LOCAL vect_t	to_start;

	VSUB2( to_start, rp->r_pt, pipe->bend.bend_V );
	dist_to_pca = VDOT( to_start, rp->r_dir );
	if( (MAGSQ( to_start ) - dist_to_pca*dist_to_pca) > pipe->bend.bend_R_SQ )
	{
		*hit_count = 0;
		return;			/* Miss */
	}

	/* Convert vector into the space of the unit torus */
	MAT4X3VEC( dprime, pipe->bend.bend_SoR, rp->r_dir );
	VUNITIZE( dprime );

	VSUB2( work, rp->r_pt, pipe->bend.bend_V );
	MAT4X3VEC( pprime, pipe->bend.bend_SoR, work );

	/* normalize distance from torus.  substitute
	 * corrected pprime which contains a translation along ray
	 * direction to closest approach to vertex of torus.
	 * Translating ray origin along direction of ray to closest pt. to
	 * origin of solid's coordinate system, new ray origin is
	 * 'cor_pprime'.
	 */
	cor_proj = VDOT( pprime, dprime );
	VSCALE( cor_pprime, dprime, cor_proj );
	VSUB2( cor_pprime, pprime, cor_pprime );

	/*
	 *  Given a line and a ratio, alpha, finds the equation of the
	 *  unit torus in terms of the variable 't'.
	 *
	 *  The equation for the torus is:
	 *
	 * [ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*( X**2 + Y**2 ) = 0
	 *
	 *  First, find X, Y, and Z in terms of 't' for this line, then
	 *  substitute them into the equation above.
	 *
	 *  	Wx = Dx*t + Px
	 *
	 *  	Wx**2 = Dx**2 * t**2  +  2 * Dx * Px  +  Px**2
	 *  		[0]                [1]           [2]    dgr=2
	 */
	X2_Y2.dgr = 2;
	X2_Y2.cf[0] = dprime[X] * dprime[X] + dprime[Y] * dprime[Y];
	X2_Y2.cf[1] = 2.0 * (dprime[X] * cor_pprime[X] +
			     dprime[Y] * cor_pprime[Y]);
	X2_Y2.cf[2] = cor_pprime[X] * cor_pprime[X] +
		      cor_pprime[Y] * cor_pprime[Y];

	/* A = X2_Y2 + Z2 */
	A.dgr = 2;
	A.cf[0] = X2_Y2.cf[0] + dprime[Z] * dprime[Z];
	A.cf[1] = X2_Y2.cf[1] + 2.0 * dprime[Z] * cor_pprime[Z];
	A.cf[2] = X2_Y2.cf[2] + cor_pprime[Z] * cor_pprime[Z] +
		  1.0 - pipe->bend.bend_alpha_o * pipe->bend.bend_alpha_o;

	/* Inline expansion of (void) rt_poly_mul( &A, &A, &Asqr ) */
	/* Both polys have degree two */
	Asqr.dgr = 4;
	Asqr.cf[0] = A.cf[0] * A.cf[0];
	Asqr.cf[1] = A.cf[0] * A.cf[1] + A.cf[1] * A.cf[0];
	Asqr.cf[2] = A.cf[0] * A.cf[2] + A.cf[1] * A.cf[1] + A.cf[2] * A.cf[0];
	Asqr.cf[3] = A.cf[1] * A.cf[2] + A.cf[2] * A.cf[1];
	Asqr.cf[4] = A.cf[2] * A.cf[2];

	/* Inline expansion of rt_poly_scale( &X2_Y2, 4.0 ) and
	 * rt_poly_sub( &Asqr, &X2_Y2, &C ).
	 */
	C.dgr   = 4;
	C.cf[0] = Asqr.cf[0];
	C.cf[1] = Asqr.cf[1];
	C.cf[2] = Asqr.cf[2] - X2_Y2.cf[0] * 4.0;
	C.cf[3] = Asqr.cf[3] - X2_Y2.cf[1] * 4.0;
	C.cf[4] = Asqr.cf[4] - X2_Y2.cf[2] * 4.0;

	/*  It is known that the equation is 4th order.  Therefore,
	 *  if the root finder returns other than 4 roots, error.
	 */
	if ( (root_count = rt_poly_roots( &C, val )) != 4 ){
		if( (root_count) != 0 )  {
			rt_log("tor:  rt_poly_roots() 4!=%d\n", root_count);
			rt_pr_roots( stp->st_name, val, root_count );
		}
		return;	/* MISSED */
	}

	/*  Only real roots indicate an intersection in real space.
	 *
	 *  Look at each root returned; if the imaginary part is zero
	 *  or sufficiently close, then use the real part as one value
	 *  of 't' for the intersections
	 */
	for ( j=0, (*hit_count)=0; j < 4; j++ ){
		if( NEAR_ZERO( val[j].im, 0.0001 ) )
		{
			struct hit_list *hitp;
			fastf_t normalized_dist;
			fastf_t dist;
			point_t hit_pt;
			vect_t	to_hit;
			fastf_t	angle;

			normalized_dist = val[j].re - cor_proj;
			dist = normalized_dist * pipe->bend.bend_radius;

			/* check if this hit is within bend angle */
			VJOIN1( hit_pt, rp->r_pt, dist, rp->r_dir );
			VSUB2( to_hit, hit_pt, pipe->bend.bend_V );
			angle = atan2( VDOT( to_hit, pipe->bend.bend_rb ), VDOT( to_hit, pipe->bend.bend_ra ) );
			if( angle < 0.0 )
				angle += 2.0 * M_PI;
			if( angle <= pipe->bend.bend_angle )
			{
				GETSTRUCT( hitp, hit_list );
				GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_dist = dist;
				VJOIN1( hitp->hitp->hit_vpriv, pprime, normalized_dist, dprime );
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_BEND_OUTER_BODY;
				RT_LIST_INSERT( &hit_headp->l, &hitp->l );
				(*hit_count)++;
			}
		}
	}

	if( pipe->bend.bend_alpha_i <= 0.0 )
		return;		/* no inner torus */

	/* Now do inner torus */
	A.cf[2] = X2_Y2.cf[2] + cor_pprime[Z] * cor_pprime[Z] +
		  1.0 - pipe->bend.bend_alpha_i * pipe->bend.bend_alpha_i;

	/* Inline expansion of (void) rt_poly_mul( &A, &A, &Asqr ) */
	/* Both polys have degree two */
	Asqr.dgr = 4;
	Asqr.cf[0] = A.cf[0] * A.cf[0];
	Asqr.cf[1] = A.cf[0] * A.cf[1] + A.cf[1] * A.cf[0];
	Asqr.cf[2] = A.cf[0] * A.cf[2] + A.cf[1] * A.cf[1] + A.cf[2] * A.cf[0];
	Asqr.cf[3] = A.cf[1] * A.cf[2] + A.cf[2] * A.cf[1];
	Asqr.cf[4] = A.cf[2] * A.cf[2];

	/* Inline expansion of rt_poly_scale( &X2_Y2, 4.0 ) and
	 * rt_poly_sub( &Asqr, &X2_Y2, &C ).
	 */
	C.dgr   = 4;
	C.cf[0] = Asqr.cf[0];
	C.cf[1] = Asqr.cf[1];
	C.cf[2] = Asqr.cf[2] - X2_Y2.cf[0] * 4.0;
	C.cf[3] = Asqr.cf[3] - X2_Y2.cf[1] * 4.0;
	C.cf[4] = Asqr.cf[4] - X2_Y2.cf[2] * 4.0;

	/*  It is known that the equation is 4th order.  Therefore,
	 *  if the root finder returns other than 4 roots, error.
	 */
	if ( (root_count = rt_poly_roots( &C, val )) != 4 ){
		if( root_count != 0 )  {
			rt_log("tor:  rt_poly_roots() 4!=%d\n", root_count);
			rt_pr_roots( stp->st_name, val, root_count );
		}
		return;	/* MISSED */
	}

	/*  Only real roots indicate an intersection in real space.
	 *
	 *  Look at each root returned; if the imaginary part is zero
	 *  or sufficiently close, then use the real part as one value
	 *  of 't' for the intersections
	 */
	for ( j=0, root_count=0; j < 4; j++ ){
		if( NEAR_ZERO( val[j].im, 0.0001 ) )
		{
			struct hit_list *hitp;
			fastf_t normalized_dist;
			fastf_t dist;
			point_t hit_pt;
			vect_t	to_hit;
			fastf_t	angle;

			normalized_dist = val[j].re - cor_proj;
			dist = normalized_dist * pipe->bend.bend_radius;

			/* check if this hit is within bend angle */
			VJOIN1( hit_pt, rp->r_pt, dist, rp->r_dir );
			VSUB2( to_hit, hit_pt, pipe->bend.bend_V );
			angle = atan2( VDOT( to_hit, pipe->bend.bend_rb ), VDOT( to_hit, pipe->bend.bend_ra ) );
			if( angle < 0.0 )
				angle += 2.0 * M_PI;
			if( angle <= pipe->bend.bend_angle )
			{
				GETSTRUCT( hitp, hit_list );
				GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_dist = dist;
				VJOIN1( hitp->hitp->hit_vpriv, pprime, normalized_dist, dprime );
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_BEND_INNER_BODY;
				RT_LIST_INSERT( &hit_headp->l, &hitp->l );
				root_count++;
			}
		}
	}

	*hit_count += root_count;

	return;

}

void
linear_pipe_shot( stp, rp, ap, seghead, pipe, hit_headp, hit_count, seg_no )
struct soltab           *stp;
register struct xray    *rp;
struct application      *ap;
struct seg              *seghead;
struct lin_pipe		*pipe;
struct hit_list		*hit_headp;
int			*hit_count;
int			seg_no;
{
	LOCAL struct hit_list	*hitp;
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

	*hit_count = 0;

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
			GETSTRUCT( hitp, hit_list );
			GETSTRUCT( hitp->hitp, hit );
			hitp->hitp->hit_dist = t_tmp;
			hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_OUTER_BODY;
			VMOVE( hitp->hitp->hit_vpriv, hit_pt );
			hitp->hitp->hit_vpriv[Z] = (-pipe->pipe_robase - hit_pt[Z] * pipe->pipe_rodiff) *
					pipe->pipe_rodiff;
			(*hit_count)++;
			RT_LIST_INSERT( &hit_headp->l, &hitp->l );
		}

		t_tmp = (-b + sqrt_descrim)/(2.0*a);
		VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
		if( hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0 )
		{
			GETSTRUCT( hitp, hit_list );
			GETSTRUCT( hitp->hitp, hit );
			hitp->hitp->hit_dist = t_tmp;
			hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_OUTER_BODY;
			VMOVE( hitp->hitp->hit_vpriv, hit_pt );
			hitp->hitp->hit_vpriv[Z] = (-pipe->pipe_robase - hit_pt[Z] * pipe->pipe_rodiff) *
					pipe->pipe_rodiff;
			(*hit_count)++;
			RT_LIST_INSERT( &hit_headp->l, &hitp->l );
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
				GETSTRUCT( hitp, hit_list );
				GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_INNER_BODY;
				VMOVE( hitp->hitp->hit_vpriv, hit_pt );
				hitp->hitp->hit_vpriv[Z] = (-pipe->pipe_ribase - hit_pt[Z] * pipe->pipe_ridiff) *
					pipe->pipe_ridiff;
				(*hit_count)++;
				RT_LIST_INSERT( &hit_headp->l, &hitp->l );
			}

			t_tmp = (-b + sqrt_descrim)/(2.0*a);
			VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
			if( hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0 )
			{
				GETSTRUCT( hitp, hit_list );
				GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_INNER_BODY;
				VMOVE( hitp->hitp->hit_vpriv, hit_pt );
				hitp->hitp->hit_vpriv[Z] = (-pipe->pipe_ribase - hit_pt[Z] * pipe->pipe_ridiff) *
					pipe->pipe_ridiff;
				(*hit_count)++;
				RT_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}

}

void
pipe_start_shot( stp, rp, ap, seghead, pipe, hit_headp, hit_count, seg_no )
struct soltab           *stp;
register struct xray    *rp;
struct application      *ap;
struct seg              *seghead;
struct pipe_id		*pipe;
struct hit_list		*hit_headp;
int			*hit_count;
int			seg_no;
{
	point_t work_pt;
	point_t ray_start;
	vect_t ray_dir;
	point_t hit_pt;
	fastf_t t_tmp;
	fastf_t radius_sq;
	struct hit_list *hitp;

	*hit_count = 0;

	if( pipe->pipe_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		struct lin_pipe *lin=(struct lin_pipe *)(&pipe->l);
		fastf_t dist_to_plane;
		fastf_t norm_dist;
		fastf_t slant_factor;

		dist_to_plane = VDOT( lin->pipe_H, lin->pipe_V );
		norm_dist = dist_to_plane - VDOT( lin->pipe_H, rp->r_pt );
		slant_factor = VDOT( lin->pipe_H, rp->r_dir );
		if( !NEAR_ZERO( slant_factor, SMALL_FASTF ) )
		{
			vect_t to_center;

			t_tmp = norm_dist/slant_factor;
			VJOIN1( hit_pt, rp->r_pt, t_tmp, rp->r_dir );
			VSUB2( to_center, lin->pipe_V, hit_pt );
			radius_sq = MAGSQ( to_center );
			if( radius_sq <= lin->pipe_robase_sq && radius_sq >= lin->pipe_ribase_sq )
			{
				GETSTRUCT( hitp, hit_list );
				GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_BASE;
				(*hit_count)++;
				RT_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}
	else if( pipe->pipe_type == WDB_PIPESEG_TYPE_BEND )
	{
		struct bend_pipe *bend=(struct bend_pipe *)(&pipe->l);
		fastf_t dist_to_plane;
		fastf_t norm_dist;
		fastf_t slant_factor;

		dist_to_plane = VDOT( bend->bend_rb, bend->bend_start );
		norm_dist = dist_to_plane - VDOT( bend->bend_rb, rp->r_pt );
		slant_factor = VDOT( bend->bend_rb, rp->r_dir );

		if( !NEAR_ZERO( slant_factor, SMALL_FASTF ) )
		{
			vect_t to_center;

			t_tmp = norm_dist/slant_factor;
			VJOIN1( hit_pt, rp->r_pt, t_tmp, rp->r_dir );
			VSUB2( to_center, bend->bend_start, hit_pt );
			radius_sq = MAGSQ( to_center );
			if( radius_sq <= bend->bend_or*bend->bend_or && radius_sq >= bend->bend_ir*bend->bend_ir )
			{
				GETSTRUCT( hitp, hit_list );
				GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_BEND_BASE;
				(*hit_count)++;
				RT_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}
}
void
pipe_end_shot( stp, rp, ap, seghead, pipe, hit_headp, hit_count, seg_no )
struct soltab           *stp;
register struct xray    *rp;
struct application      *ap;
struct seg              *seghead;
struct pipe_id		*pipe;
struct hit_list		*hit_headp;
int			*hit_count;
int			seg_no;
{
	point_t work_pt;
	point_t ray_start;
	vect_t ray_dir;
	point_t hit_pt;
	fastf_t t_tmp;
	fastf_t radius_sq;
	struct hit_list *hitp;

	*hit_count = 0;

	if( pipe->pipe_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		struct lin_pipe *lin=(struct lin_pipe *)(&pipe->l);
		point_t top;
		fastf_t dist_to_plane;
		fastf_t norm_dist;
		fastf_t slant_factor;

		VJOIN1( top, lin->pipe_V, lin->pipe_len, lin->pipe_H );
		dist_to_plane = VDOT( lin->pipe_H, top );
		norm_dist = dist_to_plane - VDOT( lin->pipe_H, rp->r_pt );
		slant_factor = VDOT( lin->pipe_H, rp->r_dir );
		if( !NEAR_ZERO( slant_factor, SMALL_FASTF ) )
		{
			vect_t to_center;

			t_tmp = norm_dist/slant_factor;
			VJOIN1( hit_pt, rp->r_pt, t_tmp, rp->r_dir );
			VSUB2( to_center, top, hit_pt );
			radius_sq = MAGSQ( to_center );
			if( radius_sq <= lin->pipe_rotop_sq && radius_sq >= lin->pipe_ritop_sq )
			{
				GETSTRUCT( hitp, hit_list );
				GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_TOP;
				(*hit_count)++;
				RT_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}
	else if( pipe->pipe_type == WDB_PIPESEG_TYPE_BEND )
	{
		struct bend_pipe *bend=(struct bend_pipe *)(&pipe->l);
		vect_t to_end;
		vect_t plane_norm;
		fastf_t dist_to_plane;
		fastf_t norm_dist;
		fastf_t slant_factor;

		VSUB2( to_end, bend->bend_end, bend->bend_V );
		VCROSS( plane_norm, to_end, bend->bend_N );
		VUNITIZE( plane_norm );

		dist_to_plane = VDOT( plane_norm, bend->bend_end );
		norm_dist = dist_to_plane - VDOT( plane_norm, rp->r_pt );
		slant_factor = VDOT( plane_norm, rp->r_dir );

		if( !NEAR_ZERO( slant_factor, SMALL_FASTF ) )
		{
			vect_t to_center;

			t_tmp = norm_dist/slant_factor;
			VJOIN1( hit_pt, rp->r_pt, t_tmp, rp->r_dir );
			VSUB2( to_center, bend->bend_end, hit_pt );
			radius_sq = MAGSQ( to_center );
			if( radius_sq <= bend->bend_or*bend->bend_or && radius_sq >= bend->bend_ir*bend->bend_ir )
			{
				GETSTRUCT( hitp, hit_list );
				GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_BEND_TOP;
				(*hit_count)++;
				RT_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}
}

HIDDEN void
rt_pipe_hitsort( h, nh )
struct hit_list *h;
int *nh;
{
	register int i, j;
	struct hit_list *hitp;
	LOCAL struct hit temp;

	hitp = RT_LIST_FIRST( hit_list, &h->l );
	while( RT_LIST_NEXT_NOT_HEAD( &hitp->l, &h->l ) )
	{
		struct hit_list *next_hit;
		struct hit_list *prev_hit;

		next_hit = RT_LIST_NEXT( hit_list, &hitp->l );
		if( hitp->hitp->hit_dist > next_hit->hitp->hit_dist )
		{
			struct hit_list *tmp;

			if( hitp == RT_LIST_FIRST( hit_list, &h->l ) )
				prev_hit = (struct hit_list *)NULL;
			else
				prev_hit = RT_LIST_PREV( hit_list, &hitp->l );

			/* move this hit to the end of the list */
			tmp = hitp;
			RT_LIST_DEQUEUE( &tmp->l );
			RT_LIST_INSERT( &h->l, &tmp->l );

			if( prev_hit )
				hitp = prev_hit;
			else
				hitp = RT_LIST_FIRST( hit_list, &h->l );
		}
		else
			hitp = next_hit;
	}

	/* delete duplicate hits */
	hitp = RT_LIST_FIRST( hit_list, &h->l );
	while( RT_LIST_NEXT_NOT_HEAD( &hitp->l, &h->l ) )
	{
		struct hit_list *next_hit;

		next_hit = RT_LIST_NEXT( hit_list, &hitp->l );

		if( NEAR_ZERO( hitp->hitp->hit_dist - next_hit->hitp->hit_dist, 0.00001  ) )
		{
			struct hit_list *tmp;

			tmp = hitp;
			hitp = next_hit;
			RT_LIST_DEQUEUE( &tmp->l );
			rt_free( (char *)tmp->hitp, "rt_pipe_hitsort: tmp->hitp" );
			rt_free( (char *)tmp, "rt_pipe_hitsort: tmp" );
			(*nh)--;
		}
		else
			hitp = next_hit;
	}

	if( *nh == 1 )
	{
		while( RT_LIST_WHILE( hitp, hit_list, &h->l ) )
		{
			RT_LIST_DEQUEUE( &hitp->l );
			rt_free( (char *)hitp->hitp, "pipe_hitsort: hitp->hitp" );
			rt_free( (char *)hitp, "pipe_hitsort: hitp" );
		}
		(*nh) = 0;
	}
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
	register union pipe_specific *head =
		(union pipe_specific *)stp->st_specific;
	register struct pipe_id *pipe;
	register struct seg *segp;
	register struct hit_list hit_head;
	LOCAL struct hit_list *hitp;
	LOCAL int hit_count;
	LOCAL int total_hits=0;
	LOCAL int seg_no=0;
	LOCAL int i;

	RT_LIST_INIT( &hit_head.l );

	pipe_start_shot( stp, rp, ap, seghead, RT_LIST_FIRST( pipe_id, &(head->id.l)),
		&hit_head, &total_hits, 1 );
	for( RT_LIST_FOR( pipe, pipe_id, &(head->id.l) ) )
		seg_no++;
	pipe_end_shot( stp, rp, ap, seghead, RT_LIST_LAST( pipe_id, &(head->id.l)),
		&hit_head, &hit_count, seg_no );
	total_hits += hit_count;

	seg_no = 0;
	for( RT_LIST_FOR( pipe, pipe_id, &(head->id.l) ) )
	{
		seg_no++;

		switch( pipe->pipe_type )
		{
			case WDB_PIPESEG_TYPE_LINEAR:
				linear_pipe_shot( stp, rp, ap, seghead, (struct lin_pipe *)pipe,
					&hit_head, &hit_count, seg_no );
				total_hits += hit_count;
				break;
			case WDB_PIPESEG_TYPE_BEND:
				bend_pipe_shot( stp, rp, ap, seghead, (struct bend_pipe *)pipe,
					&hit_head, &hit_count, seg_no );
				total_hits += hit_count;
				break;
			default:
				rt_log( "rt_pipe_shot: Bad pipe type (%d)\n", pipe->pipe_type );
				rt_bomb( "rt_pipe_shot\n" );
				break;
		}

	}
	if( !total_hits )
		return( 0 );

	rt_pipe_hitsort( &hit_head, &total_hits );

	/* Build segments */
	if( total_hits%2 )
	{
		i = 0;
		rt_log( "rt_pipe_shot: bad number of hits (%d)\n" , total_hits );
		for( RT_LIST_FOR( hitp, hit_list, &hit_head.l ) )
		{
			point_t hit_pt;

			rt_log( "#%d, dist = %g, surfno=%d\n" , ++i, hitp->hitp->hit_dist, hitp->hitp->hit_surfno );
			VJOIN1( hit_pt, rp->r_pt, hitp->hitp->hit_dist,  rp->r_dir );
			rt_log( "\t( %g %g %g )\n" , V3ARGS( hit_pt ) );
		}
		rt_bomb( "rt_pipe_shot\n" );
	}

	hitp = RT_LIST_FIRST( hit_list, &hit_head.l );
	while( RT_LIST_NOT_HEAD( &hitp->l, &hit_head.l ) )
	{
		struct hit_list *next;

		next = RT_LIST_NEXT( hit_list, &hitp->l );

		RT_GET_SEG(segp, ap->a_resource);

		segp->seg_stp = stp;
		segp->seg_in = (*hitp->hitp);
		segp->seg_out = (*next->hitp);

		RT_LIST_INSERT( &(seghead->l), &(segp->l) );

		hitp = RT_LIST_NEXT( hit_list, &next->l );
	}

	/* free the list of hits */
	while( RT_LIST_WHILE( hitp, hit_list, &hit_head.l ) )
	{
		RT_LIST_DEQUEUE( &hitp->l );
		rt_free( (char *)hitp->hitp, "rt_pipe_shot: hitp->hitp" );
		rt_free( (char *)hitp, "rt_pipe_shot: hitp" );
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
	register union pipe_specific *pipe =
		(union pipe_specific *)stp->st_specific;
	LOCAL fastf_t	w;
	LOCAL vect_t	work;
	LOCAL vect_t	work1;
	LOCAL int	segno;
	LOCAL int	i;

	segno = hitp->hit_surfno/10;

	for( i=0 ; i<segno ; i++ )
		pipe = (union pipe_specific *)((&pipe->id.l)->forw);

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	switch( hitp->hit_surfno%10 )
	{
		case PIPE_LINEAR_TOP:
			VMOVE( hitp->hit_normal, pipe->lin.pipe_H );
			break;
		case PIPE_LINEAR_BASE:
			VREVERSE( hitp->hit_normal, pipe->lin.pipe_H );
			break;
		case PIPE_LINEAR_OUTER_BODY:
			MAT4X3VEC( hitp->hit_normal, pipe->lin.pipe_invRoS, hitp->hit_vpriv );
			VUNITIZE( hitp->hit_normal );
			break;
		case PIPE_LINEAR_INNER_BODY:
			MAT4X3VEC( hitp->hit_normal, pipe->lin.pipe_invRoS, hitp->hit_vpriv );
			VUNITIZE( hitp->hit_normal );
			VREVERSE( hitp->hit_normal, hitp->hit_normal );
			break;
		case PIPE_BEND_OUTER_BODY:
			w = hitp->hit_vpriv[X]*hitp->hit_vpriv[X] +
			    hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] +
			    hitp->hit_vpriv[Z]*hitp->hit_vpriv[Z] +
			    1.0 - pipe->bend.bend_alpha_o*pipe->bend.bend_alpha_o;
			VSET( work,
				( w - 2.0 ) * hitp->hit_vpriv[X],
				( w - 2.0 ) * hitp->hit_vpriv[Y],
				  w * hitp->hit_vpriv[Z] );
			VUNITIZE( work );
			MAT3X3VEC( hitp->hit_normal, pipe->bend.bend_invR, work );
			break;
		case PIPE_BEND_INNER_BODY:
			w = hitp->hit_vpriv[X]*hitp->hit_vpriv[X] +
			    hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] +
			    hitp->hit_vpriv[Z]*hitp->hit_vpriv[Z] +
			    1.0 - pipe->bend.bend_alpha_o*pipe->bend.bend_alpha_o;
			VSET( work,
				( w - 2.0 ) * hitp->hit_vpriv[X],
				( w - 2.0 ) * hitp->hit_vpriv[Y],
				  w * hitp->hit_vpriv[Z] );
			VUNITIZE( work );
			MAT3X3VEC( work1, pipe->bend.bend_invR, work );
			VREVERSE( hitp->hit_normal, work1 );
			break;
		case PIPE_BEND_BASE:
			VREVERSE( hitp->hit_normal, pipe->bend.bend_rb );
			break;
		case PIPE_BEND_TOP:
			VSUB2( work, pipe->bend.bend_end, pipe->bend.bend_V );
			VCROSS( hitp->hit_normal, pipe->bend.bend_N, work );
			VUNITIZE( hitp->hit_normal );
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
/*	register union pipe_specific *pipe =
		(union pipe_specific *)stp->st_specific; */

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
/*	register union pipe_specific *pipe =
		(union pipe_specific *)stp->st_specific; */
}

/*
 *		R T _ P I P E _ F R E E
 */
void
rt_pipe_free( stp )
register struct soltab *stp;
{
	register union pipe_specific *pipe =
		(union pipe_specific *)stp->st_specific;

	/* free linked list */
	while( RT_LIST_NON_EMPTY( &pipe->id.l ) )
	{
		register union pipe_specific *pipe_ptr;

		pipe_ptr = (union pipe_specific *)(&pipe->id.l)->forw;
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

/*	D R A W _ P I P E _ A R C
 *
 * v1 and v2 must be unit vectors normal to each other in plane of circle
 * v1 must be in direction from center to start point (unless a full circle is
 * requested). "Start" and "end" are start and endpoints of arc. "Seg_count"
 * is how many straight line segements to use to draw the arc. "Full_circle"
 * is a flag to indicate that a complete circle is desired.
 */

static void
draw_pipe_arc( vhead, radius, center, v1, v2, start, end, seg_count, full_circle )
struct rt_list		*vhead;
fastf_t			radius;
point_t			center;
vect_t			v1,v2;
point_t			start,end;
int			seg_count;
int			full_circle;
{
	fastf_t		arc_angle;
	fastf_t		delta_ang;
	fastf_t		cos_del, sin_del;
	fastf_t		x, y, xnew, ynew;
	vect_t		to_end;
	point_t		pt;
	int		i;

	if( !full_circle )
	{
		VSUB2( to_end, end, center );
		arc_angle = atan2( VDOT( to_end, v2 ), VDOT( to_end, v1 ) );
		delta_ang = arc_angle/seg_count;
	}
	else
		delta_ang = 2.0*M_PI/seg_count;

	cos_del = cos( delta_ang );
	sin_del = sin( delta_ang );

	x = radius;
	y = 0.0;
	VJOIN2( pt, center, x, v1, y, v2 );
	RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_MOVE );
	for( i=0 ; i<seg_count ; i++ )
	{
		vect_t radial_dir;
		fastf_t local_radius;

		xnew = x*cos_del - y*sin_del;
		ynew = x*sin_del + y*cos_del;
		VJOIN2( pt, center, xnew, v1, ynew, v2 );
		RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );
		x = xnew;
		y = ynew;
	}
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
		draw_pipe_arc( vhead, r1, base_vertex, v1, v2, pt, pt, seg_count, 1 );

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
	register struct wdb_pipeseg		*nextp;
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
		LOCAL fastf_t radius;
		LOCAL fastf_t inv_radius;
		LOCAL vect_t end_dir;
		LOCAL point_t start,end;

		nextp = RT_LIST_PNEXT( wdb_pipeseg, &psp->l );
		switch( psp->ps_type )
		{
			case WDB_PIPESEG_TYPE_LINEAR:
				VSUB2( height, nextp->ps_start , psp->ps_start );
				VMOVE( pipe_dir, height );
				VUNITIZE( pipe_dir );
				mat_vec_ortho( v1, pipe_dir );
				VCROSS( v2, pipe_dir, v1 );

				/* draw outer surface */
				draw_pipe_surface( vhead, psp->ps_od/2.0, nextp->ps_od/2.0,
					psp->ps_start, height, v1, v2, ARC_SEGS );

				/* draw inner surface */
				if( psp->ps_id <= 0.0 && nextp->ps_id <= 0.0 )
					break;
				draw_pipe_surface( vhead, psp->ps_id/2.0, nextp->ps_id/2.0,
					psp->ps_start, height, v1, v2, ARC_SEGS );
				prev_type = WDB_PIPESEG_TYPE_LINEAR;
				break;
			case WDB_PIPESEG_TYPE_BEND:
				if( prev_type != WDB_PIPESEG_TYPE_LINEAR )
				{
					VSUB2( v1, psp->ps_start, psp->ps_bendcenter );
					VUNITIZE( v1 );
					VSUB2( tmp_vec, nextp->ps_start, psp->ps_bendcenter );
					VCROSS( v2, tmp_vec, v1 );
					VUNITIZE( v2 );
					VCROSS( pipe_dir, v1, v2 );
				}
				if( psp->ps_od > 0.0 )
					draw_pipe_arc( vhead, psp->ps_od/2.0, psp->ps_start,
							v1, v2, tmp_center, tmp_center, ARC_SEGS, 1 );
				if( psp->ps_id > 0.0 )
					draw_pipe_arc( vhead, psp->ps_id/2.0, psp->ps_start,
							v1, v2, tmp_center, tmp_center, ARC_SEGS, 1 );

				if( psp->ps_od != nextp->ps_od )
				{
					rt_log( "Pipe solid has bend with non-constant O.D.\n" );
					return( -1 );
				}
				if( psp->ps_id != nextp->ps_id )
				{
					rt_log( "Pipe solid has bend with non-constant I.D.\n" );
					return( -1 );
				}

				/* draw bend arcs in plane of bend */
				VSUB2( v1, psp->ps_start, psp->ps_bendcenter );
				radius = MAGNITUDE( v1 );
				inv_radius = 1.0/radius;
				VSCALE( v1, v1, inv_radius );
				VCROSS( v2, pipe_dir, v1 );
				VSUB2( end_dir, nextp->ps_start, psp->ps_bendcenter );
				VUNITIZE( end_dir );
				VJOIN1( start, psp->ps_start, psp->ps_od/2.0, v1 );
				VJOIN1( end, nextp->ps_start, nextp->ps_od/2.0, end_dir );
				draw_pipe_arc( vhead, radius+psp->ps_od/2.0,
					psp->ps_bendcenter, v1, pipe_dir, start, end, ARC_SEGS, 0 );
				VJOIN1( start, psp->ps_start, -psp->ps_od/2.0, v1 );
				VJOIN1( end, nextp->ps_start, -nextp->ps_od/2.0, end_dir );
				draw_pipe_arc( vhead, radius-psp->ps_od/2.0,
					psp->ps_bendcenter, v1, pipe_dir, start, end, ARC_SEGS, 0 );
				if( psp->ps_id > 0.0 )
				{
					VJOIN1( start, psp->ps_start, psp->ps_id/2.0, v1 );
					VJOIN1( end, nextp->ps_start, nextp->ps_id/2.0, end_dir );
					draw_pipe_arc( vhead, radius+psp->ps_id/2.0,
						psp->ps_bendcenter, v1, pipe_dir, start, end, ARC_SEGS, 0 );
					VJOIN1( start, psp->ps_start, -psp->ps_id/2.0, v1 );
					VJOIN1( end, nextp->ps_start, -nextp->ps_id/2.0, end_dir );
					draw_pipe_arc( vhead, radius-psp->ps_id/2.0,
						psp->ps_bendcenter, v1, pipe_dir, start, end, ARC_SEGS, 0 );
				}

				/* draw bend arcs not in plane of bend */
				VJOIN1( tmp_center, psp->ps_bendcenter, psp->ps_od/2.0, v2 );
				VJOIN1( start, psp->ps_start, psp->ps_od/2.0, v2 );
				VJOIN1( end, nextp->ps_start, nextp->ps_od/2.0, v2 );
				draw_pipe_arc( vhead, radius,
					tmp_center, v1, pipe_dir, start, end, ARC_SEGS, 0 );

				VJOIN1( tmp_center, psp->ps_bendcenter, -psp->ps_od/2.0, v2 );
				VJOIN1( start, psp->ps_start, -psp->ps_od/2.0, v2 );
				VJOIN1( end, nextp->ps_start, -nextp->ps_od/2.0, v2 );
				draw_pipe_arc( vhead, radius,
					tmp_center, v1, pipe_dir, start, end, ARC_SEGS, 0 );

				if( psp->ps_id > 0.0 )
				{
					VJOIN1( tmp_center, psp->ps_bendcenter, psp->ps_id/2.0, v2 );
					VJOIN1( start, psp->ps_start, psp->ps_id/2.0, v2 );
					VJOIN1( end, nextp->ps_start, nextp->ps_id/2.0, v2 );
					draw_pipe_arc( vhead, radius,
						tmp_center, v1, pipe_dir, start, end, ARC_SEGS, 0 );

					VJOIN1( tmp_center, psp->ps_bendcenter, -psp->ps_id/2.0, v2 );
					VJOIN1( start, psp->ps_start, -psp->ps_id/2.0, v2 );
					VJOIN1( end, nextp->ps_start, -nextp->ps_id/2.0, v2 );
					draw_pipe_arc( vhead, radius,
						tmp_center, v1, pipe_dir, start, end, ARC_SEGS, 0 );
				}

				/* prepare for drawing END circles */
				VSUB2( v1, nextp->ps_start, psp->ps_bendcenter );
				VUNITIZE( v1 );
				prev_type = WDB_PIPESEG_TYPE_BEND;
				break;
			case WDB_PIPESEG_TYPE_END:
				if( psp->ps_od > 0.0 )
					draw_pipe_arc( vhead, psp->ps_od/2.0,
						psp->ps_start, v1, v2, tmp_center, tmp_center, ARC_SEGS, 1 );
				if( psp->ps_id > 0.0 )
					draw_pipe_arc( vhead, psp->ps_id/2.0,
						psp->ps_start, v1, v2, tmp_center, tmp_center, ARC_SEGS, 1 );
				break;
			default:
				rt_log("rt_pipe_plot: unknown ps_type=%d\n", psp->ps_type);
				return(-1);
		}
	}
	return(0);
}

void
tesselate_pipe_start( pipe, arc_segs, sin_del, cos_del, outer_loop, inner_loop, r1, r2, s, tol )
struct wdb_pipeseg *pipe;
int arc_segs;
double sin_del;
double cos_del;
struct vertex ***outer_loop;
struct vertex ***inner_loop;
vect_t r1,r2;
struct shell *s;
struct rt_tol *tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct wdb_pipeseg *next;
	point_t pt;
	fastf_t or;
	fastf_t ir;
	fastf_t x,y,xnew,ynew;
	vect_t n;
	struct vertex ***verts;
	int i;

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	next = RT_LIST_NEXT( wdb_pipeseg, &pipe->l );

	if( pipe->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		VSUB2( n, pipe->ps_start, next->ps_start );
		VUNITIZE( n );
		mat_vec_ortho( r1, n );
		VCROSS( r2, n, r1 );

	}
	else if( pipe->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		VSUB2( r1, pipe->ps_start, pipe->ps_bendcenter );
		VUNITIZE( r1 );

		VSUB2( n, next->ps_start, pipe->ps_bendcenter );
		VCROSS( r2, r1, n );
		VUNITIZE( r2 );
	}

	or = pipe->ps_od/2.0;
	ir = pipe->ps_id/2.0;

	if( or <= tol->dist )
		return;

	if( ir > or )
	{
		rt_log( "Inner radius larger than outer radius at start of pipe solid\n" );
		return;
	}

	if( NEAR_ZERO( ir - or, tol->dist) )
		return;


	fu = nmg_cface( s, *outer_loop, arc_segs );

	x = or;
	y = 0.0;
	i = (-1);
	lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		VJOIN2( pt, pipe->ps_start, x, r1, y, r2 );
		(*outer_loop)[++i] = eu->vu_p->v_p;
		nmg_vertex_gv( eu->vu_p->v_p, pt );
		xnew = x*cos_del - y*sin_del;
		ynew = x*sin_del + y*cos_del;
		x = xnew;
		y = ynew;
	}

	if( ir > tol->dist )
	{
		struct edgeuse *new_eu;
		struct vertexuse *vu;

		/* create a loop of a single vertex using the first vertex from the inner loop */
		lu = nmg_mlv( &fu->l.magic, (struct vertex *)NULL, OT_OPPOSITE );

		vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
		eu = nmg_meonvu( vu );
		(*inner_loop)[0] = eu->vu_p->v_p;

		x = ir;
		y = 0.0;
		VJOIN2( pt, pipe->ps_start, x, r1, y, r2 );
		nmg_vertex_gv( (*inner_loop)[0], pt );
		/* split edges in loop for each vertex in inner loop */
		for( i=1 ; i<arc_segs ; i++ )
		{
			new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
			(*inner_loop)[i] = new_eu->vu_p->v_p;
			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;
			VJOIN2( pt, pipe->ps_start, x, r1, y, r2 );
			nmg_vertex_gv( (*inner_loop)[i], pt );
		}
	}
#if 0
	else if( next->ps_id > tol->dist )
	{
		struct vertexuse *vu;

		/* make a loop of a single vertex in this face */
		lu = nmg_mlv( &fu->l.magic, (struct vertex *)NULL, OT_OPPOSITE );
		vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );

		nmg_vertex_gv( vu->v_p, pipe->ps_start );
	}
#endif
	if( nmg_calc_face_g( fu ) )
		rt_bomb( "tesselate_pipe_start: nmg_calc_face_g failed\n" );

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );

		if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
			continue;

		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			NMG_CK_EDGEUSE( eu );
			eu->e_p->is_real = 1;
		}
	}
}

void
tesselate_pipe_linear( pipe, arc_segs, sin_del, cos_del, outer_loop, inner_loop, r1, r2, s, tol )
struct wdb_pipeseg *pipe;
int arc_segs;
double sin_del;
double cos_del;
struct vertex **outer_loop[];
struct vertex **inner_loop[];
vect_t r1, r2;
struct shell *s;
struct rt_tol *tol;
{
	struct wdb_pipeseg *next;
	struct vertex **new_outer_loop;
	struct vertex **new_inner_loop;
	struct vertex **verts[3];
	struct faceuse *fu;
	vect_t norm;
	vect_t reverse_norm;
	fastf_t or;
	fastf_t next_or;
	fastf_t ir;
	fastf_t next_ir;
	vect_t n;
	int i;

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	if( pipe->ps_type != WDB_PIPESEG_TYPE_LINEAR )
	{
		rt_log( "tesselate_pipe_linear() called for wrong pipe type (%d)\n", pipe->ps_type );
		rt_bomb( "tesselate_pipe_linear()\n" );
	}

	next = RT_LIST_NEXT( wdb_pipeseg, &pipe->l );

	or = pipe->ps_od/2.0;
	ir = pipe->ps_id/2.0;
	next_or = next->ps_od/2.0;
	next_ir = next->ps_id/2.0;

	if( next_or > tol->dist )
		new_outer_loop = (struct vertex **)rt_calloc( arc_segs, sizeof( struct vertex *),
				"tesselate_pipe_linear: new_outer_loop" );

	if( next_ir > tol->dist )
		new_inner_loop = (struct vertex **)rt_calloc( arc_segs, sizeof( struct vertex *),
				"tesselate_pipe_linear: new_inner_loop" );

	if( or > tol->dist && next_or > tol->dist )
	{
		point_t pt;
		fastf_t x,y,xnew,ynew;
		struct faceuse *fu_prev=(struct faceuse *)NULL;
		struct vertex **verts[3];

		x = next_or;
		y = 0.0;
		for( i=0 ; i<arc_segs ; i++ )
		{
			VJOIN2( pt, next->ps_start, x, r1, y, r2 );
			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;

			if( fu_prev )
			{
				nmg_vertex_gv( new_outer_loop[i], pt );
				if( nmg_calc_face_g( fu_prev ) )
				{
					rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
					nmg_kfu( fu_prev );
				}
			}

			if( i == arc_segs-1 )
				verts[0] = &(*outer_loop)[0];
			else
				verts[0] = &(*outer_loop)[i+1];
			verts[1] = &(*outer_loop)[i];

			verts[2] = &new_outer_loop[i];

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				rt_log( "tesselate_pipe_linear: failed to make outer face #%d or=%g, next_or=%g\n",
						i, or, next_or );
				continue;
			}
			if( !new_outer_loop[i]->vg_p )
				nmg_vertex_gv( new_outer_loop[i], pt );

			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}

			verts[1] = verts[2];
			if( i == arc_segs-1 )
				verts[2] = &new_outer_loop[0];
			else
				verts[2] = &new_outer_loop[i+1];
			if( (fu_prev = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				rt_log( "tesselate_pipe_linear: failed to make outer face #%d or=%g, next_or=%g\n",
						i, or, next_or );
				continue;
			}
			if( i == arc_segs-1 )
			{
				if( nmg_calc_face_g( fu_prev ) )
				{
					rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
					nmg_kfu( fu_prev );
				}
			}

		}
		rt_free( (char *)(*outer_loop), "tesselate_pipe_bend: outer_loop" );
		*outer_loop = new_outer_loop;
	}
	else if( or > tol->dist && next_or <= tol->dist )
	{
		struct vertex *v=(struct vertex *)NULL;

		for( i=0 ; i<arc_segs; i++ )
		{
			verts[1] = &(*outer_loop)[i];
			if( i == arc_segs-1 )
				verts[0] = &(*outer_loop)[0];
			else
				verts[0] = &(*outer_loop)[i+1];

			verts[2] = &v;

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				rt_log( "tesselate_pipe_linear: failed to make outer face #%d or=%g, next_or=%g\n",
						i, or, next_or );
				continue;
			}
			if( i == 0 )
				nmg_vertex_gv( v, next->ps_start );
			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
		}

		rt_free( (char *)(*outer_loop), "tesselate_pipe_linear: outer_loop" );
		outer_loop[0] = &v;
	}
	else if( or <= tol->dist && next_or > tol->dist )
	{
		point_t pt,pt_next;
		fastf_t x,y,xnew,ynew;
		struct vertex **verts[3];

		x = or;
		y = 0.0;
		VJOIN2( pt_next, pipe->ps_start, x, r1, y, r2 );
		for( i=0 ; i<arc_segs; i++ )
		{
			VMOVE( pt, pt_next )
			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;
			VJOIN2( pt_next, pipe->ps_start, x, r1, y, r2 );

			verts[0] = &(*outer_loop)[0];
			verts[1] = &new_outer_loop[i];
			if( i == arc_segs-1 )
				verts[2] = &new_outer_loop[0];
			else
				verts[2] = &new_outer_loop[i+1];

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				rt_log( "tesselate_pipe_linear: failed to make outer face #%d or=%g, next_or=%g\n",
						i, or, next_or );
				continue;
			}
			if( !new_outer_loop[i]->vg_p )
				nmg_vertex_gv( new_outer_loop[i], pt );
			if( i+1 < arc_segs-1 && !new_outer_loop[i+1]->vg_p )
				nmg_vertex_gv( new_outer_loop[i+1], pt_next );
			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			
		}
		rt_free( (char *)(*outer_loop), "tesselate_pipe_linear: outer_loop" );
		*outer_loop = new_outer_loop;
	}

	if( ir > tol->dist && next_ir > tol->dist )
	{
		point_t pt;
		fastf_t x,y,xnew,ynew;
		struct faceuse *fu_prev=(struct faceuse *)NULL;
		struct vertex **verts[3];

		x = next_ir;
		y = 0.0;
		for( i=0 ; i<arc_segs ; i++ )
		{
			VJOIN2( pt, next->ps_start, x, r1, y, r2 );
			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;

			if( fu_prev )
			{
				nmg_vertex_gv( new_inner_loop[i], pt );
				if( nmg_calc_face_g( fu_prev ) )
				{
					rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
					nmg_kfu( fu_prev );
				}
			}

			if( i == arc_segs-1 )
				verts[0] = &(*inner_loop)[0];
			else
				verts[0] = &(*inner_loop)[i+1];
			verts[1] = &new_inner_loop[i];

			verts[2] = &(*inner_loop)[i];

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				rt_log( "tesselate_pipe_linear: failed to make inner face #%d ir=%g, next_ir=%g\n",
						i, ir, next_ir );
				continue;
			}
			if( !new_inner_loop[i]->vg_p )
				nmg_vertex_gv( new_inner_loop[i], pt );

			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}

			verts[2] = verts[0];
			verts[0] = verts[1];
			verts[1] = verts[2];
			if( i == arc_segs-1 )
				verts[2] = &new_inner_loop[0];
			else
				verts[2] = &new_inner_loop[i+1];
			if( (fu_prev = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				rt_log( "tesselate_pipe_linear: failed to make inner face #%d ir=%g, next_ir=%g\n",
						i, ir, next_ir );
				continue;
			}
			if( i == arc_segs-1 )
			{
				if( nmg_calc_face_g( fu_prev ) )
				{
					rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
					nmg_kfu( fu_prev );
				}
			}

		}
		rt_free( (char *)(*inner_loop), "tesselate_pipe_bend: inner_loop" );
		*inner_loop = new_inner_loop;
	}
	else if( ir > tol->dist && next_ir <= tol->dist )
	{
		struct vertex *v=(struct vertex *)NULL;

		for( i=0 ; i<arc_segs; i++ )
		{
			verts[0] = &(*inner_loop)[i];
			if( i == arc_segs-1 )
				verts[1] = &(*inner_loop)[0];
			else
				verts[1] = &(*inner_loop)[i+1];

			verts[2] = &v;

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				rt_log( "tesselate_pipe_linear: failed to make inner face #%d ir=%g, next_ir=%g\n",
						i, ir, next_ir );
				continue;
			}
			if( i == 0 )
				nmg_vertex_gv( v, next->ps_start );
			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
		}

		rt_free( (char *)(*inner_loop), "tesselate_pipe_linear: inner_loop" );
		inner_loop[0] = &v;
	}
	else if( ir <= tol->dist && next_ir > tol->dist )
	{
		point_t pt,pt_next;
		fastf_t x,y,xnew,ynew;
		struct vertex **verts[3];

		x = next_ir;
		y = 0.0;
		VJOIN2( pt_next, next->ps_start, x, r1, y, r2 );
		for( i=0 ; i<arc_segs; i++ )
		{
			VMOVE( pt, pt_next )
			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;
			VJOIN2( pt_next, next->ps_start, x, r1, y, r2 );

			verts[2] = &(*inner_loop)[0];
			verts[1] = &new_inner_loop[i];
			if( i == arc_segs-1 )
				verts[0] = &new_inner_loop[0];
			else
				verts[0] = &new_inner_loop[i+1];

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				rt_log( "tesselate_pipe_linear: failed to make inner face #%d ir=%g, next_ir=%g\n",
						i, ir, next_ir );
				continue;
			}
			if( i == 0 )
				nmg_vertex_gv( (*inner_loop)[0], pipe->ps_start );
			if( !new_inner_loop[i]->vg_p )
				nmg_vertex_gv( new_inner_loop[i], pt );
			if( i+1 < arc_segs && !new_inner_loop[i+1]->vg_p )
				nmg_vertex_gv( new_inner_loop[i+1], pt_next );
			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			
		}
		rt_free( (char *)(*inner_loop), "tesselate_pipe_linear: inner_loop" );
		*inner_loop = new_inner_loop;
	}
}

void
tesselate_pipe_bend( pipe, arc_segs, sin_del, cos_del, outer_loop, inner_loop, start_r1, start_r2, s, tol, ttol )
struct wdb_pipeseg *pipe;
int arc_segs;
double sin_del;
double cos_del;
struct vertex **outer_loop[];
struct vertex **inner_loop[];
vect_t start_r1, start_r2;
struct shell *s;
struct rt_tol *tol;
struct rt_tess_tol *ttol;
{
	struct wdb_pipeseg *next;
	struct vertex **new_outer_loop;
	struct vertex **new_inner_loop;
	struct vertex *start_v;
	struct vertex_g *start_vg;
	fastf_t bend_radius;
	fastf_t bend_angle;
	fastf_t or;
	fastf_t ir;
	fastf_t x_bend,y_bend,x_bend_new,y_bend_new;
	fastf_t x,y,xnew,ynew;
	fastf_t delta_angle;
	fastf_t start_arc_angle;
	fastf_t xstart,ystart;
	double cos_bend_del;
	double sin_bend_del;
	mat_t rot;
	vect_t b1;
	vect_t b2;
	vect_t r1, r2;
	vect_t r1_tmp,r2_tmp;
	vect_t bend_norm;
	vect_t to_start;
	vect_t to_end;
	vect_t to_start_v;
	point_t origin;
	point_t center;
	int bend_segs=3;	/* minimum number of edges along bend */
	int bend_seg;
	int tol_segs;
	int i;

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );
	RT_CK_TESS_TOL( ttol );

	VMOVE( r1, start_r1 );
	VMOVE( r2, start_r2 );

	if( pipe->ps_type != WDB_PIPESEG_TYPE_BEND )
	{
		rt_log( "tesselate_pipe_bend() called with wrong pipe segment type (%d)\n" , pipe->ps_type );
		rt_bomb( "tesselate_pipe_bend()\n" );
	}

	next = RT_LIST_NEXT( wdb_pipeseg, &pipe->l );

	or = pipe->ps_od/2.0;
	ir = pipe->ps_id/2.0;

	/* Calculate vector b1, unit vector in direction from
	 * bend center to start point
	 */
	VSUB2( to_start, pipe->ps_start, pipe->ps_bendcenter );
	bend_radius = MAGNITUDE( to_start );
	VSCALE( b1, to_start, 1.0/bend_radius );

	/* bend_norm is normal to plane of bend */
	VSUB2( to_end, next->ps_start, pipe->ps_bendcenter );
	VCROSS( bend_norm, b1, to_end );
	VUNITIZE( bend_norm );

	/* b1, b2, and bend_norm form a RH coord, system */
	VCROSS( b2, bend_norm, b1 );

	bend_angle = atan2( VDOT( to_end, b2 ), VDOT( to_end, b1 ) );
	if( bend_angle < 0.0 )
		bend_angle += 2.0*M_PI;

	/* calculate number of segments to use along bend */
	if( ttol->abs > 0.0 && ttol->abs < bend_radius+or )
	{
		tol_segs = ceil( bend_angle/(2.0*acos( 1.0 - ttol->abs/(bend_radius+or) ) ) );
		if( tol_segs > bend_segs )
			bend_segs = tol_segs;
	}
	if( ttol->rel > 0.0 )
	{
		tol_segs = ceil( (fastf_t)(arc_segs)*bend_angle/2.0*M_PI );
		if( tol_segs > bend_segs )
			bend_segs = tol_segs;
	}
	if( ttol->norm > 0.0 )
	{
		tol_segs = ceil( bend_angle/(2.0*ttol->norm ) );
		if( tol_segs > bend_segs )
			bend_segs = tol_segs;
	}

	delta_angle = bend_angle/(fastf_t)(bend_segs);
	sin_bend_del = sin( delta_angle );
	cos_bend_del = cos( delta_angle );

	VSETALL( origin, 0.0 );
	mat_arb_rot( rot, origin, bend_norm, delta_angle);

	x_bend = 1.0;
	y_bend = 0.0;
	VMOVE( center, pipe->ps_start )
	for( bend_seg=0; bend_seg<bend_segs ; bend_seg++ )
	{

		new_outer_loop = (struct vertex **)rt_calloc( arc_segs, sizeof( struct vertex *),
				"tesselate_pipe_bend(): new_outer_loop" );

		MAT4X3VEC( r1_tmp, rot, r1 )
		MAT4X3VEC( r2_tmp, rot, r2 )
		VMOVE( r1, r1_tmp )
		VMOVE( r2, r2_tmp )

		VSUB2( r1_tmp, center, pipe->ps_bendcenter )
		MAT4X3PNT( r2_tmp, rot, r1_tmp )
		VADD2( center, r2_tmp, pipe->ps_bendcenter )

		x = or;
		y = 0.0;
		for( i=0; i<arc_segs; i++ )
		{
			struct faceuse *fu;
			struct vertex **verts[3];
			point_t pt;

			if( i == arc_segs-1 )
				verts[0] = &(*outer_loop)[0];
			else
				verts[0] = &(*outer_loop)[i+1];
			verts[1] = &(*outer_loop)[i];
			verts[2] = &new_outer_loop[i];

			if( (fu=nmg_cmface( s, verts, 3 )) == NULL )
			{
				rt_log( "tesselate_pipe_bend(): nmg_cmface failed\n" );
				rt_bomb( "tesselate_pipe_bend\n" );
			}
			VJOIN2( pt, center, x, r1, y, r2 );
			if( !new_outer_loop[i]->vg_p )
				nmg_vertex_gv( new_outer_loop[i], pt );
			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_bend: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}

			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;

			verts[1] = verts[2];
			if( i == arc_segs-1 )
				verts[2] = &new_outer_loop[0];
			else
				verts[2] = &new_outer_loop[i+1];

			if( (fu=nmg_cmface( s, verts, 3 )) == NULL )
			{
				rt_log( "tesselate_pipe_bend(): nmg_cmface failed\n" );
				rt_bomb( "tesselate_pipe_bend\n" );
			}
			VJOIN2( pt, center, x, r1, y, r2 );
			if( !(*verts[2])->vg_p )
				nmg_vertex_gv( *verts[2], pt );
			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_bend: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}

		}

		x_bend = x_bend_new;
		y_bend = y_bend_new;

		rt_free( (char *)(*outer_loop), "tesselate_pipe_bend: outer_loop" );
		*outer_loop = new_outer_loop;
	}

	if( ir <= tol->dist )
	{
		VMOVE( start_r1, r1 )
		VMOVE( start_r2, r2 )
		return;
	}

	VMOVE( r1, start_r1 )
	VMOVE( r2, start_r2 )

	x_bend = 1.0;
	y_bend = 0.0;
	VMOVE( center, pipe->ps_start )
	for( bend_seg=0; bend_seg<bend_segs ; bend_seg++ )
	{

		new_inner_loop = (struct vertex **)rt_calloc( arc_segs, sizeof( struct vertex *),
				"tesselate_pipe_bend(): new_inner_loop" );

		MAT4X3VEC( r1_tmp, rot, r1 )
		MAT4X3VEC( r2_tmp, rot, r2 )
		VMOVE( r1, r1_tmp )
		VMOVE( r2, r2_tmp )

		VSUB2( r1_tmp, center, pipe->ps_bendcenter )
		MAT4X3PNT( r2_tmp, rot, r1_tmp )
		VADD2( center, r2_tmp, pipe->ps_bendcenter )

		x = ir;
		y = 0.0;
		for( i=0; i<arc_segs; i++ )
		{
			struct faceuse *fu;
			struct vertex **verts[3];
			point_t pt;

			verts[0] = &(*inner_loop)[i];
			if( i == arc_segs-1 )
				verts[1] = &(*inner_loop)[0];
			else
				verts[1] = &(*inner_loop)[i+1];
			verts[2] = &new_inner_loop[i];

			if( (fu=nmg_cmface( s, verts, 3 )) == NULL )
			{
				rt_log( "tesselate_pipe_bend(): nmg_cmface failed\n" );
				rt_bomb( "tesselate_pipe_bend\n" );
			}
			VJOIN2( pt, center, x, r1, y, r2 );
			if( !new_inner_loop[i]->vg_p )
				nmg_vertex_gv( new_inner_loop[i], pt );
			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_bend: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}

			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;

			verts[0] = verts[2];
			if( i == arc_segs-1 )
				verts[2] = &new_inner_loop[0];
			else
				verts[2] = &new_inner_loop[i+1];

			if( (fu=nmg_cmface( s, verts, 3 )) == NULL )
			{
				rt_log( "tesselate_pipe_bend(): nmg_cmface failed\n" );
				rt_bomb( "tesselate_pipe_bend\n" );
			}
			VJOIN2( pt, center, x, r1, y, r2 );
			if( !(*verts[2])->vg_p )
				nmg_vertex_gv( *verts[2], pt );
			if( nmg_calc_face_g( fu ) )
			{
				rt_log( "tesselate_pipe_bend: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}

		}
		x_bend = x_bend_new;
		y_bend = y_bend_new;
		rt_free( (char *)(*inner_loop), "tesselate_pipe_bend: inner_loop" );
		*inner_loop = new_inner_loop;
	}
	VMOVE( start_r1, r1 )
	VMOVE( start_r2, r2 )
}

void
tesselate_pipe_end( pipe, arc_segs, sin_del, cos_del, outer_loop, inner_loop, s, tol )
struct wdb_pipeseg *pipe;
int arc_segs;
double sin_del;
double cos_del;
struct vertex ***outer_loop;
struct vertex ***inner_loop;
struct shell *s;
struct rt_tol *tol;
{
	struct wdb_pipeseg *prev;
	struct faceuse *fu;
	struct loopuse *lu;
	int i;

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	if( pipe->ps_od <= tol->dist )
		return;

	if( NEAR_ZERO( pipe->ps_od - pipe->ps_id, tol->dist) )
		return;

	if( (fu = nmg_cface( s, *outer_loop, arc_segs )) == NULL )
	{
		rt_log( "tesselate_pipe_end(): nmg_cface failed\n" );
		return;
	}
	fu = fu->fumate_p;
	if( nmg_calc_face_g( fu ) )
	{
		rt_log( "tesselate_pipe_end: nmg_calc_face_g failed\n" );
		nmg_kfu( fu );
		return;
	}

	prev = RT_LIST_PREV( wdb_pipeseg, &pipe->l );
	
	if( pipe->ps_id > tol->dist )
	{
		struct vertex **verts;
		int i;

		verts = (struct vertex **)rt_calloc( arc_segs, sizeof( struct vertex *),
			"tesselate_pipe_end: verts" );
		for( i=0 ; i<arc_segs; i++ )
			verts[i] = (*inner_loop)[i];

		fu = nmg_add_loop_to_face( s, fu, verts, arc_segs, OT_OPPOSITE );

		rt_free( (char *)verts, "tesselate_pipe_end: verts" );
	}
#if 0
	else if( prev->ps_id > tol->dist )
	{
		struct vertexuse *vu;

		/* make a loop of a single vertex in this face */
		lu = nmg_mlv( &fu->l.magic, (struct vertex *)NULL, OT_OPPOSITE );
		vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );

		nmg_vertex_gv( vu->v_p, pipe->ps_start );
	}
#endif
	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		struct edgeuse *eu;

		NMG_CK_LOOPUSE( lu );

		if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
			continue;

		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			NMG_CK_EDGEUSE( eu );
			eu->e_p->is_real = 1;
		}
	}
}

/*
 *			R T _ P I P E _ T E S S
 *
 *	XXXX Still needs vertexuse normals!
 */
int
rt_pipe_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	struct shell *s;
	struct rt_pipe_internal *pip;
	struct wdb_pipeseg *pipe_head;
	struct wdb_pipeseg *pipe;
	int arc_segs=6;			/* minimum number of segments for a circle */
	int tol_segs;
	fastf_t max_diam=0.0;
	fastf_t pipe_size;
	double delta_angle;
	double sin_del;
	double cos_del;
	point_t min_pt;
	point_t max_pt;
	vect_t min_to_max;
	vect_t r1, r2;
	struct vertex **outer_loop;
	struct vertex **inner_loop;

	RT_CK_DB_INTERNAL(ip);
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC( pip );

	RT_CK_TOL( tol );
	RT_CK_TESS_TOL( ttol );
	NMG_CK_MODEL( m );

	*r = (struct nmgregion *)NULL;

	pipe_head = (struct wdb_pipeseg *)(&pip->pipe_segs_head);
	if( RT_LIST_IS_EMPTY( &pipe_head->l ) )
		return( 0 );	/* nothing to tesselate */

	pipe = RT_LIST_FIRST( wdb_pipeseg, &pipe_head->l );
	if( pipe->ps_type == WDB_PIPESEG_TYPE_END )
		return( 0 );	/* nothing to tesselate */

	VMOVE( min_pt, pipe->ps_start );
	VMOVE( max_pt, pipe->ps_start );

	/* find max diameter */
	for( RT_LIST_FOR( pipe, wdb_pipeseg, &pipe_head->l ) )
	{
		if( pipe->ps_od > 0.0 && pipe->ps_od > max_diam )
			max_diam = pipe->ps_od;

		VMINMAX( min_pt, max_pt, pipe->ps_start );
	}

	if( max_diam <= tol->dist )
		return( 0 );	/* nothing to tesselate */

	/* calculate pipe size for relative tolerance */
	VSUB2( min_to_max, max_pt, min_pt );
	pipe_size = MAGNITUDE( min_to_max );

	/* calculate number of segments for circles */
	if( ttol->abs > 0.0 && ttol->abs * 2.0 < max_diam )
	{
		tol_segs = ceil( M_PI/acos( 1.0 - 2.0 * ttol->abs/max_diam) );
		if( tol_segs > arc_segs )
			arc_segs = tol_segs;
	}
	if( ttol->rel > 0.0 && 2.0 * ttol->rel * pipe_size < max_diam )
	{
		tol_segs = ceil( M_PI/acos( 1.0 - 2.0 * ttol->rel*pipe_size/max_diam) );
		if( tol_segs > arc_segs )
			arc_segs = tol_segs;
	}
	if( ttol->norm > 0.0 )
	{
		tol_segs = ceil( M_PI/ttol->norm );
		if( tol_segs > arc_segs )
			arc_segs = tol_segs;
	}

	*r = nmg_mrsv( m );
	s = RT_LIST_FIRST(shell, &(*r)->s_hd);

	outer_loop = (struct vertex **)rt_calloc( arc_segs, sizeof( struct vertex *),
			"rt_pipe_tess: outer_loop" );
	inner_loop = (struct vertex **)rt_calloc( arc_segs, sizeof( struct vertex *),
			"rt_pipe_tess: inner_loop" );

	delta_angle = 2.0 * M_PI / (double)arc_segs;
	sin_del = sin( delta_angle );
	cos_del = cos( delta_angle );

	pipe = RT_LIST_FIRST( wdb_pipeseg, &pipe_head->l );
	tesselate_pipe_start( pipe, arc_segs, sin_del, cos_del,
		&outer_loop, &inner_loop, r1, r2, s, tol );

	for( RT_LIST_FOR( pipe, wdb_pipeseg, &pipe_head->l ) )
	{
		struct wdb_pipeseg *prev_pipe=(struct wdb_pipeseg *)NULL;

		switch( pipe->ps_type )
		{
			case WDB_PIPESEG_TYPE_LINEAR:
				tesselate_pipe_linear( pipe, arc_segs, sin_del, cos_del,
						&outer_loop, &inner_loop, r1, r2, s, tol );
				prev_pipe = pipe;
				break;
			case WDB_PIPESEG_TYPE_BEND:
				tesselate_pipe_bend( pipe, arc_segs, sin_del, cos_del,
						&outer_loop, &inner_loop, r1, r2, s, tol, ttol );
				prev_pipe = pipe;
				break;
			case WDB_PIPESEG_TYPE_END:
				tesselate_pipe_end( pipe, arc_segs, sin_del, cos_del,
						&outer_loop, &inner_loop, s, tol );
				break;
			default:
				rt_log( " ERROR: rt_pipe_tess: Unrecognized segment type (%d)\n", pipe->ps_type );
				break;
		}
	}
	rt_free( (char *)outer_loop, "rt_pipe_tess: outer_loop" );
	rt_free( (char *)inner_loop, "rt_pipe_tess: inner_loop" );

	nmg_rebound( m, tol );

	return( 0 );
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
		psp->l.magic = WDB_PIPESEG_MAGIC;
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

#if 0
	/* Too much for the MGED Display!!!! */
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
#endif
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

/*
 *			R T _ P I P E _ C K
 *
 *  Check pipe solid
 *	All bends must have constant diameters
 *	No consecutive LINEAR sections without BENDS unless the
 *		LINEAR sections are collinear.
 */
int
rt_pipe_ck( headp )
struct wdb_pipeseg *headp;
{
	register struct wdb_pipeseg	*psp;
	register int			error_count=0;
	LOCAL vect_t			to_start;
	LOCAL vect_t			to_end;
	LOCAL fastf_t			bend_radius_sq1;
	LOCAL fastf_t			bend_radius_sq2;
	LOCAL fastf_t			dot_prod;
	LOCAL int			seg_no=0;

	psp = RT_LIST_FIRST( wdb_pipeseg, &headp->l );
	while( RT_LIST_NOT_HEAD( &psp->l, &headp->l ) )
	{
		register struct wdb_pipeseg	*next;
		vect_t				dir1;
		vect_t				dir2;

		seg_no++;

		if( psp->l.magic != WDB_PIPESEG_MAGIC )
		{
			rt_log( "Pipe solid segment #%d has bad MAGIC, should be x%x, but is x%x\n",
				seg_no, WDB_PIPESEG_MAGIC, psp->l.magic );
			error_count++;
			return( error_count );
		}

		next = RT_LIST_PNEXT( wdb_pipeseg, &psp->l );
		if( RT_LIST_NOT_HEAD( &next->l, &headp->l ) )
		{
			if( next->l.magic != WDB_PIPESEG_MAGIC )
			{
				rt_log( "Pipe solid segment #%d has bad MAGIC, should be x%x, but is x%x\n",
					seg_no+1, WDB_PIPESEG_MAGIC, next->l.magic );
				error_count++;
				return( error_count );
			}
		}
		else if( psp->ps_type != WDB_PIPESEG_TYPE_END )
		{
			rt_log( "Pipe solid does not end with an END segment\m" );
			error_count++;
			return( error_count );
		}

		switch( psp->ps_type )
		{
			case WDB_PIPESEG_TYPE_BEND:
				VSUB2( to_start, psp->ps_start, psp->ps_bendcenter );
				VSUB2( to_end, next->ps_start, psp->ps_bendcenter );
				bend_radius_sq1 = MAGSQ( to_start );
				bend_radius_sq2 = MAGSQ( to_end );
				if( !NEAR_ZERO( bend_radius_sq1 - bend_radius_sq2, RT_LEN_TOL*RT_LEN_TOL ) )
				{
					rt_log( "Pipe bend center not in center, radii = %g,%g\n",
						sqrt( bend_radius_sq1 ), sqrt( bend_radius_sq2 ) );
					error_count++;
				}
				dot_prod = VDOT( to_start, to_end )/bend_radius_sq1;
				if( dot_prod < 0.0 )
					dot_prod = (-dot_prod);
				if( NEAR_ZERO( dot_prod - 1.0, RT_DOT_TOL ) )
				{
					rt_log( "Pipe bend must be less than 180 degrees\n" );
					error_count++;
				}
				if( psp->ps_id >= psp->ps_od )
				{
					rt_log( "Pipe solid has BEND with inner radius (%g) as big as outer (%g)\n",
						psp->ps_id, psp->ps_od );
					error_count++;
				}
				if( psp->ps_od != next->ps_od )
				{
					rt_log( "Pipe solid has BEND with non-constant outer radius (%g and %g)\n",
						psp->ps_od, next->ps_od );
					error_count++;
				}
				if( psp->ps_id > 0.0 || next->ps_id > 0.0 )
				{
					if( psp->ps_id != next->ps_id )
					{
						rt_log( "Pipe solid has BEND with non-constant inner radius (%g and %g)\n",
							psp->ps_id, next->ps_id  );
						error_count++;
					}
				}
				if( next->ps_type == WDB_PIPESEG_TYPE_LINEAR )
				{
					register struct wdb_pipeseg *next_next;

					next_next = RT_LIST_PNEXT( wdb_pipeseg, &next->l );
					if( RT_LIST_IS_HEAD( &next_next->l, &headp->l ) )
					{
						rt_log( "Pipe does not end with an END segment\n" );
						error_count++;
						return( error_count );
					}
					if( next_next->l.magic != WDB_PIPESEG_MAGIC )
					{
						rt_log( "Pipe solid segment #%d has bad MAGIC, should be x%x, but is x%x\n",
							seg_no+2, WDB_PIPESEG_MAGIC, next_next->l.magic );
						error_count++;
						return( error_count );
					}

					VSUB2( dir1, next_next->ps_start, next->ps_start );
					VUNITIZE( dir1 );
					if( !NEAR_ZERO( VDOT( dir1, to_end )/sqrt( bend_radius_sq2 ), RT_DOT_TOL ) )
					{
						rt_log( "Pipe bend section doesn't mate with linear segment\n" );
						rt_log( "\t(segments #%d and #%d )\n", seg_no, seg_no+1 );
						error_count++;
					}
				}
				else if( next->ps_type == WDB_PIPESEG_TYPE_BEND )
				{
					LOCAL vect_t	next_to_start;
					LOCAL fastf_t	next_bend_radius;
					LOCAL fastf_t	dot;

					VSUB2( next_to_start, next->ps_start, next->ps_bendcenter );
					next_bend_radius = MAGNITUDE( next_to_start );
					dot = VDOT( next_to_start, to_end )/
						(next_bend_radius*sqrt( bend_radius_sq2));
					dot = dot < 0.0 ? -dot:dot;
					if( !NEAR_ZERO( dot - 1.0, RT_DOT_TOL ) )
					{
						rt_log( "Consecutive bend segments don't mate\n" );
						rt_log( "\t(segments #%d and #%d )\n", seg_no, seg_no+1 );
						error_count++;
					}
				}
				break;
			case WDB_PIPESEG_TYPE_LINEAR:
				if( psp->ps_id >= psp->ps_od )
				{
					rt_log( "Pipe solid has Linear section with inner radius (%g) as big as outer (%g)\n",
						psp->ps_id, psp->ps_od );
					error_count++;
				}

				/* pipe direction */
				VSUB2( dir1, next->ps_start, psp->ps_start );
				VUNITIZE( dir1 );

				if( next->ps_type == WDB_PIPESEG_TYPE_LINEAR )
				{
					register struct wdb_pipeseg *next_next;

					next_next = RT_LIST_PNEXT( wdb_pipeseg, &next->l );
					if( RT_LIST_IS_HEAD( &next_next->l, &headp->l ) )
					{
						rt_log( "Pipe does not end with an END segment\n" );
						error_count++;
						return( error_count );
					}
					if( next_next->l.magic != WDB_PIPESEG_MAGIC )
					{
						rt_log( "Pipe solid segment #%d has bad MAGIC, should be x%x, but is x%x\n",
							seg_no+2, WDB_PIPESEG_MAGIC, next_next->l.magic );
						error_count++;
						return( error_count );
					}

					VSUB2( dir2, next_next->ps_start, next->ps_start );
					VUNITIZE( dir2 );
					if( !NEAR_ZERO( VDOT( dir1, dir2 ) - 1.0, RT_DOT_TOL ) )
					{
						rt_log( "Pipe solid has a corner with no BEND segment\n" );
						error_count++;
					}
				}
				else if( next->ps_type == WDB_PIPESEG_TYPE_BEND )
				{
					VSUB2( to_start, next->ps_start, next->ps_bendcenter );
					if( !NEAR_ZERO( VDOT( dir1, to_start )/MAGNITUDE( to_start ), RT_DOT_TOL ) )
					{
						rt_log( "Linear pipe segment doesn't mate with bend\n" );
						rt_log( "\t(segments #%d and #%d )\n", seg_no, seg_no+1 );
						error_count++;
					}
				}
				break;
			case WDB_PIPESEG_TYPE_END:
				break;
			default:
				rt_log( "Pipe solid has unrecognized segment type\n" );
				error_count++;
		}
		psp = next;
	}
	return( error_count );
}
