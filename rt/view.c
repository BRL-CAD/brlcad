/*
 *			V I E W . C
 *
 *	Ray Tracing program, lighting model manager.
 *
 *  Output is either interactive to a frame buffer, or written in a file.
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
 *	The incomming light rays point IN.
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
static char RCSview[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "fb.h"
#include "../librt/debug.h"
#include "./mathtab.h"

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
";

FBIO	*fbp = FBIO_NULL;	/* Framebuffer handle */

extern int lightmodel;		/* lighting model # to use */
extern mat_t view2model;
extern mat_t model2view;
extern int hex_out;		/* Output format, 0=binary, !0=hex */

#define MAX_LINE	(1024*8)	/* Max pixels/line */
/* Current arrangement is definitely non-parallel! */
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer, R,G,B */
static char *pixelp;			/* pointer to first empty pixel */
static FILE *pixfp = NULL;		/* fd of .pix file */

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

/*
 *			V I E W I T
 *
 *  a_hit() routine for simple lighting model.
 */
viewit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	register struct hit *hitp;
	LOCAL fastf_t diffuse2, cosI2;
	LOCAL fastf_t diffuse1, cosI1;
	LOCAL fastf_t diffuse0, cosI0;
	LOCAL vect_t work0, work1;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("viewit:  no hit out front?\n");
		return(0);
	}
	hitp = pp->pt_inhit;

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
	return(0);
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

	/* To prevent bad color aliasing, add some color dither */
	r = ap->a_color[0]*255.+rand_half();
	g = ap->a_color[1]*255.+rand_half();
	b = ap->a_color[2]*255.+rand_half();
	if( r > 255 ) r = 255;
	if( g > 255 ) g = 255;
	if( b > 255 ) b = 255;
	if( r<0 || g<0 || b<0 )  {
		VPRINT("@@ Negative RGB @@", ap->a_color);
		r = 0x80;
		g = 0xFF;
		b = 0x80;
	}

	if( fbp != FBIO_NULL )  {
		Pixel p;
		p.red = r;
		p.green = g;
		p.blue = b;
		fb_write( fbp, ap->a_x, ap->a_y, &p, 1 );
	}
	if( pixfp != NULL )  {
		if( hex_out )  {
			fprintf(pixfp, "%2.2x%2.2x%2.2x\n", r, g, b);
		} else {
			*pixelp++ = r;
			*pixelp++ = g;
			*pixelp++ = b;
		}
	}
	if(rt_g.debug&DEBUG_HITS) rt_log("rgb=%3d,%3d,%3d\n", r,g,b);
}


/*
 *			C O L O R V I E W
 *
 *  Manage the coloring of whatever it was we just hit.
 *  This can be a recursive procedure.
 */
colorview( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	register struct hit *hitp;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("colorview:  no hit out front?\n");
		return(0);
	}
	hitp = pp->pt_inhit;

	if(rt_g.debug&DEBUG_HITS)  {
		rt_pr_pt(pp);
		rt_pr_hit( "colorview", pp->pt_inhit);
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
	if( hitp->hit_dist >= INFINITY )  {
		rt_log("colorview:  entry beyond infinity\n");
		VSET( ap->a_color, .5, 0, 0 );
		return(1);
	}

	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < 0.0 )  {
		struct application sub_ap;
		FAST fastf_t f;

		if( rt_g.debug || pp->pt_outhit->hit_dist >= INFINITY ||
		    ap->a_level > MAX_BOUNCE )  {
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
		if( hitp->hit_dist > 0.0001 )
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
		rt_log("colorview:  no reg_ufunc\n");
		return(0);
	}
	return( pp->pt_regionp->reg_ufunc( ap, pp ) );
}

/*
 *  			V I E W _ E O L
 *  
 *  This routine is called by main when the end of a scanline is
 *  reached.
 */
view_eol()
{
	register int cnt;
	register int i;
	if( pixfp != NULL )  {
		if( hex_out )  return;
		cnt = pixelp - scanline;
		if( cnt <= 0 || cnt > sizeof(scanline) )  {
			rt_log("corrupt pixelp=x%x, scanline=x%x, cnt=%d\n",
				pixelp, scanline, cnt );
			pixelp = scanline;
			return;
		}
		i = fwrite( (char *)scanline, cnt, 1, pixfp );
		if( i != 1 )  {
			rt_log("view_eol: fwrite returned %d\n", i);
			rt_bomb("write error");
		}			
		pixelp = &scanline[0];
	}
}

/*
 *			V I E W _ E N D
 */
view_end()
{
	if( fbp != FBIO_NULL )
		fb_close(fbp);
}

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
	}  else  {
		int width;

		/* Output interactively to framebuffer */
		if( npts <= 512 )
			width = 512;
		else {
			if( npts <= 1024 )
				width = 1024;
			else
				width = npts;
		}

		if( (fbp = fb_open( NULL, width, width )) == FBIO_NULL )  {
			rt_log("view:  can't open frame buffer\n");
			exit(12);
		}
		fb_clear( fbp, PIXEL_NULL );
		fb_wmap( fbp, COLORMAP_NULL );
		/* KLUDGE ALERT:  The library want zoom before window! */
		fb_zoom( fbp, width/npts, width/npts );
		fb_window( fbp, npts/2, npts/2 );
	}
}

/*
 *  			V I E W 2 _ I N I T
 *
 *  Called each time a new image is about to be done.
 */
view_2init( ap, outfp )
register struct application *ap;
FILE *outfp;
{
	extern int hit_nothing();
	vect_t temp;

	pixfp = outfp;
	ap->a_miss = hit_nothing;
	ap->a_onehit = 1;

	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;
		/* If present, use user-specified light solid */
		if( (l0stp=rt_find_solid(ap->a_rt_i,"LIGHT")) != SOLTAB_NULL )  {
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
