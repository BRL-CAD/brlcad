/*
 *	./rttherm -P1 -s8 -o mtherm ../.db.6d/moss.g all.g
 *
 *			V I E W T H E R M . C
 *
 *	Ray Tracing Thermal Images.
 *
 *  Output is written to a file.
 *
 *  a_uptr is pointer to returned spectral curve.
 *  When NULL, scanline buffer needs to be assigned/checked first.
 *  a_cumlen is distance to first non-atmospheric hit ("depth map").
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1996 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
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
#include "spectrum.h"
#include "./ext.h"
#include "./rdebug.h"
#include "./material.h"
#include "./mathtab.h"
#include "./light.h"

int		use_air = 0;		/* Handling of air in librt */

char usage[] = "\
Usage:  rttherm [options] model.g objects...\n\
Options:\n\
 -C spectrum=nsamp/lo_nm/hi_nm\n\
 -C bg_temp=degK\n\
 -s #		Square grid size in pixels (default 512)\n\
 -w # -n #	Grid size width and height in pixels\n\
 -V #		View (pixel) aspect ratio (width/height)\n\
 -a #		Azimuth in deg\n\
 -e #		Elevation in deg\n\
 -M		Read matrix+cmds on stdin\n\
 -N #		NMG debug flags\n\
 -o model.ssamp	Output file\n\
 -x #		librt debug flags\n\
 -X #		rt debug flags\n\
 -p #		Perspective, degrees side to side\n\
 -P #		Set number of processors\n\
 -T #/#		Tolerance: distance/angular\n\
 -r		Report overlaps\n\
 -R		Do not report overlaps\n\
";

int	max_bounces = 3;

#if NO_MATER
extern int	curframe;		/* from main.c */
extern fastf_t	frame_delta_t;		/* from main.c */
extern struct region	env_region;	/* from text.c */
#endif

/***** variables shared with rt.c *****/
extern char	*outputfile;		/* name of base of output file */
/***** end variables shared with rt.c *****/


static struct scanline {
	int	sl_left;		/* # pixels left on this scanline */
	char	*sl_buf;		/* ptr to scanline array of spectra */
} *scanline;

fastf_t	spectrum_param[3] = {
	100, 380, 12000			/* # samp, min nm, max nm */
};
fastf_t	bg_temp = 293;			/* degK.  20 degC = 293 degK */

/* Viewing module specific "set" variables */
struct structparse view_parse[] = {
	{"%f",  3, "spectrum",  (long)spectrum_param,		FUNC_NULL },
	{"%f",  1, "bg_temp",	(long)&bg_temp,			FUNC_NULL },
	{"%d",	1, "bounces",	(long)&max_bounces,		FUNC_NULL },
	{"",	0, (char *)0,	0,				FUNC_NULL }
};

/********* spectral parameters *************/
CONST struct rt_spectrum	*spectrum;	/* definition of spectrum */
struct rt_spect_sample		*ss_bg;		/* radiant emittance of bg */
/********* spectral parameters *************/

/*
 *  Ensure that a_uptr points to a valid spectral curve.
 */
void
curve_attach(ap)
register struct application *ap;
{
	register struct scanline	*slp;

	if( ap->a_uptr )  {
		RT_CK_SPECT_SAMPLE( ap->a_uptr );
		return;
	}

	/* If scanline buffer has not yet been allocated, do so now */
	slp = &scanline[ap->a_y];
	RES_ACQUIRE( &rt_g.res_results );
	if( slp->sl_buf == (char *)0 )  {
		slp->sl_buf = (char *)rt_get_spect_sample_array( spectrum, width );
	}
	RES_RELEASE( &rt_g.res_results );

	ap->a_uptr = slp->sl_buf+(ap->a_x*RT_SIZEOF_SPECT_SAMPLE(spectrum));
	RT_CK_SPECT_SAMPLE( ap->a_uptr );
}

/*
 *  			V I E W _ P I X E L
 *  
 *  Arrange to have the pixel "output".
 *  For RTTHERM this is a misnomer, as the pixel's spectral samples have
 *  been living in the scanline buffer the whole time.
 *  When a scaline is completed (possibly not in sequence), write it to file.
 */
void
view_pixel(ap)
register struct application *ap;
{
	register int	r,g,b;
	register char	*pixelp;
	register struct scanline	*slp;
	register int	do_eol = 0;

	if( ap->a_user == 0 )  {
		/* Shot missed the model, don't dither */
		curve_attach(ap);
		rt_spect_copy( (struct rt_spect_sample *)ap->a_uptr, ss_bg );
	} else {
		if( !ap->a_uptr )
			rt_bomb("view_pixel called with no spectral curve associated\n");
	}

	slp = &scanline[ap->a_y];
	RES_ACQUIRE( &rt_g.res_results );
	if( --(slp->sl_left) <= 0 )
		do_eol = 1;
	RES_RELEASE( &rt_g.res_results );

	if( !do_eol )  return;

	if( outfp != NULL )  {
		int	count;

		/* XXX This writes an array of structures out, including magic */
		/* XXX in machine-specific format */
		RES_ACQUIRE( &rt_g.res_syscall );
		if( fseek( outfp, ap->a_y*(long)width*RT_SIZEOF_SPECT_SAMPLE(spectrum), 0 ) != 0 )
			rt_log("fseek error\n");
		count = fwrite( scanline[ap->a_y].sl_buf,
			RT_SIZEOF_SPECT_SAMPLE(spectrum), width, outfp );
		RES_RELEASE( &rt_g.res_syscall );
		if( count != width )
			rt_bomb("view_pixel:  fwrite failure\n");
	}
	rt_free( scanline[ap->a_y].sl_buf, "sl_buf scanline buffer" );
	scanline[ap->a_y].sl_buf = (char *)0;
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
#if NO_MATER
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
#endif
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
#if NO_MATER
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		mlib_free( regp );
	}
	if( env_region.reg_mfuncs )  {
		rt_free( (char *)env_region.reg_name, "env_region.reg_name" );
		env_region.reg_name = (char *)0;
		mlib_free( &env_region );
	}

	light_cleanup();
#endif
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

#if NO_MATER
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
#endif

	ap->a_user = 0;		/* Signal view_pixel:  MISS */
	return(0);
}

/*
 *			C O L O R V I E W
 *
 *  Manage the coloring of whatever it was we just hit.
 *  This can be a recursive procedure.
 */
int
colorview( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
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
		ap->a_user = 0;		/* Signal view_pixel:  MISS */
		goto out;
	}

	/* Check to see if eye is "inside" the solid */
	/* It might only be worthwhile doing all this in perspective mode */
	/* XXX Note that hit_dist can be faintly negative, e.g. -1e-13 */

	if( hitp->hit_dist < 0.0 && pp->pt_regionp->reg_aircode == 0 ) {
		struct application sub_ap;
		FAST fastf_t f;

		if( pp->pt_outhit->hit_dist >= INFINITY ||
		    ap->a_level > max_bounces )  {
			rt_log("colorview:  eye inside %s (x=%d, y=%d, lvl=%d)\n",
				pp->pt_regionp->reg_name,
				ap->a_x, ap->a_y, ap->a_level);
			ap->a_user = 0;		/* Signal view_pixel:  MISS */
			goto out;
		}
		/* Push on to exit point, and trace on from there */
		sub_ap = *ap;	/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		f = pp->pt_outhit->hit_dist+0.0001;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
		sub_ap.a_purpose = "pushed eye position";
		(void)rt_shootray( &sub_ap );
		ap->a_user = sub_ap.a_user;
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

#if 0
	bzero( (char *)&sw, sizeof(sw) );
	sw.sw_transmit = sw.sw_reflect = 0.0;
	sw.sw_refrac_index = 1.0;
	sw.sw_extinction = 0;
	sw.sw_xmitonly = 0;		/* want full data */
	sw.sw_inputs = 0;		/* no fields filled yet */
	sw.sw_frame = curframe;
	sw.sw_pixeltime = sw.sw_frametime = curframe * frame_delta_t;
	VSETALL( sw.sw_color, 1 );
	VSETALL( sw.sw_basecolor, 1 );

	if (rdebug&RDEBUG_SHADE)
		rt_log("colorview calling viewshade\n");

	(void)viewshade( ap, pp, &sw );

	/* As a special case for now, handle reflection & refraction */
	if( sw.sw_reflect > 0 || sw.sw_transmit > 0 )
		(void)rr_render( ap, pp, &sw );

	VMOVE( ap->a_color, sw.sw_color );
#endif

	/* +++++ Something was hit, get the power from it. ++++++ */
rt_bomb("colorview: not written\n");



	ap->a_user = 1;		/* Signal view_pixel:  HIT */
out:
	if(rdebug&RDEBUG_HITS)  {
		rt_log("colorview: lvl=%d ret a_user=%d %s\n",
			ap->a_level,
			ap->a_user,
			pp->pt_regionp->reg_name);
	}
	return(1);
}

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
	struct rt_vls	name;

	/*
	 *  Connect up material library interfaces
	 *  Note that plastic.c defines the required "default" entry.
	 */
#if NO_MATER
	{
		extern struct mfuncs phg_mfuncs[];
		extern struct mfuncs light_mfuncs[];

		mlib_add( phg_mfuncs );
		mlib_add( light_mfuncs );
	}
#endif

	rt_structprint( "rttherm variables", view_parse, NULL );

	/* Build spectrum definition */
	spectrum = rt_spect_uniform( (int)spectrum_param[0],
		spectrum_param[1], spectrum_param[2] );

	rt_vls_init( &name );
	rt_vls_printf( &name, "%s.spect", outputfile ? outputfile : "RTTHERM" );
	rt_write_spectrum( rt_vls_addr(&name), spectrum );
	rt_vls_free( &name );

	/* Output is destined for a file */
	output_is_binary = 1;
	return(0);		/* don't open framebuffer */
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
	if (use_air)
		ap->a_onehit = 3;
	else
		ap->a_onehit = 1;

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

	rt_log("Dynamic scanline buffering\n");
	for( i=0; i<height; i++ )
		scanline[i].sl_left = width;

	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;
#if NO_MATER
		/* If present, use user-specified light solids */
		if( RT_LIST_IS_EMPTY( &(LightHead.l) )  ||
		    RT_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
			if(rdebug&RDEBUG_SHOWERR)rt_log("No explicit light\n");
			light_maker(1, view2model);
		}
#endif
		break;
	default:
		rt_bomb("bad lighting model #");
	}
#if NO_MATER
	ap->a_rt_i->rti_nlights = light_init();
#endif

	/* Compute radiant emittance of background */
	/* XXX This is wrong, need actual power (radiant flux) emitted */
	RT_GET_SPECT_SAMPLE( ss_bg, spectrum );
	rt_spect_black_body( ss_bg, bg_temp, 9 );
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
