/*                      V I E W X R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
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
/** @file viewxray.c
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
 */
#ifndef lint
static const char RCSviewxray[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
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

static int xrayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
static int xraymiss(register struct application *ap);

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL }
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
view_init(register struct application *ap, char *file, char *obj, int minus_o, int minus_F)
{
	/*
	 * We need to work to get the output pixels and scanlines
	 * in order before we can run in parallel.  Something like
	 * view.c does in its dynamic buffering mode.
	 *
	 * XXX this hack-around causes a need for more careful
	 * semaphore acquisition since it may block
	 */
	if (rt_g.rtg_parallel) {
		rt_g.rtg_parallel = 0;
		bu_log("rtxray: Can't do parallel yet, using one CPU\n");
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
			bu_malloc( width*pixsize, "scanline buffer" );
	}

	if( minus_F || (!minus_o && !minus_F) ) {
	    /* open a framebuffer? */
	    if( lightmodel == LGT_FLOAT ) {
		bu_log("rtxray: Can't do floating point mode to frame buffer, use -o\n");
		return 0;
	    }
	    return 1;
	}

	/* no framebuffer */
	return 0;
}

/* beginning of a frame */
void
view_2init(struct application *ap, char *framename)
{
	/*
	 *  This is a dangerous hack to allow us to use -A #
	 *  as a way of passing in a contrast_boost factor.
	 *  We need a way for view modules to add their own
	 *  flags to the command line arguments.
	 */
	if (AmbientIntensity > 1.0)
		contrast_boost = AmbientIntensity;
	bu_log("Contrast Boost = %5.2f\n", contrast_boost);

	ap->a_hit = xrayhit;
	ap->a_miss = xraymiss;
	ap->a_onehit = 0;
}

/* end of each pixel */
void
view_pixel(register struct application *ap)
{
}

/* end of each line */
void
view_eol(register struct application *ap)
{
    int i;
    char *buf = (char *)0;

    if( lightmodel == LGT_BW ) {
	if( outfp != NULL ) {
	    if (rt_g.rtg_parallel) {
		bu_semaphore_acquire( BU_SEM_SYSCALL );
	    }
	    fwrite( scanbuf, pixsize, width, outfp );
	    if (rt_g.rtg_parallel) {
		bu_semaphore_release( BU_SEM_SYSCALL );
	    }
	    if( fbp != FBIO_NULL ) {
		if (!buf) {
		    buf = (char *)bu_malloc(sizeof(RGBpixel)*width, "allocating temporary buffer in viewxray");
		}

		/* fb_write only accepts RGBpixel arrays, so convert it */
		for (i=0; i < width; i++) {
		    buf[i*3+RED] = scanbuf[i];
		    buf[i*3+GRN] = scanbuf[i];
		    buf[i*3+BLU] = scanbuf[i];
		}

		if (rt_g.rtg_parallel) {
		    bu_semaphore_acquire( BU_SEM_SYSCALL );
		}
		fb_write( fbp, 0, ap->a_y, buf, width );
		if (rt_g.rtg_parallel) {
		    bu_semaphore_release( BU_SEM_SYSCALL );
		}

		if (buf) {
		    bu_free(buf, "releasing temporary buffer in viewxray");
		}
	    }
	} else {
	    if (rt_g.rtg_parallel) {
		bu_semaphore_acquire( BU_SEM_SYSCALL );
	    }
	    fb_write( fbp, 0, ap->a_y, scanbuf, width );
	    if (rt_g.rtg_parallel) {
		bu_semaphore_release( BU_SEM_SYSCALL );
	    }
	}
    }
}

void	view_setup(void) {}
/* end of a frame, called after rt_clean() */
void	view_cleanup(void) {}

/* end of each frame */
void
view_end(void)
{

}

static int
xrayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp)
{
	register struct partition *pp;
	register struct hit *hitp;
	fastf_t	totdist;
	fastf_t	fvalue;
	unsigned char value;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		bu_log("xrayhit:  no hit out front?\n");
		return(0);
	}

	if(R_DEBUG&RDEBUG_HITS)  {
		rt_pr_pt( ap->a_rt_i, pp );
	}

	hitp = pp->pt_inhit;
	if( hitp->hit_dist >= INFINITY )  {
		bu_log("xrayhit:  entry beyond infinity\n");
		return(1);
	}
	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < 0.0 )  {
		/* XXX */
		bu_log("xrayhit:  Eye inside solid (%g)\n", hitp->hit_dist );
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
xraymiss(register struct application *ap)
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
		bu_log( "xraymiss: Bad lighting model %d\n", lightmodel );
		break;
	}

	return(0);	/* report miss to main routine */
}

void application_init (void) {}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
