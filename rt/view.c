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
 *	4	curvature debugging display (inv radius of curvature)
 *	5	curvature debugging (principal direction)
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
#include "./rdebug.h"
#include "./mathtab.h"
#include "./light.h"

char usage[] = "\
Usage:  rt [options] model.g objects...\n\
Options:\n\
 -f #		Grid size in pixels, default 512, max 1024\n\
 -a Az		Azimuth in degrees\n\
 -e Elev	Elevation in degrees\n\
 -M		Read model2view matrix on stdin\n\
 -o model.pix	Specify output file, .pix format (default=fb)\n\
 -x #		Set librt debug flags\n\
 -X #		Set rt debug flags\n\
 -p #		Perspective viewing, focal length scaling\n\
";

extern FBIO	*fbp;		/* Framebuffer handle */
extern FILE	*outfp;		/* optional output file */

extern int lightmodel;		/* lighting model # to use */
extern mat_t view2model;
extern mat_t model2view;
extern int npts;
extern int hex_out;		/* Output format, 0=binary, !0=hex */

#ifdef RTSRV
extern char scanbuf[];
#else
#ifdef PARALLEL
extern char *scanbuf;		/*** Output buffering, for parallelism */
#endif
#endif

extern struct light_specific *LightHeadp;
vect_t ambient_color = { 1, 1, 1 };	/* Ambient white light */
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
	RT_HIT_NORM( hitp, pp->pt_inseg->seg_stp, &(ap->a_ray) );

	/*
	 * Diffuse reflectance from each light source
	 */
	if( pp->pt_inflip )  {
		VREVERSE( hitp->hit_normal, hitp->hit_normal );
	}
	switch( lightmodel )  {
	case 1:
		/* Light from the "eye" (ray source).  Note sign change */
		diffuse0 = 0;
		if( (cosI0 = -VDOT(hitp->hit_normal, ap->a_ray.r_dir)) >= 0.0 )
			diffuse0 = cosI0 * ( 1.0 - AmbientIntensity);
		VSCALE( work0, LightHeadp->lt_color, diffuse0 );

		/* Add in contribution from ambient light */
		VSCALE( work1, ambient_color, AmbientIntensity );
		VADD2( ap->a_color, work0, work1 );
		break;
	case 3:
		/* Simple attempt at a 3-light model. */
		{
			struct light_specific *l0, *l1, *l2;
			l0 = LightHeadp;
			l1 = l0->lt_forw;
			l2 = l1->lt_forw;

			diffuse0 = 0;
			if( (cosI0 = VDOT(hitp->hit_normal, l0->lt_vec)) >= 0.0 )
				diffuse0 = cosI0 * l0->lt_fraction;
			diffuse1 = 0;
			if( (cosI1 = VDOT(hitp->hit_normal, l1->lt_vec)) >= 0.0 )
				diffuse1 = cosI1 * l1->lt_fraction;
			diffuse2 = 0;
			if( (cosI2 = VDOT(hitp->hit_normal, l2->lt_vec)) >= 0.0 )
				diffuse2 = cosI2 * l2->lt_fraction;

			VSCALE( work0, l0->lt_color, diffuse0 );
			VSCALE( work1, l1->lt_color, diffuse1 );
			VADD2( work0, work0, work1 );
			VSCALE( work1, l2->lt_color, diffuse2 );
			VADD2( work0, work0, work1 );
		}
		/* Add in contribution from ambient light */
		VSCALE( work1, ambient_color, AmbientIntensity );
		VADD2( ap->a_color, work0, work1 );
		break;
	case 2:
		/* Store surface normals pointing inwards */
		/* (For Spencer's moving light program */
		ap->a_color[0] = (hitp->hit_normal[0] * (-.5)) + .5;
		ap->a_color[1] = (hitp->hit_normal[1] * (-.5)) + .5;
		ap->a_color[2] = (hitp->hit_normal[2] * (-.5)) + .5;
		break;
	case 4:
	 	{
			LOCAL struct curvature cv;
			FAST fastf_t f;
			auto int ival;

			RT_CURVE( &cv, hitp, pp->pt_inseg->seg_stp, &(ap->a_ray) );
	
			f = cv.crv_c1;
			f /= 64;
			if( f<0 )  f = -f;
			if( f > 1 )  f = 1;
			ap->a_color[0] = 1.0 - f;
			ap->a_color[1] = 0;

			f = cv.crv_c2;
			f /= 64;
			if( f<0 )  f = -f;
			if( f > 1 )  f = 1;
			ap->a_color[2] = 1.0 - f;
		}
		break;
	case 5:
	 	{
			LOCAL struct curvature cv;
			FAST fastf_t f;
			auto int ival;

			RT_CURVE( &cv, hitp, pp->pt_inseg->seg_stp, &(ap->a_ray) );

			ap->a_color[0] = (cv.crv_pdir[0] * (-.5)) + .5;
			ap->a_color[1] = (cv.crv_pdir[1] * (-.5)) + .5;
			ap->a_color[2] = (cv.crv_pdir[2] * (-.5)) + .5;
	 	}
		break;
	}

	if(rdebug&RDEBUG_HITS)  {
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

#if !defined(PARALLEL) && !defined(RTSRV)
	if( fbp != FBIO_NULL )  {
		RGBpixel p;
		p[RED] = r;
		p[GRN] = g;
		p[BLU] = b;
		fb_write( fbp, ap->a_x, ap->a_y, p, 1 );
	}
	if( outfp != NULL )  {
		if( hex_out )  {
			fprintf(outfp, "%2.2x%2.2x%2.2x\n", r, g, b);
		} else {
			unsigned char p[4];
			p[0] = r;
			p[1] = g;
			p[2] = b;
			if( fwrite( (char *)p, 3, 1, outfp ) != 1 )
				rt_bomb("pixel fwrite error");
		}
	}
#else PARALLEL or RTSRV
	{
		register char *pixelp;

		/* .pix files go bottom to top */
#ifdef RTSRV
		/* Here, the buffer is only one line long */
		pixelp = scanbuf+ap->a_x*3;
#else RTSRV
		pixelp = scanbuf+((ap->a_y*npts)+ap->a_x)*3;
#endif RTSRV
		/* Don't depend on interlocked hardware byte-splice */
		RES_ACQUIRE( &rt_g.res_worker );	/* XXX need extra semaphore */
		*pixelp++ = r ;
		*pixelp++ = g ;
		*pixelp++ = b ;
		RES_RELEASE( &rt_g.res_worker );
	}
#endif PARALLEL or RTSRV
	if(rdebug&RDEBUG_HITS) rt_log("rgb=%3d,%3d,%3d\n", r,g,b);
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

	if(rdebug&RDEBUG_HITS)  {
		rt_pr_pt(pp);
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

		if( pp->pt_outhit->hit_dist >= INFINITY ||
		    ap->a_level > MAX_BOUNCE )  {
		    	if( rdebug&RDEBUG_SHOWERR )  {
				VSET( ap->a_color, 9, 0, 0 );	/* RED */
				rt_log("colorview:  eye inside %s (x=%d, y=%d, lvl=%d)\n",
					pp->pt_inseg->seg_stp->st_name,
					ap->a_x, ap->a_y, ap->a_level);
		    	} else {
		    		VSETALL( ap->a_color, 0.18 );	/* 18% Grey */
		    	}
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

	if( rdebug&RDEBUG_RAYWRITE )  {
		/* Record the approach path */
		if( hitp->hit_dist > 0.0001 )  {
			wraypts( ap->a_ray.r_pt,
				hitp->hit_point,
				ap, stdout );
		}
	}

	/*
	 *  Call the material-handling function.
	 *  Note that only hit_dist is valid in pp_inhit.
	 *  ft_uv() routines must have hit_point computed
	 *  in advance, which is responsibility of reg_ufunc() routines.
	 *  RT_HIT_NORM() must also be called if hit_norm is needed,
	 *  after which pt_inflip must be handled.
	 *  These operations have been pushed down to the individual
	 *  material-handling functions for efficiency reasons,
	 *  because not all materials need the normals.
	 */
	{
		register struct mfuncs *mfp = pp->pt_regionp->reg_mfuncs;

		if( mfp == MF_NULL )  {
			rt_log("colorview:  reg_mfuncs NULL\n");
			return(0);
		}
		if( mfp->mf_magic != MF_MAGIC )  {
			rt_log("colorview:  reg_mfuncs bad magic, %x != %x\n",
				mfp->mf_magic, MF_MAGIC );
			return(0);
		}
		return( mfp->mf_render( ap, pp ) );
	}
}

/*
 *  			V I E W _ E O L
 *  
 *  This routine is called by main when the end of a scanline is
 *  reached.
 */
view_eol(ap)
register struct application *ap;
{
#ifdef PARALLEL
	if( fbp ==FBIO_NULL )
		return;
	/* We make no guarantee that the last few pixels are done */
	RES_ACQUIRE( &rt_g.res_syscall );
	fb_write( fbp, 0, ap->a_y, scanbuf+ap->a_y*npts*3, npts );
	RES_RELEASE( &rt_g.res_syscall );
#endif PARALLEL
}

/*
 *			V I E W _ E N D
 */
view_end(ap)
struct application *ap;
{
	register struct light_specific *lp, *nlp;

#ifdef PARALLEL
	if( fwrite( scanbuf, sizeof(char), npts*npts*3, outfp ) != npts*npts*3 )  {
		fprintf(stderr,"view_end:  fwrite failure\n");
		return(-1);		/* BAD */
	}
#endif PARALLEL

	/* Eliminate implicit lights */
	lp=LightHeadp;
	while( lp != LIGHT_NULL )  {
		if( lp->lt_explicit )  {
			/* will be cleaned by mlib_free() */
			lp = lp->lt_forw;
			continue;
		}
		nlp = lp->lt_forw;
		light_free( (char *)lp );
		lp = nlp;
	}
}

/*
 *  			V I E W _ I N I T
 *
 *  Called once, early on in RT setup.
 */
view_init( ap, file, obj, npts, minus_o )
register struct application *ap;
char *file, *obj;
{

#ifdef PARALLEL
	scanbuf = rt_malloc( npts*npts*3 + sizeof(long), "scanbuf" );
#endif

	mlib_init();			/* initialize material library */

	if( minus_o )  {
		/* Output is destined for a pixel file */
		return(0);		/* don't open framebuffer */
	}  else  {
		return(1);		/* open a framebuffer */
	}
}

/*
 *  			V I E W 2 _ I N I T
 *
 *  Called each time a new image is about to be done.
 */
view_2init( ap )
register struct application *ap;
{
	extern int hit_nothing();

	ap->a_miss = hit_nothing;
	ap->a_onehit = 1;

	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;
		/* If present, use user-specified light solid */
		if( LightHeadp == LIGHT_NULL )  {
			if(rdebug&RDEBUG_SHOWERR)rt_log("No explicit light\n");
			light_maker(1, view2model);
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		ap->a_hit = viewit;
		light_maker(3, view2model);
		break;
	default:
		rt_bomb("bad lighting model #");
	}
	light_init();
}
