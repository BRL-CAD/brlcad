/*
 *			V I E W
 *
 * Ray Tracing program, sample lighting models.  Part of the
 * RT program proper.
 *
 *  Many varied and wonderous "lighting models" are implemented.
 *  The output format is a .PIX file
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
#include "raytrace.h"
#include "debug.h"

#include "mat_db.h"

char usage[] = "\
Usage:  rt [options] model.g objects...\n\
Options:\n\
 -f#		Grid size in pixels, default 512, max 1024\n\
 -aAz		Azimuth in degrees	(conflicts with -M)\n\
 -eElev		Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -o model.pix	Specify output file, .pix format (default=framebuffer)\n\
 -x#		Set debug flags\n\
 -p[#]		Perspective viewing, optional focal length scaling\n\
 -l#		Select lighting model\n\
 	0	Two lights, one at eye (default)\n\
	1	One light, from eye\n\
	2	Surface-normals as colors\n\
	3	Three light debugging model\n\
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

HIDDEN int	rfr_hit(), rfr_miss();
HIDDEN void	refract();

#define RI_AIR		1.0    /* Refractive index of air.		*/
#define RI_GLASS	1.3    /* Refractive index of glass.		*/

/*
 *  			R A N D 0 T O 1
 *
 *  Returns a random number in the range 0 to 1
 */
double rand0to1()
{
	FAST fastf_t f;
	f = ((double)random()) / (double)017777777777L;
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

	result = 1;
	while( cnt-- > 0 )
		result *= d;
	return( result );
}

/* These shadow functions return a boolean "light_visible" */
func_hit(ap, PartHeadp)
struct application *ap;
struct partition *PartHeadp;
{
	register struct soltab *stp;
	register char *cp;

	/* Check to see if we hit the light source */
	if( (stp = PartHeadp->pt_forw->pt_inseg->seg_stp) == l0stp )
		return(1);		/* light_visible = 1 */
	if( *(cp=stp->st_name) == 'L' && strncmp( cp, "LIGHT", 5 )==0 )  {
		l0stp = stp;
		return(1);		/* light_visible = 1 */
	}
	return(0);			/* light_visible = 0 */
}
func_miss() {return(1);}

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

	if( ikfd > 0 )
		ikwpixel( ap->a_x, ap->a_y, (b<<16)|(g<<8)|(r) );
	if( pixfd > 0 )  {
		*pixelp++ = r & 0xFF;
		*pixelp++ = g & 0xFF;
		*pixelp++ = b & 0xFF;
	}
	if(debug&DEBUG_HITS) rtlog("rgb=%3d,%3d,%3d\n", r,g,b);
}


/*	l g t _ P i x e l ( )
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

 */

/*	d i f f R e f l e c ( )
 *	Return the diffuse reflectance from 'light' source.
 */
double
diffReflec( norml, light, illum, cosI )
double	*norml, *light;
double	illum;
double	*cosI;	/* Cosine of the angle of incidence */
{
	if( (*cosI = VDOT( norml, light )) < 0.0 ) {
		return	0.0;
	} else
		return	*cosI * illum;
}

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
	auto point_t	mcolor;		/* Material color */
	Mat_Db_Entry	*entry;

	if(debug&DEBUG_HITS)  {
		pr_pt(pp);
		pr_hit( "view", pp->pt_inhit);
	}

	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < 0.0 )  {
		VSET( ap->a_color, 1, 0, 0 );
		rtlog("colorview:  eye inside %s (x=%d, y=%d, lvl=%d)\n",
			pp->pt_inseg->seg_stp->st_name,
			ap->a_x, ap->a_y, ap->a_level);
		goto finish;
	}

	/* Check to see if we hit something special */
	{
		register struct soltab *stp;
		register char *cp;
		cp = (stp = pp->pt_inseg->seg_stp)->st_name;
		if( stp == l0stp ||
		    (*cp == 'L'  &&  strncmp( cp, "LIGHT", 5 )==0 ) )  {
			l0stp = stp;
			VSET( ap->a_color, 1, 1, 1 );	/* White */
			goto finish;
		}
		/* Clouds "Texture" map */
		if( *cp == 'C' && strncmp( cp, "CLOUD", 5 )==0 )  {
			auto fastf_t uv[2];
			double inten;
			extern double texture();
			functab[pp->pt_inseg->seg_stp->st_id].ft_uv(
				pp->pt_inseg->seg_stp, hitp, uv );
			inten = texture( uv[0], uv[1], 1.0, 2.2, 1.0 );
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
	VSUB2( l0vec, l0pos, hitp->hit_point );
	VUNITIZE( l0vec );
#define illum_pri_src	0.7
	Rd1 = diffReflec( hitp->hit_normal, l0vec, illum_pri_src, &cosI1 );

	/* Diffuse reflectance from secondary light source (at eye) */
#define illum_sec_src	0.5
	d_a = diffReflec( hitp->hit_normal, to_eye, illum_sec_src, &cosI2 );

	/* Apply secondary (ambient) (white) lighting. */
	VSCALE( ap->a_color, mcolor, d_a );

	/* Fire ray at light source to check for shadowing */
	/* This SHOULD actually return an energy value */
	sub_ap.a_hit = func_hit;
	sub_ap.a_miss = func_miss;
	sub_ap.a_onehit = 1;
	sub_ap.a_level = ap->a_level + 1;
	sub_ap.a_x = ap->a_x;
	sub_ap.a_y = ap->a_y;
	VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
	if( l0stp )  {
		/* Dither light pos for penumbra by +/- 0.5 light radius */
		FAST fastf_t f;
		f = l0stp->st_aradius * 0.9;
		sub_ap.a_ray.r_dir[X] =  l0pos[X] + rand_half()*f - hitp->hit_point[X];
		sub_ap.a_ray.r_dir[Y] =  l0pos[Y] + rand_half()*f - hitp->hit_point[Y];
		sub_ap.a_ray.r_dir[Z] =  l0pos[Z] + rand_half()*f - hitp->hit_point[Z];
	} else {
		VSUB2( sub_ap.a_ray.r_dir, l0pos, hitp->hit_point );
	}
	VUNITIZE( sub_ap.a_ray.r_dir );
	light_visible = shootray( &sub_ap );
	
	/* If not shadowed add primary lighting. */
	if( light_visible )  {
		register fastf_t specular;
		register fastf_t cosS;

		/* Diffuse */
		VJOIN1( ap->a_color, ap->a_color, Rd1, mcolor );

		/* Calculate specular reflectance.
		 *	Reflected ray = (2 * cos(i) * Normal) - Incident ray.
		 * 	Cos(s) = Reflected ray DOT Incident ray.
		 */
		cosI1 *= 2;
		VSCALE( work, hitp->hit_normal, cosI1 );
		VSUB2( reflected, work, l0vec );
		if( (cosS = VDOT( reflected, to_eye )) > 0 )  {
			specular = entry->wgt_specular *
				ipow(cosS,(int)entry->shine);
			VJOIN1( ap->a_color, ap->a_color, specular, l0color );
		}
	}

	if( (entry->transmission <= 0 && entry->transparency <= 0) ||
	    ap->a_level > 3 )  {
		/* Nothing more to do for this ray */
		goto finish;
	}

	/* Add in contributions from mirror reflection & transparency */
	f = 1 - (entry->transmission + entry->transparency);
	VSCALE( ap->a_color, ap->a_color, f );
	if( entry->transmission > 0 )  {
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
		refract(ap->a_ray.r_dir, /* Incident ray.*/
			hitp->hit_normal,
			RI_AIR,		/* Ref. index of air.*/
			entry->refrac_index,
			sub_ap.a_ray.r_dir	/* Refracted ray. */
			);
		/* Find new exit point. */
		sub_ap.a_hit =  rfr_hit;
		sub_ap.a_miss = rfr_miss;
		sub_ap.a_level = ap->a_level + 1;
		VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
		(void) shootray( &sub_ap );
		/* HACK:  modifies sub_ap.a_ray.r_pt to be EXIT point! */
		/* returns EXIT normal in sub_ap.a_color, leaves r_dir */

		/* Calculate refraction at exit. */
		refract( sub_ap.a_ray.r_dir,	/* input direction */
			sub_ap.a_color,		/* exit normal */
			entry->refrac_index,
			RI_AIR,
			sub_ap.a_ray.r_dir	/* output direction */
			);
		sub_ap.a_hit =  colorview;
		sub_ap.a_miss = hit_nothing;
		sub_ap.a_level = ap->a_level + 1;
		(void) shootray( &sub_ap );
		VJOIN1( ap->a_color, ap->a_color,
			entry->transparency, sub_ap.a_color );
	}
finish:
	return(1);
}

/*	d o _ E r r o r ( )
 */
HIDDEN int
/*ARGSUSED*/
rfr_miss( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	static int	ref_missed = 0;

	rtlog("rfr_miss: Refracted ray missed!\n" );
	/* Return entry point as exit point in a_ray.r_pt */
	VREVERSE( ap->a_color, ap->a_ray.r_dir );	/* inward pointing */
	return(0);
}

/*	d o _ P r o b e ( )
 */
HIDDEN int
/*ARGSUSED*/
rfr_hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct hit	*hitp = PartHeadp->pt_forw->pt_outhit;

	VMOVE( ap->a_ray.r_pt, hitp->hit_point );
	/* For refraction, want exit normal to point inward. */
	VREVERSE( ap->a_color, hitp->hit_normal );
	return	1;
}

/*	r e f r a c t ( )
 *
 *	Compute the refracted ray 'v_2' from the incident ray 'v_1' with
 *	the refractive indices 'ri_2' and 'ri_1' respectively.
 *
 *  Note:  output (v_2) can be same storage as an input.
 */
HIDDEN void
refract( v_1, norml, ri_1, ri_2, v_2 )
register vect_t	v_1;
register vect_t	norml;
double	ri_1, ri_2;
register vect_t	v_2;
{
	LOCAL vect_t	w, u;
	FAST fastf_t	beta;

	beta = ri_1/ri_2;		/* temp */
	VSCALE( w, v_1, beta );
	VCROSS( u, w, norml );
	if( (beta = VDOT( u, u )) > 1.0 )  {
		/*  Past critical angle, total reflection.
		 *  Calculate reflected (bounced) incident ray.
		 */
		VREVERSE( u, v_1 );
		beta = 2 * VDOT( u, norml );
		VSCALE( w, norml, beta );
		VSUB2( v_2, w, u );
		return;
	} else {
		beta = -sqrt( 1.0 - beta) - VDOT( w, norml );
		VSCALE( u, norml, beta );
		VADD2( v_2, w, u );
	}
}

/*
 *  			V I E W _ E O L
 *  
 *  This routine is called by main when the end of a scanline is
 *  reached.
 */
view_eol()
{
	if( pixfd > 0 )  {
		write( pixfd, (char *)scanline, scanbytes );
		bzero( (char *)scanline, scanbytes );
		pixelp = &scanline[0];
	}
}
view_end() {}

/*
 *  			V I E W _ I N I T
 */
view_init( ap, file, obj, npts, outfd )
register struct application *ap;
char *file, *obj;
{
	pixfd = outfd;
	if( npts > MAX_LINE )  {
		rtlog("view:  %d pixels/line is too many\n", npts);
		exit(12);
	}
	if( pixfd > 0 )  {
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
view_2init( ap )
register struct application *ap;
{
	vect_t temp;

	ap->a_miss = hit_nothing;
	ap->a_onehit = 1;

	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;
		/* If present, use user-specified light solid */
		if( solid_pos( "LIGHT", l0pos ) >= 0 )  {
			/* NEED A WAY TO FIND l0stp here! */
			VPRINT("LIGHT0 at", l0pos);
			break;
		}
		rtlog("No explicit light\n");
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
