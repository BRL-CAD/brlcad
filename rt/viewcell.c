/*
 *			V I E W C E L L
 *
 *  Ray Tracing program RTCELL bottom half.
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
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCScell[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./material.h"
#include "./ext.h"

#include "rdebug.h"

#define	MM2IN	0.03937008		/* mm times MM2IN gives inches */


extern fastf_t	gift_grid_rounding;
extern point_t	viewbase_model;

extern int	npsw;			/* number of worker PSWs to run */

int		use_air = 0;		/* Handling of air in librt */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,		FUNC_NULL }
};

static mat_t	model2hv;		/* model coords to GIFT h,v in inches */

char usage[] = "\
Usage:  rtcell [options] model.g objects... >file.cell\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -g #		Grid cell width in millimeters (conflicts with -s\n\
 -G #		Grid cell height in millimeters (conflicts with -s\n\
 -J #		Jitter.  Default is off.  Any non-zero number is on\n\
 -o model.cell	Specify output file (default=stdout)\n\
 -U #		Set use_air boolean to # (default=1)\n\
 -x #		Set librt debug flags\n\
";

int	rayhit(), raymiss();

/*
 *  			V I E W _ I N I T
 *
 *  This routine is called by main().  It prints the overall shotline
 *  header. Furthermore, pointers to rayhit() and raymiss() are set up
 *  and are later called from do_run().
 */
int
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{

	if( !minus_o )
		outfp = stdout;
	
	/*
	 *  Cause grid_setup() to align the grid on one inch boundaries,
	 *  or cell_width boundaries, if it is given.
	 */
	if( cell_width > 0 )
		gift_grid_rounding = cell_width;
	else
		gift_grid_rounding = 25.4;		/* one inch */

	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 0;

	output_is_binary = 0;		/* output is printable ascii */

	return(0);		/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  View_2init is called by do_frame(), which in turn is called by
 *  main() in rt.c.
 * 
 */
void
view_2init( ap )
struct application	*ap;
{

	if( outfp == NULL )
		rt_bomb("outfp is NULL\n");

	/*
	 *  Not dropping out of parallel mode until here permits
	 *  tree walking and database prepping to still be done in parallel.
	 */
	if( npsw >= 1 )  {
		rt_log("Note: changing from %d cpus to 1 cpu\n", npsw );
		npsw = 1;		/* Disable parallel processing */
	}

	/*
	 *  GIFT uses an H,V coordinate system that is anchored at the
	 *  model origin, but rotated according to the view.
	 *  For convenience later, build a matrix that will take
	 *  a point in model space (with units of mm), and convert it
	 *  to a point in HV space, with units of inches.
	 */
	mat_copy( model2hv, Viewrotscale );
	model2hv[15] = 1/MM2IN;
}

/*
 *			R A Y M I S S
 *
 *  Null function -- handle a miss
 *  This function is called by rt_shootray(), which is called by
 *  do_frame().
 */
int
raymiss()
{
	return(0);
}

/*
 *			V I E W _ P I X E L
 *
 *  This routine is called from do_run(), and in this case does nothing.
 */
void
view_pixel()
{
	return;
}

/*
 *			R A Y H I T
 *
 *  Rayhit() is called by rt_shootray() when the ray hits one or more objects.
 *  Write information about this "cell".
 */
int
rayhit( ap, PartHeadp )
struct application *ap;
register struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	point_t			hv;		/* GIFT h,v coords, in inches */
	fastf_t			dot;
	vect_t			normal;

	if( pp == PartHeadp )
		return(0);		/* nothing was actually hit?? */

	if( jitter & JITTER_CELL )  {
		/*
		 *  Find exact h,v coordinates of actual ray start by
		 *  projecting start point into GIFT h,v coordinates.
		 */
		MAT4X3PNT( hv, model2hv, ap->a_ray.r_pt );
	} else {
		/*
		 *  Find the H,V coordinates of the grid cell _center_,
		 *  for GIFT compatible behavior.
		 *  RT uses the lower left _corner_ of each cell.
		 */
		point_t		center;
		fastf_t		dx;
		fastf_t		dy;

		dx = ap->a_x + 0.5;
		dy = ap->a_y + 0.5;
		VJOIN2( center, viewbase_model, dx, dx_model, dy, dy_model );
		MAT4X3PNT( hv, model2hv, center );
	}

	RT_HIT_NORMAL( normal, pp->pt_inhit, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );
	dot = -VDOT( normal, ap->a_ray.r_dir );
	if( dot < 0 )  dot = 0;
	fprintf( outfp, "%g %g %g\n",
		hv[0], hv[1], dot );

	return(0);
}

/*
 *			V I E W _ E O L
 *
 *  View_eol() is called by rt_shootray() in do_run().  In this case,
 *  it does nothing.
 */
void	view_eol()
{
}

/*
 *			V I E W _ E N D
 *
 *  View_end() is called by rt_shootray in do_run().
 */
void
view_end()
{
	fflush(outfp);
}

void view_setup() {}
void view_cleanup() {}

void application_init () {}
