/*                      V I E W C E L L . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
/** @file rt/viewcell.c
 *
 *  Ray Tracing program RTCELL bottom half.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"

#include "./rtuif.h"
#include "./ext.h"


#define	MM2IN	0.03937008		/* mm times MM2IN gives inches */


extern fastf_t	gift_grid_rounding;
extern point_t	viewbase_model;

extern int	npsw;			/* number of worker PSWs to run */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

static mat_t	model2hv;		/* model coords to GIFT h, v in inches */


const char title[] = "RT Cell";

void
usage(const char *argv0)
{
    bu_log("Usage:  %s [options] model.g objects... >file.cell\n", argv0);
    bu_log("Options:\n");
    bu_log(" -s #		Grid size in pixels, default 512\n");
    bu_log(" -a Az		Azimuth in degrees	(conflicts with -M)\n");
    bu_log(" -e Elev	Elevation in degrees	(conflicts with -M)\n");
    bu_log(" -M		Read model2view matrix on stdin (conflicts with -a, -e)\n");
    bu_log(" -g #		Grid cell width in millimeters (conflicts with -s\n");
    bu_log(" -G #		Grid cell height in millimeters (conflicts with -s\n");
    bu_log(" -J #		Jitter.  Default is off.  Any non-zero number is on\n");
    bu_log(" -o model.cell	Specify output file (default=stdout)\n");
    bu_log(" -U #		Set use_air boolean to # (default=0)\n");
    bu_log(" -x #		Set librt debug flags\n");
}


int	rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
int     raymiss(register struct application *ap);


/*
 *  			V I E W _ I N I T
 *
 *  This routine is called by main().  It prints the overall shotline
 *  header. Furthermore, pointers to rayhit() and raymiss() are set up
 *  and are later called from do_run().
 */
int
view_init(register struct application *ap, char *UNUSED(file), char *UNUSED(obj), int minus_o, int UNUSED(minus_F))
{

    if ( !minus_o )
	outfp = stdout;

    /*
     *  Cause grid_setup() to align the grid on one inch boundaries,
     *  or cell_width boundaries, if it is given.
     */
    if ( cell_width > 0 )
	gift_grid_rounding = cell_width;
    else
	gift_grid_rounding = 25.4;		/* one inch */

    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 0;

    output_is_binary = 0;		/* output is printable ascii */

    return 0;		/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  View_2init is called by do_frame(), which in turn is called by
 *  main() in rt.c.
 *
 */
void
view_2init(struct application *UNUSED(ap), char *UNUSED(framename))
{

    if ( outfp == NULL )
	bu_exit(EXIT_FAILURE, "outfp is NULL\n");

    /*
     *  Not dropping out of parallel mode until here permits
     *  tree walking and database prepping to still be done in parallel.
     */
    if ( npsw >= 1 )  {
	bu_log("Note: changing from %d cpus to 1 cpu\n", npsw );
	npsw = 1;		/* Disable parallel processing */
    }

    /*
     *  GIFT uses an H, V coordinate system that is anchored at the
     *  model origin, but rotated according to the view.
     *  For convenience later, build a matrix that will take
     *  a point in model space (with units of mm), and convert it
     *  to a point in HV space, with units of inches.
     */
    MAT_COPY( model2hv, Viewrotscale );
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
raymiss(register struct application *UNUSED(ap))
{
    return 0;
}

/*
 *			V I E W _ P I X E L
 *
 *  This routine is called from do_run(), and in this case does nothing.
 */
void
view_pixel(struct application *UNUSED(ap))
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
rayhit(struct application *ap, register struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    register struct partition *pp = PartHeadp->pt_forw;
    point_t			hv;		/* GIFT h, v coords, in inches */
    fastf_t			dot;
    vect_t			normal;

    if ( pp == PartHeadp )
	return 0;		/* nothing was actually hit?? */

    if ( jitter & JITTER_CELL )  {
	/*
	 *  Find exact h, v coordinates of actual ray start by
	 *  projecting start point into GIFT h, v coordinates.
	 */
	MAT4X3PNT( hv, model2hv, ap->a_ray.r_pt );
    } else {
	/*
	 *  Find the H, V coordinates of the grid cell _center_,
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
    if ( dot < 0 )  dot = 0;
    fprintf( outfp, "%g %g %g\n",
	     hv[0], hv[1], dot );

    return 0;
}

/*
 *			V I E W _ E O L
 *
 *  View_eol() is called by rt_shootray() in do_run().  In this case,
 *  it does nothing.
 */
void
view_eol(struct application *UNUSED(ap))
{
}

/*
 *			V I E W _ E N D
 *
 *  View_end() is called by rt_shootray in do_run().
 */
void
view_end(struct application *UNUSED(ap))
{
    fflush(outfp);
}

void view_setup(struct rt_i *UNUSED(rtip)) {}
void view_cleanup(struct rt_i *UNUSED(rtip)) {}

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
