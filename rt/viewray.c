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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "raytrace.h"

char usage[] = "\
Usage:  rtray [options] model.g objects... >file.ray\n\
Options:\n\
 -f#		Grid size in pixels, default 512, max 1024\n\
 -aAz		Azimuth in degrees	(conflicts with -M)\n\
 -eElev		Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -x#		Set debug flags\n\
";

/* Null function -- handle a miss */
raymiss() { ; }

view_pixel() {}

/* Write a hit to the ray file */
rayhit( ap, PartHeadp )
struct application *ap;
register struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;

	for( ; pp != PartHeadp; pp = pp->pt_forw )  {
		wray( pp, ap, stdout );
	}
}

/*
 *  			V I E W _ I N I T
 */
view_init( ap, file, obj, npts, outfd )
register struct application *ap;
char *file, *obj;
{
	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 0;

	if( outfd > 0 )
		fprintf(stderr,"Warning:  -o ignored, rays go to stdout\n");
}

view_eol() {
}

view_end()
{
	fflush(stdout);
}

view_2init()  {;}
