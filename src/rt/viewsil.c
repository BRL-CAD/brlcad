/*                       V I E W S I L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file viewsil.c
 *
 *  Application-specific code for rtsil(1)
 *
 *  For more details, see:
 *  S. Muuss, ``Rapid Creation of Analysis Codes Using BRL-CAD:
 *  The RT View-Module Interface'', in the Proceedings of the
 *  BRL-CAD Symposium 1991.
 *
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSview[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

/* private */
#include "rtprivate.h"


/*
 *  If this variable is set to zero, then "air" solids in the model
 *  will be entirely discarded.
 *  If this variable is set non-zero, then "air" solids will be
 *  retained, and can be expected to show up in the partition lists.
 */
int	use_air = 0;		/* Handling of air in librt */

extern	FILE	*outfp;
extern	int	width, height;
static	unsigned char *scanbuf;

/*
 *  Viewing module specific "set" variables.
 *  Any additional variables that the user may be allowed to change
 *  should be listed here.
 *  They can then be accessed via a "set" command, either from
 *  the command line, or from within an animation script.
 */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL}
};

const char title[] = "RT Simple Intersection Lightmap";
const char usage[] = "\
Usage:  rtsil [options] model.g objects... [> file.pix]\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees\n\
 -e Elev	Elevation in degrees\n\
 -M		Read matrix, cmds on stdin\n\
 -o file.pix	Output file name, else stdout\n\
 -x #		Set librt debug flags\n\
";


int	rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
int	raymiss(register struct application *ap);


/*
 *  			V I E W _ I N I T
 *
 *  Called by main() at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init(register struct application *ap, char *file, char *obj, int minus_o)
{
	if (rt_g.rtg_parallel) {
		rt_g.rtg_parallel = 0;
		bu_log("rtsil: Can't do parallel yet, using one CPU\n");
	}
	scanbuf = (unsigned char *)
		bu_malloc( width, "scanline buffer" );
	return(0);		/* no framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  The beginning of a frame.
 *  Called by do_frame() just before raytracing starts.
 */
void
view_2init(struct application *ap)
{
	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 1;
}

/*
 *			V I E W _ P I X E L
 *
 *  Called by worker() after the end of proccessing for each pixel.
 */
void
view_pixel(register struct application *ap)
{
}

/*
 *			V I E W _ E O L
 *
 *  Called by worker() at the end of each line.  Depricated.
 *  Any end-of-line processing should be done in view_pixel().
 */
void
view_eol(register struct application *ap)
{
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	if( outfp != NULL )
		fwrite( scanbuf, 1, width, outfp );
#if 0
	else if( fbp != FBIO_NULL )
		fb_write( fbp, 0, ap->a_y, scanbuf, width );
#endif
	bu_semaphore_release( BU_SEM_SYSCALL );
}

/*
 *			V I E W _ E N D
 *
 *  Called in do_frame() at the end of a frame,
 *  just after raytracing completes.
 */
void
view_end(register struct application *ap)
{
}

/*
 *			V I E W _ S E T U P
 *
 *  Called by do_prep(), just before rt_prep() is called, in do.c
 *  This allows the lighting model to get set up for this frame,
 *  e.g., generate lights, associate materials routines, etc.
 */
void
view_setup(struct rt_i *rtip)
{
}

/*
 *			V I E W _ C L E A N U P
 *
 *  Called by "clean" command, just before rt_clean() is called, in do.c
 */
void
view_cleanup(struct rt_i *rtip)
{
}

/*
 *			R A Y H I T
 *
 *  Called via a_hit linkage from rt_shootray() when ray hits.
 */
int
rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp)
{
	bu_semaphore_acquire( RT_SEM_RESULTS );
	scanbuf[ap->a_x] = 1;
	bu_semaphore_release( RT_SEM_RESULTS );
	return(1);	/* report hit to main routine */
}

/*
 *			R A Y M I S S
 *
 *  Called via a_miss linkage from rt_shootray() when ray misses.
 */
int
raymiss(register struct application *ap)
{
	bu_semaphore_acquire( RT_SEM_RESULTS );
	scanbuf[ap->a_x] = 255;
	bu_semaphore_release( RT_SEM_RESULTS );
	return(0);
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
