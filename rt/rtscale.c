/*                    S C A L E . P L O T
*
*  This is a program will compute and plot an appropriate scale in the lower
*  left corner of a given image.  The scale is layed out in view
*  space coordinates and then translated into model coordinates, where it is
*  plotted.  This plot can be overlayed onto any other UNIXplot file of onto
*  a pix file.
*  
*  The scale will be a simple line with a certain number of tick marks along
*  it.  It will always end with a "nice" number: that is, rounded to the the
*  nearest 10 as appropriate.
*
*  The program conisists of three parts:
*	1) take pertinent information from the rt log file (size, etc.) and
*	   setting up the view2model matrix;
*	2) lay out the scale in view-coordinates and convert all points to
*	   model coordinates for plotting in model coordinates;
*			and
*	3) overlaying the scales onto the original image (plrot and cat the
*	   files together.
*
*
*  Authors -
*	Susanne L. Muuss, J.D.
*	
*
*  Source -
*	SECAD/VLD Computing Consortium, Bldg. 394
*	The U. S. Army Ballistic Reasearch Laboratory
*	Aberdeen Proving Ground, Maryland  21005
*
*  Copyright Notice -
*	This software is Copyright (C) 1991 by the United States Army.
*	All rights reserved.
*/
#ifdef lint
static char RCSscale.plot[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"


#define TICKS	10		/* a hack for now: needs to be a variable */


char usage[] = "\
Usage:  scale.plot < file.ray\n";


static FILE		*outfp;		/* file pointer for plotting */

void	layout_n_plot();
void	get_info();
void	overlay();

mat_t		view2model = {
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0};	/* identity matrix for now */


/*
*
*                     M A I N
*
*  Main exists to coordinate the actions of the three parts of this program.
*
*/

main()
{

	layout_n_plot();

	exit(0);

}


/*
 *		L A Y O U T _ N _ P L O T
 *
 *  This routine lays out the scale in view coordinates.  It then translates
 *  all the points into model coordinates and plots them in a file.
 *  It takes no parameters and it returns nothing.
 */

void
layout_n_plot()
{

	/*  For now all the variables will be declared here.  If necessary,
	 *  some will become #defines, or external variables later.
	 */

	int		height;		/* height of picture */
	int		width;		/* width of picture */
	int		nticks;		/* number of tick marks required */
	float		ticklen;	/* distance between tick marks */
	float		tick_hgt;	/* total height of tick marks */
	point_t		v_ltick_top;	/* view coordinate left tick top */
	point_t		v_ltick_bot;	/* view coordinate left tick bottom */
	point_t		v_rtick_top;	/* view coordinate right tick top */
	point_t		v_rtick_bot;	/* view coordinate right tick bottom */
	point_t		m_ltick_top;	/* model coordinate left tick top */
	point_t		m_ltick_bot;	/* model coordinate left tick bottom */
	point_t		m_rtick_top;	/* model coordinate right tick top */
	point_t		m_rtick_bot;	/* model coordinate right tick bottom */
	point_t		v_leftpt;	/* view coordinate left point */
	point_t		v_rightpt;	/* view coordinate right point */
	point_t		m_leftpt;	/* model coordinate left point */
	point_t		m_rightpt;	/* model coordinate right point */
	fastf_t		dx;		/* view coord. x distance */
	fastf_t		dy;		/* view coord. y distance */
	fastf_t		len;		/* length of scale */


	/* Note that the view-coordinates start at the lower left corner (
	 * (0, 0) running to lower right corner (1, 0) in dx and to upper
	 * left corner (1, 0) in dy.  Thus, 10% of the distance is 0.1.
	 */

	nticks =  TICKS;
	dx = 0.1;	
	dy = 0.1;
	len = 0.2;
	tick_hgt = dy/4;

	/* Make the starting point (in view-coordinates) of the scale.
	 * Note that there is no Z coordinate since the view-coordinate
	 * system is a flat world.
	 */


	VSET(v_leftpt, dx, dy, 0);
	VSET(v_rightpt, dx + len, dy, 0);

	/* First and last ticks. */
	
	VSET(v_ltick_top, dx, (dy + (0.5 * tick_hgt)), 0);
	VSET(v_ltick_bot, dx, (dy - (0.5 * tick_hgt)), 0);

	VSET(v_rtick_top, (dx + len), (dy + (0.5 * tick_hgt)), 0);
	VSET(v_rtick_bot, (dx + len), (dy - (0.5 * tick_hgt)), 0);

/* Why don't I get my tick marks?  */

fprintf(stderr, "v_ltick_top: x=%f, y=%f, z=%f\n", v_ltick_top[X],
	v_ltick_top[Y], v_ltick_top[Z]);
fprintf(stderr, "v_ltick_bot: x=%f, y=%f, z=%f\n", v_ltick_bot[X],
	v_ltick_bot[Y], v_ltick_bot[Z]);
fprintf(stderr, "v_rtick_top: x=%f, y=%f, z=%f\n", v_rtick_top[X],
	v_rtick_top[Y], v_rtick_top[Z]);
fprintf(stderr, "v_rtick_bot: x=%f, y=%f, z=%f\n", v_rtick_bot[X],
	v_rtick_bot[Y], v_rtick_bot[Z]);

	/* Now convert the coordinates to model space using a translation
	 * matrix.  This section is just an outline of things to come.
	 * It is hokey, and not meant to compile at this time.
	 */

	MAT4X3PNT(m_leftpt, view2model, v_leftpt);
	MAT4X3PNT(m_rightpt, view2model, v_rightpt);

	MAT4X3PNT(m_ltick_top, view2model, v_ltick_top);
	MAT4X3PNT(m_ltick_bot, view2model, v_ltick_bot);
	MAT4X3PNT(m_rtick_top, view2model, v_rtick_top);
	MAT4X3PNT(m_rtick_bot, view2model, v_rtick_bot);

	/* Now plot these coordinates and the tick marks on the scale
	 * in model space. Note that the -o option for renaming the output
	 * needs to be interfaced to this.
	 */

	outfp = fopen("scale.pl",  "w");
	if( outfp == NULL)  {
		perror("scale.pl");
	}

	/* Draw the basic line. */

	pdv_3move(outfp, m_leftpt);
	pdv_3cont(outfp, m_rightpt);

	/* Draw first and last tick marks */

	pdv_3move(outfp, m_ltick_bot);
	pdv_3cont(outfp, m_ltick_top);

	pdv_3move(outfp, m_rtick_bot);
	pdv_3cont(outfp, m_rtick_top);

	fclose(outfp);
	return;
}

