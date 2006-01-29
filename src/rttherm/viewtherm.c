/*                     V I E W T H E R M . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2006 United States Government as represented by
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
/** @file viewtherm.c
 *	./rttherm -P1 -s64 -o mtherm ../.db.6d/moss.g all.g
 *
 *			V I E W T H E R M . C
 *
 *	Ray Tracing Thermal Images.
 *
 *  Output is written to a file.
 *
 *  a_spectrum is pointer to returned spectral curve.
 *  When NULL, scanline buffer needs to be assigned/checked first.
 *  a_cumlen is distance to first non-atmospheric hit ("depth map").
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "rtlist.h"
#include "raytrace.h"
#include "fb.h"
#include "bu.h"
#include "spectrum.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "rtprivate.h"
#include "plot3.h"

#include "../rt/ext.h"
#include "../rt/light.h"


extern int viewshade(struct application *ap,
		     register const struct partition *pp,
		     register struct shadework *swp);

extern void multispectral_shader_init(struct mfuncs **headp);

/* XXX Move to raytrace.h when routine goes into LIBRT */
BU_EXTERN( double	rt_pixel_footprint, (const struct application *ap,
				const struct hit *hitp,
				const struct seg *segp,
				const vect_t normal));


/* XXX move to h/tabdata.h when function moves out of spectrum.c */
BU_EXTERN(struct bn_table	*bn_table_make_visible_and_uniform, (int num,
				double first, double last, int vis_nsamp));


int		use_air = 0;		/* Handling of air in librt */

char usage[] = "\
Usage:  rttherm [options] model.g objects...\n\
Options:\n\
 -c set spectrum=nsamp/lo_nm/hi_nm\n\
 -c set bg_temp=degK\n\
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

extern FBIO	*fbp;			/* Framebuffer handle */

extern int	max_bounces;		/* from refract.c */
extern int	max_ireflect;		/* from refract.c */
extern int	curframe;		/* from main.c */
extern fastf_t	frame_delta_t;		/* from main.c */

void		free_scanlines(void);

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

struct mfuncs *mfHead = MF_NULL;	/* Head of list of shaders */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"%f",  3, "spectrum",  (long)spectrum_param,		FUNC_NULL },
	{"%f",  1, "bg_temp",	(long)&bg_temp,			FUNC_NULL },
	{"%d",	1, "bounces",	(long)&max_bounces,		FUNC_NULL },
	{"",	0, (char *)0,	0,				FUNC_NULL }
};


/*
 *  Ensure that a_spectrum points to a valid spectral curve.
 */
void
curve_attach(register struct application *ap)
{
	register struct scanline	*slp;

	RT_AP_CHECK(ap);
	RT_CK_RTI(ap->a_rt_i);

	if( ap->a_spectrum )  {
		BN_CK_TABDATA( ap->a_spectrum );
		return;
	}

	/* If scanline buffer has not yet been allocated, do so now */
	slp = &scanline[ap->a_y];
	bu_semaphore_acquire( RT_SEM_RESULTS );
	if( slp->sl_buf == (char *)0 )  {
		slp->sl_buf = (char *)bn_tabdata_malloc_array( spectrum, width );
	}
	bu_semaphore_release( RT_SEM_RESULTS );
	BN_CK_TABDATA( slp->sl_buf );	/* pun for first struct in array (sanity) */

	ap->a_spectrum = (struct bn_tabdata *)
		(slp->sl_buf+(ap->a_x*BN_SIZEOF_TABDATA(spectrum)));
	BN_CK_TABDATA( ap->a_spectrum );
	BU_ASSERT( ap->a_spectrum->table == spectrum );
}

/*
 *			B A C K G R O U N D _ R A D I A T I O N
 *
 *  Concoct _some_ kind of background radiation when the ray misses the
 *  model and there is no environment map defined.
 *  XXX For now this is a gross hack.
 */
void
background_radiation(struct application *ap)
{
	fastf_t	dist;
	fastf_t	radius;
	fastf_t	cm2;

	dist = 10000000.0;	/* 10 Km */
	radius = ap->a_rbeam + dist * ap->a_diverge;
	cm2 = 4 * radius * radius * 0.01;	/* mm2 to cm2 */

	curve_attach(ap);

	/* XXX This should be attenuated by some atmosphere now */
	/* At least it's in proper power units */
	bn_tabdata_scale( ap->a_spectrum, background, cm2 );
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
view_pixel(register struct application *ap)
{
	register struct scanline	*slp;
	register int	do_eol = 0;

	RT_AP_CHECK(ap);
	RT_CK_RTI(ap->a_rt_i);

	if( ap->a_user == 0 )  {
		/* Shot missed the model */
		background_radiation(ap);
	} else {
		if( !ap->a_spectrum )
			rt_bomb("view_pixel called with no spectral curve associated\n");
		BN_CK_TABDATA(ap->a_spectrum);
	}

	slp = &scanline[ap->a_y];
	bu_semaphore_acquire( RT_SEM_RESULTS );
	if( --(slp->sl_left) <= 0 )
		do_eol = 1;
	bu_semaphore_release( RT_SEM_RESULTS );

	if( !do_eol )  return;

	if( outfp != NULL )  {
		int	count;

		/* XXX This writes an array of structures out, including magic */
		/* XXX in machine-specific format */
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		if( fseek( outfp, ap->a_y*(long)width*BN_SIZEOF_TABDATA(spectrum), 0 ) != 0 )
			rt_log("fseek error\n");
		count = fwrite( scanline[ap->a_y].sl_buf,
			BN_SIZEOF_TABDATA(spectrum), width, outfp );
		bu_semaphore_release( BU_SEM_SYSCALL );
		if( count != width )
			rt_bomb("view_pixel:  fwrite failure\n");
	}
#ifdef MSWISS
	if( fbp != FBIO_NULL ) {
		/* MSWISS -- real-time multi-spectral case */
		unsigned char obuf[4096];
		int i;
		char *line = (char *)scanline[ap->a_y].sl_buf;
		struct bn_tabdata *sp;
		int npix;

		/* Convert from spectral to monochrome image */
		extern double	filter_bias;	/* MSWISS: values set by rtnode.c */
		extern double	filter_gain;
		extern int	filter_freq_index;
/* Hack, re-equalize per line */
#if 1
{
double lo = INFINITY, hi = -INFINITY;
	for( i=0; i<width; i++ )  {
		register double tmp;
		sp = (struct bn_tabdata *)(line + i * BN_SIZEOF_TABDATA(spectrum));
		tmp = sp->y[filter_freq_index];
		if( tmp < lo ) lo = tmp;
		if( tmp > hi ) hi = tmp;
	}
	filter_bias = lo;
	filter_gain = 255/(hi-lo);
}
#endif

		BN_CK_TABDATA(line);
		BU_ASSERT( width < sizeof(obuf) );
		/* A variety of filters could be used here, eventually */
		for( i=0; i<width; i++ )  {
			register double tmp;
			sp = (struct bn_tabdata *)(line + i * BN_SIZEOF_TABDATA(spectrum));
			tmp = filter_gain *
				(sp->y[filter_freq_index] - filter_bias);
			if( tmp <= 0 )  obuf[i] = 0;
			else if( tmp >= 255 )  obuf[i] = 255;
			else obuf[i] = (unsigned char)tmp;
		}

		/* Output the scanline */
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		npix = fb_bwwriterect(fbp, 0, ap->a_y, width, 1, obuf);
		bu_semaphore_release(BU_SEM_SYSCALL);
		BU_ASSERT( npix >= 1 );
	}
#endif /* MSWISS */
	bu_free( scanline[ap->a_y].sl_buf, "sl_buf scanline buffer" );
	scanline[ap->a_y].sl_buf = (char *)0;
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
	free_scanlines();
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
			rt_log("mlib_setup failure on %s\n", regp->reg_name);
			break;
		case 0:
			if(rdebug&RDEBUG_MATERIAL)
				rt_log("mlib_setup: drop region %s\n", regp->reg_name);
			{
				struct region *r = BU_LIST_NEXT( region, &regp->l );
				/* zap reg_udata? beware of light structs */
				rt_del_regtree( rtip, regp, &rt_uniresource );
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
		regp = BU_LIST_NEXT( region, &regp->l );
	}
}

/*
 *			V I E W _ C L E A N U P
 *
 *  Called before rt_clean() in do.c
 */
void
view_cleanup(struct rt_i *rtip)
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
static int
hit_nothing(register struct application *ap)
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

		memset((void *)&u, 0, sizeof(u) );

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

		u.sw.msw_color = bn_tabdata_get_constval( 1.0, spectrum );
		u.sw.msw_basecolor = bn_tabdata_get_constval( 1.0, spectrum );

		if (rdebug&RDEBUG_SHADE)
			rt_log("hit_nothing calling viewshade\n");

		(void)viewshade( ap, &u.part, &u.sw );

		bn_tabdata_copy( ap->a_spectrum, u.sw.msw_color );
		ap->a_user = 1;		/* Signal view_pixel:  HIT */
		ap->a_uptr = (genptr_t)&env_region;
		return(1);
	}

	ap->a_user = 0;		/* Signal view_pixel:  MISS */
	background_radiation(ap);	/* In case someone looks */
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

	sw.msw_color = BN_TABDATA_NULL;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("colorview:  no hit out front?\n");
		return(0);
	}
	hitp = pp->pt_inhit;
	ap->a_uptr = (genptr_t)pp->pt_regionp;	/* note which region was shaded */

	if(rdebug&RDEBUG_HITS)  {
		rt_log("colorview: lvl=%d coloring %s\n",
			ap->a_level,
			pp->pt_regionp->reg_name);
		rt_pr_pt( ap->a_rt_i, pp );
	}
	if( hitp->hit_dist >= INFINITY )  {
		rt_log("colorview:  entry beyond infinity\n");
		background_radiation(ap);	/* was VSET( ap->a_color, .5, 0, 0 ); */
		ap->a_user = 1;		/* Signal view_pixel:  HIT */
		ap->a_dist = hitp->hit_dist;
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
			bu_log("colorview:  eye inside %s (x=%d, y=%d, lvl=%d)\n",
				pp->pt_regionp->reg_name,
				ap->a_x, ap->a_y, ap->a_level);
		    	background_radiation(ap);	/* was: VSETALL( ap->a_color, 0.18 ); */
			ap->a_user = 1;		/* Signal view_pixel:  HIT */
			ap->a_dist = hitp->hit_dist;
			goto out;
		}
		/* Push on to exit point, and trace on from there */
		sub_ap = *ap;	/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		curve_attach(ap);
		sub_ap.a_spectrum = bn_tabdata_dup( ap->a_spectrum );

		f = pp->pt_outhit->hit_dist+0.0001;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
		sub_ap.a_purpose = "pushed eye position";
		(void)rt_shootray( &sub_ap );

		/* The eye is inside a solid and we are "Looking out" so
		 * we are going to darken what we see beyond to give a visual
		 * cue that something is wrong.
		 */
		bn_tabdata_scale( ap->a_spectrum, sub_ap.a_spectrum, 0.80 );
		bu_free( sub_ap.a_spectrum, "bn_tabdata *a_spectrum");

		ap->a_user = 1;		/* Signal view_pixel: HIT */
		ap->a_dist = f + sub_ap.a_dist;
		ap->a_uptr = sub_ap.a_uptr;	/* which region */
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

	if( !ap->a_spectrum )  curve_attach(ap);
/* XXX This is the right way to do this, but isn't quite ready yet. */
	memset( (void *)&sw, 0, sizeof(sw) );
	sw.sw_transmit = sw.sw_reflect = 0.0;
	sw.sw_refrac_index = 1.0;
	sw.sw_extinction = 0;
	sw.sw_xmitonly = 0;		/* want full data */
	sw.sw_inputs = 0;		/* no fields filled yet */
	sw.sw_frame = curframe;
	sw.sw_pixeltime = sw.sw_frametime = curframe * frame_delta_t;
	sw.msw_color = bn_tabdata_get_constval( 1.0, spectrum );
	sw.msw_basecolor = bn_tabdata_get_constval( 1.0, spectrum );
	if( pp->pt_regionp->reg_mater.ma_temperature > 0 )
		sw.sw_temperature = pp->pt_regionp->reg_mater.ma_temperature;
	else
		sw.sw_temperature = bg_temp;

	if (rdebug&RDEBUG_SHADE)
		rt_log("colorview calling viewshade, temp=%g\n", sw.sw_temperature);
if (rdebug&RDEBUG_SHADE) pr_shadework( "shadework before viewshade", &sw);

	(void)viewshade( ap, pp, &sw );
if (rdebug&RDEBUG_SHADE) pr_shadework( "shadework after viewshade", &sw);
	if (rdebug&RDEBUG_SHADE)
		rt_log("after viewshade, temp=%g\n", sw.sw_temperature);

	/* individual shaders must handle reflection & refraction */

	/* bn_tabdata_copy( ap->a_spectrum, sw.msw_color ); */
	rt_spect_black_body( sw.msw_basecolor, sw.sw_temperature, 3 );
	bn_tabdata_add( ap->a_spectrum, sw.msw_color, sw.msw_basecolor );

	ap->a_user = 1;		/* Signal view_pixel:  HIT */
	/* XXX This is always negative when eye is inside air solid */
	ap->a_dist = hitp->hit_dist;

	bu_free( sw.msw_color, "sw.msw_color");
	bu_free( sw.msw_basecolor, "sw.msw_basecolor");

out:
	RT_CK_REGION(ap->a_uptr);
	if(rdebug&RDEBUG_HITS)  {
		bu_log("colorview: lvl=%d ret a_user=%d %s\n",
			ap->a_level,
			ap->a_user,
			pp->pt_regionp->reg_name);
		bn_pr_tabdata("a_spectrum", ap->a_spectrum );
	}
	return(1);
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
view_init(register struct application *ap, char *file, char *obj, int minus_o)
{
	extern char	libmultispectral_version[];

	bu_log("%s", libmultispectral_version+5);

	multispectral_shader_init(&mfHead);	/* in libmultispectral/init.c */

	bu_struct_print( "rttherm variables", view_parse, NULL );

	if( !minus_o )   {
		bu_bomb("rttherm: No -o flag specified, can't write to framebuffer, aborting\n");
		exit(2);
	}

	/* Build spectrum definition */
	spectrum = bn_table_make_visible_and_uniform( (int)spectrum_param[0],
		spectrum_param[1], spectrum_param[2], 20 );

	/* Output is destined for a file */
	return 0;		/* don't open framebuffer */
}

/*
 *  			V I E W _ 2 I N I T
 *
 *  Called each time a new image is about to be done.
 */
void
view_2init(register struct application *ap, char *framename)
{
	register int i;
	struct bu_vls	name;

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
		scanline = (struct scanline *)bu_calloc(
			height, sizeof(struct scanline),
			"struct scanline[height]" );
	}

	/* Extra 2nd file!  Write out spectrum info */
	bu_vls_init( &name );
	bu_vls_printf( &name, "%s.spect", framename ? framename : "RTTHERM" );
	bn_table_write( bu_vls_addr(&name), spectrum );
	bu_log("Wrote %s\n", bu_vls_addr(&name) );
	bu_vls_free( &name );

	/* Check for existing file and checkpoint-restart? */

	rt_log("Dynamic scanline buffering\n");
	for( i=0; i<height; i++ )
		scanline[i].sl_left = width;

	switch( lightmodel )  {
	case 0:
		ap->a_hit = colorview;
		/* If present, use user-specified light solids */
		if( RT_LIST_IS_EMPTY( &(LightHead.l) )  ||
		    RT_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
			if(rdebug&RDEBUG_SHOWERR)rt_log("No explicit light\n");
			light_maker(1, view2model);
		}
		break;
	default:
		rt_bomb("bad lighting model #");
	}
	ap->a_rt_i->rti_nlights = light_init(ap);

	/* Compute radiant emittance of background */
	/* XXX This is wrong, need actual power (radiant flux) emitted */
	BN_GET_TABDATA( background, spectrum );
	rt_spect_black_body( background, bg_temp, 9 );
}

/*
 *  		A P P L I C A T I O N _ I N I T
 *
 *  Called once, very early on in RT setup, even before command line
 *	is processed.
 */
void application_init (void)
{
    rpt_overlap = 1;
}


/* --- */

/*
 *
 *  hitp->hit_point and normal must be computed by caller.
 *
 *  Return -
 *	area of ray footprint, in mm**2 (square milimeters).
 */
double
rt_pixel_footprint(const struct application *ap, const struct hit *hitp, const struct seg *segp, const fastf_t *normal)
{
	plane_t	perp;
	plane_t	surf_tan;
	fastf_t	h_radius, v_radius;
	point_t	corners[4];
	fastf_t	norm_dist;
	fastf_t	area;
	int	i;

	/*  If surface normal is nearly perpendicular to ray,
	 *  (i.e. ray is parallel to surface), abort
	 */
	if( fabs(VDOT(ap->a_ray.r_dir, normal)) <= 1.0e-10 )  {
parallel:
		rt_log("rt_pixel_footprint() ray parallel to surface\n");	/* debug */
		return 0;
	}

	/*  Compute H and V "radius" of the footprint along
	 *  a plane perpendicular to the ray direction.
	 *  Find the 4 corners of the footprint on this perpendicular plane.
	 */
	mat_vec_perp( perp, ap->a_ray.r_dir );
	perp[3] = VDOT( perp, hitp->hit_point );

	h_radius = ap->a_rbeam + hitp->hit_dist * ap->a_diverge;
	v_radius = ap->a_rbeam + hitp->hit_dist * ap->a_diverge * cell_width / cell_height;

	VJOIN2( corners[0], hitp->hit_point,
		 h_radius, dx_model,  v_radius, dy_model );	/* UR */
	VJOIN2( corners[1], hitp->hit_point,
		-h_radius, dx_model,  v_radius, dy_model );	/* UL */
	VJOIN2( corners[2], hitp->hit_point,
		-h_radius, dx_model, -v_radius, dy_model );	/* LL */
	VJOIN2( corners[3], hitp->hit_point,
		 h_radius, dx_model, -v_radius, dy_model );	/* LR */

	/* Approximate surface at hit point by a (tangent) plane */
	VMOVE( surf_tan, normal );
	surf_tan[3] = VDOT( surf_tan, hitp->hit_point );

	/*
	 *  Form a line from ray start point to each corner point,
	 *  compute intersection with tangent plane,
	 *  replace corner point with new point on tangent plane.
	 */
	norm_dist = DIST_PT_PLANE( ap->a_ray.r_pt, surf_tan );
	for( i=0; i<4; i++ )  {
		fastf_t		slant_factor;	/* Direction dot Normal */
		vect_t		dir;
		fastf_t		dist;

		/* XXX sanity check */
		dist = DIST_PT_PT( corners[i], segp->seg_stp->st_center );
		if( dist > segp->seg_stp->st_bradius )
			rt_log(" rt_pixel_footprint() dist = %g > radius = %g\n", dist, segp->seg_stp->st_bradius );

		VSUB2( dir, corners[i], ap->a_ray.r_pt );
		VUNITIZE(dir);
		if( (slant_factor = -VDOT( surf_tan, dir )) < -1.0e-10 ||
		     slant_factor > 1.0e-10 )  {
			dist = norm_dist / slant_factor;
			if( !NEAR_ZERO(dist, INFINITY) )
				goto parallel;
		} else {
			goto parallel;
		}
		VJOIN1( corners[i], ap->a_ray.r_pt, dist, dir );
	}

	/* Find area of 012, and 230 triangles */
	area = rt_area_of_triangle( corners[0], corners[1], corners[2] );
	area += rt_area_of_triangle( corners[2], corners[3], corners[0] );
	return area;
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
