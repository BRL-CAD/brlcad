/*
 *			V I E W
 *
 * Ray Tracing program, sample lighting models.  Part of the
 * RT program proper.
 *
 *  Many varied and wonderous "lighting models" are implemented.
 *  The output format is a .PIX file (a byte stream of R,G,B as u_char's).
 *
 *  The extern "lightmodel" selects which one is being used:
 *	0	model with color, based on Moss's LGT
 *	1	1-light, from the eye.
 *	2	Spencer's surface-normals-as-colors display
 *	3	3-light debugging model
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/mater.h"
#include "../h/raytrace.h"
#include "../librt/debug.h"

#include "mat_db.h"

char usage[] = "\
Usage:  rt [options] model.g objects...\n\
Options:\n\
 -f#		Grid size in pixels, default 512, max 1024\n\
 -aAz		Azimuth in degrees\n\
 -eElev		Elevation in degrees\n\
 -M		Read model2view matrix on stdin\n\
 -o model.pix	Specify output file, .pix format (default=fb)\n\
 -x#		Set debug flags\n\
 -p[#]		Perspective viewing, focal length scaling\n\
 -l#		Select lighting model\n\
 	0	Two lights, one at eye (default)\n\
	1	One light, from eye (diffuse)\n\
	2	Surface-normals as colors\n\
	3	Three light debugging model (diffuse)\n\
";

extern int ikfd;		/* defined in iklib.o */
extern int ikhires;		/* defined in iklib.o */

extern int lightmodel;		/* lighting model # to use */
extern mat_t view2model;
extern mat_t model2view;

#define MAX_LINE	1024		/* Max pixels/line */
/* Current arrangement is definitely non-parallel! */
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer, R,G,B */
static char *pixelp;			/* pointer to first empty pixel */
static int scanbytes;			/* # bytes in scanline to write */
static int pixfd;			/* fd of .pix file */

struct soltab *l0stp = SOLTAB_NULL;	/* ptr to light solid tab entry */
vect_t l0color = {  1,  1,  1 };		/* White */
vect_t l1color = {  1, .1, .1 };
vect_t l2color = { .1, .1,  1 };		/* R, G, B */
vect_t ambient_color = { 1, 1, 1 };	/* Ambient white light */
vect_t l0vec;			/* 0th light vector */
vect_t l1vec;			/* 1st light vector */
vect_t l2vec;			/* 2st light vector */
vect_t l0pos;			/* pos of light0 (overrides l0vec) */
extern double AmbientIntensity;

#define MAX_IREFLECT	9	/* Maximum internal reflection level */
#define MAX_BOUNCE	4	/* Maximum recursion level */

HIDDEN int	rfr_hit(), rfr_miss();
HIDDEN int	refract();

#define RI_AIR		1.0    /* Refractive index of air.		*/
#define RI_GLASS	1.3    /* Refractive index of glass.		*/

#ifdef BENCHMARK
#define rand0to1()	(0.5)
#define rand_half()	(0)
#else BENCHMARK
/*
 *  			R A N D 0 T O 1
 *
 *  Returns a random number in the range 0 to 1
 */
double rand0to1()
{
	FAST fastf_t f;
	/* On BSD, might want to use random() */
	/* / (double)0x7FFFFFFFL */
	f = ((double)rand()) *
		0.00000000046566128752457969241057508271679984532147;
	if( f > 1.0 || f < 0 )  {
		rtlog("rand0to1 out of range\n");
		return(0.42);
	}
	return(f);
}

/*
 *  			R A N D _ H A L F
 *
 *  Returns a random number in the range -0.5 to +0.5
 */
#define rand_half()	(rand0to1()-0.5)

#endif BENCHMARK

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
	FAST fastf_t result;

	if( d < 1e-8 )  return(0.0);
	result = 1;
	while( cnt-- > 0 )
		result *= d;
	return( result );
}

/* These shadow functions return a boolean "light_visible" */
light_hit(ap, PartHeadp)
struct application *ap;
struct partition *PartHeadp;
{
	/* Check to see if we hit the light source */
	if( PartHeadp->pt_forw->pt_inseg->seg_stp == l0stp )
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
nullf() { ; }

/*
 *			H I T _ N O T H I N G
 *
 *  a_miss() routine called when no part of the model is hit.
 *  Background texture mapping could be done here.
 *  For now, return a pleasant dark blue.
 */
hit_nothing( ap )
register struct application *ap;
{
	if( lightmodel == 2 )  {
		VSET( ap->a_color, 0, 0, 0 );
	}  else  {
		VSET( ap->a_color, .25, 0, .5 );	/* Background */
	}
	return(0);
}

/*
 *			V I E W I T
 *
 *  a_hit() routine for simple lighting model.
 */
viewit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	register struct hit *hitp= pp->pt_inhit;
	LOCAL fastf_t diffuse2, cosI2;
	LOCAL fastf_t diffuse1, cosI1;
	LOCAL fastf_t diffuse0, cosI0;
	LOCAL vect_t work0, work1;

	/*
	 * Diffuse reflectance from each light source
	 */
	if( pp->pt_inflip )  {
		VREVERSE( hitp->hit_normal, hitp->hit_normal );
	}
	if( lightmodel == 1 )  {
		/* Light from the "eye" (ray source).  Note sign change */
		diffuse0 = 0;
		if( (cosI0 = -VDOT(hitp->hit_normal, ap->a_ray.r_dir)) >= 0.0 )
			diffuse0 = cosI0 * ( 1.0 - AmbientIntensity);
		VSCALE( work0, l0color, diffuse0 );

		/* Add in contribution from ambient light */
		VSCALE( work1, ambient_color, AmbientIntensity );
		VADD2( ap->a_color, work0, work1 );
	}  else if( lightmodel == 3 )  {
		/* Simple attempt at a 3-light model. */
		diffuse0 = 0;
		if( (cosI0 = VDOT(hitp->hit_normal, l0vec)) >= 0.0 )
			diffuse0 = cosI0 * 0.5;		/* % from this src */
		diffuse1 = 0;
		if( (cosI1 = VDOT(hitp->hit_normal, l1vec)) >= 0.0 )
			diffuse1 = cosI1 * 0.5;		/* % from this src */
		diffuse2 = 0;
		if( (cosI2 = VDOT(hitp->hit_normal, l2vec)) >= 0.0 )
			diffuse2 = cosI2 * 0.2;		/* % from this src */

#ifdef notyet
		/* Specular reflectance from first light source */
		/* reflection = (2 * cos(i) * NormalVec) - IncidentVec */
		/* cos(s) = -VDOT(reflection, r_dir) = cosI0 */
		f = 2 * cosI1;
		VSCALE( work, hitp->hit_normal, f );
		VSUB2( reflection, work, l1vec );
		if( not_shadowed && cosI0 > cosAcceptAngle )
			/* Do specular return */;
#endif notyet

		VSCALE( work0, l0color, diffuse0 );
		VSCALE( work1, l1color, diffuse1 );
		VADD2( work0, work0, work1 );
		VSCALE( work1, l2color, diffuse2 );
		VADD2( work0, work0, work1 );

		/* Add in contribution from ambient light */
		VSCALE( work1, ambient_color, AmbientIntensity );
		VADD2( ap->a_color, work0, work1 );
	} else {
		/* lightmodel == 2 */
		/* Store surface normals pointing inwards */
		/* (For Spencer's moving light program */
		ap->a_color[0] = (hitp->hit_normal[0] * (-.5)) + .5;
		ap->a_color[1] = (hitp->hit_normal[1] * (-.5)) + .5;
		ap->a_color[2] = (hitp->hit_normal[2] * (-.5)) + .5;
	}

	if(debug&DEBUG_HITS)  {
		pr_hit( " In", hitp );
		rtlog("cosI0=%f, diffuse0=%f   ", cosI0, diffuse0 );
		VPRINT("RGB", ap->a_color);
	}
}

/*
 *  			V I E W _ P I X E L
 *  
 *  Arrange to have the pixel output.
 */
view_pixel(ap)
register struct application *ap;
{
	register int r,g,b;

	r = ap->a_color[0]*255.;
	g = ap->a_color[1]*255.;
	b = ap->a_color[2]*255.;
	if( r > 255 ) r = 255;
	if( g > 255 ) g = 255;
	if( b > 255 ) b = 255;
	if( r<0 || g<0 || b<0 )  {
		VPRINT("@@ Negative RGB @@", ap->a_color);
		r = 0x80;
		g = 0xFF;
		b = 0x80;
	}

#ifdef vax
	if( ikfd > 0 )
		ikwpixel( ap->a_x, ap->a_y, (b<<16)|(g<<8)|(r) );
#endif
	if( pixfd > 0 )  {
		*pixelp++ = r & 0xFF;
		*pixelp++ = g & 0xFF;
		*pixelp++ = b & 0xFF;
	}
	if(debug&DEBUG_HITS) rtlog("rgb=%3d,%3d,%3d\n", r,g,b);
}


/*
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
colorview( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	register struct hit *hitp= pp->pt_inhit;
	auto struct application sub_ap;
	auto int light_visible;
	auto fastf_t	Rd1;
	auto fastf_t	d_a;		/* ambient diffuse */
	auto double	cosI1, cosI2;
	auto fastf_t	f;
	auto vect_t	work;
	auto vect_t	reflected;
	auto vect_t	to_eye;
	auto vect_t	to_light;
	auto point_t	mcolor;		/* Material color */
	Mat_Db_Entry	*entry = &mat_dfl_entry;

	if(debug&DEBUG_HITS)  {
		pr_pt(pp);
		pr_hit( "view", pp->pt_inhit);
	}

	/* Temporary check to make sure normals are OK */
	if( hitp->hit_normal[X] < -1.01 || hitp->hit_normal[X] > 1.01 ||
	    hitp->hit_normal[Y] < -1.01 || hitp->hit_normal[Y] > 1.01 ||
	    hitp->hit_normal[Z] < -1.01 || hitp->hit_normal[Z] > 1.01 )  {
		rtlog("colorview: N=(%f,%f,%f)?\n",
			hitp->hit_normal[X],hitp->hit_normal[Y],hitp->hit_normal[Z]);
		VSET( ap->a_color, 1, 1, 0 );
		goto finish;
	}

	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < 0.0 )  {
		if( debug || ap->a_level > MAX_BOUNCE )  {
			VSET( ap->a_color, 1, 0, 0 );
			rtlog("colorview:  eye inside %s (x=%d, y=%d, lvl=%d)\n",
				pp->pt_inseg->seg_stp->st_name,
				ap->a_x, ap->a_y, ap->a_level);
			goto finish;
		}
		/* Push on to exit point, and trace on from there */
		sub_ap = *ap;	/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		f = pp->pt_outhit->hit_dist+0.0001;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
		(void)shootray( &sub_ap );
		VSCALE( ap->a_color, sub_ap.a_color, 0.8 );
		goto finish;
	}

	if( debug&DEBUG_RAYWRITE )  {
		/* Record the approach path */
		if( hitp->hit_dist > EPSILON )
			wraypts( ap->a_ray.r_pt,
				hitp->hit_point,
				ap, stdout );
	}

	/* Check to see if we hit something special */
	{
		register struct soltab *stp;
		register char *cp;
		cp = (stp = pp->pt_inseg->seg_stp)->st_name;
		if( stp == l0stp )  {
			VMOVE( ap->a_color, l0color );
			goto finish;
		}
		/* Clouds "Texture" map */
		if( *cp == 'C' && strncmp( cp, "CLOUD", 5 )==0 )  {
			auto fastf_t uv[2];
			double inten;
			extern double texture();
			functab[pp->pt_inseg->seg_stp->st_id].ft_uv(
				pp->pt_inseg->seg_stp, hitp, uv );
			inten = texture( uv[0], uv[1], 1.0, 2.0, 1.0 );
			skycolor( inten, ap->a_color, 0.35, 0.3 );
			goto finish;
		}
		/* "Texture" map from file */
		if( *cp == 'T' && strncmp( cp, "TEXT", 4 )==0 )  {
			auto fastf_t uv[2];
			register unsigned char *cp;
			extern unsigned char *text_uvget();
			functab[pp->pt_inseg->seg_stp->st_id].ft_uv(
				pp->pt_inseg->seg_stp, hitp, uv );
			cp = text_uvget( 0, uv );	/* tp hack */
			VSET( mcolor, *cp++/255., *cp++/255., *cp++/255.);
			goto colorit;
		}
		/* Debug map */
		if( *cp == 'M' && strncmp( cp, "MAP", 3 )==0 )  {
			auto fastf_t uv[2];
			functab[pp->pt_inseg->seg_stp->st_id].ft_uv(
				pp->pt_inseg->seg_stp, hitp, uv );
			if(debug&DEBUG_HITS) rtlog("uv=%f,%f\n", uv[0], uv[1]);
			VSET( ap->a_color, uv[0], 0, uv[1] );
			goto finish;
		}
	}

	/* Try to look up material in Moss's database */
	if( (entry = mat_Get_Db_Entry(pp->pt_regionp->reg_material))==MAT_DB_NULL
	    || !(entry->mode_flag&MF_USED) )
		entry = &mat_dfl_entry;
	else  {
		VSET( mcolor, entry->df_rgb[0]/255., entry->df_rgb[1]/255., entry->df_rgb[2]/255.);
		goto colorit;
	}

	/* Get default color-by-ident for region.			*/
	{
		register struct mater *mp;
		if( pp->pt_regionp == REGION_NULL )  {
			rtlog("bad region pointer\n");
			VSET( ap->a_color, 0.7, 0.7, 0.7 );
			goto finish;
		}
		mp = (struct mater *)pp->pt_regionp->reg_materp;
		VSET( mcolor, mp->mt_r/255., mp->mt_g/255., mp->mt_b/255.);
	}
colorit:

	if( pp->pt_inflip )  {
		VREVERSE( hitp->hit_normal, hitp->hit_normal );
	}
	VREVERSE( to_eye, ap->a_ray.r_dir );

	/* Diminish intensity of reflected light the as a function of
	 * the distance from your eye.
	 */
/**	dist_gradient = kCons / (hitp->hit_dist + cCons);  */

	/* Diffuse reflectance from primary light source. */
	VSUB2( to_light, l0pos, hitp->hit_point );
	VUNITIZE( to_light );
	Rd1 = 0;
	if( (cosI1 = VDOT( hitp->hit_normal, to_light )) > 0.0 )  {
		if( cosI1 > 1 )  {
			rtlog("cosI1=%f (x%d,y%d,lvl%d)\n", cosI1,
				ap->a_x, ap->a_y, ap->a_level);
			cosI1 = 1;
		}
		Rd1 = cosI1 * (1 - AmbientIntensity);
	}

	/* Diffuse reflectance from secondary light source (at eye) */
	d_a = 0;
	if( (cosI2 = VDOT( hitp->hit_normal, to_eye )) > 0.0 )  {
		if( cosI2 > 1 )  {
			rtlog("cosI2=%f (x%d,y%d,lvl%d)\n", cosI2,
				ap->a_x, ap->a_y, ap->a_level);
			cosI2 = 1;
		}
		d_a = cosI2 * AmbientIntensity;
	}

	/* Apply secondary (ambient) (white) lighting. */
	VSCALE( ap->a_color, mcolor, d_a );
	if( l0stp )  {
		/* An actual light solid exists */
		FAST fastf_t f;

		/* Fire ray at light source to check for shadowing */
		/* This SHOULD actually return an energy value */
		sub_ap.a_hit = light_hit;
		sub_ap.a_miss = light_miss;
		sub_ap.a_onehit = 1;
		sub_ap.a_level = ap->a_level + 1;
		sub_ap.a_x = ap->a_x;
		sub_ap.a_y = ap->a_y;
		VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );

		/* Dither light pos for penumbra by +/- 0.5 light radius */
		f = l0stp->st_aradius * 0.9;
		sub_ap.a_ray.r_dir[X] =  l0pos[X] + rand_half()*f - hitp->hit_point[X];
		sub_ap.a_ray.r_dir[Y] =  l0pos[Y] + rand_half()*f - hitp->hit_point[Y];
		sub_ap.a_ray.r_dir[Z] =  l0pos[Z] + rand_half()*f - hitp->hit_point[Z];
		VUNITIZE( sub_ap.a_ray.r_dir );
		light_visible = shootray( &sub_ap );
	} else {
		light_visible = 1;
	}
	
	/* If not shadowed add primary lighting. */
	if( light_visible )  {
		auto fastf_t specular;
		auto fastf_t cosS;

		/* Diffuse */
		VJOIN1( ap->a_color, ap->a_color, Rd1, mcolor );

		/* Calculate specular reflectance.
		 *	Reflected ray = (2 * cos(i) * Normal) - Incident ray.
		 * 	Cos(s) = Reflected ray DOT Incident ray.
		 */
		cosI1 *= 2;
		VSCALE( work, hitp->hit_normal, cosI1 );
		VSUB2( reflected, work, to_light );
		if( (cosS = VDOT( reflected, to_eye )) > 0 )  {
			if( cosS > 1 )  {
				rtlog("cosS=%f (x%d,y%d,lvl%d)\n", cosS,
					ap->a_x, ap->a_y, ap->a_level);
				cosS = 1;
			}
			specular = entry->wgt_specular *
				ipow(cosS,(int)entry->shine);
			VJOIN1( ap->a_color, ap->a_color, specular, l0color );
		}
	}

	if( (entry->transmission <= 0 && entry->transparency <= 0) ||
	    ap->a_level > MAX_BOUNCE )  {
		if( debug&DEBUG_RAYWRITE )  {
			register struct soltab *stp;
			/* Record passing through the solid */
			stp = pp->pt_outseg->seg_stp;
			functab[stp->st_id].ft_norm(
				pp->pt_outhit, stp, &(ap->a_ray) );
			wray( pp, ap, stdout );
		}
		/* Nothing more to do for this ray */
		goto finish;
	}

	/* Add in contributions from mirror reflection & transparency */
	f = 1 - (entry->transmission + entry->transparency);
	VSCALE( ap->a_color, ap->a_color, f );
	if( entry->transmission > 0 )  {
		/* Mirror reflection */
		sub_ap.a_level = ap->a_level+1;
		sub_ap.a_hit = colorview;
		sub_ap.a_miss = hit_nothing;
		sub_ap.a_onehit = 1;
		VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
		f = 2 * VDOT( to_eye, hitp->hit_normal );
		VSCALE( work, hitp->hit_normal, f );
		/* I have been told this has unit length */
		VSUB2( sub_ap.a_ray.r_dir, work, to_eye );
		(void)shootray( &sub_ap );
		VJOIN1( ap->a_color, ap->a_color,
			entry->transmission, sub_ap.a_color );
	}
	if( entry->transparency > 0 )  {
		/* Calculate refraction at entrance. */
		sub_ap.a_level = ap->a_level * 100;	/* flag */
		sub_ap.a_x = ap->a_x;
		sub_ap.a_y = ap->a_y;
		if( !refract(ap->a_ray.r_dir, /* Incident ray (IN) */
			hitp->hit_normal,
			RI_AIR, entry->refrac_index,
			sub_ap.a_ray.r_dir	/* Refracted ray (OUT) */
		) )  {
			/* Reflected back outside solid */
			VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
			goto do_exit;
		}
		/* Find new exit point from the inside. */
		VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
do_inside:
		sub_ap.a_hit =  rfr_hit;
		sub_ap.a_miss = rfr_miss;
		(void) shootray( &sub_ap );
		/* NOTE: rfr_hit returns EXIT point in sub_ap.a_uvec,
		 *  and returns EXIT normal in sub_ap.a_color.
		 */
		if( debug&DEBUG_RAYWRITE )  {
			wraypts( sub_ap.a_ray.r_pt, sub_ap.a_uvec,
				ap, stdout );
		}
		VMOVE( sub_ap.a_ray.r_pt, sub_ap.a_uvec );

		/* Calculate refraction at exit. */
		if( !refract( sub_ap.a_ray.r_dir,	/* input direction */
			sub_ap.a_color,			/* exit normal */
			entry->refrac_index, RI_AIR,
			sub_ap.a_ray.r_dir		/* output direction */
		) )  {
			/* Reflected internally -- keep going */
			if( ++sub_ap.a_level > 100+MAX_IREFLECT )  {
				rtlog("Excessive internal reflection (x%d,y%d, lvl%d)\n",
					sub_ap.a_x, sub_ap.a_y, sub_ap.a_level );
				if(debug) {
					VSET( ap->a_color, 0, 1, 0 );	/* green */
				} else {
					VSET( ap->a_color, .16, .16, .16 );	/* grey */
				}
				goto finish;
			}
			goto do_inside;
		}
do_exit:
		sub_ap.a_hit =  colorview;
		sub_ap.a_miss = hit_nothing;
		sub_ap.a_level++;
		(void) shootray( &sub_ap );
		VJOIN1( ap->a_color, ap->a_color,
			entry->transparency, sub_ap.a_color );
	}
finish:
	return(1);
}

/*
 *			R F R _ M I S S
 */
HIDDEN int
/*ARGSUSED*/
rfr_miss( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	rtlog("rfr_miss: Refracted ray missed!\n" );
	/* Return entry point as exit point */
	VREVERSE( ap->a_color, ap->a_ray.r_dir );	/* inward pointing */
	VMOVE( ap->a_uvec, ap->a_ray.r_pt );
	return(0);
}

/*
 *			R F R _ H I T
 */
HIDDEN int
rfr_hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct hit	*hitp = PartHeadp->pt_forw->pt_outhit;
	register struct soltab *stp;

	stp = PartHeadp->pt_forw->pt_outseg->seg_stp;
	functab[stp->st_id].ft_norm(
		hitp, stp, &(ap->a_ray) );
	VMOVE( ap->a_uvec, hitp->hit_point );
	/* For refraction, want exit normal to point inward. */
	VREVERSE( ap->a_color, hitp->hit_normal );
	return(1);
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
refract( v_1, norml, ri_1, ri_2, v_2 )
register vect_t	v_1;
register vect_t	norml;
double	ri_1, ri_2;
register vect_t	v_2;
{
	LOCAL vect_t	w, u;
	FAST fastf_t	beta;

	if( NEAR_ZERO(ri_1) || NEAR_ZERO( ri_2 ) )  {
		rtlog("refract:ri1=%f, ri2=%f\n", ri_1, ri_2 );
		beta = 1;
	} else {
		beta = ri_1/ri_2;		/* temp */
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
 *  			V I E W _ E O L
 *  
 *  This routine is called by main when the end of a scanline is
 *  reached.
 */
view_eol()
{
	register int i;
	if( pixfd > 0 )  {
		i = write( pixfd, (char *)scanline, scanbytes );
		if( i != scanbytes )  {
			rtlog("view_eol: wrote %d, got %d\n", scanbytes, i);
			rtbomb("write error");
		}			
		pixelp = &scanline[0];
	}
}
view_end() {}

/*
 *  			V I E W _ I N I T
 */
view_init( ap, file, obj, npts, minus_o )
register struct application *ap;
char *file, *obj;
{
	if( npts > MAX_LINE )  {
		rtlog("view:  %d pixels/line > %d\n", npts, MAX_LINE);
		exit(12);
	}
	if( minus_o )  {
		/* Output is destined for a pixel file */
		pixelp = &scanline[0];
		scanbytes = npts * 3;
	}  else  {
		/* Output directly to Ikonas */
		if( npts > 512 )
			ikhires = 1;

		ikopen();
		load_map(1);		/* Standard map: linear */
		ikclear();
		if( npts <= 32 )  {
			ikzoom( 15, 15 );	/* 1 pixel gives 16 */
			ikwindow( (0)*4, 4063+31 );
		} else if( npts <= 50 )  {
			ikzoom( 9, 9 );		/* 1 pixel gives 10 */
			ikwindow( (0)*4, 4063+31 );
		} else if( npts <= 64 )  {
			ikzoom( 7, 7 );		/* 1 pixel gives 8 */
			ikwindow( (0)*4, 4063+29 );
		} else if( npts <= 128 )  {
			ikzoom( 3, 3 );		/* 1 pixel gives 4 */
			ikwindow( (0)*4, 4063+25 );
		} else if ( npts <= 256 )  {
			ikzoom( 1, 1 );		/* 1 pixel gives 2 */
			ikwindow( (0)*4, 4063+17 );
		}
	}

	/* Moss's material database hooks */
	if( mat_Open_Db( "mat.db" ) != NULL )  {
		mat_Asc_Read_Db();
		mat_Close_Db();
	}
}

/*
 *  			V I E W _ I N I T
 *
 *  Called each time a new image is about to be done.
 */
view_2init( ap, outfd )
register struct application *ap;
{
	vect_t temp;

	pixfd = outfd;
	ap->a_miss = hit_nothing;
	ap->a_onehit = 1;

	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;
		/* If present, use user-specified light solid */
		if( (l0stp=find_solid("LIGHT")) != SOLTAB_NULL )  {
			VMOVE( l0pos, l0stp->st_center );
			VPRINT("LIGHT0 at", l0pos);
			break;
		}
		if(debug)rtlog("No explicit light\n");
		goto debug_lighting;
	case 1:
	case 2:
	case 3:
		ap->a_hit = viewit;
debug_lighting:
		/* Determine the Light location(s) in view space */
		/* lightmodel 0 does this in view.c */
		/* 0:  At left edge, 1/2 high */
		VSET( temp, -1, 0, 1 );
		MAT4X3VEC( l0pos, view2model, temp );
		VMOVE( l0vec, l0pos );
		VUNITIZE(l0vec);

		/* 1: At right edge, 1/2 high */
		VSET( temp, 1, 0, 1 );
		MAT4X3VEC( l1vec, view2model, temp );
		VUNITIZE(l1vec);

		/* 2:  Behind, and overhead */
		VSET( temp, 0, 1, -0.5 );
		MAT4X3VEC( l2vec, view2model, temp );
		VUNITIZE(l2vec);
		break;
	default:
		rtbomb("bad lighting model #");
	}
}
