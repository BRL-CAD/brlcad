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

	if(rt_g.debug&DEBUG_HITS)  {
		rt_pr_hit( " In", hitp );
		rt_log("cosI0=%f, diffuse0=%f   ", cosI0, diffuse0 );
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
	if(rt_g.debug&DEBUG_HITS) rt_log("rgb=%3d,%3d,%3d\n", r,g,b);
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

	if(rt_g.debug&DEBUG_HITS)  {
		rt_pr_pt(pp);
		rt_pr_hit( "view", pp->pt_inhit);
	}

	/* Temporary check to make sure normals are OK */
	if( hitp->hit_normal[X] < -1.01 || hitp->hit_normal[X] > 1.01 ||
	    hitp->hit_normal[Y] < -1.01 || hitp->hit_normal[Y] > 1.01 ||
	    hitp->hit_normal[Z] < -1.01 || hitp->hit_normal[Z] > 1.01 )  {
		rt_log("colorview: N=(%f,%f,%f)?\n",
			hitp->hit_normal[X],hitp->hit_normal[Y],hitp->hit_normal[Z]);
		VSET( ap->a_color, 1, 1, 0 );
		return(1);
	}

	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < 0.0 )  {
		struct application sub_ap;
		FAST fastf_t f;

		if( rt_g.debug || ap->a_level > MAX_BOUNCE )  {
			VSET( ap->a_color, 1, 0, 0 );
			rt_log("colorview:  eye inside %s (x=%d, y=%d, lvl=%d)\n",
				pp->pt_inseg->seg_stp->st_name,
				ap->a_x, ap->a_y, ap->a_level);
			return(1);
		}
		/* Push on to exit point, and trace on from there */
		sub_ap = *ap;	/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		f = pp->pt_outhit->hit_dist+0.0001;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
		(void)rt_shootray( &sub_ap );
		VSCALE( ap->a_color, sub_ap.a_color, 0.8 );
		return(1);
	}

	if( rt_g.debug&DEBUG_RAYWRITE )  {
		/* Record the approach path */
		if( hitp->hit_dist > EPSILON )
			wraypts( ap->a_ray.r_pt,
				hitp->hit_point,
				ap, stdout );
	}

	/* Check to see if we hit something special */
	{
		register struct soltab *stp;
		stp = pp->pt_inseg->seg_stp;
		if( stp == l0stp )  {
			VMOVE( ap->a_color, l0color );
			return(1);
		}
	}

	if( pp->pt_inflip )  {
		VREVERSE( hitp->hit_normal, hitp->hit_normal );
		pp->pt_inflip = 0;
	}

	if( !(pp->pt_regionp->reg_ufunc) )  {
		if( matlib_setup( pp->pt_regionp ) == 0 )  {
			rt_log("matlib_setup failure");
			return(0);
		}
	}
	return( pp->pt_regionp->reg_ufunc( ap, pp ) );
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
	rt_log("rfr_miss: Refracted ray missed!\n" );
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
	rt_functab[stp->st_id].ft_norm(
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
		rt_log("refract:ri1=%f, ri2=%f\n", ri_1, ri_2 );
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
			rt_log("view_eol: wrote %d, got %d\n", scanbytes, i);
			rt_bomb("write error");
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
		rt_log("view:  %d pixels/line > %d\n", npts, MAX_LINE);
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
}

/*
 *  			V I E W _ I N I T
 *
 *  Called each time a new image is about to be done.
 */
view_2init( ap, outfd )
register struct application *ap;
{
	extern int hit_nothing();
	vect_t temp;

	pixfd = outfd;
	ap->a_miss = hit_nothing;
	ap->a_onehit = 1;

	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;
		/* If present, use user-specified light solid */
		if( (l0stp=rt_find_solid("LIGHT")) != SOLTAB_NULL )  {
			VMOVE( l0pos, l0stp->st_center );
			VPRINT("LIGHT0 at", l0pos);
			break;
		}
		if(rt_g.debug)rt_log("No explicit light\n");
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
		rt_bomb("bad lighting model #");
	}
}
