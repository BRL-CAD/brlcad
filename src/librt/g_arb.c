/*                         G _ A R B . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
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
/** @file g_arb.c
 *  Intersect a ray with an Arbitrary Regular Polyhedron with as many as 8 vertices.
 *
 *  An ARB is a convex volume bounded by 4 (pyramid), 5 (wedge), or 6 (box)
 *  planes.  This analysis depends on the properties of objects with convex
 *  hulls.  Let the ray in question be defined such that any point X on the
 *  ray may be expressed as X = P + k D.  Intersect the ray with each of the
 *  planes bounding the ARB as discussed above, and record the values of the
 *  parametric distance k along the ray.
 *
 *  With outward pointing normal vectors,
 *  note that the ray enters the half-space defined by a plane when D cdot N <
 *  0, is parallel to the plane when D cdot N = 0, and exits otherwise.  Find
 *  the entry point farthest away from the starting point bold P, i.e.  it has
 *  the largest value of k among the entry points.
 *  The ray enters the solid at this point.
 *  Similarly, find the exit point closest to point P, i.e. it has
 *  the smallest value of k among the exit points.  The ray exits the solid
 *  here.
 *
 *  This algorithm is due to Cyrus & Beck, USAF.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
#ifndef lint
static const char RCSarb[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "db.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "nurb.h"
#include "./debug.h"


#define RT_SLOPPY_DOT_TOL	0.0087	/* inspired by RT_DOT_TOL, but less tight (.5 deg) */

/* Optionally, one of these for each face.  (Lazy evaluation) */
struct oface {
	fastf_t	arb_UVorig[3];		/* origin of UV coord system */
	fastf_t	arb_U[3];		/* unit U vector (along B-A) */
	fastf_t	arb_V[3];		/* unit V vector (perp to N and U) */
	fastf_t	arb_Ulen;		/* length of U basis (for du) */
	fastf_t	arb_Vlen;		/* length of V basis (for dv) */
};

/* One of these for each face */
struct aface {
	fastf_t	A[3];			/* "A" point */
	plane_t	peqn;			/* Plane equation, unit normal */
};

/* One of these for each ARB, custom allocated to size */
struct arb_specific  {
	int		arb_nmfaces;	/* number of faces */
	struct oface	*arb_opt;	/* pointer to optional info */
	struct aface	arb_face[4];	/* May really be up to [6] faces */
};

/* These hold temp values for the face being prep'ed */
struct prep_arb {
	vect_t		pa_center;	/* center point */
	int		pa_faces;	/* Number of faces done so far */
	int		pa_npts[6];	/* # of points on face's plane */
	int		pa_pindex[4][6]; /* subscr in arbi_pt[] */
	int		pa_clockwise[6];	/* face normal was flipped */
	struct aface	pa_face[6];	/* required face info work area */
	struct oface	pa_opt[6];	/* optional face info work area */
	/* These elements must be initialized before using */
	fastf_t		pa_tol_sq;	/* points-are-equal tol sq */
	int		pa_doopt;	/* compute pa_opt[] stuff */
};

/*
 *  Layout of arb in input record.
 *  Points are listed in "clockwise" order,
 *  to make proper outward-pointing face normals.
 *  (Although the cross product wants counter-clockwise order)
 */
struct arb_info {
	char	*ai_title;
	int	ai_sub[4];
};
static const struct arb_info rt_arb_info[6] = {
	{ "1234", {3, 2, 1, 0} },		/* "bottom" face */
	{ "8765", {4, 5, 6, 7} },		/* "top" face */
	{ "1485", {4, 7, 3, 0} },
	{ "2673", {2, 6, 5, 1} },
	{ "1562", {1, 5, 4, 0} },
	{ "4378", {7, 6, 2, 3} }
};

BU_EXTERN(void rt_arb_ifree, (struct rt_db_internal *) );

const struct bu_structparse rt_arb_parse[] = {
    { "%f", 3, "V1", bu_offsetof(struct rt_arb_internal, pt[0][X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "V2", bu_offsetof(struct rt_arb_internal, pt[1][X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "V3", bu_offsetof(struct rt_arb_internal, pt[2][X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "V4", bu_offsetof(struct rt_arb_internal, pt[3][X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "V5", bu_offsetof(struct rt_arb_internal, pt[4][X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "V6", bu_offsetof(struct rt_arb_internal, pt[5][X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "V7", bu_offsetof(struct rt_arb_internal, pt[6][X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "V8", bu_offsetof(struct rt_arb_internal, pt[7][X]), BU_STRUCTPARSE_FUNC_NULL },
    { {'\0','\0','\0','\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL }
};

/* face definitions for each arb type */
const int rt_arb_faces[5][24] = {
    {0,1,2,3, 0,1,4,5, 1,2,4,5, 0,2,4,5, -1,-1,-1,-1, -1,-1,-1,-1},	/* ARB4 */
    {0,1,2,3, 4,0,1,5, 4,1,2,5, 4,2,3,5, 4,3,0,5, -1,-1,-1,-1},		/* ARB5 */
    {0,1,2,3, 1,2,4,6, 0,4,6,3, 4,1,0,5, 6,2,3,7, -1,-1,-1,-1},		/* ARB6 */
    {0,1,2,3, 4,5,6,7, 0,3,4,7, 1,2,6,5, 0,1,5,4, 3,2,6,4},		/* ARB7 */
    {0,1,2,3, 4,5,6,7, 0,4,7,3, 1,2,6,5, 0,1,5,4, 3,2,6,7},		/* ARB8 */
};

/*  rt_arb_get_cgtype(), rt_arb_std_type(), and rt_arb_centroid()
 *  stolen from mged/arbs.c */
#define NO	0
#define YES	1

/**
 *			R T _ A R B _ G E T _ C G T Y P E
 *
 * C G A R B S :   determines COMGEOM arb types from GED general arbs
 *
 *  Inputs -
 *
 *  Returns -
 *	#	Number of distinct edge vectors
 *		(Number of entries in uvec array)
 *
 *  Implicit returns -
 *	*cgtype		Comgeom type (number range 4..8;  ARB4 .. ARB8).
 *	uvec[8]
 *	svec[11]
 *			Entries [0] and [1] are special
 */
int
rt_arb_get_cgtype(
	int			*cgtype,
	struct rt_arb_internal	*arb,
	const struct bn_tol	*tol,
	register int *uvec,	/* array of unique points */
	register int *svec)	/* array of like points */
{
	register int i,j;
	int	numuvec, unique, done;
	int	si;

	RT_ARB_CK_MAGIC(arb);
	BN_CK_TOL(tol);

	done = NO;		/* done checking for like vectors */

	svec[0] = svec[1] = 0;
	si = 2;

	for(i=0; i<7; i++) {
		unique = YES;
		if(done == NO) {
			svec[si] = i;
		}
		for(j=i+1; j<8; j++) {
			int tmp;
			vect_t vtmp;

			VSUB2( vtmp, arb->pt[i], arb->pt[j] );

			if( fabs(vtmp[0]) > tol->dist) tmp = 0;
			else 	if( fabs(vtmp[1]) > tol->dist) tmp = 0;
			else 	if( fabs(vtmp[2]) > tol->dist) tmp = 0;
			else tmp = 1;

			if( tmp ) {
				if( done == NO )
					svec[++si] = j;
				unique = NO;
			}
		}
		if( unique == NO ) {  	/* point i not unique */
			if( si > 2 && si < 6 ) {
				svec[0] = si - 1;
				if(si == 5 && svec[5] >= 6)
					done = YES;
				si = 6;
			}
			if( si > 6 ) {
				svec[1] = si - 5;
				done = YES;
			}
		}
	}

	if( si > 2 && si < 6 ) {
		svec[0] = si - 1;
	}
	if( si > 6 ) {
		svec[1] = si - 5;
	}
	for(i=1; i<=svec[1]; i++) {
		svec[svec[0]+1+i] = svec[5+i];
	}
	for(i=svec[0]+svec[1]+2; i<11; i++) {
		svec[i] = -1;
	}

	/* find the unique points */
	numuvec = 0;
	for(j=0; j<8; j++) {
		unique = YES;
		for(i=2; i<svec[0]+svec[1]+2; i++) {
			if( j == svec[i] ) {
				unique = NO;
				break;
			}
		}
		if( unique == YES ) {
			uvec[numuvec++] = j;
		}
	}

	/* Figure out what kind of ARB this is */
	switch( numuvec ) {

	case 8:
		*cgtype = ARB8;		/* ARB8 */
		break;

	case 6:
		*cgtype = ARB7;		/* ARB7 */
		break;

	case 4:
		if(svec[0] == 2)
			*cgtype = ARB6;	/* ARB6 */
		else
			*cgtype = ARB5;	/* ARB5 */
		break;

	case 2:
		*cgtype = ARB4;		/* ARB4 */
		break;

	default:
		bu_log( "rt_arb_get_cgtype: bad number of unique vectors (%d)\n",
			  numuvec);

		return(0);
	}
#if 0
	bu_log("uvec: ");
	for(j=0; j<8; j++) bu_log("%d, ", uvec[j]);
	bu_log("\nsvec: ");
	for(j=0; j<11; j++ ) bu_log("%d, ", svec[j]);
	bu_log("\n");
#endif
	return( numuvec );
}

/**
 *			R T _ A R B _ S T D _ T Y P E
 *
 *  Given an ARB in internal form, return it's specific ARB type.
 *
 *  Set tol.dist = 0.0001 to obtain past behavior.
 *
 *  Returns -
 *	0	Error in input ARB
 *	4	ARB4
 *	5	ARB5
 *	6	ARB6
 *	7	ARB7
 *	8	ARB8
 *
 *  Implicit return -
 *	rt_arb_internal pt[] array reorganized into GIFT "standard" order.
 */
int
rt_arb_std_type( const struct rt_db_internal *ip, const struct bn_tol *tol )
{
	struct rt_arb_internal	*arb;
	int uvec[8], svec[11];
	int	cgtype = 0;

	RT_CK_DB_INTERNAL(ip);
	BN_CK_TOL(tol);

	if( ip->idb_type != ID_ARB8 )  bu_bomb("rt_arb_std_type: not ARB!\n");

	arb = (struct rt_arb_internal *)ip->idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	if( rt_arb_get_cgtype( &cgtype, arb, tol, uvec, svec ) == 0 )
		return(0);

	return( cgtype );
}


/**
 *			R T _ A R B _ C E N T R O I D
 *
 * Find the center point for the arb whose values are in the s array,
 * with the given number of verticies.  Return the point in center_pt.
 */
void
rt_arb_centroid( point_t center_pt, const struct rt_arb_internal *arb, int npoints )
{
	register int	j;
	fastf_t		div;
	point_t		sum;

	RT_ARB_CK_MAGIC(arb);

	VSETALL(sum, 0);

	for( j=0; j < npoints; j++ )  {
		VADD2( sum, sum, arb->pt[j] );
	}
	div = 1.0 / npoints;
	VSCALE( center_pt, sum, div );
}

/**
 *			R T _ A R B _ A D D _ P T
 *
 *  Add another point to a struct arb_specific, checking for unique pts.
 *  The first two points are easy.  The third one triggers most of the
 *  plane calculations, and forth and subsequent ones are merely
 *  checked for validity.
 *
 *  Returns -
 *	 0	point was accepted
 *	-1	point was rejected
 */
HIDDEN int
rt_arb_add_pt(register pointp_t point, const char *title, struct prep_arb *pap, int ptno, const char *name)



   		     	/* current point # on face */

{
	LOCAL vect_t	work;
	LOCAL vect_t	P_A;		/* new point minus A */
	FAST fastf_t	f;
	register struct aface	*afp;
	register struct oface	*ofp;

	afp = &pap->pa_face[pap->pa_faces];
	ofp = &pap->pa_opt[pap->pa_faces];

	/* The first 3 points are treated differently */
	switch( ptno )  {
	case 0:
		VMOVE( afp->A, point );
		if( pap->pa_doopt )  {
			VMOVE( ofp->arb_UVorig, point );
		}
		return(0);				/* OK */
	case 1:
		VSUB2( ofp->arb_U, point, afp->A );	/* B-A */
		f = MAGNITUDE( ofp->arb_U );
		if( NEAR_ZERO( f, SQRT_SMALL_FASTF ) )  {
			return(-1);			/* BAD */
		}
		ofp->arb_Ulen = f;
		f = 1/f;
		VSCALE( ofp->arb_U, ofp->arb_U, f );
		/* Note that arb_U is used to build N, below */
		return(0);				/* OK */
	case 2:
		VSUB2( P_A, point, afp->A );	/* C-A */
		/* Pts are given clockwise, so reverse terms of cross prod. */
		/* peqn = (C-A)x(B-A), which points inwards */
		VCROSS( afp->peqn, P_A, ofp->arb_U );
		/* Check for co-linear, ie, |(B-A)x(C-A)| ~= 0 */
		f = MAGNITUDE( afp->peqn );
		if( NEAR_ZERO(f,RT_SLOPPY_DOT_TOL) )  {
			return(-1);			/* BAD */
		}
		f = 1/f;
		VSCALE( afp->peqn, afp->peqn, f );

		if( pap->pa_doopt )  {
			/*
			 * Get vector perp. to AB in face of plane ABC.
			 * Scale by projection of AC, make this V.
			 */
			VCROSS( work, afp->peqn, ofp->arb_U );
			VUNITIZE( work );
			f = VDOT( work, P_A );
			VSCALE( ofp->arb_V, work, f );
			f = MAGNITUDE( ofp->arb_V );
			ofp->arb_Vlen = f;
			f = 1/f;
			VSCALE( ofp->arb_V, ofp->arb_V, f );

			/* Check for new Ulen */
			VSUB2( P_A, point, ofp->arb_UVorig );
			f = VDOT( P_A, ofp->arb_U );
			if( f > ofp->arb_Ulen ) {
				ofp->arb_Ulen = f;
			} else if( f < 0.0 ) {
				VJOIN1( ofp->arb_UVorig, ofp->arb_UVorig, f,
					ofp->arb_U );
				ofp->arb_Ulen += (-f);
			}
		}

		/*
		 *  If C-A is clockwise from B-A, then the normal
		 *  points inwards, so we need to fix it here.
		 *  Build a vector from the centroid to vertex A.
		 *  If the surface normal points in the same direction,
		 *  then the vertcies were given in CCW order;
		 *  otherwise, vertices were given in CW order, and
		 *  the normal needs to be flipped.
		 */
		VSUB2( work, afp->A, pap->pa_center );
		f = VDOT( work, afp->peqn );
		if( f < 0.0 )  {
			VREVERSE(afp->peqn, afp->peqn);	/* "fix" normal */
			pap->pa_clockwise[pap->pa_faces] = 1;
		} else {
			pap->pa_clockwise[pap->pa_faces] = 0;
		}
		afp->peqn[3] = VDOT( afp->peqn, afp->A );
		return(0);				/* OK */
	default:
		/* Merely validate 4th and subsequent points */
		if( pap->pa_doopt )  {
			VSUB2( P_A, point, ofp->arb_UVorig );
			/* Check for new Ulen, Vlen */
			f = VDOT( P_A, ofp->arb_U );
			if( f > ofp->arb_Ulen ) {
				ofp->arb_Ulen = f;
			} else if( f < 0.0 ) {
				VJOIN1( ofp->arb_UVorig, ofp->arb_UVorig, f,
					ofp->arb_U );
				ofp->arb_Ulen += (-f);
			}
			f = VDOT( P_A, ofp->arb_V );
			if( f > ofp->arb_Vlen ) {
				ofp->arb_Vlen = f;
			} else if( f < 0.0 ) {
				VJOIN1( ofp->arb_UVorig, ofp->arb_UVorig, f,
					ofp->arb_V );
				ofp->arb_Vlen += (-f);
			}
		}

		VSUB2( P_A, point, afp->A );
		VUNITIZE( P_A );		/* Checking direction only */
		f = VDOT( afp->peqn, P_A );
		if( ! NEAR_ZERO(f,RT_SLOPPY_DOT_TOL) )  {
			/* Non-planar face */
			bu_log("arb(%s): face %s[%d] non-planar, dot=%g\n",
				name, title, ptno, f );
#ifdef CONSERVATIVE
			return(-1);			/* BAD */
#endif
		}
		return(0);				/* OK */
	}
	/* NOTREACHED */
}

/**
 *			R T _ A R B _ M K _ P L A N E S
 *
 *  Given an rt_arb_internal structure with 8 points in it,
 *  compute the face information.
 *
 *  Returns -
 *	 0	OK
 *	<0	failure
 */
HIDDEN int
rt_arb_mk_planes(register struct prep_arb *pap, struct rt_arb_internal *aip, const char *name)
{
	LOCAL vect_t	sum;		/* Sum of all endpoints */
	register int	i;
	register int	j;
	register int	k;
	int		equiv_pts[8];

	/*
	 *  Determine a point which is guaranteed to be within the solid.
	 *  This is done by averaging all the vertices.  This center is
	 *  needed for rt_arb_add_pt, which demands a point inside the solid.
	 *  The center of the enclosing RPP strategy used for the bounding
	 *  sphere can be tricked by thin plates which are non-axis aligned,
	 *  so this dual-strategy is required.  (What a bug hunt!).
	 */
	VSETALL( sum, 0 );
#	include "noalias.h"
	for( i=0; i<8; i++ )  {
		VADD2( sum, sum, aip->pt[i] );
	}
	VSCALE( pap->pa_center, sum, 0.125 );	/* sum/8 */

	/*
	 *  Find all points that are equivalent, within the specified tol.
	 *  Build the array equiv_pts[] so that it is indexed by
	 *  vertex number, and returns the lowest numbered equivalent
	 *  vertex (or its own vertex number, if non-equivalent).
	 */
	equiv_pts[0] = 0;
	for( i=1; i<8; i++ )  {
		for( j = i-1; j >= 0; j-- )  {
			/* Compare vertices I and J */
			LOCAL vect_t		work;

			VSUB2( work, aip->pt[i], aip->pt[j] );
			if( MAGSQ( work ) < pap->pa_tol_sq )  {
				/* Points I and J are the same, J is lower */
				equiv_pts[i] = equiv_pts[j];
				goto next_point;
			}
		}
		equiv_pts[i] = i;
	next_point: ;
	}
	if( RT_G_DEBUG & DEBUG_ARB8 )  {
		bu_log("arb(%s) equiv_pts[] = %d %d %d %d %d %d %d %d\n",
			name,
			equiv_pts[0], equiv_pts[1], equiv_pts[2], equiv_pts[3],
			equiv_pts[4], equiv_pts[5], equiv_pts[6], equiv_pts[7]);
	}

	pap->pa_faces = 0;
	for( i=0; i<6; i++ )  {
		int		npts;

		npts = 0;
		for( j=0; j<4; j++ )  {
			int	pt_index;

			pt_index = rt_arb_info[i].ai_sub[j];
			if( RT_G_DEBUG & DEBUG_ARB8 )  {
				bu_log("face %d, j=%d, npts=%d, orig_vert=%d, vert=%d\n",
					i, j, npts,
					pt_index, equiv_pts[pt_index] );
			}
			pt_index = equiv_pts[pt_index];

			/* Verify that this point is not the same
			 * as an earlier point, by checking point indices
			 */
#			include "noalias.h"
			for( k = npts-1; k >= 0; k-- )  {
				if( pap->pa_pindex[k][pap->pa_faces] == pt_index )  {
					/* Point is the same -- skip it */
					goto skip_pt;
				}
			}
			if( rt_arb_add_pt( aip->pt[pt_index],
			    rt_arb_info[i].ai_title, pap, npts, name ) == 0 )  {
				/* Point was accepted */
				pap->pa_pindex[npts][pap->pa_faces] = pt_index;
				npts++;
			}

skip_pt:		;
		}

		if( npts < 3 )  {
			/* This face is BAD */
			continue;
		}

		if( pap->pa_doopt )  {
			register struct oface	*ofp;

			ofp = &pap->pa_opt[pap->pa_faces];
			/* Scale U and V basis vectors by
			 * the inverse of Ulen and Vlen
			 */
			ofp->arb_Ulen = 1.0 / ofp->arb_Ulen;
			ofp->arb_Vlen = 1.0 / ofp->arb_Vlen;
			VSCALE( ofp->arb_U, ofp->arb_U, ofp->arb_Ulen );
			VSCALE( ofp->arb_V, ofp->arb_V, ofp->arb_Vlen );
		}

		pap->pa_npts[pap->pa_faces] = npts;
		pap->pa_faces++;
	}
	if( pap->pa_faces < 4  || pap->pa_faces > 6 )  {
		bu_log("arb(%s):  only %d faces present\n",
			name, pap->pa_faces);
		return(-1);			/* Error */
	}
	return(0);			/* OK */
}

/**
 *			R T _ A R B _ S E T U P
 *
 *  This is packaged as a separate function, so that it can also be
 *  called "on the fly" from the UV mapper.
 *
 *  Returns -
 *	 0	OK
 *	!0	failure
 */
HIDDEN int
rt_arb_setup(struct soltab *stp, struct rt_arb_internal *aip, struct rt_i *rtip, int uv_wanted)
{
	register int		i;
	struct prep_arb		pa;

	RT_ARB_CK_MAGIC(aip);

	pa.pa_doopt = uv_wanted;
	pa.pa_tol_sq = rtip->rti_tol.dist_sq;

	if( rt_arb_mk_planes( &pa, aip, stp->st_dp->d_namep ) < 0 )  {
		return(-2);		/* Error */
	}

	/*
	 *  Allocate a private copy of the accumulated parameters
	 *  of exactly the right size.
	 *  The size to malloc is chosen based upon the
	 *  exact number of faces.
	 */
	{
		register struct arb_specific	*arbp;
		if( (arbp = (struct arb_specific *)stp->st_specific) == 0 )  {
			arbp = (struct arb_specific *)bu_malloc(
				sizeof(struct arb_specific) +
				sizeof(struct aface) * (pa.pa_faces - 4),
				"arb_specific" );
			stp->st_specific = (genptr_t)arbp;
		}
		arbp->arb_nmfaces = pa.pa_faces;
		bcopy( (char *)pa.pa_face, (char *)arbp->arb_face,
			pa.pa_faces * sizeof(struct aface) );

		if( uv_wanted )  {
			register struct oface	*ofp;

			/*
			 * To avoid a multi-processor race here,
			 * copy the data first, THEN update arb_opt,
			 * because arb_opt doubles as the "UV avail" flag.
			 */
			ofp = (struct oface *)bu_malloc(
				pa.pa_faces * sizeof(struct oface), "arb_opt");
			bcopy( (char *)pa.pa_opt, (char *)ofp,
				pa.pa_faces * sizeof(struct oface) );
			arbp->arb_opt = ofp;
		} else {
			arbp->arb_opt = (struct oface *)0;
		}
	}

	/*
	 * Compute bounding sphere which contains the bounding RPP.
	 * Find min and max of the point co-ordinates to find the
	 * bounding RPP.  Note that this center is NOT guaranteed
	 * to be contained within the solid!
	 */
	{
		LOCAL vect_t		work;
		register fastf_t	f;

#		include "noalias.h"
		for( i=0; i< 8; i++ ) {
			VMINMAX( stp->st_min, stp->st_max, aip->pt[i] );
		}
		VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
		VSUB2SCALE( work, stp->st_max, stp->st_min, 0.5 );

		f = work[X];
		if( work[Y] > f )  f = work[Y];
		if( work[Z] > f )  f = work[Z];
		stp->st_aradius = f;
		stp->st_bradius = MAGNITUDE(work);
	}
	return(0);		/* OK */
}

/**
 *  			R T _ A R B _ P R E P
 *
 *  This is the actual LIBRT "prep" interface.
 *
 *  Returns -
 *	 0	OK
 *	!0	failure
 */
int
rt_arb_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_arb_internal	*aip;

	aip = (struct rt_arb_internal *)ip->idb_ptr;
	RT_ARB_CK_MAGIC(aip);

	return( rt_arb_setup( stp, aip, rtip, 0 ) );
}

/**
 *  			R T _ A R B _ P R I N T
 */
void
rt_arb_print(register const struct soltab *stp)
{
	register struct arb_specific *arbp =
		(struct arb_specific *)stp->st_specific;
	register struct aface	*afp;
	register int i;

	if( arbp == (struct arb_specific *)0 )  {
		bu_log("arb(%s):  no faces\n", stp->st_name);
		return;
	}
	bu_log("%d faces:\n", arbp->arb_nmfaces);
	for( i=0; i < arbp->arb_nmfaces; i++ )  {
		afp = &(arbp->arb_face[i]);
		VPRINT( "A", afp->A );
		HPRINT( "Peqn", afp->peqn );
		if( arbp->arb_opt )  {
			register struct oface	*op;
			op = &(arbp->arb_opt[i]);
			VPRINT( "UVorig", op->arb_UVorig );
			VPRINT( "U", op->arb_U );
			VPRINT( "V", op->arb_V );
			bu_log( "Ulen = %g, Vlen = %g\n",
				op->arb_Ulen, op->arb_Vlen);
		}
	}
}

/**
 *			R T _ A R B _ S H O T
 *
 * Function -
 *	Shoot a ray at an ARB8.
 *
 * Algorithm -
 * 	The intersection distance is computed for each face.
 *  The largest IN distance and the smallest OUT distance are
 *  used as the entry and exit points.
 *
 * Returns -
 *	0	MISS
 *	>0	HIT
 */
int
rt_arb_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	struct arb_specific *arbp = (struct arb_specific *)stp->st_specific;
	LOCAL int		iplane, oplane;
	LOCAL fastf_t		in, out;	/* ray in/out distances */
	register struct aface	*afp;
	register int		j;

	in = -INFINITY;
	out = INFINITY;
	iplane = oplane = -1;

	if (RT_G_DEBUG & DEBUG_ARB8) {
		bu_log("\n\n------------\n arb: ray point %g %g %g -> %g %g %g\n",
			V3ARGS(rp->r_pt),
			V3ARGS(rp->r_dir));
	}

	/* consider each face */
	for( afp = &arbp->arb_face[j=arbp->arb_nmfaces-1]; j >= 0; j--, afp-- )  {
		FAST fastf_t	dn;		/* Direction dot Normal */
		FAST fastf_t	dxbdn;
		FAST fastf_t	s;

		/* XXX some of this math should be prep work
		 * (including computing dxbdn/dn ?) *$*/
		dxbdn = VDOT( afp->peqn, rp->r_pt ) - afp->peqn[3];
		dn = -VDOT( afp->peqn, rp->r_dir );

	        if (RT_G_DEBUG & DEBUG_ARB8) {
	        	HPRINT("arb: Plane Equation", afp->peqn);
			bu_log("arb: dn=%g dxbdn=%g s=%g\n", dn, dxbdn, dxbdn/dn);
	        }

		if( dn < -SQRT_SMALL_FASTF )  {
			/* exit point, when dir.N < 0.  out = min(out,s) */
			if( out > (s = dxbdn/dn) )  {
				out = s;
				oplane = j;
			}
		} else if ( dn > SQRT_SMALL_FASTF )  {
			/* entry point, when dir.N > 0.  in = max(in,s) */
			if( in < (s = dxbdn/dn) )  {
				in = s;
				iplane = j;
			}
		}  else  {
			/* ray is parallel to plane when dir.N == 0.
			 * If it is outside the solid, stop now.
			 * Allow very small amount of slop, to catch
			 * rays that lie very nearly in the plane of a face.
			 */
			if( dxbdn > SQRT_SMALL_FASTF )
				return( 0 );	/* MISS */
		}
		if( in > out )
			return( 0 );	/* MISS */
	}
	/* Validate */
	if( iplane == -1 || oplane == -1 )  {
		bu_log("rt_arb_shoot(%s): 1 hit => MISS\n",
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

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;
/**
 *			R T _ A R B _ V S H O T
 *
 *  This is the Becker vector version
 */
void
rt_arb_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		 	       /* Number of ray/object pairs */

{
	register int    j, i;
	register struct arb_specific *arbp;
	FAST fastf_t	dn;		/* Direction dot Normal */
	FAST fastf_t	dxbdn;
	FAST fastf_t	s;

	/* Intialize return values */
#	include "noalias.h"
	for(i = 0; i < n; i++){
		segp[i].seg_stp = stp[i];	/* Assume hit, if 0 then miss */
                segp[i].seg_in.hit_dist = -INFINITY;    /* used as in */
                segp[i].seg_in.hit_surfno = -1;		/* used as iplane */
                segp[i].seg_out.hit_dist = INFINITY;    /* used as out */
                segp[i].seg_out.hit_surfno = -1;	/* used as oplane */
/**                segp[i].seg_next = SEG_NULL;**/
	}

	/* consider each face */
	for(j = 0; j < 6; j++)  {
		/* for each ray/arb_face pair */
#		include "noalias.h"
		for(i = 0; i < n; i++)  {
			if (stp[i] == 0) continue;	/* skip this ray */
			if ( segp[i].seg_stp == 0 ) continue;	/* miss */

			arbp= (struct arb_specific *) stp[i]->st_specific;
			if ( arbp->arb_nmfaces <= j )
				continue; /* faces of this ARB are done */

			dxbdn = VDOT( arbp->arb_face[j].peqn, rp[i]->r_pt ) -
				arbp->arb_face[j].peqn[3];
			if( (dn = -VDOT( arbp->arb_face[j].peqn, rp[i]->r_dir )) <
							-SQRT_SMALL_FASTF )  {
			   /* exit point, when dir.N < 0.  out = min(out,s) */
			   if( segp[i].seg_out.hit_dist > (s = dxbdn/dn) )  {
			   	   segp[i].seg_out.hit_dist = s;
				   segp[i].seg_out.hit_surfno = j;
			   }
			} else if ( dn > SQRT_SMALL_FASTF )  {
			   /* entry point, when dir.N > 0.  in = max(in,s) */
			   if( segp[i].seg_in.hit_dist < (s = dxbdn/dn) )  {
				   segp[i].seg_in.hit_dist = s;
				   segp[i].seg_in.hit_surfno = j;
			   }
		        }  else  {
			   /* ray is parallel to plane when dir.N == 0.
			    * If it is outside the solid, stop now */
			   if( dxbdn > SQRT_SMALL_FASTF ) {
				SEG_MISS(segp[i]);		/* MISS */
			   }
			}
		        if(segp[i].seg_in.hit_dist > segp[i].seg_out.hit_dist) {
			   SEG_MISS(segp[i]);		/* MISS */
			}
		} /* for each ray/arb_face pair */
	} /* for each arb_face */

	/*
	 *  Validate for each ray/arb_face pair
	 *  Segment was initialized as "good" (seg_stp set valid);
	 *  that is revoked here on misses.
	 */
#	include "noalias.h"
	for(i = 0; i < n; i++){
		if (stp[i] == 0) continue;		/* skip this ray */
		if ( segp[i].seg_stp == 0 ) continue;	/* missed */

		if( segp[i].seg_in.hit_surfno == -1 ||
		    segp[i].seg_out.hit_surfno == -1 )  {
			SEG_MISS(segp[i]);		/* MISS */
		}
		else if(segp[i].seg_in.hit_dist >= segp[i].seg_out.hit_dist ||
			segp[i].seg_out.hit_dist >= INFINITY ) {
			SEG_MISS(segp[i]);		/* MISS */
		}
	}
}

/**
 *  			R T _ A R B _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_arb_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	register struct arb_specific *arbp =
		(struct arb_specific *)stp->st_specific;
	register int	h;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	h = hitp->hit_surfno;
	VMOVE( hitp->hit_normal, arbp->arb_face[h].peqn );
}

/**
 *			R T _ A R B _ C U R V E
 *
 *  Return the "curvature" of the ARB face.
 *  Pick a principle direction orthogonal to normal, and
 *  indicate no curvature.
 */
void
rt_arb_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{

	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/**
 *  			R T _ A R B _ U V
 *
 *  For a hit on a face of an ARB, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the arb_U direction defined by B-A,
 *  v extends along the arb_V direction defined by Nx(B-A).
 */
void
rt_arb_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
	register struct arb_specific *arbp =
		(struct arb_specific *)stp->st_specific;
	struct oface	*ofp;
	LOCAL vect_t	P_A;
	LOCAL fastf_t	r;
	LOCAL vect_t	rev_dir;
	LOCAL fastf_t	dot_N;
	LOCAL vect_t	UV_dir;
	LOCAL fastf_t	*norm;
	LOCAL fastf_t	min_r_U, min_r_V;

	if( arbp->arb_opt == (struct oface *)0 )  {
		register int		ret = 0;
		struct rt_db_internal	intern;
		struct rt_arb_internal	*aip;

		if( rt_db_get_internal( &intern, stp->st_dp, ap->a_rt_i->rti_dbip, stp->st_matp, ap->a_resource ) < 0 )  {
			bu_log("rt_arb_uv(%s) rt_db_get_internal failure\n",
				stp->st_name);
			return;
		}
		RT_CK_DB_INTERNAL( &intern );
		aip = (struct rt_arb_internal *)intern.idb_ptr;
		RT_ARB_CK_MAGIC(aip);

		/*
		 *  The double check of arb_opt is to avoid the case
		 *  where another processor did the UV setup while
		 *  this processor was waiting in bu_semaphore_acquire().
		 */
		bu_semaphore_acquire( RT_SEM_MODEL );
		if( arbp->arb_opt == (struct oface *)0 )  {
			ret = rt_arb_setup(stp, aip, ap->a_rt_i, 1 );
		}
		bu_semaphore_release( RT_SEM_MODEL );

		rt_db_free_internal( &intern, ap->a_resource );

		if( ret != 0 || arbp->arb_opt == (struct oface *)0 )  {
			bu_log("rt_arb_uv(%s) dyanmic setup failure st_specific=x%x, optp=x%x\n",
				stp->st_name,
		    		stp->st_specific, arbp->arb_opt );
			return;
		}
		if(RT_G_DEBUG&DEBUG_SOLIDS)  rt_pr_soltab( stp );
	}

	ofp = &arbp->arb_opt[hitp->hit_surfno];

	VSUB2( P_A, hitp->hit_point, ofp->arb_UVorig );
	/* Flipping v is an artifact of how the faces are built */
	uvp->uv_u = VDOT( P_A, ofp->arb_U );
	uvp->uv_v = 1.0 - VDOT( P_A, ofp->arb_V );
	if( uvp->uv_u < 0 || uvp->uv_v < 0 || uvp->uv_u > 1 || uvp->uv_v > 1 )  {
		bu_log("arb_uv: bad uv=%g,%g\n", uvp->uv_u, uvp->uv_v);
		/* Fix it up */
		if( uvp->uv_u < 0 )  uvp->uv_u = (-uvp->uv_u);
		if( uvp->uv_v < 0 )  uvp->uv_v = (-uvp->uv_v);
	}
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
	min_r_U = r * ofp->arb_Ulen;
	min_r_V = r * ofp->arb_Vlen;
	VREVERSE( rev_dir, ap->a_ray.r_dir )
	norm = &arbp->arb_face[hitp->hit_surfno].peqn[0];
	dot_N = VDOT( rev_dir, norm );
	VJOIN1( UV_dir, rev_dir, -dot_N, norm )
	VUNITIZE( UV_dir )
	uvp->uv_du = r * VDOT( UV_dir, ofp->arb_U ) / dot_N;
	uvp->uv_dv = r * VDOT( UV_dir, ofp->arb_V ) / dot_N;
	if( uvp->uv_du < 0.0 )
		uvp->uv_du = -uvp->uv_du;
	if( uvp->uv_du < min_r_U )
		uvp->uv_du = min_r_U;
	if( uvp->uv_dv < 0.0 )
		uvp->uv_dv = -uvp->uv_dv;
	if( uvp->uv_dv < min_r_V )
		uvp->uv_dv = min_r_V;
}

/**
 *			R T _ A R B _ F R E E
 */
void
rt_arb_free(register struct soltab *stp)
{
	register struct arb_specific *arbp =
		(struct arb_specific *)stp->st_specific;

	if( arbp->arb_opt )
		bu_free( (char *)arbp->arb_opt, "arb_opt" );
	bu_free( (char *)arbp, "arb_specific" );
}

#define ARB_FACE( valp, a, b, c, d ) \
	RT_ADD_VLIST( vhead, valp[a], BN_VLIST_LINE_MOVE ); \
	RT_ADD_VLIST( vhead, valp[b], BN_VLIST_LINE_DRAW ); \
	RT_ADD_VLIST( vhead, valp[c], BN_VLIST_LINE_DRAW ); \
	RT_ADD_VLIST( vhead, valp[d], BN_VLIST_LINE_DRAW );

/**
 *  			R T _ A R B _ P L O T
 *
 *  Plot an ARB by tracing out four "U" shaped contours
 *  This draws each edge only once.
 *  XXX No checking for degenerate faces is done, but probably should be.
 */
int
rt_arb_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	struct rt_arb_internal	*aip;

	RT_CK_DB_INTERNAL(ip);
	aip = (struct rt_arb_internal *)ip->idb_ptr;
	RT_ARB_CK_MAGIC(aip);

	ARB_FACE( aip->pt, 0, 1, 2, 3 );
	ARB_FACE( aip->pt, 4, 0, 3, 7 );
	ARB_FACE( aip->pt, 5, 4, 7, 6 );
	ARB_FACE( aip->pt, 1, 5, 6, 2 );
	return(0);
}

/**
 *			R T _ A R B _ C L A S S
 */
int
rt_arb_class(const struct soltab *stp, const fastf_t *min, const fastf_t *max, const struct bn_tol *tol)
{
	register struct arb_specific *arbp =
		(struct arb_specific *)stp->st_specific;
	register int i;

	if( arbp == (struct arb_specific *)0 ) {
		bu_log("arb(%s): no faces\n", stp->st_name);
		return RT_CLASSIFY_UNIMPLEMENTED;
	}

	for( i=0; i<arbp->arb_nmfaces; i++ ) {
		if( bn_hlf_class( arbp->arb_face[i].peqn, min, max, tol ) ==
		    BN_CLASSIFY_OUTSIDE )
			return RT_CLASSIFY_OUTSIDE;
	}

	/* We need to test for RT_CLASSIFY_INSIDE vs. RT_CLASSIFY_OVERLAPPING!
	   XXX Do this soon */
	return RT_CLASSIFY_UNIMPLEMENTED; /* let the caller assume the worst */
}

/**
 *			R T _ A R B _ I M P O R T
 *
 *  Import an ARB8 from the database format to the internal format.
 *  There are two parts to this:  First, the database is presently
 *  single precision binary floating point.
 *  Second, the ARB in the database is represented as a vector
 *  from the origin to the first point, and 7 vectors
 *  from the first point to the remaining points.  In 1979 it seemed
 *  like a good idea...
 *
 *  Convert from vector to point notation
 *  by rotating each vector and adding in the base vector.
 */
int
rt_arb_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_arb_internal	*aip;
	union record		*rp;
	register int		i;
	LOCAL vect_t		work;
	LOCAL fastf_t		vec[3*8];

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		bu_log("rt_arb_import: defective record, id=x%x\n", rp->u_id);
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_ARB8;
	ip->idb_meth = &rt_functab[ID_ARB8];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_arb_internal), "rt_arb_internal");
	aip = (struct rt_arb_internal *)ip->idb_ptr;
	aip->magic = RT_ARB_INTERNAL_MAGIC;

	/* Convert from database to internal format */
	rt_fastf_float( vec, rp->s.s_values, 8 );

	/*
	 * Convert from vector notation (in database) to point notation.
	 */
	MAT4X3PNT( aip->pt[0], mat, &vec[0] );

#	include "noalias.h"
	for( i=1; i<8; i++ )  {
		VADD2( work, &vec[0*3], &vec[i*3] );
		MAT4X3PNT( aip->pt[i], mat, work );
	}
	return(0);			/* OK */
}

/**
 *			R T _ A R B _ E X P O R T
 */
int
rt_arb_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_arb_internal	*aip;
	union record		*rec;
	register int		i;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_ARB8 )  return(-1);
	aip = (struct rt_arb_internal *)ip->idb_ptr;
	RT_ARB_CK_MAGIC(aip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "arb external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->s.s_type = GENARB8;

	/* NOTE: This also converts to dbfloat_t */
	VSCALE( &rec->s.s_values[3*0], aip->pt[0], local2mm );
	for( i=1; i < 8; i++ )  {
		VSUB2SCALE( &rec->s.s_values[3*i],
			aip->pt[i], aip->pt[0], local2mm );
	}
	return(0);
}

/**
 *			R T _ A R B _ I M P O R T 5
 *
 * Import an arb from the db5 format and convert to the internal structure.
 * Code duplicated from rt_arb_import() with db5 help from g_ell.c
 */
int
rt_arb_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_arb_internal *aip;
	register int		i;
	fastf_t			vec[3*8];

	BU_CK_EXTERNAL( ep );
	BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3*8);
	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_ARB8;
	ip->idb_meth = &rt_functab[ID_ARB8];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_arb_internal), "rt_arb_internal");

	aip = (struct rt_arb_internal *)ip->idb_ptr;
	aip->magic = RT_ARB_INTERNAL_MAGIC;

	/* Convert from database (network) to internal (host) format */
	ntohd( (unsigned char *)vec, ep->ext_buf, 8*3);
	for (i=0; i<8; i++) {
		MAT4X3PNT( aip->pt[i], mat, &vec[i*3]);
	}
	return 0;	/* OK */
}
/**
 *			R T _ A R B _ E X P O R T 5
 */
int
rt_arb_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_arb_internal	*aip;
	fastf_t			vec[3*8];
	register int		i;

	RT_CK_DB_INTERNAL(ip);
	if (ip->idb_type != ID_ARB8) return -1;
	aip = (struct rt_arb_internal *)ip->idb_ptr;
	RT_ARB_CK_MAGIC(aip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 8 * 3;
	ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "arb external");
	for (i=0; i<8; i++) {
		VSCALE( &vec[i*3], aip->pt[i], local2mm );
	}
	htond( ep->ext_buf, (unsigned char *)vec, 8*3);
	return 0;
}
/**
 *			R T _ A R B _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_arb_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    register struct rt_arb_internal *aip = (struct rt_arb_internal *)ip->idb_ptr;
    char buf[256];
    int	i;
    int	arb_type;
    struct bn_tol tmp_tol;	/* temporay tolerance */
    
    RT_ARB_CK_MAGIC(aip);
    
    tmp_tol.magic = BN_TOL_MAGIC;
    tmp_tol.dist = 0.0001; /* to get old behavior of rt_arb_std_type() */
    tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
    tmp_tol.perp = 1e-5;
    tmp_tol.para = 1 - tmp_tol.perp;
    
    arb_type = rt_arb_std_type( ip, &tmp_tol );
    
    if( !arb_type ) {
	
	bu_vls_strcat( str, "ARB8\n");
	
	/* Use 1-based numbering, to match vertex labels in MGED */		
	sprintf( buf, "\t1 (%g, %g, %g)\n",
		 INTCLAMP(aip->pt[0][X] * mm2local),
		 INTCLAMP(aip->pt[0][Y] * mm2local),
		 INTCLAMP(aip->pt[0][Z] * mm2local) );
	bu_vls_strcat( str, buf );
	
	if( !verbose )  return(0);
	
	for( i=1; i < 8; i++ )  {
	    sprintf( buf, "\t%d (%g, %g, %g)\n", i+1,
		     INTCLAMP(aip->pt[i][X] * mm2local),
		     INTCLAMP(aip->pt[i][Y] * mm2local),
		     INTCLAMP(aip->pt[i][Z] * mm2local) );
	    bu_vls_strcat( str, buf );
	}
    } else {
	sprintf( buf, "ARB%d\n", arb_type );
	bu_vls_strcat( str, buf );
	switch( arb_type ) {
	    case ARB8:
		for( i=0 ; i<8 ; i++ ) {
		    sprintf( buf, "\t%d (%g, %g, %g)\n", i+1, 
			     INTCLAMP(aip->pt[i][X] * mm2local),
			     INTCLAMP(aip->pt[i][Y] * mm2local),
			     INTCLAMP(aip->pt[i][Z] * mm2local) );
		    bu_vls_strcat( str, buf );
		}
		break;
	    case ARB7:
		for( i=0 ; i<7 ; i++ ) {
		    sprintf( buf, "\t%d (%g, %g, %g)\n", i+1,
			     INTCLAMP(aip->pt[i][X] * mm2local),
			     INTCLAMP(aip->pt[i][Y] * mm2local),
			     INTCLAMP(aip->pt[i][Z] * mm2local) );
		    bu_vls_strcat( str, buf );
		}
		break;
	    case ARB6:
		for( i=0 ; i<5 ; i++ ) {
		    sprintf( buf, "\t%d (%g, %g, %g)\n", i+1,
			     INTCLAMP(aip->pt[i][X] * mm2local),
			     INTCLAMP(aip->pt[i][Y] * mm2local),
			     INTCLAMP(aip->pt[i][Z] * mm2local) );
		    bu_vls_strcat( str, buf );
		}
		sprintf( buf, "\t6 (%g, %g, %g)\n",
			 INTCLAMP(aip->pt[6][X] * mm2local),
			 INTCLAMP(aip->pt[6][Y] * mm2local),
			 INTCLAMP(aip->pt[6][Z] * mm2local) );
		bu_vls_strcat( str, buf );
		break;
	    case ARB5:
		for( i=0 ; i<5 ; i++ ) {
		    sprintf( buf, "\t%d (%g, %g, %g)\n", i+1,
			     INTCLAMP(aip->pt[i][X] * mm2local),
			     INTCLAMP(aip->pt[i][Y] * mm2local),
			     INTCLAMP(aip->pt[i][Z] * mm2local) );
		    bu_vls_strcat( str, buf );
		}
		break;
	    case ARB4:
		for( i=0 ; i<3 ; i++ ) {
		    sprintf( buf, "\t%d (%g, %g, %g)\n", i+1,
			     INTCLAMP(aip->pt[i][X] * mm2local),
			     INTCLAMP(aip->pt[i][Y] * mm2local),
			     INTCLAMP(aip->pt[i][Z] * mm2local) );
		    bu_vls_strcat( str, buf );
		}
		sprintf( buf, "\t4 (%g, %g, %g)\n",
			 INTCLAMP(aip->pt[4][X] * mm2local),
			 INTCLAMP(aip->pt[4][Y] * mm2local),
			 INTCLAMP(aip->pt[4][Z] * mm2local) );
		bu_vls_strcat( str, buf );
		break;
	}
    }
    return(0);
}

/**
 *			R T _ A R B _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_arb_ifree(struct rt_db_internal *ip)
{
	RT_CK_DB_INTERNAL(ip);
	bu_free( ip->idb_ptr, "arb ifree" );
	ip->idb_ptr = (genptr_t)NULL;
}

/**
 *			R T _ A R B _ T E S S
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
rt_arb_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	LOCAL struct rt_arb_internal	*aip;
	struct shell		*s;
	struct prep_arb		pa;
	register int		i;
	struct faceuse		*fu[6];
	struct vertex		*verts[8];
	struct vertex		**vertp[4];

	RT_CK_DB_INTERNAL(ip);
	aip = (struct rt_arb_internal *)ip->idb_ptr;
	RT_ARB_CK_MAGIC(aip);

	bzero( (char *)&pa, sizeof(pa) );
	pa.pa_doopt = 0;		/* no UV stuff */
	pa.pa_tol_sq = tol->dist_sq;
	if( rt_arb_mk_planes( &pa, aip, "(tess)" ) < 0 )  return(-2);

	for( i=0; i<8; i++ )  verts[i] = (struct vertex *)0;

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	/* Process each face */
	for( i=0; i < pa.pa_faces; i++ )  {
		if( pa.pa_clockwise[i] != 0 )  {
			/* Counter-Clockwise orientation (CCW) */
			vertp[0] = &verts[pa.pa_pindex[0][i]];
			vertp[1] = &verts[pa.pa_pindex[1][i]];
			vertp[2] = &verts[pa.pa_pindex[2][i]];
			if( pa.pa_npts[i] > 3 ) {
				vertp[3] = &verts[pa.pa_pindex[3][i]];
			}
		} else {
			register struct vertex	***vertpp = vertp;
			/* Clockwise orientation (CW) */
			if( pa.pa_npts[i] > 3 ) {
				*vertpp++ = &verts[pa.pa_pindex[3][i]];
			}
			*vertpp++ = &verts[pa.pa_pindex[2][i]];
			*vertpp++ = &verts[pa.pa_pindex[1][i]];
			*vertpp++ = &verts[pa.pa_pindex[0][i]];
		}
		if( RT_G_DEBUG & DEBUG_ARB8 )  {
			bu_log("face %d, npts=%d, verts %d %d %d %d\n",
				i, pa.pa_npts[i],
				pa.pa_pindex[0][i], pa.pa_pindex[1][i],
				pa.pa_pindex[2][i], pa.pa_pindex[3][i] );
		}
		if( (fu[i] = nmg_cmface( s, vertp, pa.pa_npts[i] )) == 0 )  {
			bu_log("rt_arb_tess(%s): nmg_cmface() fail on face %d\n", i);
			continue;
		}
	}

	/* Associate vertex geometry */
	for( i=0; i<8; i++ )
		if(verts[i]) nmg_vertex_gv(verts[i], aip->pt[i]);

	/* Associate face geometry */
	for( i=0; i < pa.pa_faces; i++ )  {
#if 1
		/* We already know the plane equations, this is fast */
		nmg_face_g( fu[i], pa.pa_face[i].peqn );
#else
		/* For the cautious, ensure topology and geometry match */
		if( nmg_fu_planeeqn( fu[i], tol ) < 0 )
			return -1;		/* FAIL */
#endif
	}

	/* Mark edges as real */
	(void)nmg_mark_edges_real( &s->l.magic );

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r, tol );

	/* Some arbs may not be within tolerance, so triangulate faces where needed */
	nmg_make_faces_within_tol( s, tol );

	return(0);
}

static const fastf_t rt_arb_uvw[5*3] = {
	0, 0, 0,
	1, 0, 0,
	1, 1, 0,
	0, 1, 0,
	0, 0, 0
};
static const int rt_arb_vert_index_scramble[4] = { 0, 1, 3, 2 };

/**
 *			R T _ A R B _ T N U R B
 *
 *  "Tessellate" an ARB into a trimmed-NURB-NMG data structure.
 *  Purely a mechanical transformation of one faceted object
 *  into another.
 *
 *  Depending on the application, it might be beneficial to keep ARBs
 *  as planar-NMG objects; there is no real benefit to using B-splines
 *  here, other than uniformity of the conversion for all solids.
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_arb_tnurb(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bn_tol *tol)
{
	LOCAL struct rt_arb_internal	*aip;
	struct shell		*s;
	struct prep_arb		pa;
	register int		i;
	struct faceuse		*fu[6];
	struct vertex		*verts[8];
	struct vertex		**vertp[4];
	struct edgeuse		*eu;
	struct loopuse		*lu;

	RT_CK_DB_INTERNAL(ip);
	aip = (struct rt_arb_internal *)ip->idb_ptr;
	RT_ARB_CK_MAGIC(aip);

	bzero( (char *)&pa, sizeof(pa) );
	pa.pa_doopt = 0;		/* no UV stuff */
	pa.pa_tol_sq = tol->dist_sq;
	if( rt_arb_mk_planes( &pa, aip, "(tnurb)" ) < 0 )  return(-2);

	for( i=0; i<8; i++ )  verts[i] = (struct vertex *)0;

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	/* Process each face */
	for( i=0; i < pa.pa_faces; i++ )  {
		if( pa.pa_clockwise[i] != 0 )  {
			/* Counter-Clockwise orientation (CCW) */
			vertp[0] = &verts[pa.pa_pindex[0][i]];
			vertp[1] = &verts[pa.pa_pindex[1][i]];
			vertp[2] = &verts[pa.pa_pindex[2][i]];
			if( pa.pa_npts[i] > 3 ) {
				vertp[3] = &verts[pa.pa_pindex[3][i]];
			}
		} else {
			register struct vertex	***vertpp = vertp;
			/* Clockwise orientation (CW) */
			if( pa.pa_npts[i] > 3 ) {
				*vertpp++ = &verts[pa.pa_pindex[3][i]];
			}
			*vertpp++ = &verts[pa.pa_pindex[2][i]];
			*vertpp++ = &verts[pa.pa_pindex[1][i]];
			*vertpp++ = &verts[pa.pa_pindex[0][i]];
		}
		if( RT_G_DEBUG & DEBUG_ARB8 )  {
			bu_log("face %d, npts=%d, verts %d %d %d %d\n",
				i, pa.pa_npts[i],
				pa.pa_pindex[0][i], pa.pa_pindex[1][i],
				pa.pa_pindex[2][i], pa.pa_pindex[3][i] );
		}
		/* The edges created will be linear, in parameter space...,
		 * but need to have edge_g_cnurb geometry. */
		if( (fu[i] = nmg_cmface( s, vertp, pa.pa_npts[i] )) == 0 )  {
			bu_log("rt_arb_tnurb(%s): nmg_cmface() fail on face %d\n", i);
			continue;
		}
		/* March around the fu's loop assigning uv parameter values */
		lu = BU_LIST_FIRST( loopuse, &fu[i]->lu_hd );
		NMG_CK_LOOPUSE(lu);
		eu = BU_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE(eu);

		/* Loop always has Counter-Clockwise orientation (CCW) */
		nmg_vertexuse_a_cnurb( eu->vu_p, &rt_arb_uvw[0*3] );
		nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, &rt_arb_uvw[1*3] );
		eu = BU_LIST_NEXT( edgeuse, &eu->l );

		nmg_vertexuse_a_cnurb( eu->vu_p, &rt_arb_uvw[1*3] );
		nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, &rt_arb_uvw[2*3] );
		eu = BU_LIST_NEXT( edgeuse, &eu->l );

		nmg_vertexuse_a_cnurb( eu->vu_p, &rt_arb_uvw[2*3] );
		if( pa.pa_npts[i] > 3 ) {
			nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, &rt_arb_uvw[3*3] );

			eu = BU_LIST_NEXT( edgeuse, &eu->l );
			nmg_vertexuse_a_cnurb( eu->vu_p, &rt_arb_uvw[3*3] );
		}
		/* Final eu must end back at the beginning */
		nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, &rt_arb_uvw[0*3] );
	}

	/* Associate vertex geometry */
	for( i=0; i<8; i++ )
		if(verts[i]) nmg_vertex_gv(verts[i], aip->pt[i]);

	/* Associate face geometry */
	for( i=0; i < pa.pa_faces; i++ )  {
		struct face_g_snurb	*fg;
		int	j;

		/* Let the library allocate all the storage */
		nmg_face_g_snurb( fu[i],
			2, 2,		/* u,v order */
			4, 4,		/* Number of knots, u,v */
			NULL, NULL,	/* initial u,v knot vectors */
			2, 2,		/* n_rows, n_cols */
			RT_NURB_MAKE_PT_TYPE( 3, RT_NURB_PT_XYZ, RT_NURB_PT_NONRAT ),
			NULL );		/* initial mesh */

		fg = fu[i]->f_p->g.snurb_p;
		NMG_CK_FACE_G_SNURB(fg);

		/* Assign surface knot vectors as 0, 0, 1, 1 */
		fg->u.knots[0] = fg->u.knots[1] = 0;
		fg->u.knots[2] = fg->u.knots[3] = 1;
		fg->v.knots[0] = fg->v.knots[1] = 0;
		fg->v.knots[2] = fg->v.knots[3] = 1;

		/* Assign surface control points from the corners */
		lu = BU_LIST_FIRST( loopuse, &fu[i]->lu_hd );
		NMG_CK_LOOPUSE(lu);
		eu = BU_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE(eu);

		/* For ctl_points, need 4 verts in order 0, 1, 3, 2 */
		for( j=0; j < pa.pa_npts[i]; j++ )  {
			VMOVE( &fg->ctl_points[rt_arb_vert_index_scramble[j]*3],
				eu->vu_p->v_p->vg_p->coord );

			/* Also associate edge geometry (trimming curve) */
			nmg_edge_g_cnurb_plinear(eu);
			eu = BU_LIST_NEXT( edgeuse, &eu->l );
		}
		if( pa.pa_npts[i] == 3 ) {
			vect_t	c_b;
			/*  Trimming curve describes a triangle ABC on face,
			 *  generate a phantom fourth corner at A + (C-B)
			 *  [3] = [0] + [2] - [1]
			 */
			VSUB2( c_b,
				&fg->ctl_points[rt_arb_vert_index_scramble[2]*3],
				&fg->ctl_points[rt_arb_vert_index_scramble[1]*3] );
			VADD2( &fg->ctl_points[rt_arb_vert_index_scramble[3]*3],
				&fg->ctl_points[rt_arb_vert_index_scramble[0]*3],
				c_b );
		}
	}


	/* Mark edges as real */
	(void)nmg_mark_edges_real( &s->l.magic );

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r, tol );
	return(0);
}

/* --- General ARB8 utility routines --- */

/**
 *			R T _ A R B _ C A L C _ P O I N T S
 *
 * Takes the planes[] array and intersects the planes to find the vertices
 * of a GENARB8.  The vertices are stored into arb->pt[].
 * This is an analog of rt_arb_calc_planes().
 */
int
rt_arb_calc_points(
	struct rt_arb_internal	*arb,		/* needs wdb.h */
	int			cgtype,
	const plane_t		planes[6],
	const struct bn_tol	*tol)
{
	int	i;
	point_t	pt[8];

	RT_ARB_CK_MAGIC(arb);

	/* find new points for entire solid */
	for(i=0; i<8; i++){
		if( rt_arb_3face_intersect( pt[i], planes, cgtype, i*3 ) < 0 )  {
		  bu_log("rt_arb_calc_points: Intersection of planes fails %d\n", i);
		  return -1;			/* FAIL */
		}
	}

	/* Move new points to arb */
	for( i=0; i<8; i++ )  {
		VMOVE( arb->pt[i], pt[i] );
	}
	return 0;					/* success */
}

/* planes to define ARB vertices */
const int rt_arb_planes[5][24] = {
	{0,1,3, 0,1,2, 0,2,3, 0,1,3, 1,2,3, 1,2,3, 1,2,3, 1,2,3},	/* ARB4 */
	{0,1,4, 0,1,2, 0,2,3, 0,3,4, 1,2,4, 1,2,4, 1,2,4, 1,2,4},	/* ARB5 */
	{0,2,3, 0,1,3, 0,1,4, 0,2,4, 1,2,3, 1,2,3, 1,2,4, 1,2,4},	/* ARB6 */
	{0,2,4, 0,3,4, 0,3,5, 0,2,5, 1,4,5, 1,3,4, 1,3,5, 1,2,4},	/* ARB7 */
	{0,2,4, 0,3,4, 0,3,5, 0,2,5, 1,2,4, 1,3,4, 1,3,5, 1,2,5},	/* ARB8 */
};

/**
 *			R T _ A R B _ 3 F A C E _ I N T E R S E C T
 *
 *	Finds the intersection point of three faces of an ARB.
 *
 *  Returns -
 *	  0	success, value is in 'point'
 *	 -1	failure
 */
int
rt_arb_3face_intersect(
	point_t			point,
	const plane_t		planes[6],
	int			type,		/* 4..8 */
	int			loc)
{
	int	j;
	int	i1, i2, i3;

	j = type - 4;

	i1 = rt_arb_planes[j][loc];
	i2 = rt_arb_planes[j][loc+1];
	i3 = rt_arb_planes[j][loc+2];

	return bn_mkpoint_3planes( point, planes[i1], planes[i2], planes[i3] );
}


/**
 *			R T _ A R B _ C A L C _ P L A N E S
 *
 *	Calculate the plane (face) equations for an arb
 *	output previously went to es_peqn[i].
 *
 *  Returns -
 *	-1	Failure
 *	 0	OK
 *
 *  Note -
 *	 This function migrated from mged/edsol.c.
 */
int
rt_arb_calc_planes(Tcl_Interp			*interp,
		   struct rt_arb_internal	*arb,
		   int				type,
		   plane_t			planes[6],
		   const struct bn_tol		*tol)
{
    register int i, p1, p2, p3;

    RT_ARB_CK_MAGIC(arb);
    BN_CK_TOL(tol);

    type -= 4;	/* ARB4 at location 0, ARB5 at 1, etc */

    for (i=0; i<6; i++) {
	if(rt_arb_faces[type][i*4] == -1)
	    break;	/* faces are done */

	p1 = rt_arb_faces[type][i*4];
	p2 = rt_arb_faces[type][i*4+1];
	p3 = rt_arb_faces[type][i*4+2];

	if (bn_mk_plane_3pts(planes[i],
			     arb->pt[p1],
			     arb->pt[p2],
			     arb->pt[p3],
			     tol) < 0) {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "%d %d%d%d%d (bad face)\n",
			  i+1, p1+1, p2+1, p3+1, rt_arb_faces[type][i*4+3]+1);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    return -1;
	}
    }

    return 0;
}


/**  MV_EDGE:
 *	Moves an arb edge (end1,end2) with bounding
 *	planes bp1 and bp2 through point "thru".
 *	The edge has (non-unit) slope "dir".
 *	Note that the fact that the normals here point in rather than
 *	out makes no difference for computing the correct intercepts.
 *	After the intercepts are found, they should be checked against
 *	the other faces to make sure that they are always "inside".
 */
int
rt_arb_move_edge(Tcl_Interp		*interp,
		 struct rt_arb_internal	*arb,
		 vect_t			thru,
		 int			bp1,
		 int			bp2,
		 int			end1,
		 int			end2,
		 const vect_t		dir,
		 plane_t		planes[6],
		 const struct bn_tol	*tol)
{
    fastf_t	t1, t2;

    if (bn_isect_line3_plane(&t1, thru, dir, planes[bp1], tol) < 0 ||
	bn_isect_line3_plane(&t2, thru, dir, planes[bp2], tol) < 0) {
	Tcl_AppendResult(interp, "edge (direction) parallel to face normal\n", (char *)NULL);
	return (1);
    }

    RT_ARB_CK_MAGIC(arb);

    VJOIN1(arb->pt[end1], thru, t1, dir);
    VJOIN1(arb->pt[end2], thru, t2, dir);

    return(0);
}

/**
 *  			E D I T A R B
 *
 *  An ARB edge is moved by finding the direction of
 *  the line containing the edge and the 2 "bounding"
 *  planes.  The new edge is found by intersecting the
 *  new line location with the bounding planes.  The
 *  two "new" planes thus defined are calculated and the
 *  affected points are calculated by intersecting planes.
 *  This keeps ALL faces planar.
 *
 *  Note -
 *	 This code came from mged/edarb.c (written mostly by Keith Applin)
 *	 and was modified to live here.
 *
 */

/*  The storage for the "specific" ARB types is :
 *
 *	ARB4	0 1 2 0 3 3 3 3
 *	ARB5	0 1 2 3 4 4 4 4
 *	ARB6	0 1 2 3 4 4 5 5
 *	ARB7	0 1 2 3 4 5 6 4
 *	ARB8	0 1 2 3 4 5 6 7
 */

/*
 *	ARB6	0 1 2 3 4 5 5 4
 */

/* Another summary of how the vertices of ARBs are stored:
 *
 * Vertices:	1	2	3	4	5	6	7	8
 * Location----------------------------------------------------------------
 *	ARB8	0	1	2	3	4	5	6	7
 *	ARB7	0	1	2	3	4,7	5	6
 *	ARB6	0	1	2	3	4,5	6,7
 * 	ARB5	0	1	2	3	4,5,6,7
 *	ARB4	0,3	1	2	4,5,6,7
 */

/* The following arb editing arrays generally contain the following:
 *
 *	location 	comments
 *------------------------------------------------------------------------
 *	0,1		edge end points
 * 	2,3		bounding planes 1 and 2
 *	4, 5,6,7	plane 1 to recalculate, using next 3 points
 *	8, 9,10,11	plane 2 to recalculate, using next 3 points
 *	12, 13,14,15	plane 3 to recalculate, using next 3 points
 *	16,17		points (vertices) to recalculate
 *
 *
 * Each line is repeated for each edge (or point) to move
*/

/* edit array for arb8's */
short earb8[12][18] = {
	{0,1, 2,3, 0,0,1,2, 4,0,1,4, -1,0,0,0, 3,5},	/* edge 12 */
	{1,2, 4,5, 0,0,1,2, 3,1,2,5, -1,0,0,0, 3,6},	/* edge 23 */
	{2,3, 3,2, 0,0,2,3, 5,2,3,6, -1,0,0,0, 1,7},	/* edge 34 */
	{0,3, 4,5, 0,0,1,3, 2,0,3,4, -1,0,0,0, 2,7},	/* edge 14 */
	{0,4, 0,1, 2,0,4,3, 4,0,1,4, -1,0,0,0, 7,5},	/* edge 15 */
	{1,5, 0,1, 4,0,1,5, 3,1,2,5, -1,0,0,0, 4,6},	/* edge 26 */
	{4,5, 2,3, 4,0,5,4, 1,4,5,6, -1,0,0,0, 1,7},	/* edge 56 */
	{5,6, 4,5, 3,1,5,6, 1,4,5,6, -1,0,0,0, 2,7},	/* edge 67 */
	{6,7, 3,2, 5,2,7,6, 1,4,6,7, -1,0,0,0, 3,4},	/* edge 78 */
	{4,7, 4,5, 2,0,7,4, 1,4,5,7, -1,0,0,0, 3,6},	/* edge 58 */
	{2,6, 0,1, 3,1,2,6, 5,2,3,6, -1,0,0,0, 5,7},	/* edge 37 */
	{3,7, 0,1, 2,0,3,7, 5,2,3,7, -1,0,0,0, 4,6},	/* edge 48 */
};

/* edit array for arb7's */
short earb7[12][18] = {
	{0,1, 2,3, 0,0,1,2, 4,0,1,4, -1,0,0,0, 3,5},	/* edge 12 */
	{1,2, 4,5, 0,0,1,2, 3,1,2,5, -1,0,0,0, 3,6},	/* edge 23 */
	{2,3, 3,2, 0,0,2,3, 5,2,3,6, -1,0,0,0, 1,4},	/* edge 34 */
	{0,3, 4,5, 0,0,1,3, 2,0,3,4, -1,0,0,0, 2,-1},	/* edge 41 */
	{0,4, 0,5, 4,0,5,4, 2,0,3,4, 1,4,5,6, 1,-1},	/* edge 15 */
	{1,5, 0,1, 4,0,1,5, 3,1,2,5, -1,0,0,0, 4,6},	/* edge 26 */
	{4,5, 5,3, 2,0,3,4, 4,0,5,4, 1,4,5,6, 1,-1},	/* edge 56 */
	{5,6, 4,5, 3,1,6,5, 1,4,5,6, -1,0,0,0, 2, -1},	/* edge 67 */
	{2,6, 0,1, 5,2,3,6, 3,1,2,6, -1,0,0,0, 4,5},	/* edge 37 */
	{4,6, 4,3, 2,0,3,4, 5,3,4,6, 1,4,5,6, 2,-1},	/* edge 57 */
	{3,4, 0,1, 4,0,1,4, 2,0,3,4, 5,2,3,4, 5,6},	/* edge 45 */
	{-1,-1, -1,-1, 5,2,3,4, 4,0,1,4, 8,2,1,-1, 6,5},	/* point 5 */
};

/* edit array for arb6's */
short earb6[10][18] = {
	{0,1, 2,1, 3,0,1,4, 0,0,1,2, -1,0,0,0, 3,-1},	/* edge 12 */
	{1,2, 3,4, 1,1,2,5, 0,0,1,2, -1,0,0,0, 3,4},	/* edge 23 */
	{2,3, 1,2, 4,2,3,5, 0,0,2,3, -1,0,0,0, 1,-1},	/* edge 34 */
	{0,3, 3,4, 2,0,3,5, 0,0,1,3, -1,0,0,0, 4,2},	/* edge 14 */
	{0,4, 0,1, 3,0,1,4, 2,0,3,4, -1,0,0,0, 6,-1},	/* edge 15 */
	{1,4, 0,2, 3,0,1,4, 1,1,2,4, -1,0,0,0, 6,-1},	/* edge 25 */
	{2,6, 0,2, 4,6,2,3, 1,1,2,6, -1,0,0,0, 4,-1},	/* edge 36 */
	{3,6, 0,1, 4,6,2,3, 2,0,3,6, -1,0,0,0, 4,-1},	/* edge 46 */
	{-1,-1, -1,-1, 2,0,3,4, 1,1,2,4, 3,0,1,4, 6,-1},/* point 5 */
	{-1,-1, -1,-1, 2,0,3,6, 1,1,2,6, 4,2,3,6, 4,-1},/* point 6 */
};

/* edit array for arb5's */
short earb5[9][18] = {
	{0,1, 4,2, 0,0,1,2, 1,0,1,4, -1,0,0,0, 3,-1},	/* edge 12 */
	{1,2, 1,3, 0,0,1,2, 2,1,2,4, -1,0,0,0, 3,-1},	/* edge 23 */
	{2,3, 2,4, 0,0,2,3, 3,2,3,4, -1,0,0,0, 1,-1},	/* edge 34 */
	{0,3, 1,3, 0,0,1,3, 4,0,3,4, -1,0,0,0, 2,-1},	/* edge 14 */
	{0,4, 0,2, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* edge 15 */
	{1,4, 0,3, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* edge 25 */
	{2,4, 0,4, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, 	/* edge 35 */
	{3,4, 0,1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* edge 45 */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 5 */
};

/* edit array for arb4's */
short earb4[5][18] = {
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 1 */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 2 */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 3 */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* dummy */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 4 */
};

#define RT_ARB_EDIT_EDGE 0
#define RT_ARB_EDIT_POINT 1
#define RT_ARB7_MOVE_POINT_5 11
#define RT_ARB6_MOVE_POINT_5 8
#define RT_ARB6_MOVE_POINT_6 9
#define RT_ARB5_MOVE_POINT_5 8
#define RT_ARB4_MOVE_POINT_4 3

int
rt_arb_edit(Tcl_Interp			*interp,
	    struct rt_arb_internal	*arb,
	    int				arb_type,
	    int				edit_type,
	    vect_t			pos_model,
	    plane_t			planes[6],
	    const struct bn_tol		*tol)
{
    int pt1, pt2, bp1, bp2, newp, p1, p2, p3;
    short *edptr;		/* pointer to arb edit array */
    short *final;		/* location of points to redo */
    int i;
    const int *iptr;
    int edit_class = RT_ARB_EDIT_EDGE;

    RT_ARB_CK_MAGIC(arb);

    /* set the pointer */
    switch (arb_type) {
    case ARB4:
	edptr = &earb4[edit_type][0];
	final = &earb4[edit_type][16];

	if (edit_type == RT_ARB4_MOVE_POINT_4)
	    edit_type = 4;

	edit_class = RT_ARB_EDIT_POINT;

	break;
    case ARB5:
	edptr = &earb5[edit_type][0];
	final = &earb5[edit_type][16];

	if (edit_type == RT_ARB5_MOVE_POINT_5) {
	    edit_class = RT_ARB_EDIT_POINT;
	    edit_type = 4;
	}

	if (edit_class == RT_ARB_EDIT_POINT) {
	    edptr = &earb5[8][0];
	    final = &earb5[8][16];
	}

	break;
    case ARB6:
	edptr = &earb6[edit_type][0];
	final = &earb6[edit_type][16];

	if (edit_type == RT_ARB6_MOVE_POINT_5) {
	    edit_class = RT_ARB_EDIT_POINT;
	    edit_type = 4;
	} else if (edit_type == RT_ARB6_MOVE_POINT_6) {
	    edit_class = RT_ARB_EDIT_POINT;
	    edit_type = 6;
	}

	if (edit_class == RT_ARB_EDIT_POINT) {
	    i = 9;
	    if(edit_type == 4)
		i = 8;
	    edptr = &earb6[i][0];
	    final = &earb6[i][16];
	}

	break;
    case ARB7:
	edptr = &earb7[edit_type][0];
	final = &earb7[edit_type][16];

	if (edit_type == RT_ARB7_MOVE_POINT_5) {
	    edit_class = RT_ARB_EDIT_POINT;
	    edit_type = 4;
	}

	if (edit_class == RT_ARB_EDIT_POINT) {
	    edptr = &earb7[11][0];
	    final = &earb7[11][16];
	}

	break;
    case ARB8:
	edptr = &earb8[edit_type][0];
	final = &earb8[edit_type][16];

	break;
    default:
	Tcl_AppendResult(interp, "rt_arb_edit: unknown ARB type\n", (char *)NULL);

	return(1);
    }

    /* do the arb editing */
    if (edit_class == RT_ARB_EDIT_POINT) {
	/* moving a point - not an edge */
	VMOVE(arb->pt[edit_type] , pos_model);
	edptr += 4;
    } else if (edit_class == RT_ARB_EDIT_EDGE) {
	vect_t	edge_dir;

	/* moving an edge */
	pt1 = *edptr++;
	pt2 = *edptr++;

	/* calculate edge direction */
	VSUB2(edge_dir, arb->pt[pt2], arb->pt[pt1]);

	if (MAGNITUDE(edge_dir) == 0.0)
	    goto err;

	/* bounding planes bp1,bp2 */
	bp1 = *edptr++;
	bp2 = *edptr++;

	/* move the edge */
	if (rt_arb_move_edge(interp, arb, pos_model, bp1, bp2, pt1, pt2,
			     edge_dir, planes, tol))
	    goto err;
    }

    /* editing is done - insure planar faces */
    /* redo plane eqns that changed */
    newp = *edptr++; 	/* plane to redo */

    if (newp == 9)	/* special flag --> redo all the planes */
	if (rt_arb_calc_planes(interp, arb, arb_type, planes, tol))
	    goto err;

    if (newp >= 0 && newp < 6) {
	for (i=0; i<3; i++) {
	    /* redo this plane (newp), use points p1,p2,p3 */
	    p1 = *edptr++;
	    p2 = *edptr++;
	    p3 = *edptr++;

	    if (bn_mk_plane_3pts(planes[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], tol))
		goto err;

	    /* next plane */
	    if ((newp = *edptr++) == -1 || newp == 8)
		break;
	}
    }

    if (newp == 8) {
	/* special...redo next planes using pts defined in faces */
	for (i=0; i<3; i++) {
	    if ((newp = *edptr++) == -1)
		break;

	    iptr = &rt_arb_faces[arb_type-4][4*newp];
	    p1 = *iptr++;
	    p2 = *iptr++;
	    p3 = *iptr++;

	    if (bn_mk_plane_3pts(planes[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], tol))
		goto err;
	}
    }

    /* the changed planes are all redone
     *	push necessary points back into the planes
     */
    edptr = final;	/* point to the correct location */
    for (i=0; i<2; i++) {
	if ((p1 = *edptr++) == -1)
	    break;

	/* intersect proper planes to define vertex p1 */

	if (rt_arb_3face_intersect(arb->pt[p1], (const plane_t *)planes, arb_type, p1*3))
	    goto err;
    }

    /* Special case for ARB7: move point 5 .... must
     *	recalculate plane 2 = 456
     */
    if (arb_type == ARB7 && edit_class == RT_ARB_EDIT_POINT) {
	if (bn_mk_plane_3pts( planes[2], arb->pt[4], arb->pt[5], arb->pt[6], tol))
	    goto err;
    }

    /* carry along any like points */
    switch (arb_type) {
    case ARB8:
	break;
    case ARB7:
	VMOVE(arb->pt[7], arb->pt[4]);
	break;
    case ARB6:
	VMOVE(arb->pt[5], arb->pt[4]);
	VMOVE(arb->pt[7], arb->pt[6]);
	break;
    case ARB5:
	for (i=5; i<8; i++)
	    VMOVE(arb->pt[i], arb->pt[4]);
	break;
    case ARB4:
	VMOVE(arb->pt[3] , arb->pt[0]);
	for(i=5; i<8; i++)
	    VMOVE(arb->pt[i], arb->pt[4])
		break;
    }

    return(0);		/* OK */

err:
    /* Error handling */
    {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "cannot move edge: %d%d\n", pt1+1,pt2+1);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    return(1);		/* BAD */
}
/*@}*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
