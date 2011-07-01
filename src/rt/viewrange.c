/*                     V I E W R A N G E . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2011 United States Government as represented by
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
/** @file rt/viewrange.c
 *
 *  RT-View-Module for visualizing range data.  The output is a
 *  UNIX-Plot file.  Direction vectors are preserved so that
 *  perspective is theoretically possible.
 *  The algorithm is based on plotting all the hit distances for all
 *  the pixels ray-traced.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"

/* private */
#include "./rtuif.h"
#include "./ext.h"


#define CELLNULL ( (struct cell *) 0)

struct cell {
    float	c_dist;			/* distance from emanation plane to in_hit */
    point_t	c_hit;			/* 3-space hit point of ray */
};

extern size_t   width;			/* # of pixels in X; picture width */
extern int	npsw;			/* number of worker PSWs to run */
float		max_dist;
struct cell	*cellp;

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};


const char title[] = "RT Range Plot";

void
usage(const char *argv0)
{
    bu_log("Usage:  %s [options] model.g objects... >file.ray\n", argv0);
    bu_log("Options:\n");
    bu_log(" -s #		Grid size in pixels, default 512\n");
    bu_log(" -a Az		Azimuth in degrees	(conflicts with -M)\n");
    bu_log(" -e Elev	Elevation in degrees	(conflicts with -M)\n");
    bu_log(" -M		Read model2view matrix on stdin (conflicts with -a, -e)\n");
    bu_log(" -o model.g	Specify output file (default=stdout)\n");
    bu_log(" -U #		Set use_air boolean to # (default=0)\n");
    bu_log(" -x #		Set librt debug flags\n");
}


int	rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
int	raymiss(register struct application *ap);


/*
 *  			V I E W _ I N I T
 *
 *  This routine is called by main().  It initializes the entire run, i.e.,
 *  it does things such as opening files, etc., which must be done before
 *  any other computations take place.  It is called only once per run.
 *  Pointers to rayhit() and raymiss() are set up and are later called from
 *  do_run().
 */

int
view_init(struct application *ap, char *UNUSED(file), char *UNUSED(obj), int UNUSED(minus_o), int UNUSED(minus_F))
{

    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 1;

    output_is_binary = 1;		/* output is binary */

    return 0;			/* No framebuffer needed */
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
view_2init(struct application *ap, char *UNUSED(framename))
{
    if ( outfp == NULL )
	bu_exit(EXIT_FAILURE, "outfp is NULL\n");

    /*
     *  For now, RTRANGE does not operate in parallel, while ray-tracing.
     *  However, not dropping out of parallel mode until here permits
     *  tree walking and database prepping to still be done in parallel.
     */
    if ( npsw >= 1 )  {
	bu_log("Note: changing from %d cpus to 1 cpu\n", npsw );
	npsw = 1;		/* Disable parallel processing */
    }


    /* malloc() a buffer that has room for as many struct cell 's
     * as the incoming file is wide (width).
     * Rather than using malloc(), though, bu_malloc() is used.  This
     * has the advantage of inbuild error-checking and automatic aborting
     * if there is no memory.  Also, bu_malloc() takes a string as its
     * final parameter: this tells the user exactly where memory ran out.
     */


    cellp = (struct cell *)bu_malloc(sizeof(struct cell) * width,
				     "cell buffer" );


    /* Obtain the maximun distance within the model to use as the
     * background distance.  Also get the coordinates of the model's
     * bounding box and feed them to
     * pdv_3space.  This will allow the image to appear in the plot
     * starting with the same size as the model.
     */

    pdv_3space(outfp, ap->a_rt_i->rti_pmin, ap->a_rt_i->rti_pmax);

    /* Find the max dist fron emantion plane to end of model
     * space.  This can be twice the radius of the bounding
     * sphere.
     */

    max_dist = 2 * (ap->a_rt_i->rti_radius);
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
    if ((size_t)ap->a_x > width)  {
	bu_exit(EXIT_FAILURE, "raymiss: pixels exceed width\n");
    }

    posp = &(cellp[ap->a_x]);

    /* Find the hit point for the miss. */

    VJOIN1(posp->c_hit, ap->a_ray.r_pt, max_dist, ap->a_ray.r_dir);
    posp->c_dist = max_dist;

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

void view_setup(struct rt_i *UNUSED(rtip)) {}
void view_cleanup(struct rt_i *UNUSED(rtip)) {}


/*
 *			R A Y H I T
 *
 *  Rayhit() is called by rt_shootray() when a hit is detected.  It
 *  computes the hit distance, the distance traveled by the
 *  ray, and the direction vector.
 *
 */

int
rayhit(struct application *ap, register struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    register struct partition *pp = PartHeadp->pt_forw;
    struct	cell	*posp;		/* stores current cell position */


    if ( pp == PartHeadp )
	return 0;		/* nothing was actually hit?? */


    /* Getting defensive.... just in case. */
    if ((size_t)ap->a_x > width)  {
	bu_exit(EXIT_FAILURE, "rayhit: pixels exceed width\n");
    }

    posp = &(cellp[ap->a_x]);

    /* Calculate the hit distance and the direction vector.  This is done
     * by VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir).
     */

    VJOIN1(pp->pt_inhit->hit_point, ap->a_ray.r_pt,
	   pp->pt_inhit->hit_dist, ap->a_ray.r_dir);

    /* Now store the distance and the direction vector as appropriate.
     * Output the ray data: screen plane (pixel) coordinates
     * for x and y positions of a ray, region_id, and hit_distance.
     * The x and y positions are represented by ap->a_x and ap->a_y.
     *
     *  Assume all rays are parallel.
     */

    posp->c_dist = pp->pt_inhit->hit_dist;
    VMOVE(posp->c_hit, pp->pt_inhit->hit_point);

    return 0;
}

/*
 *			V I E W _ E O L
 *
 *  View_eol() is called by rt_shootray() in do_run().
 *  This routine is called by worker.c whenever there is a full scanline.
 *  worker.c figures out what is a full scanline.  Whenever there
 *  is a full buffer in memory, the hit distances ar plotted.
 */

void
view_eol(struct application *UNUSED(ap))
{
    struct cell	*posp;
    size_t i;
    int		cont;		/* continue flag */

    posp = &(cellp[0]);
    cont = 0;

    /* Plot the starting point and set cont to 0.  Then
     * march along the entire array and continue to plot the
     * hit points based on their distance from the emanation
     * plane. When consecutive hit-points with identical distances
     * are found, cont is set to one so that the entire sequence
     * of like-distanced hit-points can be plotted together.
     */

    pdv_3move( outfp, posp->c_hit );

    for ( i = 0; i < width-1; i++, posp++ )  {
	if (EQUAL(posp->c_dist, (posp+1)->c_dist))  {
	    cont = 1;
	    continue;
	} else  {
	    if (cont)  {
		pdv_3cont(outfp, posp->c_hit);
		cont = 0;
	    }
	    pdv_3cont(outfp, (posp+1)->c_hit);
	}
    }

    /* Catch the boundary condition if the last couple of cells
     * heve the same distance.
     */

    pdv_3cont(outfp, posp->c_hit);
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
