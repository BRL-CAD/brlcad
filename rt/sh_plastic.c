/*
 *			P L A S T I C
 *
 *  Notes -
 *	The normals on all surfaces point OUT of the solid.
 *	The incomming light rays point IN.  Thus the sign change.
 *
 *  Authors -
 *	Michael John Muuss
 *	Gary S. Moss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSplastic[] = "@(#)$Header$ (BRL)";
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
#include "./light.h"

extern struct light_specific *LightHeadp;

/* from view.c */
extern int lightmodel;		/* lighting model # to use */
extern double AmbientIntensity;

/* Local information */
struct phong_specific {
	int	shine;
	double	wgt_specular;
	double	wgt_diffuse;
	double	transmit;	/* Moss "transparency" */
	double	reflect;	/* Moss "transmission" */
	double	refrac_index;
};
#define PL_NULL	((struct phong_specific *)0)

struct matparse phong_parse[] = {
	"shine",	(mp_off_ty)&(PL_NULL->shine),		"%d",
	"sh",		(mp_off_ty)&(PL_NULL->shine),		"%d",
	"specular",	(mp_off_ty)&(PL_NULL->wgt_specular),	"%f",
	"sp",		(mp_off_ty)&(PL_NULL->wgt_specular),	"%f",
	"diffuse",	(mp_off_ty)&(PL_NULL->wgt_diffuse),	"%f",
	"di",		(mp_off_ty)&(PL_NULL->wgt_diffuse),	"%f",
	"transmit",	(mp_off_ty)&(PL_NULL->transmit),	"%f",
	"tr",		(mp_off_ty)&(PL_NULL->transmit),	"%f",
	"reflect",	(mp_off_ty)&(PL_NULL->reflect),		"%f",
	"re",		(mp_off_ty)&(PL_NULL->reflect),		"%f",
	"ri",		(mp_off_ty)&(PL_NULL->refrac_index),	"%f",
	(char *)0,	(mp_off_ty)0,				(char *)0
};

HIDDEN int phong_setup(), mirror_setup(), glass_setup();
HIDDEN int phong_render();
HIDDEN int phong_print();
HIDDEN int phong_free();

struct mfuncs phg_mfuncs[] = {
	"plastic",	0,		0,		MFI_NORMAL,
	phong_setup,	phong_render,	phong_print,	phong_free,

	"mirror",	0,		0,		MFI_NORMAL,
	mirror_setup,	phong_render,	phong_print,	phong_free,

	"glass",	0,		0,		MFI_NORMAL,
	glass_setup,	phong_render,	phong_print,	phong_free,

	(char *)0,	0,		0,		0,
	0,		0,		0,		0
};

extern int light_hit(), light_miss();
extern double ipow();

#define RI_AIR		1.0    /* Refractive index of air.		*/

/*
 *			P H O N G _ S E T U P
 */
HIDDEN int
phong_setup( rp )
register struct region *rp;
{
	register struct phong_specific *pp;

	GETSTRUCT( pp, phong_specific );
	rp->reg_udata = (char *)pp;

	pp->shine = 10;
	pp->wgt_specular = 0.7;
	pp->wgt_diffuse = 0.3;
	pp->transmit = 0.0;
	pp->reflect = 0.0;
	pp->refrac_index = RI_AIR;

	mlib_parse( rp->reg_mater.ma_matparm, phong_parse, (mp_off_ty)pp );

	VSCALE( rp->reg_mater.ma_transmit, 
		rp->reg_mater.ma_color, pp->transmit );
	return(1);
}

/*
 *			M I R R O R _ S E T U P
 */
HIDDEN int
mirror_setup( rp )
register struct region *rp;
{
	register struct phong_specific *pp;

	GETSTRUCT( pp, phong_specific );
	rp->reg_udata = (char *)pp;

	pp->shine = 4;
	pp->wgt_specular = 0.6;
	pp->wgt_diffuse = 0.4;
	pp->transmit = 0.0;
	pp->reflect = 0.75;
	pp->refrac_index = 1.65;

	mlib_parse( rp->reg_mater.ma_matparm, phong_parse, (mp_off_ty)pp );
	if(rdebug&RDEBUG_MATERIAL)
		mlib_print(rp->reg_name, phong_parse, (mp_off_ty)pp);

	VSCALE( rp->reg_mater.ma_transmit, 
		rp->reg_mater.ma_color, pp->transmit );
	return(1);
}

/*
 *			G L A S S _ S E T U P
 */
HIDDEN int
glass_setup( rp )
register struct region *rp;
{
	register struct phong_specific *pp;

	GETSTRUCT( pp, phong_specific );
	rp->reg_udata = (char *)pp;

	pp->shine = 4;
	pp->wgt_specular = 0.7;
	pp->wgt_diffuse = 0.3;
	pp->transmit = 0.6;
	pp->reflect = 0.3;
	/* leaving 0.1 for diffuse/specular */
	pp->refrac_index = 1.65;

	mlib_parse( rp->reg_mater.ma_matparm, phong_parse, (mp_off_ty)pp );
	if(rdebug&RDEBUG_MATERIAL)
		mlib_print(rp->reg_name, phong_parse, (mp_off_ty)pp);

	VSCALE( rp->reg_mater.ma_transmit, 
		rp->reg_mater.ma_color, pp->transmit );
	return(1);
}

/*
 *			P H O N G _ P R I N T
 */
HIDDEN int
phong_print( rp )
register struct region *rp;
{
	mlib_print(rp->reg_name, phong_parse, (mp_off_ty)rp->reg_udata);
}

/*
 *			P H O N G _ F R E E
 */
HIDDEN int
phong_free( cp )
char *cp;
{
	rt_free( cp, "phong_specific" );
}


/*
 *			P H O N G _ R E N D E R
 *
	Color pixel based on the energy of a point light source (Eps)
	plus some diffuse illumination (Epd) reflected from the point
	<x,y> :

				E = Epd + Eps		(1)

	The energy reflected from diffuse illumination is the product
	of the reflectance coefficient at point P (Rp) and the diffuse
	illumination (Id) :

				Epd = Rp * Id		(2)

	The energy reflected from the point light source is calculated
	by the sum of the diffuse reflectance (Rd) and the specular
	reflectance (Rs), multiplied by the intensity of the light
	source (Ips) :

				Eps = (Rd + Rs) * Ips	(3)

	The diffuse reflectance is calculated by the product of the
	reflectance coefficient (Rp) and the cosine of the angle of
	incidence (I) :

				Rd = Rp * cos(I)	(4)

	The specular reflectance is calculated by the product of the
	specular reflectance coeffient and (the cosine of the angle (S)
	raised to the nth power) :

				Rs = W(I) * cos(S)**n	(5)

	Where,
		I is the angle of incidence.
		S is the angle between the reflected ray and the observer.
		W returns the specular reflection coefficient as a function
	of the angle of incidence.
		n (roughly 1 to 10) represents the shininess of the surface.
 *
	This is the heart of the lighting model which is based on a model
	developed by Bui-Tuong Phong, [see Wm M. Newman and R. F. Sproull,
	"Principles of Interactive Computer Graphics", 	McGraw-Hill, 1979]

	Er = Ra(m)*cos(Ia) + Rd(m)*cos(I1) + W(I1,m)*cos(s)^^n
	where,
 
	Er	is the energy reflected in the observer's direction.
	Ra	is the diffuse reflectance coefficient at the point
		of intersection due to ambient lighting.
	Ia	is the angle of incidence associated with the ambient
		light source (angle between ray direction (negated) and
		surface normal).
	Rd	is the diffuse reflectance coefficient at the point
		of intersection due to primary lighting.
	I1	is the angle of incidence associated with the primary
		light source (angle between light source direction and
		surface normal).
	m	is the material identification code.
	W	is the specular reflectance coefficient,
		a function of the angle of incidence, range 0.0 to 1.0,
		for the material.
	s	is the angle between the reflected ray and the observer.
	n	'Shininess' of the material,  range 1 to 10.
 */
HIDDEN int
phong_render( ap, pp, swp )
register struct application *ap;
struct partition	*pp;
struct shadework	*swp;
{
	register struct light_specific *lp;
	auto struct application sub_ap;
	auto int light_visible;
	auto fastf_t	d_a;		/* ambient diffuse */
	auto fastf_t	cosAmb;
	auto fastf_t	f;
	auto vect_t	work;
	auto vect_t	reflected;
	auto vect_t	to_eye;
	auto vect_t	to_light;
	auto point_t	matcolor;		/* Material color */
	struct phong_specific *ps =
		(struct phong_specific *)pp->pt_regionp->reg_udata;

	swp->sw_transmit = ps->transmit;
	swp->sw_reflect = ps->reflect;
	swp->sw_refrac_index = ps->refrac_index;

	VREVERSE( to_eye, ap->a_ray.r_dir );

	/* Diminish intensity of reflected light as a function of
	 * the distance from your eye.
	 */
/**	dist_gradient = kCons / (swp->sw_hit.hit_dist + cCons);  */

	/* Diffuse reflectance from "Ambient" light source (at eye) */
	d_a = 0;
	if( (cosAmb = VDOT( swp->sw_hit.hit_normal, to_eye )) > 0.0 )  {
		if( cosAmb > 1.00001 )  {
			rt_log("cosAmb=%g (x%d,y%d,lvl%d)\n", cosAmb,
				ap->a_x, ap->a_y, ap->a_level);
			cosAmb = 1;
		}
		d_a = cosAmb * AmbientIntensity;
	}
	VMOVE( matcolor, swp->sw_color );
	VSCALE( swp->sw_color, matcolor, d_a );

	/* Consider effects of each light source */
	for( lp=LightHeadp; lp; lp = lp->lt_forw )  {
		FAST fastf_t f;

		if( lp->lt_explicit )  {
			/* Fire ray at light source to check for shadowing */
			/* This SHOULD actually return an energy value */
			sub_ap = *ap;		/* struct copy */
			sub_ap.a_hit = light_hit;
			sub_ap.a_miss = light_miss;
			sub_ap.a_level++;
			VMOVE( sub_ap.a_ray.r_pt, swp->sw_hit.hit_point );
			
			/* Dither light pos for penumbra by +/- 0.5 light radius */
			/* This presently makes a cubical light source distribution */
			f = lp->lt_radius * 0.9;
			sub_ap.a_ray.r_dir[X] =  lp->lt_pos[X] + rand_half()*f - swp->sw_hit.hit_point[X];
			sub_ap.a_ray.r_dir[Y] =  lp->lt_pos[Y] + rand_half()*f - swp->sw_hit.hit_point[Y];
			sub_ap.a_ray.r_dir[Z] =  lp->lt_pos[Z] + rand_half()*f - swp->sw_hit.hit_point[Z];
			VUNITIZE( sub_ap.a_ray.r_dir );
			VSETALL( sub_ap.a_color, 1 );	/* vis intens so far */
			light_visible = rt_shootray( &sub_ap );
			/* sub_ap.a_color now contains visible fraction */
		} else {
			light_visible = 1;
			VSETALL( sub_ap.a_color, 1 );
		}
	
		/* If not shadowed add this light contribution */
		if( light_visible )  {
			auto fastf_t cosI;
			auto fastf_t cosS;
			vect_t light_intensity;

			/* Diffuse reflectance from this light source. */
			VSUB2( to_light, lp->lt_pos, swp->sw_hit.hit_point );
			VUNITIZE( to_light );
			if( (cosI = VDOT( swp->sw_hit.hit_normal, to_light )) > 0.0 )  {
				fastf_t	Rd;
				vect_t	cprod;	/* color product */
				if( cosI > 1 )  {
					rt_log("cosI=%g (x%d,y%d,lvl%d)\n", cosI,
						ap->a_x, ap->a_y, ap->a_level);
					cosI = 1;
				}
				Rd = cosI * lp->lt_fraction * ps->wgt_diffuse;
				VELMUL( light_intensity, lp->lt_color,
					sub_ap.a_color );
				VELMUL( cprod, matcolor, light_intensity );
				VJOIN1( swp->sw_color, swp->sw_color,
					Rd, cprod );
			}

			/* Calculate specular reflectance.
			 *	Reflected ray = (2 * cos(i) * Normal) - Incident ray.
			 * 	Cos(s) = Reflected ray DOT Incident ray.
			 */
			cosI *= 2;
			VSCALE( work, swp->sw_hit.hit_normal, cosI );
			VSUB2( reflected, work, to_light );
			if( (cosS = VDOT( reflected, to_eye )) > 0 )  {
				fastf_t Rs;
				if( cosS > 1 )  {
					rt_log("cosS=%g (x%d,y%d,lvl%d)\n", cosS,
						ap->a_x, ap->a_y, ap->a_level);
					cosS = 1;
				}
				Rs = ps->wgt_specular * lp->lt_fraction *
					ipow(cosS, ps->shine);
				VELMUL( light_intensity, lp->lt_color,
					sub_ap.a_color );
				VJOIN1( swp->sw_color, swp->sw_color,
					Rs, light_intensity );
			}
		}
	}
	return(1);
}

/*
 *  			I P O W
 *  
 *  Raise a floating point number to an integer power
 */
double
ipow( d, cnt )
double d;
register int cnt;
{
	FAST fastf_t input, result;

	if( (input=d) < 1e-8 )  return(0.0);
	if( cnt < 0 || cnt > 200 )  {
		rt_log("ipow(%g,%d) bad\n", d, cnt);
		return(d);
	}
	result = 1;
	while( cnt-- > 0 )
		result *= input;
	return( result );
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
	register struct region *regp;
	struct application sub_ap;
	extern int light_render();
	int ret;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("light_hit:  no hit out front?\n");
		return(0);
	}
	regp = pp->pt_regionp;

	/* Check to see if we hit a light source */
	if( ((struct mfuncs *)(regp->reg_mfuncs))->mf_render == light_render )  {
		VSETALL( ap->a_color, 1 );
		return(1);		/* light_visible = 1 */
	}

	/* If we hit an entirely opaque object, this light is invisible */
	if( pp->pt_outhit->hit_dist >= INFINITY || (
	    regp->reg_mater.ma_transmit[0] +
	    regp->reg_mater.ma_transmit[1] +
	    regp->reg_mater.ma_transmit[2] <= 0 ) )  {
		VSETALL( ap->a_color, 0 );
		return(0);			/* light_visible = 0 */
	}

	/*
	 * We hit a transparant object.  Continue on.
	 */
	if( ap->a_color[0] + ap->a_color[1] + ap->a_color[2] < 0.01 )  {
	    	/* Any light energy is "fully" attenuated by here */
		VSETALL( ap->a_color, 0 );
		return(0);		/* light_visible = 0 */
	}

	/* Push on to exit point, and trace on from there.
	 * Transmission so far is passed along in sub_ap.a_color[];
	 */
	sub_ap = *ap;			/* struct copy */
	sub_ap.a_level = ap->a_level+1;
	{
		FAST fastf_t f;
		f = pp->pt_outhit->hit_dist+0.0001;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
	}
	ret = rt_shootray( &sub_ap );
	VELMUL( ap->a_color, sub_ap.a_color,
		regp->reg_mater.ma_transmit );
	return(ret);			/* light_visible = ret */
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

	if( LightHeadp )  {
		/* Explicit lights exist, somehow we missed (dither?) */
		VSETALL( ap->a_color, 0 );
		return(0);		/* light_visible = 0 */
	}
	/* No explicit light -- it's hard to hit */
	VSETALL( ap->a_color, 1 );
	return(1);			/* light_visible = 1 */
}

/* Null function */
nullf() { return(0); }
