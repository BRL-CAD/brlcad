/*
 *			V I E W . C
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

#include "conf.h"

#include <stdio.h>
#include <math.h>

#ifdef HAVE_UNIX_IO
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "mater.h"
#include "rtlist.h"
#include "raytrace.h"
#include "fb.h"
#include "./ext.h"
#include "./rdebug.h"
#include "./material.h"
#include "./mathtab.h"
#include "./light.h"

int		use_air = 0;		/* Handling of air in librt */

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

extern FBIO	*fbp;			/* Framebuffer handle */

extern int	max_bounces;		/* from refract.c */
extern int	max_ireflect;		/* from refract.c */
extern int	curframe;		/* from main.c */
extern fastf_t	frame_delta_t;		/* from main.c */

extern struct region	env_region;	/* from text.c */

vect_t ambient_color = { 1, 1, 1 };	/* Ambient white light */

#if 0
vect_t	background = { 0.25, 0, 0.5 };	/* Dark Blue Background */
#else
vect_t	background = { 0, 0, 1.0/255 };	/* Nearly Black */
#endif
int	ibackground[3];			/* integer 0..255 version */
int	inonbackground[3];		/* integer non-background */

#ifdef RTSRV
extern int	srv_startpix;		/* offset for view_pixel */
extern int	srv_scanlen;		/* BUFMODE_RTSRV buffer length */
extern char	*scanbuf;		/* scanline(s) buffer */
#endif

void		free_scanlines();

static int	buf_mode=0;
#define BUFMODE_UNBUF	1		/* No output buffering */
#define BUFMODE_DYNAMIC	2		/* Dynamic output buffering */
#define BUFMODE_INCR	3		/* incr_mode set, dynamic buffering */
#define BUFMODE_RTSRV	4		/* output buffering into scanbuf */

static struct scanline {
	int	sl_left;		/* # pixels left on this scanline */
	char	*sl_buf;		/* ptr to buffer for scanline */
} *scanline;

static int	pwidth;			/* Width of each pixel (in bytes) */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
#if !defined(__alpha)   /* XXX Alpha does not support this initialization! */
	{"%d",	1, "bounces",	bu_byteoffset(max_bounces),		FUNC_NULL },
	{"%d",	1, "ireflect",	bu_byteoffset(max_ireflect),		FUNC_NULL },
	{"%f", ELEMENTS_PER_VECT, "background",bu_byteoffset(background[0]),	FUNC_NULL },
#endif
	{"",	0, (char *)0,	0,				FUNC_NULL }
};

/*
 *  			V I E W _ P I X E L
 *  
 *  Arrange to have the pixel output.
 */
void
view_pixel(ap)
register struct application *ap;
{
	register int	r,g,b;
	register char	*pixelp;
	register struct scanline	*slp;
	register int	do_eol = 0;
	unsigned char	dist[8];	/* pixel distance (in IEEE format) */

	if (rpt_dist)
	    htond(dist, &(ap->a_dist), 1);

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
		r = ap->a_color[0]*255.+rand0to1(ap->a_resource->re_randptr);
		g = ap->a_color[1]*255.+rand0to1(ap->a_resource->re_randptr);
		b = ap->a_color[2]*255.+rand0to1(ap->a_resource->re_randptr);
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
	}

	if(rdebug&RDEBUG_HITS) rt_log("rgb=%3d,%3d,%3d xy=%3d,%3d (%g,%g,%g)\n",
		r,g,b, ap->a_x, ap->a_y,
		ap->a_color[0], ap->a_color[1], ap->a_color[2] );

	switch( buf_mode )  {

	case BUFMODE_UNBUF:
		{
			RGBpixel	p;
			int		npix;
			p[0] = r ;
			p[1] = g ;
			p[2] = b ;

			if( outfp != NULL )  {
				if( fwrite( p, 3, 1, outfp ) != 1 )
					rt_bomb("pixel fwrite error");
				if( rpt_dist &&
				    ( fwrite( dist, 8, 1, outfp ) != 1 ))
					rt_bomb("pixel fwrite error");
			}
			if( fbp != FBIO_NULL )  {
				/* Framebuffer output */
				RES_ACQUIRE( &rt_g.res_syscall );
				npix = fb_write( fbp, ap->a_x, ap->a_y,
					(unsigned char *)p, 1 );
				RES_RELEASE( &rt_g.res_syscall );
				if( npix < 1 )  rt_bomb("pixel fb_write error");
			}
		}
		return;

#ifdef RTSRV
	case BUFMODE_RTSRV:
		/* Multi-pixel buffer */
		pixelp = scanbuf+ pwidth * 
			((ap->a_y*width) + ap->a_x - srv_startpix);
		RES_ACQUIRE( &rt_g.res_results );
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
		RES_RELEASE( &rt_g.res_results );
		return;
#endif

	/*
	 *  Store results into pixel buffer.
	 *  Don't depend on interlocked hardware byte-splice.
	 *  Need to protect scanline[].sl_left when in parallel mode.
	 */

	case BUFMODE_DYNAMIC:
		slp = &scanline[ap->a_y];
		RES_ACQUIRE( &rt_g.res_results );
		if( slp->sl_buf == (char *)0 )  {
			slp->sl_buf = rt_calloc( width, pwidth, "sl_buf scanline buffer" );
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
		RES_RELEASE( &rt_g.res_results );
		break;

	case BUFMODE_INCR:
		{
			register int dx,dy;
			register int spread;

			spread = 1<<(incr_nlevel-incr_level);

			RES_ACQUIRE( &rt_g.res_results );
			for( dy=0; dy<spread; dy++ )  {
				if( ap->a_y+dy >= height )  break;
				slp = &scanline[ap->a_y+dy];
				if( slp->sl_buf == (char *)0 )
					slp->sl_buf = rt_calloc( width+32,
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
			RES_RELEASE( &rt_g.res_results );
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
			RES_ACQUIRE( &rt_g.res_syscall );
			for( dy=spread; dy >= 0; dy-- )  {
				yy = ap->a_y + dy;
				npix = fb_write( fbp, 0, yy,
					(unsigned char *)scanline[yy].sl_buf,
					width );
				if( npix != width )  break;
			}
			RES_RELEASE( &rt_g.res_syscall );
			if( npix != width )  rt_bomb("fb_write error (incremental res)");
		}
		break;

	case BUFMODE_DYNAMIC:
		if( fbp != FBIO_NULL )  {
			int		npix;
			RES_ACQUIRE( &rt_g.res_syscall );
			npix = fb_write( fbp, 0, ap->a_y,
			    (unsigned char *)scanline[ap->a_y].sl_buf, width );
			RES_RELEASE( &rt_g.res_syscall );
			if( npix < width )  rt_bomb("scanline fb_write error");
		}
		if( outfp != NULL )  {
			int	count;

			RES_ACQUIRE( &rt_g.res_syscall );
			if( fseek( outfp, ap->a_y*width*pwidth, 0 ) != 0 )
				fprintf(stderr, "fseek error\n");
			count = fwrite( scanline[ap->a_y].sl_buf,
				sizeof(char), width*pwidth, outfp );
			RES_RELEASE( &rt_g.res_syscall );
			if( count != width*pwidth )
				rt_bomb("view_pixel:  fwrite failure\n");
		}
		rt_free( scanline[ap->a_y].sl_buf, "sl_buf scanline buffer" );
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
view_eol(ap)
register struct application *ap;
{
	return;
}

/*
 *			V I E W _ E N D
 */
view_end(ap)
struct application *ap;
{
	free_scanlines();
	return(0);		/* OK */
}

/*
 *			V I E W _ S E T U P
 *
 *  Called before rt_prep() in do.c
 */
void
view_setup(rtip)
struct rt_i	*rtip;
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
	for( regp=rtip->HeadRegion; regp != REGION_NULL; )  {
		switch( mlib_setup( regp, rtip ) )  {
		case -1:
		default:
			rt_log("mlib_setup failure on %s\n", regp->reg_name);
			break;
		case 0:
			if(rdebug&RDEBUG_MATERIAL)
				rt_log("mlib_setup: drop region %s\n", regp->reg_name);
			{
				struct region *r = regp->reg_forw;
				/* zap reg_udata? beware of light structs */
				rt_del_regtree( rtip, regp );
				regp = r;
				continue;
			}
		case 1:
			/* Full success */
			if( rdebug&RDEBUG_MATERIAL &&
			    ((struct mfuncs *)(regp->reg_mfuncs))->mf_print )  {
				((struct mfuncs *)(regp->reg_mfuncs))->
					mf_print( regp, regp->reg_udata );
			}
			/* Perhaps this should be a function? */
			break;
		}
		regp = regp->reg_forw;
	}
}

/*
 *			V I E W _ C L E A N U P
 *
 *  Called before rt_clean() in do.c
 */
void
view_cleanup(rtip)
struct rt_i	*rtip;
{
	register struct region	*regp;

	RT_CHECK_RTI(rtip);
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		mlib_free( regp );
	}
	if( env_region.reg_mfuncs )  {
		rt_free( (char *)env_region.reg_name, "env_region.reg_name" );
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
static hit_nothing( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	if( rdebug&RDEBUG_MISSPLOT )  {
		vect_t	out;

		/* XXX length should be 1 model diameter */
		VJOIN1( out, ap->a_ray.r_pt,
			10000, ap->a_ray.r_dir );	/* to imply direction */
		pl_color( stdout, 190, 0, 0 );
		pdv_3line( stdout, ap->a_ray.r_pt, out );
	}

	if( env_region.reg_mfuncs )  {
		struct gunk {
			struct partition part;
			struct hit	hit;
			struct shadework sw;
		} u;

		bzero( (char *)&u, sizeof(u) );
		/* Make "miss" hit the environment map */
		/* Build up the fakery */
		u.part.pt_inhit = u.part.pt_outhit = &u.hit;
		u.part.pt_regionp = &env_region;
		u.hit.hit_dist = ap->a_rt_i->rti_radius * 2;	/* model diam */

		u.sw.sw_transmit = u.sw.sw_reflect = 0.0;
		u.sw.sw_refrac_index = 1.0;
		u.sw.sw_extinction = 0;
		u.sw.sw_xmitonly = 1;		/* don't shade env map! */

		/* "Surface" Normal points inward, UV is azim/elev of ray */
		u.sw.sw_inputs = MFI_NORMAL|MFI_UV;
		VREVERSE( u.sw.sw_hit.hit_normal, ap->a_ray.r_dir );
		/* U is azimuth, atan() range: -pi to +pi */
		u.sw.sw_uv.uv_u = mat_atan2( ap->a_ray.r_dir[Y],
			ap->a_ray.r_dir[X] ) * rt_inv2pi;
		if( u.sw.sw_uv.uv_u < 0 )
			u.sw.sw_uv.uv_u += 1.0;
		/*
		 *  V is elevation, atan() range: -pi/2 to +pi/2,
		 *  because sqrt() ensures that X parameter is always >0
		 */
		u.sw.sw_uv.uv_v = mat_atan2( ap->a_ray.r_dir[Z],
			sqrt( ap->a_ray.r_dir[X] * ap->a_ray.r_dir[X] +
			ap->a_ray.r_dir[Y] * ap->a_ray.r_dir[Y]) ) *
			rt_invpi + 0.5;
		u.sw.sw_uv.uv_du = u.sw.sw_uv.uv_dv = 0;

		VSETALL( u.sw.sw_color, 1 );
		VSETALL( u.sw.sw_basecolor, 1 );

		if (rdebug&RDEBUG_SHADE)
			rt_log("hit_nothing calling viewshade\n");

		(void)viewshade( ap, &u.part, &u.sw );

		VMOVE( ap->a_color, u.sw.sw_color );
		ap->a_user = 1;		/* Signal view_pixel:  HIT */
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
colorview( ap, PartHeadp, finished_segs )
register struct application *ap;
struct partition *PartHeadp;
struct seg *finished_segs;
{
	register struct partition *pp;
	register struct hit *hitp;
	struct shadework sw;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("colorview:  no hit out front?\n");
		return(0);
	}
	hitp = pp->pt_inhit;

	if(rdebug&RDEBUG_HITS)  {
		rt_log("colorview: lvl=%d coloring %s\n",
			ap->a_level,
			pp->pt_regionp->reg_name);
		rt_pr_pt( ap->a_rt_i, pp );
	}
	if( hitp->hit_dist >= INFINITY )  {
		rt_log("colorview:  entry beyond infinity\n");
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
		    	if( rdebug&RDEBUG_SHOWERR )  {
				VSET( ap->a_color, 9, 0, 0 );	/* RED */
				rt_log("colorview:  eye inside %s (x=%d, y=%d, lvl=%d)\n",
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
		f = pp->pt_outhit->hit_dist+0.0001;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
		sub_ap.a_purpose = "pushed eye position";
		(void)rt_shootray( &sub_ap );

		/* The eye is inside a solid and we are "Looking out" so
		 * we are going to darken what we see beyond to give a visual
		 * cue that something is wrong.
		 */
		VSCALE( ap->a_color, sub_ap.a_color, 0.80 );

		ap->a_user = 1;		/* Signal view_pixel: HIT */
		ap->a_dist = hitp->hit_dist;
		goto out;
	}

	if( rdebug&RDEBUG_RAYWRITE )  {
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
	if( rdebug&RDEBUG_RAYPLOT )  {
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
			pl_color( stdout, i, 0, i );
			pdv_3line( stdout, ap->a_ray.r_pt, inhit );

			if( (out = pp->pt_outhit->hit_dist) >= INFINITY )
				out = 10000;	/* to imply the direction */
			VJOIN1( outhit,
				ap->a_ray.r_pt, out,
				ap->a_ray.r_dir );
			pl_color( stdout, i, i, i );
			pdv_3line( stdout, inhit, outhit );
		}
	}

	bzero( (char *)&sw, sizeof(sw) );
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

	if (rdebug&RDEBUG_SHADE)
		rt_log("colorview calling viewshade\n");

	/* individual shaders must handle reflection & refraction */
	(void)viewshade( ap, pp, &sw );

	VMOVE( ap->a_color, sw.sw_color );
	ap->a_user = 1;		/* Signal view_pixel:  HIT */
	ap->a_dist = hitp->hit_dist;
out:
	if(rdebug&RDEBUG_HITS)  {
		rt_log("colorview: lvl=%d ret a_user=%d %s\n",
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
viewit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
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
		rt_log("viewit:  no hit out front?\n");
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
		lp = RT_LIST_FIRST( light_specific, &(LightHead.l) );
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
	}

	if(rdebug&RDEBUG_HITS)  {
		rt_pr_hit( " In", hitp );
		rt_log("cosI0=%f, diffuse0=%f   ", cosI0, diffuse0 );
		VPRINT("RGB", ap->a_color);
	}
	ap->a_user = 1;		/* Signal view_pixel:  HIT */
	return(0);
}
void
free_scanlines()
{
	register int	y;

	for( y=0; y<height; y++ )  {
		if( scanline[y].sl_buf )  {
			rt_free( scanline[y].sl_buf, "sl_buf scanline buffer" );
			scanline[y].sl_buf = (char *)0;
		}
	}
	rt_free( (char *)scanline, "struct scanline[height]" );
	scanline = (struct scanline *)0;
}

/*
 *  			V I E W _ I N I T
 *
 *  Called once, early on in RT setup, before view size is set.
 */
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{

	/*
	 *  Connect up material library interfaces
	 *  Note that plastic.c defines the required "default" entry.
	 */
	{
		extern struct mfuncs phg_mfuncs[];
		extern struct mfuncs light_mfuncs[];
		extern struct mfuncs cloud_mfuncs[];
		extern struct mfuncs spm_mfuncs[];
		extern struct mfuncs txt_mfuncs[];
		extern struct mfuncs stk_mfuncs[];
		extern struct mfuncs cook_mfuncs[];
		extern struct mfuncs marble_mfuncs[];
		extern struct mfuncs stxt_mfuncs[];
		extern struct mfuncs points_mfuncs[];
		extern struct mfuncs toyota_mfuncs[];
		extern struct mfuncs wood_mfuncs[];
		extern struct mfuncs camo_mfuncs[]; 
		extern struct mfuncs scloud_mfuncs[];
		extern struct mfuncs air_mfuncs[];
		extern struct mfuncs rtrans_mfuncs[];
		extern struct mfuncs fire_mfuncs[];
		extern struct mfuncs brdf_mfuncs[];
		extern struct mfuncs gauss_mfuncs[];
		extern struct mfuncs paint_mfuncs[];
		extern struct mfuncs gravel_mfuncs[];
		extern struct mfuncs prj_mfuncs[];
		extern struct mfuncs grass_mfuncs[];

		mlib_add( phg_mfuncs );
		mlib_add( light_mfuncs );
		mlib_add( cloud_mfuncs );
		mlib_add( spm_mfuncs );
		mlib_add( txt_mfuncs );
		mlib_add( stk_mfuncs );
		mlib_add( cook_mfuncs );
		mlib_add( marble_mfuncs );
		mlib_add( stxt_mfuncs );
		mlib_add( points_mfuncs );
		mlib_add( toyota_mfuncs );
		mlib_add( wood_mfuncs );
		mlib_add( camo_mfuncs );
		mlib_add( scloud_mfuncs );
		mlib_add( air_mfuncs );
		mlib_add( rtrans_mfuncs );
		mlib_add( fire_mfuncs );
		mlib_add( brdf_mfuncs );
		mlib_add( gauss_mfuncs );
		mlib_add( paint_mfuncs );
		mlib_add( gravel_mfuncs );
		mlib_add( prj_mfuncs );
		mlib_add( grass_mfuncs );
	}

	if( minus_o )  {
		/* Output is destined for a pixel file */
		return(0);		/* don't open framebuffer */
	}  else
	{
	    if (rpt_dist)
	    {
		bu_log("Warning: -d ignored.  Writing to frame buffer\n");
		rpt_dist = 0;
	    }
	    return(1);		/* open a framebuffer */
	}
}

/*
 *  			V I E W 2 _ I N I T
 *
 *  Called each time a new image is about to be done.
 */
void
view_2init( ap, framename )
register struct application *ap;
char	*framename;
{
	register int i;
#ifdef HAVE_UNIX_IO
	struct stat sb;
#endif

	ap->a_refrac_index = 1.0;	/* RI_AIR -- might be water? */
	ap->a_cumlen = 0.0;
	ap->a_miss = hit_nothing;
	if (rpt_overlap)
		ap->a_overlap = RT_AFN_NULL;
	else
		ap->a_overlap = rt_overlap_quietly;
	ap->a_onehit = -1;		/* Require at least one non-air hit */
	if (rpt_dist)
		pwidth = 3+8;
	else
		pwidth = 3;

	/* Always allocate the scanline[] array
	 * (unless we already have one in incremental mode)
	 */
	if( !incr_mode || !scanline )
	{
		if( scanline )  free_scanlines();
		scanline = (struct scanline *)rt_calloc(
			height, sizeof(struct scanline),
			"struct scanline[height]" );
	}

#ifdef RTSRV
	buf_mode = BUFMODE_RTSRV;		/* multi-pixel buffering */
#else
	if( incr_mode )  {
		buf_mode = BUFMODE_INCR;
	} else if( rt_g.rtg_parallel )  {
		buf_mode = BUFMODE_DYNAMIC;
	} else if( width <= 96 )  {
		buf_mode = BUFMODE_UNBUF;
	}  else  {
		buf_mode = BUFMODE_DYNAMIC;
	}
#endif

	switch( buf_mode )  {
	case BUFMODE_UNBUF:
		rt_log("Single pixel I/O, unbuffered\n");
		break;	
#ifdef RTSRV
	case BUFMODE_RTSRV:
		scanbuf = rt_malloc( srv_scanlen*pwidth + sizeof(long),
			"scanbuf [multi-line]" );
		break;
#endif
	case BUFMODE_INCR:
		rt_log("Incremental resolution\n");
		{
			register int j = 1<<incr_level;
			register int w = 1<<(incr_nlevel-incr_level);

			/* Diminish buffer expectations on work-saved lines */
			for( i=0; i<j; i++ )  {
				if( (i & 1) == 0 )
					scanline[i*w].sl_left = j/2;
				else
					scanline[i*w].sl_left = j;
			}
		}
		if( incr_level > 1 )
				return;		 /* more res to come */
		break;

	case BUFMODE_DYNAMIC:
		rt_log("Dynamic scanline buffering\n");
		for( i=0; i<height; i++ )
			scanline[i].sl_left = width;

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

			pix_start = sb.st_size / sizeof(RGBpixel);

		    	/* Protect against file being too large */
			if( pix_start > pix_end )  pix_start = pix_end;

			xx = pix_start % width;
			yy = pix_start / width;
			fprintf(stderr,
				"Continuing with pixel %d (%d, %d) [size=%d]\n",
				pix_start,
				xx, yy,
				sb.st_size );

			scanline[yy].sl_buf = rt_calloc( width,
				sizeof(RGBpixel), 
				"sl_buf scanline buffer (for continuation scanline)");
			if( fseek( outfp, yy*width*3L, 0 ) != 0 )
		    		rt_log("fseek error\n");
		    	/* Read the fractional scanline */
			got = fread( scanline[yy].sl_buf, sizeof(RGBpixel),
			    xx, outfp );
		    	if( got != xx )
		    		rt_log("Unable to fread fractional scanline, wanted %d, got %d pixels\n", xx, got);

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

	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;

		/* If user did not specify any light sources then 
		 *	create default light sources
		 */
		if( RT_LIST_IS_EMPTY( &(LightHead.l) )  ||
		    RT_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
			if(rdebug&RDEBUG_SHOWERR)rt_log("No explicit light\n");
			light_maker(1, view2model);
		}
		break;
	case 2:
		VSETALL( background, 0 );	/* Neutral Normal */
		/* FALL THROUGH */
	case 1:
	case 4:
	case 5:
		ap->a_hit = viewit;
		light_maker(3, view2model);
		break;
	default:
		rt_bomb("bad lighting model #");
	}
	ap->a_rt_i->rti_nlights = light_init();

	/* Create integer version of background color */
	inonbackground[0] = ibackground[0] = background[0] * 255;
	inonbackground[1] = ibackground[1] = background[1] * 255;
	inonbackground[2] = ibackground[2] = background[2] * 255;

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
void application_init ()
{
    rpt_overlap = 1;
}
