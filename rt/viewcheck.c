/*
 *			V I E W C H E C K
 *
 *  Ray Tracing program RTCHECK bottom half.
 *
 *  This module outputs overlapping partitions, no other information.
 *  The partitions are written to the output file (typically stdout)
 *  as BRL-UNIX-plot 3-D floating point lines, so that they can be
 *  processed by any tool that reads UNIX-plot.  Because the BRL UNIX
 *  plot format is defined in a machine independent way, this program
 *  can be run anywhere, and the results piped back for local viewing,
 *  for example, on a workstation.
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
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCScheckview[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

extern FILE	*outfp;

char usage[] = "Usage:  rtcheck [options] model.g objects...\n";

/*
 *			H I T
 *
 * Null function -- handle a hit
 */
/*ARGSUSED*/
hit( ap, PartHeadp )
struct application *ap;
register struct partition *PartHeadp;
{
	return	1;
}

/*
 *			M I S S
 *
 *  Null function -- handle a miss
 */
/*ARGSUSED*/
miss( ap )
struct application *ap;
{
	return	1;
}

/*
 *			O V E R L A P
 *
 *  Write end points of partition to the standard output.
 */
overlap( ap, pp, reg1, reg2 )
struct application	*ap;
struct partition	*pp;
struct region		*reg1;
struct region		*reg2;
{	
	register struct xray	*rp = &ap->a_ray;
	register struct hit	*ihitp = pp->pt_inhit;
	register struct hit	*ohitp = pp->pt_outhit;
	vect_t	ihit;
	vect_t	ohit;

	VJOIN1( ihit, rp->r_pt, ihitp->hit_dist, rp->r_dir );
	VJOIN1( ohit, rp->r_pt, ohitp->hit_dist, rp->r_dir );

	/* Not PARALLEL -- needs syscall resource lock around this call */
	pdv_3line( outfp, ihit, ohit );
	return(0);	/* No further consideration to this partition */
}

/*
 *  			V I E W _ I N I T
 */
view_init( ap, file, obj, npts, minus_o )
register struct application *ap;
char *file, *obj;
{
	ap->a_hit = hit;
	ap->a_miss = miss;
	ap->a_overlap = overlap;
	ap->a_onehit = 0;
	return	0;		/* No framebuffer needed */
}

/*
 *	Stubs
 */
view_pixel() {}

view_eol() {}

view_end() {
	pl_flush(outfp);
	fflush(outfp);
}

view_2init( ap )
register struct application *ap;
{
	register struct rt_i *rtip = ap->a_rt_i;
	
	pdv_3space( outfp, rtip->rti_pmin, rtip->rti_pmax );
	return	0;
}

mlib_setup() { 
	return(1); 
}

mlib_free() { ; }
