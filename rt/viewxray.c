/*
 *			V I E W X R A Y . C
 *
 *  Ray Tracing program RTXRAY bottom half.
 *
 *  This module produces pseudo XRAY images of models by adding
 *  up ray path lengths.  By default it will scale these lengths
 *  by the max model RPP length and write a grey scale BW file.
 *  The alternate lighting model outputs "doubles" containing the
 *  actual lengths for post processing.
 *
 *  A better version of this would know something about material
 *  densities.
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSviewxray[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./rdebug.h"
#include "fb.h"

int	use_air = 0;			/* Handling of air in librt */
int	using_mlib = 0;			/* Material routines NOT used */

extern	FBIO	*fbp;
extern	FILE	*outfp;
extern	fastf_t	viewsize;
extern	int	lightmodel;
extern	double	AmbientIntensity;	/* XXX - temp hack for contrast! */
extern	int	width, height;

static	unsigned char *scanbuf;
static	int pixsize = 0;		/* bytes per pixel in scanbuf */
static	double	contrast_boost = 2.0;

static int xrayhit();
static int xraymiss();

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,	FUNC_NULL }
};

char usage[] = "\
Usage: rtxray [options] model.g objects... >stuff\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -o file.bw	Output file name, else frame buffer\n\
 -A #		Contrast boost (default 2.0), may clip if > 1\n\
 -x #		Set librt debug flags\n\
 -l 0		line buffered B&W X-Rays (default)\n\
 -l 1		Floating point X-Rays (path lengths in doubles)\n\
";

/* lighting models */
#define	LGT_BW		0
#define	LGT_FLOAT	1

/*
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
int minus_o;
{
	/*
	 *  We need to work to get the output pixels and scanlines
	 *  in order before we can run in parallel.  Something like
	 *  view.c does in its dynamic buffering mode.
	 */
	if (rt_g.rtg_parallel) {
		rt_g.rtg_parallel = 0;
		rt_log("rtxray: Can't do parallel yet, using one CPU\n");
	}

	if( lightmodel == LGT_BW ) {
		if( minus_o )
			pixsize = 1;		/* BW file */
		else
			pixsize = 3;		/* Frame buffer */
	} else {
		/* XXX - Floating output uses no buffer */
		pixsize = 0;
	}
	if( pixsize ) {
		scanbuf = (unsigned char *)
			rt_malloc( width*pixsize, "scanline buffer" );
	}

	if( minus_o ) {
		/* output is to a file */
		return(0);		/* don't open frame buffer */
	}

	if( lightmodel == LGT_FLOAT ) {
		rt_log("rtxray: Can't do floating point mode to frame buffer, use -o\n");
		exit(1);
	}
	return(1);		/* we need a framebuffer */
}

/* beginning of a frame */
void
view_2init( ap, framename )
struct application *ap;
char *framename;
{
	/*
	 *  This is a dangerous hack to allow us to use -A #
	 *  as a way of passing in a contrast_boost factor.
	 *  We need a way for view modules to add their own
	 *  flags to the command line arguments.
	 */
	if (AmbientIntensity > 1.0)
		contrast_boost = AmbientIntensity;
	rt_log("Contrast Boost = %5.2f\n", contrast_boost);

	ap->a_hit = xrayhit;
	ap->a_miss = xraymiss;
	ap->a_onehit = 0;
}

/* end of each pixel */
void
view_pixel( ap )
register struct application *ap;
{
}

/* end of each line */
void
view_eol( ap )
register struct application *ap;
{
	if( lightmodel == LGT_BW ) {
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		if( outfp != NULL )
			fwrite( scanbuf, pixsize, width, outfp );
		else if( fbp != FBIO_NULL )
			fb_write( fbp, 0, ap->a_y, scanbuf, width );
		bu_semaphore_release( BU_SEM_SYSCALL );
	}
}

void	view_setup() {}
/* end of a frame, called after rt_clean() */
void	view_cleanup() {}

/* end of each frame */
int
view_end()
{
	return	0;		/* OK */
}

static int
xrayhit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	register struct hit *hitp;
	fastf_t	totdist;
	fastf_t	fvalue;
	unsigned char value;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("xrayhit:  no hit out front?\n");
		return(0);
	}

	if(rdebug&RDEBUG_HITS)  {
		rt_pr_pt( ap->a_rt_i, pp );
	}

	hitp = pp->pt_inhit;
	if( hitp->hit_dist >= INFINITY )  {
		rt_log("xrayhit:  entry beyond infinity\n");
		return(1);
	}
	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < 0.0 )  {
		/* XXX */
		rt_log("xrayhit:  Eye inside solid (%g)\n", hitp->hit_dist );
		for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
			rt_pr_pt( ap->a_rt_i, pp );
		return(0);
	}

	/* Finally! We are ready to walk the partition chain */

	/* Compute the total thickness */
	totdist = 0;
	while( pp != PartHeadp ) {
		double	dist;

		dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
		totdist += dist;

		pp = pp->pt_forw;
	}

	switch( lightmodel ) {
	case LGT_FLOAT:
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		fwrite( &totdist, sizeof(totdist), 1, outfp );
		bu_semaphore_release( BU_SEM_SYSCALL );
		break;
	case LGT_BW:
		fvalue = 1.0 - contrast_boost*totdist/viewsize;
		if( fvalue > 1.0 ) fvalue = 1.0;
		else if( fvalue <= 0.0 ) fvalue = 0.0;
		value = 1.0 + 254.99 * fvalue;
		bu_semaphore_acquire( RT_SEM_RESULTS );
		if( pixsize == 1 ) {
			scanbuf[ap->a_x] = value;
		} else {
			scanbuf[ap->a_x*3+RED] = value;
			scanbuf[ap->a_x*3+GRN] = value;
			scanbuf[ap->a_x*3+BLU] = value;
		}
		bu_semaphore_release( RT_SEM_RESULTS );
		break;
	}

	return(1);	/* report hit to main routine */
}

static int
xraymiss( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	static	double	zero = 0;

	switch( lightmodel ) {
	case LGT_BW:
		bu_semaphore_acquire( RT_SEM_RESULTS );
		if( pixsize == 1 ) {
			scanbuf[ap->a_x] = 0;
		} else {
			scanbuf[ap->a_x*3+RED] = 0;
			scanbuf[ap->a_x*3+GRN] = 0;
			scanbuf[ap->a_x*3+BLU] = 0;
		}
		bu_semaphore_release( RT_SEM_RESULTS );
		break;
	case LGT_FLOAT:
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		fwrite( &zero, sizeof(zero), 1, outfp );
		bu_semaphore_release( BU_SEM_SYSCALL );
		break;
	default:
		rt_log( "xraymiss: Bad lighting model %d\n", lightmodel );
		break;
	}

	return(0);	/* report miss to main routine */
}

void application_init () {}
