/*
 *			R E F R A C T
 *
 *  Authors -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985,1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSrefract[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./rdebug.h"
#include "./material.h"
#include "./mathtab.h"

#define MAX_IREFLECT	9	/* Maximum internal reflection level */
#define MAX_BOUNCE	4	/* Maximum recursion level */

#define RI_AIR		1.0    /* Refractive index of air.		*/

HIDDEN int	rr_hit(), rr_miss();
HIDDEN int	rr_refract();

extern vect_t	background;

/*
 *			R R _ R E N D E R
 */
HIDDEN int
rr_render( ap, pp, swp, dp )
register struct application *ap;
struct partition *pp;
struct shadework	*swp;
char	*dp;
{
	auto struct application sub_ap;
	auto fastf_t	f;
	auto vect_t	work;
	auto vect_t	incident_dir;
	auto vect_t	filter_color;

	if( swp->sw_reflect <= 0 && swp->sw_transmit <= 0 )
		goto finish;
	if( ap->a_level >= MAX_BOUNCE )  {
		/* Nothing more to do for this ray */
#ifdef debug
		rt_log("rr_render: lvl=%d, stopping at %s\n",
			ap->a_level,
			pp->pt_inseg->seg_stp->st_name);
		VSET( swp->sw_color, 99, 99, 0 );
#else
		VSETALL( swp->sw_color, 0 );
#endif
		goto finish;
	}
	VMOVE( filter_color, swp->sw_basecolor );

	/*
	 *  Diminish base color appropriately, and add in
	 *  contributions from mirror reflection & transparency
	 */
	f = 1 - (swp->sw_reflect + swp->sw_transmit);
	if( f < 0 )  f = 0;
	else if( f > 1 )  f = 1;
	VSCALE( swp->sw_color, swp->sw_color, f );

	if( swp->sw_reflect > 0 )  {
		LOCAL vect_t	to_eye;

		/* Mirror reflection */
		sub_ap = *ap;		/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		sub_ap.a_onehit = 1;
		VMOVE( sub_ap.a_ray.r_pt, swp->sw_hit.hit_point );
		VREVERSE( to_eye, ap->a_ray.r_dir );
		f = 2 * VDOT( to_eye, swp->sw_hit.hit_normal );
		VSCALE( work, swp->sw_hit.hit_normal, f );
		/* I have been told this has unit length */
		VSUB2( sub_ap.a_ray.r_dir, work, to_eye );
		sub_ap.a_purpose = "reflected ray";
		(void)rt_shootray( &sub_ap );

		/* a_user has hit/miss flag! */
		if( sub_ap.a_user == 0 )  {
			VMOVE( sub_ap.a_color, background );
		}
		VJOIN1(swp->sw_color, swp->sw_color,
			swp->sw_reflect, sub_ap.a_color);
	}
	if( swp->sw_transmit > 0 )  {

		/* Calculate refraction at entrance. */
		sub_ap = *ap;		/* struct copy */
		sub_ap.a_level = 0;	/* # of internal reflections */
		sub_ap.a_onehit = 1;
		sub_ap.a_user = (int)(pp->pt_regionp);
		VMOVE( sub_ap.a_ray.r_pt, swp->sw_hit.hit_point );
		VMOVE( incident_dir, ap->a_ray.r_dir );
		if( !rr_refract(incident_dir, /* Incident ray (IN) */
			swp->sw_hit.hit_normal,
			RI_AIR, swp->sw_refrac_index,
			sub_ap.a_ray.r_dir	/* Refracted ray (OUT) */
		) )  {
			/* Reflected back outside solid */
			VSETALL( filter_color, 1 );
			goto do_exit;
		}

		/*
		 *  Find new exit point from the inside. 
		 *  We will iterate, but not recurse, due to the special
		 *  (non-recursing) hit and miss routines used here for
		 *  internal reflection.
		 */
do_inside:
		sub_ap.a_hit =  rr_hit;
		sub_ap.a_miss = rr_miss;
		sub_ap.a_purpose = "internal reflection";
		if( rt_shootray( &sub_ap ) == 0 )  {
			if(rdebug&RDEBUG_HITS)rt_log("rr_render: Refracted ray missed '%s' -- RETRYING, lvl=%d\n",
				pp->pt_inseg->seg_stp->st_name,
				sub_ap.a_level );
			/* Back off just a little bit, and try again */
			/* Useful when striking exactly in corners */
			VJOIN1( sub_ap.a_ray.r_pt, sub_ap.a_ray.r_pt,
				-0.5, incident_dir );
			sub_ap.a_purpose = "backed off, internal reflection";
			if( rt_shootray( &sub_ap ) == 0 )  {
				rt_log("rr_render: Refracted ray missed 2x '%s', lvl=%d\n",
					pp->pt_inseg->seg_stp->st_name,
					sub_ap.a_level );
				VPRINT("pt", sub_ap.a_ray.r_pt );
				VPRINT("dir", sub_ap.a_ray.r_dir );
				VSET( swp->sw_color, 0, 99, 0 );	/* green */
				goto finish;		/* abandon hope */
			}
		}
		/* NOTE: rr_hit returns EXIT Point in sub_ap.a_uvec,
		 *  and returns EXIT Normal in sub_ap.a_vvec.
		 */
		if( rdebug&RDEBUG_RAYWRITE )  {
			wraypts( sub_ap.a_ray.r_pt, sub_ap.a_uvec,
				ap, stdout );
		}
		if( rdebug&RDEBUG_RAYPLOT )  {
			pl_color( stdout, 0, 255, 0 );
			rt_drawvec( stdout, ap->a_rt_i,
				sub_ap.a_ray.r_pt,
				sub_ap.a_uvec );
		}
		VMOVE( sub_ap.a_ray.r_pt, sub_ap.a_uvec );

		/* Calculate refraction at exit. */
		VMOVE( incident_dir, sub_ap.a_ray.r_dir );
		if( !rr_refract( incident_dir,		/* input direction */
			sub_ap.a_vvec,			/* exit normal */
			swp->sw_refrac_index, RI_AIR,
			sub_ap.a_ray.r_dir		/* output direction */
		) )  {
			static long count = 0;

			/* Reflected internally -- keep going */
			if( (++sub_ap.a_level) <= MAX_IREFLECT )
				goto do_inside;

			if( ++count > 100 )  {
				if( (count%100) != 3 )  goto show_err;
				rt_log("(reflections omitted)\n");
			}
			rt_log("rr_render: %s Internal reflection stopped after %d bounces, (x%d, y%d, lvl=%d)\n",
				pp->pt_inseg->seg_stp->st_name,
				sub_ap.a_level,
				sub_ap.a_x, sub_ap.a_y, ap->a_level );
show_err:		;
			if(rdebug&RDEBUG_SHOWERR) {
				VSET( swp->sw_color, 0, 9, 0 );	/* green */
			} else {
				/* 18% grey, filtered */
				VSETALL( work, .18*swp->sw_transmit );
				VELMUL( swp->sw_color, filter_color, work );
			}
			goto finish;
		}
do_exit:
		/* This is the only place we might recurse dangerously */
		sub_ap.a_hit =  ap->a_hit;
		sub_ap.a_miss = ap->a_miss;
		sub_ap.a_level = ap->a_level+1;
		sub_ap.a_purpose = "escaping refracted ray";
		(void) rt_shootray( &sub_ap );

		/* a_user has hit/miss flag! */
		if( sub_ap.a_user == 0 )  {
			VMOVE( sub_ap.a_color, background );
		}
		VELMUL( work, filter_color, sub_ap.a_color );
		VJOIN1( swp->sw_color, swp->sw_color,
			swp->sw_transmit, work );
	}
finish:
	return(1);
}

/*
 *			R R _ M I S S
 */
HIDDEN int
/*ARGSUSED*/
rr_miss( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	return(0);
}

/*
 *			R R _ H I T
 *
 *  Implicit Returns -
 *	a_uvec	exit Point
 *	a_vvec	exit Normal
 */
HIDDEN int
rr_hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct hit	*hitp;
	register struct soltab *stp;
	register struct partition *pp;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist > 0.0 )  break;
	if( pp == PartHeadp )  {
		if(rdebug&RDEBUG_SHOWERR)
			rt_log("rr_hit:  no hit out front?\n");
		goto bad;
	}
	if( pp->pt_regionp != (struct region *)(ap->a_user) )  {
		if(rdebug&RDEBUG_HITS)
			rt_log("rr_hit:  Ray reflected within %s now in %s!\n",
				((struct region *)(ap->a_user))->reg_name,
				pp->pt_regionp->reg_name );
		goto bad;
	}

	hitp = pp->pt_inhit;
	if( !NEAR_ZERO(hitp->hit_dist, 10) )  {
/**		if(rdebug&RDEBUG_HITS) **/
		{
			stp = pp->pt_inseg->seg_stp;
			RT_HIT_NORM( hitp, stp, &(ap->a_ray) );
			if( pp->pt_inflip )  {
				VREVERSE( hitp->hit_normal, hitp->hit_normal );
			}
			rt_log("rr_hit:  '%s' inhit %g not near zero!\n",
				stp->st_name, hitp->hit_dist);
			rt_pr_hit("inhit", hitp);
		}
		goto bad;
	}

	hitp = pp->pt_outhit;
	stp = pp->pt_outseg->seg_stp;
	if( hitp->hit_dist >= INFINITY )  {
		if(rdebug&RDEBUG_SHOWERR)
			rt_log("rr_hit:  (%g,%g) bad!\n",
				pp->pt_inhit->hit_dist, hitp->hit_dist);
		goto bad;
	}
	VJOIN1( hitp->hit_point, ap->a_ray.r_pt,
		hitp->hit_dist, ap->a_ray.r_dir );
	RT_HIT_NORM( hitp, stp, &(ap->a_ray) );
	if( pp->pt_outflip )  {
		VREVERSE( hitp->hit_normal, hitp->hit_normal );
	}
	VMOVE( ap->a_uvec, hitp->hit_point );

	/* Safety check */
	if( (rdebug&RDEBUG_SHOWERR) && (
	    !NEAR_ZERO(hitp->hit_normal[X], 1.001) ||
	    !NEAR_ZERO(hitp->hit_normal[Y], 1.001) ||
	    !NEAR_ZERO(hitp->hit_normal[Z], 1.001) ) )  {
	    	rt_log("rr_hit: defective normal hitting %s\n", stp->st_name);
	    	VPRINT("hit_normal", hitp->hit_normal);
	    	goto bad;
	}
	/* For refraction, want exit normal to point inward. */
	VREVERSE( ap->a_vvec, hitp->hit_normal );
	return(1);

	/* Give serious information when problems are encountered */
bad:
	if(rdebug&RDEBUG_HITS) rt_pr_partitions( ap->a_rt_i, PartHeadp, "rr_hit" );
	return(0);
}

/*
 *			R E F R A C T
 *
 *	Compute the refracted ray 'v_2' from the incident ray 'v_1' with
 *	the refractive indices 'ri_2' and 'ri_1' respectively.
 *	Using Schnell's Law:
 *
 *		theta_1 = angle of v_1 with surface normal
 *		theta_2 = angle of v_2 with reversed surface normal
 *		ri_1 * sin( theta_1 ) = ri_2 * sin( theta_2 )
 *
 *		sin( theta_2 ) = ri_1/ri_2 * sin( theta_1 )
 *		
 *	The above condition is undefined for ri_1/ri_2 * sin( theta_1 )
 *	being greater than 1, and this represents the condition for total
 *	reflection, the 'critical angle' is the angle theta_1 for which
 *	ri_1/ri_2 * sin( theta_1 ) equals 1.
 *
 *  Returns TRUE if refracted, FALSE if reflected.
 *
 *  Note:  output (v_2) can be same storage as an input.
 */
HIDDEN int
rr_refract( v_1, norml, ri_1, ri_2, v_2 )
register vect_t	v_1;
register vect_t	norml;
double	ri_1, ri_2;
register vect_t	v_2;
{
	LOCAL vect_t	w, u;
	FAST fastf_t	beta;

	if( NEAR_ZERO(ri_1, 0.0001) || NEAR_ZERO( ri_2, 0.0001 ) )  {
		rt_log("rr_refract:ri1=%g, ri2=%g\n", ri_1, ri_2 );
		beta = 1;
	} else {
		beta = ri_1/ri_2;		/* temp */
		if( beta > 10000 )  {
			rt_log("rr_refract:  beta=%g\n", beta);
			beta = 1000;
		}
	}
	VSCALE( w, v_1, beta );
	VCROSS( u, w, norml );
	    	
	/*
	 *	|w X norml| = |w||norml| * sin( theta_1 )
	 *	        |u| = ri_1/ri_2 * sin( theta_1 ) = sin( theta_2 )
	 */
	if( (beta = VDOT( u, u )) > 1.0 )  {
		/*  Past critical angle, total reflection.
		 *  Calculate reflected (bounced) incident ray.
		 */
		VREVERSE( u, v_1 );
		beta = 2 * VDOT( u, norml );
		VSCALE( w, norml, beta );
		VSUB2( v_2, w, u );
		return(0);		/* reflected */
	} else {
		/*
		 * 1 - beta = 1 - sin( theta_2 )^^2
		 *	    = cos( theta_2 )^^2.
		 *     beta = -1.0 * cos( theta_2 ) - Dot( w, norml ).
		 */
		beta = -sqrt( 1.0 - beta) - VDOT( w, norml );
		VSCALE( u, norml, beta );
		VADD2( v_2, w, u );
		return(1);		/* refracted */
	}
	/* NOTREACHED */
}
