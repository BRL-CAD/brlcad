/*
 *			L I G H T . C
 *
 *  Implement simple isotropic light sources as a material property.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSlight[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./light.h"
#include "./rdebug.h"


struct matparse light_parse[] = {
	"inten",	(mp_off_ty)&(LIGHT_NULL->lt_intensity),	"%f",
	"angle",	(mp_off_ty)&(LIGHT_NULL->lt_angle),	"%f",
	"fract",	(mp_off_ty)&(LIGHT_NULL->lt_fraction),	"%f",
	"shadows",	(mp_off_ty)&(LIGHT_NULL->lt_shadows),	"%d",
	"infinite",	(mp_off_ty)&(LIGHT_NULL->lt_infinite),	"%d",
	"invisible",	(mp_off_ty)&(LIGHT_NULL->lt_invisible),	"%d",
	(char *)0,	(mp_off_ty)0,				(char *)0
};

struct light_specific *LightHeadp = LIGHT_NULL;		/* Linked list of lights */

extern double AmbientIntensity;

HIDDEN int	light_setup(), light_render();
HIDDEN void	light_print();
void		light_free();

struct mfuncs light_mfuncs[] = {
	"light",	0,		0,		MFI_NORMAL,
	light_setup,	light_render,	light_print,	light_free,

	(char *)0,	0,		0,
	0,		0,		0,		0
};

/*
 *			L I G H T _ R E N D E R
 *
 *  If we have a direct view of the light, return it's color.
 *  A cosine term is needed in the shading of the light source,
 *  to make it have dimension and shape.  However, just a simple
 *  cosine of the angle between the normal and the direction vector
 *  leads to a pretty dim looking light.  Therefore, a cos/2 + 0.5
 *  term is used when the viewer is within the beam, and a cos/2 term
 *  when the beam points away.
 */
HIDDEN int
light_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct light_specific *lp =
		(struct light_specific *)dp;
	register fastf_t f;

	/* Provide cosine/2 shading, to make light look round */
	if( (f = -VDOT( swp->sw_hit.hit_normal, ap->a_ray.r_dir )*0.5) < 0 )
		f = 0;

	/* See if surface normal falls in light beam direction */
	if( VDOT( lp->lt_aim, swp->sw_hit.hit_normal) < lp->lt_cosangle )  {
		/* dark, outside of light beam area */
		f *= lp->lt_fraction;
	} else {
		/* within beam area */
		f = (f+0.5) * lp->lt_fraction;
	}
	VSCALE( swp->sw_color, lp->lt_color, f );
	return(1);
}

/*
 *			L I G H T _ S E T U P
 *
 *  Called once for each light-emitting region.
 */
HIDDEN int
light_setup( rp, matparm, dpp )
register struct region *rp;
char	*matparm;
char	**dpp;
{
	register struct light_specific *lp;
	register struct soltab *stp;
	vect_t	work;
	fastf_t	f;

	GETSTRUCT( lp, light_specific );
	*dpp = (char *)lp;

	lp->lt_intensity = 1000.0;	/* Lumens */
	lp->lt_fraction = -1.0;		/* Recomputed later */
	lp->lt_invisible = 0;		/* explicitly modeled */
	lp->lt_shadows = 1;		/* by default, casts shadows */
	lp->lt_angle = 180;		/* spherical emission by default */
	lp->lt_infinite = 0;
	lp->lt_rp = rp;
	lp->lt_name = rt_strdup( rp->reg_name );
	mlib_parse( matparm, light_parse, (mp_off_ty)lp );

	if( lp->lt_angle > 180 )  lp->lt_angle = 180;
	lp->lt_cosangle = cos( (double) lp->lt_angle * 0.0174532925199433 );

	/* Determine position and size */
	if( rp->reg_treetop->tr_op == OP_SOLID )  {

		stp = rp->reg_treetop->tr_a.tu_stp;
		VMOVE( lp->lt_pos, stp->st_center );
		lp->lt_radius = stp->st_aradius;
	} else {
		vect_t	min_rpp, max_rpp, avg, rad;
		register union tree *tp;

		VSETALL( min_rpp,  INFINITY );
		VSETALL( max_rpp, -INFINITY );
		rt_rpp_tree( rp->reg_treetop, min_rpp, max_rpp );
		VADD2( avg, min_rpp, max_rpp );
		VSCALE( avg, avg, 0.5 );
		VMOVE( lp->lt_pos, avg );
		VSUB2( rad, max_rpp, avg );
		lp->lt_radius = MAGNITUDE( rad );

		/* Find first leaf node on left of tree */
		tp = rp->reg_treetop;
		while( tp->tr_op != OP_SOLID )
			tp = tp->tr_b.tb_left;
		stp = tp->tr_a.tu_stp;
	}

	/* Light is aimed down -Z in it's local coordinate system */
	VSET( work, 0, 0, -1 );
	MAT4X3VEC( lp->lt_aim, stp->st_pathmat, work );
	VUNITIZE( lp->lt_aim );

	if( rp->reg_mater.ma_override )  {
		VMOVE( lp->lt_color, rp->reg_mater.ma_color );
	} else {
		VSETALL( lp->lt_color, 1 );
	}
	rt_log( "%s at (%g, %g, %g), aimed at (%g, %g, %g), angle=%g\n",
		lp->lt_name,
		lp->lt_pos[X], lp->lt_pos[Y], lp->lt_pos[Z],
		lp->lt_aim[X], lp->lt_aim[Y], lp->lt_aim[Z],
		lp->lt_angle );

	VMOVE( lp->lt_vec, lp->lt_pos );
	f = MAGNITUDE( lp->lt_vec );
	if( f < SQRT_SMALL_FASTF ) {
		/* light at the origin, make its direction vector up */
		VSET( lp->lt_vec, 0, 0, 1 );
	} else {
		VSCALE( lp->lt_vec, lp->lt_vec, f );
	}

	/* Add to linked list of lights */
	lp->lt_forw = LightHeadp;
	LightHeadp = lp;

	if( lp->lt_invisible )
		return(0);	/* don't show it */

	return(1);
}

/*
 *			L I G H T _ P R I N T
 */
HIDDEN void
light_print( rp, dp )
register struct region *rp;
char	*dp;
{
	mlib_print(rp->reg_name, light_parse, (mp_off_ty)dp);
}

/*
 *			L I G H T _ F R E E
 */
void
light_free( cp )
char *cp;
{

	if( LightHeadp == LIGHT_NULL )  {
		rt_log("light_free(x%x), list is null\n", cp);
		return;
	}
	if( LightHeadp == (struct light_specific *)cp )  {
		LightHeadp = LightHeadp->lt_forw;
	} else {
		register struct light_specific *lp;	/* current light */
		register struct light_specific **llp;	/* last light lt_forw */

		llp = &LightHeadp;
		lp = LightHeadp->lt_forw;
		while( lp != LIGHT_NULL )  {
			if( lp == (struct light_specific *)cp )  {
				*llp = lp->lt_forw;
				goto found;
			}
			llp = &(lp->lt_forw);
			lp = lp->lt_forw;
		}
		rt_log("light_free:  unable to find light in list\n");
	}
found:
	if( ((struct light_specific *)cp)->lt_name )
		rt_free( cp, "light name" );
	rt_free( cp, "light_specific" );
}

/*
 *			L I G H T _ M A K E R
 *
 *  Special hook called by view_2init to build 1 or 3 debugging lights.
 */
void
light_maker(num, v2m)
int	num;
mat_t	v2m;
{
	register struct light_specific *lp;
	register int i;
	vect_t	temp;
	vect_t	color;
	char	name[64];

	/* Determine the Light location(s) in view space */
	for( i=0; i<num; i++ )  {
		switch(i)  {
		case 0:
			/* 0:  At left edge, 1/2 high */
			VSET( color, 1,  1,  1 );	/* White */
			VSET( temp, -1, 0, 1 );
			break;

		case 1:
			/* 1: At right edge, 1/2 high */
			VSET( color,  1, .1, .1 );	/* Red-ish */
			VSET( temp, 1, 0, 1 );
			break;

		case 2:
			/* 2:  Behind, and overhead */
			VSET( color, .1, .1,  1 );	/* Blue-ish */
			VSET( temp, 0, 1, -0.5 );
			break;

		default:
			return;
		}
		GETSTRUCT( lp, light_specific );
		VMOVE( lp->lt_color, color );
		MAT4X3VEC( lp->lt_pos, v2m, temp );
		VMOVE( lp->lt_vec, lp->lt_pos );
		VUNITIZE( lp->lt_vec );

		sprintf(name, "Implicit light %d", i);
		lp->lt_name = rt_strdup(name);

		VSET( lp->lt_aim, 0, 0, -1 );	/* any direction: spherical */
		lp->lt_intensity = 1000.0;
		lp->lt_radius = 0.1;		/* mm, "point" source */
		lp->lt_invisible = 1;		/* NOT explicitly modeled */
		lp->lt_shadows = 1;		/* casts shadows */
		lp->lt_angle = 180;		/* spherical emission */
		lp->lt_cosangle = -1;		/* cos(180) */
		lp->lt_infinite = 0;
		lp->lt_rp = REGION_NULL;
		lp->lt_forw = LightHeadp;
		LightHeadp = lp;
	}
}

/*
 *			L I G H T _ I N I T
 *
 *  Special routine called by view_2init() to determine the relative
 *  intensities of each light source.
 *
 *  Because of the limited dynamic range of RGB space (0..255),
 *  the strategy used here is a bit risky.  We find the brightest
 *  single light source in the model, and assume that the energy from
 *  multiple lights will not shine on a single location in such a way
 *  as to add up to an overload condition.
 *  We then account for the effect of ambient light, because it always
 *  adds it's contribution.  Even here we only expect 50% of the ambient
 *  intensity, to keep the pictures reasonably bright.
 */
int
light_init()
{
	register struct light_specific *lp;
	register int	nlights = 0;
	register fastf_t	inten = 0.0;

	for( lp = LightHeadp; lp; lp = lp->lt_forw )  {
		nlights++;
		if( lp->lt_fraction > 0 )  continue;	/* overridden */
		if( lp->lt_intensity <= 0 )
			lp->lt_intensity = 1;		/* keep non-neg */
		if( lp->lt_intensity > inten )
			inten = lp->lt_intensity;
	}

	/* Compute total emitted energy, including ambient */
/**	inten *= (1 + AmbientIntensity); **/
	/* This is non-physical and risky, but gives nicer pictures for now */
	inten *= (1 + AmbientIntensity*0.5);

	for( lp = LightHeadp; lp; lp = lp->lt_forw )  {
		if( lp->lt_fraction > 0 )  continue;	/* overridden */
		lp->lt_fraction = lp->lt_intensity / inten;
	}
	if( nlights > SW_NLIGHTS )  {
		rt_log("Number of lights limited to %d\n", SW_NLIGHTS);
		nlights = SW_NLIGHTS;
	}
	return(nlights);
}

/* 
 *			L I G H T _ H I T
 *
 *  Input -
 *	a_color[] contains the fraction of a the light that will be
 *	propagated back along the ray, so far.  If this gets too small,
 *	recursion through lots of glass ought to stop.
 *  Output -
 *	a_color[] contains the fraction of light that can be seen.
 *	RGB transmissions are separately indicated, to allow simplistic
 *	colored glass (with apologies to Roy Hall).
 *
 *  These shadow functions return a boolean "light_visible".
 * 
 *  This is a simplified algorithm, and could be improved.
 *  Reflected light can't be dealt with at all.
 *
 *  Would also be nice to return an actual energy level, rather than
 *  a boolean, which could account for distance, etc.
 */
light_hit(ap, PartHeadp)
struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	register struct region	*regp;
	struct application	sub_ap;
	struct shadework	sw;
	struct light_specific	*lp;
	extern int	light_render();
	vect_t	filter_color;
	int	light_visible;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("light_hit:  no hit out front?\n");
		light_visible = 0;
		goto out;
	}
	regp = pp->pt_regionp;

	/* Check to see if we hit the light source */
	lp = (struct light_specific *)(ap->a_user);
	if( lp->lt_rp == regp )  {
		VSETALL( ap->a_color, 1 );
		light_visible = 1;
		goto out;
	}

	/* or something futher away than a finite invisible light */
	if( lp->lt_invisible && !(lp->lt_infinite) ) {
		vect_t	tolight;
		VSUB2( tolight, lp->lt_pos, ap->a_ray.r_pt );
		if( pp->pt_inhit->hit_dist >= MAGNITUDE(tolight) ) {
			VSETALL( ap->a_color, 1 );
			light_visible = 1;
			goto out;
		}
	}

	/* If we hit an entirely opaque object, this light is invisible */
	if( pp->pt_outhit->hit_dist >= INFINITY ||
	    regp->reg_transmit == 0 )  {
		VSETALL( ap->a_color, 0 );
		light_visible = 0;
		goto out;
	}

	/*  See if any further contributions will mater */
	if( ap->a_color[0] + ap->a_color[1] + ap->a_color[2] < 0.01 )  {
	    	/* Any light energy is "fully" attenuated by here */
		VSETALL( ap->a_color, 0 );
		light_visible = 0;
		goto out;
	}

	/*
	 *  Determine transparency parameters of this object.
	 *  All we really need here is the opacity information;
	 *  full shading is not required.
	 */
	sw.sw_transmit = sw.sw_reflect = 0.0;
	sw.sw_refrac_index = 1.0;
	sw.sw_xmitonly = 1;		/* only want sw_transmit */
	VSETALL( sw.sw_color, 1 );
	VSETALL( sw.sw_basecolor, 1 );

	viewshade( ap, pp, &sw );

	VSCALE( filter_color, sw.sw_color, sw.sw_transmit );
	if( filter_color[0] + filter_color[1] + filter_color[2] < 0.01 )  {
	    	/* Any recursion won't be significant */
		VSETALL( ap->a_color, 0 );
		light_visible = 0;
		goto out;
	}

	/*
	 * Push on to exit point, and trace on from there.
	 * Transmission so far is passed along in sub_ap.a_color[];
	 * Don't even think of trying to refract, or we will miss the light!
	 */
	sub_ap = *ap;			/* struct copy */
	sub_ap.a_level = ap->a_level+1;
	{
		FAST fastf_t f;
		f = pp->pt_outhit->hit_dist+0.0001;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
	}
	sub_ap.a_purpose = "light transmission after filtering";
	light_visible = rt_shootray( &sub_ap );

	VELMUL( ap->a_color, sub_ap.a_color, filter_color );
out:
	if( rdebug & RDEBUG_LIGHT ) rt_log("light %s vis=%d\n", regp->reg_name, light_visible);
	return(light_visible);
}

/*
 *  			L I G H T _ M I S S
 *  
 *  If there is no explicit light solid in the model, we will always "miss"
 *  the light, so return light_visible = TRUE.
 */
/* ARGSUSED */
light_miss(ap, PartHeadp)
register struct application *ap;
struct partition *PartHeadp;
{
	extern struct light_specific *LightHeadp;
	struct light_specific *lp = (struct light_specific *)(ap->a_user);

	if( lp->lt_invisible || lp->lt_infinite ) {
		VSETALL( ap->a_color, 1 );
		return( 1 );
	}
	if( LightHeadp )  {
		/* Explicit lights exist, somehow we missed (dither?) */
		VSETALL( ap->a_color, 0 );
		return(0);		/* light_visible = 0 */
	}
	/* No explicit light -- it's hard to hit */
	rt_log( "light: warning - invisible light not on list?!\n" );
	VSETALL( ap->a_color, 1 );
	return(1);			/* light_visible = 1 */
}

/* Null function */
nullf() { return(0); }
