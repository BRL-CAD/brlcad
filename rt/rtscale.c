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
*	1) take view, orientation, eye_position, and size from the rt log 
*          file, and use this information to build up the view2model matrix;
*	2) lay out the scale in view-coordinates and convert all points to
*	   model coordinates for plotting in model coordinates;
*			and
*	3) concatenate the scales and a copy of the original image into a
*	   a composite that it printed on standard out.  For the moment this
*	   is achieved by saying " cat file.pl scale.pl >> out.file ".  Later
*	   this will be handled by scale.c as an fread() and fwrite().
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
#ifndef lint
static char RCSscale[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "wdb.h"

#define TICKS	10		/* a hack for now: needs to be a variable */
#define NAMELEN 40
#define BUFF_LEN 256
#define FALSE 0
#define TRUE 1

char usage[] = "\
Usage:  scale (width) (units) (interval) < file > file.pl\n\
	(width)		length of scale in model measurements\n\
	(units)		sting denoting the unit type,\n\
	(interval)	number of intervals on the scale\n";

int	layout_n_plot();
int	read_rt_file();
int	drawscale();
int	drawticks();
int	overlay();

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
	char		units[BUFF_LEN];	/* string denoting the type of units */
	int		intervals;		/* number of intervals desired */
	int		ret;			/* return code from functions */
	fastf_t		m_len;			/* length of scale in model size */

	fastf_t	len;				/* temporary */
	fastf_t	conv;				/* temporary */

	mat_idn(view2model);			/* makes an identity matrix */
	mat_idn(model2view);

	/* Check to see that the correct format is given, else print
	 * usage message.
	 */

	if(argc != 4)  {
		fputs(usage, stderr);
		exit(-1);
	}

	/* Now process the arguments from main */	

	strcpy(units, argv[2]);
	intervals = atof(argv[3]);

	len = atof(argv[1]);
	conv = mk_cvt_factor(argv[2]);
	m_len = atof(argv[1]) * mk_cvt_factor(argv[2]);

fprintf(stderr, "len=%g, conv=%g, units=%s, m_len=%g\n", len, conv, units, m_len);

	/* Check to make sure there has been a valid conversion. */
	if(m_len <= 0)  {
		fprintf(stderr, "Invalid length =%d\n", m_len);
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

	ret = read_rt_file(stdin, model2view);
	if(ret < 0)  {
		exit(-1);
	}

	mat_inv(view2model, model2view);

mat_print("view2model", view2model);

	ret = layout_n_plot(stdout, view2model, intervals, m_len);
	if(ret < 0)  {
		exit(-1);
	}

	exit(0);

}


/*
 *		L A Y O U T _ N _ P L O T
 *
 *  This routine lays out the scale in view coordinates.  It receives a
 *  pointer to a view2model matrix and to stdout.  This makes it very
 *  general.  Lastly, it returns 0 okay, <0 failure.
 */

int
layout_n_plot(outfp, v2mod, intervals, m_len)
FILE	*outfp;
mat_t	v2mod;
int	intervals;
fastf_t	m_len;
{


	int		nticks;		/* number of tick marks required */
	int		tickno;		/* counter of tickmarks to be done */
	int		ret;		/* return code from functions */
	float		v_tick_hgt;	/* total height of tick marks, view space */
	float		m_tick_hgt;	/* total height of tick marks, model space */
	vect_t		v_hgtv;		/* height vector for ticks, view space */
	vect_t		m_hgtv;		/* height vector for ticks, model space */
	vect_t		v_lenv;		/* direction vector along x-axis, view space */
	vect_t		m_lenv;		/* direction vector along x-axis, model space */
	point_t		v_startpt;	/* starting point of scales, view space */
	point_t		m_startpt;	/* starting point in model space */
	point_t		centerpt;	/* point on scale where tick mark starts */


	/* Note that the view-coordinates start at the lower left corner (
	 * (0, 0) running to lower right corner (1, 0) in dx and to upper
	 * left corner (1, 0) in dy.  This is based on a first quadrant
	 * world.  So, if a mark is to appear in the lower left corner of the
	 * page, 10% of the way in, dx = -1.0 + 0.1 = -0.9; dy is calculated
	 * in a similar manner.
	 */

	nticks =  intervals - 1;
	v_tick_hgt = 100.0;


 fprintf(stderr, "plot: nticks=%d,\n", nticks);

	/* Make the starting point (in view-coordinates) of the scale.
	 * Note that there is no Z coordinate since the view-coordinate
	 * system is a flat world.  The length and height vectors also
	 * are made in view coordinates.
	 */

	VSET(v_lenv, 1.0, 0.0, 0.0);
	VSET(v_hgtv, 0.0, 1.0, 0.0);
	VSET(v_startpt, -0.9, -0.9, 0.0);

	/* Now convert all necessary points to model coordinates */

	MAT4X3VEC(m_lenv, v2mod, v_lenv);
	VUNITIZE(m_lenv);
	MAT4X3VEC(m_hgtv, v2mod, v_hgtv);
	VUNITIZE(m_hgtv);
	MAT4X3VEC(m_startpt, v2mod, v_startpt);
	m_tick_hgt = v_tick_hgt * v2mod[15];		/* scale tick_hgt */

fprintf(stderr, "layout: m_tick_hgt=%g, v_tick_hgt=%g\n", m_tick_hgt, v_tick_hgt);

	/* Draw the basic scale with the two freebie end ticks.  Then,
	 * if nticks is 0, nothing further is needed.
	 */

	ret = drawscale(outfp, m_startpt, m_len, m_tick_hgt, m_lenv, m_hgtv);
	if( ret < 0 )  {
		fprintf(stderr, "Layout: drawscale failed\n");
		return(-1);
	}

	if(nticks <= 0 )  {
		return( 0 );
	}
	
	/* Now make the ticks within the basic scale */

	for( tickno = 1; tickno < nticks; tickno++ )  {
fprintf(stderr, "making ticks tickno=%d\n", tickno);
		VJOIN1(centerpt, m_startpt, m_len * tickno/nticks, m_lenv);
VPRINT("centerpt", centerpt);
		ret = drawticks(outfp, centerpt, m_hgtv, m_tick_hgt * 0.75 );
		if( ret < 0 )  {
			fprintf(stderr, "layout: drawtick skipping tickno %d\n",
				tickno);
		}

	}
}

/*		R E A D _ R T _ F I L E
 *
 * This routine reads an rt_log file line by line until it either finds
 * view, orientation, eye_postion, and size of the model, or it hits the
 * end of file.  When a colon is found, sscanf() retrieves the
 * necessary information.  It takes a file pointer and a matrix
 * pointer as parameters.  It returns 0 okay or < 0 failure.
 */

int
read_rt_file(infp, model2view)
FILE	*infp;
mat_t 	model2view;
{

	fastf_t		azimuth;		/* part of the view */
	fastf_t		elevation;		/* part of the view */
	quat_t		orientation;		/* orientation */
	point_t		eye_pos;
	fastf_t		m_size;			/* size of model in mm */
	char		*ret;			/* return code for fgets */
	char		string[BUFF_LEN];	/* temporary buffer */
	char		*arg_ptr;		/* place holder */
	char		forget_it[9];		/* need for sscanf, then forget */
	int		i;			/* reusable counter */
	int		num;			/* return code for sscanf */
	int		seen_view;		/* these are flags.  */
	int		seen_orientation;
	int		seen_eye_pos;
	int		seen_size;

	mat_t		rotate, xlate;
	mat_t		tmp_mat;

	/* Set all flags to ready state.  */

	seen_view = FALSE;
	seen_orientation = FALSE;
	seen_eye_pos = FALSE;
	seen_size = FALSE;

/* fprintf(stderr, "set flags: view=%d, orient.=%d, eye_pos=%d, size=%d\n",
 *	seen_view, seen_orientation, seen_eye_pos, seen_size);
 */

	/* feof returns 1 on failure */

	while( feof(infp) == 0 )  {

		/* clear the buffer */	
		for( i = 0; i < BUFF_LEN; i++ )  {
			string[i] = '\0';
		}
		ret = fgets(string, BUFF_LEN, infp);
		if( ret == NULL )  {
/*			fprintf(stderr, "read_rt_log: read failure on file %s\n",
 *				string);
 *			return(-1);
 */
			break;
		}


		/* Check the first for a colon in the buffer.  If there is
		 * one, replace it with a NULL, and set a pointer to the
		 * next space.  Then feed the buffer to
		 * strcmp see whether it is the view, the orientation,
		 * the eye_position, or the size.  If it is, then sscanf()
		 * the needed information into the appropriate variables.
		 * If the keyword is not found, go back for another line.
		 *
		 * Set arg_ptr to NULL so it can be used as a flag to verify
		 * finding a colon in the input buffer.
		 */

		arg_ptr = NULL;

		for( i = 0; i < BUFF_LEN; i++ )  {
			/* Check to make sure the first char. is not a NULL;
			 * if it is, go back for a new line.
			 */
			if( string[i] == '\0' )  {
				break;
			}
			if( string[i] == ':')  {
				/* If a colon is found, set arg_ptr to the
				 * address of the colon, and break: no need to
				 * look for more colons on this line.
				 */

/* fprintf(stderr, "found colon\n");
 */
				string[i] = '\0';
				arg_ptr = &string[++i];		/* increment before using */
				break;
			}
		}

		/* Check to see if a colon has been found.  If not, get another
		 * input line.
		 */

		if( arg_ptr == NULL )  {
			continue;
		}

		/* Now compare the first word in the buffer with the
		 * key words wanted.  If there is a match, read the
		 * information that follows into the appropriate
		 * variable, and set a flag to indicate that the
		 * magic thing has been seen.
		 *
		 * Note two points of interest: scanf() does not like %g;
		 * use %lf.  Also, if loading a whole array of characters
		 * with %s, then the name of the array can be used for the
		 * destination.  However, if the characters are loaded 
		 * individually into the subsripted spots with %c (or equiv),
		 * the address of the location must be provided: &eye_pos[0].
		 */

		if(strcmp(string, "View") == 0)  {

/* fprintf(stderr, "found view\n");
 */
			num = sscanf(arg_ptr, "%lf %s %lf", &azimuth, forget_it, &elevation);
			if( num != 3)  {
				fprintf(stderr, "View= %g %s %g elevation\n", azimuth, forget_it, elevation);
				return(-1);
			}
			seen_view = TRUE;
		} else if(strcmp(string, "Orientation") == 0)  {

/* fprintf(stderr, "found orientation\n");
 */
			num = sscanf(arg_ptr, "%lf, %lf, %lf, %lf",
				&orientation[0], &orientation[1], &orientation[2],
				&orientation[3]);

 			if(num != 4)  {
				fprintf(stderr, "Orientation= %g, %g, %g, %g\n",
				 	orientation[0], orientation[1],
					orientation[2], orientation[3]);
				return(-1);
			}
			seen_orientation = TRUE;
		} else if(strcmp(string, "Eye_pos") == 0)  {

/* fprintf(stderr, "found eye_pos\n");
 */
			num = sscanf(arg_ptr, "%lf, %lf, %lf", &eye_pos[0],
				&eye_pos[1], &eye_pos[2]);
			if( num != 3)  {
				fprintf(stderr, "Eye_pos= %g, %g, %g\n",
					eye_pos[0], eye_pos[1], eye_pos[2]);
				return(-1);
			}
			seen_eye_pos = TRUE;
		} else if(strcmp(string, "Size") == 0)  {

/* fprintf(stderr, "found size\n");
 */
			num = sscanf(arg_ptr, "%lf", &m_size);
			if(num != 1)  {
				fprintf(stderr, "Size=%g\n", m_size);
				return(-1);
			}
			seen_size = TRUE;
		}
	}

	/* Check that all the information to proceed is available */

/* fprintf(stderr, "now checking data\n");
 */
	if( seen_view != TRUE )  {
		fprintf(stderr, "View not read!\n");
		return(-1);
	}

	if( seen_orientation != TRUE )  {
		fprintf(stderr, "Orientation not read!\n");
		return(-1);
	}

	if( seen_eye_pos != TRUE )  {
		fprintf(stderr, "Eye_pos not read!\n");
		return(-1);
	}

	if ( seen_size != TRUE )  {
		fprintf(stderr, "Size not read!\n");
		return(-1);
	}

	/* For now, just print the stuff */

	fprintf(stderr, "view= %g azimuth, %g elevation\n", azimuth, elevation);
	fprintf(stderr, "orientation= %g, %g, %g, %g\n",
		orientation[0], orientation[1], orientation[2], orientation[3]);
	fprintf(stderr, "eye_pos= %g, %g, %g\n", eye_pos[0], eye_pos[1], eye_pos[2]);
	fprintf(stderr, "size= %gmm\n", m_size);

	/* Build the view2model matrix (matp points at this).  Variables 
	 * used only for this transaction are initialized here.
	 * 
	 */


	quat_quat2mat( rotate, orientation );
	rotate[15] = 0.5 * m_size;
	mat_idn( xlate );
	MAT_DELTAS( xlate, -eye_pos[0], -eye_pos[1], -eye_pos[2] );
	mat_mul( model2view, rotate, xlate );

mat_print("model2view", model2view);
	
	return(0);
}


	

/*		D R A W S C A L E
 *
 * This routine draws the basic scale: it draws a line confined by two
 * end tick marks.  It return either 0 okay < 0 failure.
 * The parameters are an outfile pointer, a start point in model coordinates,
 * a direction vector, a total distance to go, the scalar height of the
 * tick marks, and the height vector of the tick marks.
 */

int
drawscale(outfp, startpt, len, hgt, lenv, hgtv)
FILE		*outfp;
point_t		startpt;
fastf_t		len;
fastf_t		hgt;
vect_t		lenv;
vect_t		hgtv;
{

	point_t		endpt;

	/* Make an end point.  Call drawtick to make the two ticks. */

	VJOIN1(endpt, startpt, len, lenv);
	
	pdv_3move(outfp, startpt);
	pdv_3cont(outfp, endpt);

fprintf(stderr, "drawscale invoked drawticks\n");
VPRINT("startpt", startpt);
VPRINT("endpt", endpt);

	drawticks(outfp, startpt, hgtv, hgt);
	drawticks(outfp, endpt, hgtv, hgt);

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
drawticks(outfp, centerpt, hgtv, hgt)
FILE		*outfp;
point_t		centerpt;
vect_t		hgtv;
fastf_t		hgt;
{

	point_t		top;		/* top of tick mark */
	point_t		bot;		/* bottom of tick mark */
	vect_t		inv_hgtv;	/* height vector pointing down */

VPRINT("hgtv", hgtv);

	VREVERSE(inv_hgtv, hgtv);

VPRINT("inv_hgtv", inv_hgtv);

	VJOIN1(top, centerpt, hgt, hgtv);
	VJOIN1(bot, centerpt, hgt, inv_hgtv);

VPRINT("top", top);
VPRINT("bot", bot);
fprintf(stderr, "drawticks now using top, bot to plot\n");

	pdv_3move(outfp, top);
	pdv_3cont(outfp, bot);

	return( 0 );
}
