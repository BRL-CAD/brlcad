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

extern int colorview();		/* from view.c */

extern struct light_specific *LightHeadp;

/* from view.c */
extern int lightmodel;		/* lighting model # to use */
extern double AmbientIntensity;
#define MAX_IREFLECT	9	/* Maximum internal reflection level */
#define MAX_BOUNCE	4	/* Maximum recursion level */

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
	"plastic",	0,		0,
	phong_setup,	phong_render,	phong_print,	phong_free,

	"mirror",	0,		0,
	mirror_setup,	phong_render,	phong_print,	phong_free,

	"glass",	0,		0,
	glass_setup,	phong_render,	phong_print,	phong_free,

	(char *)0,	0,		0,
	0,		0,		0,		0
};

HIDDEN int phg_rmiss();
HIDDEN int phg_rhit();
HIDDEN int phg_refract();

extern int light_hit(), light_miss();
extern int hit_nothing();
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
	pp->refrac_index = 0.0;

	mlib_parse( rp->reg_mater.ma_matparm, phong_parse, (mp_off_ty)pp );
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
	pp->refrac_index = 0.0;

	mlib_parse( rp->reg_mater.ma_matparm, phong_parse, (mp_off_ty)pp );
	if(rdebug&RDEBUG_MATERIAL)
		mlib_print(rp->reg_name, phong_parse, (mp_off_ty)pp);
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
	pp->transmit = 0.7;
	pp->reflect = 0.3;
	pp->refrac_index = 1.65;

	mlib_parse( rp->reg_mater.ma_matparm, phong_parse, (mp_off_ty)pp );
	if(rdebug&RDEBUG_MATERIAL)
		mlib_print(rp->reg_name, phong_parse, (mp_off_ty)pp);
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
phong_render( ap, pp )
register struct application *ap;
register struct partition *pp;
{
	register struct hit *hitp= pp->pt_inhit;
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


	/* Get default color-by-ident for region.			*/
	{
		register struct region *rp;
		if( (rp=pp->pt_regionp) == REGION_NULL )  {
			rt_log("bad region pointer\n");
			VSET( ap->a_color, 0.7, 0.7, 0.7 );
			goto finish;
		}
		if( rp->reg_mater.ma_override )  {
			VSET( matcolor,
				rp->reg_mater.ma_rgb[0]/255.,
				rp->reg_mater.ma_rgb[1]/255.,
				rp->reg_mater.ma_rgb[2]/255. );
		} else {
			/* Default color is white (uncolored) */
			VSETALL( matcolor, 1.0 );
		}
	}

	/* Get surface normal for hit point */
	RT_HIT_NORM( hitp, pp->pt_inseg->seg_stp, &(ap->a_ray) );

	/* Temporary check to make sure normals are OK */
	if( hitp->hit_normal[X] < -1.01 || hitp->hit_normal[X] > 1.01 ||
	    hitp->hit_normal[Y] < -1.01 || hitp->hit_normal[Y] > 1.01 ||
	    hitp->hit_normal[Z] < -1.01 || hitp->hit_normal[Z] > 1.01 )  {
	    	VPRINT("phong_render: N", hitp->hit_normal);
		VSET( ap->a_color, 9, 9, 0 );	/* Yellow */
		return(1);
	}
	if( pp->pt_inflip )  {
		VREVERSE( hitp->hit_normal, hitp->hit_normal );
		pp->pt_inflip = 0;
	}

	VREVERSE( to_eye, ap->a_ray.r_dir );

	/* Diminish intensity of reflected light the as a function of
	 * the distance from your eye.
	 */
/**	dist_gradient = kCons / (hitp->hit_dist + cCons);  */

	/* Diffuse reflectance from "Ambient" light source (at eye) */
	d_a = 0;
	if( (cosAmb = VDOT( hitp->hit_normal, to_eye )) > 0.0 )  {
		if( cosAmb > 1.00001 )  {
			rt_log("cosAmb=%g (x%d,y%d,lvl%d)\n", cosAmb,
				ap->a_x, ap->a_y, ap->a_level);
			cosAmb = 1;
		}
		d_a = cosAmb * AmbientIntensity;
	}
	VSCALE( ap->a_color, matcolor, d_a );

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
			VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
			
			/* Dither light pos for penumbra by +/- 0.5 light radius */
			f = lp->lt_radius * 0.9;
			sub_ap.a_ray.r_dir[X] =  lp->lt_pos[X] + rand_half()*f - hitp->hit_point[X];
			sub_ap.a_ray.r_dir[Y] =  lp->lt_pos[Y] + rand_half()*f - hitp->hit_point[Y];
			sub_ap.a_ray.r_dir[Z] =  lp->lt_pos[Z] + rand_half()*f - hitp->hit_point[Z];
			VUNITIZE( sub_ap.a_ray.r_dir );
			light_visible = rt_shootray( &sub_ap );
		} else {
			light_visible = 1;
		}
	
		/* If not shadowed add this light contribution */
		if( light_visible )  {
			auto fastf_t cosI;
			auto fastf_t cosS;

			/* Diffuse reflectance from this light source. */
			VSUB2( to_light, lp->lt_pos, hitp->hit_point );
			VUNITIZE( to_light );
			if( (cosI = VDOT( hitp->hit_normal, to_light )) > 0.0 )  {
				fastf_t	Rd;
				vect_t	cprod;	/* color product */
				if( cosI > 1 )  {
					rt_log("cosI=%g (x%d,y%d,lvl%d)\n", cosI,
						ap->a_x, ap->a_y, ap->a_level);
					cosI = 1;
				}
				Rd = cosI * lp->lt_fraction * ps->wgt_diffuse;
				VELMUL( cprod, matcolor, lp->lt_color );
				VJOIN1( ap->a_color, ap->a_color, Rd, cprod );
			}

			/* Calculate specular reflectance.
			 *	Reflected ray = (2 * cos(i) * Normal) - Incident ray.
			 * 	Cos(s) = Reflected ray DOT Incident ray.
			 */
			cosI *= 2;
			VSCALE( work, hitp->hit_normal, cosI );
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
				VJOIN1( ap->a_color, ap->a_color, Rs, lp->lt_color );
			}
		}
	}

	if( (ps->reflect <= 0 && ps->transmit <= 0) ||
	    ap->a_level > MAX_BOUNCE )  {
		/* Nothing more to do for this ray */
		goto finish;
	}

	/*
	 *  Diminish base color appropriately, and add in
	 *  contributions from mirror reflection & transparency
	 */
	f = 1 - (ps->reflect + ps->transmit);
	if( f > 0 )  {
		VSCALE( ap->a_color, ap->a_color, f );
	}
	if( ps->reflect > 0 )  {
		/* Mirror reflection */
		sub_ap = *ap;		/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		sub_ap.a_onehit = 1;
		VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
		f = 2 * VDOT( to_eye, hitp->hit_normal );
		VSCALE( work, hitp->hit_normal, f );
		/* I have been told this has unit length */
		VSUB2( sub_ap.a_ray.r_dir, work, to_eye );
		(void)rt_shootray( &sub_ap );
		VJOIN1(ap->a_color, ap->a_color, ps->reflect, sub_ap.a_color);
	}
	if( ps->transmit > 0 )  {
		LOCAL vect_t incident_dir;

		/* Calculate refraction at entrance. */
		sub_ap = *ap;		/* struct copy */
		sub_ap.a_level = (ap->a_level+1) * 100;	/* flag */
		sub_ap.a_onehit = 1;
		sub_ap.a_user = (int)(pp->pt_regionp);
		VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
		VMOVE( incident_dir, ap->a_ray.r_dir );
		if( !phg_refract(incident_dir, /* Incident ray (IN) */
			hitp->hit_normal,
			RI_AIR, ps->refrac_index,
			sub_ap.a_ray.r_dir	/* Refracted ray (OUT) */
		) )  {
			/* Reflected back outside solid */
			goto do_exit;
		}
		/* Find new exit point from the inside. */
do_inside:
		sub_ap.a_hit =  phg_rhit;
		sub_ap.a_miss = phg_rmiss;
		if( rt_shootray( &sub_ap ) == 0 )  {
			if(rdebug&RDEBUG_HITS)rt_log("phong: Refracted ray missed '%s' -- RETRYING, lvl=%d\n",
				pp->pt_inseg->seg_stp->st_name,
				sub_ap.a_level );
			/* Back off just a little bit, and try again */
			/* Useful when striking exactly in corners */
			VJOIN1( sub_ap.a_ray.r_pt, sub_ap.a_ray.r_pt,
				-3.0, incident_dir );
			if( rt_shootray( &sub_ap ) == 0 )  {
				rt_log("phong: Refracted ray missed 2x '%s', lvl=%d\n",
					pp->pt_inseg->seg_stp->st_name,
					sub_ap.a_level );
				VPRINT("pt", sub_ap.a_ray.r_pt );
				VPRINT("dir", sub_ap.a_ray.r_dir );
				VSET( ap->a_color, 0, 99, 0 );	/* green */
				goto finish;		/* abandon hope */
			}
		}
		/* NOTE: phg_rhit returns EXIT Point in sub_ap.a_uvec,
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
		if( !phg_refract( incident_dir,		/* input direction */
			sub_ap.a_vvec,			/* exit normal */
			ps->refrac_index, RI_AIR,
			sub_ap.a_ray.r_dir		/* output direction */
		) )  {
			/* Reflected internally -- keep going */
			if( (++sub_ap.a_level)%100 > MAX_IREFLECT )  {
				rt_log("phong: %s Excessive internal reflection (x%d, y%d, lvl=%d)\n",
					pp->pt_inseg->seg_stp->st_name,
					sub_ap.a_x, sub_ap.a_y, sub_ap.a_level );
				if(rdebug&RDEBUG_SHOWERR) {
					VSET( ap->a_color, 0, 9, 0 );	/* green */
				} else {
					VSETALL( ap->a_color, .18 ); /* 18% grey */
				}
				goto finish;
			}
			goto do_inside;
		}
do_exit:
		sub_ap.a_hit =  colorview;
		sub_ap.a_miss = hit_nothing;
		sub_ap.a_level++;
		(void) rt_shootray( &sub_ap );
		VJOIN1( ap->a_color, ap->a_color,
			ps->transmit, sub_ap.a_color );
	}
finish:
	return(1);
}

/*
 *			R F R _ M I S S
 */
HIDDEN int
/*ARGSUSED*/
phg_rmiss( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	return(0);
}

/*
 *			R F R _ H I T
 *
 *  Implicit Returns -
 *	a_uvec	exit Point
 *	a_vvec	exit Normal
 */
HIDDEN int
phg_rhit( ap, PartHeadp )
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
			rt_log("phg_rhit:  no hit out front?\n");
		goto bad;
	}
	if( pp->pt_regionp != (struct region *)(ap->a_user) )  {
		if(rdebug&RDEBUG_HITS)
			rt_log("phg_rhit:  Ray reflected within %s now in %s!\n",
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
			rt_log("phg_rhit:  '%s' inhit %g not near zero!\n",
				stp->st_name, hitp->hit_dist);
			rt_pr_hit("inhit", hitp);
			rt_pr_hit("outhit", pp->pt_outhit);
		}
		goto bad;
	}

	hitp = pp->pt_outhit;
	stp = pp->pt_outseg->seg_stp;
	RT_HIT_NORM( hitp, stp, &(ap->a_ray) );
	if( hitp->hit_dist >= INFINITY )  {
		if(rdebug&RDEBUG_SHOWERR)
			rt_log("phg_rhit:  (%g,%g) bad!\n",
				pp->pt_inhit->hit_dist, hitp->hit_dist);
		goto bad;
	}

	VMOVE( ap->a_uvec, hitp->hit_point );
	/* Safety check */
	if( (rdebug&RDEBUG_SHOWERR) && (
	    !NEAR_ZERO(hitp->hit_normal[X], 1.001) ||
	    !NEAR_ZERO(hitp->hit_normal[Y], 1.001) ||
	    !NEAR_ZERO(hitp->hit_normal[Z], 1.001) ) )  {
	    	rt_log("phg_rhit: defective normal hitting %s\n", stp->st_name);
	    	VPRINT("hit_normal", hitp->hit_normal);
	    	goto bad;
	}
	/* For refraction, want exit normal to point inward. */
	VREVERSE( ap->a_vvec, hitp->hit_normal );
	return(1);

	/* Give serious information when problems are encountered */
bad:
	if(rdebug&RDEBUG_HITS) rt_pr_partitions( PartHeadp, "phg_rhit" );
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
phg_refract( v_1, norml, ri_1, ri_2, v_2 )
register vect_t	v_1;
register vect_t	norml;
double	ri_1, ri_2;
register vect_t	v_2;
{
	LOCAL vect_t	w, u;
	FAST fastf_t	beta;

	if( NEAR_ZERO(ri_1, 0.0001) || NEAR_ZERO( ri_2, 0.0001 ) )  {
		rt_log("phg_refract:ri1=%g, ri2=%g\n", ri_1, ri_2 );
		beta = 1;
	} else {
		beta = ri_1/ri_2;		/* temp */
		if( beta > 10000 )  {
			rt_log("phg_refract:  beta=%g\n", beta);
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
 *  These shadow functions return a boolean "light_visible".
 * 
 *  This is an incredibly simplistic algorithm, in need of improvement.
 *  If glass is hit, we need to keep going.
 *  Reflected light can't be dealt with at all.
 *  Would also be nice to return an energy level, rather than
 *  a boolean, which could account for lots of interesting effects.
 */
light_hit(ap, PartHeadp)
struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	extern int light_render();

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("light_hit:  no hit out front?\n");
		return(0);
	}

	/* Check to see if we hit a light source */
	if( pp->pt_regionp->reg_mfuncs->mf_render == light_render )
		return(1);		/* light_visible = 1 */
	return(0);			/* light_visible = 0 */
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
	return(1);			/* light_visible = 1 */
}

/* Null function */
nullf() { return(0); }

/*
 *			H I T _ N O T H I N G
 *
 *  a_miss() routine called when no part of the model is hit.
 *  Background texture mapping could be done here.
 *  For now, return a pleasant dark blue.
 */
hit_nothing( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	if( lightmodel == 2 )  {
		VSET( ap->a_color, 0, 0, 0 );
	}  else  {
		VSET( ap->a_color, .25, 0, .5 );	/* Background */
	}
	return(0);
}
