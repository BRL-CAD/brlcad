/*                        G _ P I P E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup g_  */

/*@{*/
/** @file g_pipe.c
 *	Intersect a ray with a pipe solid.
 *
 *  Authors -
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSpipe[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#include <math.h>

#ifdef HAVE_FLOAT_H
   /* for isnan() function */
#  include <float.h>
#endif

#include "tcl.h"
#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"
#include "./debug.h"

struct id_pipe
{
	struct bu_list l;
	int	pipe_is_bend;
};

struct lin_pipe
{
	struct bu_list l;
	int	pipe_is_bend;
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
};

struct bend_pipe
{
	struct bu_list l;
	int	pipe_is_bend;
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
	point_t bend_min;
	point_t bend_max;
};


struct hit_list
{
	struct bu_list	l;
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

BU_EXTERN( void rt_pipe_ifree, (struct rt_db_internal *ip) );


HIDDEN int
rt_bend_pipe_prep(struct soltab *stp, struct bu_list *head, fastf_t *bend_center, fastf_t *bend_start, fastf_t *bend_end, fastf_t bend_radius, fastf_t bend_angle, fastf_t *v1, fastf_t *v2, fastf_t od, fastf_t id)
{
	register struct bend_pipe *pipe;
	LOCAL vect_t	to_start,to_end;
	LOCAL mat_t	R;
	LOCAL point_t	work;
	LOCAL vect_t	tmp_vec;
	LOCAL fastf_t	f;

	pipe = (struct bend_pipe *)bu_malloc( sizeof( struct bend_pipe ), "rt_bend_pipe_prep:pipe" )	 ;

	pipe->pipe_is_bend = 1;
	pipe->bend_or = od * 0.5;
	pipe->bend_ir = id * 0.5;

	VMOVE( pipe->bend_start, bend_start );
	VMOVE( pipe->bend_end, bend_end );
	VMOVE( pipe->bend_V, bend_center );
	VSUB2( to_start, bend_start, bend_center );
	pipe->bend_radius = bend_radius;
	VSUB2( to_end, bend_end, bend_center );
	VSCALE( pipe->bend_ra, to_start, 1.0/pipe->bend_radius );
	VCROSS( pipe->bend_N, to_start, to_end );
	VUNITIZE( pipe->bend_N );
	VCROSS( pipe->bend_rb, pipe->bend_N, pipe->bend_ra );

	pipe->bend_angle = bend_angle;

	/* angle goes from 0.0 at start to some angle less than PI */
	if( pipe->bend_angle >= bn_pi )
	{
		bu_log( "Error: rt_pipe_prep: Bend section bends through more than 180 degrees\n" );
		return( 1 );
	}

	pipe->bend_alpha_i = pipe->bend_ir/pipe->bend_radius;
	pipe->bend_alpha_o = pipe->bend_or/pipe->bend_radius;

	pipe->bend_R_SQ = (pipe->bend_radius + pipe->bend_or) *
				(pipe->bend_radius + pipe->bend_or);

	MAT_IDN( R );
	VMOVE( &R[0], pipe->bend_ra );
	VMOVE( &R[4], pipe->bend_rb );
	VMOVE( &R[8], pipe->bend_N );

	if (bn_mat_inverse( pipe->bend_invR, R ) == 0) {
		bu_free(pipe, "rt_bend_pipe_prep:pipe");
		return 0; /* there is nothing to bend, that's OK */
	}


	MAT_COPY( pipe->bend_SoR, R );
	pipe->bend_SoR[15] *= pipe->bend_radius;

	/* bounding box for entire torus */
	/* X */
	VSET( tmp_vec, 1.0, 0.0, 0.0 );
	VCROSS( work, pipe->bend_N, tmp_vec );
	f = pipe->bend_or + pipe->bend_radius * MAGNITUDE(work);
	pipe->bend_min[X] = pipe->bend_V[X] - f;
	pipe->bend_max[X] = pipe->bend_V[X] + f;

	/* Y */
	VSET( tmp_vec, 0.0, 1.0, 0.0 );
	VCROSS( work, pipe->bend_N, tmp_vec );
	f = pipe->bend_or + pipe->bend_radius * MAGNITUDE(work);
	pipe->bend_min[Y] = pipe->bend_V[Y] - f;
	pipe->bend_max[Y] = pipe->bend_V[Y] + f;

	/* Z */
	VSET( tmp_vec, 0.0, 0.0, 1.0 );
	VCROSS( work, pipe->bend_N, tmp_vec );
	f = pipe->bend_or + pipe->bend_radius * MAGNITUDE(work);
	pipe->bend_min[Z] = pipe->bend_V[Z] - f;
	pipe->bend_max[Z] = pipe->bend_V[Z] + f;

	PIPE_MM( pipe->bend_min );
	PIPE_MM( pipe->bend_max );

	BU_LIST_INSERT( head, &pipe->l );

	return( 0 );

}

HIDDEN void
rt_linear_pipe_prep(struct soltab *stp, struct bu_list *head, fastf_t *pt1, fastf_t id1, fastf_t od1, fastf_t *pt2, fastf_t id2, fastf_t od2)
{
	register struct lin_pipe *pipe;
	LOCAL mat_t	R;
	LOCAL mat_t	Rinv;
	LOCAL mat_t	S;
	LOCAL point_t work;
	LOCAL vect_t seg_ht;
	LOCAL vect_t v1,v2;

	pipe = (struct lin_pipe *)bu_malloc( sizeof( struct lin_pipe ), "rt_bend_pipe_prep:pipe" );
	BU_LIST_INSERT( head, &pipe->l );


	VMOVE( pipe->pipe_V, pt1 );

	VSUB2( seg_ht, pt2, pt1 );
	pipe->pipe_ribase = id1/2.0;
	pipe->pipe_ribase_sq = pipe->pipe_ribase * pipe->pipe_ribase;
	pipe->pipe_ritop = id2/2.0;
	pipe->pipe_ritop_sq = pipe->pipe_ritop * pipe->pipe_ritop;
	pipe->pipe_robase = od1/2.0;
	pipe->pipe_robase_sq = pipe->pipe_robase * pipe->pipe_robase;
	pipe->pipe_rotop = od2/2.0;
	pipe->pipe_rotop_sq = pipe->pipe_rotop * pipe->pipe_rotop;
	pipe->pipe_ridiff = pipe->pipe_ritop - pipe->pipe_ribase;
	pipe->pipe_ridiff_sq = pipe->pipe_ridiff * pipe->pipe_ridiff;
	pipe->pipe_rodiff = pipe->pipe_rotop - pipe->pipe_robase;
	pipe->pipe_rodiff_sq = pipe->pipe_rodiff * pipe->pipe_rodiff;
	pipe->pipe_is_bend = 0;

	pipe->pipe_len = MAGNITUDE( seg_ht );
	VSCALE( seg_ht, seg_ht, 1.0/pipe->pipe_len );
	VMOVE( pipe->pipe_H, seg_ht );
	bn_vec_ortho( v1, seg_ht );
	VCROSS( v2, seg_ht, v1 );

	/* build R matrix */
	MAT_IDN( R );
	VMOVE( &R[0], v1 );
	VMOVE( &R[4], v2 );
	VMOVE( &R[8], seg_ht );

	/* Rinv is transpose */
	bn_mat_trn( Rinv, R );

	/* Build Scale matrix */
	MAT_IDN( S );
	S[10] = 1.0/pipe->pipe_len;

	/* Compute SoR and invRoS */
	bn_mat_mul( pipe->pipe_SoR, S, R );
	bn_mat_mul( pipe->pipe_invRoS, Rinv, S );



	VJOIN2( work, pt1, od1, v1, od1, v2 );
	PIPE_MM( work )
	VJOIN2( work, pt1, -od1, v1, od1, v2 );
	PIPE_MM( work )
	VJOIN2( work, pt1, od1, v1, -od1, v2 );
	PIPE_MM( work )
	VJOIN2( work, pt1, -od1, v1, -od1, v2 );
	PIPE_MM( work )

	VJOIN2( work, pt2, od2, v1, od2, v2 );
	PIPE_MM( work )
	VJOIN2( work, pt2, -od2, v1, od2, v2 );
	PIPE_MM( work )
	VJOIN2( work, pt2, od2, v1, -od2, v2 );
	PIPE_MM( work )
	VJOIN2( work, pt2, -od2, v1, -od2, v2 );
	PIPE_MM( work )

}

/**
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
 *  	A struct bu_list is created, and it's address is stored in
 *  	stp->st_specific for use by pipe_shot().
 */
int
rt_pipe_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	register struct bu_list *head;
	struct rt_pipe_internal	*pip;
	struct wdb_pipept *pp1,*pp2,*pp3;
	point_t curr_pt;
	fastf_t curr_id, curr_od;
	fastf_t dx,dy,dz,f;

	RT_CK_DB_INTERNAL( ip );
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	head = (struct bu_list *)bu_malloc( sizeof( struct bu_list ), "rt_pipe_prep:head" );
	stp->st_specific = (genptr_t)head;
	BU_LIST_INIT( head );

	if( BU_LIST_IS_EMPTY( &(pip->pipe_segs_head) ) )
		return( 0 );

	pp1 = BU_LIST_FIRST( wdb_pipept, &(pip->pipe_segs_head) );
	pp2 = BU_LIST_NEXT( wdb_pipept, &pp1->l );
	if( BU_LIST_IS_HEAD( &pp2->l, &(pip->pipe_segs_head) ) )
		return( 0 );
	pp3 = BU_LIST_NEXT( wdb_pipept, &pp2->l );
	if( BU_LIST_IS_HEAD( &pp3->l ,  &(pip->pipe_segs_head) ) )
		pp3 = (struct wdb_pipept *)NULL;

	VMOVE( curr_pt, pp1->pp_coord );
	curr_od = pp1->pp_od;
	curr_id = pp1->pp_id;
	while( 1 )
	{
		vect_t n1,n2;
		vect_t norm;
		vect_t v1,v2;
		fastf_t angle;
		fastf_t dist_to_bend;
		point_t bend_start, bend_end, bend_center;

		VSUB2( n1, curr_pt, pp2->pp_coord );
		if( VNEAR_ZERO( n1, SQRT_SMALL_FASTF ) )
		{
			/* duplicate point, skip to next point */
			goto next_pt;
		}

		if( !pp3 )
		{
			/* last segment */
			rt_linear_pipe_prep( stp, head, curr_pt, curr_id, curr_od, pp2->pp_coord, pp2->pp_id, pp2->pp_od );
			break;
		}

		VSUB2( n2, pp3->pp_coord, pp2->pp_coord );
		VCROSS( norm, n1, n2 );
		VUNITIZE( n1 );
		VUNITIZE( n2 );
		angle = bn_pi - acos( VDOT( n1, n2 ) );
		dist_to_bend = pp2->pp_bendradius * tan( angle/2.0 );
		if( isnan( dist_to_bend ) || VNEAR_ZERO( norm, SQRT_SMALL_FASTF) || NEAR_ZERO( dist_to_bend, SQRT_SMALL_FASTF) )
		{
			/* points are colinear, treat as a linear segment */
			rt_linear_pipe_prep( stp, head, curr_pt, curr_id, curr_od,
				pp2->pp_coord, pp2->pp_id, pp2->pp_od );
			VMOVE( curr_pt, pp2->pp_coord );
			goto next_pt;
		}

		VJOIN1( bend_start, pp2->pp_coord, dist_to_bend, n1 );
		VJOIN1( bend_end, pp2->pp_coord, dist_to_bend, n2 );

		VUNITIZE( norm );

		/* linear section */
		rt_linear_pipe_prep( stp, head, curr_pt, curr_id, curr_od,
				bend_start, pp2->pp_id, pp2->pp_od );

		/* and bend section */
		VCROSS( v1, n1, norm );
		VCROSS( v2, v1, norm );
		VJOIN1( bend_center, bend_start, -pp2->pp_bendradius, v1 );
		rt_bend_pipe_prep( stp, head, bend_center, bend_start, bend_end, pp2->pp_bendradius, angle,
			v1, v2, pp2->pp_od, pp2->pp_id );

		VMOVE( curr_pt, bend_end );
next_pt:
		if (!pp3) break;
		curr_id = pp2->pp_id;
		curr_od = pp2->pp_od;
		pp1 = pp2;
		pp2 = pp3;
		pp3 = BU_LIST_NEXT( wdb_pipept, &pp3->l );
		if( BU_LIST_IS_HEAD( &pp3->l ,  &(pip->pipe_segs_head) ) )
			pp3 = (struct wdb_pipept *)NULL;
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

/**
 *			R T _ P I P E _ P R I N T
 */
void
rt_pipe_print(register const struct soltab *stp)
{
/*	register struct bu_list *pipe =
		(struct bu_list *)stp->st_specific; */
}

/**
 *			R T _ P I P E P T _ P R I N T
 */
void
rt_pipept_print( const struct wdb_pipept *pipe, double mm2local )
{
	point_t p1;

	bu_log( "Pipe Vertex:\n" );
	VSCALE( p1, pipe->pp_coord, mm2local );
	bu_log( "\tat (%g %g %g)\n", V3ARGS( p1 ) );
	bu_log( "\tbend radius = %g\n", pipe->pp_bendradius*mm2local );
	if( pipe->pp_id > 0.0 )
		bu_log( "\tod=%g, id=%g\n",
			pipe->pp_od*mm2local,
			pipe->pp_id*mm2local );
	else
		bu_log( "\tod=%g\n", pipe->pp_od*mm2local );
}

/**
 *			R T _ V L S _ P I P E P T
 */
void
rt_vls_pipept(
	struct bu_vls *vp,
	int seg_no,
	const struct rt_db_internal *ip,
	double mm2local)
{
	struct rt_pipe_internal *pint;
	struct wdb_pipept *pipe;
	int seg_count=0;
	char buf[256];
	point_t p1;

	pint = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC( pint );

	pipe = BU_LIST_FIRST( wdb_pipept, &pint->pipe_segs_head );
	while( ++seg_count != seg_no && BU_LIST_NOT_HEAD( &pipe->l, &pint->pipe_segs_head ) )
		pipe = BU_LIST_NEXT( wdb_pipept, &pipe->l );


	sprintf( buf, "Pipe Vertex:\n" );
	bu_vls_strcat( vp, buf );
	VSCALE( p1, pipe->pp_coord, mm2local );
	sprintf( buf, "\tat (%g %g %g)\n", V3ARGS( p1 ) );
	bu_vls_strcat( vp, buf );
	sprintf( buf, "\tbend radius = %g\n", pipe->pp_bendradius*mm2local );
	bu_vls_strcat( vp, buf );
	if( pipe->pp_id > 0.0 )
		sprintf( buf, "\tod=%g, id=%g\n",
			pipe->pp_od*mm2local,
			pipe->pp_id*mm2local );
	else
		sprintf( buf, "\tod=%g\n", pipe->pp_od*mm2local );
	bu_vls_strcat( vp, buf );
}

HIDDEN void
bend_pipe_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead, struct bend_pipe *pipe, struct hit_list *hit_headp, int *hit_count, int seg_no)
{
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL vect_t	work;		/* temporary vector */
	LOCAL bn_poly_t	C;		/* The final equation */
	LOCAL bn_complex_t	val[4];	/* The complex roots */
	LOCAL int	j;
	LOCAL int	root_count=0;
	LOCAL bn_poly_t	A, Asqr;
	LOCAL bn_poly_t	X2_Y2;		/* X**2 + Y**2 */
	LOCAL vect_t	cor_pprime;	/* new ray origin */
	LOCAL fastf_t	cor_proj;

	*hit_count = 0;

	if( !rt_in_rpp( rp, ap->a_inv_dir, pipe->bend_min, pipe->bend_max ) ) {
		return;		 /* miss */
	}

	/* Convert vector into the space of the unit torus */
	MAT4X3VEC( dprime, pipe->bend_SoR, rp->r_dir );
	VUNITIZE( dprime );

	VSUB2( work, rp->r_pt, pipe->bend_V );
	MAT4X3VEC( pprime, pipe->bend_SoR, work );

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
		  1.0 - pipe->bend_alpha_o * pipe->bend_alpha_o;

	/* Inline expansion of (void) rt_poly_mul( &A, &A, &Asqr ) */
	/* Both polys have degree two */
	Asqr.dgr = 4;
	Asqr.cf[0] = A.cf[0] * A.cf[0];
	Asqr.cf[1] = A.cf[0] * A.cf[1] + A.cf[1] * A.cf[0];
	Asqr.cf[2] = A.cf[0] * A.cf[2] + A.cf[1] * A.cf[1] + A.cf[2] * A.cf[0];
	Asqr.cf[3] = A.cf[1] * A.cf[2] + A.cf[2] * A.cf[1];
	Asqr.cf[4] = A.cf[2] * A.cf[2];

	/* Inline expansion of bn_poly_scale( &X2_Y2, 4.0 ) and
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
	if ( (root_count = rt_poly_roots( &C, val, stp->st_dp->d_namep )) != 4 ){
	    if( root_count > 0 )  {
		bu_log("tor:  rt_poly_roots() 4!=%d\n", root_count);
		bn_pr_roots( stp->st_name, val, root_count );
	    } else if (root_count < 0) {
		static int reported=0;
		bu_log("The root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
		if (!reported) {
		    VPRINT("while shooting from:\t", rp->r_pt);
		    VPRINT("while shooting at:\t", rp->r_dir);
		    bu_log("Additional pipe convergence failure details will be suppressed.\n");
		    reported=1;
		}
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
			dist = normalized_dist * pipe->bend_radius;

			/* check if this hit is within bend angle */
			VJOIN1( hit_pt, rp->r_pt, dist, rp->r_dir );
			VSUB2( to_hit, hit_pt, pipe->bend_V );
			angle = atan2( VDOT( to_hit, pipe->bend_rb ), VDOT( to_hit, pipe->bend_ra ) );
			if( angle < 0.0 )
				angle += 2.0 * bn_pi;
			if( angle <= pipe->bend_angle )
			{
				BU_GETSTRUCT( hitp, hit_list );
				BU_GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hitp->hit_dist = dist;
				VJOIN1( hitp->hitp->hit_vpriv, pprime, normalized_dist, dprime );
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_BEND_OUTER_BODY;
				BU_LIST_INSERT( &hit_headp->l, &hitp->l );
				(*hit_count)++;
			}
		}
	}

	if( pipe->bend_alpha_i <= 0.0 )
		return;		/* no inner torus */

	/* Now do inner torus */
	A.cf[2] = X2_Y2.cf[2] + cor_pprime[Z] * cor_pprime[Z] +
		  1.0 - pipe->bend_alpha_i * pipe->bend_alpha_i;

	/* Inline expansion of (void) rt_poly_mul( &A, &A, &Asqr ) */
	/* Both polys have degree two */
	Asqr.dgr = 4;
	Asqr.cf[0] = A.cf[0] * A.cf[0];
	Asqr.cf[1] = A.cf[0] * A.cf[1] + A.cf[1] * A.cf[0];
	Asqr.cf[2] = A.cf[0] * A.cf[2] + A.cf[1] * A.cf[1] + A.cf[2] * A.cf[0];
	Asqr.cf[3] = A.cf[1] * A.cf[2] + A.cf[2] * A.cf[1];
	Asqr.cf[4] = A.cf[2] * A.cf[2];

	/* Inline expansion of bn_poly_scale( &X2_Y2, 4.0 ) and
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
	if ( (root_count = rt_poly_roots( &C, val, stp->st_dp->d_namep)) != 4 ){
	    if( root_count > 0 )  {
		bu_log("tor:  rt_poly_roots() 4!=%d\n", root_count);
		bn_pr_roots( stp->st_name, val, root_count );
	    } else if (root_count < 0) {
		static int reported=0;
		bu_log("The root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
		if (!reported) {
		    VPRINT("while shooting from:\t", rp->r_pt);
		    VPRINT("while shooting at:\t", rp->r_dir);
		    bu_log("Additional pipe convergence failure details will be suppressed.\n");
		    reported=1;
		}
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
			dist = normalized_dist * pipe->bend_radius;

			/* check if this hit is within bend angle */
			VJOIN1( hit_pt, rp->r_pt, dist, rp->r_dir );
			VSUB2( to_hit, hit_pt, pipe->bend_V );
			angle = atan2( VDOT( to_hit, pipe->bend_rb ), VDOT( to_hit, pipe->bend_ra ) );
			if( angle < 0.0 )
				angle += 2.0 * bn_pi;
			if( angle <= pipe->bend_angle )
			{
				BU_GETSTRUCT( hitp, hit_list );
				BU_GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hitp->hit_dist = dist;
				VJOIN1( hitp->hitp->hit_vpriv, pprime, normalized_dist, dprime );
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_BEND_INNER_BODY;
				BU_LIST_INSERT( &hit_headp->l, &hitp->l );
				root_count++;
			}
		}
	}

	*hit_count += root_count;

	return;

}

HIDDEN void
linear_pipe_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead, struct lin_pipe *pipe, struct hit_list *hit_headp, int *hit_count, int seg_no)
{
	LOCAL struct hit_list	*hitp;
	LOCAL point_t	work_pt;
	LOCAL point_t	ray_start;
	LOCAL vect_t	ray_dir;
	LOCAL double	t_tmp;
	LOCAL double	a,b,c;
	LOCAL double	descrim;

	if( pipe->pipe_is_bend )
	{
		bu_log( "linear_pipe_shot called for pipe bend\n" );
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
			BU_GETSTRUCT( hitp, hit_list );
			BU_GETSTRUCT( hitp->hitp, hit );
			hitp->hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hitp->hit_dist = t_tmp;
			hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_OUTER_BODY;
			VMOVE( hitp->hitp->hit_vpriv, hit_pt );
			hitp->hitp->hit_vpriv[Z] = (-pipe->pipe_robase - hit_pt[Z] * pipe->pipe_rodiff) *
					pipe->pipe_rodiff;
			(*hit_count)++;
			BU_LIST_INSERT( &hit_headp->l, &hitp->l );
		}

		t_tmp = (-b + sqrt_descrim)/(2.0*a);
		VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
		if( hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0 )
		{
			BU_GETSTRUCT( hitp, hit_list );
			BU_GETSTRUCT( hitp->hitp, hit );
			hitp->hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hitp->hit_dist = t_tmp;
			hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_OUTER_BODY;
			VMOVE( hitp->hitp->hit_vpriv, hit_pt );
			hitp->hitp->hit_vpriv[Z] = (-pipe->pipe_robase - hit_pt[Z] * pipe->pipe_rodiff) *
					pipe->pipe_rodiff;
			(*hit_count)++;
			BU_LIST_INSERT( &hit_headp->l, &hitp->l );
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
				BU_GETSTRUCT( hitp, hit_list );
				BU_GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_INNER_BODY;
				VMOVE( hitp->hitp->hit_vpriv, hit_pt );
				hitp->hitp->hit_vpriv[Z] = (-pipe->pipe_ribase - hit_pt[Z] * pipe->pipe_ridiff) *
					pipe->pipe_ridiff;
				(*hit_count)++;
				BU_LIST_INSERT( &hit_headp->l, &hitp->l );
			}

			t_tmp = (-b + sqrt_descrim)/(2.0*a);
			VJOIN1( hit_pt, ray_start, t_tmp, ray_dir );
			if( hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0 )
			{
				BU_GETSTRUCT( hitp, hit_list );
				BU_GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_INNER_BODY;
				VMOVE( hitp->hitp->hit_vpriv, hit_pt );
				hitp->hitp->hit_vpriv[Z] = (-pipe->pipe_ribase - hit_pt[Z] * pipe->pipe_ridiff) *
					pipe->pipe_ridiff;
				(*hit_count)++;
				BU_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}

}

HIDDEN void
pipe_start_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead, struct id_pipe *pipe, struct hit_list *hit_headp, int *hit_count, int seg_no)
{
	point_t hit_pt;
	fastf_t t_tmp;
	fastf_t radius_sq;
	struct hit_list *hitp;

	*hit_count = 0;

	if( !pipe->pipe_is_bend )
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
				BU_GETSTRUCT( hitp, hit_list );
				BU_GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_BASE;
				(*hit_count)++;
				BU_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}
	else if( pipe->pipe_is_bend )
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
				BU_GETSTRUCT( hitp, hit_list );
				BU_GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_BEND_BASE;
				(*hit_count)++;
				BU_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}
}

HIDDEN void
pipe_end_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead, struct id_pipe *pipe, struct hit_list *hit_headp, int *hit_count, int seg_no)
{
	point_t hit_pt;
	fastf_t t_tmp;
	fastf_t radius_sq;
	struct hit_list *hitp;

	*hit_count = 0;

	if( !pipe->pipe_is_bend )
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
				BU_GETSTRUCT( hitp, hit_list );
				BU_GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_LINEAR_TOP;
				(*hit_count)++;
				BU_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}
	else if( pipe->pipe_is_bend )
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
				BU_GETSTRUCT( hitp, hit_list );
				BU_GETSTRUCT( hitp->hitp, hit );
				hitp->hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hitp->hit_dist = t_tmp;
				hitp->hitp->hit_surfno = seg_no*10 + PIPE_BEND_TOP;
				(*hit_count)++;
				BU_LIST_INSERT( &hit_headp->l, &hitp->l );
			}
		}
	}
}

HIDDEN void
rt_pipe_hitsort(struct hit_list *h, int *nh, register struct xray *rp, struct soltab *stp)
{
	struct hit_list *hitp;
	struct hit_list *first;
	struct hit_list *second;
	struct hit_list *prev;
	struct hit_list *next_hit;

	hitp = BU_LIST_FIRST( hit_list, &h->l );
	while( BU_LIST_NEXT_NOT_HEAD( &hitp->l, &h->l ) )
	{
		struct hit_list *next_hit;
		struct hit_list *prev_hit;

		next_hit = BU_LIST_NEXT( hit_list, &hitp->l );
		if( hitp->hitp->hit_dist > next_hit->hitp->hit_dist )
		{
			struct hit_list *tmp;

			if( hitp == BU_LIST_FIRST( hit_list, &h->l ) )
				prev_hit = (struct hit_list *)NULL;
			else
				prev_hit = BU_LIST_PREV( hit_list, &hitp->l );

			/* move this hit to the end of the list */
			tmp = hitp;
			BU_LIST_DEQUEUE( &tmp->l );
			BU_LIST_INSERT( &h->l, &tmp->l );

			if( prev_hit )
				hitp = prev_hit;
			else
				hitp = BU_LIST_FIRST( hit_list, &h->l );
		}
		else
			hitp = next_hit;
	}

	/* delete duplicate hits */
	hitp = BU_LIST_FIRST( hit_list, &h->l );
	while( BU_LIST_NEXT_NOT_HEAD( &hitp->l, &h->l ) )
	{
		next_hit = BU_LIST_NEXT( hit_list, &hitp->l );

		if( NEAR_ZERO( hitp->hitp->hit_dist - next_hit->hitp->hit_dist, 0.00001) &&
			       hitp->hitp->hit_surfno == next_hit->hitp->hit_surfno )
		{
			struct hit_list *tmp;

			tmp = hitp;
			hitp = next_hit;
			BU_LIST_DEQUEUE( &tmp->l );
			bu_free( (char *)tmp->hitp, "rt_pipe_hitsort: tmp->hitp" );
			bu_free( (char *)tmp, "rt_pipe_hitsort: tmp" );
			(*nh)--;
			tmp = hitp;
			next_hit = BU_LIST_NEXT( hit_list, &hitp->l );
			hitp = next_hit;
			BU_LIST_DEQUEUE( &tmp->l );
			bu_free( (char *)tmp->hitp, "rt_pipe_hitsort: tmp->hitp" );
			bu_free( (char *)tmp, "rt_pipe_hitsort: tmp" );
			(*nh)--;
			if( BU_LIST_IS_HEAD( &hitp->l, &h->l ) )
				break;
		}
		else
			hitp = next_hit;
	}

	if( *nh == 1 )
	{
		while( BU_LIST_WHILE( hitp, hit_list, &h->l ) )
		{
			BU_LIST_DEQUEUE( &hitp->l );
			bu_free( (char *)hitp->hitp, "pipe_hitsort: hitp->hitp" );
			bu_free( (char *)hitp, "pipe_hitsort: hitp" );
		}
		(*nh) = 0;
		return;
	}

	if( *nh == 0 || *nh == 2 )
		return;

	/* handle cases where this pipe overlaps with itself */
	first = BU_LIST_FIRST( hit_list, &h->l );
	if( VDOT( first->hitp->hit_normal, rp->r_dir ) > 0.0 )
	{

		bu_log( "ERROR: first hit on %s (surfno = %d) is an exit at (%g %g %g)\n",
			stp->st_dp->d_namep, first->hitp->hit_surfno, V3ARGS( first->hitp->hit_point ) );
		bu_log( "\tray start = (%.12e %.12e %.12e), ray dir = (%.12e %.12e %.12e)\n",
			V3ARGS( rp->r_pt ), V3ARGS( rp->r_dir ) );

		while( BU_LIST_WHILE( hitp, hit_list, &h->l ) )
		{
			BU_LIST_DEQUEUE( &hitp->l );
			bu_free( (char *)hitp->hitp, "pipe_hitsort: hitp->hitp" );
			bu_free( (char *)hitp, "pipe_hitsort: hitp" );
		}
		(*nh) = 0;
		return;
	}

	while( BU_LIST_NOT_HEAD( &first->l, &h->l ) )
	{
		second = BU_LIST_NEXT( hit_list, &first->l );
		if( BU_LIST_IS_HEAD( &second->l, &h->l ) )
			break;

		while( BU_LIST_NOT_HEAD( &second->l, &h->l ) && VDOT( second->hitp->hit_normal, rp->r_dir ) < 0.0 )
		{
			prev = second;
			second = BU_LIST_NEXT( hit_list, &second->l );
			if( BU_LIST_NOT_HEAD( &second->l, &h->l ) )
			{
				BU_LIST_DEQUEUE( &prev->l );
				bu_free( (char *)prev->hitp, "pipe_hitsort: prev->hitp" );
				bu_free( (char *)prev, "pipe_hitsort: prev" );
				(*nh)--;
			}
		}
		prev = NULL;
		while( BU_LIST_NOT_HEAD( &second->l, &h->l ) && VDOT( second->hitp->hit_normal, rp->r_dir ) > 0.0 )
		{
			if( prev )
			{
				BU_LIST_DEQUEUE( &prev->l );
				bu_free( (char *)prev->hitp, "pipe_hitsort: prev->hitp" );
				bu_free( (char *)prev, "pipe_hitsort: prev" );
				(*nh)--;
			}
			prev = second;
			second = BU_LIST_NEXT( hit_list, &second->l );
		}
		first = second;
	}
}

/**
 *  			R T _ P I P E _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_pipe_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	register struct bu_list		*pipe =
		(struct bu_list *)stp->st_specific;
	register struct id_pipe		*pipe_id;
	register struct lin_pipe	*pipe_lin;
	register struct bend_pipe	*pipe_bend;
	LOCAL fastf_t	w;
	LOCAL vect_t	work;
	LOCAL vect_t	work1;
	LOCAL int	segno;
	LOCAL int	i;

	segno = hitp->hit_surfno/10;

	pipe_id = BU_LIST_FIRST( id_pipe, pipe );
	for( i=1 ; i<segno ; i++ )
		pipe_id = BU_LIST_NEXT( id_pipe, &pipe_id->l );

	pipe_lin = (struct lin_pipe *)pipe_id;
	pipe_bend = (struct bend_pipe *)pipe_id;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	switch( hitp->hit_surfno%10 )
	{
		case PIPE_LINEAR_TOP:
			VMOVE( hitp->hit_normal, pipe_lin->pipe_H );
			break;
		case PIPE_LINEAR_BASE:
			VREVERSE( hitp->hit_normal, pipe_lin->pipe_H );
			break;
		case PIPE_LINEAR_OUTER_BODY:
			MAT4X3VEC( hitp->hit_normal, pipe_lin->pipe_invRoS, hitp->hit_vpriv );
			VUNITIZE( hitp->hit_normal );
			break;
		case PIPE_LINEAR_INNER_BODY:
			MAT4X3VEC( hitp->hit_normal, pipe_lin->pipe_invRoS, hitp->hit_vpriv );
			VUNITIZE( hitp->hit_normal );
			VREVERSE( hitp->hit_normal, hitp->hit_normal );
			break;
		case PIPE_BEND_OUTER_BODY:
			w = hitp->hit_vpriv[X]*hitp->hit_vpriv[X] +
			    hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] +
			    hitp->hit_vpriv[Z]*hitp->hit_vpriv[Z] +
			    1.0 - pipe_bend->bend_alpha_o*pipe_bend->bend_alpha_o;
			VSET( work,
				( w - 2.0 ) * hitp->hit_vpriv[X],
				( w - 2.0 ) * hitp->hit_vpriv[Y],
				  w * hitp->hit_vpriv[Z] );
			VUNITIZE( work );
			MAT3X3VEC( hitp->hit_normal, pipe_bend->bend_invR, work );
			break;
		case PIPE_BEND_INNER_BODY:
			w = hitp->hit_vpriv[X]*hitp->hit_vpriv[X] +
			    hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] +
			    hitp->hit_vpriv[Z]*hitp->hit_vpriv[Z] +
			    1.0 - pipe_bend->bend_alpha_o*pipe_bend->bend_alpha_o;
			VSET( work,
				( w - 2.0 ) * hitp->hit_vpriv[X],
				( w - 2.0 ) * hitp->hit_vpriv[Y],
				  w * hitp->hit_vpriv[Z] );
			VUNITIZE( work );
			MAT3X3VEC( work1, pipe_bend->bend_invR, work );
			VREVERSE( hitp->hit_normal, work1 );
			break;
		case PIPE_BEND_BASE:
			VREVERSE( hitp->hit_normal, pipe_bend->bend_rb );
			break;
		case PIPE_BEND_TOP:
			VSUB2( work, pipe_bend->bend_end, pipe_bend->bend_V );
			VCROSS( hitp->hit_normal, pipe_bend->bend_N, work );
			VUNITIZE( hitp->hit_normal );
			break;
		default:
			bu_log( "rt_pipe_norm: Unrecognized surfno (%d)\n", hitp->hit_surfno );
			break;
	}
}

/**
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
rt_pipe_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register struct bu_list		*head =
		(struct bu_list *)stp->st_specific;
	register struct id_pipe		*pipe_id;
	register struct seg		*segp;
	LOCAL struct hit_list		hit_head;
	LOCAL struct hit_list		*hitp;
	LOCAL int			hit_count;
	LOCAL int			total_hits;
	LOCAL int			seg_no;
	LOCAL int			i;

	BU_LIST_INIT( &hit_head.l );

	pipe_start_shot( stp, rp, ap, seghead, BU_LIST_FIRST( id_pipe, head ),
		&hit_head, &total_hits, 1 );
	seg_no = 0;
	for( BU_LIST_FOR( pipe_id, id_pipe, head ) )
		seg_no++;
	pipe_end_shot( stp, rp, ap, seghead, BU_LIST_LAST( id_pipe, head ),
		&hit_head, &hit_count, seg_no );
	total_hits += hit_count;

	seg_no = 0;
	for( BU_LIST_FOR( pipe_id, id_pipe, head ) )
	{
		seg_no++;

		if( !pipe_id->pipe_is_bend )
		{
			linear_pipe_shot( stp, rp, ap, seghead, (struct lin_pipe *)pipe_id,
				&hit_head, &hit_count, seg_no );
			total_hits += hit_count;
		}
		else
		{
			bend_pipe_shot( stp, rp, ap, seghead, (struct bend_pipe *)pipe_id,
				&hit_head, &hit_count, seg_no );
			total_hits += hit_count;
		}
	}
	if( !total_hits )
		return( 0 );

	/* calculate hit points and normals */
	for( BU_LIST_FOR( hitp, hit_list, &hit_head.l ) )
		rt_pipe_norm( hitp->hitp, stp , rp );

	rt_pipe_hitsort( &hit_head, &total_hits, rp, stp );

	/* Build segments */
	if( total_hits%2 )
	{
		i = 0;
		bu_log( "rt_pipe_shot: bad number of hits on solid %s (%d)\n", stp->st_dp->d_namep, total_hits );
		bu_log( "Ignoring this solid for this ray\n" );
		bu_log( "\tray start = (%e %e %e), ray dir = (%e %e %e)\n", V3ARGS( rp->r_pt ), V3ARGS( rp->r_dir ) );
		for( BU_LIST_FOR( hitp, hit_list, &hit_head.l ) )
		{
			point_t hit_pt;

			bu_log( "#%d, dist = %g, surfno=%d\n" , ++i, hitp->hitp->hit_dist, hitp->hitp->hit_surfno );
			VJOIN1( hit_pt, rp->r_pt, hitp->hitp->hit_dist,  rp->r_dir );
			bu_log( "\t( %g %g %g )\n" , V3ARGS( hit_pt ) );
		}

		/* free the list of hits */
		while( BU_LIST_WHILE( hitp, hit_list, &hit_head.l ) )
		{
			BU_LIST_DEQUEUE( &hitp->l );
			bu_free( (char *)hitp->hitp, "rt_pipe_shot: hitp->hitp" );
			bu_free( (char *)hitp, "rt_pipe_shot: hitp" );
		}

		return( 0 );
	}

	hitp = BU_LIST_FIRST( hit_list, &hit_head.l );
	while( BU_LIST_NOT_HEAD( &hitp->l, &hit_head.l ) )
	{
		struct hit_list *next;

		next = BU_LIST_NEXT( hit_list, &hitp->l );

		RT_GET_SEG(segp, ap->a_resource);

		segp->seg_stp = stp;
		segp->seg_in = (*hitp->hitp);
		segp->seg_out = (*next->hitp);

		BU_LIST_INSERT( &(seghead->l), &(segp->l) );

		hitp = BU_LIST_NEXT( hit_list, &next->l );
	}

	/* free the list of hits */
	while( BU_LIST_WHILE( hitp, hit_list, &hit_head.l ) )
	{
		BU_LIST_DEQUEUE( &hitp->l );
		bu_free( (char *)hitp->hitp, "rt_pipe_shot: hitp->hitp" );
		bu_free( (char *)hitp, "rt_pipe_shot: hitp" );
	}

	if( total_hits )
		return( 1 );		/* HIT */
	else
		return(0);		/* MISS */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;

/**
 *			R T_ P I P E _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_pipe_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		  	       /* Number of ray/object pairs */

{
	rt_vstub( stp, rp, segp, n, ap );
}

/**
 *			R T _ P I P E _ C U R V E
 *
 *  Return the curvature of the pipe.
 */
void
rt_pipe_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
/*	register struct bu_list *pipe =
		(struct bu_list *)stp->st_specific; */

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/**
 *  			R T _ P I P E _ U V
 *
 *  For a hit on the surface of an pipe, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_pipe_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
/*	register struct bu_list *pipe =
		(struct bu_list *)stp->st_specific; */
}

/**
 *		R T _ P I P E _ F R E E
 */
void
rt_pipe_free(register struct soltab *stp)
{
#if 0
	register struct bu_list *pipe =
		(struct bu_list *)stp->st_specific;

	/* free linked list */
	while( BU_LIST_NON_EMPTY( &pipe->id.l ) )
	{
		register struct bu_list *pipe_ptr;

		pipe_ptr = (struct bu_list *)(&pipe->id.l)->forw;
		bu_free( (char *)pipe_ptr, "pipe_specific" );
	}

	/* free list head */
	bu_free( (char *)pipe, "pipe_specific head" );
#endif
}

/**
 *			R T _ P I P E _ C L A S S
 */
int
rt_pipe_class(void)
{
	return(0);
}

/**			D R A W _ P I P E _ A R C
 *
 * v1 and v2 must be unit vectors normal to each other in plane of circle
 * v1 must be in direction from center to start point (unless a full circle is
 * requested). "End" is the endpoint of arc. "Seg_count"
 * is how many straight line segements to use to draw the arc. "Full_circle"
 * is a flag to indicate that a complete circle is desired.
 */

HIDDEN void
draw_pipe_arc(struct bu_list *vhead, fastf_t radius, fastf_t *center, const fastf_t *v1, const fastf_t *v2, fastf_t *end, int seg_count, int full_circle)
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
		delta_ang = 2.0*bn_pi/seg_count;

	cos_del = cos( delta_ang );
	sin_del = sin( delta_ang );

	x = radius;
	y = 0.0;
	VJOIN2( pt, center, x, v1, y, v2 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
	for( i=0 ; i<seg_count ; i++ )
	{
		xnew = x*cos_del - y*sin_del;
		ynew = x*sin_del + y*cos_del;
		VJOIN2( pt, center, xnew, v1, ynew, v2 );
		RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
		x = xnew;
		y = ynew;
	}
}

HIDDEN void
draw_linear_seg(struct bu_list *vhead, const fastf_t *p1, const fastf_t or1, const fastf_t ir1, const fastf_t *p2, const fastf_t or2, const fastf_t ir2, const fastf_t *v1, const fastf_t *v2)
{
	point_t pt;

	VJOIN1( pt, p1, or1, v1 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
	VJOIN1( pt, p2, or2, v1 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
	VJOIN1( pt, p1, or1, v2 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
	VJOIN1( pt, p2, or2, v2 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
	VJOIN1( pt, p1, -or1, v1 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
	VJOIN1( pt, p2, -or2, v1 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
	VJOIN1( pt, p1, -or1, v2 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
	VJOIN1( pt, p2, -or2, v2 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );

	if( ir1 <= 0.0 && ir2 <= 0.0 )
		return;

	VJOIN1( pt, p1, ir1, v1 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
	VJOIN1( pt, p2, ir2, v1 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
	VJOIN1( pt, p1, ir1, v2 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
	VJOIN1( pt, p2, ir2, v2 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
	VJOIN1( pt, p1, -ir1, v1 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
	VJOIN1( pt, p2, -ir2, v1 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
	VJOIN1( pt, p1, -ir1, v2 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
	VJOIN1( pt, p2, -ir2, v2 );
	RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
}

HIDDEN void
draw_pipe_bend(struct bu_list *vhead, const fastf_t *center, const fastf_t *end, const fastf_t radius, const fastf_t angle, const fastf_t *v1, const fastf_t *v2, const fastf_t *norm, const fastf_t or, const fastf_t ir, fastf_t *f1, fastf_t *f2, const int
seg_count)
{

	point_t	tmp_center, tmp_start, tmp_end;
	vect_t tmp_vec;
	fastf_t tmp_radius;
	fastf_t move_dist;
	vect_t end_f1,end_f2;
	mat_t mat;
	vect_t tmp_norm;

	VREVERSE( tmp_norm, norm );
	bn_mat_arb_rot( mat, center, tmp_norm, angle );
	MAT4X3VEC( tmp_vec, mat, f1 );
	VMOVE( end_f1, tmp_vec );
	MAT4X3VEC( tmp_vec, mat, f2 );
	VMOVE( end_f2, tmp_vec );

	move_dist = or * VDOT( f1, norm );
	VJOIN2( tmp_start, center, radius, v1, or, f1 );
	VJOIN1( tmp_center, center, move_dist, norm );
	VJOIN1( tmp_end, end, or, end_f1 );
	VSUB2( tmp_vec, tmp_start, tmp_center );
	tmp_radius = MAGNITUDE( tmp_vec );
	draw_pipe_arc( vhead, tmp_radius, tmp_center, v1, v2, tmp_end, seg_count, 0 );
	VJOIN2( tmp_start, center, radius, v1, -or, f1 );
	VJOIN1( tmp_center, center, -move_dist, norm );
	VJOIN1( tmp_end, end, -or, end_f1 );
	VSUB2( tmp_vec, tmp_start, tmp_center );
	tmp_radius = MAGNITUDE( tmp_vec );
	draw_pipe_arc( vhead, tmp_radius, tmp_center, v1, v2, tmp_end, seg_count, 0 );
	move_dist = or * VDOT( f2, norm );
	VJOIN2( tmp_start, center, radius, v1, or, f2 );
	VJOIN1( tmp_center, center, move_dist, norm );
	VJOIN1( tmp_end, end, or, end_f2 );
	VSUB2( tmp_vec, tmp_start, tmp_center );
	tmp_radius = MAGNITUDE( tmp_vec );
	draw_pipe_arc( vhead, tmp_radius, tmp_center, v1, v2, tmp_end, seg_count, 0 );
	VJOIN2( tmp_start, center, radius, v1, -or, f2 );
	VJOIN1( tmp_center, center, -move_dist, norm );
	VJOIN1( tmp_end, end, -or, end_f2 );
	VSUB2( tmp_vec, tmp_start, tmp_center );
	tmp_radius = MAGNITUDE( tmp_vec );
	draw_pipe_arc( vhead, tmp_radius, tmp_center, v1, v2, tmp_end, seg_count, 0 );

	if( ir <= 0.0 )
	{
		VMOVE( f1, end_f1 );
		VMOVE( f2, end_f2 );
		return;
	}

	move_dist = ir * VDOT( f1, norm );
	VJOIN2( tmp_start, center, radius, v1, ir, f1 );
	VJOIN1( tmp_center, center, move_dist, norm );
	VJOIN1( tmp_end, end, ir, end_f1 );
	VSUB2( tmp_vec, tmp_start, tmp_center );
	tmp_radius = MAGNITUDE( tmp_vec );
	draw_pipe_arc( vhead, tmp_radius, tmp_center, v1, v2, tmp_end, seg_count, 0 );
	VJOIN2( tmp_start, center, radius, v1, -ir, f1 );
	VJOIN1( tmp_center, center, -move_dist, norm );
	VJOIN1( tmp_end, end, -ir, end_f1 );
	VSUB2( tmp_vec, tmp_start, tmp_center );
	tmp_radius = MAGNITUDE( tmp_vec );
	draw_pipe_arc( vhead, tmp_radius, tmp_center, v1, v2, tmp_end, seg_count, 0 );
	move_dist = ir * VDOT( f2, norm );
	VJOIN2( tmp_start, center, radius, v1, ir, f2 );
	VJOIN1( tmp_center, center, move_dist, norm );
	VJOIN1( tmp_end, end, ir, end_f2 );
	VSUB2( tmp_vec, tmp_start, tmp_center );
	tmp_radius = MAGNITUDE( tmp_vec );
	draw_pipe_arc( vhead, tmp_radius, tmp_center, v1, v2, tmp_end, seg_count, 0 );
	VJOIN2( tmp_start, center, radius, v1, -ir, f2 );
	VJOIN1( tmp_center, center, -move_dist, norm );
	VJOIN1( tmp_end, end, -ir, end_f2 );
	VSUB2( tmp_vec, tmp_start, tmp_center );
	tmp_radius = MAGNITUDE( tmp_vec );
	draw_pipe_arc( vhead, tmp_radius, tmp_center, v1, v2, tmp_end, seg_count, 0 );

	VMOVE( f1, end_f1 );
	VMOVE( f2, end_f2 );
}

/**
 *			R T _ P I P E _ P L O T
 */
int
rt_pipe_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	register struct wdb_pipept		*prevp;
	register struct wdb_pipept		*curp;
	register struct wdb_pipept		*nextp;
	LOCAL struct rt_pipe_internal		*pip;
	LOCAL point_t				current_point;
	LOCAL vect_t				f1,f2,f3;

	RT_CK_DB_INTERNAL(ip);
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	if( BU_LIST_IS_EMPTY( &pip->pipe_segs_head ) )
		return( 0 );

	prevp = BU_LIST_FIRST( wdb_pipept, &pip->pipe_segs_head );
	curp = BU_LIST_NEXT( wdb_pipept, &prevp->l );
	nextp = BU_LIST_NEXT( wdb_pipept, &curp->l );

	if( BU_LIST_IS_HEAD( &curp->l , &pip->pipe_segs_head ) )
		return( 0 );	/* nothing to plot */

	VMOVE( current_point, prevp->pp_coord );

	/* draw end at pipe start */
	VSUB2( f3, prevp->pp_coord, curp->pp_coord );
	bn_vec_ortho( f1, f3 );
	VCROSS( f2, f3, f1 );
	VUNITIZE( f2 );

	draw_pipe_arc( vhead, prevp->pp_od/2.0, prevp->pp_coord, f1, f2, f2, ARC_SEGS, 1 );
	if( prevp->pp_id > 0.0 )
		draw_pipe_arc( vhead, prevp->pp_id/2.0, prevp->pp_coord, f1, f2, f2, ARC_SEGS, 1 );

	while( 1 )
	{
		LOCAL vect_t n1,n2;
		LOCAL vect_t norm;
		LOCAL fastf_t angle;
		LOCAL fastf_t dist_to_bend;

		if( BU_LIST_IS_HEAD( &nextp->l, &pip->pipe_segs_head ) )
		{
			/* last segment */
			draw_linear_seg( vhead, current_point, prevp->pp_od/2.0, prevp->pp_id/2.0,
				curp->pp_coord, curp->pp_od/2.0, curp->pp_id/2.0, f1, f2 );
			break;
		}

		VSUB2( n1, prevp->pp_coord, curp->pp_coord );
		if( VNEAR_ZERO( n1, SQRT_SMALL_FASTF ) )
		{
			/* duplicate point, nothing to plot */
			goto next_pt;
		}
		VSUB2( n2, nextp->pp_coord, curp->pp_coord );
		VCROSS( norm, n1, n2 );
		VUNITIZE( n1 );
		VUNITIZE( n2 );
		angle = bn_pi - acos( VDOT( n1, n2 ) );
		dist_to_bend = curp->pp_bendradius * tan( angle/2.0 );
		if( isnan( dist_to_bend ) || VNEAR_ZERO( norm, SQRT_SMALL_FASTF) || NEAR_ZERO( dist_to_bend, SQRT_SMALL_FASTF) )
		{
			/* points are colinear, draw linear segment */
			draw_linear_seg( vhead, current_point, prevp->pp_od/2.0, prevp->pp_id/2.0,
				curp->pp_coord, curp->pp_od/2.0, curp->pp_id/2.0, f1, f2 );
			VMOVE( current_point, curp->pp_coord );
		}
		else
		{
			LOCAL point_t bend_center;
			LOCAL point_t bend_start;
			LOCAL point_t bend_end;
			LOCAL vect_t v1,v2;

			VUNITIZE( norm );

			/* draw linear segment to start of bend */
			VJOIN1( bend_start, curp->pp_coord, dist_to_bend, n1 );
			draw_linear_seg( vhead, current_point, prevp->pp_od/2.0, prevp->pp_id/2.0,
				bend_start, curp->pp_od/2.0, curp->pp_id/2.0, f1, f2 );

			/* draw bend */
			VJOIN1( bend_end, curp->pp_coord, dist_to_bend, n2 );
			VCROSS( v1, n1, norm );
			VCROSS( v2, v1, norm );
			VJOIN1( bend_center, bend_start, -curp->pp_bendradius, v1 );
			draw_pipe_bend( vhead, bend_center, bend_end, curp->pp_bendradius, angle, v1, v2, norm,
				curp->pp_od/2.0, curp->pp_id/2.0, f1, f2, ARC_SEGS );

			VMOVE( current_point, bend_end );
		}
next_pt:
		prevp = curp;
		curp = nextp;
		nextp = BU_LIST_NEXT( wdb_pipept, &curp->l );
	}

	draw_pipe_arc( vhead, curp->pp_od/2.0, curp->pp_coord, f1, f2, f2, ARC_SEGS, 1 );
	if( curp->pp_id > 0.0 )
		draw_pipe_arc( vhead, curp->pp_id/2.0, curp->pp_coord, f1, f2, f2, ARC_SEGS, 1 );

	return(0);
}

HIDDEN void
tesselate_pipe_start(struct wdb_pipept *pipe, int arc_segs, double sin_del, double cos_del, struct vertex ***outer_loop, struct vertex ***inner_loop, fastf_t *r1, fastf_t *r2, struct shell *s, const struct bn_tol *tol)
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct wdb_pipept *next;
	point_t pt;
	fastf_t or;
	fastf_t ir;
	fastf_t x,y,xnew,ynew;
	vect_t n;
	int i;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	next = BU_LIST_NEXT( wdb_pipept, &pipe->l );

	VSUB2( n, pipe->pp_coord, next->pp_coord );
	VUNITIZE( n );
	bn_vec_ortho( r1, n );
	VCROSS( r2, n, r1 );

	or = pipe->pp_od/2.0;
	ir = pipe->pp_id/2.0;

	if( or <= tol->dist )
		return;

	if( ir > or )
	{
		bu_log( "Inner radius larger than outer radius at start of pipe solid\n" );
		return;
	}

	if( NEAR_ZERO( ir - or, tol->dist) )
		return;


	fu = nmg_cface( s, *outer_loop, arc_segs );

	x = or;
	y = 0.0;
	i = (-1);
	lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		VJOIN2( pt, pipe->pp_coord, x, r1, y, r2 );
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

		vu = BU_LIST_FIRST( vertexuse, &lu->down_hd );
		eu = nmg_meonvu( vu );
		(*inner_loop)[0] = eu->vu_p->v_p;

		x = ir;
		y = 0.0;
		VJOIN2( pt, pipe->pp_coord, x, r1, y, r2 );
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
			VJOIN2( pt, pipe->pp_coord, x, r1, y, r2 );
			nmg_vertex_gv( (*inner_loop)[i], pt );
		}
	}

	else if( next->pp_id > tol->dist )
	{
		struct vertexuse *vu;

		/* make a loop of a single vertex in this face */
		lu = nmg_mlv( &fu->l.magic, (struct vertex *)NULL, OT_OPPOSITE );
		vu = BU_LIST_FIRST( vertexuse, &lu->down_hd );

		nmg_vertex_gv( vu->v_p, pipe->pp_coord );
	}

	if( nmg_calc_face_g( fu ) )
		rt_bomb( "tesselate_pipe_start: nmg_calc_face_g failed\n" );

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );

		if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
			continue;

		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			NMG_CK_EDGEUSE( eu );
			eu->e_p->is_real = 1;
		}
	}
}

HIDDEN void
tesselate_pipe_linear(fastf_t *start_pt,
		      fastf_t or,
		      fastf_t ir,
		      fastf_t *end_pt,
		      fastf_t end_or,
		      fastf_t end_ir,
		      int arc_segs,
		      double sin_del,
		      double cos_del,
		      struct vertex ***outer_loop,
		      struct vertex ***inner_loop,
		      fastf_t *r1,
		      fastf_t *r2,
		      struct shell *s,
		      const struct bn_tol *tol)
{
	struct vertex **new_outer_loop;
	struct vertex **new_inner_loop;
	struct vertex **verts[3];
	struct faceuse *fu;
	vect_t *norms;
	vect_t n;
	fastf_t slope;
	fastf_t seg_len;
	int i,j;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	norms = (vect_t *)bu_calloc( arc_segs, sizeof( vect_t ), "tesselate_pipe_linear: new normals" );

	if( end_or > tol->dist )
		new_outer_loop = (struct vertex **)bu_calloc( arc_segs, sizeof( struct vertex *),
				"tesselate_pipe_linear: new_outer_loop" );
	else
		new_outer_loop = (struct vertex **)NULL;

	if( end_ir > tol->dist )
		new_inner_loop = (struct vertex **)bu_calloc( arc_segs, sizeof( struct vertex *),
				"tesselate_pipe_linear: new_inner_loop" );
	else
		new_inner_loop = (struct vertex **)NULL;

	VSUB2( n, end_pt, start_pt );
	seg_len = MAGNITUDE( n );
	VSCALE( n, n, 1.0/seg_len );
	slope = (or - end_or)/seg_len;

	if( or > tol->dist && end_or > tol->dist )
	{
		point_t pt;
		fastf_t x,y,xnew,ynew;
		struct faceuse *fu_prev=(struct faceuse *)NULL;
		struct vertex **verts[3];

		x = 1.0;
		y = 0.0;
		VCOMB2( norms[0], x, r1, y, r2 );
		VJOIN1( norms[0], norms[0], slope, n );
		VUNITIZE( norms[0] );
		for( i=0 ; i<arc_segs ; i++ )
		{
			j = i+1;
			if( j == arc_segs )
				j = 0;

			VJOIN2( pt, end_pt, x*end_or, r1, y*end_or, r2 );
			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;
			if( i < arc_segs-1 )
			{
				VCOMB2( norms[j], x, r1, y, r2 );
				VJOIN1( norms[j], norms[j], slope, n );
				VUNITIZE( norms[j] );
			}

			if( fu_prev )
			{
				nmg_vertex_gv( new_outer_loop[i], pt );
				if( nmg_calc_face_g( fu_prev ) )
				{
					bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
					nmg_kfu( fu_prev );
				}
				else
				{
					/* assign vertexuse normals */
					struct loopuse *lu;
					struct edgeuse *eu;

					NMG_CK_FACEUSE( fu_prev );

					if( fu_prev->orientation != OT_SAME )
						fu_prev = fu_prev->fumate_p;

					lu = BU_LIST_FIRST( loopuse, &fu_prev->lu_hd );

					for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
					{
						vect_t reverse_norm;
						struct edgeuse *eu_opp_use;

						eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );
						if( eu->vu_p->v_p == new_outer_loop[i-1] )
						{
							nmg_vertexuse_nv( eu->vu_p, norms[i-1] );
							VREVERSE( reverse_norm, norms[i-1] );
							nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
						}
						else if(  eu->vu_p->v_p == (*outer_loop)[i-1] )
						{
							nmg_vertexuse_nv( eu->vu_p, norms[i-1] );
							VREVERSE( reverse_norm, norms[i-1] );
							nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
						}
						else if(  eu->vu_p->v_p == new_outer_loop[i] )
						{
							nmg_vertexuse_nv( eu->vu_p, norms[i] );
							VREVERSE( reverse_norm, norms[i] );
							nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
						}
						else if(  eu->vu_p->v_p == (*outer_loop)[i] )
						{
							nmg_vertexuse_nv( eu->vu_p, norms[i] );
							VREVERSE( reverse_norm, norms[i] );
							nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
						}
						else
						{
							bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
							bu_log( "\ti=%d, arc_segs=%d, fu_prev = x%x\n" , i, arc_segs, fu_prev );
						}
					}
				}
			}

			verts[0] = &(*outer_loop)[j];
			verts[1] = &(*outer_loop)[i];
			verts[2] = &new_outer_loop[i];

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				bu_log( "tesselate_pipe_linear: failed to make outer face #%d or=%g, end_or=%g\n",
						i, or, end_or );
				continue;
			}
			if( !new_outer_loop[i]->vg_p )
				nmg_vertex_gv( new_outer_loop[i], pt );

			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				/* assign vertexuse normals */
				struct loopuse *lu;
				struct edgeuse *eu;

				NMG_CK_FACEUSE( fu );

				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );

				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					vect_t reverse_norm;
					struct edgeuse *eu_opp_use;

					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );
					if( eu->vu_p->v_p == (*outer_loop)[0] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[0] );
						VREVERSE( reverse_norm, norms[0] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if(  eu->vu_p->v_p == new_outer_loop[i] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[i] );
						VREVERSE( reverse_norm, norms[i] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if(  eu->vu_p->v_p == (*outer_loop)[i] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[i] );
						VREVERSE( reverse_norm, norms[i] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if(  eu->vu_p->v_p == (*outer_loop)[j] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[j] );
						VREVERSE( reverse_norm, norms[j] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, arc_segs=%d, fu = x%x\n" , i, arc_segs, fu );
					}
				}
			}

			verts[1] = verts[2];
			verts[2] = &new_outer_loop[j];

			if( (fu_prev = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				bu_log( "tesselate_pipe_linear: failed to make outer face #%d or=%g, end_or=%g\n",
						i, or, end_or );
				continue;
			}
			if( i == arc_segs-1 )
			{
				if( nmg_calc_face_g( fu_prev ) )
				{
					bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
					nmg_kfu( fu_prev );
				}
			}
		}
		bu_free( (char *)(*outer_loop), "tesselate_pipe_bend: outer_loop" );
		*outer_loop = new_outer_loop;
	}
	else if( or > tol->dist && end_or <= tol->dist )
	{
		struct vertex *v=(struct vertex *)NULL;

		VSUB2( norms[0], (*outer_loop)[0]->vg_p->coord, start_pt );
		VJOIN1( norms[0], norms[0], slope*or, n );
		VUNITIZE( norms[0] );
		for( i=0 ; i<arc_segs; i++ )
		{
			j = i+1;
			if( j == arc_segs )
				j = 0;

			verts[0] = &(*outer_loop)[j];
			verts[1] = &(*outer_loop)[i];
			verts[2] = &v;

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				bu_log( "tesselate_pipe_linear: failed to make outer face #%d or=%g, end_or=%g\n",
						i, or, end_or );
				continue;
			}
			if( i == 0 )
				nmg_vertex_gv( v, end_pt );

			if( i < arc_segs-1 )
			{
				VSUB2( norms[j], (*outer_loop)[j]->vg_p->coord, start_pt );
				VJOIN1( norms[j], norms[j], slope*or, n );
				VUNITIZE( norms[j] );
			}

			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				struct loopuse *lu;
				struct edgeuse *eu;
				struct edgeuse *eu_opp_use;
				vect_t reverse_norm;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );
					if( eu->vu_p->v_p == (*outer_loop)[i] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[i] );
						VREVERSE( reverse_norm, norms[i] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if( eu->vu_p->v_p == (*outer_loop)[j] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[j] );
						VREVERSE( reverse_norm, norms[j] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if( eu->vu_p->v_p == v )
					{
						vect_t tmp_norm;
						VBLEND2( tmp_norm, 0.5, norms[i], 0.5, norms[j] );
						VUNITIZE( tmp_norm );
						nmg_vertexuse_nv( eu->vu_p, tmp_norm );
						VREVERSE( reverse_norm, tmp_norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, j=%d, arc_segs=%d, fu = x%x\n" , i,j, arc_segs, fu );
					}
				}
			}
		}

		bu_free( (char *)(*outer_loop), "tesselate_pipe_linear: outer_loop" );
		outer_loop[0] = &v;
	}
	else if( or <= tol->dist && end_or > tol->dist )
	{
		point_t pt,pt_next;
		fastf_t x,y,xnew,ynew;
		struct vertex **verts[3];


		x = 1.0;
		y = 0.0;
		VCOMB2( norms[0], x, r1, y, r2 );
		VJOIN1( pt_next, end_pt, end_or, norms[0] );
		VJOIN1( norms[0], norms[0], slope, n );
		VUNITIZE( norms[0] );
		for( i=0 ; i<arc_segs; i++ )
		{
			j = i + 1;
			if( j == arc_segs )
				j = 0;

			VMOVE( pt, pt_next )
			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;
			if( i < j )
			{
				VCOMB2( norms[j], x, r1, y, r2 );
				VJOIN1( pt_next, end_pt, end_or, norms[j] );
				VJOIN1( norms[j], norms[j], slope, n );
				VUNITIZE( norms[j] );
			}

			verts[0] = &(*outer_loop)[0];
			verts[1] = &new_outer_loop[i];
			verts[2] = &new_outer_loop[j];

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				bu_log( "tesselate_pipe_linear: failed to make outer face #%d or=%g, end_or=%g\n",
						i, or, end_or );
				continue;
			}
			if( !(*outer_loop)[0]->vg_p )
				nmg_vertex_gv( (*outer_loop)[0], start_pt );
			if( !new_outer_loop[i]->vg_p )
				nmg_vertex_gv( new_outer_loop[i], pt );
			if( !new_outer_loop[j]->vg_p )
				nmg_vertex_gv( new_outer_loop[j], pt_next );
			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				struct loopuse *lu;
				struct edgeuse *eu;
				struct edgeuse *eu_opp_use;
				vect_t reverse_norm;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );
					if( eu->vu_p->v_p == new_outer_loop[i] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[i] );
						VREVERSE( reverse_norm, norms[i] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if( eu->vu_p->v_p == new_outer_loop[j] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[j] );
						VREVERSE( reverse_norm, norms[j] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if( eu->vu_p->v_p == (*outer_loop)[0] )
					{
						vect_t tmp_norm;
						VBLEND2( tmp_norm, 0.5, norms[i], 0.5, norms[j] );
						VUNITIZE( tmp_norm );
						nmg_vertexuse_nv( eu->vu_p, tmp_norm );
						VREVERSE( reverse_norm, tmp_norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, j=%d, arc_segs=%d, fu = x%x\n" , i,j, arc_segs, fu );
					}
				}
			}
		}
		bu_free( (char *)(*outer_loop), "tesselate_pipe_linear: outer_loop" );
		*outer_loop = new_outer_loop;
	}

	slope = (ir - end_ir)/seg_len;

	if( ir > tol->dist && end_ir > tol->dist )
	{
		point_t pt;
		fastf_t x,y,xnew,ynew;
		struct faceuse *fu_prev=(struct faceuse *)NULL;
		struct vertex **verts[3];

		x = 1.0;
		y = 0.0;
		VCOMB2( norms[0], -x, r1, -y, r2 );
		VJOIN1( norms[0], norms[0], -slope, n );
		VUNITIZE( norms[0] );
		for( i=0 ; i<arc_segs ; i++ )
		{
			j = i+1;
			if( j == arc_segs )
				j = 0;

			VJOIN2( pt, end_pt, x*end_ir, r1, y*end_ir, r2 );
			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;
			if( i < arc_segs-1 )
			{
				VCOMB2( norms[j], -x, r1, -y, r2 );
				VJOIN1( norms[j], norms[j], -slope, n );
				VUNITIZE( norms[j] );
			}

			if( fu_prev )
			{
				nmg_vertex_gv( new_inner_loop[i], pt );
				if( nmg_calc_face_g( fu_prev ) )
				{
					bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
					nmg_kfu( fu_prev );
				}
				else
				{
					/* assign vertexuse normals */
					struct loopuse *lu;
					struct edgeuse *eu;

					NMG_CK_FACEUSE( fu_prev );

					if( fu_prev->orientation != OT_SAME )
						fu_prev = fu_prev->fumate_p;

					lu = BU_LIST_FIRST( loopuse, &fu_prev->lu_hd );

					for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
					{
						vect_t reverse_norm;
						struct edgeuse *eu_opp_use;

						eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );
						if( eu->vu_p->v_p == new_inner_loop[i-1] )
						{
							nmg_vertexuse_nv( eu->vu_p, norms[i-1] );
							VREVERSE( reverse_norm, norms[i-1] );
							nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
						}
						else if(  eu->vu_p->v_p == (*inner_loop)[i-1] )
						{
							nmg_vertexuse_nv( eu->vu_p, norms[i-1] );
							VREVERSE( reverse_norm, norms[i-1] );
							nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
						}
						else if(  eu->vu_p->v_p == new_inner_loop[i] )
						{
							nmg_vertexuse_nv( eu->vu_p, norms[i] );
							VREVERSE( reverse_norm, norms[i] );
							nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
						}
						else if(  eu->vu_p->v_p == (*inner_loop)[i] )
						{
							nmg_vertexuse_nv( eu->vu_p, norms[i] );
							VREVERSE( reverse_norm, norms[i] );
							nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
						}
						else
						{
							bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
							bu_log( "\ti=%d, arc_segs=%d, fu_prev = x%x\n" , i, arc_segs, fu_prev );
						}
					}
				}
			}

			verts[0] = &(*inner_loop)[j];
			verts[1] = &new_inner_loop[i];
			verts[2] = &(*inner_loop)[i];

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				bu_log( "tesselate_pipe_linear: failed to make inner face #%d ir=%g, end_ir=%g\n",
						i, ir, end_ir );
				continue;
			}
			if( !new_inner_loop[i]->vg_p )
				nmg_vertex_gv( new_inner_loop[i], pt );

			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				/* assign vertexuse normals */
				struct loopuse *lu;
				struct edgeuse *eu;

				NMG_CK_FACEUSE( fu );

				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );

				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					vect_t reverse_norm;
					struct edgeuse *eu_opp_use;

					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );
					if( eu->vu_p->v_p == (*inner_loop)[0] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[0] );
						VREVERSE( reverse_norm, norms[0] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if(  eu->vu_p->v_p == new_inner_loop[i] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[i] );
						VREVERSE( reverse_norm, norms[i] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if(  eu->vu_p->v_p == (*inner_loop)[i] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[i] );
						VREVERSE( reverse_norm, norms[i] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if(  eu->vu_p->v_p == (*inner_loop)[j] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[j] );
						VREVERSE( reverse_norm, norms[j] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, arc_segs=%d, fu = x%x\n" , i, arc_segs, fu );
					}
				}
			}

			verts[2] = verts[0];
			verts[0] = verts[1];
			verts[1] = verts[2];
			if( i == arc_segs-1 )
				verts[2] = &new_inner_loop[0];
			else
				verts[2] = &new_inner_loop[j];
			if( (fu_prev = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				bu_log( "tesselate_pipe_linear: failed to make inner face #%d ir=%g, end_ir=%g\n",
						i, ir, end_ir );
				continue;
			}
			if( i == arc_segs-1 )
			{
				if( nmg_calc_face_g( fu_prev ) )
				{
					bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
					nmg_kfu( fu_prev );
				}
			}

		}
		bu_free( (char *)(*inner_loop), "tesselate_pipe_bend: inner_loop" );
		*inner_loop = new_inner_loop;
	}
	else if( ir > tol->dist && end_ir <= tol->dist )
	{
		struct vertex *v=(struct vertex *)NULL;

		VSUB2( norms[0], (*inner_loop)[0]->vg_p->coord, start_pt );
		VJOIN1( norms[0], norms[0], -slope*ir, n );
		VUNITIZE( norms[0] );
		VREVERSE( norms[0], norms[0] );
		for( i=0 ; i<arc_segs; i++ )
		{
			j = i+1;
			if( j == arc_segs )
				j = 0;

			verts[0] = &(*inner_loop)[i];
			verts[1] = &(*inner_loop)[j];
			verts[2] = &v;

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				bu_log( "tesselate_pipe_linear: failed to make inner face #%d ir=%g, end_ir=%g\n",
						i, ir, end_ir );
				continue;
			}
			if( i == 0 )
				nmg_vertex_gv( v, end_pt );

			if( i < arc_segs-1 )
			{
				VSUB2( norms[j], (*inner_loop)[j]->vg_p->coord, start_pt );
				VJOIN1( norms[j], norms[j], -slope*ir, n );
				VUNITIZE( norms[j] );
				VREVERSE( norms[j], norms[j] );
			}

			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				struct loopuse *lu;
				struct edgeuse *eu;
				struct edgeuse *eu_opp_use;
				vect_t reverse_norm;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );
					if( eu->vu_p->v_p == (*inner_loop)[i] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[i] );
						VREVERSE( reverse_norm, norms[i] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if( eu->vu_p->v_p == (*inner_loop)[j] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[j] );
						VREVERSE( reverse_norm, norms[j] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if( eu->vu_p->v_p == v )
					{
						vect_t tmp_norm;
						VBLEND2( tmp_norm, 0.5, norms[i], 0.5, norms[j] );
						VUNITIZE( tmp_norm );
						nmg_vertexuse_nv( eu->vu_p, tmp_norm );
						VREVERSE( reverse_norm, tmp_norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, j=%d, arc_segs=%d, fu = x%x\n" , i,j, arc_segs, fu );
					}
				}
			}
		}

		bu_free( (char *)(*inner_loop), "tesselate_pipe_linear: inner_loop" );
		inner_loop[0] = &v;
	}
	else if( ir <= tol->dist && end_ir > tol->dist )
	{
		point_t pt,pt_next;
		fastf_t x,y,xnew,ynew;
		struct vertex **verts[3];

		x = 1.0;
		y = 0.0;
		VCOMB2( norms[0], -x, r1, -y, r2 );
		VJOIN1( pt_next, end_pt, -end_ir, norms[0] );
		VJOIN1( norms[0], norms[0], -slope, n );
		VUNITIZE( norms[0] );
		for( i=0 ; i<arc_segs; i++ )
		{
			j = i + 1;
			if( j == arc_segs )
				j = 0;

			VMOVE( pt, pt_next )
			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;
			if( i < j )
			{
				VCOMB2( norms[j], -x, r1, -y, r2 );
				VJOIN1( pt_next, end_pt, -end_ir, norms[j] );
				VJOIN1( norms[j], norms[j], -slope, n );
				VUNITIZE( norms[j] );
			}

			verts[0] = &new_inner_loop[j];
			verts[1] = &new_inner_loop[i];
			verts[2] = &(*inner_loop)[0];

			if( (fu = nmg_cmface( s, verts, 3 ) ) == NULL )
			{
				bu_log( "tesselate_pipe_linear: failed to make inner face #%d ir=%g, end_ir=%g\n",
						i, ir, end_ir );
				continue;
			}
			if( !(*inner_loop)[0]->vg_p )
				nmg_vertex_gv( (*inner_loop)[0], start_pt );
			if( !new_inner_loop[i]->vg_p )
				nmg_vertex_gv( new_inner_loop[i], pt );
			if( !new_inner_loop[j]->vg_p )
				nmg_vertex_gv( new_inner_loop[j], pt_next );
			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_linear: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				struct loopuse *lu;
				struct edgeuse *eu;
				struct edgeuse *eu_opp_use;
				vect_t reverse_norm;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );
					if( eu->vu_p->v_p == new_inner_loop[i] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[i] );
						VREVERSE( reverse_norm, norms[i] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if( eu->vu_p->v_p == new_inner_loop[j] )
					{
						nmg_vertexuse_nv( eu->vu_p, norms[j] );
						VREVERSE( reverse_norm, norms[j] );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else if( eu->vu_p->v_p == (*inner_loop)[0] )
					{
						vect_t tmp_norm;
						VBLEND2( tmp_norm, 0.5, norms[i], 0.5, norms[j] );
						VUNITIZE( tmp_norm );
						nmg_vertexuse_nv( eu->vu_p, tmp_norm );
						VREVERSE( reverse_norm, tmp_norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, reverse_norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, j=%d, arc_segs=%d, fu = x%x\n" , i,j, arc_segs, fu );
					}
				}
			}
		}
		bu_free( (char *)(*inner_loop), "tesselate_pipe_linear: inner_loop" );
		*inner_loop = new_inner_loop;
	}
	bu_free( (char *)norms, "tesselate_linear_pipe: norms" );
}

HIDDEN void
tesselate_pipe_bend(fastf_t *bend_start, fastf_t *bend_end, fastf_t *bend_center, fastf_t or, fastf_t ir, int arc_segs, double sin_del, double cos_del, struct vertex ***outer_loop, struct vertex ***inner_loop, fastf_t *start_r1, fastf_t *start_r2, struct
shell *s, const struct bn_tol *tol, const struct rt_tess_tol *ttol)
{
	struct vertex **new_outer_loop;
	struct vertex **new_inner_loop;
	fastf_t bend_radius;
	fastf_t bend_angle;
	fastf_t x,y,xnew,ynew;
	fastf_t delta_angle;
	mat_t rot;
	vect_t b1;
	vect_t b2;
	vect_t r1, r2;
	vect_t r1_tmp,r2_tmp;
	vect_t bend_norm;
	vect_t to_start;
	vect_t to_end;
	vect_t norm;
	point_t origin;
	point_t center;
	point_t old_center;
	int bend_segs=1;	/* minimum number of edges along bend */
	int bend_seg;
	int tol_segs;
	int i,j;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );
	RT_CK_TESS_TOL( ttol );

	VMOVE( r1, start_r1 );
	VMOVE( r2, start_r2 );

	/* Calculate vector b1, unit vector in direction from
	 * bend center to start point
	 */
	VSUB2( to_start, bend_start, bend_center );
	bend_radius = MAGNITUDE( to_start );
	VSCALE( b1, to_start, 1.0/bend_radius );

	/* bend_norm is normal to plane of bend */
	VSUB2( to_end, bend_end, bend_center );
	VCROSS( bend_norm, b1, to_end );
	VUNITIZE( bend_norm );

	/* b1, b2, and bend_norm form a RH coord, system */
	VCROSS( b2, bend_norm, b1 );

	bend_angle = atan2( VDOT( to_end, b2 ), VDOT( to_end, b1 ) );
	if( bend_angle < 0.0 )
		bend_angle += 2.0*bn_pi;

	/* calculate number of segments to use along bend */
	if( ttol->abs > 0.0 && ttol->abs < bend_radius+or )
	{
		tol_segs = ceil( bend_angle/(2.0*acos( 1.0 - ttol->abs/(bend_radius+or) ) ) );
		if( tol_segs > bend_segs )
			bend_segs = tol_segs;
	}
	if( ttol->rel > 0.0 )
	{
		tol_segs = ceil(bend_angle/(2.0*acos( 1.0 - ttol->rel ) ) );
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

	VSETALL( origin, 0.0 );
	bn_mat_arb_rot( rot, origin, bend_norm, delta_angle);

	VMOVE( old_center, bend_start )
	for( bend_seg=0; bend_seg<bend_segs ; bend_seg++ )
	{

		new_outer_loop = (struct vertex **)bu_calloc( arc_segs, sizeof( struct vertex *),
				"tesselate_pipe_bend(): new_outer_loop" );

		MAT4X3VEC( r1_tmp, rot, r1 )
		MAT4X3VEC( r2_tmp, rot, r2 )
		VMOVE( r1, r1_tmp )
		VMOVE( r2, r2_tmp )

		VSUB2( r1_tmp, old_center, bend_center )
		MAT4X3PNT( r2_tmp, rot, r1_tmp )
		VADD2( center, r2_tmp, bend_center )

		x = or;
		y = 0.0;
		for( i=0; i<arc_segs; i++ )
		{
			struct faceuse *fu;
			struct vertex **verts[3];
			point_t pt;

			j = i+1;
			if( j == arc_segs )
				j = 0;

			verts[0] = &(*outer_loop)[j];
			verts[1] = &(*outer_loop)[i];
			verts[2] = &new_outer_loop[i];

			if( (fu=nmg_cmface( s, verts, 3 )) == NULL )
			{
				bu_log( "tesselate_pipe_bend(): nmg_cmface failed\n" );
				rt_bomb( "tesselate_pipe_bend\n" );
			}
			VJOIN2( pt, center, x, r1, y, r2 );
			if( !new_outer_loop[i]->vg_p )
				nmg_vertex_gv( new_outer_loop[i], pt );
			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_bend: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				struct loopuse *lu;
				struct edgeuse *eu;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					struct edgeuse *eu_opp_use;

					NMG_CK_EDGEUSE( eu );
					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );

					if( eu->vu_p->v_p == (*outer_loop)[j] )
					{
						VSUB2( norm, (*outer_loop)[j]->vg_p->coord, old_center );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else if( eu->vu_p->v_p == (*outer_loop)[i] )
					{
						VSUB2( norm, (*outer_loop)[i]->vg_p->coord, old_center );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else if( eu->vu_p->v_p == new_outer_loop[i] )
					{
						VSUB2( norm, new_outer_loop[i]->vg_p->coord, center );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, j=%d, arc_segs=%d, fu = x%x\n" , i,j, arc_segs, fu );
					}
				}
			}

			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;

			verts[1] = verts[2];
			verts[2] = &new_outer_loop[j];

			if( (fu=nmg_cmface( s, verts, 3 )) == NULL )
			{
				bu_log( "tesselate_pipe_bend(): nmg_cmface failed\n" );
				rt_bomb( "tesselate_pipe_bend\n" );
			}
			VJOIN2( pt, center, x, r1, y, r2 );
			if( !(*verts[2])->vg_p )
				nmg_vertex_gv( *verts[2], pt );
			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_bend: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				struct loopuse *lu;
				struct edgeuse *eu;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					struct edgeuse *eu_opp_use;

					NMG_CK_EDGEUSE( eu );
					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );

					if( eu->vu_p->v_p == (*outer_loop)[j] )
					{
						VSUB2( norm, (*outer_loop)[j]->vg_p->coord, old_center );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else if( eu->vu_p->v_p == new_outer_loop[i] )
					{
						VSUB2( norm, new_outer_loop[i]->vg_p->coord, center );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else if( eu->vu_p->v_p == new_outer_loop[j] )
					{
						VSUB2( norm, new_outer_loop[j]->vg_p->coord, center );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, j=%d, arc_segs=%d, fu = x%x\n" , i,j, arc_segs, fu );
					}
				}
			}
		}

		bu_free( (char *)(*outer_loop), "tesselate_pipe_bend: outer_loop" );
		*outer_loop = new_outer_loop;
		VMOVE( old_center, center );
	}

	if( ir <= tol->dist )
	{
		VMOVE( start_r1, r1 )
		VMOVE( start_r2, r2 )
		return;
	}

	VMOVE( r1, start_r1 )
	VMOVE( r2, start_r2 )

	VMOVE( old_center, bend_start )
	for( bend_seg=0; bend_seg<bend_segs ; bend_seg++ )
	{

		new_inner_loop = (struct vertex **)bu_calloc( arc_segs, sizeof( struct vertex *),
				"tesselate_pipe_bend(): new_inner_loop" );

		MAT4X3VEC( r1_tmp, rot, r1 )
		MAT4X3VEC( r2_tmp, rot, r2 )
		VMOVE( r1, r1_tmp )
		VMOVE( r2, r2_tmp )

		VSUB2( r1_tmp, old_center, bend_center )
		MAT4X3PNT( r2_tmp, rot, r1_tmp )
		VADD2( center, r2_tmp, bend_center )

		x = ir;
		y = 0.0;
		for( i=0; i<arc_segs; i++ )
		{
			struct faceuse *fu;
			struct vertex **verts[3];
			point_t pt;

			j = i + 1;
			if( j == arc_segs )
				j = 0;

			verts[0] = &(*inner_loop)[i];
			verts[1] = &(*inner_loop)[j];
			verts[2] = &new_inner_loop[i];

			if( (fu=nmg_cmface( s, verts, 3 )) == NULL )
			{
				bu_log( "tesselate_pipe_bend(): nmg_cmface failed\n" );
				rt_bomb( "tesselate_pipe_bend\n" );
			}
			VJOIN2( pt, center, x, r1, y, r2 );
			if( !new_inner_loop[i]->vg_p )
				nmg_vertex_gv( new_inner_loop[i], pt );
			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_bend: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				struct loopuse *lu;
				struct edgeuse *eu;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					struct edgeuse *eu_opp_use;

					NMG_CK_EDGEUSE( eu );
					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );

					if( eu->vu_p->v_p == (*inner_loop)[j] )
					{
						VSUB2( norm, old_center, (*inner_loop)[j]->vg_p->coord );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else if( eu->vu_p->v_p == (*inner_loop)[i] )
					{
						VSUB2( norm, old_center, (*inner_loop)[i]->vg_p->coord );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else if( eu->vu_p->v_p == new_inner_loop[i] )
					{
						VSUB2( norm, center, new_inner_loop[i]->vg_p->coord );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, j=%d, arc_segs=%d, fu = x%x\n" , i,j, arc_segs, fu );
					}
				}
			}

			xnew = x*cos_del - y*sin_del;
			ynew = x*sin_del + y*cos_del;
			x = xnew;
			y = ynew;

			verts[0] = verts[2];
			verts[2] = &new_inner_loop[j];

			if( (fu=nmg_cmface( s, verts, 3 )) == NULL )
			{
				bu_log( "tesselate_pipe_bend(): nmg_cmface failed\n" );
				rt_bomb( "tesselate_pipe_bend\n" );
			}
			VJOIN2( pt, center, x, r1, y, r2 );
			if( !(*verts[2])->vg_p )
				nmg_vertex_gv( *verts[2], pt );
			if( nmg_calc_face_g( fu ) )
			{
				bu_log( "tesselate_pipe_bend: nmg_calc_face_g failed\n" );
				nmg_kfu( fu );
			}
			else
			{
				struct loopuse *lu;
				struct edgeuse *eu;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					struct edgeuse *eu_opp_use;

					NMG_CK_EDGEUSE( eu );
					eu_opp_use = BU_LIST_PNEXT_CIRC( edgeuse, &eu->eumate_p->l );

					if( eu->vu_p->v_p == (*inner_loop)[j] )
					{
						VSUB2( norm, old_center, (*inner_loop)[j]->vg_p->coord );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else if( eu->vu_p->v_p == new_inner_loop[i] )
					{
						VSUB2( norm, center, new_inner_loop[i]->vg_p->coord );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else if( eu->vu_p->v_p == new_inner_loop[j] )
					{
						VSUB2( norm, center, new_inner_loop[j]->vg_p->coord );
						VUNITIZE( norm );
						nmg_vertexuse_nv( eu->vu_p, norm );
						VREVERSE( norm, norm );
						nmg_vertexuse_nv( eu_opp_use->vu_p, norm );
					}
					else
					{
						bu_log( "No vu_normal assigned at (%g %g %g)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ) );
						bu_log( "\ti=%d, j=%d, arc_segs=%d, fu = x%x\n" , i,j, arc_segs, fu );
					}
				}
			}
		}
		bu_free( (char *)(*inner_loop), "tesselate_pipe_bend: inner_loop" );
		*inner_loop = new_inner_loop;
		VMOVE( old_center, center );
	}
	VMOVE( start_r1, r1 )
	VMOVE( start_r2, r2 )
}

HIDDEN void
tesselate_pipe_end(struct wdb_pipept *pipe, int arc_segs, double sin_del, double cos_del, struct vertex ***outer_loop, struct vertex ***inner_loop, struct shell *s, const struct bn_tol *tol)
{
	struct wdb_pipept *prev;
	struct faceuse *fu;
	struct loopuse *lu;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	if( pipe->pp_od <= tol->dist )
		return;

	if( NEAR_ZERO( pipe->pp_od - pipe->pp_id, tol->dist) )
		return;

	if( (fu = nmg_cface( s, *outer_loop, arc_segs )) == NULL )
	{
		bu_log( "tesselate_pipe_end(): nmg_cface failed\n" );
		return;
	}
	fu = fu->fumate_p;
	if( nmg_calc_face_g( fu ) )
	{
		bu_log( "tesselate_pipe_end: nmg_calc_face_g failed\n" );
		nmg_kfu( fu );
		return;
	}

	prev = BU_LIST_PREV( wdb_pipept, &pipe->l );

	if( pipe->pp_id > tol->dist )
	{
		struct vertex **verts;
		int i;

		verts = (struct vertex **)bu_calloc( arc_segs, sizeof( struct vertex *),
			"tesselate_pipe_end: verts" );
		for( i=0 ; i<arc_segs; i++ )
			verts[i] = (*inner_loop)[i];

		fu = nmg_add_loop_to_face( s, fu, verts, arc_segs, OT_OPPOSITE );

		bu_free( (char *)verts, "tesselate_pipe_end: verts" );
	}

	else if( prev->pp_id > tol->dist )
	{
		struct vertexuse *vu;

		/* make a loop of a single vertex in this face */
		lu = nmg_mlv( &fu->l.magic, (struct vertex *)NULL, OT_OPPOSITE );
		vu = BU_LIST_FIRST( vertexuse, &lu->down_hd );

		nmg_vertex_gv( vu->v_p, pipe->pp_coord );
	}

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		struct edgeuse *eu;

		NMG_CK_LOOPUSE( lu );

		if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
			continue;

		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			NMG_CK_EDGEUSE( eu );
			eu->e_p->is_real = 1;
		}
	}
}

/**
 *			R T _ P I P E _ T E S S
 *
 *	XXXX Still needs vertexuse normals!
 */
int
rt_pipe_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	struct wdb_pipept	*pp1;
	struct wdb_pipept	*pp2;
	struct wdb_pipept	*pp3;
	point_t			curr_pt;
	struct shell *s;
	struct rt_pipe_internal *pip;
	int arc_segs=6;			/* minimum number of segments for a circle */
	int tol_segs;
	fastf_t max_diam=0.0;
	fastf_t pipe_size;
	fastf_t curr_od,curr_id;
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

	BN_CK_TOL( tol );
	RT_CK_TESS_TOL( ttol );
	NMG_CK_MODEL( m );

	*r = (struct nmgregion *)NULL;

	if( BU_LIST_IS_EMPTY( &pip->pipe_segs_head ) )
		return( 0 );	/* nothing to tesselate */

	pp1 = BU_LIST_FIRST( wdb_pipept, &pip->pipe_segs_head );

	VMOVE( min_pt, pp1->pp_coord );
	VMOVE( max_pt, pp1->pp_coord );

	/* find max diameter */
	for( BU_LIST_FOR( pp1, wdb_pipept, &pip->pipe_segs_head ) )
	{
		if( pp1->pp_od > 0.0 && pp1->pp_od > max_diam )
			max_diam = pp1->pp_od;

		VMINMAX( min_pt, max_pt, pp1->pp_coord );
	}

	if( max_diam <= tol->dist )
		return( 0 );	/* nothing to tesselate */

	/* calculate pipe size for relative tolerance */
	VSUB2( min_to_max, max_pt, min_pt );
	pipe_size = MAGNITUDE( min_to_max );

	/* calculate number of segments for circles */
	if( ttol->abs > 0.0 && ttol->abs * 2.0 < max_diam )
	{
		tol_segs = ceil( bn_pi/acos( 1.0 - 2.0 * ttol->abs/max_diam) );
		if( tol_segs > arc_segs )
			arc_segs = tol_segs;
	}
	if( ttol->rel > 0.0 && 2.0 * ttol->rel * pipe_size < max_diam )
	{
		tol_segs = ceil( bn_pi/acos( 1.0 - 2.0 * ttol->rel*pipe_size/max_diam) );
		if( tol_segs > arc_segs )
			arc_segs = tol_segs;
	}
	if( ttol->norm > 0.0 )
	{
		tol_segs = ceil( bn_pi/ttol->norm );
		if( tol_segs > arc_segs )
			arc_segs = tol_segs;
	}

	*r = nmg_mrsv( m );
	s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	outer_loop = (struct vertex **)bu_calloc( arc_segs, sizeof( struct vertex *),
			"rt_pipe_tess: outer_loop" );
	inner_loop = (struct vertex **)bu_calloc( arc_segs, sizeof( struct vertex *),
			"rt_pipe_tess: inner_loop" );
	delta_angle = 2.0 * bn_pi / (double)arc_segs;
	sin_del = sin( delta_angle );
	cos_del = cos( delta_angle );

	pp1 = BU_LIST_FIRST( wdb_pipept, &(pip->pipe_segs_head) );
	tesselate_pipe_start( pp1, arc_segs, sin_del, cos_del,
		&outer_loop, &inner_loop, r1, r2, s, tol );

	pp2 = BU_LIST_NEXT( wdb_pipept, &pp1->l );
	if( BU_LIST_IS_HEAD( &pp2->l, &(pip->pipe_segs_head) ) )
		return( 0 );
	pp3 = BU_LIST_NEXT( wdb_pipept, &pp2->l );
	if( BU_LIST_IS_HEAD( &pp3->l ,  &(pip->pipe_segs_head) ) )
		pp3 = (struct wdb_pipept *)NULL;

	VMOVE( curr_pt, pp1->pp_coord );
	curr_od = pp1->pp_od;
	curr_id = pp1->pp_id;
	while( 1 )
	{
		vect_t n1,n2;
		vect_t norm;
		vect_t v1;
		fastf_t angle;
		fastf_t dist_to_bend;
		point_t bend_start, bend_end, bend_center;

		VSUB2( n1, curr_pt, pp2->pp_coord );
		if( VNEAR_ZERO( n1, SQRT_SMALL_FASTF ) )
		{
			/* duplicate point, skip to next point */
			goto next_pt;
		}

		if( !pp3 )
		{
			/* last segment */
			tesselate_pipe_linear(curr_pt, curr_od/2.0, curr_id/2.0,
				pp2->pp_coord, pp2->pp_od/2.0, pp2->pp_id/2.0,
				arc_segs, sin_del, cos_del, &outer_loop, &inner_loop, r1, r2, s, tol );
			break;
		}

		VSUB2( n2, pp3->pp_coord, pp2->pp_coord );
		VCROSS( norm, n1, n2 );
		if( VNEAR_ZERO( norm, SQRT_SMALL_FASTF ) )
		{
			/* points are colinear, treat as a linear segment */
			tesselate_pipe_linear(curr_pt, curr_od/2.0, curr_id/2.0,
				pp2->pp_coord, pp2->pp_od/2.0, pp2->pp_id/2.0,
				arc_segs, sin_del, cos_del, &outer_loop, &inner_loop, r1, r2, s, tol );

			VMOVE( curr_pt, pp2->pp_coord );
			curr_id = pp2->pp_id;
			curr_od = pp2->pp_od;
			goto next_pt;
		}

		VUNITIZE( n1 );
		VUNITIZE( n2 );
		VUNITIZE( norm );

		/* linear section */
		angle = bn_pi - acos( VDOT( n1, n2 ) );
		dist_to_bend = pp2->pp_bendradius * tan( angle/2.0 );
		VJOIN1( bend_start, pp2->pp_coord, dist_to_bend, n1 );
		tesselate_pipe_linear( curr_pt, curr_od/2.0, curr_id/2.0,
				bend_start, pp2->pp_od/2.0, pp2->pp_id/2.0,
				arc_segs, sin_del, cos_del, &outer_loop, &inner_loop, r1, r2, s, tol );

		/* and bend section */
		VJOIN1( bend_end, pp2->pp_coord, dist_to_bend, n2 );
		VCROSS( v1, n1, norm );
		VJOIN1( bend_center, bend_start, -pp2->pp_bendradius, v1 );
		tesselate_pipe_bend( bend_start, bend_end, bend_center, curr_od/2.0, curr_id/2.0,
			arc_segs, sin_del, cos_del, &outer_loop, &inner_loop,
			r1, r2, s, tol, ttol );

		VMOVE( curr_pt, bend_end );
		curr_id = pp2->pp_id;
		curr_od = pp2->pp_od;
next_pt:
		pp1 = pp2;
		pp2 = pp3;
		pp3 = BU_LIST_NEXT( wdb_pipept, &pp3->l );
		if( BU_LIST_IS_HEAD( &pp3->l ,  &(pip->pipe_segs_head) ) )
			pp3 = (struct wdb_pipept *)NULL;
	}

	tesselate_pipe_end( pp2, arc_segs, sin_del, cos_del, &outer_loop, &inner_loop, s, tol );

	bu_free( (char *)outer_loop, "rt_pipe_tess: outer_loop" );
	bu_free( (char *)inner_loop, "rt_pipe_tess: inner_loop" );

	nmg_rebound( m, tol );

	return( 0 );
}

/**
 *			R T _ P I P E _ I M P O R T
 */
int
rt_pipe_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	register struct exported_pipept *exp;
	register struct wdb_pipept	*ptp;
	struct wdb_pipept		tmp;
	struct rt_pipe_internal		*pipe;
	union record			*rp;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_PIPE )  {
		bu_log("rt_pipe_import: defective record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_PIPE;
	ip->idb_meth = &rt_functab[ID_PIPE];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_pipe_internal), "rt_pipe_internal");
	pipe = (struct rt_pipe_internal *)ip->idb_ptr;
	pipe->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
	pipe->pipe_count = bu_glong( rp->pwr.pwr_pt_count);

	/*
	 *  Walk the array of segments in reverse order,
	 *  allocating a linked list of segments in internal format,
	 *  using exactly the same structures as libwdb.
	 */
	BU_LIST_INIT( &pipe->pipe_segs_head );
	for( exp = &rp->pwr.pwr_data[pipe->pipe_count-1]; exp >= &rp->pwr.pwr_data[0]; exp-- )  {
		ntohd( (unsigned char *)&tmp.pp_id, exp->epp_id, 1 );
		ntohd( (unsigned char *)&tmp.pp_od, exp->epp_od, 1 );
		ntohd( (unsigned char *)&tmp.pp_bendradius, exp->epp_bendradius, 1 );
		ntohd( (unsigned char *)tmp.pp_coord, exp->epp_coord, 3 );

		/* Apply modeling transformations */
		BU_GETSTRUCT( ptp, wdb_pipept );
		ptp->l.magic = WDB_PIPESEG_MAGIC;
		MAT4X3PNT( ptp->pp_coord, mat, tmp.pp_coord );
		ptp->pp_id = tmp.pp_id / mat[15];
		ptp->pp_od = tmp.pp_od / mat[15];
		ptp->pp_bendradius = tmp.pp_bendradius / mat[15];
		BU_LIST_APPEND( &pipe->pipe_segs_head, &ptp->l );
	}

	return(0);			/* OK */
}

/**
 *			R T _ P I P E _ E X P O R T
 */
int
rt_pipe_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_pipe_internal	*pip;
	struct bu_list		*headp;
	register struct exported_pipept *epp;
	register struct wdb_pipept	*ppt;
	struct wdb_pipept		tmp;
	int		count;
	int		ngran;
	int		nbytes;
	union record	*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_PIPE )  return(-1);
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	if (pip->pipe_segs_head.magic == 0) {
	    return -1; /* no segments provided, empty pipe is bogus */
	}
	headp = &pip->pipe_segs_head;

	/* Count number of points */
	count = 0;
	for( BU_LIST_FOR( ppt, wdb_pipept, headp ) )
		count++;

	if( count < 1 )
		return(-4);			/* Not enough for 1 pipe! */

	/* Determine how many whole granules will be required */
	nbytes = sizeof(struct pipewire_rec) +
		(count-1) * sizeof(struct exported_pipept);
	ngran = (nbytes + sizeof(union record) - 1) / sizeof(union record);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = ngran * sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "pipe external");
	rec = (union record *)ep->ext_buf;

	rec->pwr.pwr_id = DBID_PIPE;
	(void)bu_plong( rec->pwr.pwr_count, ngran-1 );	/* # EXTRA grans */
	(void)bu_plong( rec->pwr.pwr_pt_count, count );

	/* Convert the pipe segments to external form */
	epp = &rec->pwr.pwr_data[0];
	for( BU_LIST_FOR( ppt, wdb_pipept, headp ), epp++ )  {
		/* Convert from user units to mm */
		VSCALE( tmp.pp_coord, ppt->pp_coord, local2mm );
		tmp.pp_id = ppt->pp_id * local2mm;
		tmp.pp_od = ppt->pp_od * local2mm;
		tmp.pp_bendradius = ppt->pp_bendradius * local2mm;
		htond( epp->epp_coord, (unsigned char *)tmp.pp_coord, 3 );
		htond( epp->epp_id, (unsigned char *)&tmp.pp_id, 1 );
		htond( epp->epp_od, (unsigned char *)&tmp.pp_od, 1 );
		htond( epp->epp_bendradius, (unsigned char *)&tmp.pp_bendradius, 1 );
	}

	return(0);
}

/**
 *			R T _ P I P E _ I M P O R T 5
 */
int
rt_pipe_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	register struct wdb_pipept	*ptp;
	struct rt_pipe_internal		*pipe;
	fastf_t				*vec;
	int				total_count;
	int				double_count;
	int				byte_count;
	unsigned long			pipe_count;
	int				i;

	BU_CK_EXTERNAL( ep );

	pipe_count = bu_glong((unsigned char *)ep->ext_buf);
	double_count = pipe_count * 6;
	byte_count = double_count * SIZEOF_NETWORK_DOUBLE;
	total_count = 4 + byte_count;
	BU_ASSERT_LONG( ep->ext_nbytes, ==, total_count);

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_PIPE;
	ip->idb_meth = &rt_functab[ID_PIPE];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_pipe_internal), "rt_pipe_internal");

	pipe = (struct rt_pipe_internal *)ip->idb_ptr;
	pipe->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
	pipe->pipe_count = pipe_count;

	vec = (fastf_t *)bu_malloc(byte_count, "rt_pipe_import5: vec");
	/* Convert from database (network) to internal (host) format */
	ntohd((unsigned char *)vec, (unsigned char *)ep->ext_buf + 4, double_count);

	/*
	 *  Walk the array of segments in reverse order,
	 *  allocating a linked list of segments in internal format,
	 *  using exactly the same structures as libwdb.
	 */
	BU_LIST_INIT( &pipe->pipe_segs_head );
	for (i = 0; i < double_count; i += 6) {
		/* Apply modeling transformations */
		BU_GETSTRUCT( ptp, wdb_pipept );
		ptp->l.magic = WDB_PIPESEG_MAGIC;
		MAT4X3PNT( ptp->pp_coord, mat, &vec[i] );
		ptp->pp_id =		vec[i+3] / mat[15];
		ptp->pp_od =		vec[i+4] / mat[15];
		ptp->pp_bendradius =	vec[i+5] / mat[15];
		BU_LIST_INSERT( &pipe->pipe_segs_head, &ptp->l );
	}

	bu_free((genptr_t)vec, "rt_pipe_import5: vec");
	return(0);			/* OK */
}

/**
 *			R T _ P I P E _ E X P O R T 5
 */
int
rt_pipe_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_pipe_internal	*pip;
	struct bu_list		*headp;
	register struct wdb_pipept	*ppt;
	fastf_t				*vec;
	int				total_count;
	int				double_count;
	int				byte_count;
	unsigned long			pipe_count;
	int				i = 0;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_PIPE )  return(-1);
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	if (pip->pipe_segs_head.magic == 0) {
	    return -1; /* no segments provided, empty pipe is bogus */
	}
	headp = &pip->pipe_segs_head;

	/* Count number of points */
	pipe_count = 0;
	for( BU_LIST_FOR( ppt, wdb_pipept, headp ) )
		pipe_count++;

	if( pipe_count <= 1 )
		return(-4);			/* Not enough for 1 pipe! */

	double_count = pipe_count * 6;
	byte_count = double_count * SIZEOF_NETWORK_DOUBLE;
	total_count = 4 + byte_count;
	vec = (fastf_t *)bu_malloc(byte_count, "rt_pipe_export5: vec");

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = total_count;
	ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "pipe external");

	(void)bu_plong((unsigned char *)ep->ext_buf, pipe_count);

	/* Convert the pipe segments to external form */
	for( BU_LIST_FOR( ppt, wdb_pipept, headp ), i += 6  )  {
		/* Convert from user units to mm */
		VSCALE( &vec[i], ppt->pp_coord, local2mm );
		vec[i+3] = ppt->pp_id * local2mm;
		vec[i+4] = ppt->pp_od * local2mm;
		vec[i+5] = ppt->pp_bendradius * local2mm;
	}

	/* Convert from internal (host) to database (network) format */
	htond((unsigned char *)ep->ext_buf + 4, (unsigned char *)vec, double_count);

	bu_free((genptr_t)vec, "rt_pipe_export5: vec");
	return(0);
}

/**
 *			R T _ P I P E _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_pipe_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_pipe_internal	*pip;
	register struct wdb_pipept	*ptp;
	char	buf[256];
	int	segno = 0;

	RT_CK_DB_INTERNAL(ip);
	pip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pip);

	sprintf(buf, "pipe with %d points\n", pip->pipe_count );
	bu_vls_strcat( str, buf );

	if( !verbose )  return(0);

#if 1
	/* Too much for the MGED Display!!!! */
	for( BU_LIST_FOR( ptp, wdb_pipept, &pip->pipe_segs_head ) )  {
		sprintf(buf, "\t%d ", segno++ );
		bu_vls_strcat( str, buf );
		sprintf( buf, "\tbend radius = %g", INTCLAMP(ptp->pp_bendradius * mm2local) );
		bu_vls_strcat( str, buf );
		sprintf(buf, "  od=%g", INTCLAMP(ptp->pp_od * mm2local) );
		bu_vls_strcat( str, buf );
		if( ptp->pp_id > 0 )  {
			sprintf(buf, ", id  = %g", INTCLAMP(ptp->pp_id * mm2local) );
			bu_vls_strcat( str, buf );
		}
		bu_vls_strcat( str, "\n" );

		sprintf(buf, "\t  at=(%g, %g, %g)\n",
			INTCLAMP(ptp->pp_coord[X] * mm2local),
			INTCLAMP(ptp->pp_coord[Y] * mm2local),
			INTCLAMP(ptp->pp_coord[Z] * mm2local) );
		bu_vls_strcat( str, buf );

	}
#endif
	return(0);
}

/**
 *			R T _ P I P E _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_pipe_ifree(struct rt_db_internal *ip)
{
	register struct rt_pipe_internal	*pipe;
	register struct wdb_pipept	*ptp;

	RT_CK_DB_INTERNAL(ip);
	pipe = (struct rt_pipe_internal*)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pipe);

	if (pipe->pipe_segs_head.magic != 0) {
	    while( BU_LIST_WHILE( ptp, wdb_pipept, &pipe->pipe_segs_head ) )  {
		BU_LIST_DEQUEUE( &(ptp->l) );
		bu_free( (char *)ptp, "wdb_pipept" );
	    }
	}
	bu_free( ip->idb_ptr, "pipe ifree" );
	ip->idb_ptr = GENPTR_NULL;
}

/**
 *			R T _ P I P E _ C K
 *
 *  Check pipe solid
 *	Bend radius must be at least as large as the outer radius
 *	All bends must have constant diameters
 *	No consecutive LINEAR sections without BENDS unless the
 *		LINEAR sections are collinear.
 */
int
rt_pipe_ck( const struct bu_list *headp )
{
	int error_count=0;
	struct wdb_pipept *cur,*prev,*next;
	fastf_t old_bend_dist=0.0;
	fastf_t new_bend_dist;
	fastf_t v2_len=0.0;

	prev = BU_LIST_FIRST( wdb_pipept, headp );
	if( prev->pp_bendradius < prev->pp_od * 0.5 )
	{
		bu_log( "Bend radius (%gmm) is less than outer radius at ( %g %g %g )\n",
			prev->pp_bendradius, V3ARGS( prev->pp_coord ) );
		error_count++;
	}
	cur = BU_LIST_NEXT( wdb_pipept, &prev->l );
	next = BU_LIST_NEXT( wdb_pipept, &cur->l );
	while( BU_LIST_NOT_HEAD( &next->l, headp ) )
	{
		vect_t v1, v2, norm;
		fastf_t v1_len;
		fastf_t angle;

		if( cur->pp_bendradius < cur->pp_od * 0.5 )
		{
			bu_log( "Bend radius (%gmm) is less than outer radius at ( %g %g %g )\n",
				cur->pp_bendradius, V3ARGS( cur->pp_coord ) );
			error_count++;
		}

		VSUB2( v1, prev->pp_coord, cur->pp_coord );
		v1_len = MAGNITUDE( v1 );
		if( v1_len > VDIVIDE_TOL )
		{
			fastf_t inv_len;

			inv_len = 1.0/v1_len;
			VSCALE( v1, v1, inv_len );
		}
		else
			VSETALL( v1, 0.0 )

		VSUB2( v2, next->pp_coord, cur->pp_coord );
		v2_len = MAGNITUDE( v2 );
		if( v2_len > VDIVIDE_TOL )
		{
			fastf_t inv_len;

			inv_len = 1.0/v2_len;
			VSCALE( v2, v2, inv_len );
		}
		else
			VSETALL( v2, 0.0 )

		VCROSS( norm, v1, v2 );
		if( VNEAR_ZERO( norm, SQRT_SMALL_FASTF) )
		{
			new_bend_dist = 0.0;
			goto next_pt;
		}

		angle = bn_pi - acos( VDOT( v1, v2 ) );
		new_bend_dist = cur->pp_bendradius * tan( angle/2.0 );

		if( new_bend_dist + old_bend_dist > v1_len )
		{
			error_count++;
			bu_log( "Bend radii (%gmm) at ( %g %g %g ) and (%gmm) at ( %g %g %g ) are too large\n",
				prev->pp_bendradius, V3ARGS( prev->pp_coord),
				cur->pp_bendradius,V3ARGS( cur->pp_coord ) );
			bu_log( "for pipe segment between ( %g %g %g ) and ( %g %g %g )\n",
				V3ARGS( prev->pp_coord ), V3ARGS( cur->pp_coord ) );
		}
next_pt:
		old_bend_dist = new_bend_dist;
		prev = cur;
		cur = next;
		next = BU_LIST_NEXT( wdb_pipept, &cur->l );
	}

	if( old_bend_dist > v2_len )
	{
		error_count++;
		bu_log( "last segment ( %g %g %g ) to ( %g %g %g ) is too short to allow\n",
			V3ARGS( prev->pp_coord ), V3ARGS( cur->pp_coord ) );
		bu_log( "bend radius of %gmm\n", prev->pp_bendradius );
	}
	return( error_count );
}


/**
 *			R T _ P I P E _ T C L _ G E T
 *
 *  Examples -
 *	db get name V#			get coordinates for vertex #
 *	db get name I#			get inner radius for vertex #
 *	db get name O#			get outer radius for vertex #
 *	db get name R#			get bendradius for vertex #
 *	db get name P#			get all data for vertex #
 *	db get name N			get number of vertices
 */

int
rt_pipe_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
	register struct rt_pipe_internal *pipe=(struct rt_pipe_internal *)intern->idb_ptr;
	struct wdb_pipept *ptp;
	Tcl_DString	ds;
	struct bu_vls	vls;
	int		status=TCL_OK;
	int		seg_no;
	int		num_segs=0;

	RT_PIPE_CK_MAGIC( pipe );

	Tcl_DStringInit( &ds );
	bu_vls_init( &vls );

	/* count segments */
	for( BU_LIST_FOR( ptp, wdb_pipept, &pipe->pipe_segs_head ) )
		num_segs++;

	if( attr == (char *)NULL )
	{
		bu_vls_strcat( &vls, "pipe");

		seg_no = 0;
		for( BU_LIST_FOR( ptp, wdb_pipept, &pipe->pipe_segs_head ) ) {
			bu_vls_printf( &vls, " V%d { %.25G %.25G %.25G } O%d %.25G I%d %.25G R%d %.25G",
				      seg_no, V3ARGS( ptp->pp_coord ),
				      seg_no, ptp->pp_od,
				      seg_no, ptp->pp_id,
				      seg_no, ptp->pp_bendradius );
			seg_no++;
		}
	}
	else if( attr[0] == 'N' )
	{
		bu_vls_printf( &vls, "%d", num_segs );
		goto out;
	}
	else
	{
		int curr_seg=0;

		seg_no = atoi( &attr[1] );
		if( seg_no < 0 || seg_no >= num_segs ) {
			bu_vls_printf( &vls, "segment number out of range (0 - %d)", num_segs-1 );
			status = TCL_ERROR;
			goto out;
		}

		/* find the desired vertex */
		for( BU_LIST_FOR( ptp, wdb_pipept, &pipe->pipe_segs_head ) ) {
			if( curr_seg == seg_no )
				break;
			curr_seg++;
		}

		switch( attr[0] ) {
			case 'V':
				bu_vls_printf( &vls, "%.25G %.25G %.25G", V3ARGS( ptp->pp_coord ) );
				break;
			case 'I':
				bu_vls_printf( &vls, "%.25G", ptp->pp_id );
				break;
			case 'O':
				bu_vls_printf( &vls, "%.25G", ptp->pp_od );
				break;
			case 'R':
				bu_vls_printf( &vls, "%.25G", ptp->pp_bendradius );
				break;
			case 'P':
				bu_vls_printf( &vls, " V%d { %.25G %.25G %.25G } I%d %.25G O%d %.25G R%d %.25G",
					      seg_no, V3ARGS( ptp->pp_coord ),
					      seg_no, ptp->pp_id,
					      seg_no, ptp->pp_od,
					      seg_no, ptp->pp_bendradius );
				break;
			default:
				bu_vls_printf( &vls, "unrecognized attribute (%c), choices are V, I, O, R, or P", attr[0] );
				status = TCL_ERROR;
				break;
		}
	}
out:
	Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
	Tcl_DStringResult( interp, &ds );
	Tcl_DStringFree( &ds );
	bu_vls_free( &vls );

	return( status );

}

int
rt_pipe_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv, struct resource *resp)
{
	struct rt_pipe_internal		*pipe;
	struct wdb_pipept		*ptp;
	Tcl_Obj				*obj, *list;
	int				seg_no;
	int				num_segs;
	int				curr_seg;
	fastf_t				tmp;
	char				*v_str;


	RT_CK_DB_INTERNAL( intern );
	pipe = (struct rt_pipe_internal *)intern->idb_ptr;
	RT_PIPE_CK_MAGIC( pipe );

	while( argc >= 2 ) {

		/* count vertices */
		num_segs = 0;
		if( pipe->pipe_segs_head.forw ) {
			for( BU_LIST_FOR( ptp, wdb_pipept, &pipe->pipe_segs_head ) )
				num_segs++;
		} else {
			BU_LIST_INIT( &pipe->pipe_segs_head );
		}

		if( !isdigit( argv[0][1] ) ) {
			Tcl_SetResult( interp, "no vertex number specified", TCL_STATIC );
			return( TCL_ERROR );
		}

		seg_no = atoi( &argv[0][1] );
		if( seg_no == num_segs ) {
			struct wdb_pipept *new_pt;

			new_pt = (struct wdb_pipept *)bu_calloc( 1, sizeof( struct wdb_pipept ), "New pipe segment" );
			if( num_segs > 0 ) {
				ptp = BU_LIST_LAST( wdb_pipept, &pipe->pipe_segs_head );
				*new_pt = *ptp;		/* struct copy */
				BU_LIST_INSERT( &pipe->pipe_segs_head, &new_pt->l );
				ptp = new_pt;
			} else {
				VSETALL( new_pt->pp_coord, 0.0 );
				new_pt->pp_id = 0.0;
				new_pt->pp_od = 10.0;
				new_pt->pp_bendradius = 20.0;
				BU_LIST_INSERT( &pipe->pipe_segs_head, &new_pt->l );
				ptp = new_pt;
			}
			num_segs++;
		}
		if( seg_no < 0 || seg_no >= num_segs ) {
			Tcl_SetResult( interp, "vertex number out of range", TCL_STATIC );
			return( TCL_ERROR );
		}

		/* get the specified vertex */
		curr_seg = 0;
		for( BU_LIST_FOR( ptp, wdb_pipept, &pipe->pipe_segs_head ) ) {
			if( curr_seg == seg_no )
				break;
			curr_seg++;
		}


		switch( argv[0][0] ) {
			case 'V':
				obj = Tcl_NewStringObj( argv[1], -1 );
				list = Tcl_NewListObj( 0, NULL );
				Tcl_ListObjAppendList( interp, list, obj );
				v_str = Tcl_GetStringFromObj( list, NULL );
				while( isspace( *v_str ) ) v_str++;
				if( *v_str == '\0' ) {
					Tcl_SetResult( interp, "incomplete vertex specification", TCL_STATIC );
					Tcl_DecrRefCount( list );
					return( TCL_ERROR );
				}
				ptp->pp_coord[0] = atof( v_str );
				v_str = bu_next_token( v_str );
				if( *v_str == '\0' ) {
					Tcl_SetResult( interp, "incomplete vertex specification", TCL_STATIC );
					Tcl_DecrRefCount( list );
					return( TCL_ERROR );
				}
				ptp->pp_coord[1] = atof( v_str );
				v_str = bu_next_token( v_str );
				if( *v_str == '\0' ) {
					Tcl_SetResult( interp, "incomplete vertex specification", TCL_STATIC );
					Tcl_DecrRefCount( list );
					return( TCL_ERROR );
				}
				ptp->pp_coord[2] = atof( v_str );
				Tcl_DecrRefCount( list );
				break;
			case 'I':
				tmp = atof( argv[1] );
				if( tmp >= ptp->pp_od ) {
					Tcl_SetResult( interp, "inner diameter must be less than outer diameter", TCL_STATIC );
					return( TCL_ERROR );
				}
				ptp->pp_id = tmp;
				break;
			case 'O':
				tmp = atof( argv[1] );
				if( tmp <= 0.0 ) {
					Tcl_SetResult( interp, "outer diameter cannot be 0.0 or less", TCL_STATIC );
					return( TCL_ERROR );
				}
				if( tmp <= ptp->pp_id ) {
					Tcl_SetResult( interp, "outer diameter must be greater than inner diameter", TCL_STATIC );
					return( TCL_ERROR );
				}
				ptp->pp_od = tmp;
				break;
			case 'R':
				tmp = atof( argv[1] );
				if( tmp < ptp->pp_od * 0.5 ) {
					Tcl_SetResult( interp, "cannot set bend radius to less than outer radius", TCL_STATIC );
					return( TCL_ERROR );
				}
				ptp->pp_bendradius = tmp;
				break;
			default:
				Tcl_SetResult( interp, "unrecognized attribute, choices are V, I, O, or R", TCL_STATIC );
				return( TCL_ERROR );
		}

		argc -= 2;
		argv += 2;
	}

	return( TCL_OK );
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
