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
*	   a composite that it printed on standard out.
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


#define TICKS	10		/* a hack for now: needs to be a variable */
#define NAMELEN 40
#define BUFF_LEN 256
#define SUCCESS 1
#define ERROR -1
#define FALSE 0
#define TRUE 1

char usage[] = "\
Usage:  scale < file > file.pl\n";


int	layout_n_plot();
int	read_rt_file();
int	overlay();

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

main(argc)
int	argc;
{

	int		ret;		/* return code from functions */

	/* Check to see that the correct format is given, else print
	 * usage message.
	 */

	if(argc != 1)  {
		fputs(usage, stderr);
		exit(-1);
	}

	/* Send pointer to stdin to read_rt_file, and send pointer to
	 * to stdout to layout_n_plot. */

	ret = read_rt_file();
	if(ret == ERROR)  {
		exit(-1);
	}
	ret = layout_n_plot(stdout);
	if(ret == ERROR)  {
		exit(-1);
	}

	exit(0);

}


/*
 *		L A Y O U T _ N _ P L O T
 *
 *  This routine lays out the scale in view coordinates.  It then translates
 *  all the points into model coordinates and plots them in a file.
 *  It takes stdout as its parameter: this makes it very general.  Lastly,
 *  it returns SUCCESS or ERROR.
 */

int
layout_n_plot(outfp)
FILE	*outfp;

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

/* fprintf(stderr, "plot: nticks=%d, dx=%f, dy=%f, len=%f, tick_hgt=%f\n",
 *	nticks, dx, dy, len, tick_hgt);
 */

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

/* Why don't I get my tick marks?  
 * fprintf(stderr, "v_ltick_top: x=%f, y=%f, z=%f\n", v_ltick_top[X],
 *	v_ltick_top[Y], v_ltick_top[Z]);
 * fprintf(stderr, "v_ltick_bot: x=%f, y=%f, z=%f\n", v_ltick_bot[X],
 *	v_ltick_bot[Y], v_ltick_bot[Z]);
 * fprintf(stderr, "v_rtick_top: x=%f, y=%f, z=%f\n", v_rtick_top[X],
 *	v_rtick_top[Y], v_rtick_top[Z]);
 * fprintf(stderr, "v_rtick_bot: x=%f, y=%f, z=%f\n", v_rtick_bot[X],
 *	v_rtick_bot[Y], v_rtick_bot[Z]);
 */

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
	 * in model space.  Plot on stdout (later this should be outfp).
	 */

/* fprintf(stderr, "plot: now plotting:\n");
 * fprintf(stderr, "m_leftpt: %f, %f, %f\n", m_leftpt[0], m_leftpt[1], m_leftpt[2]);
 * fprintf(stderr, "m_rightpt: %f, %f, %f\n", m_rightpt[0], m_rightpt[1], m_rightpt[2]);
 * fprintf(stderr, "m_ltick_bot: %f, %f, %f\n", m_ltick_bot[0], m_ltick_bot[1], m_ltick_bot[2]);
 * fprintf(stderr, "m_ltick_top: %f, %f, %f\n", m_ltick_top[0], m_ltick_top[1], m_ltick_top[2]);
 * fprintf(stderr, "m_rtick_bot: %f, %f, %f\n", m_rtick_bot[0], m_rtick_bot[1], m_rtick_bot[2]);
 * fprintf(stderr, "m_rtick_top: %f, %f, %f\n", m_rtick_top[0], m_rtick_top[1], m_rtick_top[2]);
 */

	pdv_3move(outfp, m_leftpt);
	pdv_3cont(outfp, m_rightpt);

	/* Draw first and last tick marks */

	pdv_3move(outfp, m_ltick_bot);
	pdv_3cont(outfp, m_ltick_top);

	pdv_3move(outfp, m_rtick_bot);
	pdv_3cont(outfp, m_rtick_top);

	return(SUCCESS);
}


/*		R E A D _ R T _ F I L E
 *
 * This routine reads an rt_log file line by line until it either finds
 * view, orientation, eye_postion, and size of the model, or it hits the
 * end of file.  When a colon is found, sscanf() retrieves the
 * necessary information.  It returns SUCCESS or ERROR.
 */

int
read_rt_file()
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


	/* Set all flags to ready state.  */

	seen_view = FALSE;
	seen_orientation = FALSE;
	seen_eye_pos = FALSE;
	seen_size = FALSE;

/* fprintf(stderr, "set flags: view=%d, orient.=%d, eye_pos=%d, size=%d\n",
 *	seen_view, seen_orientation, seen_eye_pos, seen_size);
 */

	/* feof returns 1 on failure */

	while( feof(stdin) == 0 )  {

		/* clear the buffer */	
		for( i = 0; i < BUFF_LEN; i++ )  {
			string[i] = '\0';
		}
		ret = fgets(string, BUFF_LEN, stdin);
		if( ret == NULL )  {
/*			fprintf(stderr, "read_rt_log: read failure on file %s\n",
 *				string);
 *			return(ERROR);
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
			if( string[0] == NULL )  {
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
				return(ERROR);
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
				return(ERROR);
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
				return(ERROR);
			}
			seen_eye_pos = TRUE;
		} else if(strcmp(string, "Size") == 0)  {

/* fprintf(stderr, "found size\n");
 */
			num = sscanf(arg_ptr, "%lf", &m_size);
			if(num != 1)  {
				fprintf(stderr, "Size=%g\n", m_size);
				return(ERROR);
			}
			seen_size = TRUE;
		}
	}

	/* Check that all the information to proceed is available */

/* fprintf(stderr, "now checking data\n");
 */
	if( seen_view != TRUE )  {
		fprintf(stderr, "View not read!\n");
		return(ERROR);
	}

	if( seen_orientation != TRUE )  {
		fprintf(stderr, "Orientation not read!\n");
		return(ERROR);
	}

	if( seen_eye_pos != TRUE )  {
		fprintf(stderr, "Eye_pos not read!\n");
		return(ERROR);
	}

	if ( seen_size != TRUE )  {
		fprintf(stderr, "Size not read!\n");
		return(ERROR);
	}

	/* For now, just print the stuff */

	fprintf(stderr, "view= %g azimuth, %g elevation\n", azimuth, elevation);
	fprintf(stderr, "orientation= %g, %g, %g, %g\n",
		orientation[0], orientation[1], orientation[2], orientation[3]);
	fprintf(stderr, "eye_pos= %g, %g, %g\n", eye_pos[0], eye_pos[1], eye_pos[2]);
	fprintf(stderr, "size= %gmm\n", m_size);

	/* Build the view2model matrix. */
	return(SUCCESS);
}
