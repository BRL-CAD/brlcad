/*
 *			G _ B O T . C
 *
 *  Purpose -
 *	Intersect a ray with a bag o' triangles
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_xxx_internal --- parameters for solid
 *	define xxx_specific --- raytracing form, possibly w/precomuted terms
 *	define rt_xxx_parse --- struct bu_structparse for "db get", "db adjust", ...
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_xxx_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_BOT, increment ID_MAXIMUM
 *	edit db_scan.c to add the new solid to db_scan()
 *	edit Cakefile to add g_xxx.c to compile
 *
 *	Then:
 *	go to /cad/libwdb and create mk_xxx() routine
 *	go to /cad/conv and edit g2asc.c and asc2g.c to support the new solid
 *	go to /cad/librt and edit tcl.c to add the new solid to  rt_solid_type_lookup[]
 *	go to /cad/mged and create the edit support
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
static char RCSxxx[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
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
	struct tri_specific *bot_facelist;
};

static fastf_t cos_min=0.0008726;

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
	int				tri_index;
	LOCAL fastf_t			dx, dy, dz;
	LOCAL fastf_t			f;

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
		(void)rt_botface( stp, bot, p1, p2, p3, tri_index, tol );
	}

	if( stp->st_specific == (genptr_t)0 )  {
		bu_log("bot(%s):  no faces\n", stp->st_name);
		return(-1);             /* BAD */
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

	return( 0 );
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
	if( m1 < tol->dist || m2 < tol->dist ||
	    m3 < tol->dist || m4 < tol->dist )  {
		bu_free( (char *)trip, "getstruct tri_specific");
	    	{
			bu_log("bot(%s): degenerate facet\n", stp->st_name);
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
	register CONST struct bot_specific *bot =
		(struct bot_specific *)stp->st_specific;
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
		if( ++nhits >= MAXHITS )  {
			bu_log("rt_bot_shot(%s): too many hits (%d)\n", stp->st_name, nhits);
			break;
		}
		hp++;
	}
	if( nhits == 0 )
		return(0);		/* MISS */

	/* Sort hits, Near to Far */
	{
		register int i, j;
		LOCAL struct hit temp;

		for( i=0; i < nhits-1; i++ )  {
			for( j=i+1; j < nhits; j++ )  {
				if( hits[i].hit_dist <= hits[j].hit_dist )
					continue;
				temp = hits[j];		/* struct copy */
				hits[j] = hits[i];	/* struct copy */
				hits[i] = temp;		/* struct copy */
			}
		}
	}

	/* build segments */
	if( bot->bot_mode == RT_BOT_PLATE )
	{
		register struct seg *segp;
		register int i;

		for( i=0; i < nhits; i++ )
		{
			register int surfno = hits[i].hit_surfno;
			register fastf_t los;

			if( NEAR_ZERO( hits[i].hit_vpriv[X], cos_min ) )
				continue;
			los = bot->bot_thickness[surfno] / hits[i].hit_vpriv[X];
			if( los < 0.0 )
				los = -los;
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

				/* set out hit */
				segp->seg_out.hit_surfno = surfno;
				segp->seg_out.hit_dist = segp->seg_in.hit_dist + los;
				segp->seg_out.hit_vpriv[X] = segp->seg_in.hit_vpriv[X]; /* ray dir dot normal */
				segp->seg_out.hit_vpriv[Y] = bot->bot_orientation;
				segp->seg_out.hit_vpriv[Z] = -1;
				segp->seg_out.hit_private = hits[i].hit_private;

				BU_LIST_INSERT( &(seghead->l), &(segp->l) );
			}
		}
		return( nhits );
	}

	if( bot->bot_mode == RT_BOT_SURFACE )
	{
		register struct seg *segp;
		register int i;

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
	}

	if( bot->bot_mode == RT_BOT_SOLID )
	{
		register struct seg *segp;
		register int i;

		if( bot->bot_orientation == RT_BOT_UNORIENTED )
		{
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

			if( nhits&1 )
			{
				bu_log( "rt_bot_shot(%s): WARNING: odd number of hits (%d), last hit ignored\n",
					stp->st_name, nhits );
				nhits--;
			}

			for( i=0 ; i<nhits ; i += 2 )
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
			return( nhits );
		}

		/* from this point on, process very similar to a polysolid */

		/* Remove duplicate hits.
		   We remove one of a pair of hits when they are
			1) close together, and
			2) both "entry" or both "exit" occurrences.
		   Two immediate "entry" or two immediate "exit" hits suggest
		   that we hit both of two joined faces, while we want to hit only
		   one.  An "entry" followed by an "exit" (or vice versa) suggests
		   that we grazed an edge, and thus we should leave both
		   in the hit list. */
		
		{
			register int i, j;
			LOCAL struct hit temp;

			for( i=0 ; i<nhits-1 ; i++ )
			{
				fastf_t dist;

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


		if( nhits == 1 )
			nhits = 0;

		if( nhits&1 )  {
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
				bu_log("rt_bot_shot(%s): WARNING %d hits:\n", stp->st_name, nhits);
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
			else
			{
				hits[nhits] = hits[nhits-1];	/* struct copy */
				hits[nhits].hit_vpriv[X] = -hits[nhits].hit_vpriv[X];
				bu_log( "\t\tadding fictitious hit at %f\n", hits[nhits].hit_dist );
				nhits++;
			}
		}

		/* nhits is even, build segments */
		{
			register struct seg *segp;
			register int	i;
			for( i=0; i < nhits; i += 2 )  {
				RT_GET_SEG(segp, ap->a_resource);
				segp->seg_stp = stp;
				segp->seg_in = hits[i];		/* struct copy */
				segp->seg_in.hit_vpriv[Z] = 1;
				segp->seg_out = hits[i+1];	/* struct copy */
				segp->seg_out.hit_vpriv[Z] = -1;
				BU_LIST_INSERT( &(seghead->l), &(segp->l) );
			}
		}
	}
	return(nhits);			/* HIT */
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
	register struct bot_specific *bot =
		(struct bot_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
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
	register struct bot_specific *bot =
		(struct bot_specific *)stp->st_specific;
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

	if( bot->bot_thickness )
		bu_free( (char *)bot->bot_thickness, "bot_thickness" );
	if( bot->bot_facemode )
		bu_free( (char *)bot->bot_facemode, "bot_facemode" );
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

        *r = nmg_mrsv( m );     /* Make region, empty shell, vertex */
        s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	verts = (struct vertex **)bu_calloc( bot_ip->num_vertices, sizeof( struct vertex *),
		"rt_bot_tess: *verts[]" );

	for( i=0 ; i<bot_ip->num_faces ; i++ )
	{
		struct faceuse *fu;
		struct vertex **corners[3];

		VMOVE( pt[0], &bot_ip->vertices[bot_ip->faces[i*3]*3] );
		VMOVE( pt[1], &bot_ip->vertices[bot_ip->faces[i*3+1]*3] );
		VMOVE( pt[2], &bot_ip->vertices[bot_ip->faces[i*3+2]*3] );

		if( !bn_3pts_distinct( pt[0], pt[1], pt[2], tol )
                           || bn_3pts_collinear( pt[0], pt[1], pt[2], tol ) )
				continue;

		corners[0] = &verts[bot_ip->faces[i*3]];
		corners[1] = &verts[bot_ip->faces[i*3+1]];
		corners[2] = &verts[bot_ip->faces[i*3+2]];
		if( (fu=nmg_cmface( s, corners, 3 )) == (struct faceuse *)NULL )
			bu_log( "rt_bot_tess() nmg_cmface() failed for face #%d\n", i );

		if( !(*corners[0])->vg_p )
			nmg_vertex_gv( *(corners[0]), pt[0] );
		if( !(*corners[1])->vg_p )
			nmg_vertex_gv( *(corners[1]), pt[1] );
		if( !(*corners[2])->vg_p )
			nmg_vertex_gv( *(corners[2]), pt[2] );

		if( nmg_calc_face_g( fu ) )
			nmg_kfu( fu );
	}

	bu_free( (char *)verts, "rt_bot_tess *verts[]" );

	nmg_mark_edges_real( &s->l );

	nmg_region_a( *r, tol );

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

	if( bot_ip->mode == RT_BOT_PLATE )
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
	if( bot_ip->mode == RT_BOT_PLATE )
	{
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

	if( bot_ip->mode == RT_BOT_PLATE )
	{
		for( i=0 ; i<bot_ip->num_faces ; i++ )
		{
			htond( (unsigned char *)&rec->bot.bot_data[chars_used], (CONST unsigned char *)&bot_ip->thickness[i], 1 );
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
			sprintf( buf, "\tface %d: (%g %g %g), (%g %g %g), (%g %g %g)\n", i,
				V3ARGS( &bot_ip->vertices[bot_ip->faces[i*3]*3] ),
				V3ARGS( &bot_ip->vertices[bot_ip->faces[i*3+1]*3] ),
				V3ARGS( &bot_ip->vertices[bot_ip->faces[i*3+2]*3] ) );
			bu_vls_strcat( str, buf );
			if( bot_ip->mode == RT_BOT_PLATE )
			{
				char *face_mode;

				if( BU_BITTEST( bot_ip->face_mode, i ) )
					face_mode = "appended to hit point";
				else
					face_mode = "centered about hit point";
				sprintf( buf, "\t\tthickness = %g, %s\n", bot_ip->thickness[i], face_mode );
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

	if( bot_ip->thickness )
		bu_free( (char *)bot_ip->thickness, "BOT thickness" );
	if( bot_ip->face_mode )
		bu_free( (char *)bot_ip->face_mode, "BOT face_mode" );

	bu_free( (char *)bot_ip, "bot ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

int
rt_bot_tnurb( r, m, ip, tol )
struct nmgregion        **r;
struct model            *m;
struct rt_db_internal   *ip;
struct bn_tol           *tol;
{
	return( 1 );
}

int
rt_bot_xform( op, mat, ip, free )
struct rt_db_internal *op, *ip;
CONST mat_t	mat;
CONST int free;
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
