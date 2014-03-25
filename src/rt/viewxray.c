/*                      V I E W X R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file rt/viewxray.c
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
 */

#include "common.h"

#include <stdio.h>

#include "bu/parallel.h"
#include "vmath.h"
#include "icv.h"
#include "raytrace.h"
#include "fb.h"

#include "./rtuif.h"
#include "./ext.h"


/* lighting models */
#define	LGT_BW		0
#define	LGT_FLOAT	1


extern	FBIO	*fbp;
extern	FILE	*outfp;
extern	fastf_t	viewsize;
extern	int	lightmodel;

unsigned char *scanbuf;
static int pixsize = 0;		/* bytes per pixel in scanbuf */
static double	contrast_boost = 2.0;

static int xrayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
static int xraymiss(register struct application *ap);

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};
static char *floatfilename=NULL;
const char floatfileext[] = ".los";

const char title[] = "RT X-Ray";
void
usage(const char *argv0)
{
    bu_log("Usage: %s [options] model.g objects... >stuff\n", argv0);
    bu_log("Options:\n");
    bu_log(" -s #		Grid size in pixels, default 512\n");
    bu_log(" -a Az		Azimuth in degrees	(conflicts with -M)\n");
    bu_log(" -e Elev	Elevation in degrees	(conflicts with -M)\n");
    bu_log(" -M		Read model2view matrix on stdin (conflicts with -a, -e)\n");
    bu_log(" -o file.bw	Output file name, else frame buffer\n");
    bu_log(" -A #		Contrast boost (default 2.0), may clip if > 1\n");
    bu_log(" -x #		Set librt debug flags\n");
    bu_log(" -l 0		line buffered B&W X-Rays (default)\n");
    bu_log(" -l 1		Floating point X-Rays (path lengths in doubles)\n");
}


/*
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init(struct application *UNUSED(ap), char *UNUSED(file), char *UNUSED(obj), int minus_o, int minus_F)
{
    /*
     * We need to work to get the output pixels and scanlines
     * in order before we can run in parallel.  Something like
     * view.c does in its dynamic buffering mode.
     *
     * XXX this hack-around causes a need for more careful
     * semaphore acquisition since it may block
     */
    if (RTG.rtg_parallel) {
	RTG.rtg_parallel = 0;
	bu_log("rtxray: Can't do parallel yet, using one CPU\n");
    }

    if (lightmodel == LGT_BW) {
	pixsize = 3; /* use frame buffer size with icv */
	scanbuf = (unsigned char *)bu_malloc( width*pixsize, "scanline buffer" );
    } else {
	/* XXX - Floating output uses no buffer */
	pixsize = 0;
	/* force change of 'outputfile' to short circuit libicv for LGT_FLOAT outputs, adds '.los' extension */
	if (outputfile) {
	    char buf[BUFSIZ];
	    int format = icv_guess_file_format(outputfile, buf);
	    if (format != ICV_IMAGE_UNKNOWN) {
		bu_strlcpy(buf, outputfile, BUFSIZ);
		bu_strlcat(buf, floatfileext, BUFSIZ);
		outputfile = floatfilename = bu_strdup(buf);
	    }
	}
    }

    if ( minus_F || (!minus_o && !minus_F) ) {
	/* open a framebuffer? */
	if ( lightmodel == LGT_FLOAT ) {
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
view_2init(struct application *ap, char *UNUSED(framename))
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
view_pixel(struct application *UNUSED(ap))
{
}

/* end of each line */
void
view_eol(struct application *ap)
{
    size_t i;

    if ( lightmodel == LGT_BW ) {

	if (bif != NULL) {
	    if (RTG.rtg_parallel) {
		bu_semaphore_acquire( BU_SEM_SYSCALL );
	    }
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    /* TODO : Add double type data to maintain resolution */
	    icv_writeline(bif, ap->a_y, scanbuf, ICV_DATA_UCHAR);
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    if (RTG.rtg_parallel) {
		bu_semaphore_release( BU_SEM_SYSCALL );
	    }
	} else if ( outfp != NULL ) {
	    if (RTG.rtg_parallel) {
		bu_semaphore_acquire( BU_SEM_SYSCALL );
	    }
	    i = fwrite( scanbuf, pixsize, width, outfp );
	    if (i < width) {
		perror("fwrite");
	    }
	    if (RTG.rtg_parallel) {
		bu_semaphore_release( BU_SEM_SYSCALL );
	    }
	}
	if ( fbp != FBIO_NULL ) {
	    if (RTG.rtg_parallel) {
		bu_semaphore_acquire( BU_SEM_SYSCALL );
	    }
	    fb_write( fbp, 0, ap->a_y, scanbuf, width );
	    if (RTG.rtg_parallel) {
		bu_semaphore_release( BU_SEM_SYSCALL );
	    }
	}

	if (bif == NULL && fbp == FBIO_NULL && outfp == NULL)
	    bu_log("rtxray: strange, no end of line actions taken.\n");
    }
}

void	view_setup(struct rt_i *UNUSED(rtip)) {}
/* end of a frame, called after rt_clean() */
void	view_cleanup(struct rt_i *UNUSED(rtip)) {}

/* end of each frame */
void
view_end(struct application *UNUSED(ap))
{

}

static int
xrayhit(register struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    register struct partition *pp;
    register struct hit *hitp;
    fastf_t	totdist;
    fastf_t	fvalue;
    unsigned char value;
    size_t ret;

    for ( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
	if ( pp->pt_outhit->hit_dist >= 0.0 )  break;
    if ( pp == PartHeadp )  {
	bu_log("xrayhit:  no hit out front?\n");
	return 0;
    }

    if (R_DEBUG&RDEBUG_HITS)  {
	rt_pr_pt( ap->a_rt_i, pp );
    }

    hitp = pp->pt_inhit;
    if ( hitp->hit_dist >= INFINITY )  {
	bu_log("xrayhit:  entry beyond infinity\n");
	return 1;
    }
    /* Check to see if eye is "inside" the solid */
    if ( hitp->hit_dist < 0.0 )  {
	/* XXX */
	bu_log("xrayhit:  Eye inside solid (%g)\n", hitp->hit_dist );
	for ( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
	    rt_pr_pt( ap->a_rt_i, pp );
	return 0;
    }

    /* Finally! We are ready to walk the partition chain */

    /* Compute the total thickness */
    totdist = 0;
    while ( pp != PartHeadp ) {
	double	dist;

	dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	totdist += dist;

	pp = pp->pt_forw;
    }

    switch ( lightmodel ) {
	case LGT_FLOAT:
	    bu_semaphore_acquire( BU_SEM_SYSCALL );
	    ret = fwrite( &totdist, sizeof(totdist), 1, outfp );
	    if (ret < 1)
		perror("fwrite");
	    bu_semaphore_release( BU_SEM_SYSCALL );
	    break;
	case LGT_BW:
	    fvalue = 1.0 - contrast_boost*totdist/viewsize;
	    if ( fvalue > 1.0 ) fvalue = 1.0;
	    else if ( fvalue <= 0.0 ) fvalue = 0.0;
	    value = 1.0 + 254.99 * fvalue;
	    bu_semaphore_acquire( RT_SEM_RESULTS );
	    if ( pixsize == 1 ) {
		scanbuf[ap->a_x] = value;
	    } else {
		scanbuf[ap->a_x*3+RED] = value;
		scanbuf[ap->a_x*3+GRN] = value;
		scanbuf[ap->a_x*3+BLU] = value;
	    }
	    bu_semaphore_release( RT_SEM_RESULTS );
	    break;
    }

    return 1;	/* report hit to main routine */
}

static int
xraymiss(register struct application *ap)
{
    static double zero = 0;
    size_t ret;

    switch ( lightmodel ) {
	case LGT_BW:
	    bu_semaphore_acquire( RT_SEM_RESULTS );
	    if ( pixsize == 1 ) {
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
	    ret = fwrite( &zero, sizeof(zero), 1, outfp );
	    if (ret < 1)
		perror("fwrite");
	    bu_semaphore_release( BU_SEM_SYSCALL );
	    break;
	default:
	    bu_log( "xraymiss: Bad lighting model %d\n", lightmodel );
	    break;
    }

    return 0;	/* report miss to main routine */
}

void application_init (void) {}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
