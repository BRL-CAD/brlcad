/*                        G _ A R B N . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup g_  */
/** @{ */
/** @file g_arbn.c
 *
 *  Intersect a ray with an Arbitrary Regular Polyhedron with
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
 */

#ifndef lint
static const char RCSarbn[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#include <math.h>
#include <ctype.h>

#include "machine.h"
#include "tcl.h"
#include "vmath.h"
#include "nmg.h"
#include "db.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"


BU_EXTERN(void rt_arbn_print, (const struct soltab *stp) );
BU_EXTERN(void rt_arbn_ifree, (struct rt_db_internal *ip) );

/**
 *  			R T _ A R B N _ P R E P
 *
 *  Returns -
 *	 0	OK
 *	!0	failure
 */
int
rt_arbn_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_arbn_internal	*aip;
	vect_t		work;
	fastf_t		f;
	register int	i;
	int		j;
	int		k;
	int		*used = (int *)0;	/* plane eqn use count */
	const struct bn_tol	*tol = &rtip->rti_tol;

	RT_CK_DB_INTERNAL( ip );
	aip = (struct rt_arbn_internal *)ip->idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	used = (int *)bu_malloc(aip->neqn*sizeof(int), "arbn used[]");

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
			if( BN_VECT_ARE_PARALLEL(dot, tol) )  continue;

			/* Have an edge line, isect with higher numbered planes */
			for( k=j+1; k<aip->neqn; k++ )  {
				register int	m;
				point_t		pt;
				int		next_k;

				next_k = 0;

				if( bn_mkpoint_3planes( pt, aip->eqn[i], aip->eqn[j], aip->eqn[k] ) < 0 )  continue;

				/* See if point is outside arb */
				for( m=0; m<aip->neqn; m++ )  {
					if( i==m || j==m || k==m )  continue;
					if( VDOT(pt, aip->eqn[m])-aip->eqn[m][3] > tol->dist )
					{
						next_k = 1;
						break;
					}
				}
				if( next_k != 0)  continue;

				VMINMAX( stp->st_min, stp->st_max, pt );

				/* Increment "face used" counts */
				used[i]++;
				used[j]++;
				used[k]++;
			}
		}
	}

	/* If any planes were not used, then arbn is not convex */
	for( i=0; i<aip->neqn; i++ )  {
		if( used[i] != 0 )  continue;	/* face was used */
		bu_log("arbn(%s) face %d unused, solid is not convex\n",
			stp->st_name, i);
		bu_free( (char *)used, "arbn used[]");
		return(-1);		/* BAD */
	}
	bu_free( (char *)used, "arbn used[]");

	stp->st_specific = (genptr_t)aip;
	ip->idb_ptr = GENPTR_NULL;	/* indicate we stole it */

	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSUB2SCALE( work, stp->st_max, stp->st_min, 0.5 );

	f = work[X];
	if( work[Y] > f )  f = work[Y];
	if( work[Z] > f )  f = work[Z];
	stp->st_aradius = f;
	stp->st_bradius = MAGNITUDE(work);
	return(0);			/* OK */
}

/**
 *  			R T _ A R B N _ P R I N T
 */
void
rt_arbn_print(register const struct soltab *stp)
{
}

/**
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
rt_arbn_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register struct rt_arbn_internal	*aip =
		(struct rt_arbn_internal *)stp->st_specific;
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
		bu_log("rt_arbn_shoot(%s): 1 hit => MISS\n",
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
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	}
	return(2);			/* HIT */
}

/**
 *			R T _ A R B N _ V S H O T
 */
void
rt_arbn_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		 	       /* Number of ray/object pairs */

{
	rt_vstub( stp, rp, segp, n, ap );
}

/**
 *  			R T _ A R B N _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_arbn_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	register struct rt_arbn_internal *aip =
		(struct rt_arbn_internal *)stp->st_specific;
	int	h;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	h = hitp->hit_surfno;
	if( h < 0 || h > aip->neqn )  {
		bu_log("rt_arbn_norm(%s): hit_surfno=%d?\n", h );
		VSETALL( hitp->hit_normal, 0 );
		return;
	}
	VMOVE( hitp->hit_normal, aip->eqn[h] );
}

/**
 *			R T _ A R B N _ C U R V E
 *
 *  Return the "curvature" of the ARB face.
 *  Pick a principle direction orthogonal to normal, and
 *  indicate no curvature.
 */
void
rt_arbn_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{

	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/**
 *  			R T _ A R B N _ U V
 *
 *  For a hit on a face of an ARB, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the arb_U direction defined by B-A,
 *  v extends along the arb_V direction defined by Nx(B-A).
 */
void
rt_arbn_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
	uvp->uv_u = uvp->uv_v = 0;
	uvp->uv_du = uvp->uv_dv = 0;
}

/**
 *			R T _ A R B N _ F R E E
 */
void
rt_arbn_free(register struct soltab *stp)
{
	register struct rt_arbn_internal *aip =
		(struct rt_arbn_internal *)stp->st_specific;

	bu_free( (char *)aip->eqn, "rt_arbn_internal eqn[]");
	bu_free( (char *)aip, "rt_arbn_internal" );
}

/**
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
int
rt_arbn_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	register struct rt_arbn_internal	*aip;
	register int	i;
	register int	j;
	register int	k;

	RT_CK_DB_INTERNAL(ip);
	aip = (struct rt_arbn_internal *)ip->idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	for( i=0; i<aip->neqn-1; i++ )  {
		for( j=i+1; j<aip->neqn; j++ )  {
			double	dot;
			int	point_count;	/* # points on this line */
			point_t	a,b;		/* start and end points */
			vect_t	dist;

			/* If normals are parallel, no intersection */
			dot = VDOT( aip->eqn[i], aip->eqn[j] );
			if( BN_VECT_ARE_PARALLEL(dot, tol) )  continue;

			/* Have an edge line, isect with all other planes */
			point_count = 0;
			for( k=0; k<aip->neqn; k++ )  {
				register int	m;
				point_t		pt;
				int		next_k;

				next_k = 0;

				if( k==i || k==j )  continue;
				if( bn_mkpoint_3planes( pt, aip->eqn[i], aip->eqn[j], aip->eqn[k] ) < 0 )  continue;

				/* See if point is outside arb */
				for( m=0; m<aip->neqn; m++ )  {
					if( i==m || j==m || k==m )  continue;
					if( VDOT(pt, aip->eqn[m])-aip->eqn[m][3] > tol->dist )
					{
						next_k = 1;
						break;
					}
				}

				if( next_k != 0)  continue;

				if( point_count <= 0 )  {
					RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );
					VMOVE( a, pt );
				} else if( point_count == 1 )  {
					VSUB2( dist, pt, a );
					if( MAGSQ(dist) < tol->dist_sq )  continue;
					RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
					VMOVE( b, pt );
				} else {
					VSUB2( dist, pt, a );
					if( MAGSQ(dist) < tol->dist_sq )  continue;
					VSUB2( dist, pt, b );
					if( MAGSQ(dist) < tol->dist_sq )  continue;
					bu_log("rt_arbn_plot() error, point_count=%d (>2) on edge %d/%d, non-convex\n",
						point_count+1,
						i, j );
					VPRINT(" a", a);
					VPRINT(" b", b);
					VPRINT("pt", pt);
					RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );	/* draw it */
				}
				point_count++;
			}
			/* Point counts of 1 are (generally) not harmful,
			 * occuring on pyramid peaks and the like.
			 */
		}
	}
	return(0);
}

/**
 *			R T _ A R B N _ C L A S S
 */
int
rt_arbn_class(void)
{
	return(0);
}


/* structures used by arbn tessellator */
struct arbn_pts
{
	point_t		pt;		/* coordinates for vertex */
	int		plane_no[3];	/* which planes intersect here */
	struct vertex	**vp;		/* pointer to vertex struct pointer for NMG's */
};
struct arbn_edges
{
	int		v1_no,v2_no;	/* index into arbn_pts for endpoints of edge */
};

#define		LOC(i,j)	i*(aip->neqn)+j

static void
Sort_edges(struct arbn_edges *edges, int *edge_count, const struct rt_arbn_internal *aip)
{
	int face;

	for( face=0 ; face<aip->neqn ; face++ )
	{
		int done=0;
		int edge1,edge2;

		if( edge_count[face] < 3 )
			continue;	/* nothing to sort */

		edge1 = 0;
		edge2 = 0;
		while( !done )
		{
			int edge3;
			int tmp_v1,tmp_v2;

			/* Look for out of order edge (edge2) */
			while( ++edge2 < edge_count[face] &&
				edges[LOC(face,edge1)].v2_no == edges[LOC(face,edge2)].v1_no )
					edge1++;
			if( edge2 == edge_count[face] )
			{
				/* all edges are in order */
				done = 1;
				continue;
			}

			/* look for edge (edge3) that belongs where edge2 is */
			edge3 = edge2 - 1;
			while( ++edge3 < edge_count[face] &&
				edges[LOC(face,edge1)].v2_no != edges[LOC(face,edge3)].v1_no &&
				edges[LOC(face,edge1)].v2_no != edges[LOC(face,edge3)].v2_no );

			if( edge3 == edge_count[face] )
				rt_bomb( "rt_arbn_tess: Sort_edges: Cannot find next edge in loop\n" );

			if( edge2 != edge3 )
			{
				/* swap edge2 and edge3 */
				tmp_v1 = edges[LOC(face,edge2)].v1_no;
				tmp_v2 = edges[LOC(face,edge2)].v2_no;
				edges[LOC(face,edge2)].v1_no = edges[LOC(face,edge3)].v1_no;
				edges[LOC(face,edge2)].v2_no = edges[LOC(face,edge3)].v2_no;
				edges[LOC(face,edge3)].v1_no = tmp_v1;
				edges[LOC(face,edge3)].v2_no = tmp_v2;
			}
			if( edges[LOC(face,edge1)].v2_no == edges[LOC(face,edge2)].v2_no )
			{
				/* reverse order of edge */
				tmp_v1 = edges[LOC(face,edge2)].v1_no;
				edges[LOC(face,edge2)].v1_no = edges[LOC(face,edge2)].v2_no;
				edges[LOC(face,edge2)].v2_no = tmp_v1;
			}

			edge1 = edge2;
		}
	}
}

/**
 *			R T _ A R B N _ T E S S
 *
 *  "Tessellate" an ARB into an NMG data structure.
 *  Purely a mechanical transformation of one faceted object
 *  into another.
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_arbn_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	LOCAL struct rt_arbn_internal	*aip;
	struct shell		*s;
	struct faceuse		**fu;		/* array of faceuses */
	int			nverts;		/* maximum possible number of vertices = neqn!/(3!(neqn-3)! */
	int			point_count=0;	/* actual number of vertices */
	int			face_count=0;	/* actual number of faces built */
	int			i,j,k,l,n;
	struct arbn_pts		*pts;
	struct arbn_edges	*edges;		/* A list of edges for each plane eqn (each face) */
	int			*edge_count;	/* number of edges for each face */
	int			max_edge_count; /* maximium number of edges for any face */
	struct vertex		**verts;	/* Array of pointers to vertex structs */
	struct vertex		***loop_verts;	/* Array of pointers to vertex structs to pass to nmg_cmface */

	RT_CK_DB_INTERNAL(ip);
	aip = (struct rt_arbn_internal *)ip->idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	/* Allocate memory for the vertices */
	nverts = aip->neqn * (aip->neqn-1) * (aip->neqn-2) / 6;
	pts = (struct arbn_pts *)bu_calloc( nverts , sizeof( struct arbn_pts ) , "rt_arbn_tess: pts" );

	/* Allocate memory for arbn_edges */
	edges = (struct arbn_edges *)bu_calloc( aip->neqn*aip->neqn , sizeof( struct arbn_edges ) ,
			"rt_arbn_tess: edges" );
	edge_count = (int *)bu_calloc( aip->neqn , sizeof( int ) , "rt_arbn_tess: edge_count" );

	/* Allocate memory for faceuses */
	fu = (struct faceuse **)bu_calloc( aip->neqn , sizeof( struct faceuse *) , "rt_arbn_tess: fu" );

	/* Calculate all vertices */
	for( i=0 ; i<aip->neqn ; i++ )
	{
		for( j=i+1 ; j<aip->neqn ; j++ )
		{
			for( k=j+1 ; k<aip->neqn ; k++ )
			{
				int keep_point=1;

				if( bn_mkpoint_3planes( pts[point_count].pt, aip->eqn[i], aip->eqn[j], aip->eqn[k]))
					continue;

				for( l=0 ; l<aip->neqn ; l++ )
				{
					if( l == i || l == j || l == k )
						continue;
					if( DIST_PT_PLANE( pts[point_count].pt , aip->eqn[l] ) > tol->dist )
					{
						keep_point = 0;
						break;
					}
				}
				if( keep_point )
				{
					pts[point_count].plane_no[0] = i;
					pts[point_count].plane_no[1] = j;
					pts[point_count].plane_no[2] = k;
					point_count++;
				}
			}
		}
	}

	/* Allocate memory for the NMG vertex pointers */
	verts = (struct vertex **)bu_calloc( point_count , sizeof( struct vertex *) ,
			"rt_arbn_tess: verts" );

	/* Associate points with vertices */
	for( i=0 ; i<point_count ; i++ )
		pts[i].vp = &verts[i];

	/* Check for duplicate points */
	for( i=0 ; i<point_count ; i++ )
	{
		for( j=i+1 ; j<point_count ; j++ )
		{
			vect_t dist;

			VSUB2( dist , pts[i].pt , pts[j].pt )
			if( MAGSQ( dist ) < tol->dist_sq )
			{
				/* These two points should point to the same vertex */
				pts[j].vp = pts[i].vp;
			}
		}
	}

	/* Make list of edges for each face */
	for( i=0 ; i<aip->neqn ; i++ )
	{
		/* look for a point that lies in this face */
		for( j=0 ; j<point_count ; j++ )
		{
			if( pts[j].plane_no[0] != i && pts[j].plane_no[1] != i && pts[j].plane_no[2] != i )
				continue;

			/* look for another point that shares plane "i" and another with this one */
			for( k=j+1 ; k<point_count ; k++ )
			{
				int match=(-1);
				int pt1,pt2;
				int duplicate=0;

				/* skip points not on plane "i" */
				if( pts[k].plane_no[0] != i && pts[k].plane_no[1] != i && pts[k].plane_no[2] != i )
					continue;

				for( l=0 ; l<3 ; l++ )
				{
					for( n=0 ; n<3 ; n++ )
					{
						if( pts[j].plane_no[l] == pts[k].plane_no[n] &&
						    pts[j].plane_no[l] != i )
						{
							match = pts[j].plane_no[l];
							break;
						}
					}
					if( match != (-1) )
						break;
				}

				if( match == (-1) )
					continue;

				/* convert equivalent points to lowest point number */
				pt1 = j;
				pt2 = k;
				for( l=0 ; l<pt1 ; l++ )
				{
					if( pts[pt1].vp == pts[l].vp )
					{
						pt1 = l;
						break;
					}
				}
				for( l=0 ; l<pt2 ; l++ )
				{
					if( pts[pt2].vp == pts[l].vp )
					{
						pt2 = l;
						break;
					}
				}

				/* skip null edges */
				if( pt1 == pt2 )
					continue;

				/* check for duplicate edge */
				for( l=0 ; l<edge_count[i] ; l++ )
				{
					if( (edges[LOC(i,l)].v1_no == pt1 &&
					    edges[LOC(i,l)].v2_no == pt2) ||
					    (edges[LOC(i,l)].v2_no == pt1 &&
					    edges[LOC(i,l)].v1_no == pt2) )
					{
						duplicate = 1;
						break;
					}
				}
				if( duplicate )
					continue;

				/* found an edge belonging to faces "i" and "match" */
				if( edge_count[i] == aip->neqn )
				{
					bu_log( "Too many edges found for one face\n" );
					goto fail;
				}
				edges[LOC( i , edge_count[i] )].v1_no = pt1;
				edges[LOC( i , edge_count[i] )].v2_no = pt2;
				edge_count[i]++;
			}
		}
	}

	/* for each face, sort the list of edges into a loop */
	Sort_edges( edges , edge_count , aip );

	/* Get max number of edges for any face */
	max_edge_count = 0;
	for( i=0 ; i<aip->neqn ; i++ )
		if( edge_count[i] > max_edge_count )
			max_edge_count = edge_count[i];

	/* Allocate memory for array to pass to nmg_cmface */
	loop_verts = (struct vertex ***) bu_calloc( max_edge_count , sizeof( struct vertex **) ,
				"rt_arbn_tess: loop_verts" );

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	/* Make the faces */
	for( i=0 ; i<aip->neqn ; i++ )
	{
		int loop_length=0;

		for( j=0 ; j<edge_count[i] ; j++ )
		{
			/* skip zero length edges */
			if( pts[edges[LOC(i,j)].v1_no].vp == pts[edges[LOC(i,j)].v2_no].vp )
				continue;

			/* put vertex pointers into loop_verts array */
			loop_verts[loop_length] = pts[edges[LOC(i,j)].v2_no].vp;
			loop_length++;
		}

		/* Make the face if there is are least 3 vertices */
		if( loop_length > 2 )
			fu[face_count++] = nmg_cmface( s , loop_verts , loop_length );
	}

	/* Associate vertex geometry */
	for( i=0 ; i<point_count ; i++ )
	{
		if( !(*pts[i].vp) )
			continue;

		if( (*pts[i].vp)->vg_p )
			continue;

		nmg_vertex_gv( *pts[i].vp , pts[i].pt );
	}

	bu_free( (char *)pts , "rt_arbn_tess: pts" );
	bu_free( (char *)edges , "rt_arbn_tess: edges" );
	bu_free( (char *)edge_count , "rt_arbn_tess: edge_count" );
	bu_free( (char *)verts , "rt_arbn_tess: verts" );
	bu_free( (char *)loop_verts , "rt_arbn_tess: loop_verts" );

	/* Associate face geometry */
	for( i=0 ; i<face_count ; i++ )
	{
		if( nmg_fu_planeeqn( fu[i] , tol ) )
		{
			bu_log( "Failed to calculate face plane equation\n" );
			bu_free( (char *)fu , "rt_arbn_tess: fu" );
			nmg_kr( *r );
			*r = (struct nmgregion *)NULL;
			return( -1 );
		}
	}

	bu_free( (char *)fu , "rt_arbn_tess: fu" );

	nmg_fix_normals( s , tol );

	(void)nmg_mark_edges_real( &s->l.magic );

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r, tol );

	return( 0 );

fail:
	bu_free( (char *)pts , "rt_arbn_tess: pts" );
	bu_free( (char *)edges , "rt_arbn_tess: edges" );
	bu_free( (char *)edge_count , "rt_arbn_tess: edge_count" );
	bu_free( (char *)verts , "rt_arbn_tess: verts" );
	return( -1 );
}

/**
 *			R T _ A R B N _ I M P O R T
 *
 *  Convert from "network" doubles to machine specific.
 *  Transform
 */
int
rt_arbn_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	union record		*rp;
	struct rt_arbn_internal	*aip;
	register int	i;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != DBID_ARBN )  {
		bu_log("rt_arbn_import: defective record, id=x%x\n", rp->u_id );
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_ARBN;
	ip->idb_meth = &rt_functab[ID_ARBN];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_arbn_internal), "rt_arbn_internal");
	aip = (struct rt_arbn_internal *)ip->idb_ptr;
	aip->magic = RT_ARBN_INTERNAL_MAGIC;
	aip->neqn = bu_glong( rp->n.n_neqn );
	if( aip->neqn <= 0 )  return(-1);
	aip->eqn = (plane_t *)bu_malloc( aip->neqn*sizeof(plane_t), "arbn plane eqn[]");

	ntohd( (unsigned char *)aip->eqn, (unsigned char *)(&rp[1]), aip->neqn*4 );

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
		VUNITIZE( norm );
		VMOVE( aip->eqn[i], norm );
		aip->eqn[i][3] = VDOT( pt, norm );
	}

	return(0);
}

/**
 *			R T _ A R B N _ E X P O R T
 */
int
rt_arbn_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_arbn_internal	*aip;
	union record		*rec;
	int			ngrans;
	double			*sbuf;		/* scalling buffer */
	register double		*sp;
	register int		i;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_ARBN )  return(-1);
	aip = (struct rt_arbn_internal *)ip->idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	if( aip->neqn <= 0 )  return(-1);

	/*
	 * The network format for a double is 8 bytes and there are 4
	 * doubles per plane equation.
	 */
	ngrans = (aip->neqn * 8 * 4 + sizeof(union record)-1 ) /
		sizeof(union record);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = (ngrans + 1) * sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "arbn external");
	rec = (union record *)ep->ext_buf;

	rec[0].n.n_id = DBID_ARBN;
	(void)bu_plong( rec[0].n.n_neqn, aip->neqn );
	(void)bu_plong( rec[0].n.n_grans, ngrans );

	/* Take the data from the caller, and scale it, into sbuf */
	sp = sbuf = (double *)bu_malloc(
		aip->neqn * sizeof(double) * 4, "arbn temp");
	for( i=0; i<aip->neqn; i++ )  {
		/* Normal is unscaled, should have unit length; d is scaled */
		*sp++ = aip->eqn[i][X];
		*sp++ = aip->eqn[i][Y];
		*sp++ = aip->eqn[i][Z];
		*sp++ = aip->eqn[i][3] * local2mm;
	}

	htond( (unsigned char *)&rec[1], (unsigned char *)sbuf, aip->neqn * 4 );

	bu_free( (char *)sbuf, "arbn temp" );
	return(0);			/* OK */
}


/**
 *			R T _ A R B N _ I M P O R T 5
 *
 *  Convert from "network" doubles to machine specific.
 *  Transform
 */
int
rt_arbn_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_arbn_internal	*aip;
	register int		i;
	unsigned long		neqn;
	int			double_count;
	int			byte_count;

	BU_CK_EXTERNAL( ep );

	neqn = bu_glong((unsigned char *)ep->ext_buf);
	double_count = neqn * ELEMENTS_PER_PLANE;
	byte_count = double_count * SIZEOF_NETWORK_DOUBLE;

	BU_ASSERT_LONG(ep->ext_nbytes, ==, 4+ byte_count);

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_ARBN;
	ip->idb_meth = &rt_functab[ID_ARBN];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_arbn_internal), "rt_arbn_internal");

	aip = (struct rt_arbn_internal *)ip->idb_ptr;
	aip->magic = RT_ARBN_INTERNAL_MAGIC;
	aip->neqn = neqn;
	if (aip->neqn <= 0)  return(-1);
	aip->eqn = (plane_t *)bu_malloc(byte_count, "arbn plane eqn[]");

	ntohd((unsigned char *)aip->eqn, (unsigned char *)ep->ext_buf + 4, double_count);

	/* Transform by the matrix, if we have one that is not the identity */
#	include "noalias.h"
	if( mat && !bn_mat_is_identity( mat ) ) {
		for (i=0; i < aip->neqn; i++) {
			point_t	orig_pt;
			point_t	pt;
			vect_t	norm;

			/* Pick a point on the original halfspace */
			VSCALE( orig_pt, aip->eqn[i], aip->eqn[i][3] );

			/* Transform the point, and the normal */
			MAT4X3VEC( norm, mat, aip->eqn[i] );
			MAT4X3PNT( pt, mat, orig_pt );

			/* Measure new distance from origin to new point */
			VUNITIZE( norm );
			VMOVE( aip->eqn[i], norm );
			aip->eqn[i][3] = VDOT( pt, norm );
		}
	}

	return(0);
}

/**
 *			R T _ A R B N _ E X P O R T 5
 */
int
rt_arbn_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_arbn_internal	*aip;
	register int		i;
	fastf_t			*vec;
	register fastf_t	*sp;
	int			double_count;
	int			byte_count;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_ARBN )  return(-1);
	aip = (struct rt_arbn_internal *)ip->idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	if( aip->neqn <= 0 )  return(-1);

	double_count = aip->neqn * ELEMENTS_PER_PLANE;
	byte_count = double_count * SIZEOF_NETWORK_DOUBLE;

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = 4 + byte_count;
	ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "arbn external");

	(void)bu_plong((unsigned char *)ep->ext_buf, aip->neqn);

	/* Take the data from the caller, and scale it, into vec */
	sp = vec = (double *)bu_malloc(byte_count, "arbn temp");
	for (i=0; i<aip->neqn; i++) {
		/* Normal is unscaled, should have unit length; d is scaled */
		*sp++ = aip->eqn[i][X];
		*sp++ = aip->eqn[i][Y];
		*sp++ = aip->eqn[i][Z];
		*sp++ = aip->eqn[i][3] * local2mm;
	}

	/* Convert from internal (host) to database (network) format */
	htond((unsigned char *)ep->ext_buf + 4, (unsigned char *)vec, double_count);

	bu_free((char *)vec, "arbn temp");
	return(0);			/* OK */
}


/**
 *			R T _ A R B N _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_arbn_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_arbn_internal	*aip =
		(struct rt_arbn_internal *)ip->idb_ptr;
	char	buf[256];
	int	i;

	RT_ARBN_CK_MAGIC(aip);
	sprintf(buf, "arbn bounded by %d planes\n", aip->neqn);
	bu_vls_strcat( str, buf );

	if( !verbose )  return(0);

	for( i=0; i < aip->neqn; i++ )  {
		sprintf(buf, "\t%d: (%g, %g, %g) %g\n",
			i,
			INTCLAMP(aip->eqn[i][X]),		/* should have unit length */
			INTCLAMP(aip->eqn[i][Y]),
			INTCLAMP(aip->eqn[i][Z]),
			INTCLAMP(aip->eqn[i][3] * mm2local) );
		bu_vls_strcat( str, buf );
	}
	return(0);
}


/**
 *			R T _ A R B N _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_arbn_ifree(struct rt_db_internal *ip)
{
	struct rt_arbn_internal	*aip;

	RT_CK_DB_INTERNAL(ip);
	aip = (struct rt_arbn_internal *)ip->idb_ptr;
	RT_ARBN_CK_MAGIC(aip);

	if( aip->neqn > 0 )
		bu_free( (char *)aip->eqn, "rt_arbn_internal eqn[]");
	bu_free( (char *)aip, "rt_arbn_internal" );

	ip->idb_ptr = (genptr_t)0;	/* sanity */
}

/**
 * 		R T _ A R B N _ T C L G E T
 *
 *	Routine to format the parameters of an ARBN primitive for "db get"
 *
 *	Legal requested parameters include:
 *		"N" - number of equations
 *		"P" - list of all the planes
 *		"P#" - the specified plane number (0 based)
 *		no arguments returns everything
 */

int
rt_arbn_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
	register struct rt_arbn_internal *arbn=(struct rt_arbn_internal *)intern->idb_ptr;
	Tcl_DString	ds;
	struct bu_vls	vls;
	int		i;

	RT_ARBN_CK_MAGIC( arbn );

	Tcl_DStringInit( &ds );
	bu_vls_init( &vls );

	if( attr == (char *)NULL ) {
		bu_vls_strcpy( &vls, "arbn" );
		bu_vls_printf( &vls, " N %d", arbn->neqn );
		for( i=0 ; i<arbn->neqn ; i++ ) {
			bu_vls_printf( &vls, " P%d {%.25g %.25g %.25g %.25g}", i,
				       V4ARGS( arbn->eqn[i] ) );
		}
	}
	else if( !strcmp( attr, "N" ) )
		bu_vls_printf( &vls, "%d", arbn->neqn );
	else if( !strcmp( attr, "P" ) ) {
		for( i=0 ; i<arbn->neqn ; i++ ) {
			bu_vls_printf( &vls, " P%d {%.25g %.25g %.25g %.25g}", i,
				       V4ARGS( arbn->eqn[i] ) );
		}
	}
	else if( attr[0] == 'P' ) {
		if( isdigit( attr[1] ) == 0 ) {
			Tcl_SetResult( interp, "ERROR: Illegal plane number\n",
				       TCL_STATIC );
			bu_vls_free( &vls );
			return( TCL_ERROR );
		}

		i = atoi( &attr[1] );
		if( i >= arbn->neqn || i < 0 ) {
			Tcl_SetResult( interp, "ERROR: Illegal plane number\n",
				       TCL_STATIC );
			bu_vls_free( &vls );
			return( TCL_ERROR );
		}

		bu_vls_printf( &vls, "%.25g %.25g %.25g %.25g", V4ARGS( arbn->eqn[i] ) );
	}
	else {
		Tcl_SetResult( interp,"ERROR: Unknown attribute, choices are N, P, or P#\n",
		TCL_STATIC );
		bu_vls_free( &vls );
		return( TCL_ERROR );
	}

	Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
	Tcl_DStringResult( interp, &ds );
	Tcl_DStringFree( &ds );
	bu_vls_free( &vls );
	return( TCL_OK );
}

/**
 *		R T _ A R B N _ T C L A D J U S T
 *
 *	Routine to modify an arbn via the "db adjust" command
 *
 *	Legal parameters are:
 *		"N" - adjust the number of planes (new ones will be zeroed)
 *		"P" - adjust the entire list of planes
 *		"P#" - adjust a specific plane (0 based)
 *		"P+" - add a new plane to the list of planes
 */

int
rt_arbn_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv)
{
	struct rt_arbn_internal *arbn;
	unsigned char		*c;
	int			len;
	int			i, j;
	fastf_t			*new_planes;
	fastf_t			*array;

	RT_CK_DB_INTERNAL( intern );

	arbn = (struct rt_arbn_internal *)intern->idb_ptr;
	RT_ARBN_CK_MAGIC( arbn );

	while( argc >= 2 ) {
		if( !strcmp( argv[0], "N" ) ) {
			i = atoi( argv[1] );
			if( i == arbn->neqn )
				goto cont;
			if( i > 0 ) {
				arbn->eqn = (plane_t *)bu_realloc( arbn->eqn,
						   i * sizeof( plane_t ),
								   "arbn->eqn");
				for( j=arbn->neqn ; j<i ; j++ ) {
					VSETALLN( arbn->eqn[j], 0.0, 4 );
				}
				arbn->neqn = i;
			} else {
				Tcl_SetResult( interp,
				       "ERROR: number of planes must be greater than 0\n",
					TCL_STATIC );
			}
		}
		else if( !strcmp( argv[0], "P" ) ) {
			/* eliminate all the '{' and '}' chars */
			c = (unsigned char *)argv[1];
			while( *c != '\0' ) {
				if( *c == '{' || *c == '}' )
					*c = ' ';
				c++;
			}
			len = 0;
			(void)tcl_list_to_fastf_array( interp, argv[1], &new_planes, &len );

			if( len%4 ) {
				Tcl_SetResult( interp,
				       "ERROR: Incorrect number of plane coefficients\n",
					TCL_STATIC );
				if( len )
					bu_free( (char *)new_planes, "new_planes" );
				return( TCL_ERROR );
			}
			if( arbn->eqn )
				bu_free( (char *)arbn->eqn, "arbn->eqn" );
			arbn->eqn = (plane_t *)new_planes;
			arbn->neqn = len / 4;
			for( i=0 ; i<arbn->neqn ; i++ )
				VUNITIZE( arbn->eqn[i] );
		}
		else if( argv[0][0] == 'P' ) {
			if( argv[0][1] == '+' ) {
				i = arbn->neqn;
				arbn->neqn++;
				arbn->eqn = (plane_t *)bu_realloc( arbn->eqn,
							 (arbn->neqn) * sizeof( plane_t ),
							 "arbn->eqn" );
			}
			else if( isdigit( argv[0][1] ) ) {
				i = atoi( &argv[0][1] );
			} else {
				Tcl_SetResult( interp,
				   "ERROR: illegal argument, choices are P, P#, P+, or N\n",
				   TCL_STATIC );
				return( TCL_ERROR );
			}
			if( i < 0 || i >= arbn->neqn ) {
				Tcl_SetResult( interp,
					       "ERROR: plane number out of range\n",
					       TCL_STATIC );
				return( TCL_ERROR );
			}
			len = 4;
			array = (fastf_t *)&arbn->eqn[i];
			if( tcl_list_to_fastf_array( interp, argv[1],
							  &array, &len ) != 4 ) {
				Tcl_SetResult( interp,
				    "ERROR: incorrect number of coefficients for a plane\n",
				    TCL_STATIC );
				return( TCL_ERROR );
			}
			VUNITIZE( arbn->eqn[i] );
		}
		else {
			Tcl_SetResult( interp,
			      "ERROR: illegal argument, choices are P, P#, P+, or N\n",
			      TCL_STATIC );
			return( TCL_ERROR );
		}
	cont:
		argc -= 2;
		argv += 2;
	}
	return( TCL_OK );
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
