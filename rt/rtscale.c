/*                    R T S C A L E 
*
*  This is a program will compute and plot an appropriate scale in the lower
*  left corner of a given image.  The scale is layed out in view
*  space coordinates and then translated into model coordinates, where it is
*  plotted.  This plot can be overlayed onto any other UNIX-Plot file of onto
*  a pix file.
*  
*  The scale will be a simple line with a certain number of tick marks along
*  it.  It will always end with a "nice" number: that is, rounded to the 
*  nearest 10 as appropriate.
*
*  The program consists of three parts:
*	1) take view, orientation, eye_position, and size from the rt log 
*          file, and use this information to build up the view2model matrix;
*	2) lay out the scale in view-coordinates and convert all points to
*	   model coordinates for plotting in model coordinates;
*			and
*	3) concatenate the scales and a copy of the original image into a
*	   a composite that it printed on standard out.  For the moment this
*	   is achieved by saying " cat scale.pl file.pl >> out.file ".  
*	   The order of the files is very important: if not cat'ed in the
*	   right order, the scales will be lost when plrot is applied though
*	   they will still be seen with pl-sgi and mged. Later
*	   this will be handled by scale.c as an fread() and fwrite().
*
*
*  Authors -
*	Susanne L. Muuss, J.D.
*	
*
*  Source -
*	SECAD/VLD Computing Consortium, Bldg. 394
*	The U. S. Army Ballistic Research Laboratory
*	Aberdeen Proving Ground, Maryland  21005
*
*  Copyright Notice -
*	This software is Copyright (C) 1991 by the United States Army.
*	All rights reserved.
*/
#ifndef lint
static char RCSscale[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "wdb.h"
#include "raytrace.h"
#include "plot3.h"

#define NAMELEN 40
#define BUFF_LEN 256
#define FALSE 0
#define TRUE 1


char usage[] = "\
Usage:  rtscale (width) (units) (interval) filename [string] >  file.pl\n\
	(width)		length of scale in model measurements\n\
	(units)		sting denoting the unit type,\n\
	(interval)	number of intervals on the scale\n\
	filename	name of log file to be read\n\
	[string]	optional, descriptive string\n";

int	layout_n_plot();
int	read_rt_file();
int	drawscale();
int	drawticks();
int	overlay();
void	make_border();
void	make_bounding_rpp();

static FILE	*fp;
int		border;			/* flag for debugging; to be used later */
int		verbose;		/* flag for debugging; to be used later */
int		SEEN_DESCRIPT=0;	/* flag for descriptive string */

/*
 *
 *                     M A I N
 *
 *  Main exists to coordinate the actions of the three parts of this program.
 *  It also processes its own arguments (argc and argv).
 */

main(argc, argv)
int	argc;
char	**argv;

{

	mat_t		model2view;		/* matrix for converting from model to view space */
	mat_t		view2model;		/* matrix for converting from view to model space */
	char		units[BUFF_LEN];	/* string for units type */
	char		label[BUFF_LEN];	/* string for scale labeling */
	char		name[BUFF_LEN];		/* incoming file name */
	char		descript[BUFF_LEN];	/* descriptive string, optional */
	int		intervals;		/* number of intervals */
	int		ret;			/* function return code */
	fastf_t		m_len;			/* scale length in model size */

	mat_idn(view2model);			/* makes an identity matrix */
	mat_idn(model2view);

	/* Check to see that the correct format is given, else print
	 * usage message.
	 */

	if(argc < 5)  {
		fputs(usage, stderr);
		exit(-1);
	}

	/* Open an incoming file for reading */

	fp = fopen( argv[4], "r");
	if( fp == NULL )  {
		perror(argv[4]);
		exit(-1);
	}

	/* Now process the arguments from main */	

	strcpy(label, argv[1]);
	strcpy(units, argv[2]);
	strcat(label, units);
	strcpy(name, argv[4]);

	if( argc == 6 )  {
		strcpy( descript, argv[5] );
		SEEN_DESCRIPT = 1;
	}

	intervals = atof(argv[3]);

	m_len = atof(argv[1]) * rt_units_conversion(argv[2]);

	/* Check to make sure there has been a valid conversion. */
	if(m_len <= 0)  {
		fprintf(stderr, "Invalid length =%.6f\n", m_len);
		fputs(usage, stderr);
		exit(-1);
	}

	/* Send pointer read_rt_file() a pointer to local model2view matrix 
	 * and to stdin. Send lay_out_n_plot() a pointer to stdout and to
	 * the inverted private matrix, which is now view2model.  In inverting
	 * the matrix in main(), greater modularity and reusability is gained
	 * for lay_out_n_plot(). ( Note &view2model[0] can be used, but is
	 * not elegant.)
	 */

	ret = read_rt_file(fp, name,  model2view);
	if(ret < 0)  {
		exit(-1);
	}

	mat_inv(view2model, model2view);

	if(verbose)  {
		fprintf(stderr, "label=%s\n", label);
		mat_print("view2model", view2model);
	}

	/* Make a bounding rpp for the model and put out a space command. */
	make_bounding_rpp(stdout, view2model);

	ret = layout_n_plot(stdout, label, view2model, model2view, intervals, m_len, descript);
	if(ret < 0)  {
		exit(-1);
	}

	if(border)  {
		/* For diagnostic purposes, a border can be put out. */
		make_border(stdout, view2model);
	}

	exit(0);

}


/*
 *		L A Y O U T _ N _ P L O T
 *
 *  This routine lays out the scale in view coordinates.  These are then
 *  converted to model space.
 *  It receives pointers to stdout, a label, and a view2model matrix, as 
 *  well as a number of intervals and the length of the scale on model units.
 *  An optional, descriptive string is also taken.
 *  This makes it very general.  Lastly, it returns 0 okay, <0 failure.
 */

int
layout_n_plot(outfp, label, v2mod, m2view, intervals, m_len, descript)
FILE	*outfp;
char	*label;
mat_t	v2mod;
mat_t	m2view;
int	intervals;
fastf_t	m_len;
char	*descript;

{


	int		nticks;		/* number of tick marks required */
	int		tickno;		/* counter of tickmarks to be done */
	int		ret;		/* return code from functions */
	int		nchar;		/* number of characters in the label */
	double		v_char_width;	/* char. width in view space */
	double		m_char_width;	/* char. width in model space */
	mat_t		v2symbol;	/* view to symbol sapce matrix */
	float		v_len;		/* scale length in view space */
	float		v_tick_hgt;	/* total height of tick marks, view space */
	float		m_tick_hgt;	/* total height of tick marks, model space */
	float		m_free_space;	/* 80% of scale len, use for writing */
	float		v_free_space;	/* m_free_space's analogue in view space */
	float		v_x_offset;	/* distance the label is offset in x */
	float		v_y_offset;	/* distance the label is offset in y */
	float		v_y_des_offset;	/* descriptive string offset */
	point_t		v_offset;
	point_t		v_des_offset;
	vect_t		v_hgtv;		/* height vector for ticks, view space */
	vect_t		m_hgtv;		/* height vector for ticks, model space */
	vect_t		m_inv_hgtv;
	vect_t		v_lenv;		/* direction vector along x-axis, view space */
	vect_t		m_lenv;		/* direction vector along x-axis, model space */
	point_t		v_startpt;	/* starting point of scales, view space */
	point_t		m_startpt;	/* starting point in model space */
	point_t		v_label_st;	/* starting point of label, view space */
	point_t		m_label_st;	/* starting point of label, model space */
	point_t		v_descript_st;	/* starting point of description, view space */
	point_t		m_descript_st;	/* starting point of description, model space */
	point_t		centerpt;	/* point on scale where tick mark starts */


	/* Note that the view-coordinates start at the lower left corner (
	 * (0, 0) running to lower right corner (1, 0) in dx and to upper
	 * left corner (1, 0) in dy.  This is based on a first quadrant
	 * world.  So, if a mark is to appear in the lower left corner of the
	 * page, 10% of the way in, dx = -1.0 + 0.1 = -0.9; dy is calculated
	 * in a similar manner.
	 */

	nticks =  intervals - 1;
	v_tick_hgt = 0.05;

	if(verbose)  {
		fprintf(stderr, "plot: nticks=%d,\n", nticks);
	}

	/* Make the starting point (in view-coordinates) of the scale.
	 * Note that there is no Z coordinate since the view-coordinate
	 * system is a flat world.  The length and height vectors also
	 * are made in view coordinates.
	 */

	VSET(v_lenv, 1.0, 0.0, 0.0);
	VSET(v_hgtv, 0.0, 1.0, 0.0);

	/* If a second, descriptive string is given, move the printed
	 * material up by one line so that it will not be cut off by
	 * pl-fb, since the lettering rides right on the hairy edge
	 * height wise.
	 */

	if(SEEN_DESCRIPT)  {
		VSET(v_startpt, -0.9, -0.7, 0.0);
	} else {
		VSET(v_startpt, -0.9, -0.8, 0.0);
	}

	/* Now convert all necessary points and vectors to model coordinates.
	 * Unitize all direction vectors, and invert the height vector so
	 * it can be used to make the bottom half of the ticks.
	 */

	MAT4X3VEC(m_lenv, v2mod, v_lenv);
	VUNITIZE(m_lenv);
	MAT4X3VEC(m_hgtv, v2mod, v_hgtv);
	VUNITIZE(m_hgtv);
	VREVERSE(m_inv_hgtv, m_hgtv);
	MAT4X3PNT(m_startpt, v2mod, v_startpt);
	m_tick_hgt = v_tick_hgt / v2mod[15];		/* scale tick_hgt */

	if(verbose)  {
		fprintf(stderr, "layout: m_tick_hgt=%.6f, v_tick_hgt=%.6f\n", 
			m_tick_hgt, v_tick_hgt);
	}

	/* Lay out the label in view space.  Find the number of characters
	 * in the label and calculate their width.  Since characters are
	 * square, that will also be the char. height.  Use this to calculate
	 * the x- and y-offset of the label starting point in view space.
	 */

	nchar = strlen(label);
	m_free_space = m_len - (m_len * 0.2);
	v_free_space = m_free_space / m2view[15];
	v_char_width = v_free_space/nchar;
	m_char_width = v_char_width / v2mod[15];
	v_len = m_len / m2view[15];

	v_x_offset = 0.1 * v_len;
	v_y_offset = -(2 * v_tick_hgt + v_char_width);

	VSET(v_offset, v_x_offset, v_y_offset, 0 );

	VADD2(v_label_st, v_startpt, v_offset);

	/* Convert v_label_st to model space */
	MAT4X3PNT(m_label_st, v2mod, v_label_st);

	/* Now make the offset for the optional descriptive lable.
	 * The lable should appear beneath the begining of the scale and
	 * run the length of the paper if that is what it takes.
	 */

	v_y_des_offset = -( 4 * v_tick_hgt + v_char_width );
	VSET(v_des_offset, 0.0, v_y_des_offset, 0.0);
	VADD2(v_descript_st, v_startpt, v_des_offset);

	MAT4X3PNT(m_descript_st, v2mod, v_descript_st);


	/* Make a view to symbol matrix.  Copy the view2model matrix, set the
	 * MAT_DELTAS to 0, and set the scale to 1.
	 */

	mat_copy(v2symbol, v2mod);
	MAT_DELTAS(v2symbol, 0, 0, 0);
	v2symbol[15] = 1;

	/* Draw the basic scale with the two freebie end ticks.  Then,
	 * if nticks is 0, nothing further is needed.  If nticks is
	 * greater than zero, then tickmarks must be made.
	 */

	ret = drawscale(outfp, m_startpt, m_len, m_tick_hgt, m_lenv, m_hgtv, m_inv_hgtv);
	if( ret < 0 )  {
		fprintf(stderr, "Layout: drawscale failed\n");
		return(-1);
	}

	if(nticks >  0 )  {

		/* Now make the ticks within the basic scale.  The ticks should
		 * be half the height of the end ticks to be distinguishable.
		 */

		for( tickno = 1; tickno < nticks; tickno++ )  {

			VJOIN1(centerpt, m_startpt, m_len * tickno/nticks, m_lenv);

			if(verbose)  {
				VPRINT("centerpt", centerpt);
			}

			ret = drawticks(outfp, centerpt, m_hgtv, m_tick_hgt * 0.5, m_inv_hgtv );
			if( ret < 0 )  {
				fprintf(stderr, "layout: drawtick skipping tickno %d\n",
					tickno);
			}
		}

	}
	
	if(verbose)  {
		fprintf(stderr, "Now calling tp_3symbol( outfp, %s, m_lable_st= %.6f, %.6f, %.6f, m_char_width=%.6f\n",
		        label, V3ARGS(m_label_st), m_char_width);
		mat_print("v2symbol", v2symbol);
	}

	/* Now put the label on the plot.  The first is the lable for
	 * numbers under the scale; the second is for an optional string.
	 */	

	tp_3symbol(outfp, label, m_label_st, v2symbol, m_char_width);
	tp_3symbol(outfp, descript, m_descript_st, v2symbol, m_char_width);
	return( 0 );		/* OK */
}

	

/*		D R A W S C A L E
 *
 * This routine draws the basic scale: it draws a line confined by two
 * end tick marks.  It return either 0 okay < 0 failure.
 * The parameters are a pointer to stdout, a start 
 * point, a height, a length vector, a height vector, and an inverse height
 * vector.
 */

int
drawscale(outfp, startpt, len, hgt, lenv, hgtv, inv_hgtv)
FILE		*outfp;
point_t		startpt;
fastf_t		len;
fastf_t		hgt;
vect_t		lenv;
vect_t		hgtv;
vect_t		inv_hgtv;
{

	point_t		endpt;

	/* Make an end point.  Call drawtick to make the two ticks. */

	VJOIN1(endpt, startpt, len, lenv);
	
	pdv_3move(outfp, startpt);
	pdv_3cont(outfp, endpt);

	if(verbose)  {
		fprintf(stderr, "drawscale invoked drawticks\n");
		VPRINT("startpt", startpt);
		VPRINT("endpt", endpt);
	}

	drawticks(outfp, startpt, hgtv, hgt, inv_hgtv);
	drawticks(outfp, endpt, hgtv, hgt, inv_hgtv);

	return(0);
}


/*		D R A W T I C K S
 *
 * This routine draws the tick marks for the scale.  It takes a out file
 * pointer, a center point whereat to start the tick mark, a height vector
 * for the tick, and a scalar for the tick height.  It returns either
 * 0 okay or < 0 failure.
 */

int
drawticks(outfp, centerpt, hgtv, hgt, inv_hgtv)
FILE		*outfp;
point_t		centerpt;
vect_t		hgtv;
fastf_t		hgt;
vect_t		inv_hgtv;
{

	point_t		top;		/* top of tick mark */
	point_t		bot;		/* bottom of tick mark */

	if(verbose)  {
		VPRINT("hgtv", hgtv);
	}

	VJOIN1(top, centerpt, hgt, hgtv);
	VJOIN1(bot, centerpt, hgt, inv_hgtv);

	if(verbose)  {
		VPRINT("top", top);
		VPRINT("bot", bot);
		fprintf(stderr, "drawticks now using top, bot to plot\n");
	}

	pdv_3move(outfp, top);
	pdv_3cont(outfp, bot);

	return( 0 );
}

/*		M A K E _ B O R D E R
 *
 * This routine exists to draw an optional border around the image.  It
 * exists for diagnostic purposes.  It takes a view to model matrix and
 * a file pointer.  It lays out and plots the four corners of the image border.
 */

void
make_border(outfp, v2mod)
FILE	*outfp;
mat_t	v2mod;
{

	point_t		v_lleft_pt;		/* lower left point, view space */
	point_t		v_lright_pt;		/*lower right point, view space */
	point_t		v_uleft_pt;		/* upper left point, view space */
	point_t 	v_uright_pt;		/* upper right point, view space */
	point_t		m_lleft_pt;		/* lower left point, mod. space */
	point_t 	m_lright_pt;		/* lower right point, mod. space */
	point_t		m_uleft_pt;		/* upper left point, mod. space */
	point_t		m_uright_pt;		/* upper right point, mod. space */

	/* Make all the points in view space. */

	VSET(v_lleft_pt, -1.0, -1.0, 0.0);
	VSET(v_lright_pt, 1.0, -1.0, 0.0);
	VSET(v_uleft_pt, -1.0, 1.0, 0.0);
	VSET(v_uright_pt, 1.0, 1.0, 0.0);

	/* Convert all the points to model space */
	MAT4X3PNT(m_lleft_pt, v2mod, v_lleft_pt);
	MAT4X3PNT(m_lright_pt, v2mod, v_lright_pt);
	MAT4X3PNT(m_uleft_pt, v2mod, v_uleft_pt);
	MAT4X3PNT(m_uright_pt, v2mod, v_uright_pt);

	/* Now plot the border in model space. */
	pdv_3move(outfp, m_lleft_pt);
	pdv_3cont(outfp, m_lright_pt);
	pdv_3cont(outfp, m_uright_pt);
	pdv_3cont(outfp, m_uleft_pt);
	pdv_3cont(outfp, m_lleft_pt);

	return;
}

/*
 *		M A K E _ B O U N D I N G _ R P P
 *
 * This routine takes a view2model matrix and a file pointer.  It calculates the  minimun and
 * the maximun points of the viewing cube in view space, and then translates
 * it to model space and rotates it so that it will not shrink when rotated and
 * cut off the geometry/image.  This routine returns nothing.
 */

void
make_bounding_rpp(outfp, v2mod)
FILE	*outfp;
mat_t	v2mod;
{

	point_t		v_min;		/* view space minimum coordinate */
	point_t		v_max;		/* view space maximum coordinate */
	point_t		new_min;	/* new min of rotated viewing cube */
	point_t		new_max;	/* new max of rotated viewing cube */

	/* Make the min and max points of the view-space viewing cube */
	VSET(v_min, -1.0, -1.0, -1.0);
	VSET(v_max, 1.0, 1.0, 1.0);

	/* Now rotate the viewing cube and obtain new minimum and maximum. */

	rt_rotate_bbox(new_min, new_max, v2mod, v_min, v_max);

	/* Now issue the space command */

	pdv_3space(outfp, new_min, new_max);

	return;
}
