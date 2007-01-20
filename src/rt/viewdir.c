/*                       V I E W D I R . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
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
/** @file viewdir.c
 *
 *  RT-View-Module for printing out the hit point of a ray and the ray's
 *  direction on a user-specified grid.
 *
 *  Author -
 *	Susanne L. Muuss, J.D.
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */

#ifndef lint
static const char RCSraydir[] = "@(#)$Header$";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ext.h"
#include "rtprivate.h"


extern	int	width;			/* # of pixels in X; picture width */
extern int	npsw;			/* number of worker PSWs to run */



int		use_air = 0;		/* Internal air recognition is off */

int		using_mlib = 0;		/* Material routines NOT used */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL }
};


char usage[] = "\
Usage:  rtrange [options] model.g objects... >file.ray\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -o model.g	Specify output file (default=stdout)\n\
 -U #		Set use_air boolean to # (default=1)\n\
 -x #		Set librt debug flags\n\
";

int	rayhit(register struct application *ap, struct partition *PartHeadp), raymiss(register struct application *ap);

/*
 *  			V I E W _ I N I T
 *
 *  This routine is called by main().  It initializes the entire run, i.e.,
 *  it does things such as opening files, etc., which must be done before
 *  any other computations take place.  It is called only once per run.
 *  Pointers to rayhit() and raymiss() are set up and are later called from
 *  do_run().
 *
 */

int
view_init(register struct application *ap, char *file, char *obj, int minus_o)
{

	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 1;		/* only the first hit is considered */

	output_is_binary = 0;		/* output is not binary */

	return(0);			/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  A null-function.
 *  View_2init is called by do_frame(), which in turn is called by
 *  main() in rt.c.  This routine is called once per frame.  Static
 *  images only have one frame.  Animations have MANY frames, and bounding
 *  boxes, for example, need to be computed once per frame.
 *  Never preclude a new and nifty animation: rule: if it's a variable, it can
 *  change from frame to frame ( frame/picture width; angle between surface
 *  normals triggering shading.... etc).
 */

void
view_2init(struct application *ap)
{
	if( outfp == NULL )
		rt_bomb("outfp is NULL\n");

	/*
	 *  For now, RTVIEWDIR does not operate in parallel, while ray-tracing.
	 *  However, not dropping out of parallel mode here permits
	 *  tree walking and database prepping to still be done in parallel.
	 */
	if( npsw >= 1 )  {
		bu_log("Note: changing from %d cpus to 1 cpu\n", npsw );
		npsw = 1;		/* Disable parallel processing */
	}


}


/*
 *			R A Y M I S S
 *
 *  This function is called by rt_shootray(), which is called by
 *  do_frame(). Records coordinates where a miss is detected.
 */

int
raymiss(register struct application *ap)
{

	struct	cell	*posp;		/* store the current cell position */

	/* Getting defensive.... just in case. */
	if(ap->a_x > width)  {
		rt_bomb("raymiss: pixels exceed width\n");
	}



	return(0);
}

/*
 *			V I E W _ P I X E L
 *
 *  This routine is called from do_run(), and in this case does nothing.
 */

void
view_pixel(void)
{
	return;
}

void view_setup(void) {}
void view_cleanup(void) {}


/*
 *			R A Y H I T
 *
 *  Rayhit() is called by rt_shootray() when a hit is detected.  It
 *  computes the hit distance, the distance traveled by the
 *  ray, and the direction vector.
 *
 */

 int
rayhit(struct application *ap, register struct partition *PartHeadp)
{
	register struct partition *pp = PartHeadp->pt_forw;

	if( pp == PartHeadp )
		return(0);		/* nothing was actually hit?? */


	/* Getting defensive.... just in case. */
	if(ap->a_x > width)  {
		rt_bomb("rayhit: pixels exceed width\n");
	}


	/* Calculate the hit distance and the direction vector.  This is done
	 * by VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir).
	 */

	VJOIN1(pp->pt_inhit->hit_point, ap->a_ray.r_pt,
		pp->pt_inhit->hit_dist, ap->a_ray.r_dir);

	/* Print the information onto stdout.  The first three numbers are
	 * ray impact coordinates, and the next three numbers are the ray
	 * direction.  The line must be semi-colon terminated so that
	 * the output can be read by "miss" for use with PCAVAM.
	 */

	fprintf(stdout, "%g %g %g %g %g %g;\n",
	    pp->pt_inhit->hit_point[0], pp->pt_inhit->hit_point[1], pp->pt_inhit->hit_point[2],
            ap->a_ray.r_dir[0], ap->a_ray.r_dir[1], ap->a_ray.r_dir[2]);



	return(0);
}

/*
 *			V I E W _ E O L
 *
 *  View_eol() is called by rt_shootray() in do_run().
 *  This routine is called by worker.c whenever there is a full scanline.
 *  worker.c figures out what is a full scanline.  Whenever there
 *  is a full buffer in memory, the hit distances ar plotted.
 */

void	view_eol(struct application *ap)
{


}


/*
 *			V I E W _ E N D
 *
 *  View_end() is called by rt_shootray in do_run().
 */

void
view_end(struct application *ap)
{

	fflush(outfp);
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
