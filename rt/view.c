/*
 *			V I E W
 *
 * Ray Tracing program, sample lighting models.  Part of the
 * RT program proper.
 *
 *  Many varied and wonderous "lighting models" are implemented.
 *  The notion of output format is randomly mixed in as well.
 *  The extern "lightmodel" selects which one is being used:
 *	0	model with color, based on Moss's LGT
 *	1	1-light, from the eye.
 *	2	Spencer's surface-normals-as-colors display
 *	3	GIFT format .PP (pretty picture) files
 *	4	Gwyn format ray files
 *	5	3-light debugging model
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
#include "vmath.h"
#include "raytrace.h"
#include "debug.h"
#include "../h/mater.h"

/* /vld/include/ray.h -- ray segment data format (D A Gwyn) */
/* binary ray segment data record; see ray(4V) */
struct vldray  {
	float	ox;			/* origin coordinates */
	float	oy;
	float	oz;
	float	rx;			/* ray vector */
	float	ry;
	float	rz;
	float	na;			/* origin surface normal */
	float	ne;
	/* the following are in 2 pieces for binary file portability: */
	short	ob_lo;			/* object code low 16 bits */
	short	ob_hi;			/* object code high 16 bits */
	short	rt_lo;			/* ray tag low 16 bits */
	short	rt_hi;			/* ray tag high 16 bits */
};
static struct vldray vldray;

extern int ikfd;		/* defined in iklib.o */
extern int ikhires;		/* defined in iklib.o */

extern int outfd;		/* defined in rt.c */
extern FILE *outfp;		/* defined in rt.c */
extern int lightmodel;		/* lighting model # to use */
extern int one_hit_flag;

#define MAX_LINE	1024		/* Max pixels/line */
/* Current arrangement is definitely non-parallel! */
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer, R,G,B */
static char *pixelp;			/* pointer to first empty pixel */
static int scanbytes;			/* # bytes in scanline to write */

/* Stuff for pretty-picture output format */
static struct soltab *last_solidp;	/* pointer to last solid hit */
static int last_item;			/* item number of last region hit */
static int last_ihigh;			/* last inten_high */
static int ntomiss;			/* number of pixels to miss */
static int col;				/* column; for PP 75 char/line crap */

#define BACKGROUND	0x00800040		/* Blue/Green */
#define GREY_BACKGROUND	0x00404040		/* Grey */

vect_t l0color = {  28,  28, 255 };		/* R, G, B */
vect_t l1color = { 255,  28,  28 };
vect_t l2color = { 255, 255, 255 };		/* White */
vect_t ambient_color = { 255, 255, 255 };	/* Ambient white light */
extern vect_t l0vec;
extern vect_t l1vec;
extern vect_t l2vec;
extern vect_t l0pos;			/* pos of light0 (overrides l0vec) */
extern double AmbientIntensity;

/* These shadow functions return a boolean "light_visible" */
func_hit(ap, PartHeadp)
struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;

	/* Check to see if we hit the light source */
	if( strncmp( pp->pt_inseg->seg_stp->st_name, "LIGHT", 5 )==0 )
		return(1);		/* light_visible = 1 */
	return(0);	/* light_visible = 0 */
}
func_miss() {return(1);}

/* Null function */
nullf() { ; }

l3init(ap, title1, title2 )
register struct application *ap;
{
	extern double azimuth, elevation;
	extern int npts;

	fprintf(outfp, "%s: %s (RT)\n", title1, title2 );
	fprintf(outfp, "%10d%10d", (int)azimuth, (int)elevation );
	fprintf(outfp, "%10d%10d\n", npts, npts );
}

/* Support for Gwyn's ray files -- write a hit */
l4hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	register int i;	/* XXX */

	for( ; pp != PartHeadp; pp = pp->pt_forw )  {
		VMOVE( &(vldray.ox), pp->pt_inhit->hit_point );
		VSUB2( &(vldray.rx), pp->pt_outhit->hit_point,
			pp->pt_inhit->hit_point );
		/* Check pt_inflip, pt_outflip for normals! */
		vldray.na = vldray.ne = 0.0;	/* need angle/azim */
		i = pp->pt_regionp->reg_regionid;
		vldray.ob_lo = i & 0xFFFF;
		vldray.ob_hi = (i>>16) & 0xFFFF;
		vldray.rt_lo = ap->a_x;
		vldray.rt_hi = ap->a_y;
		fwrite( &vldray, sizeof(struct vldray), 1, outfp );
	}
}

/* Support for pretty-picture files */
l3hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	register struct hit *hitp= pp->pt_inhit;
	LOCAL double cosI0;
	register int i,j;

#define pchar(c) {putc(c,outfp);if(col++==74){putc('\n',outfp);col=0;}}

	cosI0 = -VDOT(hitp->hit_normal, ap->a_ray.r_dir);
	if( pp->pt_inflip )
		cosI0 = -cosI0;
	if( cosI0 <= 0.0 )  {
		ntomiss++;
		return;
	}
	if( ntomiss > 0 )  {
		pchar(' ');	/* miss target cmd */
		pknum( ntomiss );
		ntomiss = 0;
		last_solidp = SOLTAB_NULL;
	}
	if( last_item != pp->pt_regionp->reg_regionid )  {
		last_item = pp->pt_regionp->reg_regionid;
		pchar( '#' );	/* new item cmd */
		pknum( last_item );
		last_solidp = SOLTAB_NULL;
	}
	if( last_solidp != pp->pt_inseg->seg_stp )  {
		last_solidp = pp->pt_inseg->seg_stp;
		pchar( '!' );		/* new solid cmd */
	}
	i = cosI0 * 255.0;		/* integer angle */
	j = (i>>5) & 07;
	if( j != last_ihigh )  {
		last_ihigh = j;
		pchar( '0'+j );		/* new inten high */
	}
	j = i & 037;
	pchar( '@'+j );			/* low bits of pixel */
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
	LOCAL long inten;
	LOCAL fastf_t diffuse2, cosI2;
	LOCAL fastf_t diffuse1, cosI1;
	LOCAL fastf_t diffuse0, cosI0;
	LOCAL vect_t work0, work1;
	LOCAL int r,g,b;

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
		VADD2( work0, work0, work1 );
	}  else if( lightmodel == 5 )  {
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
		VADD2( work0, work0, work1 );
	} else if( lightmodel == 2 )  {
		/* Store surface normals pointing inwards */
		/* (For Spencer's moving light program */
		work0[0] = (hitp->hit_normal[0] * (-127)) + 128;
		work0[1] = (hitp->hit_normal[1] * (-127)) + 128;
		work0[2] = (hitp->hit_normal[2] * (-127)) + 128;
	}

	if(debug&DEBUG_HITS)  {
		hit_print( " In", hitp );
		fprintf(stderr,"cosI0=%f, diffuse0=%f   ", cosI0, diffuse0 );
		VPRINT("RGB", work0);
	}

	r = work0[0];
	g = work0[1];
	b = work0[2];
	if( r > 255 ) r = 255;
	if( g > 255 ) g = 255;
	if( b > 255 ) b = 255;
	if( r<0 || g<0 || b<0 )  {
		VPRINT("@@ Negative RGB @@", work0);
		inten = 0x0080FF80;
	} else {
		inten = (b << 16) |		/* B */
			(g <<  8) |		/* G */
			(r);			/* R */
	}

	if( ikfd > 0 )
		ikwpixel( ap->a_x, ap->a_y, inten);
	if( outfd > 0 )  {
		*pixelp++ = inten & 0xFF;	/* R */
		*pixelp++ = (inten>>8) & 0xFF;	/* G */
		*pixelp++ = (inten>>16) & 0xFF;	/* B */
	}
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
	LOCAL struct application shadow_ap;
	LOCAL long inten;
	LOCAL int r,g,b;
	LOCAL int light_visible;

	double	Rd1, Rd2;
	double	cosI1, cosI2;
	double	cosS;
	vect_t	work;
	double	dist_gradient = 1.0;
	int	red, grn, blu;
	LOCAL vect_t	reflected;
	LOCAL vect_t	to_eye;

	/* Check to see if we hit the light source */
	if( strncmp( pp->pt_inseg->seg_stp->st_name, "LIGHT", 5 )==0 )  {
		inten = 0x00FFFFFF;	/* white */
		goto done;
	}
	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < 0.0 )  {
		inten = 0xffL;		/* red */
		if(debug)fprintf(stderr,"colorview:  eye inside solid\n");
		goto done;
	}

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
#define illum_pri_src	0.6
	Rd1 = diffReflec( hitp->hit_normal, l0vec, illum_pri_src, &cosI1 );
	Rd1 *= dist_gradient;

	/* Diffuse reflectance from secondary light source (at eye) */
#define illum_sec_src	0.4
	Rd2 = diffReflec( hitp->hit_normal, to_eye, illum_sec_src, &cosI2 );
	Rd2 *= dist_gradient;

	/* Calculate specular reflectance.
	 *	Reflected ray = (2 * cos(i) * Normal) - Incident ray.
	 * 	Cos(s) = dot product of Reflected ray with Shotline vector.
	 */
	VSCALE( work, hitp->hit_normal, 2 * cosI1 );
	VSUB2( reflected, work, l0vec );
	cosS = VDOT( reflected, to_eye );

	/* Get default color-by-ident for region.			*/
	{
		register struct mater *mp;
		mp = ((struct mater *)pp->pt_inseg->seg_tp->tr_materp);
		red = mp->mt_r;
		grn = mp->mt_g;
		blu = mp->mt_b;
	}

	/* Apply secondary (ambient) (white) lighting. */
	r = (double) red * Rd2;
	g = (double) grn * Rd2;
	b = (double) blu * Rd2;

	/* Fire ray at light source to check for shadowing */
	shadow_ap.a_hit = func_hit;
	shadow_ap.a_miss = func_miss;
	VMOVE( shadow_ap.a_ray.r_pt, hitp->hit_point );
	VSUB2( shadow_ap.a_ray.r_dir, l0pos, hitp->hit_point );
	VUNITIZE( shadow_ap.a_ray.r_dir );
	light_visible = shootray( &shadow_ap );
	
	/* If not shadowed add primary lighting.			*/
#define lgt1_red_coef	1
#define lgt1_grn_coef	1
#define lgt1_blu_coef	1
	if( light_visible )  {
		red = r + (int)((double)red * Rd1 * lgt1_red_coef);
		grn = g + (int)((double)grn * Rd1 * lgt1_grn_coef);
		blu = b + (int)((double)blu * Rd1 * lgt1_blu_coef);

		/* Check for specular reflection. */
#define cosSRAngle 0.9
		if( cosS >= cosSRAngle )  {
			/* We have a specular return.	*/
			double	spec_intensity = 0.0;

			if( cosSRAngle < 1.0 )	{
				spec_intensity = (cosS-cosSRAngle)/(1.0-cosSRAngle);
			}
#define redSpecComponent 255
#define grnSpecComponent 255
#define bluSpecComponent 255
			red += ((redSpecComponent-red) * spec_intensity);
			grn += ((grnSpecComponent-grn) * spec_intensity);
			blu += ((bluSpecComponent-blu) * spec_intensity);
		}
		r = red;
		g = grn;
		b = blu;
	}

	if( r > 255 ) r = 255;
	if( g > 255 ) g = 255;
	if( b > 255 ) b = 255;
	if( r<0 || g<0 || b<0 )  {
		inten = 0x0080FF80;
	} else {
		inten = (b << 16) |		/* B */
			(g <<  8) |		/* G */
			(r);			/* R */
	}
done:
	if( ikfd > 0 )
		ikwpixel( ap->a_x, ap->a_y, inten);
	if( outfd > 0 )  {
		*pixelp++ = inten & 0xFF;	/* R */
		*pixelp++ = (inten>>8) & 0xFF;	/* G */
		*pixelp++ = (inten>>16) & 0xFF;	/* B */
	}
}

l3miss()  {
	last_solidp = SOLTAB_NULL;
	ntomiss++;
}

/*
 *			W B A C K G R O U N D
 *
 *  a_miss() routine.
 */
wbackground( ap )
register struct application *ap;
{
	register long bg;

	if( lightmodel == 2 )
		bg = 0;
	else
		bg = BACKGROUND;
		
	if( ikfd > 0 )
		ikwpixel( ap->a_x, ap->a_y, bg );
	if( outfd > 0 )  {
		*pixelp++ = bg & 0xFF;	/* R */
		*pixelp++ = (bg>>8) & 0xFF;	/* G */
		*pixelp++ = (bg>>16) & 0xFF;	/* B */
	}
}

/*
 *  			H I T _ P R I N T
 */
hit_print( str, hitp )
char *str;
register struct hit *hitp;
{
	fprintf(stderr,"** %s HIT, dist=%f\n", str, hitp->hit_dist );
	VPRINT("** Point ", hitp->hit_point );
	VPRINT("** Normal", hitp->hit_normal );
}

/*
 *  			D E V _ S E T U P
 *  
 *  Prepare the Ikonas display for operation with
 *  npts x npts of useful pixels.
 */
dev_setup(n)
int n;
{
	if( n > MAX_LINE )  {
		fprintf(stderr,"view:  %d pixels/line is too many\n", n);
		exit(12);
	}
	if( outfd > 0 )  {
		/* Output is destined for a pixel file */
		pixelp = &scanline[0];
		if( n > 512 )
			scanbytes = MAX_LINE * 3;
		else
			scanbytes = 512 * 3;
	}  else  {
		/* Output directly to Ikonas */
		if( n > 512 )
			ikhires = 1;

		ikopen();
		load_map(1);		/* Standard map: linear */
		ikclear();
		if( n <= 64 )  {
			ikzoom( 7, 7 );		/* 1 pixel gives 8 */
			ikwindow( (0)*4, 4063+29 );
		} else if( n <= 128 )  {
			ikzoom( 3, 3 );		/* 1 pixel gives 4 */
			ikwindow( (0)*4, 4063+25 );
		} else if ( n <= 256 )  {
			ikzoom( 1, 1 );		/* 1 pixel gives 2 */
			ikwindow( (0)*4, 4063+17 );
		}
	}
}

l3eol()
{
		pchar( '.' );		/* End of scanline */
		last_solidp = SOLTAB_NULL;
		ntomiss = 0;
}

/*
 *  			D E V _ E O L
 *  
 *  This routine is called by main when the end of a scanline is
 *  reached.
 */
dev_eol()
{
	if( outfd > 0 )  {
		write( outfd, (char *)scanline, scanbytes );
		bzero( (char *)scanline, scanbytes );
		pixelp = &scanline[0];
	}
}

/*
 *  Called when the picture is finally done.
 */
l3end()
{
	fprintf( outfp, "/\n" );	/* end of view */
	fflush( outfp );
}

/*
 *  			P K N U M
 *  
 *  Oddball 5-bits in a char ('@', 'A', ... on up) number packing.
 *  Number is written 5 bits at a time, right to left (low to high)
 *  until there are no more non-zero bits remaining.
 */
pknum( arg )
int arg;
{
	register long i = arg;

	do {
		pchar( (int)('@'+(i & 037)) );
		i >>= 5;
	} while( i > 0 );
}

/*
 *  			V I E W _ I N I T
 */
view_init( ap )
register struct application *ap;
{
	struct soltab *stp;

	/* Initialize the application selected */
	ap->a_hit = ap->a_miss = nullf;	/* ?? */
	ap->a_init = ap->a_eol = ap->a_end = nullf;

	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;
		ap->a_miss = wbackground;
		ap->a_eol = dev_eol;
		one_hit_flag = 1;
		/* If present, use user-specified light solid */
		(void)solid_pos( "LIGHT", l0pos );
VPRINT("LIGHT0 at", l0pos);
		break;
	case 1:
	case 2:
	case 5:
		ap->a_hit = viewit;
		ap->a_miss = wbackground;
		ap->a_eol = dev_eol;
		one_hit_flag = 1;
		break;
	case 3:
		ap->a_hit = l3hit;
		ap->a_miss = l3miss;
		ap->a_init = l3init;
		ap->a_end = l3end;
		ap->a_eol = l3eol;
		one_hit_flag = 1;
		break;
	case 4:
		ap->a_hit = l4hit;
		ap->a_miss = nullf;
		ap->a_eol = nullf;
		break;
	default:
		rtbomb("bad lighting model #");
	}

	if( lightmodel == 3 || lightmodel == 4 )
		if( outfd > 0 )
			outfp = fdopen( outfd, "w" );
		else
			rtbomb("No output file specified");
}
