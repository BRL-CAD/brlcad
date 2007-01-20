/*                          V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file view.c
 *
 *	Ray Tracing program, lighting model manager.
 *
 *  Output is either interactive to a frame buffer, or written in a file.
 *  The output format is a .PIX file (a byte stream of R,G,B as u_char's).
 *
 *  The extern "lightmodel" selects which one is being used:
 *	0	Full lighting model (default)
 *	1	1-light, from the eye.
 *	2	Spencer's surface-normals-as-colors display
 *	3	(removed)
 *	4	curvature debugging display (inv radius of curvature)
 *	5	curvature debugging (principal direction)
 *	6	UV Coord
 *	7	Photon Mapping
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
 */
#ifndef lint
static const char RCSview[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_UNIX_IO
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "fb.h"
#include "rtprivate.h"
#include "./ext.h"
#include "plot3.h"
#include "photonmap.h"

char usage[] = "\
Usage:  rt [options] model.g objects...\n\
Options:\n\
 -s #		Square grid size in pixels (default 512)\n\
 -w # -n #	Grid size width and height in pixels\n\
 -V #		View (pixel) aspect ratio (width/height)\n\
 -a #		Azimuth in deg\n\
 -e #		Elevation in deg\n\
 -M		Read matrix+cmds on stdin\n\
 -N #		NMG debug flags\n\
 -o model.pix	Output file, .pix format (default=fb)\n\
 -x #		librt debug flags\n\
 -X #		rt debug flags\n\
 -p #		Perspective, degrees side to side\n\
 -P #		Set number of processors\n\
 -T #/#		Tolerance: distance/angular\n\
 -r		Report overlaps\n\
 -R		Do not report overlaps\n\
";

int		use_air = 0;		/* Handling of air in librt */

extern FBIO	*fbp;			/* Framebuffer handle */

extern int	curframe;		/* from main.c */
extern fastf_t	frame_delta_t;		/* from main.c */
extern double	airdensity;		/* from opt.c */
extern double	haze[3];		/* from opt.c */

extern struct floatpixel	*curr_float_frame;	/* buffer of full frame */


vect_t ambient_color = { 1, 1, 1 };	/* Ambient white light */

int	ibackground[3] = {0};			/* integer 0..255 version */
int	inonbackground[3] = {0};		/* integer non-background */

#ifdef RTSRV
extern int	srv_startpix;		/* offset for view_pixel */
extern int	srv_scanlen;		/* BUFMODE_RTSRV buffer length */
extern char	*scanbuf;		/* scanline(s) buffer */
#endif

void		free_scanlines(void);

static int	buf_mode=0;
#define BUFMODE_UNBUF	1		/* No output buffering */
#define BUFMODE_DYNAMIC	2		/* Dynamic output buffering */
#define BUFMODE_INCR	3		/* incr_mode set, dynamic buffering */
#define BUFMODE_RTSRV	4		/* output buffering into scanbuf */
#define BUFMODE_FULLFLOAT 5		/* buffer entire frame as floats */
#define BUFMODE_SCANLINE 6		/* Like _DYNAMIC, one scanline/cpu */

static struct scanline {
	int	sl_left;		/* # pixels left on this scanline */
	char	*sl_buf;		/* ptr to buffer for scanline */
} *scanline;

static short int	pwidth;			/* Width of each pixel (in bytes) */

struct mfuncs *mfHead = MF_NULL;	/* Head of list of shaders */

fastf_t	gamma_corr = 0.0;			/* gamma correction if !0 */

/* The default a_onehit = -1 requires at least one non-air hit,
 * (stop at first surface) and stops ray/geometry intersection after that.
 * Set to 0 to turn off first hit optimization, with -c 'set a_onehit=0'
 */
int a_onehit = -1;

/*
 * Overlay
 *
 * If in overlay mode, and writeing to a framebuffer,
 * only write non-background pixels.
 */
static int overlay = 0;

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
/*XXX need to investigate why this doesn't work on Windows */
#if !defined(__alpha) && !defined(_WIN32) /* XXX Alpha does not support this initialization! */
	{"%f",	1, "gamma",	bu_byteoffset(gamma_corr),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "bounces",	bu_byteoffset(max_bounces),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "ireflect",	bu_byteoffset(max_ireflect),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "a_onehit",	bu_byteoffset(a_onehit),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f", ELEMENTS_PER_VECT, "background",bu_byteoffset(background[0]),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d", 1, "overlay",	bu_byteoffset(overlay),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d", 1, "ov", bu_byteoffset(overlay),	BU_STRUCTPARSE_FUNC_NULL },
#endif
	{"",	0, (char *)0,	0,				BU_STRUCTPARSE_FUNC_NULL }
};

/*
 *  			V I E W _ P I X E L
 *
 *  Arrange to have the pixel output.
 *  a_uptr has region pointer, for reference.
 */
void
view_pixel(register struct application *ap)
{
	register int	r,g,b;
	register char	*pixelp;
	register struct scanline	*slp;
	register int	do_eol = 0;
	unsigned char	dist[8];	/* pixel distance (in IEEE format) */

	if (rpt_dist)
	    htond(dist, (unsigned char *)&(ap->a_dist), 1);

	if( ap->a_user == 0 )  {
		/* Shot missed the model, don't dither */
		r = ibackground[0];
		g = ibackground[1];
		b = ibackground[2];
		VSETALL( ap->a_color, -1e-20 );	/* background flag */
	} else {
		/*
		 *  To prevent bad color aliasing, add some color dither.
		 *  Be certain to NOT output the background color here.
		 *  Random numbers in the range 0 to 1 are used, so
		 *  that integer valued colors (eg, from texture maps)
		 *  retain their original values.
		 */
		if( gamma_corr != 0 )  {
			/*
			 * Perform gamma correction in floating-point space,
			 * and avoid nasty mach bands in dark areas
			 * from doing it in 0..255 space later.
			 */
			double ex = 1.0/gamma_corr;
			r = pow(ap->a_color[0], ex)*255.+
				bn_rand0to1(ap->a_resource->re_randptr);
			g = pow(ap->a_color[1], ex)*255.+
				bn_rand0to1(ap->a_resource->re_randptr);
			b = pow(ap->a_color[2], ex)*255.+
				bn_rand0to1(ap->a_resource->re_randptr);
		} else {
			r = ap->a_color[0]*255.+bn_rand0to1(ap->a_resource->re_randptr);
			g = ap->a_color[1]*255.+bn_rand0to1(ap->a_resource->re_randptr);
			b = ap->a_color[2]*255.+bn_rand0to1(ap->a_resource->re_randptr);
		}
		if( r > 255 ) r = 255;
		else if( r < 0 )  r = 0;
		if( g > 255 ) g = 255;
		else if( g < 0 )  g = 0;
		if( b > 255 ) b = 255;
		else if( b < 0 )  b = 0;
		if( r == ibackground[0] && g == ibackground[1] &&
		    b == ibackground[2] )  {
		    	r = inonbackground[0];
		    	g = inonbackground[1];
		    	b = inonbackground[2];
		}

		/* Make sure it's never perfect black */
		if (r==0 && g==0 && b==0 && benchmark==0)
		  b = 1;
	}

	if(R_DEBUG&RDEBUG_HITS) bu_log("rgb=%3d,%3d,%3d xy=%3d,%3d (%g,%g,%g)\n",
		r,g,b, ap->a_x, ap->a_y,
		V3ARGS(ap->a_color) );

	switch( buf_mode )  {

	case BUFMODE_FULLFLOAT:
		{
			/* No output semaphores required for word-width memory writes */
			register struct floatpixel	*fp;
			fp = &curr_float_frame[ap->a_y*width + ap->a_x];
			fp->ff_frame = curframe;
			fp->ff_color[0] = r;
			fp->ff_color[1] = g;
			fp->ff_color[2] = b;
			fp->ff_x = ap->a_x;
			fp->ff_y = ap->a_y;
			if( ap->a_user == 0 )  {
				fp->ff_dist = -INFINITY;	/* shot missed model */
				fp->ff_frame = -1;		/* Don't cache misses */
				return;
			}
			/* XXX a_dist is negative and misleading when eye is in air */
			fp->ff_dist = (float)ap->a_dist;
			VJOIN1( fp->ff_hitpt, ap->a_ray.r_pt,
				ap->a_dist, ap->a_ray.r_dir );
			fp->ff_regp = (struct region *)ap->a_uptr;
			RT_CK_REGION(fp->ff_regp);
#if 0
{
	point_t	new_view_pt;
	MAT4X3PNT( new_view_pt, model2view, fp->ff_hitpt );
}
#endif
			/*
			 *  This pixel was just computed.
			 *  Look at next pixel on scanline,
			 *  and if it is a reprojected old value
			 *  and hit a different region than this pixel,
			 *  then recompute it too.
			 */
			if( ap->a_x >= width-1 )  return;
			if( fp[1].ff_frame <= 0 )  return;	/* not valid, will be recomputed. */
			if( fp[1].ff_regp == fp->ff_regp )
				return;				/* OK */
			/* Next pixel is probably out of date, mark it for re-computing */
			fp[1].ff_frame = -1;
			return;
		}

	case BUFMODE_UNBUF:
		{
			RGBpixel	p;
			int		npix;

			p[0] = r ;
			p[1] = g ;
			p[2] = b ;

			if( outfp != NULL )  {
				bu_semaphore_acquire( BU_SEM_SYSCALL );
				if( fseek( outfp, (ap->a_y*width*pwidth) + (ap->a_x*pwidth), 0 ) != 0 )
					fprintf(stderr, "fseek error\n");
				if( fwrite( p, 3, 1, outfp ) != 1 )
					rt_bomb("pixel fwrite error");
				if( rpt_dist &&
				    ( fwrite( dist, 8, 1, outfp ) != 1 ))
					rt_bomb("pixel fwrite error");
				bu_semaphore_release( BU_SEM_SYSCALL);
			}

			if( fbp != FBIO_NULL )  {
				/* Framebuffer output */
				bu_semaphore_acquire( BU_SEM_SYSCALL );
				npix = fb_write( fbp, ap->a_x, ap->a_y,
					(unsigned char *)p, 1 );
				bu_semaphore_release( BU_SEM_SYSCALL);
				if( npix < 1 )  rt_bomb("pixel fb_write error");
			}
		}
		return;

#ifdef RTSRV
	case BUFMODE_RTSRV:
		/* Multi-pixel buffer */
		pixelp = scanbuf+ pwidth *
			((ap->a_y*width) + ap->a_x - srv_startpix);
		bu_semaphore_acquire( RT_SEM_RESULTS );
		*pixelp++ = r ;
		*pixelp++ = g ;
		*pixelp++ = b ;
		if (rpt_dist)
		{
		    *pixelp++ = dist[0];
		    *pixelp++ = dist[1];
		    *pixelp++ = dist[2];
		    *pixelp++ = dist[3];
		    *pixelp++ = dist[4];
		    *pixelp++ = dist[5];
		    *pixelp++ = dist[6];
		    *pixelp++ = dist[7];
		}
		bu_semaphore_release( RT_SEM_RESULTS );
		return;
#endif

	/*
	 *  Store results into pixel buffer.
	 *  Don't depend on interlocked hardware byte-splice.
	 *  Need to protect scanline[].sl_left when in parallel mode.
	 */

	case BUFMODE_DYNAMIC:
		slp = &scanline[ap->a_y];
		bu_semaphore_acquire( RT_SEM_RESULTS );
		if( slp->sl_buf == (char *)0 )  {
			slp->sl_buf = bu_calloc( width, pwidth, "sl_buf scanline buffer" );
		}
		pixelp = slp->sl_buf+(ap->a_x*pwidth);
		*pixelp++ = r ;
		*pixelp++ = g ;
		*pixelp++ = b ;
		if (rpt_dist)
		{
		    *pixelp++ = dist[0];
		    *pixelp++ = dist[1];
		    *pixelp++ = dist[2];
		    *pixelp++ = dist[3];
		    *pixelp++ = dist[4];
		    *pixelp++ = dist[5];
		    *pixelp++ = dist[6];
		    *pixelp++ = dist[7];
		}
		if( --(slp->sl_left) <= 0 )
			do_eol = 1;
		bu_semaphore_release( RT_SEM_RESULTS );
		break;

	/*
	 *  Only one CPU is working on this scanline,
	 *  no parallel interlock required!  Much faster.
	 */
	case BUFMODE_SCANLINE:
		slp = &scanline[ap->a_y];
		if( slp->sl_buf == (char *)0 )  {
			slp->sl_buf = bu_calloc( width, pwidth, "sl_buf scanline buffer" );
		}
		pixelp = slp->sl_buf+(ap->a_x*pwidth);
		*pixelp++ = r ;
		*pixelp++ = g ;
		*pixelp++ = b ;
		if (rpt_dist)
		{
		    *pixelp++ = dist[0];
		    *pixelp++ = dist[1];
		    *pixelp++ = dist[2];
		    *pixelp++ = dist[3];
		    *pixelp++ = dist[4];
		    *pixelp++ = dist[5];
		    *pixelp++ = dist[6];
		    *pixelp++ = dist[7];
		}
		if( --(slp->sl_left) <= 0 )
			do_eol = 1;
		break;

	case BUFMODE_INCR:
		{
			register int dx,dy;
			register int spread;

			spread = 1<<(incr_nlevel-incr_level);

			bu_semaphore_acquire( RT_SEM_RESULTS );
			for( dy=0; dy<spread; dy++ )  {
				if( ap->a_y+dy >= height )  break;
				slp = &scanline[ap->a_y+dy];
				if( slp->sl_buf == (char *)0 )
					slp->sl_buf = bu_calloc( width+32,
						pwidth, "sl_buf scanline buffer" );

				pixelp = slp->sl_buf+(ap->a_x*pwidth);
				for( dx=0; dx<spread; dx++ )  {
					*pixelp++ = r ;
					*pixelp++ = g ;
					*pixelp++ = b ;
					if (rpt_dist)
					{
					    *pixelp++ = dist[0];
					    *pixelp++ = dist[1];
					    *pixelp++ = dist[2];
					    *pixelp++ = dist[3];
					    *pixelp++ = dist[4];
					    *pixelp++ = dist[5];
					    *pixelp++ = dist[6];
					    *pixelp++ = dist[7];
					}
				}
			}
			/* First 3 incremental iterations are boring */
			if( incr_level > 3 )  {
				if( --(scanline[ap->a_y].sl_left) <= 0 )
					do_eol = 1;
			}
			bu_semaphore_release( RT_SEM_RESULTS );
		}
		break;

	default:
		rt_bomb("bad buf_mode");
	}


	if( !do_eol )  return;

	switch( buf_mode )  {
	case BUFMODE_INCR:
		if( fbp == FBIO_NULL )  rt_bomb("Incremental rendering with no framebuffer?");
		{
			register int dy, yy;
			register int spread;
			int		npix = 0;

			spread = (1<<(incr_nlevel-incr_level))-1;
			bu_semaphore_acquire( BU_SEM_SYSCALL );
			for( dy=spread; dy >= 0; dy-- )  {
				yy = ap->a_y + dy;
				if( sub_grid_mode )  {
					if( dy < sub_ymin || dy > sub_ymax )
						continue;
					npix = fb_write( fbp, sub_xmin, yy,
						(unsigned char *)scanline[yy].sl_buf+3*sub_xmin,
						sub_xmax-sub_xmin+1 );
					if( npix != sub_xmax-sub_xmin+1 )  break;
				} else {
					npix = fb_write( fbp, 0, yy,
						(unsigned char *)scanline[yy].sl_buf,
						width );
					if( npix != width )  break;
				}
			}
			bu_semaphore_release( BU_SEM_SYSCALL);
			if( npix != width )  rt_bomb("fb_write error (incremental res)");
		}
		break;

	case BUFMODE_SCANLINE:
	case BUFMODE_DYNAMIC:
		if( fbp != FBIO_NULL )  {
			int		npix;
			bu_semaphore_acquire( BU_SEM_SYSCALL );
			if( sub_grid_mode )  {
				npix = fb_write( fbp, sub_xmin, ap->a_y,
				    (unsigned char *)scanline[ap->a_y].sl_buf+3*sub_xmin,
				    sub_xmax-sub_xmin+1 );
			} else {
				npix = fb_write( fbp, 0, ap->a_y,
				    (unsigned char *)scanline[ap->a_y].sl_buf, width );
			}
			bu_semaphore_release( BU_SEM_SYSCALL);
			if( sub_grid_mode )  {
				if( npix < sub_xmax-sub_xmin-1 )  rt_bomb("scanline fb_write error");
			} else {
				if( npix < width )  rt_bomb("scanline fb_write error");
			}
		}
		if( outfp != NULL )  {
			int	count;

			bu_semaphore_acquire( BU_SEM_SYSCALL );
			if( fseek( outfp, ap->a_y*width*pwidth, 0 ) != 0 )
				fprintf(stderr, "fseek error\n");
			count = fwrite( scanline[ap->a_y].sl_buf,
				sizeof(char), width*pwidth, outfp );
			bu_semaphore_release( BU_SEM_SYSCALL);
			if( count != width*pwidth )
				rt_bomb("view_pixel:  fwrite failure\n");
		}
		bu_free( scanline[ap->a_y].sl_buf, "sl_buf scanline buffer" );
		scanline[ap->a_y].sl_buf = (char *)0;
	}
}

/*
 *  			V I E W _ E O L
 *
 *  This routine is not used;  view_pixel() determines when the last
 *  pixel of a scanline is really done, for parallel considerations.
 */
void
view_eol(register struct application *ap)
{
	return;
}

/*
 *			V I E W _ E N D
 */
void
view_end(struct application *ap)
{
	if( fullfloat_mode )  {
		struct floatpixel	*tmp;
		/* Transmitting scanlines, is done by rtsync before calling here. */
		/* Exchange previous and current buffers.  No freeing. */
		if( reproject_mode != 2 )  {
			tmp = prev_float_frame;
			prev_float_frame = curr_float_frame;
			curr_float_frame = tmp;
		}
	}

	if( scanline )  free_scanlines();
}

/*
 *			V I E W _ S E T U P
 *
 *  Called before rt_prep() in do.c
 */
void
view_setup(struct rt_i *rtip)
{
	register struct region *regp;

	RT_CHECK_RTI(rtip);
	/*
	 *  Initialize the material library for all regions.
	 *  As this may result in some regions being dropped,
	 *  (eg, light solids that become "implicit" -- non drawn),
	 *  this must be done before allowing the library to prep
	 *  itself.  This is a slight layering violation;  later it
	 *  may be clear how to repackage this operation.
	 */
	regp = BU_LIST_FIRST( region, &rtip->HeadRegion );
	while( BU_LIST_NOT_HEAD( regp, &rtip->HeadRegion ) )  {
		switch( mlib_setup( &mfHead, regp, rtip ) )  {
		case -1:
		default:
			bu_log("mlib_setup failure on %s\n", regp->reg_name);
			break;
		case 0:
			if(R_DEBUG&RDEBUG_MATERIAL)
				bu_log("mlib_setup: drop region %s\n", regp->reg_name);
			{
				struct region *r = BU_LIST_NEXT( region, &regp->l );
				/* zap reg_udata? beware of light structs */
				rt_del_regtree( rtip, regp, &rt_uniresource );
				regp = r;
				continue;
			}
		case 1:
			/* Full success */
			if( R_DEBUG&RDEBUG_MATERIAL &&
			    ((struct mfuncs *)(regp->reg_mfuncs))->mf_print )  {
				((struct mfuncs *)(regp->reg_mfuncs))->
					mf_print( regp, regp->reg_udata );
			}
			/* Perhaps this should be a function? */
			break;
	        case 2:
			/* Full success, and this region should get dropped later */
			/* Add to list of regions to drop */
			bu_ptbl_ins( &rtip->delete_regs, (long *)regp );
			break;
		}
		regp = BU_LIST_NEXT( region, &regp->l );
	}
}

/*
 *			V I E W _ R E _ S E T U P
 *
 *	This routine is used to do a "mlib_setup" on reprepped regions.
 *	only regions with a NULL reg_mfuncs pointer will be processed.
 */
void
view_re_setup( struct rt_i *rtip )
{
	struct region *rp;

	rp = BU_LIST_FIRST( region, &(rtip->HeadRegion) );
	while( BU_LIST_NOT_HEAD( rp, &(rtip->HeadRegion) ) ) {
		if( !rp->reg_mfuncs ) {
			switch( mlib_setup( &mfHead, rp, rtip ) ) {
			default:
			case -1:
				bu_log( "view_re_setup(): mlib_setup failed for region %s\n", rp->reg_name );
				break;
			case 0:
				{
					struct region *r = BU_LIST_NEXT( region, &rp->l );
					/* zap reg_udata? beware of light structs */
					rt_del_regtree( rtip, rp, &rt_uniresource );
					rp = r;
					continue;
				}
			case 1:
				break;
			}
		}
		rp = BU_LIST_NEXT( region, &rp->l );
	}
}

/*
 *			V I E W _ C L E A N U P
 *
 *  Called before rt_clean() in do.c
 */
void view_cleanup(struct rt_i	*rtip)
{
	register struct region	*regp;

	RT_CHECK_RTI(rtip);
	for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
		mlib_free( regp );
	}
	if( env_region.reg_mfuncs )  {
		bu_free( (char *)env_region.reg_name, "env_region.reg_name" );
		env_region.reg_name = (char *)0;
		mlib_free( &env_region );
	}

	light_cleanup();
}

/*
 *			H I T _ N O T H I N G
 *
 *  a_miss() routine called when no part of the model is hit.
 *  Background texture mapping could be done here.
 *  For now, return a pleasant dark blue.
 */
static int hit_nothing(register struct application *ap)
{
	if( R_DEBUG&RDEBUG_MISSPLOT )  {
		vect_t	out;

		/* XXX length should be 1 model diameter */
		VJOIN1( out, ap->a_ray.r_pt,
			10000, ap->a_ray.r_dir );	/* to imply direction */
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		pl_color( stdout, 190, 0, 0 );
		pdv_3line( stdout, ap->a_ray.r_pt, out );
		bu_semaphore_release( BU_SEM_SYSCALL );
	}

	if( env_region.reg_mfuncs )  {
		struct gunk {
			struct partition part;
			struct hit	hit;
			struct shadework sw;
		} u;

		memset( (char *)&u, 0, sizeof(u) );
		/* Make "miss" hit the environment map */
		/* Build up the fakery */
		u.part.pt_magic = PT_MAGIC;
		u.part.pt_inhit = u.part.pt_outhit = &u.hit;
		u.part.pt_regionp = &env_region;
		u.hit.hit_magic = RT_HIT_MAGIC;
		u.hit.hit_dist = ap->a_rt_i->rti_radius * 2;	/* model diam */
		u.hit.hit_rayp = &ap->a_ray;

		u.sw.sw_transmit = u.sw.sw_reflect = 0.0;
		u.sw.sw_refrac_index = 1.0;
		u.sw.sw_extinction = 0;
		u.sw.sw_xmitonly = 1;		/* don't shade env map! */

		/* "Surface" Normal points inward, UV is azim/elev of ray */
		u.sw.sw_inputs = MFI_NORMAL|MFI_UV;
		VREVERSE( u.sw.sw_hit.hit_normal, ap->a_ray.r_dir );
		/* U is azimuth, atan() range: -pi to +pi */
		u.sw.sw_uv.uv_u = bn_atan2( ap->a_ray.r_dir[Y],
			ap->a_ray.r_dir[X] ) * bn_inv2pi;
		if( u.sw.sw_uv.uv_u < 0 )
			u.sw.sw_uv.uv_u += 1.0;
		/*
		 *  V is elevation, atan() range: -pi/2 to +pi/2,
		 *  because sqrt() ensures that X parameter is always >0
		 */
		u.sw.sw_uv.uv_v = bn_atan2( ap->a_ray.r_dir[Z],
			sqrt( ap->a_ray.r_dir[X] * ap->a_ray.r_dir[X] +
			ap->a_ray.r_dir[Y] * ap->a_ray.r_dir[Y]) ) *
			bn_invpi + 0.5;
		u.sw.sw_uv.uv_du = u.sw.sw_uv.uv_dv = 0;

		VSETALL( u.sw.sw_color, 1 );
		VSETALL( u.sw.sw_basecolor, 1 );

		if (R_DEBUG&RDEBUG_SHADE)
			bu_log("hit_nothing calling viewshade\n");

		(void)viewshade( ap, &u.part, &u.sw );

		VMOVE( ap->a_color, u.sw.sw_color );
		ap->a_user = 1;		/* Signal view_pixel:  HIT */
		ap->a_uptr = (genptr_t)&env_region;
		return(1);
	}

	ap->a_user = 0;		/* Signal view_pixel:  MISS */
	VMOVE( ap->a_color, background );	/* In case someone looks */
	return(0);
}

/*
 *			C O L O R V I E W
 *
 *  Manage the coloring of whatever it was we just hit.
 *  This can be a recursive procedure.
 */
int
colorview(register struct application *ap, struct partition *PartHeadp, struct seg *finished_segs)
{
	register struct partition *pp;
	register struct hit *hitp;
	struct shadework sw;

	pp = PartHeadp->pt_forw;
	if( ap->a_flag == 1 )
	{
		/* This ray is an escaping internal ray after refraction through glass.
		 * Sometimes, after refraction and starting a new ray at the glass exit,
		 * the new ray hits a sliver of the same glass, and gets confused. This bit
		 * of code attempts to spot this behavior and skip over the glass sliver.
		 * Any sliver less than 0.05mm thick will be skipped (0.05 is a SWAG).
		 */
		if( (genptr_t)pp->pt_regionp == ap->a_uptr &&
			pp->pt_forw != PartHeadp &&
			pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist < 0.05 )
				pp = pp->pt_forw;
	}

	for( ; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 ) break;

	if( pp == PartHeadp )  {
		bu_log("colorview:  no hit out front?\n");
		return(0);
	}


	RT_CK_PT(pp);
	hitp = pp->pt_inhit;
	RT_CK_HIT(hitp);
	RT_CK_RAY(hitp->hit_rayp);
	ap->a_uptr = (genptr_t)pp->pt_regionp;	/* note which region was shaded */

	if(R_DEBUG&RDEBUG_HITS)  {
		bu_log("colorview: lvl=%d coloring %s\n",
			ap->a_level,
			pp->pt_regionp->reg_name);
		rt_pr_pt( ap->a_rt_i, pp );
	}
	if( hitp->hit_dist >= INFINITY )  {
		bu_log("colorview:  entry beyond infinity\n");
		VSET( ap->a_color, .5, 0, 0 );
		ap->a_user = 1;		/* Signal view_pixel:  HIT */
		ap->a_dist = hitp->hit_dist;
		goto out;
	}

	/* Check to see if eye is "inside" the solid
	 * It might only be worthwhile doing all this in perspective mode
	 * XXX Note that hit_dist can be faintly negative, e.g. -1e-13
	 *
	 * XXX we should certainly only do this if the eye starts out inside
	 *  an opaque solid.  If it starts out inside glass or air we don't
	 *  really want to do this
	 */

	if( hitp->hit_dist < 0.0 && pp->pt_regionp->reg_aircode == 0 ) {
		struct application sub_ap;
		FAST fastf_t f;

		if( pp->pt_outhit->hit_dist >= INFINITY ||
		    ap->a_level > max_bounces )  {
		    	if( R_DEBUG&RDEBUG_SHOWERR )  {
				VSET( ap->a_color, 9, 0, 0 );	/* RED */
				bu_log("colorview:  eye inside %s (x=%d, y=%d, lvl=%d)\n",
					pp->pt_regionp->reg_name,
					ap->a_x, ap->a_y, ap->a_level);
		    	} else {
		    		VSETALL( ap->a_color, 0.18 );	/* 18% Grey */
		    	}
			ap->a_user = 1;		/* Signal view_pixel:  HIT */
			ap->a_dist = hitp->hit_dist;
			goto out;
		}
		/* Push on to exit point, and trace on from there */
		sub_ap = *ap;	/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		f = pp->pt_outhit->hit_dist+hitp->hit_dist+0.0001;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
		sub_ap.a_purpose = "pushed eye position";
		(void)rt_shootray( &sub_ap );

		/* The eye is inside a solid and we are "Looking out" so
		 * we are going to darken what we see beyond to give a visual
		 * cue that something is wrong.
		 */
		VSCALE( ap->a_color, sub_ap.a_color, 0.80 );

		ap->a_user = 1;		/* Signal view_pixel: HIT */
		ap->a_dist = f + sub_ap.a_dist;
		ap->a_uptr = sub_ap.a_uptr;	/* which region */
		goto out;
	}

	if( R_DEBUG&RDEBUG_RAYWRITE )  {
		/* Record the approach path */
		if( hitp->hit_dist > 0.0001 )  {
			VJOIN1( hitp->hit_point, ap->a_ray.r_pt,
				hitp->hit_dist, ap->a_ray.r_dir );
			wraypts( ap->a_ray.r_pt,
				ap->a_ray.r_dir,
				hitp->hit_point,
				-1, ap, stdout );	/* -1 = air */
		}
	}
	if( R_DEBUG&(RDEBUG_RAYPLOT|RDEBUG_RAYWRITE|RDEBUG_REFRACT) )  {
		/*  There are two parts to plot here.
		 *  Ray start to inhit (purple),
		 *  and inhit to outhit (grey).
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
			if( R_DEBUG&RDEBUG_RAYPLOT )  {
				bu_semaphore_acquire( BU_SEM_SYSCALL );
				pl_color( stdout, i, 0, i );
				pdv_3line( stdout, ap->a_ray.r_pt, inhit );
				bu_semaphore_release( BU_SEM_SYSCALL );
			}
			bu_log("From ray start to inhit (purple):\n \
vdraw open oray;vdraw params c %2.2x%2.2x%2.2x;vdraw write n 0 %g %g %g;vdraw write n 1 %g %g %g;vdraw send\n",
				i, 0, i,
				V3ARGS(ap->a_ray.r_pt),
				V3ARGS(inhit) );

			if( (out = pp->pt_outhit->hit_dist) >= INFINITY )
				out = 10000;	/* to imply the direction */
			VJOIN1( outhit,
				ap->a_ray.r_pt, out,
				ap->a_ray.r_dir );
			if( R_DEBUG&RDEBUG_RAYPLOT )  {
				bu_semaphore_acquire( BU_SEM_SYSCALL );
				pl_color( stdout, i, i, i );
				pdv_3line( stdout, inhit, outhit );
				bu_semaphore_release( BU_SEM_SYSCALL );
			}
			bu_log("From inhit to outhit (grey):\n \
vdraw open iray;vdraw params c %2.2x%2.2x%2.2x;vdraw write n 0 %g %g %g;vdraw write n 1 %g %g %g;vdraw send\n",
				i, i, i,
				V3ARGS(inhit), V3ARGS(outhit) );
		}
	}

	memset( (char *)&sw, 0, sizeof(sw) );
	sw.sw_transmit = sw.sw_reflect = 0.0;
	sw.sw_refrac_index = 1.0;
	sw.sw_extinction = 0;
	sw.sw_xmitonly = 0;		/* want full data */
	sw.sw_inputs = 0;		/* no fields filled yet */
	sw.sw_frame = curframe;
	sw.sw_pixeltime = sw.sw_frametime = curframe * frame_delta_t;
	sw.sw_segs = finished_segs;
	VSETALL( sw.sw_color, 1 );
	VSETALL( sw.sw_basecolor, 1 );

	if (R_DEBUG&RDEBUG_SHADE)
		bu_log("colorview calling viewshade\n");

	/* individual shaders must handle reflection & refraction */
	(void)viewshade( ap, pp, &sw );

	VMOVE( ap->a_color, sw.sw_color );
	ap->a_user = 1;		/* Signal view_pixel:  HIT */
	/* XXX This is always negative when eye is inside air solid */
	ap->a_dist = hitp->hit_dist;

out:
	/*
	 *  e ^(-density * distance)
	 */
	if (airdensity != 0.0) {
	    double g;
	    double f = exp(-hitp->hit_dist * airdensity);
	    g = (1.0 - f);

	    VSCALE(ap->a_color, ap->a_color, f);
	    VJOIN1(ap->a_color, ap->a_color, g, haze);
	}
	RT_CK_REGION(ap->a_uptr);
	if(R_DEBUG&RDEBUG_HITS)  {
		bu_log("colorview: lvl=%d ret a_user=%d %s\n",
			ap->a_level,
			ap->a_user,
			pp->pt_regionp->reg_name);
		VPRINT("color   ", ap->a_color);
	}
	return(1);
}



/*
 *			V I E W I T
 *
 *  a_hit() routine for simple lighting model.
 */
int viewit(register struct application *ap,
	   struct partition *PartHeadp,
	   struct seg	*segHeadp)
{
	register struct partition *pp;
	register struct hit *hitp;
	LOCAL fastf_t	diffuse0 = 0;
	LOCAL fastf_t	cosI0 = 0;
	LOCAL vect_t work0, work1;
	LOCAL struct light_specific *lp;
	vect_t		normal;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		bu_log("viewit:  no hit out front?\n");
		return(0);
	}
	hitp = pp->pt_inhit;
	RT_HIT_NORMAL( normal, hitp, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );

	/*
	 * Diffuse reflectance from each light source
	 */
	switch( lightmodel )  {
	case 1:
		/* Light from the "eye" (ray source).  Note sign change */
		lp = BU_LIST_FIRST( light_specific, &(LightHead.l) );
		diffuse0 = 0;
		if( (cosI0 = -VDOT(normal, ap->a_ray.r_dir)) >= 0.0 )
			diffuse0 = cosI0 * ( 1.0 - AmbientIntensity);
		VSCALE( work0, lp->lt_color, diffuse0 );

		/* Add in contribution from ambient light */
		VSCALE( work1, ambient_color, AmbientIntensity );
		VADD2( ap->a_color, work0, work1 );
		break;
	case 2:
		/* Store surface normals pointing inwards */
		/* (For Spencer's moving light program) */
		ap->a_color[0] = (normal[0] * (-.5)) + .5;
		ap->a_color[1] = (normal[1] * (-.5)) + .5;
		ap->a_color[2] = (normal[2] * (-.5)) + .5;
		break;
	case 4:
	 	{
			LOCAL struct curvature cv;
			FAST fastf_t f;

			RT_CURVATURE( &cv, hitp, pp->pt_inflip, pp->pt_inseg->seg_stp );

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

			RT_CURVATURE( &cv, hitp, pp->pt_inflip, pp->pt_inseg->seg_stp );

			ap->a_color[0] = (cv.crv_pdir[0] * (-.5)) + .5;
			ap->a_color[1] = (cv.crv_pdir[1] * (-.5)) + .5;
			ap->a_color[2] = (cv.crv_pdir[2] * (-.5)) + .5;
	 	}
		break;
	case 6:
		{
			LOCAL struct uvcoord uv;

			/* Exactly like 'testmap' shader: UV debug */
			RT_HIT_UVCOORD( ap, pp->pt_inseg->seg_stp, hitp, &uv );

			BU_ASSERT( uv.uv_u >= 0 );
			BU_ASSERT( uv.uv_u <= 1 );
			BU_ASSERT( uv.uv_v >= 0 );
			BU_ASSERT( uv.uv_v <= 1 );

			VSET( ap->a_color, uv.uv_u, 0, uv.uv_v );
		}
		break;
	case 7:
		{
		}
		break;
	}

	if(R_DEBUG&RDEBUG_HITS)  {
		rt_pr_hit( " In", hitp );
		bu_log("cosI0=%f, diffuse0=%f   ", cosI0, diffuse0 );
		VPRINT("RGB", ap->a_color);
	}
	ap->a_user = 1;		/* Signal view_pixel:  HIT */
	return(0);
}

void
free_scanlines(void)
{
	register int	y;

	for( y=0; y<height; y++ )  {
		if( scanline[y].sl_buf )  {
			bu_free( scanline[y].sl_buf, "sl_buf scanline buffer" );
			scanline[y].sl_buf = (char *)0;
		}
	}
	bu_free( (char *)scanline, "struct scanline[height]" );
	scanline = (struct scanline *)0;
}

/*
 *  			V I E W _ I N I T
 *
 *  Called once, early on in RT setup, before view size is set.
 */
int
view_init(register struct application *ap, char *file, char *obj, int minus_o, int minus_F)
{
	if (rt_verbosity & VERBOSE_LIBVERSIONS)
		bu_log("%s", liboptical_version+5);

	optical_shader_init(&mfHead);	/* in liboptical/init.c */

	if ( rpt_dist && !minus_o )  {
	    bu_log("Warning: -d ignored.  Writing to frame buffer only\n");
	    rpt_dist = 0;
	}

	if (minus_F || (!minus_o && !minus_F)) {
	    return 1;		/* open a framebuffer */
	}
	return 0;
}

/*
 *			R E P R O J E C T _ S P L A T
 *
 *  Called when the reprojected value lies on the current screen.
 *  Write the reprojected value into the screen,
 *  checking *screen* Z values if the new location is already occupied.
 *
 *  May be run in parallel.
 */
int	rt_scr_lim_dist_sq = 100;	/* dist**2 pixels allowed to move */

int
reproject_splat(int ix, int iy, register struct floatpixel *ip, const fastf_t *new_view_pt)
{
	register struct floatpixel	*op;
	int	count = 1;
#if 0
	static int foo;
#endif

	/* Reprojection lies on screen, see if dest pixel already occupied */
	op = &curr_float_frame[iy*width + ix];

	/* Don't reproject again if new val is more distant */
	if( op->ff_frame >= 0 )  {
		point_t o_pt;
		/* Recompute both distances from current eye_pt! */
		/* Inefficient, only need Z component. */
		MAT4X3PNT(o_pt, model2view, op->ff_hitpt);
#if 0
		if( foo != curframe )  {
			extern int print_on;
			foo = curframe;
			print_on = 1;
			bu_log("  ip=(%g,%g,%g) ip_view=(%g,%g,%g)\n  op=(%g,%g,%g), o_pt=(%g,%g,%g)\n",
				V3ARGS(ip->ff_hitpt), V3ARGS(new_view_pt),
				V3ARGS(op->ff_hitpt), V3ARGS(o_pt) );
		}
#endif
		if( o_pt[Z] > new_view_pt[Z] )
			return 0;	/* previous val closer to eye, leave it be. */
		else
			count = 0;	/* Already reproj, don't double-count */
	}

	/* re-use old pixel as new pixel */
	*op = *ip;	/* struct copy */

	return count;
}

/* Local communication a.la. worker() */
extern int per_processor_chunk;	/* how many pixels to do at once */
extern int cur_pixel;		/* current pixel number, 0..last_pixel */
extern int last_pixel;		/* last pixel number */

/*
 *			R E P R O J E C T _ W O R K E R
 */
void
reproject_worker(int cpu, genptr_t arg)
{
	int	pixel_start;
	int	pixelnum;
	register struct floatpixel	*ip;
	int	count = 0;

	/* The more CPUs at work, the bigger the bites we take */
	if( per_processor_chunk <= 0 )  per_processor_chunk = npsw;

	while(1)  {

		bu_semaphore_acquire( RT_SEM_WORKER );
		pixel_start = cur_pixel;
		cur_pixel += per_processor_chunk;
		bu_semaphore_release( RT_SEM_WORKER );

		for( pixelnum = pixel_start; pixelnum < pixel_start+per_processor_chunk; pixelnum++ )  {
			point_t	new_view_pt;
			int	ix, iy;

			if( pixelnum > last_pixel )
				goto out;

			ip = &prev_float_frame[pixelnum];

			if( ip->ff_frame < 0 )
				continue;	/* Not valid */
			if( ip->ff_dist <= -INFINITY )
				continue;	/* was a miss */
			/* new model2view has been computed before here */
			MAT4X3PNT( new_view_pt, model2view, ip->ff_hitpt );

			/* Convert from -1..+1 range to pixel subscript */
			ix = (new_view_pt[X] + 1) * 0.5 * width;
			iy = (new_view_pt[Y] + 1) * 0.5 * height;

			/*  If not in reproject-only mode,
			 *  apply quality-preserving heuristics.
			 */
			if( reproject_mode != 2 )  {
				register int dx, dy;
				int	agelim;

				/* Don't reproject if too pixel moved too far on the screen */
				dx = ix - ip->ff_x;
				dy = iy - ip->ff_y;
				if( dx*dx + dy*dy > rt_scr_lim_dist_sq )
					continue;	/* moved too far */

				/* Don't reproject for too many frame-times */
				/* See if old pixel is more then N frames old */
				/* Temporal load-spreading: Don't have 'em all die at the same age! */
				agelim = ((iy+ix)&03)+4;
				if( curframe - ip->ff_frame >= agelim )
					continue;	/* too old */
			}

			/* 4-way splat.  See if reprojects off of screen */
			if( ix >= 0 && ix < width && iy >= 0 && iy < height )
				count += reproject_splat( ix, iy, ip, new_view_pt );

			ix++;
			if( ix >= 0 && ix < width && iy >= 0 && iy < height )
				count += reproject_splat( ix, iy, ip, new_view_pt );

			iy++;
			if( ix >= 0 && ix < width && iy >= 0 && iy < height )
				count += reproject_splat( ix, iy, ip, new_view_pt );

			ix--;
			if( ix >= 0 && ix < width && iy >= 0 && iy < height )
				count += reproject_splat( ix, iy, ip, new_view_pt );
		}
	}

	/* Deposit the statistics */
out:
	bu_semaphore_acquire( RT_SEM_WORKER );
	reproj_cur += count;
	bu_semaphore_release( RT_SEM_WORKER );
}

void
collect_soltabs( struct bu_ptbl *stp_list, union tree *tr )
{
	switch( tr->tr_op ) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_XOR:
		collect_soltabs( stp_list, tr->tr_b.tb_left );
		collect_soltabs( stp_list, tr->tr_b.tb_right );
		break;
	case OP_SUBTRACT:
		collect_soltabs( stp_list, tr->tr_b.tb_left );
		break;
	case OP_SOLID:
		bu_ptbl_ins( stp_list, (long *)tr->tr_a.tu_stp );
		break;
	}
}

/*
 *  			V I E W 2 _ I N I T
 *
 *  Called each time a new image is about to be done.
 */
void
view_2init(register struct application *ap, char *framename)
{
	register int i;
	struct bu_ptbl stps;
#ifdef HAVE_UNIX_IO
	struct stat sb;
#endif

	ap->a_refrac_index = 1.0;	/* RI_AIR -- might be water? */
	ap->a_cumlen = 0.0;
	ap->a_miss = hit_nothing;
	ap->a_onehit = a_onehit;

	if (rpt_dist)
		pwidth = 3+8;
	else
		pwidth = 3;

	/* Always allocate the scanline[] array
	 * (unless we already have one in incremental mode)
	 */
	if( (!incr_mode || !scanline) && !fullfloat_mode )
	{
		if( scanline )  free_scanlines();
		scanline = (struct scanline *)bu_calloc(
			height, sizeof(struct scanline),
			"struct scanline[height]" );
	}

#ifdef RTSRV
	buf_mode = BUFMODE_RTSRV;		/* multi-pixel buffering */
#else
	if( fullfloat_mode )  {
		buf_mode = BUFMODE_FULLFLOAT;
	} else if( incr_mode )  {
		buf_mode = BUFMODE_INCR;
	} else if( width <= 96 )  {
		buf_mode = BUFMODE_UNBUF;
	} else if( npsw <= height/4 )  {
	    	/* Have each CPU do a whole scanline.
	    	 * Saves lots of semaphore overhead.
		 * For load balancing make sure each CPU has several lines to do.
	    	 */
		per_processor_chunk = width;
	    	buf_mode = BUFMODE_SCANLINE;
	}  else  {
		buf_mode = BUFMODE_DYNAMIC;
	}
#endif

	switch( buf_mode )  {
	case BUFMODE_UNBUF:
		bu_log("Single pixel I/O, unbuffered\n");
		break;
	case BUFMODE_FULLFLOAT:
		if( !curr_float_frame )  {
bu_log("mallocing curr_float_frame\n");
			curr_float_frame = (struct floatpixel *)bu_malloc(
				width * height * sizeof(struct floatpixel),
				"floatpixel frame");
		}

		/* Mark entire current frame as "not computed" */
		{
			register struct floatpixel	*fp;

			for( fp = &curr_float_frame[width*height-1];
			     fp >= curr_float_frame; fp--
			) {
				fp->ff_frame = -1;
			}
		}

		/* Reproject previous frame */
		if( prev_float_frame && reproject_mode )  {
			reproj_cur = 0;	/* incremented by reproject_worker */
			reproj_max = width*height;

			cur_pixel = 0;
			last_pixel = width*height-1;
			if( npsw == 1 )
				reproject_worker(0,NULL);
			else
				bu_parallel( reproject_worker, npsw, NULL );
		} else {
			reproj_cur = reproj_max = 0;
		}
		break;
#ifdef RTSRV
	case BUFMODE_RTSRV:
		scanbuf = bu_malloc( srv_scanlen*pwidth + sizeof(long),
			"scanbuf [multi-line]" );
		break;
#endif
	case BUFMODE_INCR:
		{
			register int j = 1<<incr_level;
			register int w = 1<<(incr_nlevel-incr_level);

			bu_log("Incremental resolution %d\n", j);

			/* Diminish buffer expectations on work-saved lines */
			for( i=0; i<j; i++ )  {
				if( sub_grid_mode )  {
					/* ???? */
					if( (i & 1) == 0 )
						scanline[i*w].sl_left = j/2;
					else
						scanline[i*w].sl_left = j;
				} else {
					if( (i & 1) == 0 )
						scanline[i*w].sl_left = j/2;
					else
						scanline[i*w].sl_left = j;
				}
			}
		}
		if( incr_level > 1 )
				return;		 /* more res to come */
		break;

	case BUFMODE_SCANLINE:
		bu_log("Low overhead scanline-per-CPU buffering\n");
		/* Fall through... */
	case BUFMODE_DYNAMIC:
		if( buf_mode == BUFMODE_DYNAMIC &&
		    rt_verbosity & VERBOSE_OUTPUTFILE)
			bu_log("Dynamic scanline buffering\n");
		if( sub_grid_mode )  {
			for( i=sub_ymin; i<=sub_ymax; i++ )
				scanline[i].sl_left = sub_xmax-sub_xmin+1;
		} else {
			for( i=0; i<height; i++ )
				scanline[i].sl_left = width;
		}

#ifdef HAVE_UNIX_IO
		/*
		 *  This code allows the computation of a particular frame
		 *  to a disk file to be resumed automaticly.
		 *  This is worthwhile crash protection.
		 *  This use of stat() and fseek() is UNIX-specific.
		 *
		 *  This code depends on the file having already been opened
		 *  for both reading and writing for this special circumstance
		 *  of having a pre-existing file with partial results.
		 *  Ensure that positioning is precisely pixel aligned.
		 *  The file size is almost certainly
		 *  not an exact multiple of three bytes.
		 */
		if( outfp != NULL && pix_start == 0 &&
		    stat( framename, &sb ) >= 0 &&
		    sb.st_size > 0 )  {
			/* File exists, with partial results */
			register int	xx, yy;
		    	int		got;

			/* Replaced with new method of looking through whole image and calculating black pixels */
			/* pix_start = sb.st_size / sizeof(RGBpixel); */

		    	/* Protect against file being too large */
			if( pix_start > pix_end )  pix_start = pix_end;

		    	/* XXX Need to check for black interior regions! */

			xx = pix_start % width;
			yy = pix_start / width;
			fprintf(stderr,
				"Continuing with pixel %d (%d, %d) [size=%ld]\n",
				pix_start,
				xx, yy,
				(long)sb.st_size );

			scanline[yy].sl_buf = bu_calloc( width,
				sizeof(RGBpixel),
				"sl_buf scanline buffer (for continuation scanline)");
			if( fseek( outfp, yy*width*pwidth, 0 ) != 0 )
		    		bu_log("fseek error\n");
		    	/* Read the fractional scanline */
			got = fread( scanline[yy].sl_buf, sizeof(RGBpixel),
			    xx, outfp );
		    	if( got != xx )
		    		bu_log("Unable to fread fractional scanline, wanted %d, got %d pixels\n", xx, got);

			/* Account for pixels that don't need to be done */
			scanline[yy].sl_left -= xx;
			for( i = yy-1; i >= 0; i-- )
				scanline[i].sl_left = 0;
		}
#endif
		break;
	default:
		rt_bomb("bad buf_mode");
	}

	/* This is where we do Preperations for each Lighting Model if it needs it.
	   Set Photon Mapping Off by default */
	PM_Activated= 0;
	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;

		/* If user did not specify any light sources then
		 *	create default light sources
		 */
		if( BU_LIST_IS_EMPTY( &(LightHead.l) )  ||
		    BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
			if(R_DEBUG&RDEBUG_SHOWERR)bu_log("No explicit light\n");
			light_maker(1, view2model);
		}
		break;
	case 2:
		VSETALL( background, 0 );	/* Neutral Normal */
		/* FALL THROUGH */
	case 1:
	case 4:
	case 5:
	case 6:
		ap->a_hit = viewit;
		light_maker(3, view2model);
		break;
	case 7:
		{
		struct	application	bakapp;

		memcpy(&bakapp,ap,sizeof(struct application));

		/* If user did not specify any light sources then create one */
		if (BU_LIST_IS_EMPTY(&(LightHead.l)) || BU_LIST_UNINITIALIZED(&(LightHead.l))) {
			if (rdebug&RDEBUG_SHOWERR)
				bu_log("No explicit light\n");
			light_maker(1, view2model);
		}

                /* Build Photon Map */
		PM_Activated= 1;
		BuildPhotonMap(ap,eye_model,npsw,width,height,hypersample,(int)pmargs[0],pmargs[1],(int)pmargs[2],pmargs[3],(int)pmargs[4],(int)pmargs[5],(int)pmargs[6],(int)pmargs[7],pmargs[8],pmfile);

		memcpy(ap,&bakapp,sizeof(struct application));
		/* Set callback for ray hit */
		ap->a_hit= colorview;

		}
		break;
	default:
		rt_bomb("bad lighting model #");
	}
	ap->a_rt_i->rti_nlights = light_init(ap);


	/* Now OK to delete invisible light regions.
	 * Actually we just remove the references to these regions
	 * from the soltab structures in the space paritioning tree
	 */
	bu_ptbl_init( &stps, 8, "soltabs to delete" );
	if (R_DEBUG & RDEBUG_LIGHT) {
		bu_log( "deleting %d invisible light regions\n", BU_PTBL_LEN( &ap->a_rt_i->delete_regs ) );
	}
	for( i=0 ; i<BU_PTBL_LEN( &ap->a_rt_i->delete_regs ) ; i++ ) {
		struct region *rp;
		struct soltab *stp;
		int j;


		rp = (struct region *)BU_PTBL_GET( &ap->a_rt_i->delete_regs, i );

		/* make a list of soltabs containing primitives referenced by
		 * invisible light regions
		 */
		collect_soltabs( &stps, rp->reg_treetop );

		/* remove the invisible light region pointers from the soltab structs */
		if (R_DEBUG & RDEBUG_LIGHT) {
			bu_log( "Removing invisible light region pointers from %d soltabs\n",
				BU_PTBL_LEN( &stps ) );
		}
		for( j=0 ; j<BU_PTBL_LEN( &stps ) ; j++ ) {
			int k;
			struct region *rp2;
			stp = (struct soltab *)BU_PTBL_GET( &stps, j );

			k = BU_PTBL_LEN( &stp->st_regions ) - 1;
			for( ; k>=0 ; k-- ) {
				rp2 = (struct region *)BU_PTBL_GET( &stp->st_regions, k );
				if( rp2 == rp ) {
					if (R_DEBUG & RDEBUG_LIGHT) {
						bu_log( "\tRemoving region %s from soltab for %s\n", rp2->reg_name, stp->st_dp->d_namep );
					}
					bu_ptbl_rm( &stp->st_regions, (long *)rp2 );
				}
			}

		}

		bu_ptbl_reset( &stps );
	}
	bu_ptbl_free( &stps );

	/* Create integer version of background color */
	inonbackground[0] = ibackground[0] = background[0] * 255.0 + 0.5;
	inonbackground[1] = ibackground[1] = background[1] * 255.0 + 0.5;
	inonbackground[2] = ibackground[2] = background[2] * 255.0 + 0.5;

	/*
	 * If a non-background pixel comes out the same color as the
	 * background, modify it slightly, to permit compositing.
	 * Perturb the background color channel with the largest intensity.
	 */
	if( inonbackground[0] > inonbackground[1] )  {
    		if( inonbackground[0] > inonbackground[2] )  i = 0;
    		else i = 2;
    	} else {
		if( inonbackground[1] > inonbackground[2] ) i = 1;
    		else i = 2;
    	}
	if( inonbackground[i] < 127 ) inonbackground[i]++;
    	else inonbackground[i]--;

}

/*
 *  		A P P L I C A T I O N _ I N I T
 *
 *  Called once, very early on in RT setup, even before command line
 *	is processed.
 */
void application_init (void)
{
  /*    rpt_overlap = 1; */
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
