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
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCScheckview[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

int		use_air = 0;		/* Handling of air in librt */

int		using_mlib = 0;		/* Material routines NOT used */
int		max_bounces;		/* stub for "set" cmd */
int		max_ireflect;		/* stub for "set" cmd */

extern FILE	*outfp;

char usage[] = "Usage:  rtcheck [options] model.g objects...\n";

static int	noverlaps;		/* Number of overlaps seen */

/*
 *			H I T
 *
 * Null function -- handle a hit
 */
/*ARGSUSED*/
int
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
int
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
int
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

	RES_ACQUIRE( &rt_g.res_syscall );
	pdv_3line( outfp, ihit, ohit );
	noverlaps++;
	RES_RELEASE( &rt_g.res_syscall );

	return(0);	/* No further consideration to this partition */
}

/*
 *  			V I E W _ I N I T
 *
 *  Called once for this run.
 */
int
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
 *			V I E W _ 2 I N I T
 *
 *  Called at the beginning of each frame
 */
void
view_2init( ap )
register struct application *ap;
{
	register struct rt_i *rtip = ap->a_rt_i;
	
	pdv_3space( outfp, rtip->rti_pmin, rtip->rti_pmax );
	noverlaps = 0;
}

/*
 *			V I E W _ E N D
 *
 *  Called at the end of each frame
 */
void
view_end() {
	pl_flush(outfp);
	fflush(outfp);
	rt_log("%d overlaps detected\n", noverlaps);
}

/*
 *	Stubs
 */
void view_pixel() {}

void view_eol() {}
