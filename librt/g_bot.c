/*
 *			G _ B O T . C
 *
 *  Purpose -
 *	Intersect a ray with a bag o' triangles
 *
 *  Authors -
 *  	John R. Anderson
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1999 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSbot[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include "tcl.h"
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"
#include "./plane.h"

struct bot_specific
{
	unsigned char bot_mode;
	unsigned char bot_orientation;
	unsigned char bot_errmode;
	fastf_t *bot_thickness;
	struct bu_bitv *bot_facemode;
	struct tri_specific *bot_facelist;	/* head of linked list */
	struct tri_specific **bot_facearray;	/* head of face array */
};

/* XXX Set this to 32 to enable pieces by default */
int rt_bot_minpieces = 0;

/*
 *  			R T _ B O T _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid BOT, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	BOT is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct bot_specific is created, and it's address is stored in
 *  	stp->st_specific for use by bot_shot().
 */
int
rt_bot_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_bot_internal		*bot_ip;
	register struct bot_specific	*bot;
	CONST struct bn_tol		*tol = &rtip->rti_tol;
	int				tri_index, i;
	LOCAL fastf_t			dx, dy, dz;
	LOCAL fastf_t			f;
	int				ntri = 0;

	RT_CK_DB_INTERNAL(ip);
	bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
	RT_BOT_CK_MAGIC(bot_ip);

	BU_GETSTRUCT( bot, bot_specific );
	stp->st_specific = (genptr_t)bot;
	bot->bot_mode = bot_ip->mode;
	bot->bot_orientation = bot_ip->orientation;
	bot->bot_errmode = bot_ip->error_mode;
	if( bot_ip->thickness )
	{
		bot->bot_thickness = (fastf_t *)bu_calloc( bot_ip->num_faces, sizeof( fastf_t ), "bot_thickness" );
		for( tri_index=0 ; tri_index <  bot_ip->num_faces ; tri_index++ )
			bot->bot_thickness[tri_index] = bot_ip->thickness[tri_index];
	}
	if( bot_ip->face_mode )
		bot->bot_facemode = bu_bitv_dup( bot_ip->face_mode );
	bot->bot_facelist = (struct tri_specific *)NULL;

	for( tri_index=0 ; tri_index < bot_ip->num_faces ; tri_index++ )
	{
		point_t p1, p2, p3;

		VMOVE( p1, &bot_ip->vertices[bot_ip->faces[tri_index*3]*3] );
		VMOVE( p2, &bot_ip->vertices[bot_ip->faces[tri_index*3 + 1]*3] );
		VMOVE( p3, &bot_ip->vertices[bot_ip->faces[tri_index*3 + 2]*3] );
		VMINMAX( stp->st_min, stp->st_max, p1 );
		VMINMAX( stp->st_min, stp->st_max, p2 );
		VMINMAX( stp->st_min, stp->st_max, p3 );
		if( rt_botface( stp, bot, p1, p2, p3, ntri, tol ) > 0 )
			ntri++;
	}

	if( stp->st_specific == (genptr_t)0 )  {
		bu_log("bot(%s):  no faces\n", stp->st_name);
		return(-1);             /* BAD */
	}

	/* zero thickness will get missed by the raytracer */
	for( i=0 ; i<3 ; i++ )
	{
		if( NEAR_ZERO( stp->st_min[i] - stp->st_max[i], 1.0 ) )
		{
			stp->st_min[i] -= 0.000001;
			stp->st_max[i] += 0.000001;
		}
	}

	VADD2SCALE( stp->st_center, stp->st_max, stp->st_min, 0.5 );

	dx = (stp->st_max[X] - stp->st_min[X])/2;
	f = dx;
	dy = (stp->st_max[Y] - stp->st_min[Y])/2;
	if( dy > f )  f = dy;
	dz = (stp->st_max[Z] - stp->st_min[Z])/2;
	if( dz > f )  f = dz;
	stp->st_aradius = f;
	stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);

	/*
	 *  Support for solid 'pieces'
	 *  For now, each triangle is considered a separate piece.
	 *  These array allocations can't be made until the number of
	 *  triangles are known.
	 *
	 *  If the number of triangles is too small,
	 *  don't bother making pieces, the overhead isn't worth it.
	 *
	 *  To disable BoT pieces, on the RT command line specify:
	 *	-c "set rt_bot_minpieces=0"
	 */
	if( rt_bot_minpieces <= 0 )  return 0;
	if( ntri < rt_bot_minpieces )  return 0;

	stp->st_npieces = ntri;

	bot->bot_facearray = (struct tri_specific **)
		bu_malloc( sizeof(struct tri_specific *) * ntri,
			"bot_facearray" );

	stp->st_piece_rpps = (struct bound_rpp *)
		bu_malloc( sizeof(struct bound_rpp) * ntri,
			"st_piece_rpps" );

	{
		struct bound_rpp	*minmax;
		CONST struct tri_specific **fap =
			(CONST struct tri_specific **)bot->bot_facearray;
		register CONST struct tri_specific *trip = bot->bot_facelist;
		int surfno = ntri-1;

		for( ; trip; trip = trip->tri_forw, surfno-- )  {
			point_t b,c;
			point_t d,e,f;
			vect_t offset;
			fastf_t los;

			fap[surfno] = trip;
			minmax = &stp->st_piece_rpps[surfno];

			/* It is critical that the surfno's used in
			 * rt error messages (from hit_surfno) match
			 * the tri_surfno values, so that mged users who
			 * run "db get name f#" to see that triangle
			 * get the triangle they're expecting!
			 */
			BU_ASSERT_LONG( trip->tri_surfno, ==, surfno );
			if( bot->bot_mode == RT_BOT_PLATE || bot->bot_mode == RT_BOT_PLATE_NOCOS )  {
				if( BU_BITTEST( bot->bot_facemode, surfno ) )  {
					/* Append full thickness on both sides */
					los = bot->bot_thickness[surfno];
				} else {
					/* Center thickness.  Append 1/2 thickness on both sides */
					los = bot->bot_thickness[surfno] * 0.51;
				}
			} else {
				/* Prevent the RPP from being 0 thickness */
				los = tol->dist;	/* typ 0.005mm */
			}

			minmax->min[X] = minmax->max[X] = trip->tri_A[X];
			minmax->min[Y] = minmax->max[Y] = trip->tri_A[Y];
			minmax->min[Z] = minmax->max[Z] = trip->tri_A[Z];
			VADD2( b, trip->tri_BA, trip->tri_A );
			VADD2( c, trip->tri_CA, trip->tri_A );
			VMINMAX( minmax->min, minmax->max, b );
			VMINMAX( minmax->min, minmax->max, c );

			/* Offset face in +los */
			VSCALE( offset, trip->tri_N, los );
			VADD2( d, trip->tri_A, offset );
			VADD2( e, b, offset );
			VADD2( f, c, offset );
			VMINMAX( minmax->min, minmax->max, d );
			VMINMAX( minmax->min, minmax->max, e );
			VMINMAX( minmax->min, minmax->max, f );

			/* Offset face in -los */
			VSCALE( offset, trip->tri_N, -los );
			VADD2( d, trip->tri_A, offset );
			VADD2( e, b, offset );
			VADD2( f, c, offset );
			VMINMAX( minmax->min, minmax->max, d );
			VMINMAX( minmax->min, minmax->max, e );
			VMINMAX( minmax->min, minmax->max, f );
		}
	}

	return 0;
}


/*
 *			R T _ B O T F A C E
 *
 *  This function is called with pointers to 3 points,
 *  and is used to prepare BOT faces.
 *  ap, bp, cp point to vect_t points.
 *
 * Return -
 *	0	if the 3 points didn't form a plane (eg, colinear, etc).
 *	#pts	(3) if a valid plane resulted.
 */
HIDDEN int
rt_botface( stp, bot, ap, bp, cp, face_no, tol )
struct soltab *stp;
struct bot_specific *bot;
fastf_t		*ap, *bp, *cp;
int		face_no;
CONST struct bn_tol	*tol;
{
	register struct tri_specific *trip;
	vect_t work;
	LOCAL fastf_t m1, m2, m3, m4;

	BU_GETSTRUCT( trip, tri_specific );
	VMOVE( trip->tri_A, ap );
	VSUB2( trip->tri_BA, bp, ap );
	VSUB2( trip->tri_CA, cp, ap );
	VCROSS( trip->tri_wn, trip->tri_BA, trip->tri_CA );
	trip->tri_surfno = face_no;

	/* Check to see if this plane is a line or pnt */
	m1 = MAGNITUDE( trip->tri_BA );
	m2 = MAGNITUDE( trip->tri_CA );
	VSUB2( work, bp, cp );
	m3 = MAGNITUDE( work );
	m4 = MAGNITUDE( trip->tri_wn );
	if( m1 < 0.00001 || m2 < 0.00001 ||
	    m3 < 0.00001 || m4 < 0.00001 )  {
		bu_free( (char *)trip, "getstruct tri_specific");
	    	{
			bu_log("bot(%s): degenerate facet #%d\n",
				stp->st_name, face_no);
	    		bu_log( "\t(%g %g %g) (%g %g %g) (%g %g %g)\n",
	    			V3ARGS( ap ), V3ARGS( bp ), V3ARGS( cp ) );
	    	}
		return(0);			/* BAD */
	}		

	/*  wn is a normal of not necessarily unit length.
	 *  N is an outward pointing unit normal.
	 *  We depend on the points being given in CCW order here.
	 */
	VMOVE( trip->tri_N, trip->tri_wn );
	VUNITIZE( trip->tri_N );
	if( bot->bot_mode == RT_BOT_CW )
		VREVERSE( trip->tri_N, trip->tri_N )

	/* Add this face onto the linked list for this solid */
	trip->tri_forw = bot->bot_facelist;
	bot->bot_facelist = trip;
	return(3);				/* OK */
}

/*
 *			R T _ B O T _ P R I N T
 */
void
rt_bot_print( stp )
register CONST struct soltab *stp;
{
}

/*
 *			R T _ B O T _ M A K E S E G S
 *
 *  Given an array of hits, make segments out of them.
 *  Exactly how this is to be done depends on the mode of the BoT.
 */
HIDDEN int
rt_bot_makesegs( hits, nhits, stp, rp, ap, seghead )
struct hit		*hits;
int			nhits;
struct soltab		*stp;
struct xray		*rp;
struct application	*ap;
struct seg		*seghead;
{
	struct bot_specific *bot = (struct bot_specific *)stp->st_specific;
	register struct seg *segp;
	register int i;

	RT_CK_SOLTAB(stp);

	if( bot->bot_mode == RT_BOT_PLATE || bot->bot_mode == RT_BOT_PLATE_NOCOS )
	{
		for( i=0; i < nhits; i++ )
		{
			register int surfno = hits[i].hit_surfno;
			register fastf_t los;

			if( bot->bot_mode == RT_BOT_PLATE_NOCOS )
			  los = bot->bot_thickness[surfno];
			else
			  {
			    los = bot->bot_thickness[surfno] / hits[i].hit_vpriv[X];
			    if( los < 0.0 )
			      los = -los;
			  }
			if( BU_BITTEST( bot->bot_facemode, hits[i].hit_surfno ) )
			{
				/* append thickness to hit point */
				RT_GET_SEG( segp, ap->a_resource);
				segp->seg_stp = stp;

				/* set in hit */
				segp->seg_in = hits[i];
				segp->seg_in.hit_vpriv[Z] = 1;

				/* set out hit */
				segp->seg_out.hit_surfno = surfno;
				segp->seg_out.hit_dist = segp->seg_in.hit_dist + los;
				segp->seg_out.hit_vpriv[X] = segp->seg_in.hit_vpriv[X]; /* ray dir dot normal */
				segp->seg_out.hit_vpriv[Y] = bot->bot_orientation;
				segp->seg_out.hit_vpriv[Z] = -1; /* a clue for rt_bot_norm that this is an exit */
				segp->seg_out.hit_private = segp->seg_in.hit_private;
				segp->seg_out.hit_rayp = &ap->a_ray;

				BU_LIST_INSERT( &(seghead->l), &(segp->l) );
			}
			else
			{
				/* center thickness about hit point */
				RT_GET_SEG( segp, ap->a_resource);
				segp->seg_stp = stp;

				/* set in hit */
				segp->seg_in.hit_surfno = surfno;
				segp->seg_in.hit_vpriv[X] = hits[i].hit_vpriv[X]; /* ray dir dot normal */
				segp->seg_in.hit_vpriv[Y] = bot->bot_orientation;
				segp->seg_in.hit_vpriv[Z] = 1;
				segp->seg_in.hit_private = hits[i].hit_private;
				segp->seg_in.hit_dist = hits[i].hit_dist - (los*0.5 );
				segp->seg_in.hit_rayp = &ap->a_ray;

				/* set out hit */
				segp->seg_out.hit_surfno = surfno;
				segp->seg_out.hit_dist = segp->seg_in.hit_dist + los;
				segp->seg_out.hit_vpriv[X] = segp->seg_in.hit_vpriv[X]; /* ray dir dot normal */
				segp->seg_out.hit_vpriv[Y] = bot->bot_orientation;
				segp->seg_out.hit_vpriv[Z] = -1;
				segp->seg_out.hit_private = hits[i].hit_private;
				segp->seg_out.hit_rayp = &ap->a_ray;

				BU_LIST_INSERT( &(seghead->l), &(segp->l) );
			}
		}
		/* Every hit turns into two, and makes a seg.  No leftovers */
		return( nhits*2 );
	}

	if( bot->bot_mode == RT_BOT_SURFACE )
	{
		for( i=0 ; i<nhits ; i++ )
		{
			RT_GET_SEG( segp, ap->a_resource );
			segp->seg_stp = stp;

			/* set in hit */
			segp->seg_in = hits[i];
			segp->seg_in.hit_vpriv[Z] = 1;

			/* set out hit */
			segp->seg_out = hits[i];
			segp->seg_out.hit_vpriv[Z] = -1;
			BU_LIST_INSERT( &(seghead->l), &(segp->l) );
		}
		/* Every hit turns into two, and makes a seg.  No leftovers */
		return( nhits*2 );
	}

	BU_ASSERT( bot->bot_mode == RT_BOT_SOLID );

	if( bot->bot_orientation == RT_BOT_UNORIENTED )
	{
		/*
		 *  RT_BOT_SOLID, RT_BOT_UNORIENTED.
		 */
		if( nhits == 1 )
		{
			/* make a zero length partition */
			RT_GET_SEG( segp, ap->a_resource );
			segp->seg_stp = stp;

			/* set in hit */
			segp->seg_in = hits[0];
			segp->seg_in.hit_vpriv[Z] = 1;

			/* set out hit */
			segp->seg_out = hits[0];
			segp->seg_out.hit_vpriv[Z] = -1;

			BU_LIST_INSERT( &(seghead->l), &(segp->l) );
			return( 1 );
		}

		/* Remove duplicate hits */
		{
			register int j;

			for( i=0 ; i<nhits-1 ; i++ )
			{
				fastf_t dist;

				dist = hits[i].hit_dist - hits[i+1].hit_dist;
				if( NEAR_ZERO( dist, ap->a_rt_i->rti_tol.dist ) )
				{
					for( j=i ; j<nhits-1 ; j++ )
						hits[j] = hits[j+1];
					nhits--;
					i--;
				}
			}
		}

		if( nhits == 1 )
			return( 0 );

		for( i=0 ; i<(nhits&~1) ; i += 2 )
		{
			RT_GET_SEG( segp, ap->a_resource );
			segp->seg_stp = stp;

			/* set in hit */
			segp->seg_in = hits[i];
			segp->seg_in.hit_vpriv[Z] = 1;

			/* set out hit */
			segp->seg_out = hits[i+1];
			segp->seg_out.hit_vpriv[Z] = -1;

			BU_LIST_INSERT( &(seghead->l), &(segp->l) );
		}
		if( nhits&1 )
		{
			bu_log( "rt_bot_makesegs(%s): WARNING: odd number of hits (%d), last hit ignored\n",
				stp->st_name, nhits );
			nhits--;
		}
		return( nhits );
	}

	/*
	 *  RT_BOT_SOLID, RT_BOT_ORIENTED.
	 *
	 *  From this point on, process very similar to a polysolid
	 */

	/* Remove duplicate hits.
	 *  We remove one of a pair of hits when they are
	 *	1) close together, and
	 *	2) both "entry" or both "exit" occurrences.
	 *   Two immediate "entry" or two immediate "exit" hits suggest
	 *   that we hit both of two joined faces, while we want to hit only
	 *   one.  An "entry" followed by an "exit" (or vice versa) suggests
	 *   that we grazed an edge, and thus we should leave both
	 *   in the hit list.
	 */
	{
		register int j;

		for( i=0 ; i<nhits-1 ; i++ )
		{
			FAST fastf_t dist;

			dist = hits[i].hit_dist - hits[i+1].hit_dist;
			if( NEAR_ZERO( dist, ap->a_rt_i->rti_tol.dist ) &&
				hits[i].hit_vpriv[X] * hits[i+1].hit_vpriv[X] > 0)
			{
				for( j=i ; j<nhits-1 ; j++ )
					hits[j] = hits[j+1];
				nhits--;
				i--;
			}
		}
	}

	if( (nhits&1) )  {
		register int i;
		static int nerrors = 0;		/* message counter */
		/*
		 * If this condition exists, it is almost certainly due to
		 * the dn==0 check above.  Thus, we will make the last
		 * surface rather thin.
		 * This at least makes the
		 * presence of this solid known.  There may be something
		 * better we can do.
		 */

		if( nerrors++ < 6 )  {
			bu_log("rt_bot_makesegs(%s): WARNING %d hits:\n", stp->st_name, nhits);
			bu_log( "\tray start = (%g %g %g) ray dir = (%g %g %g)\n",
				V3ARGS( rp->r_pt ), V3ARGS( rp->r_dir ) );
			for(i=0; i < nhits; i++ )
			{
				point_t tmp_pt;

				VJOIN1( tmp_pt, rp->r_pt, hits[i].hit_dist, rp->r_dir );
				if( hits[i].hit_vpriv[X] < 0.0 && bot->bot_orientation == RT_BOT_CCW )
					bu_log("\tentrance at dist=%f (%g %g %g)\n", hits[i].hit_dist, V3ARGS( tmp_pt ) );
				else
					bu_log("\texit at dist=%f (%g %g %g)\n", hits[i].hit_dist, V3ARGS( tmp_pt ) );
			}
		}

		if( nhits > 2 )
		{
			fastf_t dot1,dot2;
			int j;

			/* likely an extra hit,
			 * look for consecutive entrances or exits */

			dot2 = 1.0;
			i = 0;
			while( i<nhits )
			{
				dot1 = dot2;
				dot2 = hits[i].hit_vpriv[X];
				if( dot1 > 0.0 && dot2 > 0.0 )
				{
					/* two consectutive exits,
					 * manufacture an entrance at same distance
					 * as second exit.
					 */
					/* XXX This consumes an extra hit structure in the array */
					for( j=nhits ; j>i ; j-- )
						hits[j] = hits[j-1];	/* struct copy */

					hits[i].hit_vpriv[X] = -hits[i].hit_vpriv[X];
					dot2 = hits[i].hit_vpriv[X];
					nhits++;
					bu_log( "\t\tadding fictitious entry at %f (%s)\n", hits[i].hit_dist, stp->st_name );
				}
				else if( dot1 < 0.0 && dot2 < 0.0 )
				{
					/* two consectutive entrances,
					 * manufacture an exit between them.
					 */
					/* XXX This consumes an extra hit structure in the array */

					for( j=nhits ; j>i ; j-- )
						hits[j] = hits[j-1];	/* struct copy */

					hits[i] = hits[i-1];	/* struct copy */
					hits[i].hit_vpriv[X] = -hits[i].hit_vpriv[X];
					dot2 = hits[i].hit_vpriv[X];
					nhits++;
					bu_log( "\t\tadding fictitious exit at %f (%s)\n", hits[i].hit_dist, stp->st_name );
				}
				i++;
			}
		}
	}
	if( (nhits&1) )  {
#if 1
		/* XXX This consumes an extra hit structure in the array */
		hits[nhits] = hits[nhits-1];	/* struct copy */
		hits[nhits].hit_vpriv[X] = -hits[nhits].hit_vpriv[X];
		bu_log( "\t\tadding fictitious hit at %f (%s)\n", hits[nhits].hit_dist, stp->st_name );
		nhits++;
#else
		nhits--;
#endif
	}

	/* nhits is even, build segments */
	for( i=0; i < nhits; i += 2 )  {
		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in = hits[i];		/* struct copy */
		segp->seg_in.hit_vpriv[Z] = 1;
		segp->seg_out = hits[i+1];	/* struct copy */
		segp->seg_out.hit_vpriv[Z] = -1;
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	}

	return(nhits);			/* HIT */
}

/*
 *  			R T _ B O T _ S H O T
 *  
 *  Intersect a ray with a bot.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *
 *	Notes for rt_bot_norm():
 *		hit_private contains pointer to the tri_specific structure
 *		hit_vpriv[X] contains dot product of ray direction and unit normal from tri_specific
 *		hit_vpriv[Y] contains the BOT solid orientation
 *		hit_vpriv[Z] contains the +1 if the hit is a entrance, or -1 for an exit
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
#define MAXHITS 128
int
rt_bot_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	struct bot_specific *bot = (struct bot_specific *)stp->st_specific;
	register struct tri_specific *trip = bot->bot_facelist;
	LOCAL struct hit hits[MAXHITS];
	register struct hit *hp;
	LOCAL int	nhits;

	nhits = 0;
	hp = &hits[0];

	/* consider each face */
	for( ; trip; trip = trip->tri_forw )  {
		FAST fastf_t	dn;		/* Direction dot Normal */
		LOCAL fastf_t	abs_dn;
		FAST fastf_t	k;
		LOCAL fastf_t	alpha, beta;
		LOCAL vect_t	wxb;		/* vertex - ray_start */
		LOCAL vect_t	xp;		/* wxb cross ray_dir */

		/*
		 *  Ray Direction dot N.  (N is outward-pointing normal)
		 *  wn points inwards, and is not unit length.
		 */
		dn = VDOT( trip->tri_wn, rp->r_dir );

		/*
		 *  If ray lies directly along the face, (ie, dot product
		 *  is zero), drop this face.
		 */
		abs_dn = dn >= 0.0 ? dn : (-dn);
		if( abs_dn < SQRT_SMALL_FASTF )
			continue;
		VSUB2( wxb, trip->tri_A, rp->r_pt );
		VCROSS( xp, wxb, rp->r_dir );

		/* Check for exceeding along the one side */
		alpha = VDOT( trip->tri_CA, xp );
		if( dn < 0.0 )  alpha = -alpha;
		if( alpha < 0.0 || alpha > abs_dn )
			continue;

		/* Check for exceeding along the other side */
		beta = VDOT( trip->tri_BA, xp );
		if( dn > 0.0 )  beta = -beta;
		if( beta < 0.0 || beta > abs_dn )
			continue;
		if( alpha+beta > abs_dn )
			continue;
		k = VDOT( wxb, trip->tri_wn ) / dn;

		/* HIT is within planar face */
		hp->hit_magic = RT_HIT_MAGIC;
		hp->hit_dist = k;
		hp->hit_private = (genptr_t)trip;
		hp->hit_vpriv[X] = VDOT( trip->tri_N, rp->r_dir );
		hp->hit_vpriv[Y] = bot->bot_orientation;
		hp->hit_surfno = trip->tri_surfno;
		hp->hit_rayp = &ap->a_ray;
		if( ++nhits >= MAXHITS )  {
			bu_log("rt_bot_shot(%s): too many hits (%d)\n", stp->st_name, nhits);
			break;
		}
		hp++;
	}
	if( nhits == 0 )
		return(0);		/* MISS */

	/* Sort hits, Near to Far */
	rt_hitsort( hits, nhits );

	/* build segments */
	return rt_bot_makesegs( hits, nhits, stp, rp, ap, seghead );
}

/*
 *			R T _ B O T _ P I E C E _ S H O T
 *
 *  Intersect a ray with a list of "pieces" of a BoT.
 *
 *  This routine may be invoked many times for a single ray,
 *  as the ray traverses from one space partitioning cell to the next.
 *
 *  Plate-mode (2 hit) segments will be returned immediately in seghead.
 *
 *  Generally the hits are stashed between invocations in psp.
 */
int
rt_bot_piece_shot( psp, plp, dist_corr, rp, ap, seghead )
struct rt_piecestate	*psp;
struct rt_piecelist	*plp;
double			dist_corr;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	struct resource		*resp;
	long		*sol_piece_subscr_p;
	struct soltab	*stp;
	long		piecenum;
	register struct hit *hp;
	struct bot_specific *bot;
	CONST int	debug_shoot = rt_g.debug & DEBUG_SHOOT;
	int		starting_hits;

	RT_CK_PIECELIST(plp);
	RT_CK_PIECESTATE(psp);
	stp = plp->stp;
	RT_CK_SOLTAB(stp);
	resp = ap->a_resource;
	RT_CK_RESOURCE(resp);
	bot = (struct bot_specific *)stp->st_specific;
	starting_hits = psp->htab.end;

	sol_piece_subscr_p = &(plp->pieces[plp->npieces-1]);
	for( ; sol_piece_subscr_p >= plp->pieces; sol_piece_subscr_p-- )  {
		FAST fastf_t	dn;		/* Direction dot Normal */
		LOCAL fastf_t	abs_dn;
		FAST fastf_t	k;
		LOCAL fastf_t	alpha, beta;
		LOCAL vect_t	wxb;		/* vertex - ray_start */
		LOCAL vect_t	xp;		/* wxb cross ray_dir */
		register struct tri_specific *trip;

		piecenum = *sol_piece_subscr_p;

		if( BU_BITTEST( psp->shot, piecenum ) )  {
			if(debug_shoot) bu_log("%s piece %d already shot\n", stp->st_name, piecenum);
			resp->re_piece_ndup++;
			continue;	/* this piece already shot */
		}

		/* Shoot a ray */
		BU_BITSET( psp->shot, piecenum );
			if(debug_shoot) bu_log("%s piece %d ...\n", stp->st_name, piecenum);

		/* Now intersect with each piece */
		trip = bot->bot_facearray[piecenum];
		BU_ASSERT_LONG(trip->tri_surfno, ==, piecenum);

		/*
		 *  Ray Direction dot N.  (N is outward-pointing normal)
		 *  wn points inwards, and is not unit length.
		 */
		dn = VDOT( trip->tri_wn, rp->r_dir );

		/*
		 *  If ray lies directly along the face, (ie, dot product
		 *  is zero), drop this face.
		 */
		abs_dn = dn >= 0.0 ? dn : (-dn);
		if( abs_dn < SQRT_SMALL_FASTF )
			continue;
		VSUB2( wxb, trip->tri_A, rp->r_pt );
		VCROSS( xp, wxb, rp->r_dir );

		/* Check for exceeding along the one side */
		alpha = VDOT( trip->tri_CA, xp );
		if( dn < 0.0 )  alpha = -alpha;
		if( alpha < 0.0 || alpha > abs_dn )
			continue;

		/* Check for exceeding along the other side */
		beta = VDOT( trip->tri_BA, xp );
		if( dn > 0.0 )  beta = -beta;
		if( beta < 0.0 || beta > abs_dn )
			continue;
		if( alpha+beta > abs_dn )
			continue;
		k = VDOT( wxb, trip->tri_wn ) / dn;

		/* HIT is within planar face */
		hp = rt_htbl_get( &psp->htab );
		hp->hit_magic = RT_HIT_MAGIC;
		hp->hit_dist = k + dist_corr;
		hp->hit_private = (genptr_t)trip;
		hp->hit_vpriv[X] = VDOT( trip->tri_N, rp->r_dir );
		hp->hit_vpriv[Y] = bot->bot_orientation;
		hp->hit_surfno = trip->tri_surfno;
		hp->hit_rayp = &ap->a_ray;
		if(debug_shoot) bu_log("%s piece %d ... HIT %g\n", stp->st_name, piecenum, hp->hit_dist);
	}
	if( psp->htab.end > 0 &&
	    (bot->bot_mode == RT_BOT_PLATE || bot->bot_mode == RT_BOT_PLATE_NOCOS) )
	{
		/*
		 * Each of these hits is really two, resulting in an instant seg.
		 * Saving an odd number of these will confuse a_onehit processing.
		 */
		rt_hitsort( psp->htab.hits, psp->htab.end );
		return rt_bot_makesegs( psp->htab.hits, psp->htab.end, stp, rp, ap, seghead );
	}
	return psp->htab.end - starting_hits;
}

/*
 *			R T _ B O T _ P I E C E _ H I T S E G S
 */
void
rt_bot_piece_hitsegs( psp, seghead, ap )
struct rt_piecestate	*psp;
struct seg		*seghead;
struct application	*ap;
{
	int ret;

	RT_CK_PIECESTATE(psp);
	RT_CK_AP(ap);
	RT_CK_HTBL(&psp->htab);

	/* Sort hits, Near to Far */
	rt_hitsort( psp->htab.hits, psp->htab.end );

	/* build segments */
	(void)rt_bot_makesegs( psp->htab.hits, psp->htab.end, psp->stp, &ap->a_ray, ap, seghead );
}

#define RT_BOT_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ B O T _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_bot_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ B O T _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_bot_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	struct tri_specific *trip=(struct tri_specific *)hitp->hit_private;
	fastf_t dn=hitp->hit_vpriv[X]; /* ray dir dot normal */

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );

	if( hitp->hit_vpriv[Y] == RT_BOT_UNORIENTED )
	{
		if( hitp->hit_vpriv[Z] < 0 ) /* this is an out hit */
		{
			if( dn < 0.0 )
				VREVERSE( hitp->hit_normal, trip->tri_N )
			else
				VMOVE( hitp->hit_normal, trip->tri_N )
		}
		else
		{
			if( dn > 0.0 )
				VREVERSE( hitp->hit_normal, trip->tri_N )
			else
				VMOVE( hitp->hit_normal, trip->tri_N )
		}
	}
	else
		VMOVE( hitp->hit_normal, trip->tri_N )
}

/*
 *			R T _ B O T _ C U R V E
 *
 *  Return the curvature of the bot.
 */
void
rt_bot_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ B O T _ U V
 *  
 *  For a hit on the surface of an bot, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_bot_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
}

/*
 *		R T _ B O T _ F R E E
 */
void
rt_bot_free( stp )
register struct soltab *stp;
{
	register struct bot_specific *bot =
		(struct bot_specific *)stp->st_specific;
	register struct tri_specific *tri, *ptr;

	if( bot->bot_facearray )
		bu_free( (char *)bot->bot_facearray, "bot_facearray" );

	if( bot->bot_thickness )
		bu_free( (char *)bot->bot_thickness, "bot_thickness" );
	if( bot->bot_facemode )
		bu_free( (char *)bot->bot_facemode, "bot_facemode" );
	ptr = bot->bot_facelist;
	while( ptr )
	{
		tri = ptr->tri_forw;
		if( tri )
			bu_free( (char *)tri, "bot tri_specific" );
		ptr = tri;
	}
	bu_free( (char *)bot, "bot_specific" );
}

/*
 *			R T _ B O T _ C L A S S
 */
int
rt_bot_class( stp, min, max, tol )
CONST struct soltab    *stp;
CONST vect_t		min, max;
CONST struct bn_tol    *tol;
{
	return RT_CLASSIFY_UNIMPLEMENTED;
}

/*
 *			R T _ B O T _ P L O T
 */
int
rt_bot_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_bot_internal	*bot_ip;
	int i;

	RT_CK_DB_INTERNAL(ip);
	bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
	RT_BOT_CK_MAGIC(bot_ip);

	for( i=0 ; i<bot_ip->num_faces ; i++ )
	{
		RT_ADD_VLIST( vhead, &bot_ip->vertices[bot_ip->faces[i*3]*3], BN_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, &bot_ip->vertices[bot_ip->faces[i*3+1]*3], BN_VLIST_LINE_DRAW );
		RT_ADD_VLIST( vhead, &bot_ip->vertices[bot_ip->faces[i*3+2]*3], BN_VLIST_LINE_DRAW );
		RT_ADD_VLIST( vhead, &bot_ip->vertices[bot_ip->faces[i*3]*3], BN_VLIST_LINE_DRAW );
	}

	return(0);
}

/*
 *			R T _ B O T _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_bot_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_bot_internal	*bot_ip;
	struct shell *s;
	struct vertex **verts;
	point_t pt[3];
	int i;

	RT_CK_DB_INTERNAL(ip);
	bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
	RT_BOT_CK_MAGIC(bot_ip);
#if 0
	if( bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS )	/* tesselation not supported */
		return( -1 );
#endif
        *r = nmg_mrsv( m );     /* Make region, empty shell, vertex */
        s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	verts = (struct vertex **)bu_calloc( bot_ip->num_vertices, sizeof( struct vertex *),
		"rt_bot_tess: *verts[]" );

	for( i=0 ; i<bot_ip->num_faces ; i++ )
	{
		struct faceuse *fu;
		struct vertex **corners[3];

		if( bot_ip->orientation == RT_BOT_CW )
		{
			VMOVE( pt[2], &bot_ip->vertices[bot_ip->faces[i*3]*3] );
			VMOVE( pt[1], &bot_ip->vertices[bot_ip->faces[i*3+1]*3] );
			VMOVE( pt[0], &bot_ip->vertices[bot_ip->faces[i*3+2]*3] );
			corners[2] = &verts[bot_ip->faces[i*3]];
			corners[1] = &verts[bot_ip->faces[i*3+1]];
			corners[0] = &verts[bot_ip->faces[i*3+2]];
		}
		else
		{
			VMOVE( pt[0], &bot_ip->vertices[bot_ip->faces[i*3]*3] );
			VMOVE( pt[1], &bot_ip->vertices[bot_ip->faces[i*3+1]*3] );
			VMOVE( pt[2], &bot_ip->vertices[bot_ip->faces[i*3+2]*3] );
			corners[0] = &verts[bot_ip->faces[i*3]];
			corners[1] = &verts[bot_ip->faces[i*3+1]];
			corners[2] = &verts[bot_ip->faces[i*3+2]];
		}

		if( !bn_3pts_distinct( pt[0], pt[1], pt[2], tol )
                           || bn_3pts_collinear( pt[0], pt[1], pt[2], tol ) )
				continue;

		if( (fu=nmg_cmface( s, corners, 3 )) == (struct faceuse *)NULL )
		{
			bu_log( "rt_bot_tess() nmg_cmface() failed for face #%d\n", i );
			continue;
		}

		if( !(*corners[0])->vg_p )
			nmg_vertex_gv( *(corners[0]), pt[0] );
		if( !(*corners[1])->vg_p )
			nmg_vertex_gv( *(corners[1]), pt[1] );
		if( !(*corners[2])->vg_p )
			nmg_vertex_gv( *(corners[2]), pt[2] );

		if( nmg_calc_face_g( fu ) )
			nmg_kfu( fu );
		else if( bot_ip->mode == RT_BOT_SURFACE )
		{
			struct vertex **tmp;

			tmp = corners[0];
			corners[0] = corners[2];
			corners[2] = tmp;
			if( (fu=nmg_cmface( s, corners, 3 )) == (struct faceuse *)NULL )
				bu_log( "rt_bot_tess() nmg_cmface() failed for face #%d\n", i );
			else
				 nmg_calc_face_g( fu );
		}
	}

	bu_free( (char *)verts, "rt_bot_tess *verts[]" );

	nmg_mark_edges_real( &s->l.magic );

	nmg_region_a( *r, tol );

	if( bot_ip->mode == RT_BOT_SOLID && bot_ip->orientation == RT_BOT_UNORIENTED )
		nmg_fix_normals( s, tol );

	return( 0 );
}

/*
 *			R T _ B O T _ I M P O R T
 *
 *  Import an BOT from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_bot_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	LOCAL struct rt_bot_internal	*bot_ip;
	union record			*rp;
	int				i;
	int				chars_used;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_BOT )  {
		bu_log("rt_bot_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_BOT;
	ip->idb_meth = &rt_functab[ID_BOT];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_bot_internal), "rt_bot_internal");
	bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
	bot_ip->magic = RT_BOT_INTERNAL_MAGIC;

	bot_ip->num_vertices = bu_glong( rp->bot.bot_num_verts );
	bot_ip->num_faces = bu_glong( rp->bot.bot_num_triangles );
	bot_ip->orientation = rp->bot.bot_orientation;
	bot_ip->mode = rp->bot.bot_mode;
	bot_ip->error_mode = rp->bot.bot_err_mode;

	bot_ip->vertices = (fastf_t *)bu_calloc( bot_ip->num_vertices * 3, sizeof( fastf_t ), "Bot vertices" );
	bot_ip->faces = (int *)bu_calloc( bot_ip->num_faces * 3, sizeof( int ), "Bot faces" );

	for( i=0 ; i<bot_ip->num_vertices ; i++ )
	{
		point_t tmp;

		ntohd( (unsigned char *)tmp, (CONST unsigned char *)(&rp->bot.bot_data[i*24]), 3 );
		MAT4X3PNT( &(bot_ip->vertices[i*3]), mat, tmp );
	}

	chars_used = bot_ip->num_vertices * 3 * 8;

	for( i=0 ; i<bot_ip->num_faces ; i++ )
	{
		int index=chars_used + i * 12;

		bot_ip->faces[i*3] = bu_glong( (CONST unsigned char *)&rp->bot.bot_data[index] );
		bot_ip->faces[i*3 + 1] = bu_glong( (CONST unsigned char *)&rp->bot.bot_data[index + 4] );
		bot_ip->faces[i*3 + 2] = bu_glong( (CONST unsigned char *)&rp->bot.bot_data[index + 8] );
	}

	if( bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS )
	{
		chars_used = bot_ip->num_vertices * 3 * 8 + bot_ip->num_faces * 12;

		bot_ip->thickness = (fastf_t *)bu_calloc( bot_ip->num_faces, sizeof( fastf_t ), "BOT thickness" );
		for( i=0 ; i<bot_ip->num_faces ; i++ )
			ntohd( (unsigned char *)&(bot_ip->thickness[i]),
				(CONST unsigned char *)(&rp->bot.bot_data[chars_used + i*8]), 1 );
		bot_ip->face_mode = bu_hex_to_bitv( (CONST char *)(&rp->bot.bot_data[chars_used + bot_ip->num_faces * 8]) );
	}
	else
	{
		bot_ip->thickness = (fastf_t *)NULL;
		bot_ip->face_mode = (struct bu_bitv *)NULL;
	}

	return(0);			/* OK */
}

/*
 *			R T _ B O T _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_bot_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_bot_internal	*bot_ip;
	union record		*rec;
	int			i;
	int			chars_used;
	int			num_recs;
	struct bu_vls		face_mode;


	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_BOT )  return(-1);
	bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
	RT_BOT_CK_MAGIC(bot_ip);

	BU_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof( struct bot_rec ) - 1 +
		bot_ip->num_vertices * 3 * 8 + bot_ip->num_faces * 3 * 4;
	if( bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS )
	{
	  if( !bot_ip->face_mode )
	    {
	      bot_ip->face_mode = bu_bitv_new( bot_ip->num_faces );
	      bu_bitv_clear( bot_ip->face_mode );
	    }
	  if( !bot_ip->thickness )
	      bot_ip->thickness = (fastf_t *)bu_calloc( bot_ip->num_faces, sizeof( fastf_t ), "BOT thickness" );
	  bu_vls_init( &face_mode );
	  bu_bitv_to_hex( &face_mode, bot_ip->face_mode );
	  ep->ext_nbytes += bot_ip->num_faces * 8 + bu_vls_strlen( &face_mode ) + 1;
	}

	/* round up to the nearest granule */
	if( ep->ext_nbytes % (sizeof( union record ) ) )
	{
		ep->ext_nbytes += (sizeof( union record ) )
			- ep->ext_nbytes % (sizeof( union record ) );
	}
	num_recs = ep->ext_nbytes / sizeof( union record ) - 1;
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "bot external");
	rec = (union record *)ep->ext_buf;

	rec->bot.bot_id = DBID_BOT;

	bu_plong( (unsigned char *)rec->bot.bot_nrec, num_recs );
	rec->bot.bot_orientation = bot_ip->orientation;
	rec->bot.bot_mode = bot_ip->mode;
	rec->bot.bot_err_mode = bot_ip->error_mode;
	bu_plong( (unsigned char *)rec->bot.bot_num_verts, bot_ip->num_vertices );
	bu_plong( (unsigned char *)rec->bot.bot_num_triangles, bot_ip->num_faces );

	/* Since libwdb users may want to operate in units other
	 * than mm, we offer the opportunity to scale the solid
	 * (to get it into mm) on the way out.
	 */


	/* convert from local editing units to mm and export
	 * to database record format
	 */
	for( i=0 ; i<bot_ip->num_vertices ; i++ )
	{
		point_t tmp;

		VSCALE( tmp, &bot_ip->vertices[i*3], local2mm );
		htond( (unsigned char *)&rec->bot.bot_data[i*24], (CONST unsigned char *)tmp, 3 );
	}

	chars_used = bot_ip->num_vertices * 24;

	for( i=0 ; i<bot_ip->num_faces ; i++ )
	{
		int index=chars_used + i * 12;

		bu_plong( (unsigned char *)(&rec->bot.bot_data[index]), bot_ip->faces[i*3] );
		bu_plong( (unsigned char *)(&rec->bot.bot_data[index + 4]), bot_ip->faces[i*3+1] );
		bu_plong( (unsigned char *)(&rec->bot.bot_data[index + 8]), bot_ip->faces[i*3+2] );
	}

	chars_used += bot_ip->num_faces * 12;

	if( bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS )
	{
		for( i=0 ; i<bot_ip->num_faces ; i++ )
		{
			fastf_t tmp;
			tmp = bot_ip->thickness[i] * local2mm;
			htond( (unsigned char *)&rec->bot.bot_data[chars_used], (CONST unsigned char *)&tmp, 1 );
			chars_used += 8;
		}
		strcpy( (char *)&rec->bot.bot_data[chars_used], bu_vls_addr( &face_mode ) );
		bu_vls_free( &face_mode );
	}

	return(0);
}

/*
 *			R T _ B O T _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
static char *unoriented="unoriented";
static char *ccw="counter-clockwise";
static char *cw="clockwise";
static char *unknown_orientation="unknown orientation";
static char *surface="\tThis is a surface with no volume\n";
static char *solid="\tThis is a solid object (not just a surface)\n";
static char *plate="\tThis is a FASTGEN plate mode solid\n";
static char *nocos="\tThis is a plate mode solid with no obliquity angle effect\n";
static char *unknown_mode="\tunknown mode\n";
int
rt_bot_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
CONST struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_bot_internal	*bot_ip =
		(struct rt_bot_internal *)ip->idb_ptr;
	char	buf[256];
	char *orientation,*mode;
	int i;

	RT_BOT_CK_MAGIC(bot_ip);
	bu_vls_strcat( str, "Bag of triangles (BOT)\n");

	switch( bot_ip->orientation )
	{
		case RT_BOT_UNORIENTED:
			orientation = unoriented;
			break;
		case RT_BOT_CCW:
			orientation = ccw;
			break;
		case RT_BOT_CW:
			orientation = cw;
			break;
		default:
			orientation = unknown_orientation;
			break;
	}
	switch( bot_ip->mode )
	{
		case RT_BOT_SURFACE:
			mode = surface;
			break;
		case RT_BOT_SOLID:
			mode = solid;
			break;
		case RT_BOT_PLATE:
			mode = plate;
			break;
	        case RT_BOT_PLATE_NOCOS:
		        mode = nocos;
		        break;
		default:
			mode = unknown_mode;
			break;
	}
	sprintf(buf, "\t%d vertices, %d faces (%s)\n",
		bot_ip->num_vertices,
		bot_ip->num_faces,
		orientation );
	bu_vls_strcat( str, buf );
	bu_vls_strcat( str, mode );

	if( verbose )
	{
		for( i=0 ; i<bot_ip->num_faces ; i++ )
		{
			int j;
			point_t pt[3];

			for( j=0 ; j<3 ; j++ )
				VSCALE( pt[j], &bot_ip->vertices[bot_ip->faces[i*3+j]*3], mm2local )
			sprintf( buf, "\tface %d: (%g %g %g), (%g %g %g), (%g %g %g)\n", i,
				V3ARGS( pt[0] ),
				V3ARGS( pt[1] ),
				V3ARGS( pt[2] ) );
			bu_vls_strcat( str, buf );
			if( bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS )
			{
				char *face_mode;

				if( BU_BITTEST( bot_ip->face_mode, i ) )
					face_mode = "appended to hit point";
				else
					face_mode = "centered about hit point";
				sprintf( buf, "\t\tthickness = %g, %s\n", mm2local*bot_ip->thickness[i], face_mode );
				bu_vls_strcat( str, buf );
			}
		}
	}

	return(0);
}

/*
 *			R T _ B O T _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_bot_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_bot_internal	*bot_ip;

	RT_CK_DB_INTERNAL(ip);
	bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
	RT_BOT_CK_MAGIC(bot_ip);
	bot_ip->magic = 0;			/* sanity */

	bu_free( (char *)bot_ip->vertices, "BOT vertices" );
	bu_free( (char *)bot_ip->faces, "BOT faces" );

	if( bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS )
	{
		bu_free( (char *)bot_ip->thickness, "BOT thickness" );
		bu_free( (char *)bot_ip->face_mode, "BOT face_mode" );
	}

	bu_free( (char *)bot_ip, "bot ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

int
rt_bot_tnurb( r, m, ip, tol )
struct nmgregion        **r;
struct model            *m;
struct rt_db_internal   *ip;
CONST struct bn_tol           *tol;
{
	return( 1 );
}

int
rt_bot_xform( op, mat, ip, free, dbip )
struct rt_db_internal *op, *ip;
CONST mat_t	mat;
CONST int free;
struct db_i	*dbip;
{
	struct rt_bot_internal *botip, *botop;
	register int		i;
	point_t			pt;

	RT_CK_DB_INTERNAL( ip );
	botip = (struct rt_bot_internal *)ip->idb_ptr;
	RT_BOT_CK_MAGIC(botip);

	if( op != ip && !free )
	{
		RT_INIT_DB_INTERNAL( op );
		botop = (struct rt_bot_internal *)bu_malloc( sizeof( struct rt_bot_internal ), "botop" );
		botop->magic = RT_BOT_INTERNAL_MAGIC;
		botop->mode = botip->mode;
		botop->orientation = botip->orientation;
		botop->error_mode = botip->error_mode;
		botop->num_vertices = botip->num_vertices;
		botop->num_faces = botip->num_faces;
		botop->vertices = (fastf_t *)bu_malloc( botip->num_vertices * 3 *
			sizeof( fastf_t ), "botop->vertices" );
		botop->faces = (int *)bu_malloc( botip->num_faces * 3 *
			sizeof( int ), "botop->faces" );
		if( botip->thickness )
			botop->thickness = (fastf_t *)bu_malloc( botip->num_faces *
				sizeof( fastf_t ), "botop->thickness" );
		if( botip->face_mode )
			botop->face_mode = bu_bitv_dup( botip->face_mode );
		for( i=0 ; i<botip->num_vertices ; i++ )
			VMOVE( &botop->vertices[i*3], &botip->vertices[i*3] )
		for( i=0 ; i<botip->num_faces ; i++ )
		{
			botop->faces[i*3] = botip->faces[i*3];
			botop->faces[i*3+1] = botip->faces[i*3+1];
			botop->faces[i*3+2] = botip->faces[i*3+2];
		}
		if( botip->thickness )
		{
			for( i=0 ; i<botip->num_faces ; i++ )
				botop->thickness[i] = botip->thickness[i];
		}
		op->idb_ptr = (genptr_t)botop;
		op->idb_type = ID_BOT;
		op->idb_meth = &rt_functab[ID_BOT];
	}
	else
		botop = botip;

	for( i=0 ; i<botip->num_vertices ; i++ )
	{
		MAT4X3PNT( pt, mat, &botip->vertices[i*3] );
		VMOVE( &botop->vertices[i*3], pt );
	}

	if( free && op != ip )
		rt_bot_ifree( ip );

	return( 0 );
}

int
bot_find_v_nearest_pt2( bot, pt2, mat )
CONST struct rt_bot_internal *bot;
CONST point_t	pt2;
CONST mat_t	mat;
{
	point_t v;
	int index;
	fastf_t dist=MAX_FASTF;
	int closest=-1;

	RT_BOT_CK_MAGIC( bot );

	for( index=0 ; index < bot->num_vertices ; index++ )
	{
		fastf_t tmp_dist;
		fastf_t tmpx, tmpy;

		MAT4X3PNT( v, mat, &bot->vertices[index*3] )
		tmpx = v[X] - pt2[X];
		tmpy = v[Y] - pt2[Y];
		tmp_dist = tmpx * tmpx + tmpy * tmpy;
		if( tmp_dist < dist )
		{
			dist = tmp_dist;
			closest = index;
		}
	}

	return( closest );
}

int
bot_edge_in_list( v1, v2, edge_list, edge_count )
CONST int v1, v2, edge_count, edge_list[];
{
	int i, ev1, ev2;

	for( i=0 ; i<edge_count ; i++ )
	{
		ev1 = edge_list[i*2];
		ev2 = edge_list[i*2 + 1];

		if( ev1 == v1 && ev2 == v2 )
			return( 1 );

		if( ev1 == v2 && ev2 == v1 )
			return( 1 );
	}

	return( 0 );
}

/* This routine finds the edge closest to the 2D point "pt2", and returns the edge as two
 * vertex indices (vert1 and vert2). These vertices are ordered (closest to pt2 is first)
 */
int
bot_find_e_nearest_pt2( vert1, vert2, bot, pt2, mat)
int *vert1, *vert2;
CONST struct rt_bot_internal *bot;
CONST point_t	pt2;
CONST mat_t	mat;
{
	int i;
	int v1, v2, v3;
	fastf_t dist=MAX_FASTF, tmp_dist;
	int *edge_list;
	int edge_count=0;
	struct bn_tol tol;

	RT_BOT_CK_MAGIC( bot );

	if( bot->num_faces < 1 )
		return( -1 );

	/* first build a list of edges */
	edge_list = (int *)bu_calloc( bot->num_faces * 3 * 2, sizeof( int ), "bot edge list" );

	for( i=0 ; i<bot->num_faces ; i++ )
	{
		v1 = bot->faces[i*3];
		v2 = bot->faces[i*3 + 1];
		v3 = bot->faces[i*3 + 2];

		if( !bot_edge_in_list( v1, v2, edge_list, edge_count ) )
		{
			edge_list[edge_count*2] = v1;
			edge_list[edge_count*2 + 1] = v2;
			edge_count++;
		}
		if( !bot_edge_in_list( v3, v2, edge_list, edge_count ) )
		{
			edge_list[edge_count*2] = v3;
			edge_list[edge_count*2 + 1] = v2;
			edge_count++;
		}
		if( !bot_edge_in_list( v1, v3, edge_list, edge_count ) )
		{
			edge_list[edge_count*2] = v1;
			edge_list[edge_count*2 + 1] = v3;
			edge_count++;
		}
	}

	/* build a tyolerance structure for the bn_dist routine */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.0;
	tol.dist_sq = 0.0;
	tol.perp = 0.0;
	tol.para =  1.0;

	/* now look for the closest edge */
	for( i=0 ; i<edge_count ; i++ )
	{
		point_t p1, p2, pca;
		vect_t p1_to_pca, p1_to_p2;
		int ret;

		MAT4X3PNT( p1, mat, &bot->vertices[ edge_list[i*2]*3] )
		MAT4X3PNT( p2, mat, &bot->vertices[ edge_list[i*2+1]*3] )

		ret = bn_dist_pt2_lseg2( &tmp_dist, pca, p1, p2, pt2, &tol );

		if( ret < 3 || tmp_dist < dist )
		{
			switch( ret )
			{
				case 0:
					dist = 0.0;
					if( tmp_dist < 0.5 )
					{
						*vert1 = edge_list[i*2];
						*vert2 = edge_list[i*2+1];
					}
					else
					{
						*vert1 = edge_list[i*2+1];
						*vert2 = edge_list[i*2];
					}
					break;
				case 1:
					dist = 0.0;
					*vert1 = edge_list[i*2];
					*vert2 = edge_list[i*2+1];
					break;
				case 2:
					dist = 0.0;
					*vert1 = edge_list[i*2+1];
					*vert2 = edge_list[i*2];
					break;
				case 3:
					dist = tmp_dist;
					*vert1 = edge_list[i*2];
					*vert2 = edge_list[i*2+1];
					break;
				case 4:
					dist = tmp_dist;
					*vert1 = edge_list[i*2+1];
					*vert2 = edge_list[i*2];
				case 5:
					dist = tmp_dist;
					V2SUB2( p1_to_pca, pca, p1 );
					V2SUB2( p1_to_p2, p2, p1 );
					if( MAG2SQ( p1_to_pca ) / MAG2SQ( p1_to_p2 ) < 0.25 )
					{
						*vert1 = edge_list[i*2];
						*vert2 = edge_list[i*2+1];
					}
					else
					{
						*vert1 = edge_list[i*2+1];
						*vert2 = edge_list[i*2];
					}
					break;
			}
		}
	}

	bu_free( (char *)edge_list, "bot edge list" );

	return( 0 );
}

static char *modes[]={
	"ERROR: Unrecognized mode",
	"surf",
	"volume",
	"plate",
	"plate_nocos"
};

static char *orientation[]={
	"ERROR: Unrecognized orientation",
	"no",
	"rh",
	"lh"
};

static char *los[]={
	"center",
	"append"
};

/*
 *			R T _ B O T _ T C L G E T
 *
 *  Examples -
 *	db get name fm		get los facemode bit vector
 *	db get name fm#		get los face mode of face # (center, append)
 *	db get name V		get coords for all vertices
 *	db get name V#		get coords for vertex #
 *	db get name F		get vertex indices for all faces
 *	db get name F#		get vertex indices for face #
 *	db get name f		get list of coords for all faces
 *	db get name f#		get list of 3 3tuple coords for face #
 *	db get name T		get thickness for all faces
 *	db get name T#		get thickness for face #
 *	db get name nv		get num_vertices
 *	db get name nt		get num_faces
 *	db get name mode	get mode (surf, volume, plate, plane_nocos)
 *	db get name orient	get orientation (no, rh, lh)
 */
int
rt_bot_tclget( interp, intern, attr )
Tcl_Interp			*interp;
CONST struct rt_db_internal	*intern;
CONST char			*attr;
{
	register struct rt_bot_internal *bot=(struct rt_bot_internal *)intern->idb_ptr;
	Tcl_DString	ds;
	struct bu_vls	vls;
	int		status;
	int		i;

	RT_BOT_CK_MAGIC( bot );

	Tcl_DStringInit( &ds );
	bu_vls_init( &vls );

	if( attr == (char *)NULL )
	{
		bu_vls_strcpy( &vls, "bot" );
		bu_vls_printf( &vls, " mode %s orient %s V {",
				modes[bot->mode], orientation[bot->orientation] );
		for( i=0 ; i<bot->num_vertices ; i++ )
			bu_vls_printf( &vls, " { %.25G %.25G %.25G }",
				V3ARGS( &bot->vertices[i*3] ) );
		bu_vls_strcat( &vls, " } F {" );
		for( i=0 ; i<bot->num_faces ; i++ )
			bu_vls_printf( &vls, " { %d %d %d }",
				V3ARGS( &bot->faces[i*3] ) );
		bu_vls_strcat( &vls, " }" );
		if( bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS )
		{
			bu_vls_strcat( &vls, " T {" );
			for( i=0 ; i<bot->num_faces ; i++ )
				bu_vls_printf( &vls, " %.25G", bot->thickness[i] );
			bu_vls_strcat( &vls, " } fm " );
			bu_bitv_to_hex( &vls, bot->face_mode );
		}
		status = TCL_OK;
	}
	else
	{
		if( !strncmp( attr, "fm", 2 ) )
		{
			if( bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS )
			{
				bu_vls_strcat( &vls, "Only plate mode BOTs have face_modes" );
				status = TCL_ERROR;
			}
			else
			{
				if( attr[2] == '\0' )
				{
					bu_bitv_to_hex( &vls, bot->face_mode );
					status = TCL_OK;
				}
				else
				{
					i = atoi( &attr[2] );
					if( i < 0 || i >=bot->num_faces )
					{
						bu_vls_printf( &vls, "face number %d out of range (0..%d)", i, bot->num_faces-1 );
						status = TCL_ERROR;
					}
					else
					{
						bu_vls_printf( &vls, "%s",
							los[BU_BITTEST( bot->face_mode, i )] );
						status = TCL_OK;
					}
				}
			}
		}
		else if( attr[0] == 'V' )
		{
			if( attr[1] != '\0' )
			{
				i = atoi( &attr[1] );
				if( i < 0 || i >=bot->num_vertices )
				{
					bu_vls_printf( &vls, "vertex number %d out of range (0..%d)", i, bot->num_vertices-1 );
					status = TCL_ERROR;
				}
				else
				{
					bu_vls_printf( &vls, "%.25G %.25G %.25G",
						V3ARGS( &bot->vertices[i*3] ) );
					status = TCL_OK;
				}
			}
			else
			{
				for( i=0 ; i<bot->num_vertices ; i++ )
					bu_vls_printf( &vls, " { %.25G %.25G %.25G }",
						V3ARGS( &bot->vertices[i*3] ) );
				status = TCL_OK;
			}
		}
		else if( attr[0] == 'F' )
		{
			/* Retrieve one face, as vertex indices */
			if( attr[1] == '\0' )
			{
				for( i=0 ; i<bot->num_faces ; i++ )
					bu_vls_printf( &vls, " { %d %d %d }",
						V3ARGS( &bot->faces[i*3] ) );
				status = TCL_OK;
			}
			else
			{
				i = atoi( &attr[1] );
				if( i < 0 || i >=bot->num_faces )
				{
					bu_vls_printf( &vls, "face number %d out of range (0..%d)", i, bot->num_faces-1 );
					status = TCL_ERROR;
				}
				else
				{
					bu_vls_printf( &vls, "%d %d %d",
						V3ARGS( &bot->faces[i*3] ) );
					status = TCL_OK;
				}
			}
		}
		else if( attr[0] == 'f' )
		{
			int indx;
			/* Retrieve one face, as list of 3 3tuple coordinates */
			if( attr[1] == '\0' )
			{
				for( i=0 ; i<bot->num_faces ; i++ )  {
					indx = bot->faces[i*3];
					bu_vls_printf( &vls, " { %.25G %.25G %.25G }",
						bot->vertices[indx*3],
						bot->vertices[indx*3+1],
						bot->vertices[indx*3+2] );
					indx = bot->faces[i*3+1];
					bu_vls_printf( &vls, " { %.25G %.25G %.25G }",
						bot->vertices[indx*3],
						bot->vertices[indx*3+1],
						bot->vertices[indx*3+2] );
					indx = bot->faces[i*3+2];
					bu_vls_printf( &vls, " { %.25G %.25G %.25G }",
						bot->vertices[indx*3],
						bot->vertices[indx*3+1],
						bot->vertices[indx*3+2] );
				}
				status = TCL_OK;
			}
			else
			{
				i = atoi( &attr[1] );
				if( i < 0 || i >=bot->num_faces )
				{
					bu_vls_printf( &vls, "face number %d out of range (0..%d)", i, bot->num_faces-1 );
					status = TCL_ERROR;
				}
				else
				{
					indx = bot->faces[i*3];
					bu_vls_printf( &vls, " { %.25G %.25G %.25G }",
						bot->vertices[indx*3],
						bot->vertices[indx*3+1],
						bot->vertices[indx*3+2] );
					indx = bot->faces[i*3+1];
					bu_vls_printf( &vls, " { %.25G %.25G %.25G }",
						bot->vertices[indx*3],
						bot->vertices[indx*3+1],
						bot->vertices[indx*3+2] );
					indx = bot->faces[i*3+2];
					bu_vls_printf( &vls, " { %.25G %.25G %.25G }",
						bot->vertices[indx*3],
						bot->vertices[indx*3+1],
						bot->vertices[indx*3+2] );
					status = TCL_OK;
				}
			}
		}
		else if( attr[0] == 'T' )
		{
			if( bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS )
			{
				bu_vls_strcat( &vls, "Only plate mode BOTs have thicknesses" );
				status = TCL_ERROR;
			}
			else
			{
				if( attr[1] == '\0' )
				{
					for( i=0 ; i<bot->num_faces ; i++ )
						bu_vls_printf( &vls, " %.25G", bot->thickness[i] );
					status = TCL_OK;
				}
				else
				{
					i = atoi( &attr[1] );
					if( i < 0 || i >=bot->num_faces )
					{
						bu_vls_printf( &vls, "face number %d out of range (0..%d)", i, bot->num_faces-1 );
						status = TCL_ERROR;
					}
					else
					{
						bu_vls_printf( &vls, " %.25G", bot->thickness[i] );
						status = TCL_OK;
					}
				}
			}
		}
		else if( !strcmp( attr, "nv" ) )
		{
			bu_vls_printf( &vls, "%d", bot->num_vertices );
			status = TCL_OK;
		}
		else if( !strcmp( attr, "nt" ) )
		{
			bu_vls_printf( &vls, "%d", bot->num_faces );
			status = TCL_OK;
		}
		else if( !strcmp( attr, "mode" ) )
		{
			bu_vls_printf( &vls, "%s", modes[bot->mode] );
			status = TCL_OK;
		}
		else if( !strcmp( attr, "orient" ) )
		{
			bu_vls_printf( &vls, "%s", orientation[bot->orientation] );
			status = TCL_OK;
		}
		else
		{
			bu_vls_printf( &vls, "BoT has no attribute '%s'", attr );
			status = TCL_ERROR;
		}
	}

	Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
	Tcl_DStringResult( interp, &ds );
	Tcl_DStringFree( &ds );
	bu_vls_free( &vls );

	return( status );
}

char *
next_tok( str )
char *str;
{
  char *ret;

  ret = str;
  while( !isspace( *ret ) && *ret !='\0' )
    ret++;
  while( isspace( *ret ) )
    ret++;

  return( ret );
}

int
rt_bot_tcladjust( interp, intern, argc, argv )
Tcl_Interp		*interp;
struct rt_db_internal	*intern;
int			argc;
char			**argv;
{
	struct rt_bot_internal *bot;
	Tcl_Obj *obj, *list, **obj_array;
	int len;
	int i;

	RT_CK_DB_INTERNAL( intern );
	bot = (struct rt_bot_internal *)intern->idb_ptr;
	RT_BOT_CK_MAGIC( bot );

	while( argc >= 2 )
	{
		obj = Tcl_NewStringObj( argv[1], -1 );
		list = Tcl_NewListObj( 0, NULL );
		Tcl_ListObjAppendList( interp, list, obj );

		if( !strncmp( argv[0], "fm", 2 ) )
		{
			if( argv[0][2] == '\0' )
			{
				if( bot->face_mode )
					bu_free( (char *)bot->face_mode, "bot->face_mode" );
				bot->face_mode = bu_hex_to_bitv( argv[1] );
			}
			else
			{
				i = atoi( &(argv[0][2]) );
				if( i < 0 || i >= bot->num_faces )
				{
					Tcl_SetResult( interp, "Face number out of range", TCL_STATIC );
					Tcl_DecrRefCount( list );
					return( TCL_ERROR );
				}

				if( isdigit( *argv[1] ) )
				  {
				    if( atoi( argv[1] ) == 0 )
					BU_BITCLR( bot->face_mode, i );
				    else
					BU_BITSET( bot->face_mode, i );
				  }
				else if( !strcmp( argv[1], "append" ) )
					BU_BITSET( bot->face_mode, i );
				else
				        BU_BITCLR( bot->face_mode, i );
			}
		}
		else if( argv[0][0] == 'V' )
		{
		  char *v_str;

		  if( argv[0][1] == '\0' )
		    {
		      (void)Tcl_ListObjGetElements( interp, list, &len, &obj_array );
		      if( len <= 0 )
			{
			  Tcl_SetResult( interp, "Must provide at least one vertex!!!", TCL_STATIC );
			  Tcl_DecrRefCount( list );
			  return( TCL_ERROR );
			}
		      bot->num_vertices = len;
		      bu_free( (char *)bot->vertices, "BOT vertices" );
		      bot->vertices = (fastf_t *)bu_calloc( len*3, sizeof( fastf_t ), "BOT vertices" );
		      for( i=0 ; i<len ; i++ )
			{
			  v_str = Tcl_GetStringFromObj( obj_array[i], NULL );
			  while( isspace( *v_str ) ) v_str++;
			  if( *v_str == '\0' )
			    {
			      Tcl_SetResult( interp, "incomplete list of vertices", TCL_STATIC );
			      Tcl_DecrRefCount( list );
			      return( TCL_ERROR );
			    }
			  bot->vertices[i*3] = atof( v_str );
			  v_str = next_tok( v_str );
			  if( *v_str == '\0' )
			    {
			      Tcl_SetResult( interp, "incomplete list of vertices", TCL_STATIC );
			      Tcl_DecrRefCount( list );
			      return( TCL_ERROR );
			    }
			  bot->vertices[i*3+1] = atof( v_str );
			  v_str = next_tok( v_str );
			  if( *v_str == '\0' )
			    {
			      Tcl_SetResult( interp, "incomplete list of vertices", TCL_STATIC );
			      Tcl_DecrRefCount( list );
			      return( TCL_ERROR );
			    }
			  bot->vertices[i*3+2] = atof( v_str );
			  Tcl_DecrRefCount( obj_array[i] );
			}
		    }
		  else
		    {
		      i = atoi( &argv[0][1] );
		      if( i < 0 || i >= bot->num_vertices )
			{
			  Tcl_SetResult( interp, "vertex number out of range!!!", TCL_STATIC );
			  Tcl_DecrRefCount( list );
			  return( TCL_ERROR );
			}
		      v_str = Tcl_GetStringFromObj( list, NULL );
		      while( isspace( *v_str ) ) v_str++;
		      bot->vertices[i*3] = atof( v_str );
		      v_str = next_tok( v_str );
		      if( *v_str == '\0' )
			{
			  Tcl_SetResult( interp, "incomplete vertex", TCL_STATIC );
			  Tcl_DecrRefCount( list );
			  return( TCL_ERROR );
			}
		      bot->vertices[i*3+1] = atof( v_str );
		      v_str = next_tok( v_str );
		      if( *v_str == '\0' )
			{
			  Tcl_SetResult( interp, "incomplete vertex", TCL_STATIC );
			  Tcl_DecrRefCount( list );
			  return( TCL_ERROR );
			}
		      bot->vertices[i*3+2] = atof( v_str );
		    }
		}
		else if( argv[0][0] == 'F' )
		  {
		    char *f_str;

		    if( argv[0][1] == '\0' )
		      {
			(void)Tcl_ListObjGetElements( interp, list, &len, &obj_array );
			if( len <= 0 )
			  {
			    Tcl_SetResult( interp, "Must provide at least one face!!!", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
			bot->num_faces = len;
			bu_free( (char *)bot->faces, "BOT faces" );
			bot->faces = (int *)bu_calloc( len*3, sizeof( int ), "BOT faces" );
			for( i=0 ; i<len ; i++ )
			  {
			    f_str = Tcl_GetStringFromObj( obj_array[i], NULL );
			    while( isspace( *f_str ) ) f_str++;
			    if( *f_str == '\0' )
			      {
				Tcl_SetResult( interp, "incomplete list of faces", TCL_STATIC );
				Tcl_DecrRefCount( list );
				return( TCL_ERROR );
			      }
			    bot->faces[i*3] = atoi( f_str );
			    f_str = next_tok( f_str );
			    if( *f_str == '\0' )
			      {
				Tcl_SetResult( interp, "incomplete list of faces", TCL_STATIC );
				Tcl_DecrRefCount( list );
				return( TCL_ERROR );
			      }
			    bot->faces[i*3+1] = atoi( f_str );
			    f_str = next_tok( f_str );
			    if( *f_str == '\0' )
			      {
				Tcl_SetResult( interp, "incomplete list of faces", TCL_STATIC );
				Tcl_DecrRefCount( list );
				return( TCL_ERROR );
			      }
			    bot->faces[i*3+2] = atoi( f_str );
			  }
		      }
		    else
		      {
			i = atoi( &argv[0][1] );
			if( i < 0 || i >= bot->num_faces )
			  {
			    Tcl_SetResult( interp, "face number out of range!!!", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
			f_str = Tcl_GetStringFromObj( list, NULL );
		      	while( isspace( *f_str ) ) f_str++;
			bot->faces[i*3] = atoi( f_str );
			f_str = next_tok( f_str );
			if( *f_str == '\0' )
			  {
			    Tcl_SetResult( interp, "incomplete vertex", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
			bot->faces[i*3+1] = atoi( f_str );
			f_str = next_tok( f_str );
			if( *f_str == '\0' )
			  {
			    Tcl_SetResult( interp, "incomplete vertex", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
			bot->faces[i*3+2] = atoi( f_str );
		      }
		  }
		else if( argv[0][0] ==  'T' )
		  {
		    char *t_str;

		    if( argv[0][1] == '\0' )
		      {
			(void)Tcl_ListObjGetElements( interp, list, &len, &obj_array );
			if( len <= 0 )
			  {
			    Tcl_SetResult( interp, "Must provide at least one thickness!!!", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
			if( len > bot->num_faces )
			  {
			    Tcl_SetResult( interp, "Too many thicknesses (there are not that many faces)!!!", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
			for( i=0 ; i<len ; i++ )
			  {
			    bot->thickness[i] = atof( Tcl_GetStringFromObj( obj_array[i], NULL ) );
			    Tcl_DecrRefCount( obj_array[i] );
			  }
		      }
		    else
		      {
			i = atoi( &argv[0][1] );
			if( i < 0 || i >= bot->num_faces )
			  {
			    Tcl_SetResult( interp, "face number out of range!!!", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
			t_str = Tcl_GetStringFromObj( list, NULL );
			bot->thickness[i] = atof( t_str );
		      }
		  }
		else if( !strcmp( argv[0], "mode" ) )
		  {
		    char *m_str;

		    m_str = Tcl_GetStringFromObj( list, NULL );
		    if( isdigit( *m_str ) )
		      {
			int mode;

			mode = atoi( m_str );
			if( mode < RT_BOT_SURFACE || mode > RT_BOT_PLATE_NOCOS )
			  {
			    Tcl_SetResult( interp, "unrecognized mode!!!", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
			bot->mode = mode;
		      }
		    else
		      {
			if( !strncmp( m_str, modes[RT_BOT_SURFACE], 4 ) )
			  bot->mode = RT_BOT_SURFACE;
			else if( !strcmp( m_str, modes[RT_BOT_SOLID] ) )
			  bot->mode = RT_BOT_SOLID;
			else if( !strcmp( m_str, modes[RT_BOT_PLATE] ) )
			  bot->mode = RT_BOT_PLATE;
			else if( !strcmp( m_str, modes[RT_BOT_PLATE_NOCOS] ) )
			  bot->mode = RT_BOT_PLATE_NOCOS;
			else
			  {
			    Tcl_SetResult( interp, "unrecognized mode!!!", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
		      }
		  }
		else if( !strncmp( argv[0], "orient", 6 ) )
		  {
		    char *o_str;

		    o_str = Tcl_GetStringFromObj( list, NULL );
		    if( isdigit( *o_str ) )
		      {
			int orientation;

			orientation = atoi( o_str );
			if( orientation < RT_BOT_UNORIENTED || orientation > RT_BOT_CW )
			  {
			    Tcl_SetResult( interp, "unrecognized orientation!!!", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
			bot->orientation = orientation;
		      }
		    else
		      {
			if( !strcmp( o_str, orientation[RT_BOT_UNORIENTED] ) )
			  bot->orientation = RT_BOT_UNORIENTED;
			else if( !strcmp( o_str, orientation[RT_BOT_CCW] ) )
			  bot->orientation = RT_BOT_CCW;
			else if( !strcmp( o_str, orientation[RT_BOT_CW] ) )
			  bot->orientation = RT_BOT_CW;
			else
			  {
			    Tcl_SetResult( interp, "unrecognized orientation!!!", TCL_STATIC );
			    Tcl_DecrRefCount( list );
			    return( TCL_ERROR );
			  }
		      }
		  }

		Tcl_DecrRefCount( list );

		argc -= 2;
		argv += 2;
	}

	if( bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS )
	  {
	    if( !bot->thickness )
	      bot->thickness = (fastf_t *)bu_calloc( bot->num_faces, sizeof( fastf_t ), "BOT thickness" );
	    if( !bot->face_mode )
	      {
		bot->face_mode = bu_bitv_new( bot->num_faces );
		bu_bitv_clear( bot->face_mode );
	      }
	  }
	else
	  {
	    if( bot->thickness )
	      {
		bu_free( (char *)bot->thickness, "BOT thickness" );
		bot->thickness = (fastf_t *)NULL;
	      }
	    if( bot->face_mode )
	      {
		bu_free( (char *)bot->face_mode, "BOT facemode" );
		bot->face_mode = (bitv_t)NULL;
	      }
	  }

	return( TCL_OK );
}

/*	This routine adjusts the vertex pointers in each face so that 
 *	pointers to duplicate vertices end up pointing to the same vertex.
 *	The unused vertices are removed.
 *	Returns the number of vertices fused.
 */
int
bot_vertex_fuse( bot )
struct rt_bot_internal *bot;
{
	int i,j,k;
	int count=0;

	RT_BOT_CK_MAGIC( bot );

	for( i=0 ; i<bot->num_vertices ; i++ )
	{
		for( j=i+1 ; j<bot->num_vertices ; j++ )
		{
			/* specifically not using tolerances here */
			if( VEQUAL( &bot->vertices[i*3], &bot->vertices[j*3] ) )
			{
				count++;
				bot->num_vertices--;
				for( k=j ; k<bot->num_vertices ; k++ )
					VMOVE( &bot->vertices[k*3] , &bot->vertices[(k+1)*3] );
				for( k=0 ; k<bot->num_faces*3 ; k++ )
				{
					if( bot->faces[k] == j )
					{
						bot->faces[k] = i;
					}
					else if ( bot->faces[k] > j )
						bot->faces[k]--;
				}
			}
		}
	}

	return( count );
}

int
same_orientation( a, b )
int *a, *b;
{
	int i,j;

	for( i=0 ; i<3 ; j++ )
	{
		if( a[0] == b[i] )
		{
			i++;
			if( i == 3 )
				i = 0;
			if( a[1] == b[i] )
				return( 1 );
			else
				return( 0 );
		}
	}

	return( 0 );
}

int
bot_face_fuse( bot )
struct rt_bot_internal *bot;
{
	int num_faces;
	int i,j,k,l;
	int count=0;

	RT_BOT_CK_MAGIC( bot );

	num_faces = bot->num_faces;
	for( i=0 ; i<num_faces ; i++ )
	{
		j = i+1;
		while( j<num_faces )
		{
			/* each pass through this loop either increments j or decrements num_faces */
			int match=0;
			int elim;

			for( k=i*3 ; k<(i+1)*3 ; k++ )
			{
				for( l=j*3 ; l<(j+1)*3 ; l++ )
				{
					if( bot->faces[k] == bot->faces[l] )
					{
						match++;
						break;
					}
				}
			}

			if( match != 3 )
			{
				j++;
				continue;
			}

			/* these two faces have the same vertices */
			elim = -1;
			switch( bot->mode )
			{
				case RT_BOT_PLATE:
				case RT_BOT_PLATE_NOCOS:
					/* check the face thickness and face mode */
					if( bot->thickness[i] != bot->thickness[j] ||
						BU_BITTEST( bot->face_mode, i ) != BU_BITTEST( bot->face_mode, j ) )
							break;
				case RT_BOT_SOLID:
				case RT_BOT_SURFACE:
					if( bot->orientation == RT_BOT_UNORIENTED )
					{
						/* faces are identical, so eliminate one */
						elim = j;
					}
					else
					{
						/* need to check orientation */
						if( same_orientation( &bot->faces[i*3], &bot->faces[j*3] ) )
							elim = j;
					}
					break;
				default:
					bu_bomb( "bot_face_condense: Unrecognized BOT mode!!!\n" );
					break;
			}

			if( elim < 0 )
			{
				j++;
				continue;
			}

			/* we are eliminating face number "elim" */
			for( l=elim ; l< num_faces-1 ; l++ )
				VMOVE( &bot->faces[l*3], &bot->faces[(l+1)*3] )
			if( bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS )
			{
				for( l=elim ; l<num_faces-1 ; l++ )
				{
					bot->thickness[l] = bot->thickness[l+1];
					if( BU_BITTEST( bot->face_mode, l+1 ) )
						BU_BITSET( bot->face_mode, l );
					else
						BU_BITCLR( bot->face_mode, l );
				}
			}
			num_faces--;
		}
	}

	count = bot->num_faces - num_faces;

	if( count )
	{
		bot->num_faces = num_faces;
		bot->faces = (int *)bu_realloc( bot->faces, num_faces*3*sizeof( int ), "BOT faces realloc" );
		if( bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS )
		{
			struct bu_bitv *new_mode;

			bot->thickness = bu_realloc( bot->thickness, num_faces*sizeof( fastf_t ), "BOT thickness realloc" );
			new_mode = bu_bitv_new( num_faces );
			bu_bitv_clear( new_mode );
			for( l=0 ; l<num_faces ; l++ )
			{
				if( BU_BITTEST( bot->face_mode, l ) )
					BU_BITSET( new_mode, l );
			}
			bu_free( (char *)bot->face_mode, "BOT face_mode" );
			bot->face_mode = new_mode;
		}
	}

	return( count );
}

int
bot_condense( bot )
struct rt_bot_internal *bot;
{
	int i,j,k;
	int num_verts;
	int dead_verts=0;
	int *verts;

	RT_BOT_CK_MAGIC( bot );

	num_verts = bot->num_vertices;
	verts = (int *)bu_calloc( num_verts, sizeof( int ), "VERTEX LIST" );
	for( i=0 ; i<bot->num_faces*3 ; i++ )
	{
		j = bot->faces[i];
		if( j >= num_verts || j < 0 )
		{
			bu_log( "Illegal vertex number %d, should be 0 through %d\n", j, num_verts-1 );
			bu_bomb( "Illegal vertex number\n" );
		}
		verts[j] = 1;
	}

	i = 0;
	while( i < num_verts-dead_verts )
	{
		while( !verts[i] && i < num_verts-dead_verts )
		{
			dead_verts++;
			for( j=i ; j<num_verts-dead_verts ; j++ )
			{
				k = j+1;
				VMOVE( &bot->vertices[j*3], &bot->vertices[k*3] );
				verts[j] = verts[k];
			}
			for( j=0 ; j<bot->num_faces*3 ; j++ )
			{
				if( bot->faces[j] >= i )
					bot->faces[j]--;
			}
		}
		i++;
	}

	if( !dead_verts )
		return( 0 );

	bot->num_vertices -= dead_verts;
	bot->vertices = (fastf_t *)bu_realloc( bot->vertices, bot->num_vertices*3*sizeof( fastf_t ), "bot verts realloc" );

	return( dead_verts );
}
