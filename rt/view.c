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
#include "./material.h"
#include "./mathtab.h"
#include "./light.h"

char usage[] = "\
Usage:  rt [options] model.g objects...\n\
Options:\n\
 -s #		Grid size in pixels (default 512)\n\
 -a #		Azimuth in degrees\n\
 -e #		Elevation in degrees\n\
 -M		Read matrix, cmds on stdin\n\
 -o model.pix	Specify output file, .pix format (default=fb)\n\
 -x #		Set librt debug flags\n\
 -X #		Set rt debug flags\n\
 -p #		Perspective viewing, focal length scaling\n\
 -P #		Set number of processors (default 1)\n\
";

extern FBIO	*fbp;			/* Framebuffer handle */
extern FILE	*outfp;			/* optional output file */

extern int	perspective;
extern int	width;
extern int	height;
extern int	lightmodel;		/* lighting model # to use */
extern mat_t	view2model;
extern mat_t	model2view;
extern int	hex_out;		/* Output format, 0=binary, !0=hex */
extern char	*scanbuf;		/* Optional output buffer */

extern struct light_specific *LightHeadp;
vect_t ambient_color = { 1, 1, 1 };	/* Ambient white light */
extern double AmbientIntensity;

vect_t	background = { 0.25, 0, 0.5 };	/* Dark Blue Background */
int	ibackground[3];			/* integer 0..255 version */

#define MAX_IREFLECT	9	/* Maximum internal reflection level */
#define MAX_BOUNCE	4	/* Maximum recursion level */

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
	if( rdebug&RDEBUG_RAYPLOT )  {
		vect_t	out;

		VJOIN1( out, ap->a_ray.r_pt,
			10000, ap->a_ray.r_dir );	/* to imply direction */
		pl_color( stdout, 190, 0, 0 );
		rt_drawvec( stdout, ap->a_rt_i,
			ap->a_ray.r_pt, out );
	}
	ap->a_user = 0;		/* Signal view_pixel:  MISS */
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
		/* (For Spencer's moving light program) */
		ap->a_color[0] = (hitp->hit_normal[0] * (-.5)) + .5;
		ap->a_color[1] = (hitp->hit_normal[1] * (-.5)) + .5;
		ap->a_color[2] = (hitp->hit_normal[2] * (-.5)) + .5;
		break;
	case 4:
	 	{
			LOCAL struct curvature cv;
			FAST fastf_t f;
			auto int ival;

			RT_CURVE( &cv, hitp, pp->pt_inseg->seg_stp );
	
			f = cv.crv_c1;
			f *= 10;
			if( f < -0.5 )  f = -0.5;
			if( f > 0.5 )  f = 0.5;
			ap->a_color[0] = 0.5 + f;
			ap->a_color[1] = 0;

			f = cv.crv_c2;
			f *= 10;
			if( f < -0.5 )  f = -0.5;
			if( f > 0.5 )  f = 0.5;
			ap->a_color[2] = 0.5 + f;
		}
		break;
	case 5:
	 	{
			LOCAL struct curvature cv;
			FAST fastf_t f;
			auto int ival;

			RT_CURVE( &cv, hitp, pp->pt_inseg->seg_stp );

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
	ap->a_user = 1;		/* Signal view_pixel:  HIT */
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

	if( ap->a_user == 0 )  {
		/* Shot missed the model, don't dither */
		r = ibackground[0];
		g = ibackground[1];
		b = ibackground[2];
	} else {
		/*
		 *  To prevent bad color aliasing, add some color dither.
		 *  Be certain to NOT output the background color here.
		 */
		r = ap->a_color[0]*255.+rand_half();
		g = ap->a_color[1]*255.+rand_half();
		b = ap->a_color[2]*255.+rand_half();
		if( r > 255 ) r = 255;
		else if( r < 0 )  r = 0;
		if( g > 255 ) g = 255;
		else if( g < 0 )  g = 0;
		if( b > 255 ) b = 255;
		else if( b < 0 )  b = 0;
		if( r == ibackground[0] && g == ibackground[1] &&
		    b == ibackground[2] )  {
		    	register int i;
		    	int newcolor[3];
		    	/*  Find largest color channel to perterb.
		    	 *  It should happen infrequently.
		    	 *  If you have a faster algorithm, tell me.
		    	 */
		    	if( r > g )  {
		    		if( r > b )  i = 0;
		    		else i = 2;
		    	} else {
		    		if( g > b ) i = 1;
		    		else i = 2;
		    	}
		    	newcolor[0] = r;
		    	newcolor[1] = g;
		    	newcolor[2] = b;
			if( newcolor[i] < 127 ) newcolor[i]++;
		    	else newcolor[i]--;
		    	r = newcolor[0];
		    	g = newcolor[1];
		    	b = newcolor[2];
		}
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
		pixelp = scanbuf+((ap->a_y*width)+ap->a_x)*3;
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

static struct shadework shade_default = {
	0.0,				/* xmit */
	0.0,				/* reflect */
	1.0,				/* refractive index */
	1.0, 1.0, 1.0,			/* color: white */
	/* rest are zeros */
};

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
	register struct mfuncs *mfp;
	struct shadework sw;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("colorview:  no hit out front?\n");
		return(0);
	}
	hitp = pp->pt_inhit;

	if(rdebug&RDEBUG_HITS)  {
		rt_pr_pt( ap->a_rt_i, pp );
	}
	if( hitp->hit_dist >= INFINITY )  {
		rt_log("colorview:  entry beyond infinity\n");
		VSET( ap->a_color, .5, 0, 0 );
		ap->a_user = 1;		/* Signal view_pixel:  HIT */
		return(1);
	}

	/* Check to see if eye is "inside" the solid */
	/* It might only be worthwhile doing all this in perspective mode */
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
			ap->a_user = 1;		/* Signal view_pixel:  HIT */
			return(1);
		}
		/* Push on to exit point, and trace on from there */
		sub_ap = *ap;	/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		f = pp->pt_outhit->hit_dist+0.0001;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
		ap->a_user = rt_shootray( &sub_ap );	/* Signal view_pixel*/
		VSCALE( ap->a_color, sub_ap.a_color, 0.80 );
		return(1);
	}

	if( rdebug&RDEBUG_RAYWRITE )  {
		/* Record the approach path */
		if( hitp->hit_dist > 0.0001 )  {
			VJOIN1( hitp->hit_point, ap->a_ray.r_pt,
				hitp->hit_dist, ap->a_ray.r_dir );
			wraypts( ap->a_ray.r_pt,
				hitp->hit_point,
				ap, stdout );
		}
	}
	if( rdebug&RDEBUG_RAYPLOT )  {
		/* There are two parts to plot here.
		 *  Ray start to in hit, and inhit to outhit.
		 */
		if( hitp->hit_dist > 0.0001 )  {
			register int i, lvl;
			fastf_t out;
			vect_t inhit, outhit;

			lvl = ap->a_level % 100;
			if( lvl < 0 )  lvl = 0;
			else if( lvl > 3 )  lvl = 3;
			i = 255 - lvl * (128/4);

			VJOIN1( inhit, ap->a_ray.r_pt,
				hitp->hit_dist, ap->a_ray.r_dir );
			pl_color( stdout, i, 0, i );
			rt_drawvec( stdout, ap->a_rt_i,
				ap->a_ray.r_pt, inhit );

			if( (out = pp->pt_outhit->hit_dist) >= INFINITY )
				out = 10000;	/* to imply the direction */
			VJOIN1( outhit,
				ap->a_ray.r_pt, out,
				ap->a_ray.r_dir );
			pl_color( stdout, i, i, i );
			rt_drawvec( stdout, ap->a_rt_i,
				inhit, outhit );
		}
	}

	/*
	 *  Call the material-handling function.
	 *  Note that only hit_dist is valid in pp_inhit.
	 *  RT_HIT_NORM() must be called if hit_norm is needed,
	 *  after which pt_inflip must be handled.
	 *  ft_uv() routines must have hit_point computed
	 *  in advance.
	 */
	sw = shade_default;	/* struct copy */

	if( (mfp = (struct mfuncs *)pp->pt_regionp->reg_mfuncs) == MF_NULL )  {
		rt_log("colorview:  reg_mfuncs NULL\n");
		return(0);
	}
	if( mfp->mf_magic != MF_MAGIC )  {
		rt_log("colorview:  reg_mfuncs bad magic, %x != %x\n",
			mfp->mf_magic, MF_MAGIC );
		return(0);
	}

	/* Get color-by-ident for this region id */
	{
		register struct region *rp;
		if( (rp=pp->pt_regionp) == REGION_NULL )  {
			rt_log("bad region pointer\n");
			return(0);
		}
		if( rp->reg_mater.ma_override )  {
			VSET( sw.sw_color,
				rp->reg_mater.ma_color[0],
				rp->reg_mater.ma_color[1],
				rp->reg_mater.ma_color[2] );
		} else {
			/* Default color is white (uncolored) */
		}
	}

	sw.sw_hit = *(pp->pt_inhit);		/* struct copy */
	if( sw.sw_hit.hit_dist < 0.0 )
		sw.sw_hit.hit_dist = 0.0;	/* Eye inside solid */
	if( mfp->mf_inputs )  {
		VJOIN1( sw.sw_hit.hit_point, ap->a_ray.r_pt,
			sw.sw_hit.hit_dist, ap->a_ray.r_dir );
	}
	if( mfp->mf_inputs & MFI_NORMAL )  {
		if( pp->pt_inhit->hit_dist < 0.0 )  {
			/* Eye inside solid, orthoview */
			VREVERSE( sw.sw_hit.hit_normal, ap->a_ray.r_dir );
		} else {
			fastf_t f;
			/* Get surface normal for hit point */
			RT_HIT_NORM( &(sw.sw_hit), pp->pt_inseg->seg_stp,
				&(ap->a_ray) );

			/* Temporary check to make sure normals are OK */
			if( sw.sw_hit.hit_normal[X] < -1.01 || sw.sw_hit.hit_normal[X] > 1.01 ||
			    sw.sw_hit.hit_normal[Y] < -1.01 || sw.sw_hit.hit_normal[Y] > 1.01 ||
			    sw.sw_hit.hit_normal[Z] < -1.01 || sw.sw_hit.hit_normal[Z] > 1.01 )  {
			    	VPRINT("colorview: N", sw.sw_hit.hit_normal);
				VSET( ap->a_color, 9, 9, 0 );	/* Yellow */
				return(0);
			}
			if( pp->pt_inflip )  {
				VREVERSE( sw.sw_hit.hit_normal, sw.sw_hit.hit_normal );
				pp->pt_inflip = 0;	/* shouldnt be needed now??? */
			}
			/* More temporary checking */
			if( (f=VDOT( ap->a_ray.r_dir, sw.sw_hit.hit_normal )) > 0 )  {
				rt_log("colorview: flipped normal %d %d %s dot=%g\n",
					ap->a_x, ap->a_y,
					pp->pt_inseg->seg_stp->st_name, f);
				VPRINT("Dir ", ap->a_ray.r_dir);
				VPRINT("Norm", sw.sw_hit.hit_normal);
			}
		}
	}
	if( mfp->mf_inputs & MFI_UV )  {
		if( pp->pt_inhit->hit_dist < 0.0 )  {
			/* Eye inside solid, orthoview */
			sw.sw_uv.uv_u = sw.sw_uv.uv_v = 0.5;
			sw.sw_uv.uv_du = sw.sw_uv.uv_dv = 0;
		} else {
			rt_functab[pp->pt_inseg->seg_stp->st_id].ft_uv(
				ap, pp->pt_inseg->seg_stp,
				&(sw.sw_hit), &(sw.sw_uv) );
		}
		if( sw.sw_uv.uv_u < 0 || sw.sw_uv.uv_u > 1 ||
		    sw.sw_uv.uv_v < 0 || sw.sw_uv.uv_v > 1 )  {
			rt_log("colorview:  bad u,v=%g,%g du,dv=%g,%g seg=%s\n",
				sw.sw_uv.uv_u, sw.sw_uv.uv_v,
				sw.sw_uv.uv_du, sw.sw_uv.uv_dv,
				pp->pt_inseg->seg_stp->st_name );
			VSET( sw.sw_color, 0, 1, 0 );	/* Green */
			return(1);
		}
	}

	/* Invoke the actual shader (may be a tree of them) */
	(void)mfp->mf_render( ap, pp, &sw, pp->pt_regionp->reg_udata );

	/* As a special case for now, handle reflection & refraction */
	if( sw.sw_reflect > 0 || sw.sw_transmit > 0 )
		(void)rr_render( ap, pp, &sw, pp->pt_regionp->reg_udata );

	VMOVE( ap->a_color, sw.sw_color );
	ap->a_user = 1;		/* Signal view_pixel:  HIT */
	return(1);		/* "ret" isn't reliable yet */
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
	fb_write( fbp, 0, ap->a_y, scanbuf+ap->a_y*width*3, width );
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
	if( (outfp != NULL) &&
	    fwrite( scanbuf, sizeof(char), width*height*3, outfp ) != width*height*3 )  {
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
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{

#ifdef PARALLEL
	scanbuf = rt_malloc( width*height*3 + sizeof(long), "scanbuf" );
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
	case 2:
		VSETALL( background, 0 );	/* Neutral Normal */
		/* FALL THROUGH */
	case 1:
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

	ibackground[0] = background[0] * 255;
	ibackground[1] = background[1] * 255;
	ibackground[2] = background[2] * 255;
}
