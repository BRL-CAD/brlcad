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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./rdebug.h"
#include "./material.h"
#include "./light.h"

/* Fast approximation to specular term */
#define PHAST_PHONG 1	/* See Graphics Gems IV pg 387 */

/* from view.c */
extern double AmbientIntensity;

/* Local information */
struct phong_specific {
	int	magic;
	int	shine;
	double	wgt_specular;
	double	wgt_diffuse;
	double	transmit;	/* Moss "transparency" */
	double	reflect;	/* Moss "transmission" */
	double	refrac_index;
	double	extinction;
	struct mfuncs *mfp;
};
#define PL_MAGIC	0xbeef00d
#define PL_NULL	((struct phong_specific *)0)
#define PL_O(m)	offsetof(struct phong_specific, m)

struct bu_structparse phong_parse[] = {
	{"%d",	1, "shine",		PL_O(shine),		FUNC_NULL },
	{"%d",	1, "sh",		PL_O(shine),		FUNC_NULL },
	{"%f",	1, "specular",		PL_O(wgt_specular),	FUNC_NULL },
	{"%f",	1, "sp",		PL_O(wgt_specular),	FUNC_NULL },
	{"%f",	1, "diffuse",		PL_O(wgt_diffuse),	FUNC_NULL },
	{"%f",	1, "di",		PL_O(wgt_diffuse),	FUNC_NULL },
	{"%f",	1, "transmit",		PL_O(transmit),		FUNC_NULL },
	{"%f",	1, "tr",		PL_O(transmit),		FUNC_NULL },
	{"%f",	1, "reflect",		PL_O(reflect),		FUNC_NULL },
	{"%f",	1, "re",		PL_O(reflect),		FUNC_NULL },
	{"%f",	1, "ri",		PL_O(refrac_index),	FUNC_NULL },
	{"%f",	1, "extinction_per_meter", PL_O(extinction),	FUNC_NULL },
	{"%f",	1, "extinction",	PL_O(extinction),	FUNC_NULL },
	{"%f",	1, "ex",		PL_O(extinction),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int phong_setup(), mirror_setup(), glass_setup();
HIDDEN int phong_render();
HIDDEN void	phong_print();
HIDDEN void	phong_free();

struct mfuncs phg_mfuncs[] = {
	{"default",	MF_MAGIC,	0,		MFI_NORMAL,	0,
	phong_setup,	phong_render,	phong_print,	phong_free },

	{"plastic",	MF_MAGIC,	0,		MFI_NORMAL,	0,
	phong_setup,	phong_render,	phong_print,	phong_free },

	{"mirror",	MF_MAGIC,	0,		MFI_NORMAL,	0,
	mirror_setup,	phong_render,	phong_print,	phong_free },

	{"glass",	MF_MAGIC,	0,		MFI_NORMAL,	0,
	glass_setup,	phong_render,	phong_print,	phong_free },

	{(char *)0,	0,		0,		0,	0,
	0,		0,		0,		0 }
};

#ifndef PHAST_PHONG
extern double phg_ipow();
#endif

#define RI_AIR		1.0    /* Refractive index of air.		*/

/*
 *			P H O N G _ S E T U P
 */
HIDDEN int
phong_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
struct mfuncs           *mfp;
struct rt_i             *rtip;  /* New since 4.4 release */
{
	register struct phong_specific *pp;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( pp, phong_specific );
	*dpp = (char *)pp;

	pp->magic = PL_MAGIC;
	pp->shine = 10;
	pp->wgt_specular = 0.7;
	pp->wgt_diffuse = 0.3;
	pp->transmit = 0.0;
	pp->reflect = 0.0;
	pp->refrac_index = RI_AIR;
	pp->extinction = 0.0;
	pp->mfp = mfp;

	if( bu_struct_parse( matparm, phong_parse, (char *)pp ) < 0 )  {
		rt_free( (char *)pp, "phong_specific" );
		return(-1);
	}

	if( pp->transmit > 0 )
		rp->reg_transmit = 1;
	return(1);
}

/*
 *			M I R R O R _ S E T U P
 */
HIDDEN int
mirror_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
struct mfuncs           *mfp;
struct rt_i             *rtip;  /* New since 4.4 release */
{
	register struct phong_specific *pp;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( pp, phong_specific );
	*dpp = (char *)pp;

	pp->magic = PL_MAGIC;
	pp->shine = 4;
	pp->wgt_specular = 0.6;
	pp->wgt_diffuse = 0.4;
	pp->transmit = 0.0;
	pp->reflect = 0.75;
	pp->refrac_index = 1.65;
	pp->extinction = 0.0;
	pp->mfp = mfp;

	if( bu_struct_parse( matparm, phong_parse, (char *)pp ) < 0 )  {
		rt_free( (char *)pp, "phong_specific" );
		return(-1);
	}

	if( pp->transmit > 0 )
		rp->reg_transmit = 1;
	return(1);
}

/*
 *			G L A S S _ S E T U P
 */
HIDDEN int
glass_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
struct mfuncs           *mfp;
struct rt_i             *rtip;  /* New since 4.4 release */
{
	register struct phong_specific *pp;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( pp, phong_specific );
	*dpp = (char *)pp;

	pp->magic = PL_MAGIC;
	pp->shine = 4;
	pp->wgt_specular = 0.7;
	pp->wgt_diffuse = 0.3;
	pp->transmit = 0.8;
	pp->reflect = 0.1;
	/* leaving 0.1 for diffuse/specular */
	pp->refrac_index = 1.65;
	pp->extinction = 0.0;
	pp->mfp = mfp;

	if( bu_struct_parse( matparm, phong_parse, (char *)pp ) < 0 )  {
		rt_free( (char *)pp, "phong_specific" );
		return(-1);
	}

	if( pp->transmit > 0 )
		rp->reg_transmit = 1;
	return(1);
}

/*
 *			P H O N G _ P R I N T
 */
HIDDEN void
phong_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print(rp->reg_name, phong_parse, (char *)dp);
}

/*
 *			P H O N G _ F R E E
 */
HIDDEN void
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
phong_render( ap, pp, swp, dp )
register struct application *ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct light_specific *lp;
	register fastf_t *intensity, *to_light;
	register int	i;
	register fastf_t cosine;
	register fastf_t refl;
	vect_t	work;
	vect_t	reflected;
	vect_t	cprod;			/* color product */
	point_t	matcolor;		/* Material color */
	struct phong_specific *ps =
		(struct phong_specific *)dp;

	if( ps->magic != PL_MAGIC )  rt_log("phong_render: bad magic\n");

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "phong_render", phong_parse, (char *)ps );

	swp->sw_transmit = ps->transmit;
	swp->sw_reflect = ps->reflect;
	swp->sw_refrac_index = ps->refrac_index;
	swp->sw_extinction = ps->extinction;
	if( swp->sw_xmitonly ) {
		if( swp->sw_reflect > 0 || swp->sw_transmit > 0 ) {
			if( rdebug&RDEBUG_SHADE)
				bu_log("calling rr_render from phong, sw_xmitonly\n");
			(void)rr_render( ap, pp, swp );
		}
		return(1);	/* done */
	}

	VMOVE( matcolor, swp->sw_color );

	/* Diffuse reflectance from "Ambient" light source (at eye) */
	if( (cosine = -VDOT( swp->sw_hit.hit_normal, ap->a_ray.r_dir )) > 0.0 )  {
		if( cosine > 1.00001 )  {
			rt_log("cosAmb=1+%g (x%d,y%d,lvl%d)\n", cosine-1,
				ap->a_x, ap->a_y, ap->a_level);
			cosine = 1;
		}
		cosine *= AmbientIntensity;
		VSCALE( swp->sw_color, matcolor, cosine );
	} else {
		VSETALL( swp->sw_color, 0 );
	}

	/* With the advent of procedural shaders, the caller can no longer
	 * provide us reliable light visibility information.  The hit point
	 * may have been changed by another shader in a stack.  There is no
	 * way that anyone else can tell us whether lights are visible.
	 */
	light_visibility(ap, swp, ps->mfp->mf_inputs);

	/* Consider effects of each light source */
	for( i=ap->a_rt_i->rti_nlights-1; i>=0; i-- )  {

		if( (lp = (struct light_specific *)swp->sw_visible[i]) == LIGHT_NULL )
			continue;
	
		/* Light is not shadowed -- add this contribution */
		intensity = swp->sw_intensity+3*i;
		to_light = swp->sw_tolight+3*i;

		/* Diffuse reflectance from this light source. */
		if( (cosine = VDOT( swp->sw_hit.hit_normal, to_light )) > 0.0 )  {
			if( cosine > 1.00001 )  {
				rt_log("cosI=1+%g (x%d,y%d,lvl%d)\n", cosine-1,
					ap->a_x, ap->a_y, ap->a_level);
				cosine = 1;
			}
			refl = cosine * lp->lt_fraction * ps->wgt_diffuse;
			VELMUL( work, lp->lt_color,
				intensity );
			VELMUL( cprod, matcolor, work );
			VJOIN1( swp->sw_color, swp->sw_color,
				refl, cprod );
		}

		/* Calculate specular reflectance.
		 *	Reflected ray = (2 * cos(i) * Normal) - Incident ray.
		 * 	Cos(s) = Reflected ray DOT Incident ray.
		 */
		cosine *= 2;
		VSCALE( work, swp->sw_hit.hit_normal, cosine );
		VSUB2( reflected, work, to_light );
		if( (cosine = -VDOT( reflected, ap->a_ray.r_dir )) > 0 )  {
			if( cosine > 1.00001 )  {
				rt_log("cosS=1+%g (x%d,y%d,lvl%d)\n", cosine-1,
					ap->a_x, ap->a_y, ap->a_level);
				cosine = 1;
			}
			refl = ps->wgt_specular * lp->lt_fraction *
#ifdef PHAST_PHONG
				/* It is unnecessary to compute the actual
				 * exponential here since phong is just a
				 * gross hack.  We approximate re:
				 *  Graphics Gems IV "A Fast Alternative to
				 *  Phong's Specular Model" Pg 385
				 */
				cosine /
				(ps->shine - ps->shine*cosine + cosine);
#else
				phg_ipow(cosine, ps->shine);
#endif
			VELMUL( work, lp->lt_color,
				intensity );
			VJOIN1( swp->sw_color, swp->sw_color,
				refl, work );
		}
	}
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );
	return(1);
}

#ifndef PHAST_PHONG
/*
 *  			I P O W
 *  
 *  Raise a floating point number to an integer power
 */
double
phg_ipow( d, cnt )
double d;
register int cnt;
{
	FAST fastf_t input, result;

	if( (input=d) < 1e-8 )  return(0.0);
	if( cnt < 0 || cnt > 200 )  {
		rt_log("phg_ipow(%g,%d) bad\n", d, cnt);
		return(d);
	}
	result = 1;
	while( cnt-- > 0 )
		result *= input;
	return( result );
}
#endif
