/*                      V I E W H I D E . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
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
/** @file viewhide.c
 *
 *  Ray Tracing program RTHIDE bottom half.
 *
 *  This module utilizes the RT library to interrogate a MGED
 *  model and plots a hidden-line removed UnixPlot file.
 *  This is accomplished by comparing region-id codes between
 *  pixels.  A vertical or horizontal line is plotted when a change in
 *  region-id codes is detected.
 *
 *  At present, the main use for this module is to generate
 *  UnixPlot file that can be read by various existing filters
 *  to produce a bas-relief line drawing of an MGED object: i.e.
 *  flat when viewed head-on, but with relief detail when seen at
 *  an angle.
 *
 *  This is based on previous work done by Michael John Muuss.
 *
 *  Author -
 *	Susanne L. Muuss, J.D.
 *	Michael J. Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
#ifndef lint
static const char RCSrayhide[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"

/* private */
#include "./ext.h"
#include "rtprivate.h"


#define SEEKING_START_PT 0
#define FOUND_START_PT 1
#define CELLNULL ( (struct cell *) 0)
#define ID_BACKGROUND (-999)

struct cell {
	float	c_dist;			/* distance from emanation plane to in_hit */
	int	c_id;			/* region_id of component hit */
	point_t	c_hit;			/* 3-space hit point of ray */
	vect_t	c_normal;		/* surface normal at the hit point */
	vect_t	c_rdir;			/* ray direction, permits perspective */
};

extern	int	width;			/* # of pixels in X; picture width */
extern	double	AmbientIntensity;	/* angle bet. surface normals; default of 5deg */
extern int	npsw;			/* number of worker PSWs to run */

fastf_t		pit_depth;		/* min. distance for drawing pits/mountains */
fastf_t		maxangle;		/* value of the cosine of the angle bet. surface normals that triggers shading */

void		swapbuff(struct cell **onepp, struct cell **twopp);
void		cleanline(struct cell *inbuffp, int file_width);
void		horiz_cmp(struct cell *botp, int mem_width, int y);
void		vert_cmp(struct cell *botp, struct cell *topp, int mem_width, int y);
struct cell	*find_cell(struct cell *cur_cellp, struct cell *next_cellp);
struct cell	*botp;			/* pointer to bottom line   */
struct cell	*topp;			/* pointer to top line	    */


int		use_air = 0;		/* Internal air recognition is off */

int		using_mlib = 0;		/* Material routines NOT used */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL }
};


const char title[] = "RT Hidden-Line Plot";
const char usage[] = "\
Usage:  rthide [options] model.g objects... >file.pl\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -A angle	Angle between surface normals (default=5degrees)\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -o output.pl	Specify output file (default=stdout)\n\
 -U #		Set use_air boolean to # (default=1)\n\
 -x #		Set librt debug flags\n\
";


int	rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *);
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
view_init(register struct application *ap, char *file, char *obj, int minus_o)
{

	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 1;

	output_is_binary = 1;		/* output is binary */

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
	 *  For now, RTHIDE does not operate in parallel, while ray-tracing.
	 *  However, not dropping out of parallel mode until here permits
	 *  tree walking and database prepping to still be done in parallel.
	 */
	if( npsw >= 1 )  {
		bu_log("Note: changing from %d cpus to 1 cpu\n", npsw );
		npsw = 1;		/* Disable parallel processing */
	}

	/* malloc() two buffers that have room for as many struct cell 's
	 * as the incoming file is wide (width), plus two for the border.
	 * Rather than using malloc(), though, bu_malloc() is used.  This
	 * has the advantage of inbuild error-checking and automatic aborting
	 * if there is no memory.  Also, bu_malloc() takes a string as its
	 * final parameter: this tells the usr exactly where memory ran out.
	 * The file_height is counted by using ap->a_y directly. The benefit
	 * of this is WHAT?
	 */


	botp = (struct cell *)bu_malloc(sizeof(struct cell) * (width + 2),
		"bottom cell buffer" );
	topp = (struct cell *)bu_malloc(sizeof(struct cell) * (width + 2),
		"top cell buffer" );

	/* Clear both in-buffers to ensure abscence of garbage.  Note
	 * that the zero-filled "bottom" buffer now provides the first
	 * in-memory buffer for comparisons.
	 */

	cleanline(botp, width);
	cleanline(topp, width);


	/* Determine the angle between surface normal below which shading
	 * will take place.  The default is the for less than the cosine
	 * of 5 degrees, there will be shading.  With the -A option, the
	 * user can specify other number of degrees below which he wants
	 * shading to take place.  Note that the option for ambient light
	 * intensity had been reused since that will not be needed here.
	 * The default of 5 degrees is used when AmbientIntensity is less
	 * than 0.5 because it's default is set to 0.4, and the permissible
	 * range for light intensity is 0 -> 1.
	 */

	if( AmbientIntensity <= 0.5 )  {
		maxangle = cos( 5.0 * bn_degtorad);
	} else {
		maxangle = cos( AmbientIntensity * bn_degtorad);
	}

	/* Obtain the bounding boxes for the model from the rt_i(stance)
	 * structure and feed the maximum and minimum coordinates to
	 * pdv_3space.  This will allow the image to appear in the plot
	 * starting with the same size as the model.
	 */

	pdv_3space(outfp, ap->a_rt_i->rti_pmin, ap->a_rt_i->rti_pmax);

	/* Now calculated and store the minimun depth change that will
	 * trigger the drawing of "pits" and "pendula" (mountains).  In
	 * this case, a change in distance of 2 pixels was picked.  Note
	 * that the distance of one pixel in model space is MAGNITUDE(dx_model).
	 * This is calculated once per frame dx_model may be different in
	 * another frame.
	 */

	pit_depth = 4 * MAGNITUDE( dx_model );


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

	posp = &(topp[ap->a_x + 1]);

	/*
	 * cleanline() zero-fills a buffer.  Therefore, it is possible to
	 * let this "line scrubber" do all the zero-filling for raymiss()
	 * by calling it before EVERY new topbuff is filled.  This would
	 * result in very inefficient code.  Thus, even on a miss, raymiss()
	 * will do its own zero-filling of the distance, region_id, surface
	 * normal, and the hit distance.  This prevents the image from being
	 * "smeared".
	 */

	posp->c_id = ID_BACKGROUND;
	posp->c_dist = 0;
	VSET(posp->c_hit, 0, 0, 0);
	VSET(posp->c_normal, 0, 0, 0);
	VSET(posp->c_rdir, 0, 0, 0);

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
 *  computes the hit distance, the region_id, the distance traveled by the
 *  ray, and the surface normal at the hit point.
 *
 */

int
rayhit(struct application *ap, register struct partition *PartHeadp, struct seg *segp)
{
	register struct partition *pp = PartHeadp->pt_forw;
	struct	cell	*posp;			/* stores current cell position */
	register struct hit	*hitp;		/* which hit */

	if( pp == PartHeadp )
		return(0);		/* nothing was actually hit?? */


	/* Getting defensive.... just in case. */
	if(ap->a_x > width)  {
		rt_bomb("rayhit: pixels exceed width\n");
	}

	posp = &(topp[ap->a_x + 1]);

	/* Ensure that inhit is in front of emanation plane */
	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_inhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		bu_log("rthide/rayhit:  no hit out front? x%d y%d lvl%d\n",
			ap->a_x, ap->a_y, ap->a_level);
		return(0);
	}
	hitp = pp->pt_inhit;

	VMOVE(posp->c_rdir, ap->a_ray.r_dir);
	VJOIN1( posp->c_hit, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir );

	/* Calculate the hit normal and the hit distance. */
	RT_HIT_NORMAL( posp->c_normal, hitp, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);


	/* Now store the distance and the region_id in the appropriate
	 * cell struct: i.e., ap->a_x (the x-coordinate) +1 within the
	 * array of cell structs.
	 * Extract the hit distance and the hit normals from the hit structure
	 * and store in the cell structure.
	 * Since repeatedly computing topp[ap->a_x + 1] is very inefficient,
	 * the value of ap->a_x + 1 will be stored in a struct cell pointer
	 * to vitiate the need to recompute this value repeatedly. LATER.
	 */
	posp->c_dist = hitp->hit_dist;

	/*
	 * Output the ray data: screen plane (pixel) coordinates
	 * for x and y positions of a ray, region_id, and hit_distance.
	 * The x and y positions are represented by ap->a_x and ap->a_y.
	 *
	 *  Assume all rays are parallel.
	 */

	posp->c_id = pp->pt_regionp->reg_regionid;

	/* make sure that if there is a hit, the region_id is not the
	 * same as the background.  If it is, set to 1.
	 */
	if(posp->c_id == ID_BACKGROUND)
		posp->c_id = 1;
	return(0);
}

/*
 *			V I E W _ E O L
 *
 *  View_eol() is called by rt_shootray() in do_run().
 *  This routine is called by worker.c whenever there is a full scanline.
 *  worker.c figures out what is a full scanline.  Whenever there
 *  are two full buffers in memory, the horizontal and the vertical compares
 *  are done, as well as any plotting.  Then the "bottom" buffer is "dropped",
 *  and a new one is read into memory until end-of-file is reached.
 */

void	view_eol(struct application *ap)
{


	/* Now add 2 pixels to file_width to convert it to memory_width
	 * for doing the comparisons and determining the boundaries around
	 * the picture.  Note that the file_height is simply expressed as
	 * ap->a_y.  It is not necessary to add 2 pixels to it for the
	 * boundary because that is taken care of by originally allocating
	 * the bottom cell buffer, and at the end by doing one extra top
	 * cell buffer.
	 */

	horiz_cmp(botp, width + 2, ap->a_y);
	vert_cmp(botp, topp, width + 2, ap->a_y);
	swapbuff(&botp, &topp);

}


/*
 *			V I E W _ E N D
 *
 *  View_end() is called by rt_shootray in do_run().  It is necessary to
 *  iterate one more time through the horizontal and vertical comparisons
 *  to put down a top border.  This routine is responsible for doing this.
 *  Note also that since view_end() takes an application structure pointer,
 *  the file height can be expressed directly as ap->a_y.  One might consider
 *  allocating a height variable set to this value....
 */

void
view_end(struct application *ap)
{

	cleanline(topp, width);
	horiz_cmp(botp, width + 2, ap->a_y);
	vert_cmp(botp, topp, width + 2, ap->a_y);

	fflush(outfp);
}


/*
 *		H O R I Z O N T A L   C O M P A R I S O N
 *
 *  This routine takes three parameters: a pointer to a "bottom" buffer, the
 *  line width of the incoming file plus two border pixels (mem_width), and
 *  a y-coordinate (file_height).  It returns nothing.
 *
 *  Pixels on a scanline are compared to see if their region_id codes
 *  are the same.  If these are not identical, a vertical line is
 *  plotted to mark to boundary where the region_id codes change.  Likewise,
 *  the size of the angle between surface normals is checked: if it exceeds
 *  maxangle, then a line s drawn.
 */

void
horiz_cmp(struct cell *botp, int mem_width, int y)
{
	int		x;
	struct	cell	*cellp;
	vect_t		start;		/* start of vector */
	vect_t		stop;		/* end of vector */

	for (x=0; x < (mem_width-1); x++, botp++)  {

		/* If the region_ids of neighboring pixels do
		 * not match, compare their hit distances.  If
		 * either distance is zero, select the non-zero
		 * distance for plotting; otherwise, select the
		 * lesser of the two distances.
		 * Check the angle between surface normals to see
		 * whether a line needs to be drawn.  This is accomplished
		 * by finding the cosine of the angle between the two
		 * vectors with VDOT(), the dot product.  The result
		 * is compared against maxangle, which must be determined
		 * experimentally.  This scheme will prevent curved surfaces,
		 * on which practically "every point is a surface", from
		 * being represented as dark blobs.
		 * Note that maxangle needs to be greater than the cosine
		 * of the angle between the two vectors because as the angle
		 * between the increases, the cosine of said angle decreases.
		 * Also of interest is that one needs to say: plot if id's
		 * are not the same OR if either id is not 0 AND the cosine
		 * of the angle between the normals is less than maxangle.
		 * This test prevents the background from being shaded in.
		 * Furthermore, it is necessary to select the hit_point.
		 * Check for pits and pendula.  The below if statement can
		 * be translated as follows: if(ids don't match ||
		 * (cur_id is not 0 && ( (there's a pit) || (there's a mtn) ).
		 */

		if (botp->c_id != (botp+1)->c_id ||
		   ( botp->c_id != ID_BACKGROUND &&
		   ( (botp->c_dist + pit_depth < (botp+1)->c_dist) ||
		     ((botp+1)->c_dist + pit_depth < botp->c_dist)  ||
		     (VDOT(botp->c_normal, (botp + 1)->c_normal) < maxangle))))  {

			cellp = find_cell(botp, (botp+1));

			/* Note that the coordinates must be expressed
			 * as MODEL coordinates.  This can be done by
			 * adding offsets to the hit_point.  Thus, 0.5*
			 * dx_model means moving 0.5 of a cell in model
			 * space, and replaces (x -1 +0.5) representing
			 * backing up one whole cell and then moving to
			 * the center of the new cell in file coordinates.
			 * In that case, the x represented the screen coords.
			 * Now, make the beginning point and the ending point.
			 */

			/* To make sure that all the vertical lines are in the
			 * correct place, if cellp is the same as botp, then
			 * to start,move half a cell right to start, else move half a
			 * cell left; and to end, move right and up one half cell.
			 */

			if(botp == cellp)  {
				VJOIN2(start, cellp->c_hit, 0.5, dx_model, -0.5, dy_model);
				VJOIN2(stop, cellp->c_hit, 0.5, dx_model, 0.5, dy_model);
			} else {
				VJOIN2(start, cellp->c_hit, -0.5, dx_model, -0.5, dy_model);
				VJOIN2(stop, cellp->c_hit, -0.5, dx_model, 0.5, dy_model);
			}

			pdv_3line(outfp, start, stop);

		}
	}
}

/*
 *		V E R T I C A L  C O M P A R I S O N
 *
 *  This routine takes four parameters: a pointer to a "top" buffer, a pointer
 *  to a "bottom" buffer, the file_width + two border pixels (mem_width), and
 *  a y-coordinate (line-count, or file_height).  It returns nothing.
 *
 *  Pixels residing on adjacent scanlines are compared to determine
 *  whether their region_id codes are the same.  If these are not
 *  identical, a horizontal line is plotted to mark the boundary where
 *  the region_id codes change.
 *
 */

void
vert_cmp(struct cell *botp, struct cell *topp, int mem_width, int y)
{

	register int	x;
	struct	 cell	*cellp;
	struct	 cell	*start_cellp;
	int		state;
	vect_t		start;
	vect_t		stop;

	VSET(start, 0, 0, 0);	/* cleans out start point... a safety */

	state = SEEKING_START_PT;

	/* If the region_ids are not equal OR either region_id is not 0 AND
	 * the cosine of the angle between the normals is less than maxangle,
	 * plot a line or shade the plot to produce surface normals or give
	 * a sense of curvature.
	 */

	for (x=0; x < mem_width; x++, botp++, topp++)  {

		/* If the id's are not the same, or if bottom id is not
		 * zero, find pits (botp->c_dist+pit_depth < topp->c_dist)
		 * and mountains (topp->c_dist+pit_depth < botp->c_dist).
		 */

		if (botp->c_id != topp->c_id ||
		   ( botp->c_id != ID_BACKGROUND &&
		     ((botp->c_dist + pit_depth < topp->c_dist) ||
		      (topp->c_dist + pit_depth < botp->c_dist) ||
		      (VDOT(botp->c_normal, topp->c_normal) < maxangle))))  {
			if( state == FOUND_START_PT ) {
				continue;
			} else {
				/* find the correct cell. */
				start_cellp = find_cell(botp, topp);

				/* Move to and remember left point.  If start_cellp
				 * is botp, then move left and up half a cell.
				 */

				if(botp == start_cellp)  {
					VJOIN2(start, start_cellp->c_hit, -0.5, dx_model, 0.5, dy_model);
				} else  {
					VJOIN2(start, start_cellp->c_hit, -0.5, dx_model, -0.5, dy_model);
				}

				state = FOUND_START_PT;
			}
		} else {
			/* points are the same */

			if (state == FOUND_START_PT) {

				/* Draw to current left edge
				 * Note that x and y must be converted back
				 * to file coordinates so that the file
				 * picture gets plotted.  The 0.5 factors
				 * are for centering. This is for (x-1-0.5),
				 * y-1+0.5).  These file coordinate must then
				 * be expressed in model space.  That is done
				 * by starting at the hit_pt and adding or
				 * subtracting 0.5 cell for centering.
				 */

				cellp = find_cell( (botp-1), (topp-1) );

				/* If botp-1 is cellp, then move right and up
				 * by half a cell.  Otherwise, move right and down
				 * by half a cell.
				 */

				if( (botp-1) == cellp)  {
					VJOIN2(stop, cellp->c_hit, 0.5, dx_model, 0.5, dy_model);
				} else {
					VJOIN2(stop, cellp->c_hit, 0.5, dx_model, -0.5, dy_model);
				}

				pdv_3line(outfp, start, stop);
				state = SEEKING_START_PT;
			} else {
				continue;
			}
		}
	}

	/* Now check for end of scan-line. */
	if (state == FOUND_START_PT) {

			/* Note that x and y must be converted back
			 * to file coordinates so that the file
			 * picture gets plotted.  The 0.5 factors
			 * are for centering.  This is for (x-1-0.5), (y-1+0.5).
			 * These file coordinates must then be expressed in
			 * model space.  That is done by starting at the
			 * hit_pt and adding or subtracting 0.5 cell for
			 * centering.
			 */

		cellp = find_cell( (botp-1), (topp-1) );

			/* If botp-1 is cellp, then move right and up
			 * by half a cell.  Otherwise, move right and down
			 * by half a cell.
			 */

		if( (botp-1) == cellp)  {
			VJOIN2(stop, cellp->c_hit, 0.5, dx_model, 0.5, dy_model);
		} else {
			VJOIN2(stop, cellp->c_hit, 0.5, dx_model, -0.5, dy_model);
		}

		pdv_3line(outfp, start, stop);
		state = SEEKING_START_PT;
	}
}


/*
 *	           F I N D_ C E L L
 *
 *  This routine takes pointers to two cells.  This is more efficient (takes
 *  less space) than sending the hit_distances.  Furthermore, by selecting
 *  a cell, rather than just a distance, more information becomes available
 *  to the calling routine.
 *  If the region_ids of neighboring pixels do not match, compare their
 *  respective hit_distances.  If either distance is zero, select the
 *  non-zero distance for plotting; otherwise, select the lesser of the
 *  two distances.  Return a pointer to the cell with the smaller hit_distance.
 *  Using this hit_distance will be more esthetically pleasing for the bas-
 *  relief.
 */

struct	cell	*
find_cell (struct cell *cur_cellp, struct cell *next_cellp)
{
	struct cell	*cellp;

	if (cur_cellp->c_dist == 0)
		cellp = next_cellp;
	else if (next_cellp->c_dist == 0)
		cellp = cur_cellp;
	else if (cur_cellp->c_dist < next_cellp->c_dist )
		cellp = cur_cellp;
	else
		cellp = next_cellp;

	return (cellp);
}


/*		S W A P B U F F
 *
 *  This routine serves to swap buffer pointers: i.e., one buffer is read
 *  at a time.  The first buffer read becomes the "bottom buffer" the new
 *  buffer becomes the "top buffer".  Once the vertical comparison between
 *  the two buffers has been done, the "top buffer" now becomes the "bottom
 *  buffer" and is retained, while the erstwhile "bottom buffer" is the new
 *  "top buffer", which is overwritten when the next line of information is
 *  read.
 *  This routine takes as its parameters the address of two pointers to buffers.
 *  It manipulates these, but returns nothing.
 */

void
swapbuff(struct cell **onepp, struct cell **twopp)
					/* caveat: variables must start w/ letters */


{

	struct cell	*temp_p;	/* caveat: hyphens are read as "minus" */

	temp_p = *onepp;
	*onepp = *twopp;
	*twopp = temp_p;

}


/*		C L E A N L I N E
 *
 *  This routine takes as paramenters the address of a buffer and an integer
 *  reflecting the width of the file.  It proceeds to ZERO fill the buffer.
 *  This routine returns nothing.
 */

void
cleanline(struct cell *inbuffp, int file_width)
{

	int	i;

	for(i = 0; i < file_width + 2; i++, inbuffp++)  {
		inbuffp->c_id = ID_BACKGROUND;
		inbuffp->c_dist = 0;
		VSET(inbuffp->c_hit, 0, 0, 0);
		VSET(inbuffp->c_normal, 0, 0, 0);
		VSET(inbuffp->c_rdir, 0, 0, 0);
	}
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
