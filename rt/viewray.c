/*
 *			V I E W R A Y
 *
 *  Ray Tracing program RTRAY bottom half.
 *
 *  This module turns RT library partition lists into VLD Standard Format
 *  ray files, as defined by /vld/include/ray.h.  A variety of VLD programs
 *  exist to manipulate these files, including rayvect.
 *
 *  To obtain a UNIX-plot of a ray file, the procedure is:
 *	/vld/bin/rayvect -mMM < file.ray > file.vect
 *	/vld/bin/vectplot -mMM < file.vect > file.plot
 *	tplot -Tmeg file.plot		# or equivalent
 *
 *  Author -
 *	Michael John Muuss
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
static char RCSrayview[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

char usage[] = "\
Usage:  rtray [options] model.g objects... >file.ray\n\
Options:\n\
 -f #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -x #		Set librt debug flags\n\
";

/* Null function -- handle a miss */
int	raymiss() { return(0); }

void	view_pixel() {}

/* Write a hit to the ray file */
int
rayhit( ap, PartHeadp )
struct application *ap;
register struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;

	for( ; pp != PartHeadp; pp = pp->pt_forw )  {
		RT_HIT_NORM( pp->pt_inhit, pp->pt_inseg->seg_stp, &(ap->a_ray) );
		RT_HIT_NORM( pp->pt_outhit, pp->pt_outseg->seg_stp, &(ap->a_ray) );
		wray( pp, ap, stdout );
	}
	return(0);
}

/*
 *  			V I E W _ I N I T
 */
int
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{
	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 0;

	if( minus_o )
		fprintf(stderr,"Warning:  -o ignored, rays go to stdout\n");
	return(0);		/* No framebuffer needed */
}

void	view_eol() {;}

void
view_end()
{
	fflush(stdout);
}

void	view_2init()  {;}

int	mlib_setup() { return(1); }

void	mlib_free() { ; }
